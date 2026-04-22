// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file wss_signer.cpp
 * @brief OpenSSL-backed WS-Security XML-DSig signer.
 *
 * Canonicalization policy
 * -----------------------
 * This signer does NOT implement Exclusive XML Canonicalization
 * (http://www.w3.org/2001/10/xml-exc-c14n#). It serializes referenced
 * nodes with pugixml's format_raw, which produces deterministic UTF-8
 * XML with no whitespace perturbation. Because the envelope built by
 * soap_envelope.cpp declares every namespace on <soap:Envelope> at build
 * time, every descendant inherits the same prefix bindings, so the byte
 * stream is stable under our own verifier. It is NOT interoperable with
 * a strict exc-c14n implementation (IHE Gazelle, .NET WIF, Apache CXF),
 * which would require namespace rewriting and attribute ordering.
 *
 * The advertised CanonicalizationMethod / Transform Algorithm URIs in
 * the emitted <ds:Signature> are therefore the project-local identifier
 *
 *     urn:kcenon:xds:c14n:pugixml-format-raw-v1
 *
 * NOT the exc-c14n URI. This is deliberate: advertising exc-c14n while
 * emitting something else would silently produce signatures that look
 * valid to our own test (EVP_DigestVerify over the same serializer) but
 * fail in any third-party verifier. The project-local URI makes
 * non-interoperable callers fail loudly at Algorithm check instead.
 *
 * Full exc-c14n + Apache CXF / Gazelle round-trip verification is
 * tracked as a follow-up issue and must land before production use
 * against a real XDS.b registry.
 *
 * Addresses review finding MAJOR-2 (Option A - honest URI).
 */

#include "wss_signer.h"

#include "soap_envelope.h"

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/sha.h>
#include <openssl/x509.h>

#include <pugixml.hpp>

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace kcenon::pacs::ihe::xds::detail {

namespace {

struct bio_deleter {
    void operator()(BIO* bio) const noexcept {
        if (bio) BIO_free_all(bio);
    }
};
using bio_ptr = std::unique_ptr<BIO, bio_deleter>;

struct x509_deleter {
    void operator()(X509* x) const noexcept {
        if (x) X509_free(x);
    }
};
using x509_ptr = std::unique_ptr<X509, x509_deleter>;

struct pkey_deleter {
    void operator()(EVP_PKEY* p) const noexcept {
        if (p) EVP_PKEY_free(p);
    }
};
using pkey_ptr = std::unique_ptr<EVP_PKEY, pkey_deleter>;

struct md_ctx_deleter {
    void operator()(EVP_MD_CTX* c) const noexcept {
        if (c) EVP_MD_CTX_free(c);
    }
};
using md_ctx_ptr = std::unique_ptr<EVP_MD_CTX, md_ctx_deleter>;

int pem_password_callback(char* buf, int size, int /*rwflag*/, void* userdata) {
    if (userdata == nullptr) return 0;
    const auto* pass = static_cast<const std::string*>(userdata);
    const int to_copy =
        (static_cast<int>(pass->size()) < size) ? static_cast<int>(pass->size())
                                                : size;
    std::memcpy(buf, pass->data(), static_cast<std::size_t>(to_copy));
    return to_copy;
}

x509_ptr load_cert(std::string_view pem) {
    bio_ptr bio(BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size())));
    if (!bio) return nullptr;
    X509* raw = PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr);
    return x509_ptr(raw);
}

pkey_ptr load_key(std::string_view pem, std::string_view password) {
    bio_ptr bio(BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size())));
    if (!bio) return nullptr;
    std::string pw(password);
    EVP_PKEY* raw = PEM_read_bio_PrivateKey(
        bio.get(), nullptr, pem_password_callback,
        pw.empty() ? nullptr : static_cast<void*>(&pw));
    return pkey_ptr(raw);
}

std::string base64_encode(const unsigned char* data, std::size_t len) {
    bio_ptr b64(BIO_new(BIO_f_base64()));
    if (!b64) return {};
    BIO* mem = BIO_new(BIO_s_mem());
    if (!mem) return {};
    BIO* chain = BIO_push(b64.release(), mem);
    bio_ptr owner(chain);
    BIO_set_flags(owner.get(), BIO_FLAGS_BASE64_NO_NL);
    BIO_write(owner.get(), data, static_cast<int>(len));
    BIO_flush(owner.get());
    BUF_MEM* bptr = nullptr;
    BIO_get_mem_ptr(owner.get(), &bptr);
    if (!bptr) return {};
    return std::string(bptr->data, bptr->length);
}

std::vector<unsigned char> cert_to_der(X509* cert) {
    int len = i2d_X509(cert, nullptr);
    if (len <= 0) return {};
    std::vector<unsigned char> out(static_cast<std::size_t>(len));
    unsigned char* p = out.data();
    if (i2d_X509(cert, &p) != len) return {};
    return out;
}

std::string sha256_base64(std::string_view bytes) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size(),
           digest);
    return base64_encode(digest, SHA256_DIGEST_LENGTH);
}

struct xml_string_writer : pugi::xml_writer {
    std::string out;
    void write(const void* data, std::size_t size) override {
        out.append(static_cast<const char*>(data), size);
    }
};

std::string serialize_subtree(pugi::xml_node node) {
    xml_string_writer w;
    node.print(w, "", pugi::format_raw | pugi::format_no_declaration,
               pugi::encoding_utf8);
    return w.out;
}

}  // namespace

kcenon::common::Result<bool> sign_envelope(built_envelope& env,
                                           std::string_view cert_pem,
                                           std::string_view key_pem,
                                           std::string_view key_password) {
    if (cert_pem.empty()) {
        return kcenon::common::make_error<bool>(
            static_cast<int>(error_code::signer_invalid_certificate),
            "certificate PEM is empty", std::string(error_source));
    }
    if (key_pem.empty()) {
        return kcenon::common::make_error<bool>(
            static_cast<int>(error_code::signer_invalid_private_key),
            "private key PEM is empty", std::string(error_source));
    }

    auto cert = load_cert(cert_pem);
    if (!cert) {
        return kcenon::common::make_error<bool>(
            static_cast<int>(error_code::signer_invalid_certificate),
            "PEM_read_bio_X509 failed", std::string(error_source));
    }
    auto key = load_key(key_pem, key_password);
    if (!key) {
        return kcenon::common::make_error<bool>(
            static_cast<int>(error_code::signer_invalid_private_key),
            "PEM_read_bio_PrivateKey failed", std::string(error_source));
    }

    const auto der = cert_to_der(cert.get());
    if (der.empty()) {
        return kcenon::common::make_error<bool>(
            static_cast<int>(error_code::signer_invalid_certificate),
            "i2d_X509 failed", std::string(error_source));
    }
    const std::string cert_b64 = base64_encode(der.data(), der.size());
    if (cert_b64.empty()) {
        return kcenon::common::make_error<bool>(
            static_cast<int>(error_code::signer_invalid_certificate),
            "base64 of DER certificate failed", std::string(error_source));
    }

    pugi::xml_document doc;
    auto parse_result =
        doc.load_buffer(env.xml.data(), env.xml.size(),
                        pugi::parse_default, pugi::encoding_utf8);
    if (!parse_result) {
        return kcenon::common::make_error<bool>(
            static_cast<int>(error_code::envelope_build_failed),
            std::string("failed to reparse envelope: ") +
                parse_result.description(),
            std::string(error_source));
    }

    auto envelope = doc.child("soap:Envelope");
    auto header = envelope.child("soap:Header");
    auto security = header.child("wsse:Security");
    auto bst = security.child("wsse:BinarySecurityToken");
    auto timestamp = security.child("wsu:Timestamp");
    auto body = envelope.child("soap:Body");

    if (!envelope || !header || !security || !bst || !timestamp || !body) {
        return kcenon::common::make_error<bool>(
            static_cast<int>(error_code::envelope_build_failed),
            "envelope is missing required WS-Security scaffolding",
            std::string(error_source));
    }

    bst.text().set(cert_b64.c_str());

    const std::string body_c14n = serialize_subtree(body);
    const std::string ts_c14n = serialize_subtree(timestamp);

    if (body_c14n.empty() || ts_c14n.empty()) {
        return kcenon::common::make_error<bool>(
            static_cast<int>(error_code::signer_digest_failed),
            "serialized Body or Timestamp is empty",
            std::string(error_source));
    }

    const std::string body_digest = sha256_base64(body_c14n);
    const std::string ts_digest = sha256_base64(ts_c14n);
    if (body_digest.empty() || ts_digest.empty()) {
        return kcenon::common::make_error<bool>(
            static_cast<int>(error_code::signer_digest_failed),
            "SHA-256 digest failed", std::string(error_source));
    }

    auto signature = security.append_child("ds:Signature");
    signature.append_attribute("xmlns:ds") = "http://www.w3.org/2000/09/xmldsig#";

    // Project-local canonicalization identifier. See the canonicalization
    // policy section at the top of this file. Do not substitute the
    // exc-c14n URI until the signer actually implements exc-c14n.
    static constexpr const char* kKcenonC14nUri =
        "urn:kcenon:xds:c14n:pugixml-format-raw-v1";

    auto signed_info = signature.append_child("ds:SignedInfo");
    auto c14n_method = signed_info.append_child("ds:CanonicalizationMethod");
    c14n_method.append_attribute("Algorithm") = kKcenonC14nUri;
    auto sig_method = signed_info.append_child("ds:SignatureMethod");
    sig_method.append_attribute("Algorithm") =
        "http://www.w3.org/2001/04/xmldsig-more#rsa-sha256";

    const auto add_reference = [&](const std::string& uri,
                                   const std::string& digest_b64) {
        auto ref = signed_info.append_child("ds:Reference");
        ref.append_attribute("URI") = ("#" + uri).c_str();
        auto transforms = ref.append_child("ds:Transforms");
        auto xform = transforms.append_child("ds:Transform");
        xform.append_attribute("Algorithm") = kKcenonC14nUri;
        auto digest_method = ref.append_child("ds:DigestMethod");
        digest_method.append_attribute("Algorithm") =
            "http://www.w3.org/2001/04/xmlenc#sha256";
        ref.append_child("ds:DigestValue").text().set(digest_b64.c_str());
    };
    add_reference(env.body_id, body_digest);
    add_reference(env.timestamp_id, ts_digest);

    const std::string signed_info_bytes = serialize_subtree(signed_info);
    if (signed_info_bytes.empty()) {
        return kcenon::common::make_error<bool>(
            static_cast<int>(error_code::signer_sign_failed),
            "serialized SignedInfo is empty", std::string(error_source));
    }

    md_ctx_ptr md(EVP_MD_CTX_new());
    if (!md) {
        return kcenon::common::make_error<bool>(
            static_cast<int>(error_code::signer_sign_failed),
            "EVP_MD_CTX_new failed", std::string(error_source));
    }
    if (EVP_DigestSignInit(md.get(), nullptr, EVP_sha256(), nullptr,
                           key.get()) != 1) {
        return kcenon::common::make_error<bool>(
            static_cast<int>(error_code::signer_sign_failed),
            "EVP_DigestSignInit failed", std::string(error_source));
    }
    if (EVP_DigestSignUpdate(
            md.get(), signed_info_bytes.data(), signed_info_bytes.size()) != 1) {
        return kcenon::common::make_error<bool>(
            static_cast<int>(error_code::signer_sign_failed),
            "EVP_DigestSignUpdate failed", std::string(error_source));
    }

    std::size_t sig_len = 0;
    if (EVP_DigestSignFinal(md.get(), nullptr, &sig_len) != 1 || sig_len == 0) {
        return kcenon::common::make_error<bool>(
            static_cast<int>(error_code::signer_sign_failed),
            "EVP_DigestSignFinal probe failed", std::string(error_source));
    }
    std::vector<unsigned char> sig(sig_len);
    if (EVP_DigestSignFinal(md.get(), sig.data(), &sig_len) != 1) {
        return kcenon::common::make_error<bool>(
            static_cast<int>(error_code::signer_sign_failed),
            "EVP_DigestSignFinal failed", std::string(error_source));
    }
    sig.resize(sig_len);
    const std::string sig_b64 = base64_encode(sig.data(), sig.size());

    signature.append_child("ds:SignatureValue").text().set(sig_b64.c_str());

    auto key_info = signature.append_child("ds:KeyInfo");
    auto sec_ref = key_info.append_child("wsse:SecurityTokenReference");
    auto ref = sec_ref.append_child("wsse:Reference");
    ref.append_attribute("URI") = ("#" + env.binary_security_token_id).c_str();
    ref.append_attribute("ValueType") =
        "http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-x509-token-"
        "profile-1.0#X509v3";

    xml_string_writer w;
    doc.save(w, "", pugi::format_raw | pugi::format_no_declaration,
             pugi::encoding_utf8);
    env.xml = R"(<?xml version="1.0" encoding="UTF-8"?>)" + w.out;
    return true;
}

}  // namespace kcenon::pacs::ihe::xds::detail
