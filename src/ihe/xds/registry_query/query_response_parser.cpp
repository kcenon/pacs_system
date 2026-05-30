// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file query_response_parser.cpp
 * @brief pugixml-backed parser for ITI-18 AdhocQueryResponse.
 */

#include "query_response_parser.h"

#include <pugixml.hpp>

#include <cstdlib>
#include <string>
#include <string_view>

namespace kcenon::pacs::ihe::xds::detail {

namespace {

// Response-size cap shared with the retrieve parser. The http_client write
// callback already bounds the body to 8 MiB; this is a defensive second
// gate in case a future non-libcurl transport is wired in without the
// same write limit.
constexpr std::size_t kMaxRootXmlBytes = 8 * 1024 * 1024;

constexpr std::string_view kSuccessStatus =
    "urn:oasis:names:tc:ebxml-regrep:ResponseStatusType:Success";
constexpr std::string_view kPartialSuccessStatus =
    "urn:oasis:names:tc:ebxml-regrep:ResponseStatusType:PartialSuccess";

// Node accessor for the nameAttribute-scoped XDS classification codes.
// ebRIM encodes class, format, type, and content-type codes as
// Classification children of the ExtrinsicObject, each tagged by a
// classificationScheme UUID. The registry-side UUIDs are fixed by the
// IHE ITI TF-3 §4.1.5 metadata attribute table - any drift means the
// classification applies to a different attribute than the caller
// expects, which would silently populate the wrong field.
constexpr std::string_view kSchemeClassCode =
    "urn:uuid:41a5887f-8865-4c09-adf7-e362475b143a";
constexpr std::string_view kSchemeFormatCode =
    "urn:uuid:a09d5840-386c-46f2-b5ad-9c3699a4309d";
constexpr std::string_view kSchemeTypeCode =
    "urn:uuid:f0306f51-975f-434e-a61b-a3f66f70e3dc";

// External-identifier scheme UUIDs for patient id and XDSDocumentEntry
// unique id on ExtrinsicObject, from IHE ITI TF-3 §4.1.5 metadata table.
constexpr std::string_view kExtIdPatientId =
    "urn:uuid:58a6f841-87b3-4a3e-92fd-a8ffeff98427";
constexpr std::string_view kExtIdUniqueId =
    "urn:uuid:2e82c1f6-a085-4c72-9da3-8640a32e42ab";

std::string get_slot_value(pugi::xml_node extrinsic, const char* slot_name) {
    for (auto slot : extrinsic.select_nodes("*[local-name()='Slot']")) {
        auto node = slot.node();
        if (std::string_view(node.attribute("name").as_string()) ==
            slot_name) {
            auto value = node.select_node(
                "*[local-name()='ValueList']/"
                "*[local-name()='Value']").node();
            return value.text().as_string();
        }
    }
    return {};
}

std::string get_classification_code(pugi::xml_node extrinsic,
                                    std::string_view scheme) {
    for (auto c : extrinsic.select_nodes("*[local-name()='Classification']")) {
        auto node = c.node();
        if (std::string_view(
                node.attribute("classificationScheme").as_string()) ==
            scheme) {
            return node.attribute("nodeRepresentation").as_string();
        }
    }
    return {};
}

std::string get_external_identifier(pugi::xml_node extrinsic,
                                    std::string_view scheme) {
    for (auto ei : extrinsic.select_nodes(
             "*[local-name()='ExternalIdentifier']")) {
        auto node = ei.node();
        if (std::string_view(
                node.attribute("identificationScheme").as_string()) ==
            scheme) {
            return node.attribute("value").as_string();
        }
    }
    return {};
}

std::string get_localized_name(pugi::xml_node extrinsic) {
    // XDS Name is an i18n container: <Name><LocalizedString value="..."/></Name>.
    // Pick the first LocalizedString regardless of xml:lang - callers that
    // need a specific locale can re-walk the tree, but the common case is
    // "whatever the registry emitted".
    auto ls = extrinsic.select_node(
        "*[local-name()='Name']/"
        "*[local-name()='LocalizedString']").node();
    return ls.attribute("value").as_string();
}

std::uint64_t parse_size_slot(pugi::xml_node extrinsic) {
    const std::string s = get_slot_value(extrinsic, "size");
    if (s.empty()) return 0;
    // Guard against malformed slot values: strtoull returns 0 on failure
    // which collides with the "omitted" sentinel but is the safer pick.
    // Callers that require strict size reporting should re-parse the
    // raw envelope themselves.
    char* end = nullptr;
    const auto v = std::strtoull(s.c_str(), &end, 10);
    if (end == s.c_str()) return 0;
    return static_cast<std::uint64_t>(v);
}

registry_document_entry extract_entry(pugi::xml_node extrinsic) {
    registry_document_entry e;
    e.entry_uuid = extrinsic.attribute("id").as_string();
    e.mime_type = extrinsic.attribute("mimeType").as_string();
    e.status = extrinsic.attribute("status").as_string();
    e.home_community_id = extrinsic.attribute("home").as_string();

    e.unique_id = get_external_identifier(extrinsic, kExtIdUniqueId);
    e.patient_id = get_external_identifier(extrinsic, kExtIdPatientId);

    e.creation_time = get_slot_value(extrinsic, "creationTime");
    e.repository_unique_id =
        get_slot_value(extrinsic, "repositoryUniqueId");
    e.hash = get_slot_value(extrinsic, "hash");
    e.author_person = get_slot_value(extrinsic, "authorPerson");
    e.size_bytes = parse_size_slot(extrinsic);

    e.title = get_localized_name(extrinsic);
    e.class_code = get_classification_code(extrinsic, kSchemeClassCode);
    e.format_code = get_classification_code(extrinsic, kSchemeFormatCode);
    e.type_code = get_classification_code(extrinsic, kSchemeTypeCode);

    return e;
}

}  // namespace

kcenon::common::Result<registry_query_result> parse_query_response(
    const std::string& root_xml) {
    if (root_xml.size() > kMaxRootXmlBytes) {
        return kcenon::common::make_error<registry_query_result>(
            static_cast<int>(error_code::transport_response_too_large),
            "AdhocQueryResponse root part exceeds 8 MiB cap (" +
                std::to_string(root_xml.size()) + " bytes)",
            std::string(error_source));
    }

    pugi::xml_document doc;
    auto parse = doc.load_buffer(root_xml.data(), root_xml.size(),
                                 pugi::parse_default, pugi::encoding_utf8);
    if (!parse) {
        return kcenon::common::make_error<registry_query_result>(
            static_cast<int>(error_code::transport_invalid_response),
            std::string("failed to parse response envelope: ") +
                parse.description(),
            std::string(error_source));
    }

    // Use local-name() selectors so we do not depend on the specific
    // namespace prefix the registry emits. ebRS stacks (Apache CXF,
    // .NET WIF, Gazelle, Open Health Tools) pick different prefixes for
    // the query and rim namespaces.
    auto adhoc_response =
        doc.select_node(
               "//*[local-name()='AdhocQueryResponse']").node();
    if (!adhoc_response) {
        return kcenon::common::make_error<registry_query_result>(
            static_cast<int>(error_code::registry_query_response_malformed),
            "response contains no AdhocQueryResponse element",
            std::string(error_source));
    }

    const std::string status =
        adhoc_response.attribute("status").as_string();
    if (status.empty()) {
        return kcenon::common::make_error<registry_query_result>(
            static_cast<int>(error_code::registry_query_response_malformed),
            "AdhocQueryResponse is missing status attribute",
            std::string(error_source));
    }
    // PartialSuccess means "some entries returned with warnings" in
    // IHE ITI-18 - the registry has produced a valid RegistryObjectList
    // alongside a RegistryErrorList. An earlier draft returned
    // registry_partial_success as an error, which dropped the entries
    // and contradicted the public-header contract that callers branch on
    // entries.empty() and inspect registry_response_status. Treat
    // PartialSuccess as a non-fatal status: fall through to the
    // ExtrinsicObject walk and surface the raw status string in the
    // returned result so callers can distinguish Success from
    // PartialSuccess if their policy differs (review M2).
    if (status != kSuccessStatus && status != kPartialSuccessStatus) {
        return kcenon::common::make_error<registry_query_result>(
            static_cast<int>(error_code::registry_failure_response),
            "registry reported non-success status: " + status,
            std::string(error_source));
    }

    registry_query_result out;
    out.registry_response_status = status;

    // Narrow child xpath (review m6): match ExtrinsicObject children of
    // RegistryObjectList only, not arbitrary descendants. This avoids
    // picking up nested ExtrinsicObject elements that could appear inside
    // a RegistryError's detail payload on non-conformant responses.
    // The RegistryObjectList is optional when no entries match; a Success
    // response with zero results is valid and maps to out.entries.empty().
    for (auto node : adhoc_response.select_nodes(
             "*[local-name()='RegistryObjectList']/"
             "*[local-name()='ExtrinsicObject']")) {
        out.entries.push_back(extract_entry(node.node()));
    }

    return out;
}

}  // namespace kcenon::pacs::ihe::xds::detail
