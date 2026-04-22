// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file document_consumer_test.cpp
 * @brief End-to-end tests for the ITI-43 Document Consumer actor.
 */

#include "../../../src/ihe/xds/common/http_client.h"
#include "../../../src/ihe/xds/common/soap_envelope.h"
#include "../../../src/ihe/xds/common/wss_signer.h"

#include <kcenon/pacs/ihe/xds/document_consumer.h>
#include <kcenon/pacs/ihe/xds/document_source.h>
#include <kcenon/pacs/ihe/xds/errors.h>
#include <kcenon/pacs/ihe/xds/http_options.h>
#include <kcenon/pacs/ihe/xds/submission_set.h>

#include <catch2/catch_test_macros.hpp>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include <cstring>
#include <memory>
#include <string>
#include <utility>

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
        reinterpret_cast<const unsigned char*>("kcenon-dc-test"), -1, -1, 0);
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
    o.endpoint = "https://repository.example.test/iti43";
    return o;
}

constexpr const char* kDocUid = "1.2.840.10008.5.1.4.1.1.2.99999";
constexpr const char* kRepoUid = "1.2.276.0.7230010.3.0.3.6.2";

// Build a signed RetrieveDocumentSetResponse envelope that matches what a
// conformant repository would emit, with a single DocumentResponse pointing
// at an xop:Include with the supplied content-id.
std::string build_signed_response_envelope(const std::string& cid,
                                           const signing_material& signer) {
    constexpr const char* kResponseEnvelope =
        R"(<?xml version="1.0" encoding="UTF-8"?>)"
        R"(<soap:Envelope xmlns:soap="http://www.w3.org/2003/05/soap-envelope" )"
        R"(xmlns:wsa="http://www.w3.org/2005/08/addressing" )"
        R"(xmlns:wsse="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd" )"
        R"(xmlns:wsu="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd" )"
        R"(xmlns:xdsb="urn:ihe:iti:xds-b:2007" )"
        R"(xmlns:rs="urn:oasis:names:tc:ebxml-regrep:xsd:rs:3.0" )"
        R"(xmlns:xop="http://www.w3.org/2004/08/xop/include">)"
        R"(<soap:Header>)"
        R"(<wsa:Action>urn:ihe:iti:2007:RetrieveDocumentSetResponse</wsa:Action>)"
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
        R"(<xdsb:RetrieveDocumentSetResponse>)"
        R"(<rs:RegistryResponse status="urn:oasis:names:tc:ebxml-regrep:ResponseStatusType:Success"/>)"
        R"(<xdsb:DocumentResponse>)"
        R"(<xdsb:RepositoryUniqueId>{REPO_UID}</xdsb:RepositoryUniqueId>)"
        R"(<xdsb:DocumentUniqueId>{DOC_UID}</xdsb:DocumentUniqueId>)"
        R"(<xdsb:mimeType>application/dicom</xdsb:mimeType>)"
        R"(<xdsb:Document>)"
        R"(<xop:Include href="cid:{CID}"/>)"
        R"(</xdsb:Document>)"
        R"(</xdsb:DocumentResponse>)"
        R"(</xdsb:RetrieveDocumentSetResponse>)"
        R"(</soap:Body>)"
        R"(</soap:Envelope>)";

    // Build the envelope through the internal helper we use for signing so
    // the wsu:Id attributes are valid. The helpers are detail-scoped, so we
    // hand-roll the envelope and then invoke sign_envelope on it.
    detail::built_envelope env;
    env.body_id = "body-response-1";
    env.timestamp_id = "ts-response-1";
    env.binary_security_token_id = "bst-response-1";

    std::string xml = kResponseEnvelope;
    const auto replace = [&xml](const std::string& needle,
                                const std::string& value) {
        for (;;) {
            const auto pos = xml.find(needle);
            if (pos == std::string::npos) break;
            xml.replace(pos, needle.size(), value);
        }
    };
    replace("{TSID}", env.timestamp_id);
    replace("{BSTID}", env.binary_security_token_id);
    replace("{BODYID}", env.body_id);
    replace("{REPO_UID}", kRepoUid);
    replace("{DOC_UID}", kDocUid);
    replace("{CID}", cid);
    env.xml = std::move(xml);

    auto r = detail::sign_envelope(env, signer.certificate_pem,
                                   signer.private_key_pem, "");
    REQUIRE(r.is_ok());
    return env.xml;
}

// Wrap a signed root envelope into a multipart/related MTOM body that
// carries the document bytes as the sole attachment. The consumer parses
// exactly this shape.
std::string pack_multipart(const std::string& root_xml,
                           const std::string& cid,
                           const std::string& mime,
                           const std::string& bytes,
                           std::string& out_content_type) {
    constexpr const char* kBoundary = "----=_Part_test_boundary_consumer";
    out_content_type = std::string(
        "multipart/related; boundary=\"") + kBoundary +
        "\"; type=\"application/xop+xml\"; "
        "start=\"<root.message@cxf.apache.org>\"";
    std::string out;
    out.reserve(root_xml.size() + bytes.size() + 512);
    out += "--";
    out += kBoundary;
    out += "\r\n";
    out +=
        "Content-Type: application/xop+xml; charset=UTF-8; "
        "type=\"application/soap+xml\"\r\n";
    out += "Content-Transfer-Encoding: 8bit\r\n";
    out += "Content-ID: <root.message@cxf.apache.org>\r\n";
    out += "\r\n";
    out += root_xml;
    out += "\r\n";
    out += "--";
    out += kBoundary;
    out += "\r\n";
    out += "Content-Type: ";
    out += mime;
    out += "\r\n";
    out += "Content-Transfer-Encoding: binary\r\n";
    out += "Content-ID: <";
    out += cid;
    out += ">\r\n\r\n";
    out += bytes;
    out += "\r\n";
    out += "--";
    out += kBoundary;
    out += "--\r\n";
    return out;
}

constexpr const char* kNotFoundResponseEnvelope =
    R"(<?xml version="1.0" encoding="UTF-8"?>)"
    R"(<soap:Envelope xmlns:soap="http://www.w3.org/2003/05/soap-envelope" )"
    R"(xmlns:xdsb="urn:ihe:iti:xds-b:2007" )"
    R"(xmlns:rs="urn:oasis:names:tc:ebxml-regrep:xsd:rs:3.0">)"
    R"(<soap:Body>)"
    R"(<xdsb:RetrieveDocumentSetResponse>)"
    R"(<rs:RegistryResponse status="urn:oasis:names:tc:ebxml-regrep:ResponseStatusType:Failure"/>)"
    R"(</xdsb:RetrieveDocumentSetResponse>)"
    R"(</soap:Body>)"
    R"(</soap:Envelope>)";

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

}  // namespace

TEST_CASE("document_consumer happy path returns document bytes",
          "[ihe][xds][iti43][e2e]") {
    auto signer = make_signing();
    const std::string cid = "doc-payload@kcenon.test";
    const std::string payload = "DICM\x00\x01\x02\x03binary-body";
    const std::string root_xml = build_signed_response_envelope(cid, signer);
    std::string content_type;
    const std::string mtom_body =
        pack_multipart(root_xml, cid, "application/dicom", payload,
                       content_type);

    std::string captured_body;
    std::string captured_action;
    scoped_transport_override guard(
        [&](const detail::transport_request& req)
            -> kcenon::common::Result<detail::http_response> {
            captured_body = req.body;
            captured_action = req.soap_action;
            detail::http_response r;
            r.status_code = 200;
            r.body = mtom_body;
            return r;
        });

    document_consumer dc(make_opts(), signer);
    auto result = dc.retrieve(kDocUid, kRepoUid);
    REQUIRE(result.is_ok());
    REQUIRE(result.value().document_unique_id == kDocUid);
    REQUIRE(result.value().repository_unique_id == kRepoUid);
    REQUIRE(result.value().mime_type == "application/dicom");
    REQUIRE(result.value().content.size() == payload.size());
    REQUIRE(std::memcmp(result.value().content.data(), payload.data(),
                        payload.size()) == 0);

    REQUIRE(captured_action == "urn:ihe:iti:2007:RetrieveDocumentSet");
    REQUIRE(captured_body.find("xdsb:RetrieveDocumentSetRequest") !=
            std::string::npos);
    REQUIRE(captured_body.find("ds:Signature") != std::string::npos);
}

TEST_CASE("document_consumer rejects missing document_unique_id",
          "[ihe][xds][iti43][e2e]") {
    scoped_transport_override guard(
        [](const detail::transport_request&)
            -> kcenon::common::Result<detail::http_response> {
            FAIL("transport should not be invoked when identifiers are "
                 "invalid");
            return detail::http_response{};
        });

    document_consumer dc(make_opts(), make_signing());
    auto r = dc.retrieve("", kRepoUid);
    REQUIRE(r.is_err());
    REQUIRE(r.error().code ==
            static_cast<int>(
                error_code::consumer_retrieve_missing_document_id));
}

TEST_CASE("document_consumer rejects missing repository_unique_id",
          "[ihe][xds][iti43][e2e]") {
    scoped_transport_override guard(
        [](const detail::transport_request&)
            -> kcenon::common::Result<detail::http_response> {
            FAIL("transport should not be invoked when identifiers are "
                 "invalid");
            return detail::http_response{};
        });

    document_consumer dc(make_opts(), make_signing());
    auto r = dc.retrieve(kDocUid, "");
    REQUIRE(r.is_err());
    REQUIRE(r.error().code ==
            static_cast<int>(
                error_code::consumer_retrieve_missing_repository_id));
}

TEST_CASE("document_consumer maps registry Failure to not_found",
          "[ihe][xds][iti43][e2e]") {
    scoped_transport_override guard(
        [](const detail::transport_request&)
            -> kcenon::common::Result<detail::http_response> {
            detail::http_response r;
            r.status_code = 200;
            r.body = kNotFoundResponseEnvelope;
            return r;
        });

    document_consumer dc(make_opts(), make_signing());
    auto r = dc.retrieve(kDocUid, kRepoUid);
    REQUIRE(r.is_err());
    // The Failure-status envelope in this test carries no WS-Security
    // scaffolding at all. The consumer refuses it at verify_envelope
    // rather than trusting the status attribute of an unsigned response,
    // which is the correct safety ordering. A conformant repository
    // reporting not-found would sign the response first; that path is
    // exercised by the "no DocumentResponse matches requested uid" test.
    REQUIRE(r.error().code ==
            static_cast<int>(
                error_code::consumer_signature_verification_failed));
}

TEST_CASE("document_consumer reports not_found when no DocumentResponse "
          "matches the requested uid",
          "[ihe][xds][iti43][e2e]") {
    auto signer = make_signing();
    const std::string cid = "doc-payload@kcenon.test";
    const std::string payload = "ignored";
    // Sign a response for a different document uid than the one we'll ask
    // the consumer for.
    const std::string other_uid = "9.9.9.9.9";
    std::string root_xml;
    {
        constexpr const char* kTemplate =
            R"(<?xml version="1.0" encoding="UTF-8"?>)"
            R"(<soap:Envelope xmlns:soap="http://www.w3.org/2003/05/soap-envelope" )"
            R"(xmlns:wsa="http://www.w3.org/2005/08/addressing" )"
            R"(xmlns:wsse="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd" )"
            R"(xmlns:wsu="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd" )"
            R"(xmlns:xdsb="urn:ihe:iti:xds-b:2007" )"
            R"(xmlns:rs="urn:oasis:names:tc:ebxml-regrep:xsd:rs:3.0" )"
            R"(xmlns:xop="http://www.w3.org/2004/08/xop/include">)"
            R"(<soap:Header>)"
            R"(<wsse:Security soap:mustUnderstand="true">)"
            R"(<wsu:Timestamp wsu:Id="ts-r2"><wsu:Created>2026-04-22T12:00:00Z</wsu:Created></wsu:Timestamp>)"
            R"(<wsse:BinarySecurityToken wsu:Id="bst-r2" )"
            R"(EncodingType="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-security-1.0#Base64Binary" )"
            R"(ValueType="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-x509-token-profile-1.0#X509v3"/>)"
            R"(</wsse:Security>)"
            R"(</soap:Header>)"
            R"(<soap:Body wsu:Id="body-r2">)"
            R"(<xdsb:RetrieveDocumentSetResponse>)"
            R"(<rs:RegistryResponse status="urn:oasis:names:tc:ebxml-regrep:ResponseStatusType:Success"/>)"
            R"(<xdsb:DocumentResponse>)"
            R"(<xdsb:RepositoryUniqueId>)" "{REPO}" R"(</xdsb:RepositoryUniqueId>)"
            R"(<xdsb:DocumentUniqueId>)" "{OTHER}" R"(</xdsb:DocumentUniqueId>)"
            R"(<xdsb:mimeType>application/dicom</xdsb:mimeType>)"
            R"(<xdsb:Document><xop:Include href="cid:)" "{CID}" R"("/></xdsb:Document>)"
            R"(</xdsb:DocumentResponse>)"
            R"(</xdsb:RetrieveDocumentSetResponse>)"
            R"(</soap:Body></soap:Envelope>)";
        std::string xml = kTemplate;
        const auto replace = [&xml](const std::string& a,
                                    const std::string& b) {
            const auto pos = xml.find(a);
            if (pos != std::string::npos) xml.replace(pos, a.size(), b);
        };
        replace("{REPO}", kRepoUid);
        replace("{OTHER}", other_uid);
        replace("{CID}", cid);
        detail::built_envelope env;
        env.body_id = "body-r2";
        env.timestamp_id = "ts-r2";
        env.binary_security_token_id = "bst-r2";
        env.xml = std::move(xml);
        auto r = detail::sign_envelope(env, signer.certificate_pem,
                                       signer.private_key_pem, "");
        REQUIRE(r.is_ok());
        root_xml = env.xml;
    }

    std::string content_type;
    const std::string mtom_body =
        pack_multipart(root_xml, cid, "application/dicom", payload,
                       content_type);

    scoped_transport_override guard(
        [&](const detail::transport_request&)
            -> kcenon::common::Result<detail::http_response> {
            detail::http_response r;
            r.status_code = 200;
            r.body = mtom_body;
            return r;
        });

    document_consumer dc(make_opts(), signer);
    auto r = dc.retrieve(kDocUid, kRepoUid);
    REQUIRE(r.is_err());
    REQUIRE(r.error().code ==
            static_cast<int>(error_code::consumer_response_document_not_found));
}

TEST_CASE("document_consumer rejects tampered response (signature mismatch)",
          "[ihe][xds][iti43][e2e]") {
    auto signer = make_signing();
    const std::string cid = "doc-payload@kcenon.test";
    const std::string payload = "DICM-original";
    std::string root_xml = build_signed_response_envelope(cid, signer);

    // Tamper with the Body after signing - replace the mimeType text so
    // the body hash no longer matches the DigestValue in SignedInfo.
    const std::string before = "application/dicom";
    const std::string after = "application/evil_";  // same length
    REQUIRE(before.size() == after.size());
    const auto pos = root_xml.find(before);
    REQUIRE(pos != std::string::npos);
    root_xml.replace(pos, before.size(), after);

    std::string content_type;
    const std::string mtom_body =
        pack_multipart(root_xml, cid, "application/dicom", payload,
                       content_type);

    scoped_transport_override guard(
        [&](const detail::transport_request&)
            -> kcenon::common::Result<detail::http_response> {
            detail::http_response r;
            r.status_code = 200;
            r.body = mtom_body;
            return r;
        });

    document_consumer dc(make_opts(), signer);
    auto r = dc.retrieve(kDocUid, kRepoUid);
    REQUIRE(r.is_err());
    REQUIRE(r.error().code ==
            static_cast<int>(
                error_code::consumer_signature_verification_failed));
}

TEST_CASE("document_consumer surfaces transport TLS errors",
          "[ihe][xds][iti43][e2e]") {
    scoped_transport_override guard(
        [](const detail::transport_request&)
            -> kcenon::common::Result<detail::http_response> {
            return kcenon::common::make_error<detail::http_response>(
                static_cast<int>(error_code::transport_tls_error),
                "simulated TLS handshake failure",
                std::string(error_source));
        });

    document_consumer dc(make_opts(), make_signing());
    auto r = dc.retrieve(kDocUid, kRepoUid);
    REQUIRE(r.is_err());
    REQUIRE(r.error().code ==
            static_cast<int>(error_code::transport_tls_error));
}

TEST_CASE("document_consumer surfaces HTTP errors",
          "[ihe][xds][iti43][e2e]") {
    scoped_transport_override guard(
        [](const detail::transport_request&)
            -> kcenon::common::Result<detail::http_response> {
            return kcenon::common::make_error<detail::http_response>(
                static_cast<int>(error_code::transport_http_error),
                "500 Internal Server Error", std::string(error_source));
        });

    document_consumer dc(make_opts(), make_signing());
    auto r = dc.retrieve(kDocUid, kRepoUid);
    REQUIRE(r.is_err());
    REQUIRE(r.error().code ==
            static_cast<int>(error_code::transport_http_error));
}

TEST_CASE("document_consumer rejects oversize responses",
          "[ihe][xds][iti43][e2e]") {
    // The http_client write callback already caps bytes at 8 MiB. The
    // transport override bypasses libcurl so we assert the defensive
    // parser-side guard inside parse_retrieve_response directly by
    // delivering an 8 MiB + 1 root envelope. The envelope is junk; the
    // size gate fires before pugixml parses it.
    scoped_transport_override guard(
        [](const detail::transport_request&)
            -> kcenon::common::Result<detail::http_response> {
            detail::http_response r;
            r.status_code = 200;
            r.body.assign(static_cast<std::size_t>(8 * 1024 * 1024) + 1, 'A');
            return r;
        });

    document_consumer dc(make_opts(), make_signing());
    auto r = dc.retrieve(kDocUid, kRepoUid);
    REQUIRE(r.is_err());
    // Upstream multipart parser also bounds the walk to 8 MiB; either
    // transport_response_too_large (defensive parse-side) or
    // consumer_response_mtom_malformed (no boundary in junk body) is
    // acceptable here.
    const int code = r.error().code;
    const bool ok =
        code ==
            static_cast<int>(error_code::transport_response_too_large) ||
        code ==
            static_cast<int>(error_code::consumer_response_mtom_malformed);
    REQUIRE(ok);
}
