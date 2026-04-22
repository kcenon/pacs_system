// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file wss_signer_test.cpp
 * @brief Unit tests for the WS-Security XML-DSig signer.
 */

#include "../../../src/ihe/xds/common/wss_signer.h"

#include "../../../src/ihe/xds/common/soap_envelope.h"

#include <kcenon/pacs/ihe/xds/submission_set.h>

#include <catch2/catch_test_macros.hpp>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

#include <pugixml.hpp>

#include <cstring>
#include <memory>
#include <string>

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

struct test_credentials {
    std::string cert_pem;
    std::string key_pem;
};

test_credentials make_rsa_credentials() {
    pkey_ptr pkey(EVP_RSA_gen(2048));
    REQUIRE(pkey);

    x509_ptr cert(X509_new());
    REQUIRE(cert);
    X509_set_version(cert.get(), 2);
    ASN1_INTEGER_set(X509_get_serialNumber(cert.get()), 1);
    X509_gmtime_adj(X509_getm_notBefore(cert.get()), 0);
    X509_gmtime_adj(X509_getm_notAfter(cert.get()), 60L * 60 * 24 * 30);
    X509_set_pubkey(cert.get(), pkey.get());

    X509_NAME* name = X509_get_subject_name(cert.get());
    X509_NAME_add_entry_by_txt(
        name, "CN", MBSTRING_ASC,
        reinterpret_cast<const unsigned char*>("kcenon-xds-test"), -1, -1, 0);
    X509_set_issuer_name(cert.get(), name);
    X509_sign(cert.get(), pkey.get(), EVP_sha256());

    test_credentials out;

    bio_ptr cert_bio(BIO_new(BIO_s_mem()));
    PEM_write_bio_X509(cert_bio.get(), cert.get());
    char* data = nullptr;
    long len = BIO_get_mem_data(cert_bio.get(), &data);
    out.cert_pem.assign(data, static_cast<std::size_t>(len));

    bio_ptr key_bio(BIO_new(BIO_s_mem()));
    PEM_write_bio_PrivateKey(key_bio.get(), pkey.get(), nullptr, nullptr, 0,
                             nullptr, nullptr);
    len = BIO_get_mem_data(key_bio.get(), &data);
    out.key_pem.assign(data, static_cast<std::size_t>(len));

    return out;
}

submission_set make_submission() {
    submission_set s;
    s.submission_set_unique_id = "1.2.3.4.5";
    s.entry_uuid = "urn:uuid:00000000-0000-0000-0000-000000000001";
    s.source_id = "1.2.276.0.7230010.3.0.3.6.1";
    s.patient_id = "P12345^^^&1.2.3.4&ISO";
    s.submission_time = "20260422120000";

    xds_document d;
    d.entry_uuid = "urn:uuid:00000000-0000-0000-0000-000000000002";
    d.unique_id = "1.2.840.10008.1.2.1";
    d.mime_type = "application/dicom";
    d.content = std::vector<std::uint8_t>{0x01, 0x02, 0x03};
    s.documents.push_back(std::move(d));
    return s;
}

}  // namespace

TEST_CASE("WS-Security signer produces a signature verifiable by OpenSSL",
          "[ihe][xds][wss]") {
    auto creds = make_rsa_credentials();
    auto s = make_submission();

    auto env_res = detail::build_iti41_envelope(s, {"doc-0@test"});
    REQUIRE(env_res.is_ok());
    auto env = env_res.value();

    auto sign_res = detail::sign_envelope(env, creds.cert_pem, creds.key_pem,
                                          "");
    REQUIRE(sign_res.is_ok());

    pugi::xml_document doc;
    REQUIRE(doc.load_buffer(env.xml.data(), env.xml.size()));

    auto security =
        doc.child("soap:Envelope").child("soap:Header").child("wsse:Security");
    auto signature = security.child("ds:Signature");
    REQUIRE(signature);

    auto signed_info = signature.child("ds:SignedInfo");
    REQUIRE(signed_info);
    auto sig_value_node = signature.child("ds:SignatureValue");
    REQUIRE(sig_value_node);
    const std::string sig_b64 = sig_value_node.text().get();
    REQUIRE(!sig_b64.empty());

    // MAJOR-2: the emitted signature must advertise the project-local
    // canonicalization URI, not exc-c14n, until a follow-up issue lands
    // real exc-c14n. Regressing this would resurrect the honesty bug.
    static constexpr const char* kKcenonC14nUri =
        "urn:kcenon:xds:c14n:pugixml-format-raw-v1";
    auto c14n = signed_info.child("ds:CanonicalizationMethod");
    REQUIRE(c14n);
    REQUIRE(std::string(c14n.attribute("Algorithm").value()) == kKcenonC14nUri);
    for (auto ref : signed_info.children("ds:Reference")) {
        auto xform = ref.child("ds:Transforms").child("ds:Transform");
        REQUIRE(std::string(xform.attribute("Algorithm").value()) ==
                kKcenonC14nUri);
    }

    struct xml_string_writer : pugi::xml_writer {
        std::string out;
        void write(const void* data, std::size_t size) override {
            out.append(static_cast<const char*>(data), size);
        }
    } w;
    signed_info.print(w, "", pugi::format_raw | pugi::format_no_declaration,
                      pugi::encoding_utf8);
    const std::string signed_info_bytes = w.out;

    bio_ptr b64(BIO_new(BIO_f_base64()));
    BIO* mem = BIO_new_mem_buf(sig_b64.data(), static_cast<int>(sig_b64.size()));
    BIO* chain = BIO_push(b64.release(), mem);
    bio_ptr owner(chain);
    BIO_set_flags(owner.get(), BIO_FLAGS_BASE64_NO_NL);
    std::vector<unsigned char> sig(sig_b64.size());
    const int decoded = BIO_read(owner.get(), sig.data(),
                                 static_cast<int>(sig.size()));
    REQUIRE(decoded > 0);
    sig.resize(static_cast<std::size_t>(decoded));

    bio_ptr cert_bio(
        BIO_new_mem_buf(creds.cert_pem.data(),
                        static_cast<int>(creds.cert_pem.size())));
    x509_ptr cert(PEM_read_bio_X509(cert_bio.get(), nullptr, nullptr, nullptr));
    REQUIRE(cert);
    pkey_ptr pubkey(X509_get_pubkey(cert.get()));
    REQUIRE(pubkey);

    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> md(
        EVP_MD_CTX_new(), &EVP_MD_CTX_free);
    REQUIRE(md);
    REQUIRE(EVP_DigestVerifyInit(md.get(), nullptr, EVP_sha256(), nullptr,
                                 pubkey.get()) == 1);
    REQUIRE(EVP_DigestVerifyUpdate(md.get(), signed_info_bytes.data(),
                                   signed_info_bytes.size()) == 1);
    const int ok = EVP_DigestVerifyFinal(md.get(), sig.data(), sig.size());
    REQUIRE(ok == 1);
}

TEST_CASE("WS-Security signer rejects empty material",
          "[ihe][xds][wss]") {
    auto s = make_submission();
    auto env_res = detail::build_iti41_envelope(s, {"doc-0@test"});
    REQUIRE(env_res.is_ok());
    auto env = env_res.value();

    SECTION("empty certificate") {
        auto r = detail::sign_envelope(env, "", "irrelevant", "");
        REQUIRE(r.is_err());
        REQUIRE(r.error().code ==
                static_cast<int>(error_code::signer_invalid_certificate));
    }
    SECTION("empty private key") {
        auto r = detail::sign_envelope(env, "irrelevant", "", "");
        REQUIRE(r.is_err());
        REQUIRE(r.error().code ==
                static_cast<int>(error_code::signer_invalid_private_key));
    }
    SECTION("malformed certificate") {
        auto r = detail::sign_envelope(env, "not-a-pem", "not-a-pem", "");
        REQUIRE(r.is_err());
        REQUIRE(r.error().code ==
                static_cast<int>(error_code::signer_invalid_certificate));
    }
}
