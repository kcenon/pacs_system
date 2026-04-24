// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file xds_atna_integration_test.cpp
 * @brief Gazelle-lite integration harness for IHE XDS.b ATNA audit
 *        emission (Issue #1131).
 *
 * Exercises each of the three XDS.b actors end-to-end through the
 * detail::set_http_transport_override hook, wiring a
 * recording_xds_audit_sink to the actor and asserting that:
 *
 *   1. Every success AND failure path emits exactly one audit event.
 *   2. The event carries the expected outcome code
 *      (atna_event_outcome::success or serious_failure).
 *   3. The event captures transaction context - patient id, document
 *      uids, endpoint - so a downstream ATNA aggregator can reconstruct
 *      the transaction without re-parsing the wire.
 *
 * Scope: in-process. The transport override returns canned SOAP
 * envelopes the real parsers accept, so no docker-compose registry is
 * needed. This is the cheapest CI shape that still exercises the full
 * Result<T> -> audit-sink plumbing, and matches the "gazelle-lite"
 * framing from the issue body.
 *
 * CTest label: pacs_xds_integration
 */

#include "../../../../src/ihe/xds/common/http_client.h"
#include "../../../../src/ihe/xds/common/soap_envelope.h"
#include "../../../../src/ihe/xds/common/wss_signer.h"

#include <kcenon/pacs/ihe/xds/document_consumer.h>
#include <kcenon/pacs/ihe/xds/document_source.h>
#include <kcenon/pacs/ihe/xds/errors.h>
#include <kcenon/pacs/ihe/xds/http_options.h>
#include <kcenon/pacs/ihe/xds/registry_query.h>
#include <kcenon/pacs/ihe/xds/submission_set.h>
#include <kcenon/pacs/security/xds_audit_events.h>

#include <catch2/catch_test_macros.hpp>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace kcenon::pacs::ihe::xds;
using kcenon::pacs::security::atna_event_outcome;
using kcenon::pacs::security::recording_xds_audit_sink;
using kcenon::pacs::security::xds_iti18_event;

namespace {

// -- Crypto helpers ----------------------------------------------------------

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

signing_material make_signing(const char* subject_cn) {
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
        reinterpret_cast<const unsigned char*>(subject_cn), -1, -1, 0);
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

// -- Transport override scope guard -----------------------------------------

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

// -- Shared fixture data -----------------------------------------------------

constexpr const char* kPatientId =
    "P12345^^^&1.2.276.0.7230010.3.1.2.999&ISO";
constexpr const char* kDocUid = "1.2.840.10008.5.1.4.1.1.2.99999";
constexpr const char* kRepoUid = "1.2.276.0.7230010.3.0.3.6.2";
constexpr const char* kEntryUuid =
    "urn:uuid:aabbccdd-1111-2222-3333-444455556666";

constexpr const char* kIti41Endpoint = "https://registry.example.test/iti41";
constexpr const char* kIti43Endpoint = "https://repo.example.test/iti43";
constexpr const char* kIti18Endpoint = "https://registry.example.test/iti18";

constexpr const char* kSuccessStatus =
    "urn:oasis:names:tc:ebxml-regrep:ResponseStatusType:Success";
constexpr const char* kFailureStatus =
    "urn:oasis:names:tc:ebxml-regrep:ResponseStatusType:Failure";

submission_set make_submission() {
    submission_set ss;
    ss.submission_set_unique_id = "1.2.3.4.5";
    ss.entry_uuid = "urn:uuid:00000000-0000-0000-0000-000000000001";
    ss.source_id = "1.2.276.0.7230010.3.0.3.6.1";
    ss.patient_id = kPatientId;
    ss.submission_time = "20260422120000";

    xds_document d;
    d.entry_uuid = "urn:uuid:00000000-0000-0000-0000-000000000002";
    d.unique_id = kDocUid;
    d.mime_type = "application/dicom";
    d.content = std::vector<std::uint8_t>{0x44, 0x49, 0x43, 0x4D};
    ss.documents.push_back(std::move(d));
    return ss;
}

// -- ITI-41 stock responses --------------------------------------------------

std::string iti41_registry_response(bool success) {
    return std::string(
               R"(<?xml version="1.0" encoding="UTF-8"?>)"
               R"(<soap:Envelope xmlns:soap="http://www.w3.org/2003/05/soap-envelope">)"
               R"(<soap:Body>)"
               R"(<rs:RegistryResponse xmlns:rs="urn:oasis:names:tc:ebxml-regrep:xsd:rs:3.0" status=")") +
           (success ? kSuccessStatus : kFailureStatus) +
           R"("/></soap:Body></soap:Envelope>)";
}

// -- ITI-43 signed response + MTOM wrapper ----------------------------------

std::string build_signed_iti43_response(const std::string& cid,
                                        const signing_material& signer) {
    std::string xml =
        R"(<?xml version="1.0" encoding="UTF-8"?>)"
        R"(<soap:Envelope xmlns:soap="http://www.w3.org/2003/05/soap-envelope" )"
        R"(xmlns:xdsb="urn:ihe:iti:xds-b:2007" )"
        R"(xmlns:rs="urn:oasis:names:tc:ebxml-regrep:xsd:rs:3.0" )"
        R"(xmlns:xop="http://www.w3.org/2004/08/xop/include" )"
        R"(xmlns:wsa="http://www.w3.org/2005/08/addressing" )"
        R"(xmlns:wsse="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd" )"
        R"(xmlns:wsu="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd">)"
        R"(<soap:Header>)"
        R"(<wsa:Action>urn:ihe:iti:2007:RetrieveDocumentSetResponse</wsa:Action>)"
        R"(<wsse:Security soap:mustUnderstand="true">)"
        R"(<wsu:Timestamp wsu:Id="ts-1">)"
        R"(<wsu:Created>2026-04-22T12:00:00Z</wsu:Created>)"
        R"(</wsu:Timestamp>)"
        R"(<wsse:BinarySecurityToken wsu:Id="bst-1" )"
        R"(EncodingType="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-security-1.0#Base64Binary" )"
        R"(ValueType="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-x509-token-profile-1.0#X509v3"/>)"
        R"(</wsse:Security>)"
        R"(</soap:Header>)"
        R"(<soap:Body wsu:Id="body-1">)"
        R"(<xdsb:RetrieveDocumentSetResponse>)"
        R"(<rs:RegistryResponse status=")" +
        std::string(kSuccessStatus) +
        R"("/>)"
        R"(<xdsb:DocumentResponse>)"
        R"(<xdsb:RepositoryUniqueId>)" +
        std::string(kRepoUid) +
        R"(</xdsb:RepositoryUniqueId>)"
        R"(<xdsb:DocumentUniqueId>)" +
        std::string(kDocUid) +
        R"(</xdsb:DocumentUniqueId>)"
        R"(<xdsb:mimeType>application/dicom</xdsb:mimeType>)"
        R"(<xdsb:Document>)"
        R"(<xop:Include href="cid:)" +
        cid +
        R"("/>)"
        R"(</xdsb:Document>)"
        R"(</xdsb:DocumentResponse>)"
        R"(</xdsb:RetrieveDocumentSetResponse>)"
        R"(</soap:Body>)"
        R"(</soap:Envelope>)";

    detail::built_envelope env;
    env.body_id = "body-1";
    env.timestamp_id = "ts-1";
    env.binary_security_token_id = "bst-1";
    env.xml = std::move(xml);

    auto r = detail::sign_envelope(env, signer.certificate_pem,
                                   signer.private_key_pem, "");
    REQUIRE(r.is_ok());
    return env.xml;
}

std::string pack_iti43_mtom(const std::string& root_xml,
                            const std::string& cid,
                            const std::string& bytes,
                            std::string& out_content_type) {
    constexpr const char* kBoundary = "----=_Part_atna_integration_boundary";
    out_content_type = std::string(
                           "multipart/related; boundary=\"") +
                       kBoundary +
                       "\"; type=\"application/xop+xml\"; "
                       "start=\"<root.message@kcenon.test>\"";
    std::string out;
    out.reserve(root_xml.size() + bytes.size() + 512);
    out += "--";
    out += kBoundary;
    out += "\r\nContent-Type: application/xop+xml; charset=UTF-8; "
           "type=\"application/soap+xml\"\r\n";
    out += "Content-Transfer-Encoding: 8bit\r\n";
    out += "Content-ID: <root.message@kcenon.test>\r\n\r\n";
    out += root_xml;
    out += "\r\n--";
    out += kBoundary;
    out += "\r\nContent-Type: application/dicom\r\n";
    out += "Content-Transfer-Encoding: binary\r\n";
    out += "Content-ID: <";
    out += cid;
    out += ">\r\n\r\n";
    out += bytes;
    out += "\r\n--";
    out += kBoundary;
    out += "--\r\n";
    return out;
}

// -- ITI-18 signed response --------------------------------------------------

std::string build_signed_iti18_response(const std::string& status_urn,
                                        const signing_material& signer) {
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
        R"(<wsu:Timestamp wsu:Id="ts-q1">)"
        R"(<wsu:Created>2026-04-22T12:00:00Z</wsu:Created>)"
        R"(</wsu:Timestamp>)"
        R"(<wsse:BinarySecurityToken wsu:Id="bst-q1" )"
        R"(EncodingType="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-security-1.0#Base64Binary" )"
        R"(ValueType="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-x509-token-profile-1.0#X509v3"/>)"
        R"(</wsse:Security>)"
        R"(</soap:Header>)"
        R"(<soap:Body wsu:Id="body-q1">)"
        R"(<query:AdhocQueryResponse status=")" +
        status_urn +
        R"(">)"
        R"(<rim:RegistryObjectList/>)"
        R"(</query:AdhocQueryResponse>)"
        R"(</soap:Body>)"
        R"(</soap:Envelope>)";

    detail::built_envelope env;
    env.body_id = "body-q1";
    env.timestamp_id = "ts-q1";
    env.binary_security_token_id = "bst-q1";
    env.xml = std::move(xml);

    auto r = detail::sign_envelope(env, signer.certificate_pem,
                                   signer.private_key_pem, "");
    REQUIRE(r.is_ok());
    return env.xml;
}

}  // namespace

// =============================================================================
// ITI-41 Document Source
// =============================================================================

TEST_CASE("ITI-41 emits a success audit event on successful submit",
          "[ihe][xds][atna][integration][iti41]") {
    auto signer = make_signing("atna-iti41-success");
    auto sink = std::make_shared<recording_xds_audit_sink>();

    scoped_transport_override guard(
        [](const detail::transport_request&)
            -> kcenon::common::Result<detail::http_response> {
            detail::http_response r;
            r.status_code = 200;
            r.body = iti41_registry_response(true);
            return r;
        });

    http_options opts;
    opts.endpoint = kIti41Endpoint;
    document_source ds(opts, signer);
    ds.set_audit_sink(sink);

    auto result = ds.submit(make_submission());
    REQUIRE(result.is_ok());

    REQUIRE(sink->events().size() == 1);
    const auto& ev = sink->events()[0];
    REQUIRE(ev.k == recording_xds_audit_sink::captured_event::kind::iti41);
    CHECK(ev.iti41.outcome == atna_event_outcome::success);
    CHECK(ev.iti41.patient_id == kPatientId);
    CHECK(ev.iti41.destination_endpoint == kIti41Endpoint);
    CHECK(ev.iti41.submission_set_unique_id == "1.2.3.4.5");
    REQUIRE(ev.iti41.document_unique_ids.size() == 1);
    CHECK(ev.iti41.document_unique_ids[0] == kDocUid);
    CHECK(ev.iti41.failure_description.empty());
}

TEST_CASE("ITI-41 emits a failure audit event when the registry rejects",
          "[ihe][xds][atna][integration][iti41]") {
    auto signer = make_signing("atna-iti41-failure");
    auto sink = std::make_shared<recording_xds_audit_sink>();

    scoped_transport_override guard(
        [](const detail::transport_request&)
            -> kcenon::common::Result<detail::http_response> {
            detail::http_response r;
            r.status_code = 200;
            r.body = iti41_registry_response(false);
            return r;
        });

    http_options opts;
    opts.endpoint = kIti41Endpoint;
    document_source ds(opts, signer);
    ds.set_audit_sink(sink);

    auto result = ds.submit(make_submission());
    REQUIRE(result.is_err());
    CHECK(result.error().code ==
          static_cast<int>(error_code::registry_failure_response));

    REQUIRE(sink->events().size() == 1);
    const auto& ev = sink->events()[0];
    REQUIRE(ev.k == recording_xds_audit_sink::captured_event::kind::iti41);
    CHECK(ev.iti41.outcome == atna_event_outcome::serious_failure);
    CHECK(ev.iti41.patient_id == kPatientId);
    CHECK(!ev.iti41.failure_description.empty());
}

TEST_CASE("ITI-41 emits a failure audit event when metadata validation fails",
          "[ihe][xds][atna][integration][iti41]") {
    auto signer = make_signing("atna-iti41-metadata");
    auto sink = std::make_shared<recording_xds_audit_sink>();

    scoped_transport_override guard([](const detail::transport_request&)
                                        -> kcenon::common::Result<
                                            detail::http_response> {
        FAIL("transport should not be invoked for metadata-level rejection");
        return detail::http_response{};
    });

    http_options opts;
    opts.endpoint = kIti41Endpoint;
    document_source ds(opts, signer);
    ds.set_audit_sink(sink);

    auto bad = make_submission();
    bad.patient_id.clear();
    auto result = ds.submit(bad);
    REQUIRE(result.is_err());

    REQUIRE(sink->events().size() == 1);
    CHECK(sink->events()[0].iti41.outcome ==
          atna_event_outcome::serious_failure);
}

// =============================================================================
// ITI-43 Document Consumer
// =============================================================================

TEST_CASE("ITI-43 emits a success audit event on successful retrieve",
          "[ihe][xds][atna][integration][iti43]") {
    auto signer = make_signing("atna-iti43-success");
    auto sink = std::make_shared<recording_xds_audit_sink>();

    const std::string cid = "doc-atna-payload@kcenon.test";
    const std::string payload = "DICM\x00\x01\x02\x03integration-body";
    const std::string root = build_signed_iti43_response(cid, signer);
    std::string content_type;
    const std::string mtom = pack_iti43_mtom(root, cid, payload, content_type);

    scoped_transport_override guard(
        [&](const detail::transport_request&)
            -> kcenon::common::Result<detail::http_response> {
            detail::http_response r;
            r.status_code = 200;
            r.content_type = content_type;
            r.body = mtom;
            return r;
        });

    http_options opts;
    opts.endpoint = kIti43Endpoint;
    document_consumer dc(opts, signer);
    dc.set_audit_sink(sink);

    auto result = dc.retrieve(kDocUid, kRepoUid);
    REQUIRE(result.is_ok());

    REQUIRE(sink->events().size() == 1);
    const auto& ev = sink->events()[0];
    REQUIRE(ev.k == recording_xds_audit_sink::captured_event::kind::iti43);
    CHECK(ev.iti43.outcome == atna_event_outcome::success);
    CHECK(ev.iti43.document_unique_id == kDocUid);
    CHECK(ev.iti43.repository_unique_id == kRepoUid);
    CHECK(ev.iti43.source_endpoint == kIti43Endpoint);
    CHECK(ev.iti43.failure_description.empty());
}

TEST_CASE("ITI-43 emits a failure audit event on transport TLS error",
          "[ihe][xds][atna][integration][iti43]") {
    auto signer = make_signing("atna-iti43-tls");
    auto sink = std::make_shared<recording_xds_audit_sink>();

    scoped_transport_override guard(
        [](const detail::transport_request&)
            -> kcenon::common::Result<detail::http_response> {
            return kcenon::common::make_error<detail::http_response>(
                static_cast<int>(error_code::transport_tls_error),
                "integration harness: simulated TLS handshake failure",
                std::string(error_source));
        });

    http_options opts;
    opts.endpoint = kIti43Endpoint;
    document_consumer dc(opts, signer);
    dc.set_audit_sink(sink);

    auto result = dc.retrieve(kDocUid, kRepoUid);
    REQUIRE(result.is_err());
    CHECK(result.error().code ==
          static_cast<int>(error_code::transport_tls_error));

    REQUIRE(sink->events().size() == 1);
    const auto& ev = sink->events()[0];
    REQUIRE(ev.k == recording_xds_audit_sink::captured_event::kind::iti43);
    CHECK(ev.iti43.outcome == atna_event_outcome::serious_failure);
    CHECK(ev.iti43.document_unique_id == kDocUid);
    CHECK(ev.iti43.repository_unique_id == kRepoUid);
    CHECK(!ev.iti43.failure_description.empty());
}

TEST_CASE("ITI-43 emits a failure audit event on empty input validation",
          "[ihe][xds][atna][integration][iti43]") {
    auto signer = make_signing("atna-iti43-input");
    auto sink = std::make_shared<recording_xds_audit_sink>();

    scoped_transport_override guard([](const detail::transport_request&)
                                        -> kcenon::common::Result<
                                            detail::http_response> {
        FAIL("transport should not be invoked for empty document uid");
        return detail::http_response{};
    });

    http_options opts;
    opts.endpoint = kIti43Endpoint;
    document_consumer dc(opts, signer);
    dc.set_audit_sink(sink);

    auto result = dc.retrieve("", kRepoUid);
    REQUIRE(result.is_err());

    REQUIRE(sink->events().size() == 1);
    CHECK(sink->events()[0].iti43.outcome ==
          atna_event_outcome::serious_failure);
}

// =============================================================================
// ITI-18 Registry Query
// =============================================================================

TEST_CASE("ITI-18 FindDocuments emits a success audit event",
          "[ihe][xds][atna][integration][iti18]") {
    auto signer = make_signing("atna-iti18-find-success");
    auto sink = std::make_shared<recording_xds_audit_sink>();

    const std::string body = build_signed_iti18_response(kSuccessStatus, signer);
    scoped_transport_override guard(
        [&](const detail::transport_request&)
            -> kcenon::common::Result<detail::http_response> {
            detail::http_response r;
            r.status_code = 200;
            r.body = body;
            return r;
        });

    http_options opts;
    opts.endpoint = kIti18Endpoint;
    registry_query rq(opts, signer);
    rq.set_audit_sink(sink);

    auto result = rq.find_documents(kPatientId);
    REQUIRE(result.is_ok());

    REQUIRE(sink->events().size() == 1);
    const auto& ev = sink->events()[0];
    REQUIRE(ev.k == recording_xds_audit_sink::captured_event::kind::iti18);
    CHECK(ev.iti18.outcome == atna_event_outcome::success);
    CHECK(ev.iti18.query_kind == xds_iti18_event::kind::find_documents);
    CHECK(ev.iti18.patient_id == kPatientId);
    CHECK(ev.iti18.registry_endpoint == kIti18Endpoint);
}

TEST_CASE("ITI-18 GetDocuments emits a failure audit event when response "
          "reports Failure",
          "[ihe][xds][atna][integration][iti18]") {
    auto signer = make_signing("atna-iti18-get-failure");
    auto sink = std::make_shared<recording_xds_audit_sink>();

    const std::string body = build_signed_iti18_response(kFailureStatus, signer);
    scoped_transport_override guard(
        [&](const detail::transport_request&)
            -> kcenon::common::Result<detail::http_response> {
            detail::http_response r;
            r.status_code = 200;
            r.body = body;
            return r;
        });

    http_options opts;
    opts.endpoint = kIti18Endpoint;
    registry_query rq(opts, signer);
    rq.set_audit_sink(sink);

    auto result = rq.get_documents({kEntryUuid});
    REQUIRE(result.is_err());

    REQUIRE(sink->events().size() == 1);
    const auto& ev = sink->events()[0];
    REQUIRE(ev.k == recording_xds_audit_sink::captured_event::kind::iti18);
    CHECK(ev.iti18.outcome == atna_event_outcome::serious_failure);
    CHECK(ev.iti18.query_kind == xds_iti18_event::kind::get_documents);
    REQUIRE(ev.iti18.document_uuids.size() == 1);
    CHECK(ev.iti18.document_uuids[0] == kEntryUuid);
    CHECK(!ev.iti18.failure_description.empty());
}

TEST_CASE("ITI-18 emits a failure audit event for slot-literal injection",
          "[ihe][xds][atna][integration][iti18]") {
    auto signer = make_signing("atna-iti18-injection");
    auto sink = std::make_shared<recording_xds_audit_sink>();

    scoped_transport_override guard([](const detail::transport_request&)
                                        -> kcenon::common::Result<
                                            detail::http_response> {
        FAIL("transport should not be invoked for malformed patient_id");
        return detail::http_response{};
    });

    http_options opts;
    opts.endpoint = kIti18Endpoint;
    registry_query rq(opts, signer);
    rq.set_audit_sink(sink);

    auto result = rq.find_documents("bad'injection");
    REQUIRE(result.is_err());

    REQUIRE(sink->events().size() == 1);
    CHECK(sink->events()[0].iti18.outcome ==
          atna_event_outcome::serious_failure);
}
