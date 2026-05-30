// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file soap_envelope.cpp
 * @brief pugixml-backed SOAP 1.2 envelope builder for ITI-41.
 */

#include "soap_envelope.h"

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
constexpr const char* kLcm =
    "urn:oasis:names:tc:ebxml-regrep:xsd:lcm:3.0";
constexpr const char* kRim =
    "urn:oasis:names:tc:ebxml-regrep:xsd:rim:3.0";
constexpr const char* kXop = "http://www.w3.org/2004/08/xop/include";

constexpr const char* kSubmissionSetClassification =
    "urn:uuid:a54d6aa5-d40d-43f9-88c5-b4633d873bdd";
constexpr const char* kSubmissionSetIdScheme =
    "urn:uuid:554ac39e-e3fe-47fe-b233-965d2a147832";
constexpr const char* kDocumentEntryIdScheme =
    "urn:uuid:2e82c1f6-a085-4c72-9da3-8640a32e42ab";

/**
 * @brief Generate a random UUID-like id. Not RFC 4122 compliant - it is
 *        only used for xml:id attributes that need to be unique within the
 *        envelope, never persisted. OpenSSL's RNG is deliberately not used
 *        here to keep this helper self-contained; the XML-DSig layer seeds
 *        its own randomness.
 */
std::string make_xml_id(std::string_view prefix) {
    static thread_local std::mt19937_64 rng(
        static_cast<std::uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::uniform_int_distribution<std::uint64_t> dist;
    std::ostringstream oss;
    oss << prefix << '-' << std::hex << dist(rng) << dist(rng);
    return oss.str();
}

/**
 * @brief Format the current UTC time as ISO 8601 with seconds and 'Z'.
 */
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

void add_classification(pugi::xml_node parent, const char* classification_scheme,
                        const char* classified_object,
                        const char* node_representation,
                        const std::string& id_prefix) {
    auto cls = parent.append_child("rim:Classification");
    cls.append_attribute("classificationScheme") = classification_scheme;
    cls.append_attribute("classifiedObject") = classified_object;
    cls.append_attribute("nodeRepresentation") = node_representation;
    cls.append_attribute("id") = make_xml_id(id_prefix).c_str();
    cls.append_attribute("objectType") =
        "urn:oasis:names:tc:ebxml-regrep:ObjectType:RegistryObject:Classification";
}

void add_external_identifier(pugi::xml_node parent, const char* scheme,
                             const char* registry_object, const char* value,
                             const char* name) {
    auto eid = parent.append_child("rim:ExternalIdentifier");
    eid.append_attribute("identificationScheme") = scheme;
    eid.append_attribute("registryObject") = registry_object;
    eid.append_attribute("value") = value;
    eid.append_attribute("id") = make_xml_id("eid").c_str();
    eid.append_attribute("objectType") =
        "urn:oasis:names:tc:ebxml-regrep:ObjectType:RegistryObject:ExternalIdentifier";
    auto name_node = eid.append_child("rim:Name");
    auto loc = name_node.append_child("rim:LocalizedString");
    loc.append_attribute("value") = name;
}

void add_registry_package(pugi::xml_node leaf_list, const submission_set& s) {
    auto rp = leaf_list.append_child("rim:RegistryPackage");
    rp.append_attribute("id") = s.entry_uuid.c_str();
    rp.append_attribute("objectType") =
        "urn:oasis:names:tc:ebxml-regrep:ObjectType:RegistryObject:RegistryPackage";

    if (!s.author_person.empty() || !s.author_institution.empty()) {
        auto cls = rp.append_child("rim:Classification");
        cls.append_attribute("classificationScheme") =
            "urn:uuid:a7058bb9-b4e4-4307-ba5b-e3f0ab85e12d";
        cls.append_attribute("classifiedObject") = s.entry_uuid.c_str();
        cls.append_attribute("nodeRepresentation") = "";
        cls.append_attribute("id") = make_xml_id("cls").c_str();
        cls.append_attribute("objectType") =
            "urn:oasis:names:tc:ebxml-regrep:ObjectType:RegistryObject:Classification";
        if (!s.author_person.empty()) {
            auto slot = cls.append_child("rim:Slot");
            slot.append_attribute("name") = "authorPerson";
            auto vl = slot.append_child("rim:ValueList");
            vl.append_child("rim:Value")
                .text()
                .set(s.author_person.c_str());
        }
        if (!s.author_institution.empty()) {
            auto slot = cls.append_child("rim:Slot");
            slot.append_attribute("name") = "authorInstitution";
            auto vl = slot.append_child("rim:ValueList");
            vl.append_child("rim:Value")
                .text()
                .set(s.author_institution.c_str());
        }
    }

    if (!s.submission_time.empty()) {
        auto slot = rp.append_child("rim:Slot");
        slot.append_attribute("name") = "submissionTime";
        auto vl = slot.append_child("rim:ValueList");
        vl.append_child("rim:Value").text().set(s.submission_time.c_str());
    }

    add_classification(rp, kSubmissionSetClassification,
                       s.entry_uuid.c_str(), "", "ssc");

    add_external_identifier(rp, kSubmissionSetIdScheme, s.entry_uuid.c_str(),
                            s.submission_set_unique_id.c_str(),
                            "XDSSubmissionSet.uniqueId");
    add_external_identifier(rp, kSubmissionSetIdScheme, s.entry_uuid.c_str(),
                            s.source_id.c_str(), "XDSSubmissionSet.sourceId");
    add_external_identifier(rp, kSubmissionSetIdScheme, s.entry_uuid.c_str(),
                            s.patient_id.c_str(), "XDSSubmissionSet.patientId");
}

void add_document_entry(pugi::xml_node leaf_list, const xds_document& doc,
                        const std::string& cid) {
    auto eo = leaf_list.append_child("rim:ExtrinsicObject");
    eo.append_attribute("id") = doc.entry_uuid.c_str();
    eo.append_attribute("mimeType") = doc.mime_type.c_str();
    eo.append_attribute("objectType") =
        "urn:uuid:7edca82f-054d-47f2-a032-9b2a5b5186c1";

    auto include = eo.append_child("xop:Include");
    include.append_attribute("href") = ("cid:" + cid).c_str();

    if (!doc.creation_time.empty()) {
        auto slot = eo.append_child("rim:Slot");
        slot.append_attribute("name") = "creationTime";
        auto vl = slot.append_child("rim:ValueList");
        vl.append_child("rim:Value").text().set(doc.creation_time.c_str());
    }
    if (!doc.language_code.empty()) {
        auto slot = eo.append_child("rim:Slot");
        slot.append_attribute("name") = "languageCode";
        auto vl = slot.append_child("rim:ValueList");
        vl.append_child("rim:Value").text().set(doc.language_code.c_str());
    }
    if (!doc.title.empty()) {
        auto name_node = eo.append_child("rim:Name");
        auto loc = name_node.append_child("rim:LocalizedString");
        loc.append_attribute("value") = doc.title.c_str();
    }

    add_external_identifier(eo, kDocumentEntryIdScheme,
                            doc.entry_uuid.c_str(), doc.unique_id.c_str(),
                            "XDSDocumentEntry.uniqueId");
}

struct xml_string_writer : pugi::xml_writer {
    std::string out;
    void write(const void* data, size_t size) override {
        out.append(static_cast<const char*>(data), size);
    }
};

}  // namespace

kcenon::common::Result<built_envelope> build_iti41_envelope(
    const submission_set& s, const std::vector<std::string>& cid_refs) {
    if (cid_refs.size() != s.documents.size()) {
        return kcenon::common::make_error<built_envelope>(
            static_cast<int>(error_code::envelope_build_failed),
            "content-id list size does not match document count",
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
    env.append_attribute("xmlns:lcm") = kLcm;
    env.append_attribute("xmlns:rim") = kRim;
    env.append_attribute("xmlns:xop") = kXop;

    auto header = env.append_child("soap:Header");

    auto action = header.append_child("wsa:Action");
    action.append_attribute("soap:mustUnderstand") = "true";
    action.text().set("urn:ihe:iti:2007:RegisterDocumentSet-b");

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

    auto req = body.append_child("lcm:SubmitObjectsRequest");

    auto leaf_list = req.append_child("rim:RegistryObjectList");

    if (s.patient_id.empty()) {
        return kcenon::common::make_error<built_envelope>(
            static_cast<int>(error_code::metadata_missing_patient_id),
            "submission_set.patient_id is empty",
            std::string(error_source));
    }
    if (s.source_id.empty()) {
        return kcenon::common::make_error<built_envelope>(
            static_cast<int>(error_code::metadata_missing_source_id),
            "submission_set.source_id is empty",
            std::string(error_source));
    }
    if (s.submission_set_unique_id.empty()) {
        return kcenon::common::make_error<built_envelope>(
            static_cast<int>(error_code::metadata_missing_submission_set_uid),
            "submission_set.submission_set_unique_id is empty",
            std::string(error_source));
    }
    if (s.documents.empty()) {
        return kcenon::common::make_error<built_envelope>(
            static_cast<int>(error_code::metadata_no_documents),
            "submission_set contains no documents",
            std::string(error_source));
    }

    for (std::size_t i = 0; i < s.documents.size(); ++i) {
        const auto& d = s.documents[i];
        if (d.unique_id.empty()) {
            return kcenon::common::make_error<built_envelope>(
                static_cast<int>(
                    error_code::metadata_document_missing_unique_id),
                "document[" + std::to_string(i) + "].unique_id is empty",
                std::string(error_source));
        }
        if (d.mime_type.empty()) {
            return kcenon::common::make_error<built_envelope>(
                static_cast<int>(
                    error_code::metadata_document_missing_mime_type),
                "document[" + std::to_string(i) + "].mime_type is empty",
                std::string(error_source));
        }
        if (d.content.empty()) {
            return kcenon::common::make_error<built_envelope>(
                static_cast<int>(error_code::metadata_document_empty),
                "document[" + std::to_string(i) + "].content is empty",
                std::string(error_source));
        }
        add_document_entry(leaf_list, d, cid_refs[i]);
    }

    add_registry_package(leaf_list, s);

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
