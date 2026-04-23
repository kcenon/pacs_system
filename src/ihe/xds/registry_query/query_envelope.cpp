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
constexpr const char* kEbQuery =
    "urn:oasis:names:tc:ebxml-regrep:xsd:query:3.0";
constexpr const char* kEbRim =
    "urn:oasis:names:tc:ebxml-regrep:xsd:rim:3.0";

// Stored query ids defined in IHE ITI TF-2a §3.18.4.1.2.3.7.
constexpr const char* kFindDocumentsQueryId =
    "urn:uuid:14d4debf-8f97-4251-9a1e-8aabd7b6f011";
constexpr const char* kGetDocumentsQueryId =
    "urn:uuid:5c4f972b-d56b-40ac-a5fc-c8ca9b40b9d4";
constexpr const char* kIti18Action =
    "urn:ihe:iti:2007:RegistryStoredQuery";

// Id generator kept local to match retrieve_envelope.cpp's policy: not
// RFC 4122 compliant, but unique-per-envelope within a process, which is
// what wss_signer needs for xml:id referencing.
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

// Wrap each raw value in single quotes and join with commas so the
// resulting slot looks like "('v1','v2')" — the ebRS stored-query list
// grammar (ITI TF-2a §3.18.4.1.2.3.5).
std::string format_quoted_list(const std::vector<std::string>& values) {
    std::string out = "(";
    bool first = true;
    for (const auto& v : values) {
        if (!first) out += ',';
        first = false;
        out += '\'';
        out += v;
        out += '\'';
    }
    out += ')';
    return out;
}

// Scaffolds the SOAP envelope, header (wsa:Action + wsa:MessageID +
// wsse:Security stub), and empty soap:Body with wsu:Id attached. Returns
// the node pointing at soap:Body so callers can append their request
// element directly.
pugi::xml_node build_envelope_shell(pugi::xml_document& doc,
                                    built_envelope& out) {
    auto decl = doc.append_child(pugi::node_declaration);
    decl.append_attribute("version") = "1.0";
    decl.append_attribute("encoding") = "UTF-8";

    auto env = doc.append_child("soap:Envelope");
    env.append_attribute("xmlns:soap") = kSoapEnv;
    env.append_attribute("xmlns:wsa") = kWsa;
    env.append_attribute("xmlns:wsse") = kWsse;
    env.append_attribute("xmlns:wsu") = kWsu;
    env.append_attribute("xmlns:query") = kEbQuery;
    env.append_attribute("xmlns:rim") = kEbRim;

    auto header = env.append_child("soap:Header");

    auto action = header.append_child("wsa:Action");
    action.append_attribute("soap:mustUnderstand") = "true";
    action.text().set(kIti18Action);

    auto message_id = header.append_child("wsa:MessageID");
    message_id.text().set(("urn:uuid:" + make_xml_id("msg")).c_str());

    auto security = header.append_child("wsse:Security");
    security.append_attribute("soap:mustUnderstand") = "true";

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
    return body;
}

// Emit one <rim:Slot name="..."><rim:ValueList><rim:Value>...</rim:Value>...
// </rim:ValueList></rim:Slot> under the supplied parent.
void append_slot(pugi::xml_node parent, const char* name,
                 const std::string& value) {
    auto slot = parent.append_child("rim:Slot");
    slot.append_attribute("name") = name;
    auto value_list = slot.append_child("rim:ValueList");
    auto v = value_list.append_child("rim:Value");
    v.text().set(value.c_str());
}

kcenon::common::Result<built_envelope> serialize(pugi::xml_document& doc,
                                                 built_envelope& out) {
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

}  // namespace

kcenon::common::Result<built_envelope> build_iti18_find_documents_envelope(
    const std::string& patient_id,
    const std::vector<std::string>& statuses) {
    if (patient_id.empty()) {
        return kcenon::common::make_error<built_envelope>(
            static_cast<int>(error_code::consumer_query_missing_patient_id),
            "find_documents: patient_id is empty",
            std::string(error_source));
    }

    pugi::xml_document doc;
    built_envelope out{};
    auto body = build_envelope_shell(doc, out);

    auto req = body.append_child("query:AdhocQueryRequest");
    auto rs_response_option = req.append_child("query:ResponseOption");
    rs_response_option.append_attribute("returnComposedObjects") = "true";
    rs_response_option.append_attribute("returnType") = "LeafClass";

    auto adhoc = req.append_child("rim:AdhocQuery");
    adhoc.append_attribute("id") = kFindDocumentsQueryId;

    // Slot order is not semantically required by the ebRS grammar but
    // matches the examples in the IHE TF to minimize reviewer friction.
    append_slot(adhoc, "$XDSDocumentEntryPatientId",
                std::string("'") + patient_id + "'");

    if (!statuses.empty()) {
        append_slot(adhoc, "$XDSDocumentEntryStatus",
                    format_quoted_list(statuses));
    }

    return serialize(doc, out);
}

kcenon::common::Result<built_envelope> build_iti18_get_documents_envelope(
    const std::vector<std::string>& uuids) {
    if (uuids.empty()) {
        return kcenon::common::make_error<built_envelope>(
            static_cast<int>(error_code::consumer_query_empty_uuid_list),
            "get_documents: uuids list is empty",
            std::string(error_source));
    }

    pugi::xml_document doc;
    built_envelope out{};
    auto body = build_envelope_shell(doc, out);

    auto req = body.append_child("query:AdhocQueryRequest");
    auto rs_response_option = req.append_child("query:ResponseOption");
    rs_response_option.append_attribute("returnComposedObjects") = "true";
    rs_response_option.append_attribute("returnType") = "LeafClass";

    auto adhoc = req.append_child("rim:AdhocQuery");
    adhoc.append_attribute("id") = kGetDocumentsQueryId;

    append_slot(adhoc, "$XDSDocumentEntryUUID", format_quoted_list(uuids));

    return serialize(doc, out);
}

}  // namespace kcenon::pacs::ihe::xds::detail
