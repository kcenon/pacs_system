// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file registry_query_test.cpp
 * @brief End-to-end tests for the ITI-18 Registry Query actor.
 */

#include "../../../src/ihe/xds/common/http_client.h"
#include "../../../src/ihe/xds/common/soap_envelope.h"
#include "../../../src/ihe/xds/common/wss_signer.h"

#include <kcenon/pacs/ihe/xds/document_source.h>
#include <kcenon/pacs/ihe/xds/errors.h>
#include <kcenon/pacs/ihe/xds/http_options.h>
#include <kcenon/pacs/ihe/xds/registry_query.h>
#include <kcenon/pacs/ihe/xds/submission_set.h>

#include <catch2/catch_test_macros.hpp>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace kcenon::pacs::ihe::xds;

namespace {

struct bio_deleter {
    void operator()(BIO* b) const noexcept {
        if (b) BIO_free_all(b);
    }
};
using bio_ptr = std::unique_ptr<BIO, bio_deleter>;

struct pkey_deleter {
    void operator()(EVP_PKEY* p) const noexcept {
        if (p) EVP_PKEY_free(p);
    }
};
using pkey_ptr = std::unique_ptr<EVP_PKEY, pkey_deleter>;

struct x509_deleter {
    void operator()(X509* x) const noexcept {
        if (x) X509_free(x);
    }
};
using x509_ptr = std::unique_ptr<X509, x509_deleter>;

signing_material make_signing() {
    pkey_ptr pkey(EVP_RSA_gen(2048));
    REQUIRE(pkey);
    x509_ptr cert(X509_new());
    X509_set_version(cert.get(), 2);
    ASN1_INTEGER_set(X509_get_serialNumber(cert.get()), 1);
    X509_gmtime_adj(X509_getm_notBefore(cert.get()), 0);
    X509_gmtime_adj(X509_getm_notAfter(cert.get()), 60L * 60 * 24 * 30);
    X509_set_pubkey(cert.get(), pkey.get());
    X509_NAME* name = X509_get_subject_name(cert.get());
    X509_NAME_add_entry_by_txt(
        name, "CN", MBSTRING_ASC,
        reinterpret_cast<const unsigned char*>("kcenon-rq-test"), -1, -1, 0);
    X509_set_issuer_name(cert.get(), name);
    X509_sign(cert.get(), pkey.get(), EVP_sha256());

    signing_material s;
    bio_ptr cb(BIO_new(BIO_s_mem()));
    PEM_write_bio_X509(cb.get(), cert.get());
    char* buf = nullptr;
    long n = BIO_get_mem_data(cb.get(), &buf);
    s.certificate_pem.assign(buf, static_cast<std::size_t>(n));

    bio_ptr kb(BIO_new(BIO_s_mem()));
    PEM_write_bio_PrivateKey(kb.get(), pkey.get(), nullptr, nullptr, 0,
                             nullptr, nullptr);
    n = BIO_get_mem_data(kb.get(), &buf);
    s.private_key_pem.assign(buf, static_cast<std::size_t>(n));

    return s;
}

http_options make_opts() {
    http_options o;
    o.endpoint = "https://registry.example.test/iti18";
    return o;
}

constexpr const char* kPatientId =
    "7d3f2d0a^^^&1.2.276.0.7230010.3.1.2.999&ISO";
constexpr const char* kDocEntryUuid1 =
    "urn:uuid:aabbccdd-1111-2222-3333-444455556666";
constexpr const char* kDocEntryUuid2 =
    "urn:uuid:aabbccdd-1111-2222-3333-777788889999";
constexpr const char* kRepoUid = "1.2.276.0.7230010.3.0.3.6.2";

// Replace every occurrence of @p needle in @p s with @p value.
void replace_all(std::string& s, const std::string& needle,
                 const std::string& value) {
    for (;;) {
        const auto pos = s.find(needle);
        if (pos == std::string::npos) break;
        s.replace(pos, needle.size(), value);
    }
}

// Build and sign a RegistryStoredQueryResponse envelope containing one
// ExtrinsicObject. wsu:Id attributes on Body and Timestamp are required
// because sign_envelope references them by Id.
std::string build_signed_response_envelope(
    const std::string& status_urn, const std::vector<std::string>& entry_uuids,
    const signing_material& signer) {
    std::string extrinsic_blocks;
    for (std::size_t i = 0; i < entry_uuids.size(); ++i) {
        const auto& uid = entry_uuids[i];
        std::string block =
            R"(<rim:ExtrinsicObject id="{UID}" mimeType="application/dicom" )"
            R"(status="urn:oasis:names:tc:ebxml-regrep:StatusType:Approved" )"
            R"(objectType="urn:uuid:7edca82f-054d-47f2-a032-9b2a5b5186c1">)"
            R"(<rim:Slot name="creationTime"><rim:ValueList>)"
            R"(<rim:Value>20260422120000</rim:Value></rim:ValueList></rim:Slot>)"
            R"(<rim:Slot name="repositoryUniqueId"><rim:ValueList>)"
            R"(<rim:Value>{REPO}</rim:Value></rim:ValueList></rim:Slot>)"
            R"(<rim:Slot name="size"><rim:ValueList>)"
            R"(<rim:Value>1024</rim:Value></rim:ValueList></rim:Slot>)"
            R"(<rim:Slot name="authorPerson"><rim:ValueList>)"
            R"(<rim:Value>Dr. Smith</rim:Value></rim:ValueList></rim:Slot>)"
            R"(<rim:Name><rim:LocalizedString value="CT Abdomen {IDX}"/></rim:Name>)"
            R"(<rim:Classification classificationScheme=)"
            R"("urn:uuid:41a5887f-8865-4c09-adf7-e362475b143a" )"
            R"(nodeRepresentation="55113-5"/>)"
            R"(<rim:Classification classificationScheme=)"
            R"("urn:uuid:a09d5840-386c-46f2-b5ad-9c3699a4309d" )"
            R"(nodeRepresentation="urn:ihe:iti:xds-sd:text:2008"/>)"
            R"(<rim:Classification classificationScheme=)"
            R"("urn:uuid:f0306f51-975f-434e-a61b-a3f66f70e3dc" )"
            R"(nodeRepresentation="34133-9"/>)"
            R"(<rim:ExternalIdentifier identificationScheme=)"
            R"("urn:uuid:2e82c1f6-a085-4c72-9da3-8640a32e42ab" )"
            R"(value="2.16.840.1.113883.3.42.1.{IDX}"/>)"
            R"(<rim:ExternalIdentifier identificationScheme=)"
            R"("urn:uuid:58a6f841-87b3-4a3e-92fd-a8ffeff98427" )"
            R"(value="{PATIENT}"/>)"
            R"(</rim:ExtrinsicObject>)";
        replace_all(block, "{UID}", uid);
        replace_all(block, "{REPO}", kRepoUid);
        replace_all(block, "{IDX}", std::to_string(i + 1));
        replace_all(block, "{PATIENT}", kPatientId);
        extrinsic_blocks += block;
    }

    std::string xml =
        R"(<?xml version="1.0" encoding="UTF-8"?>)"
        R"(<soap:Envelope xmlns:soap="http://www.w3.org/2003/05/soap-envelope" )"
        R"(xmlns:wsa="http://www.w3.org/2005/08/addressing" )"
        R"(xmlns:wsse="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd" )"
        R"(xmlns:wsu="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd" )"
        R"(xmlns:query="urn:oasis:names:tc:ebxml-regrep:xsd:query:3.0" )"
        R"(xmlns:rim="urn:oasis:names:tc:ebxml-regrep:xsd:rim:3.0">)"
        R"(<soap:Header>)"
        R"(<wsa:Action>urn:ihe:iti:2007:RegistryStoredQueryResponse</wsa:Action>)"
        R"(<wsse:Security soap:mustUnderstand="true">)"
        R"(<wsu:Timestamp wsu:Id="{TSID}">)"
        R"(<wsu:Created>2026-04-22T12:00:00Z</wsu:Created>)"
        R"(</wsu:Timestamp>)"
        R"(<wsse:BinarySecurityToken wsu:Id="{BSTID}" )"
        R"(EncodingType="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-security-1.0#Base64Binary" )"
        R"(ValueType="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-x509-token-profile-1.0#X509v3"/>)"
        R"(</wsse:Security>)"
        R"(</soap:Header>)"
        R"(<soap:Body wsu:Id="{BODYID}">)"
        R"(<query:AdhocQueryResponse status="{STATUS}">)"
        R"(<rim:RegistryObjectList>)"
        R"({EXTRINSICS})"
        R"(</rim:RegistryObjectList>)"
        R"(</query:AdhocQueryResponse>)"
        R"(</soap:Body>)"
        R"(</soap:Envelope>)";

    detail::built_envelope env;
    env.body_id = "body-rq-1";
    env.timestamp_id = "ts-rq-1";
    env.binary_security_token_id = "bst-rq-1";

    replace_all(xml, "{TSID}", env.timestamp_id);
    replace_all(xml, "{BSTID}", env.binary_security_token_id);
    replace_all(xml, "{BODYID}", env.body_id);
    replace_all(xml, "{STATUS}", status_urn);
    replace_all(xml, "{EXTRINSICS}", extrinsic_blocks);
    env.xml = std::move(xml);

    auto r = detail::sign_envelope(env, signer.certificate_pem,
                                   signer.private_key_pem, "");
    REQUIRE(r.is_ok());
    return env.xml;
}

struct scoped_transport_override {
    scoped_transport_override() = default;
    explicit scoped_transport_override(detail::transport_override fn) {
        detail::set_http_transport_override(std::move(fn));
    }
    ~scoped_transport_override() {
        detail::clear_http_transport_override();
    }
    scoped_transport_override(const scoped_transport_override&) = delete;
    scoped_transport_override& operator=(const scoped_transport_override&) =
        delete;
};

constexpr const char* kSuccessStatus =
    "urn:oasis:names:tc:ebxml-regrep:ResponseStatusType:Success";

}  // namespace

TEST_CASE("registry_query find_documents returns entries on success",
          "[ihe][xds][iti18][e2e]") {
    auto signer = make_signing();
    const std::string root_xml = build_signed_response_envelope(
        kSuccessStatus, {kDocEntryUuid1, kDocEntryUuid2}, signer);

    std::string captured_body;
    std::string captured_action;
    scoped_transport_override guard(
        [&](const detail::transport_request& req)
            -> kcenon::common::Result<detail::http_response> {
            captured_body = req.body;
            captured_action = req.soap_action;
            detail::http_response r;
            r.status_code = 200;
            r.content_type = "application/soap+xml; charset=UTF-8";
            r.body = root_xml;
            return r;
        });

    registry_query rq(make_opts(), signer);
    auto result = rq.find_documents(kPatientId);
    REQUIRE(result.is_ok());
    REQUIRE(result.value().registry_response_status == kSuccessStatus);
    REQUIRE(result.value().entries.size() == 2);

    const auto& first = result.value().entries[0];
    REQUIRE(first.entry_uuid == kDocEntryUuid1);
    REQUIRE(first.repository_unique_id == kRepoUid);
    REQUIRE(first.patient_id == kPatientId);
    REQUIRE(first.mime_type == "application/dicom");
    REQUIRE(first.creation_time == "20260422120000");
    REQUIRE(first.size_bytes == 1024);
    REQUIRE(first.unique_id == "2.16.840.1.113883.3.42.1.1");
    REQUIRE(first.class_code == "55113-5");
    REQUIRE(first.format_code == "urn:ihe:iti:xds-sd:text:2008");
    REQUIRE(first.title == "CT Abdomen 1");

    REQUIRE(captured_action == "urn:ihe:iti:2007:RegistryStoredQuery");
    REQUIRE(captured_body.find("query:AdhocQueryRequest") !=
            std::string::npos);
    REQUIRE(captured_body.find("urn:uuid:14d4debf-8f97-4251-9a1e-a3a9d68b2a6f") !=
            std::string::npos);
    REQUIRE(captured_body.find("$XDSDocumentEntryPatientId") !=
            std::string::npos);
    REQUIRE(captured_body.find("ds:Signature") != std::string::npos);
}

TEST_CASE("registry_query find_documents returns empty list on no results",
          "[ihe][xds][iti18][e2e]") {
    auto signer = make_signing();
    const std::string root_xml =
        build_signed_response_envelope(kSuccessStatus, {}, signer);

    scoped_transport_override guard(
        [&](const detail::transport_request&)
            -> kcenon::common::Result<detail::http_response> {
            detail::http_response r;
            r.status_code = 200;
            r.body = root_xml;
            return r;
        });

    registry_query rq(make_opts(), signer);
    auto result = rq.find_documents(kPatientId);
    REQUIRE(result.is_ok());
    REQUIRE(result.value().entries.empty());
    REQUIRE(result.value().registry_response_status == kSuccessStatus);
}

TEST_CASE("registry_query find_documents rejects empty patient_id",
          "[ihe][xds][iti18][e2e]") {
    scoped_transport_override guard(
        [](const detail::transport_request&)
            -> kcenon::common::Result<detail::http_response> {
            FAIL("transport should not be invoked when patient_id is empty");
            return detail::http_response{};
        });

    registry_query rq(make_opts(), make_signing());
    auto r = rq.find_documents("");
    REQUIRE(r.is_err());
    REQUIRE(r.error().code ==
            static_cast<int>(error_code::registry_query_missing_patient_id));
}

TEST_CASE("registry_query find_documents surfaces transport TLS errors",
          "[ihe][xds][iti18][e2e]") {
    scoped_transport_override guard(
        [](const detail::transport_request&)
            -> kcenon::common::Result<detail::http_response> {
            return kcenon::common::make_error<detail::http_response>(
                static_cast<int>(error_code::transport_tls_error),
                "simulated TLS handshake failure",
                std::string(error_source));
        });

    registry_query rq(make_opts(), make_signing());
    auto r = rq.find_documents(kPatientId);
    REQUIRE(r.is_err());
    REQUIRE(r.error().code ==
            static_cast<int>(error_code::transport_tls_error));
}

TEST_CASE("registry_query find_documents maps registry Failure status",
          "[ihe][xds][iti18][e2e]") {
    auto signer = make_signing();
    const std::string root_xml = build_signed_response_envelope(
        "urn:oasis:names:tc:ebxml-regrep:ResponseStatusType:Failure", {},
        signer);

    scoped_transport_override guard(
        [&](const detail::transport_request&)
            -> kcenon::common::Result<detail::http_response> {
            detail::http_response r;
            r.status_code = 200;
            r.body = root_xml;
            return r;
        });

    registry_query rq(make_opts(), signer);
    auto r = rq.find_documents(kPatientId);
    REQUIRE(r.is_err());
    REQUIRE(r.error().code ==
            static_cast<int>(error_code::registry_failure_response));
}

TEST_CASE("registry_query find_documents rejects tampered response",
          "[ihe][xds][iti18][e2e]") {
    auto signer = make_signing();
    std::string root_xml = build_signed_response_envelope(
        kSuccessStatus, {kDocEntryUuid1}, signer);

    // Tamper with the body after signing - replace a byte in the entry
    // mimeType so the body digest diverges from SignedInfo's DigestValue.
    const std::string before = "application/dicom";
    const std::string after = "application/evil_";  // same length
    REQUIRE(before.size() == after.size());
    const auto pos = root_xml.find(before);
    REQUIRE(pos != std::string::npos);
    root_xml.replace(pos, before.size(), after);

    scoped_transport_override guard(
        [&](const detail::transport_request&)
            -> kcenon::common::Result<detail::http_response> {
            detail::http_response r;
            r.status_code = 200;
            r.body = root_xml;
            return r;
        });

    registry_query rq(make_opts(), signer);
    auto r = rq.find_documents(kPatientId);
    REQUIRE(r.is_err());
    REQUIRE(r.error().code ==
            static_cast<int>(
                error_code::registry_query_signature_verification_failed));
}

TEST_CASE("registry_query get_documents resolves explicit UUIDs",
          "[ihe][xds][iti18][e2e]") {
    auto signer = make_signing();
    const std::string root_xml = build_signed_response_envelope(
        kSuccessStatus, {kDocEntryUuid1, kDocEntryUuid2}, signer);

    std::string captured_body;
    scoped_transport_override guard(
        [&](const detail::transport_request& req)
            -> kcenon::common::Result<detail::http_response> {
            captured_body = req.body;
            detail::http_response r;
            r.status_code = 200;
            r.body = root_xml;
            return r;
        });

    registry_query rq(make_opts(), signer);
    auto result = rq.get_documents({kDocEntryUuid1, kDocEntryUuid2});
    REQUIRE(result.is_ok());
    REQUIRE(result.value().entries.size() == 2);

    // Verify the emitted request uses the GetDocuments stored query id
    // and carries both UUIDs as a list literal inside a single Value
    // element (the slot shape ebRIM requires).
    REQUIRE(captured_body.find("urn:uuid:5737b14c-8a1a-4539-b659-e03a34a5e1e4") !=
            std::string::npos);
    REQUIRE(captured_body.find("$XDSDocumentEntryEntryUUID") !=
            std::string::npos);
    REQUIRE(captured_body.find(kDocEntryUuid1) != std::string::npos);
    REQUIRE(captured_body.find(kDocEntryUuid2) != std::string::npos);
}

TEST_CASE("registry_query get_documents rejects empty uuid list",
          "[ihe][xds][iti18][e2e]") {
    scoped_transport_override guard(
        [](const detail::transport_request&)
            -> kcenon::common::Result<detail::http_response> {
            FAIL("transport should not be invoked when uuid list is empty");
            return detail::http_response{};
        });

    registry_query rq(make_opts(), make_signing());
    auto r = rq.get_documents({});
    REQUIRE(r.is_err());
    REQUIRE(r.error().code ==
            static_cast<int>(error_code::registry_query_empty_uuid_list));
}

TEST_CASE(
    "registry_query find_documents emits creation-time bounds when set",
    "[ihe][xds][iti18][e2e]") {
    auto signer = make_signing();
    const std::string root_xml =
        build_signed_response_envelope(kSuccessStatus, {}, signer);

    std::string captured_body;
    scoped_transport_override guard(
        [&](const detail::transport_request& req)
            -> kcenon::common::Result<detail::http_response> {
            captured_body = req.body;
            detail::http_response r;
            r.status_code = 200;
            r.body = root_xml;
            return r;
        });

    registry_query_options opts;
    opts.creation_time_from = "20260101000000";
    opts.creation_time_to = "20260401000000";

    registry_query rq(make_opts(), signer);
    auto result = rq.find_documents(kPatientId, opts);
    REQUIRE(result.is_ok());
    REQUIRE(captured_body.find("$XDSDocumentEntryCreationTimeFrom") !=
            std::string::npos);
    REQUIRE(captured_body.find("20260101000000") != std::string::npos);
    REQUIRE(captured_body.find("$XDSDocumentEntryCreationTimeTo") !=
            std::string::npos);
    REQUIRE(captured_body.find("20260401000000") != std::string::npos);
}

TEST_CASE("registry_query find_documents preserves entries on PartialSuccess",
          "[ihe][xds][iti18][e2e]") {
    // IHE ITI-18 PartialSuccess carries a valid RegistryObjectList
    // alongside RegistryError entries - dropping the entries by mapping
    // PartialSuccess to an error would contradict registry_query.h's
    // contract. Verify the parser returns Ok, preserves the status
    // string in the result, and yields the ExtrinsicObject entries
    // that a conformant registry would include (review M2).
    auto signer = make_signing();
    const std::string partial_status =
        "urn:oasis:names:tc:ebxml-regrep:ResponseStatusType:PartialSuccess";
    const std::string root_xml = build_signed_response_envelope(
        partial_status, {kDocEntryUuid1, kDocEntryUuid2}, signer);

    scoped_transport_override guard(
        [&](const detail::transport_request&)
            -> kcenon::common::Result<detail::http_response> {
            detail::http_response r;
            r.status_code = 200;
            r.body = root_xml;
            return r;
        });

    registry_query rq(make_opts(), signer);
    auto result = rq.find_documents(kPatientId);
    REQUIRE(result.is_ok());
    REQUIRE(result.value().registry_response_status == partial_status);
    REQUIRE(result.value().entries.size() == 2);
    REQUIRE(result.value().entries[0].entry_uuid == kDocEntryUuid1);
}

TEST_CASE(
    "registry_query find_documents rejects patient_id with slot-literal "
    "injection characters",
    "[ihe][xds][iti18][e2e]") {
    // A patient_id that contains a single quote would otherwise break out
    // of the ('...') list literal in the XDSDocumentEntryPatientId slot
    // value, letting a caller forge additional slot values on the wire.
    // The envelope builder must reject such inputs before serialization -
    // the transport should never be invoked.
    scoped_transport_override guard(
        [](const detail::transport_request&)
            -> kcenon::common::Result<detail::http_response> {
            FAIL("transport must not be invoked for an unsafe patient_id");
            return detail::http_response{};
        });

    registry_query rq(make_opts(), make_signing());

    for (const char* bad : {"id'; DROP--", "id(^^^)&x&ISO", "id,extra",
                            "id\nwith\nnewline"}) {
        auto r = rq.find_documents(bad);
        REQUIRE(r.is_err());
        REQUIRE(r.error().code ==
                static_cast<int>(
                    error_code::registry_query_invalid_patient_id));
    }
}

TEST_CASE("registry_query get_documents rejects malformed UUID inputs",
          "[ihe][xds][iti18][e2e]") {
    scoped_transport_override guard(
        [](const detail::transport_request&)
            -> kcenon::common::Result<detail::http_response> {
            FAIL("transport must not be invoked for unsafe UUID inputs");
            return detail::http_response{};
        });

    registry_query rq(make_opts(), make_signing());

    // Empty string inside an otherwise valid UUID list.
    {
        auto r = rq.get_documents({kDocEntryUuid1, ""});
        REQUIRE(r.is_err());
        REQUIRE(r.error().code ==
                static_cast<int>(
                    error_code::registry_query_missing_document_uuid));
    }

    // UUID containing a single quote would break the list literal.
    {
        auto r = rq.get_documents({"urn:uuid:bad'injection"});
        REQUIRE(r.is_err());
        REQUIRE(r.error().code ==
                static_cast<int>(
                    error_code::registry_query_missing_document_uuid));
    }

    // UUID containing a comma would fracture the list.
    {
        auto r = rq.get_documents({"urn:uuid:a,b"});
        REQUIRE(r.is_err());
        REQUIRE(r.error().code ==
                static_cast<int>(
                    error_code::registry_query_missing_document_uuid));
    }
}

TEST_CASE("registry_query find_documents accepts conformant CX patient IDs",
          "[ihe][xds][iti18][e2e]") {
    // The CX-format assigning authority delimiter is '&' - a character
    // that is XML-significant but NOT slot-literal-significant. The
    // builder must accept it and pugixml must escape it as &amp; in the
    // emitted body so the registry's parser sees the original string.
    auto signer = make_signing();
    const std::string root_xml =
        build_signed_response_envelope(kSuccessStatus, {}, signer);

    std::string captured_body;
    scoped_transport_override guard(
        [&](const detail::transport_request& req)
            -> kcenon::common::Result<detail::http_response> {
            captured_body = req.body;
            detail::http_response r;
            r.status_code = 200;
            r.body = root_xml;
            return r;
        });

    const std::string cx_id = "ABC-123^^^&1.2.3.4.5&ISO";
    registry_query rq(make_opts(), signer);
    auto result = rq.find_documents(cx_id);
    REQUIRE(result.is_ok());
    // The raw & must have been XML-escaped by pugixml's text setter.
    // The body carries the escaped form; neither the raw "&I" nor the
    // unescaped CX substring should appear without XML escaping.
    REQUIRE(captured_body.find("&amp;1.2.3.4.5&amp;ISO") !=
            std::string::npos);
}

TEST_CASE(
    "registry_query find_documents rejects unsigned response envelope",
    "[ihe][xds][iti18][e2e]") {
    // A response envelope that carries no ds:Signature must be refused
    // at verify_envelope_integrity. The consumer_signature_missing code
    // the shared verifier emits is remapped by map_signature_error to
    // registry_query_signature_missing before surfacing to the caller
    // (review m3). This exercises the second branch of that mapper
    // which no other test touches.
    // Envelope carries a wsse:Security header with Timestamp and BST so
    // verify_envelope_integrity locates the security context, but omits
    // ds:Signature entirely - the missing-signature branch rather than
    // the digest-mismatch branch.
    constexpr const char* kUnsignedEnvelope =
        R"(<?xml version="1.0" encoding="UTF-8"?>)"
        R"(<soap:Envelope )"
        R"(xmlns:soap="http://www.w3.org/2003/05/soap-envelope" )"
        R"(xmlns:wsse="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd" )"
        R"(xmlns:wsu="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd" )"
        R"(xmlns:query="urn:oasis:names:tc:ebxml-regrep:xsd:query:3.0" )"
        R"(xmlns:rim="urn:oasis:names:tc:ebxml-regrep:xsd:rim:3.0">)"
        R"(<soap:Header>)"
        R"(<wsse:Security>)"
        R"(<wsu:Timestamp wsu:Id="ts-unsigned">)"
        R"(<wsu:Created>2026-04-22T12:00:00Z</wsu:Created>)"
        R"(</wsu:Timestamp>)"
        R"(<wsse:BinarySecurityToken wsu:Id="bst-unsigned" )"
        R"(EncodingType="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-security-1.0#Base64Binary" )"
        R"(ValueType="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-x509-token-profile-1.0#X509v3"/>)"
        R"(</wsse:Security>)"
        R"(</soap:Header>)"
        R"(<soap:Body wsu:Id="body-unsigned">)"
        R"(<query:AdhocQueryResponse )"
        R"(status="urn:oasis:names:tc:ebxml-regrep:ResponseStatusType:Success">)"
        R"(<rim:RegistryObjectList/>)"
        R"(</query:AdhocQueryResponse>)"
        R"(</soap:Body>)"
        R"(</soap:Envelope>)";

    scoped_transport_override guard(
        [&](const detail::transport_request&)
            -> kcenon::common::Result<detail::http_response> {
            detail::http_response r;
            r.status_code = 200;
            r.body = kUnsignedEnvelope;
            return r;
        });

    registry_query rq(make_opts(), make_signing());
    auto r = rq.find_documents(kPatientId);
    REQUIRE(r.is_err());
    REQUIRE(r.error().code ==
            static_cast<int>(
                error_code::registry_query_signature_missing));
}


