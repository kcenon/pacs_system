// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file registry_query_test.cpp
 * @brief End-to-end tests for the ITI-18 Registry Query actor.
 *
 * Covers the four scenarios mandated by Issue #1130:
 *  - happy path (Registry returns two ExtrinsicObject entries)
 *  - no-results (Success status, empty RegistryObjectList)
 *  - malformed input (empty patient_id surfaces without a network call)
 *  - TLS failure (transport_override simulates transport_tls_error)
 *
 * Tests that reach into internal headers use the same pattern as
 * document_consumer_test.cpp; the HTTP transport is replaced via
 * set_http_transport_override so no live server is required.
 */

#include "../../../src/ihe/xds/common/http_client.h"
#include "../../../src/ihe/xds/common/soap_envelope.h"
#include "../../../src/ihe/xds/common/wss_signer.h"
#include "../../../src/ihe/xds/registry_query/query_envelope.h"
#include "../../../src/ihe/xds/registry_query/query_response_parser.h"

#include <kcenon/pacs/ihe/xds/document_source.h>
#include <kcenon/pacs/ihe/xds/errors.h>
#include <kcenon/pacs/ihe/xds/http_options.h>
#include <kcenon/pacs/ihe/xds/registry_query.h>

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

// Self-signed cert/key pair generated in-process. Registry Query doesn't
// verify the response signature (ITI-18 returns plain SOAP without a
// signed envelope), but registry_query_impl still signs its REQUEST via
// sign_envelope, so we need a valid credential to reach the transport.
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

// Success response body the Registry would return for a FindDocuments
// query with two matching ExtrinsicObject entries. Kept as a raw string
// rather than built via pugixml so the test fixture is self-contained
// and any regression in the parser is visible in the string itself.
constexpr const char* kTwoEntryResponse =
    R"(<?xml version="1.0" encoding="UTF-8"?>)"
    R"(<soap:Envelope xmlns:soap="http://www.w3.org/2003/05/soap-envelope" )"
    R"(xmlns:query="urn:oasis:names:tc:ebxml-regrep:xsd:query:3.0" )"
    R"(xmlns:rim="urn:oasis:names:tc:ebxml-regrep:xsd:rim:3.0">)"
    R"(<soap:Body>)"
    R"(<query:AdhocQueryResponse )"
    R"(status="urn:oasis:names:tc:ebxml-regrep:ResponseStatusType:Success">)"
    R"(<rim:RegistryObjectList>)"
    R"(<rim:ExtrinsicObject id="urn:uuid:aaaa-1111" )"
    R"(mimeType="application/dicom" status="urn:oasis:names:tc:ebxml-regrep:StatusType:Approved">)"
    R"(<rim:Slot name="creationTime"><rim:ValueList>)"
    R"(<rim:Value>20260422120000</rim:Value></rim:ValueList></rim:Slot>)"
    R"(<rim:Slot name="repositoryUniqueId"><rim:ValueList>)"
    R"(<rim:Value>1.2.276.0.7230010.3.0.3.6.2</rim:Value></rim:ValueList></rim:Slot>)"
    R"(<rim:ExternalIdentifier identificationScheme="urn:uuid:2e82c1f6-a085-4c72-9da3-8640a32e42ab" )"
    R"(value="1.2.840.10008.5.1.4.1.1.2.99991"/>)"
    R"(<rim:ExternalIdentifier identificationScheme="urn:uuid:58a6f841-87b3-4a3e-92fd-a8ffeff98427" )"
    R"(value="12345^^^&amp;1.2.3.4.5&amp;ISO"/>)"
    R"(</rim:ExtrinsicObject>)"
    R"(<rim:ExtrinsicObject id="urn:uuid:bbbb-2222" )"
    R"(mimeType="application/pdf" status="urn:oasis:names:tc:ebxml-regrep:StatusType:Approved">)"
    R"(<rim:ExternalIdentifier identificationScheme="urn:uuid:2e82c1f6-a085-4c72-9da3-8640a32e42ab" )"
    R"(value="1.2.840.10008.5.1.4.1.1.2.99992"/>)"
    R"(</rim:ExtrinsicObject>)"
    R"(</rim:RegistryObjectList>)"
    R"(</query:AdhocQueryResponse>)"
    R"(</soap:Body>)"
    R"(</soap:Envelope>)";

// Success response with an empty RegistryObjectList. ebRS permits this
// as a legitimate zero-matches response.
constexpr const char* kEmptyResponse =
    R"(<?xml version="1.0" encoding="UTF-8"?>)"
    R"(<soap:Envelope xmlns:soap="http://www.w3.org/2003/05/soap-envelope" )"
    R"(xmlns:query="urn:oasis:names:tc:ebxml-regrep:xsd:query:3.0" )"
    R"(xmlns:rim="urn:oasis:names:tc:ebxml-regrep:xsd:rim:3.0">)"
    R"(<soap:Body>)"
    R"(<query:AdhocQueryResponse )"
    R"(status="urn:oasis:names:tc:ebxml-regrep:ResponseStatusType:Success">)"
    R"(<rim:RegistryObjectList/>)"
    R"(</query:AdhocQueryResponse>)"
    R"(</soap:Body>)"
    R"(</soap:Envelope>)";

// RAII wrapper so an override installed on the transport is cleared even
// if the test body throws.
struct scoped_transport_override {
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

}  // namespace

TEST_CASE("ITI-18 find_documents envelope embeds patient id and status",
          "[registry_query][envelope]") {
    auto env = detail::build_iti18_find_documents_envelope(
        "12345^^^&1.2.3.4.5&ISO",
        {"urn:oasis:names:tc:ebxml-regrep:StatusType:Approved"});
    REQUIRE(env.is_ok());
    const auto& xml = env.value().xml;

    // Sanity: the action URI, the stored-query id, and both slot names
    // all have to reach the wire.
    CHECK(xml.find("urn:ihe:iti:2007:RegistryStoredQuery") !=
          std::string::npos);
    CHECK(xml.find("urn:uuid:14d4debf-8f97-4251-9a1e-8aabd7b6f011") !=
          std::string::npos);
    CHECK(xml.find("$XDSDocumentEntryPatientId") != std::string::npos);
    CHECK(xml.find("$XDSDocumentEntryStatus") != std::string::npos);
    CHECK(xml.find("12345") != std::string::npos);
}

TEST_CASE("ITI-18 find_documents rejects empty patient id",
          "[registry_query][input_validation]") {
    auto env = detail::build_iti18_find_documents_envelope(
        "", {"urn:oasis:names:tc:ebxml-regrep:StatusType:Approved"});
    REQUIRE(env.is_err());
    CHECK(env.error().code() ==
          static_cast<int>(
              error_code::consumer_query_missing_patient_id));
}

TEST_CASE("ITI-18 get_documents envelope embeds uuid list",
          "[registry_query][envelope]") {
    auto env = detail::build_iti18_get_documents_envelope(
        {"urn:uuid:aaaa-1111", "urn:uuid:bbbb-2222"});
    REQUIRE(env.is_ok());
    const auto& xml = env.value().xml;

    CHECK(xml.find("urn:uuid:5c4f972b-d56b-40ac-a5fc-c8ca9b40b9d4") !=
          std::string::npos);
    CHECK(xml.find("$XDSDocumentEntryUUID") != std::string::npos);
    CHECK(xml.find("urn:uuid:aaaa-1111") != std::string::npos);
    CHECK(xml.find("urn:uuid:bbbb-2222") != std::string::npos);
}

TEST_CASE("ITI-18 get_documents rejects empty uuid list",
          "[registry_query][input_validation]") {
    auto env = detail::build_iti18_get_documents_envelope({});
    REQUIRE(env.is_err());
    CHECK(env.error().code() ==
          static_cast<int>(error_code::consumer_query_empty_uuid_list));
}

TEST_CASE("ITI-18 parser returns two entries with XDSDocumentEntry slots",
          "[registry_query][parser][happy_path]") {
    auto parsed = detail::parse_query_response(kTwoEntryResponse);
    REQUIRE(parsed.is_ok());
    const auto& result = parsed.value();
    REQUIRE(result.entries.size() == 2);

    const auto& first = result.entries[0];
    CHECK(first.entry_uuid == "urn:uuid:aaaa-1111");
    CHECK(first.mime_type == "application/dicom");
    CHECK(first.document_unique_id == "1.2.840.10008.5.1.4.1.1.2.99991");
    CHECK(first.patient_id == "12345^^^&1.2.3.4.5&ISO");
    CHECK(first.creation_time == "20260422120000");
    CHECK(first.repository_unique_id == "1.2.276.0.7230010.3.0.3.6.2");

    const auto& second = result.entries[1];
    CHECK(second.entry_uuid == "urn:uuid:bbbb-2222");
    CHECK(second.mime_type == "application/pdf");
    CHECK(second.document_unique_id == "1.2.840.10008.5.1.4.1.1.2.99992");
    // second entry intentionally lacks creationTime / repositoryUniqueId
    // / patient_id slots — those come back as empty strings rather than
    // a parse failure.
    CHECK(second.creation_time.empty());
    CHECK(second.repository_unique_id.empty());
    CHECK(second.patient_id.empty());
}

TEST_CASE("ITI-18 parser treats empty RegistryObjectList as zero-matches",
          "[registry_query][parser][no_results]") {
    auto parsed = detail::parse_query_response(kEmptyResponse);
    REQUIRE(parsed.is_ok());
    CHECK(parsed.value().entries.empty());
    CHECK(parsed.value().registry_response_status ==
          "urn:oasis:names:tc:ebxml-regrep:ResponseStatusType:Success");
}

TEST_CASE("ITI-18 parser reports Failure status as a typed error",
          "[registry_query][parser]") {
    constexpr const char* kFailure =
        R"(<?xml version="1.0" encoding="UTF-8"?>)"
        R"(<soap:Envelope xmlns:soap="http://www.w3.org/2003/05/soap-envelope" )"
        R"(xmlns:query="urn:oasis:names:tc:ebxml-regrep:xsd:query:3.0">)"
        R"(<soap:Body><query:AdhocQueryResponse )"
        R"(status="urn:oasis:names:tc:ebxml-regrep:ResponseStatusType:Failure"/>)"
        R"(</soap:Body></soap:Envelope>)";
    auto parsed = detail::parse_query_response(kFailure);
    REQUIRE(parsed.is_err());
    CHECK(parsed.error().code() ==
          static_cast<int>(
              error_code::consumer_query_response_parse_failed));
}

TEST_CASE("ITI-18 registry_query propagates TLS transport failure",
          "[registry_query][transport][tls_failure]") {
    scoped_transport_override guard([](const detail::transport_request& req)
                                        -> kcenon::common::Result<
                                            detail::http_response> {
        // Assert the request the actor built reaches the transport with
        // the ITI-18 action URI and the stored-query envelope payload.
        CHECK(req.soap_action == "urn:ihe:iti:2007:RegistryStoredQuery");
        CHECK(req.body.find(
                  "urn:uuid:14d4debf-8f97-4251-9a1e-8aabd7b6f011") !=
              std::string::npos);
        return kcenon::common::make_error<detail::http_response>(
            static_cast<int>(error_code::transport_tls_error),
            "simulated TLS handshake failure for test",
            std::string(error_source));
    });

    registry_query q(make_opts(), make_signing());
    auto result = q.find_documents("12345^^^&1.2.3.4.5&ISO");
    REQUIRE(result.is_err());
    CHECK(result.error().code() ==
          static_cast<int>(error_code::transport_tls_error));
}

TEST_CASE("ITI-18 registry_query returns matching entries end-to-end",
          "[registry_query][integration]") {
    scoped_transport_override guard([](const detail::transport_request&)
                                        -> kcenon::common::Result<
                                            detail::http_response> {
        detail::http_response r;
        r.status_code = 200;
        r.content_type = "application/soap+xml; charset=UTF-8";
        r.body = kTwoEntryResponse;
        return r;
    });

    registry_query q(make_opts(), make_signing());
    auto result = q.find_documents("12345^^^&1.2.3.4.5&ISO");
    REQUIRE(result.is_ok());
    CHECK(result.value().entries.size() == 2);
    CHECK(result.value().entries[0].document_unique_id ==
          "1.2.840.10008.5.1.4.1.1.2.99991");
}
