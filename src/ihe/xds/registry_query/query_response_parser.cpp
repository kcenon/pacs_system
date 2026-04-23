// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file query_response_parser.cpp
 * @brief pugixml-backed parser for ITI-18 query:AdhocQueryResponse.
 */

#include "query_response_parser.h"

#include <pugixml.hpp>

#include <string>
#include <string_view>

namespace kcenon::pacs::ihe::xds::detail {

namespace {

// Mirrors retrieve_response_parser.cpp: the http_client write callback
// already bounds the response to 8 MiB, and the signing path would also
// have rejected anything larger. Keep a defensive bound here in case a
// future non-libcurl transport is wired without the same cap.
constexpr std::size_t kMaxRootXmlBytes = 8 * 1024 * 1024;

constexpr std::string_view kSuccessStatus =
    "urn:oasis:names:tc:ebxml-regrep:ResponseStatusType:Success";
constexpr std::string_view kPartialSuccessStatus =
    "urn:oasis:names:tc:ebxml-regrep:ResponseStatusType:PartialSuccess";

// XDSDocumentEntry well-known classification / external identifier
// scheme UUIDs. The Registry emits these as @identificationScheme on the
// ExternalIdentifier children we walk to pull out uniqueId and patientId.
constexpr std::string_view kExtIdUniqueIdScheme =
    "urn:uuid:2e82c1f6-a085-4c72-9da3-8640a32e42ab";
constexpr std::string_view kExtIdPatientIdScheme =
    "urn:uuid:58a6f841-87b3-4a3e-92fd-a8ffeff98427";

// Return the value inside a rim:Slot's first <rim:Value> child, or "" if
// the slot or ValueList is missing. The Registry may emit a slot with
// multiple Values (e.g. multi-valued author fields); callers that care
// about those should walk select_nodes themselves.
std::string slot_first_value(const pugi::xml_node& parent,
                             const char* slot_name) {
    const std::string xpath = std::string("*[local-name()='Slot' and @name='") +
                              slot_name + "']/*[local-name()='ValueList']/"
                              "*[local-name()='Value']";
    return parent.select_node(xpath.c_str())
        .node()
        .text()
        .as_string();
}

// Find an ExternalIdentifier child by its @identificationScheme and return
// its @value. Used to pick out XDSDocumentEntry.uniqueId and patientId.
std::string external_identifier_value(const pugi::xml_node& extrinsic,
                                      std::string_view scheme) {
    for (auto ei :
         extrinsic.select_nodes("*[local-name()='ExternalIdentifier']")) {
        auto node = ei.node();
        const std::string_view s =
            node.attribute("identificationScheme").as_string();
        if (s == scheme) {
            return node.attribute("value").as_string();
        }
    }
    return std::string{};
}

}  // namespace

kcenon::common::Result<registry_query_result> parse_query_response(
    const std::string& root_xml) {
    if (root_xml.size() > kMaxRootXmlBytes) {
        return kcenon::common::make_error<registry_query_result>(
            static_cast<int>(error_code::transport_response_too_large),
            "AdhocQueryResponse body exceeds 8 MiB cap (" +
                std::to_string(root_xml.size()) + " bytes)",
            std::string(error_source));
    }

    pugi::xml_document doc;
    auto parse = doc.load_buffer(root_xml.data(), root_xml.size(),
                                 pugi::parse_default, pugi::encoding_utf8);
    if (!parse) {
        return kcenon::common::make_error<registry_query_result>(
            static_cast<int>(error_code::consumer_query_response_parse_failed),
            std::string("failed to parse AdhocQueryResponse envelope: ") +
                parse.description(),
            std::string(error_source));
    }

    // local-name() selectors: the Registry can legitimately use any
    // prefix for the query / rim / rs namespaces, and different stacks
    // (Apache CXF, .NET WIF, Gazelle) all pick different prefixes.
    auto response =
        doc.select_node(
               "//*[local-name()='AdhocQueryResponse']").node();
    if (!response) {
        return kcenon::common::make_error<registry_query_result>(
            static_cast<int>(error_code::consumer_query_response_parse_failed),
            "response contains no AdhocQueryResponse element",
            std::string(error_source));
    }

    const std::string status = response.attribute("status").as_string();
    if (status.empty()) {
        return kcenon::common::make_error<registry_query_result>(
            static_cast<int>(error_code::consumer_query_response_parse_failed),
            "AdhocQueryResponse is missing status attribute",
            std::string(error_source));
    }
    if (status == kPartialSuccessStatus) {
        return kcenon::common::make_error<registry_query_result>(
            static_cast<int>(error_code::registry_partial_success),
            "registry reported PartialSuccess: " + status,
            std::string(error_source));
    }
    if (status != kSuccessStatus) {
        return kcenon::common::make_error<registry_query_result>(
            static_cast<int>(error_code::consumer_query_response_parse_failed),
            "registry reported non-success status: " + status,
            std::string(error_source));
    }

    registry_query_result out;
    out.registry_response_status = status;

    // No RegistryObjectList = zero matches. That is a legal Success
    // response in the ebRS grammar (TF-2a §3.18.4.1.3) and is surfaced
    // as an ok Result with an empty entries vector.
    auto registry_object_list =
        response.select_node("*[local-name()='RegistryObjectList']").node();
    if (!registry_object_list) {
        return out;
    }

    for (auto eo : registry_object_list.select_nodes(
             "*[local-name()='ExtrinsicObject']")) {
        auto node = eo.node();
        document_entry entry;
        entry.entry_uuid = node.attribute("id").as_string();
        entry.mime_type = node.attribute("mimeType").as_string();
        entry.status = node.attribute("status").as_string();

        entry.document_unique_id =
            external_identifier_value(node, kExtIdUniqueIdScheme);
        entry.patient_id =
            external_identifier_value(node, kExtIdPatientIdScheme);

        entry.creation_time = slot_first_value(node, "creationTime");
        entry.repository_unique_id =
            slot_first_value(node, "repositoryUniqueId");

        out.entries.push_back(std::move(entry));
    }

    return out;
}

}  // namespace kcenon::pacs::ihe::xds::detail
