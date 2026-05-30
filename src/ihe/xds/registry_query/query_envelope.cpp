// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file query_envelope.cpp
 * @brief pugixml-backed SOAP 1.2 envelope builder for ITI-18.
 */

#include "query_envelope.h"

#include <pugixml.hpp>

#include <chrono>
#include <cstdint>
#include <random>
#include <sstream>
#include <string>
#include <string_view>

namespace kcenon::pacs::ihe::xds::detail {

namespace {

constexpr const char* kSoapEnv = "http://www.w3.org/2003/05/soap-envelope";
constexpr const char* kWsa = "http://www.w3.org/2005/08/addressing";
constexpr const char* kWsse =
    "http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd";
constexpr const char* kWsu =
    "http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd";
constexpr const char* kQuery = "urn:oasis:names:tc:ebxml-regrep:xsd:query:3.0";
constexpr const char* kRim = "urn:oasis:names:tc:ebxml-regrep:xsd:rim:3.0";

// Stored query IDs from IHE ITI TF-2a §3.18.4.1.2.3.7 (FindDocuments)
// and §3.18.4.1.2.3.8 (GetDocuments). These must match the spec bit-exactly
// or the registry returns "XDSUnknownStoredQuery".
constexpr const char* kFindDocumentsQueryId =
    "urn:uuid:14d4debf-8f97-4251-9a1e-a3a9d68b2a6f";
constexpr const char* kGetDocumentsQueryId =
    "urn:uuid:5737b14c-8a1a-4539-b659-e03a34a5e1e4";

// Default status filter applied by FindDocuments when the caller does not
// override it. "Approved" is the only state that represents a document
// published and usable in the affinity domain.
constexpr const char* kDefaultApprovedStatus =
    "urn:oasis:names:tc:ebxml-regrep:StatusType:Approved";

// ebRIM StoredQuery slot values are wrapped in a list literal
// ('value1','value2',...) that uses single quotes as the delimiter with no
// defined escape mechanism. A value containing one of these characters
// would either break out of the literal (') or fracture the comma-separated
// list (,) and let a caller forge additional slot values. Reject rather
// than escape - the XDS ID shapes that legitimately appear in the slots
// (CX patient id, OID-based unique id, urn:uuid: entry uuid) never contain
// these characters in conformant metadata, so rejection closes the
// injection vector without rejecting real traffic.
//
// We also reject control characters and the XML-significant bytes that
// pugixml's text setter does not escape automatically (pugixml escapes
// <, >, &, " in text nodes but not ' or NUL). This keeps the responsibility
// at the input-validation boundary rather than relying on downstream
// serializer behavior.
bool contains_slot_literal_metachar(std::string_view v) {
    for (char c : v) {
        if (c == '\'' || c == '(' || c == ')' || c == ',') return true;
        // Control characters below 0x20 break ebRIM XML text nodes. Tab,
        // CR, LF are technically allowed in XML but not in IHE XDS
        // identifier fields, which are flat ASCII.
        if (static_cast<unsigned char>(c) < 0x20) return true;
    }
    return false;
}

// Per-envelope id generator; matches the approach used by retrieve_envelope
// so logs from both actors share the same id shape. Not RFC 4122 compliant
// but sufficient for an xml:id whose lifetime is a single message.
std::string make_xml_id(std::string_view prefix) {
    static thread_local std::mt19937_64 rng(
        static_cast<std::uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::uniform_int_distribution<std::uint64_t> dist;
    std::ostringstream oss;
    oss << prefix << '-' << std::hex << dist(rng) << dist(rng);
    return oss.str();
}

std::string utc_iso8601_now() {
    const auto now = std::chrono::system_clock::now();
    const auto tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &tt);
#else
    gmtime_r(&tt, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return std::string(buf);
}

struct xml_string_writer : pugi::xml_writer {
    std::string out;
    void write(const void* data, std::size_t size) override {
        out.append(static_cast<const char*>(data), size);
    }
};

// Emit an ebRIM Slot with name @p slot_name carrying one single-quoted
// Value. The StoredQuery wire format wraps scalar slot values as
// "('value')" - a one-element list literal that ebRS parsers reduce back
// to the scalar. The quote character is single because the registry's
// query parser requires it; double quotes are a parse error in at least
// one conformant stack (Open Health Tools).
void append_slot_single(pugi::xml_node parent, const char* slot_name,
                        const std::string& value) {
    auto slot = parent.append_child("rim:Slot");
    slot.append_attribute("name") = slot_name;
    auto values = slot.append_child("rim:ValueList");
    auto v = values.append_child("rim:Value");
    std::string quoted = "('";
    quoted += value;
    quoted += "')";
    v.text().set(quoted.c_str());
}

// Emit an ebRIM Slot with name @p slot_name carrying one Value whose text
// is a list literal of the given strings. For GetDocuments, the registry
// expects ('uuid1','uuid2',...) - comma-separated, each element
// single-quoted, no spaces around commas.
void append_slot_list(pugi::xml_node parent, const char* slot_name,
                      const std::vector<std::string>& values) {
    auto slot = parent.append_child("rim:Slot");
    slot.append_attribute("name") = slot_name;
    auto value_list = slot.append_child("rim:ValueList");
    auto v = value_list.append_child("rim:Value");
    std::string text = "(";
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) text += ",";
        text += "'";
        text += values[i];
        text += "'";
    }
    text += ")";
    v.text().set(text.c_str());
}

}  // namespace

kcenon::common::Result<built_envelope> build_iti18_envelope(
    const registry_query_request& req, stored_query_kind kind) {
    if (kind == stored_query_kind::find_documents) {
        if (req.patient_id.empty()) {
            return kcenon::common::make_error<built_envelope>(
                static_cast<int>(
                    error_code::registry_query_missing_patient_id),
                "registry_query_request.patient_id is empty",
                std::string(error_source));
        }
        if (contains_slot_literal_metachar(req.patient_id)) {
            return kcenon::common::make_error<built_envelope>(
                static_cast<int>(
                    error_code::registry_query_invalid_patient_id),
                "patient_id contains a character that is not legal inside "
                "an ebRIM StoredQuery slot literal ('(),\\' or control "
                "character)",
                std::string(error_source));
        }
        for (const auto& s : req.options.status_values) {
            if (contains_slot_literal_metachar(s)) {
                return kcenon::common::make_error<built_envelope>(
                    static_cast<int>(
                        error_code::registry_query_invalid_patient_id),
                    "options.status_values contains an element with a "
                    "character that is not legal inside an ebRIM slot "
                    "literal",
                    std::string(error_source));
            }
        }
        if (contains_slot_literal_metachar(req.options.creation_time_from) ||
            contains_slot_literal_metachar(req.options.creation_time_to)) {
            return kcenon::common::make_error<built_envelope>(
                static_cast<int>(
                    error_code::registry_query_invalid_patient_id),
                "options.creation_time_* contains a character that is not "
                "legal inside an ebRIM slot literal",
                std::string(error_source));
        }
    }
    if (kind == stored_query_kind::get_documents) {
        if (req.document_uuids.empty()) {
            return kcenon::common::make_error<built_envelope>(
                static_cast<int>(
                    error_code::registry_query_empty_uuid_list),
                "registry_query_request.document_uuids is empty",
                std::string(error_source));
        }
        for (const auto& u : req.document_uuids) {
            if (u.empty()) {
                return kcenon::common::make_error<built_envelope>(
                    static_cast<int>(
                        error_code::registry_query_missing_document_uuid),
                    "registry_query_request.document_uuids contains an "
                    "empty element",
                    std::string(error_source));
            }
            if (contains_slot_literal_metachar(u)) {
                return kcenon::common::make_error<built_envelope>(
                    static_cast<int>(
                        error_code::registry_query_missing_document_uuid),
                    "registry_query_request.document_uuids contains an "
                    "element with a character that is not legal inside "
                    "an ebRIM StoredQuery slot literal",
                    std::string(error_source));
            }
        }
    }

    pugi::xml_document doc;
    auto decl = doc.append_child(pugi::node_declaration);
    decl.append_attribute("version") = "1.0";
    decl.append_attribute("encoding") = "UTF-8";

    auto env = doc.append_child("soap:Envelope");
    env.append_attribute("xmlns:soap") = kSoapEnv;
    env.append_attribute("xmlns:wsa") = kWsa;
    env.append_attribute("xmlns:wsse") = kWsse;
    env.append_attribute("xmlns:wsu") = kWsu;
    env.append_attribute("xmlns:query") = kQuery;
    env.append_attribute("xmlns:rim") = kRim;

    auto header = env.append_child("soap:Header");

    auto action = header.append_child("wsa:Action");
    action.append_attribute("soap:mustUnderstand") = "true";
    action.text().set("urn:ihe:iti:2007:RegistryStoredQuery");

    auto message_id = header.append_child("wsa:MessageID");
    message_id.text().set(("urn:uuid:" + make_xml_id("msg")).c_str());

    auto security = header.append_child("wsse:Security");
    security.append_attribute("soap:mustUnderstand") = "true";

    built_envelope out{};
    out.timestamp_id = make_xml_id("ts");
    out.binary_security_token_id = make_xml_id("bst");
    out.body_id = make_xml_id("body");

    auto timestamp = security.append_child("wsu:Timestamp");
    timestamp.append_attribute("wsu:Id") = out.timestamp_id.c_str();
    auto created = timestamp.append_child("wsu:Created");
    const std::string now = utc_iso8601_now();
    created.text().set(now.c_str());

    auto bst = security.append_child("wsse:BinarySecurityToken");
    bst.append_attribute("wsu:Id") = out.binary_security_token_id.c_str();
    bst.append_attribute("EncodingType") =
        "http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-"
        "security-1.0#Base64Binary";
    bst.append_attribute("ValueType") =
        "http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-x509-token-"
        "profile-1.0#X509v3";

    auto body = env.append_child("soap:Body");
    body.append_attribute("wsu:Id") = out.body_id.c_str();

    auto adhoc_request = body.append_child("query:AdhocQueryRequest");
    // ReturnType=LeafClass is the only form the XDS.b profile of ebRS
    // accepts for FindDocuments/GetDocuments. ObjectRef returns IDs only
    // and is unusable for the registry_query_result we assemble later.
    auto response_option =
        adhoc_request.append_child("query:ResponseOption");
    response_option.append_attribute("returnComposedObjects") = "true";
    response_option.append_attribute("returnType") = "LeafClass";

    auto adhoc_query = adhoc_request.append_child("rim:AdhocQuery");
    adhoc_query.append_attribute("id") =
        (kind == stored_query_kind::find_documents) ? kFindDocumentsQueryId
                                                    : kGetDocumentsQueryId;

    if (kind == stored_query_kind::find_documents) {
        append_slot_single(adhoc_query, "$XDSDocumentEntryPatientId",
                           req.patient_id);
        if (req.options.status_values.empty()) {
            append_slot_single(adhoc_query, "$XDSDocumentEntryStatus",
                               kDefaultApprovedStatus);
        } else {
            append_slot_list(adhoc_query, "$XDSDocumentEntryStatus",
                             req.options.status_values);
        }
        if (!req.options.creation_time_from.empty()) {
            append_slot_single(adhoc_query,
                               "$XDSDocumentEntryCreationTimeFrom",
                               req.options.creation_time_from);
        }
        if (!req.options.creation_time_to.empty()) {
            append_slot_single(adhoc_query,
                               "$XDSDocumentEntryCreationTimeTo",
                               req.options.creation_time_to);
        }
    } else {
        append_slot_list(adhoc_query, "$XDSDocumentEntryEntryUUID",
                         req.document_uuids);
    }

    xml_string_writer writer;
    doc.save(writer, "", pugi::format_raw | pugi::format_no_declaration,
             pugi::encoding_utf8);
    if (writer.out.empty()) {
        return kcenon::common::make_error<built_envelope>(
            static_cast<int>(error_code::envelope_serialization_failed),
            "pugixml produced an empty envelope",
            std::string(error_source));
    }

    out.xml = R"(<?xml version="1.0" encoding="UTF-8"?>)";
    out.xml += writer.out;
    return out;
}

}  // namespace kcenon::pacs::ihe::xds::detail
