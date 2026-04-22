// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file document_source_test.cpp
 * @brief End-to-end tests for the ITI-41 Document Source actor.
 */

#include "../../../src/ihe/xds/common/http_client.h"

#include <kcenon/pacs/ihe/xds/document_source.h>
#include <kcenon/pacs/ihe/xds/errors.h>
#include <kcenon/pacs/ihe/xds/http_options.h>
#include <kcenon/pacs/ihe/xds/submission_set.h>

#include <catch2/catch_test_macros.hpp>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

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
        reinterpret_cast<const unsigned char*>("kcenon-ds-test"), -1, -1, 0);
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

submission_set make_submission() {
    submission_set ss;
    ss.submission_set_unique_id = "1.2.3.4.5";
    ss.entry_uuid = "urn:uuid:00000000-0000-0000-0000-000000000001";
    ss.source_id = "1.2.276.0.7230010.3.0.3.6.1";
    ss.patient_id = "P12345^^^&1.2.3.4&ISO";
    ss.submission_time = "20260422120000";

    xds_document d;
    d.entry_uuid = "urn:uuid:00000000-0000-0000-0000-000000000002";
    d.unique_id = "1.2.840.10008.5.1.4.1.1.2.99999";
    d.mime_type = "application/dicom";
    d.content = std::vector<std::uint8_t>{0x44, 0x49, 0x43, 0x4D};
    ss.documents.push_back(std::move(d));
    return ss;
}

http_options make_opts() {
    http_options o;
    o.endpoint = "https://registry.example.test/iti41";
    return o;
}

constexpr const char* kSuccessResponse =
    R"(<?xml version="1.0" encoding="UTF-8"?>)"
    R"(<soap:Envelope xmlns:soap="http://www.w3.org/2003/05/soap-envelope">)"
    R"(<soap:Body>)"
    R"(<rs:RegistryResponse xmlns:rs="urn:oasis:names:tc:ebxml-regrep:xsd:rs:3.0" )"
    R"(status="urn:oasis:names:tc:ebxml-regrep:ResponseStatusType:Success"/>)"
    R"(</soap:Body></soap:Envelope>)";

constexpr const char* kFailureResponse =
    R"(<?xml version="1.0" encoding="UTF-8"?>)"
    R"(<soap:Envelope xmlns:soap="http://www.w3.org/2003/05/soap-envelope">)"
    R"(<soap:Body>)"
    R"(<rs:RegistryResponse xmlns:rs="urn:oasis:names:tc:ebxml-regrep:xsd:rs:3.0" )"
    R"(status="urn:oasis:names:tc:ebxml-regrep:ResponseStatusType:Failure"/>)"
    R"(</soap:Body></soap:Envelope>)";

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

TEST_CASE("document_source happy path against a stubbed transport",
          "[ihe][xds][iti41][e2e]") {
    std::string captured_body;
    std::string captured_content_type;
    scoped_transport_override guard(
        [&](const detail::transport_request& req)
            -> kcenon::common::Result<detail::http_response> {
            captured_body = req.body;
            captured_content_type = req.content_type;
            detail::http_response r;
            r.status_code = 200;
            r.body = kSuccessResponse;
            return r;
        });

    document_source ds(make_opts(), make_signing());
    auto result = ds.submit(make_submission());
    REQUIRE(result.is_ok());
    REQUIRE(result.value().registry_response_status ==
            "urn:oasis:names:tc:ebxml-regrep:ResponseStatusType:Success");

    REQUIRE(captured_content_type.find("multipart/related") == 0);
    REQUIRE(captured_body.find("lcm:SubmitObjectsRequest") != std::string::npos);
    REQUIRE(captured_body.find("ds:Signature") != std::string::npos);
    REQUIRE(captured_body.find("xop:Include") != std::string::npos);
}

TEST_CASE("document_source maps malformed metadata to typed error",
          "[ihe][xds][iti41][e2e]") {
    scoped_transport_override guard([](const detail::transport_request&)
                                        -> kcenon::common::Result<
                                            detail::http_response> {
        FAIL("transport should not be invoked when metadata is invalid");
        return detail::http_response{};
    });

    auto bad = make_submission();
    bad.patient_id.clear();

    document_source ds(make_opts(), make_signing());
    auto r = ds.submit(bad);
    REQUIRE(r.is_err());
    REQUIRE(r.error().code ==
            static_cast<int>(error_code::metadata_missing_patient_id));
}

TEST_CASE("document_source maps registry Failure to registry_failure_response",
          "[ihe][xds][iti41][e2e]") {
    scoped_transport_override guard(
        [](const detail::transport_request&)
            -> kcenon::common::Result<detail::http_response> {
            detail::http_response r;
            r.status_code = 200;
            r.body = kFailureResponse;
            return r;
        });

    document_source ds(make_opts(), make_signing());
    auto r = ds.submit(make_submission());
    REQUIRE(r.is_err());
    REQUIRE(r.error().code ==
            static_cast<int>(error_code::registry_failure_response));
}

TEST_CASE("document_source surfaces transport TLS errors",
          "[ihe][xds][iti41][e2e]") {
    scoped_transport_override guard(
        [](const detail::transport_request&)
            -> kcenon::common::Result<detail::http_response> {
            return kcenon::common::make_error<detail::http_response>(
                static_cast<int>(error_code::transport_tls_error),
                "simulated TLS handshake failure",
                std::string(error_source));
        });

    document_source ds(make_opts(), make_signing());
    auto r = ds.submit(make_submission());
    REQUIRE(r.is_err());
    REQUIRE(r.error().code ==
            static_cast<int>(error_code::transport_tls_error));
}

TEST_CASE("document_source surfaces HTTP errors",
          "[ihe][xds][iti41][e2e]") {
    scoped_transport_override guard(
        [](const detail::transport_request&)
            -> kcenon::common::Result<detail::http_response> {
            return kcenon::common::make_error<detail::http_response>(
                static_cast<int>(error_code::transport_http_error),
                "500 Internal Server Error", std::string(error_source));
        });

    document_source ds(make_opts(), make_signing());
    auto r = ds.submit(make_submission());
    REQUIRE(r.is_err());
    REQUIRE(r.error().code ==
            static_cast<int>(error_code::transport_http_error));
}
