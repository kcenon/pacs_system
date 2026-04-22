// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file retrieve_envelope.cpp
 * @brief pugixml-backed SOAP 1.2 envelope builder for ITI-43.
 */

#include "retrieve_envelope.h"

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
constexpr const char* kXdsb = "urn:ihe:iti:xds-b:2007";

// Per-envelope id generator; not RFC 4122 compliant. See the matching
// helper in soap_envelope.cpp for rationale.
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

}  // namespace

kcenon::common::Result<built_envelope> build_iti43_envelope(
    const retrieve_request& req) {
    if (req.document_unique_id.empty()) {
        return kcenon::common::make_error<built_envelope>(
            static_cast<int>(
                error_code::consumer_retrieve_missing_document_id),
            "retrieve_request.document_unique_id is empty",
            std::string(error_source));
    }
    if (req.repository_unique_id.empty()) {
        return kcenon::common::make_error<built_envelope>(
            static_cast<int>(
                error_code::consumer_retrieve_missing_repository_id),
            "retrieve_request.repository_unique_id is empty",
            std::string(error_source));
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
    env.append_attribute("xmlns:xdsb") = kXdsb;

    auto header = env.append_child("soap:Header");

    auto action = header.append_child("wsa:Action");
    action.append_attribute("soap:mustUnderstand") = "true";
    action.text().set("urn:ihe:iti:2007:RetrieveDocumentSet");

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

    auto rq = body.append_child("xdsb:RetrieveDocumentSetRequest");
    auto doc_req = rq.append_child("xdsb:DocumentRequest");

    if (!req.home_community_id.empty()) {
        doc_req.append_child("xdsb:HomeCommunityId")
            .text()
            .set(req.home_community_id.c_str());
    }
    doc_req.append_child("xdsb:RepositoryUniqueId")
        .text()
        .set(req.repository_unique_id.c_str());
    doc_req.append_child("xdsb:DocumentUniqueId")
        .text()
        .set(req.document_unique_id.c_str());

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
