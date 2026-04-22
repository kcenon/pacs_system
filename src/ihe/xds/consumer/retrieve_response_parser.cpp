// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file retrieve_response_parser.cpp
 * @brief pugixml-backed parser for ITI-43 RetrieveDocumentSetResponse.
 */

#include "retrieve_response_parser.h"

#include "../common/wss_signer.h"

#include <pugixml.hpp>

#include <string>
#include <string_view>

namespace kcenon::pacs::ihe::xds::detail {

namespace {

// Response-size cap shared with the Source parse path. The http_client
// write callback already bounds the body to 8 MiB, but the envelope we
// pass to pugixml is an extracted substring from parse_mtom_response and
// cannot grow past that cap either. Keep a defensive check in case a
// future non-libcurl transport is wired without the same write limit.
constexpr std::size_t kMaxRootXmlBytes = 8 * 1024 * 1024;

// Literal ebRS status strings. Matching the Source behavior (non-Success
// is an error; PartialSuccess is a typed error rather than a silent
// success) for consistency between the two actors.
constexpr std::string_view kSuccessStatus =
    "urn:oasis:names:tc:ebxml-regrep:ResponseStatusType:Success";
constexpr std::string_view kPartialSuccessStatus =
    "urn:oasis:names:tc:ebxml-regrep:ResponseStatusType:PartialSuccess";

// The xop:Include href is "cid:<bare-content-id>". Strip the prefix before
// matching against the part content-ids that parse_mtom_response already
// stored without angle brackets.
std::string strip_cid_href(const std::string& href) {
    constexpr std::string_view kPrefix = "cid:";
    if (href.size() >= kPrefix.size() &&
        href.compare(0, kPrefix.size(), kPrefix) == 0) {
        return href.substr(kPrefix.size());
    }
    return href;
}

}  // namespace

kcenon::common::Result<document_response> parse_retrieve_response(
    const std::string& root_xml, const std::vector<mtom_part>& parts,
    const retrieve_request& req) {
    if (root_xml.size() > kMaxRootXmlBytes) {
        return kcenon::common::make_error<document_response>(
            static_cast<int>(error_code::transport_response_too_large),
            "RetrieveDocumentSetResponse root part exceeds 8 MiB cap (" +
                std::to_string(root_xml.size()) + " bytes)",
            std::string(error_source));
    }

    pugi::xml_document doc;
    auto parse = doc.load_buffer(root_xml.data(), root_xml.size(),
                                 pugi::parse_default, pugi::encoding_utf8);
    if (!parse) {
        return kcenon::common::make_error<document_response>(
            static_cast<int>(error_code::transport_invalid_response),
            std::string("failed to parse response envelope: ") +
                parse.description(),
            std::string(error_source));
    }

    // Use local-name() selectors so we do not depend on the specific
    // namespace prefix the repository emits. Different XDS.b stacks
    // (Apache CXF, .NET WIF, Gazelle) pick different prefixes for the
    // xdsb namespace.
    auto rds_response =
        doc.select_node(
               "//*[local-name()='RetrieveDocumentSetResponse']").node();
    if (!rds_response) {
        return kcenon::common::make_error<document_response>(
            static_cast<int>(error_code::transport_invalid_response),
            "response contains no RetrieveDocumentSetResponse element",
            std::string(error_source));
    }

    auto registry_response =
        rds_response.select_node("*[local-name()='RegistryResponse']").node();
    if (!registry_response) {
        return kcenon::common::make_error<document_response>(
            static_cast<int>(error_code::transport_invalid_response),
            "response has no RegistryResponse",
            std::string(error_source));
    }
    const std::string status =
        registry_response.attribute("status").as_string();
    if (status.empty()) {
        return kcenon::common::make_error<document_response>(
            static_cast<int>(error_code::transport_invalid_response),
            "RegistryResponse is missing status attribute",
            std::string(error_source));
    }
    if (status == kPartialSuccessStatus) {
        return kcenon::common::make_error<document_response>(
            static_cast<int>(error_code::registry_partial_success),
            "repository reported PartialSuccess: " + status,
            std::string(error_source));
    }
    if (status != kSuccessStatus) {
        // A Failure here means the repository could not locate the
        // requested document - either the uid is unknown or the requester
        // has no access. Either way, surface as not-found to the caller.
        return kcenon::common::make_error<document_response>(
            static_cast<int>(error_code::consumer_response_document_not_found),
            "repository reported non-success status: " + status,
            std::string(error_source));
    }

    // Walk each DocumentResponse looking for the requested uid. Use
    // local-name() selectors throughout because repositories emit the
    // xdsb namespace with different prefixes (xdsb, ns2, ihe-xdsb, etc.).
    pugi::xml_node match;
    for (auto dr :
         rds_response.select_nodes("*[local-name()='DocumentResponse']")) {
        auto node = dr.node();
        const std::string uid =
            node.select_node("*[local-name()='DocumentUniqueId']")
                .node()
                .text()
                .as_string();
        if (uid == req.document_unique_id) {
            match = node;
            break;
        }
    }
    if (!match) {
        return kcenon::common::make_error<document_response>(
            static_cast<int>(error_code::consumer_response_document_not_found),
            "no DocumentResponse matched requested uid " +
                req.document_unique_id,
            std::string(error_source));
    }

    const std::string mime_type =
        match.select_node("*[local-name()='mimeType']").node().text().as_string();

    auto document_node =
        match.select_node("*[local-name()='Document']").node();
    if (!document_node) {
        return kcenon::common::make_error<document_response>(
            static_cast<int>(error_code::consumer_response_document_not_found),
            "matching DocumentResponse has no Document element",
            std::string(error_source));
    }

    // Prefer resolving an xop:Include reference to one of the MTOM
    // attachment parts. Fall back to base64-inlined Document text when the
    // repository returned a non-MTOM response (some stacks do that for
    // small payloads or test responders).
    auto include =
        document_node.select_node("*[local-name()='Include']").node();

    document_response out;
    out.registry_response_status = status;
    out.document_unique_id = req.document_unique_id;
    out.repository_unique_id = req.repository_unique_id;
    out.mime_type = mime_type;

    if (include) {
        const std::string raw_href = include.attribute("href").as_string();
        const std::string cid = strip_cid_href(raw_href);
        if (cid.empty()) {
            return kcenon::common::make_error<document_response>(
                static_cast<int>(
                    error_code::consumer_response_document_not_found),
                "xop:Include has empty href",
                std::string(error_source));
        }
        const mtom_part* found = nullptr;
        for (const auto& p : parts) {
            if (p.content_id == cid) {
                found = &p;
                break;
            }
        }
        if (found == nullptr) {
            return kcenon::common::make_error<document_response>(
                static_cast<int>(
                    error_code::consumer_response_document_not_found),
                "xop:Include references unknown cid: " + cid,
                std::string(error_source));
        }
        out.content = found->content;
        if (out.mime_type.empty()) {
            out.mime_type = found->mime_type;
        }
        return out;
    }

    // Non-MTOM fallback: Document carries inline base64.
    const std::string inline_b64 = document_node.text().as_string();
    if (inline_b64.empty()) {
        return kcenon::common::make_error<document_response>(
            static_cast<int>(error_code::consumer_response_document_not_found),
            "Document element has neither xop:Include nor inline content",
            std::string(error_source));
    }
    // Decode the inline base64 so document_response.content has the
    // same "already-decoded binary bytes" semantics regardless of
    // whether the repository used MTOM or inline-base64 framing. Review
    // Minor-1 fix: avoid divergent caller expectations.
    out.content = base64_decode_bytes(inline_b64);
    if (out.content.empty()) {
        return kcenon::common::make_error<document_response>(
            static_cast<int>(error_code::consumer_response_document_not_found),
            "inline Document base64 decode failed or yielded empty bytes",
            std::string(error_source));
    }
    return out;
}

}  // namespace kcenon::pacs::ihe::xds::detail
