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

#include <cstring>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace kcenon::pacs::ihe::xds::detail {

namespace {

// Project-local canonicalization identifier. See the canonicalization
// policy section at the top of this file. Single-source to prevent
// drift between sign_envelope and verify_envelope - any divergence
// would silently accept signatures with a different byte stream.
constexpr const char* kKcenonC14nUri =
    "urn:kcenon:xds:c14n:pugixml-format-raw-v1";

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

// Strip any whitespace (CR/LF/space/tab) the BST text may carry - the
// signer emits a single-line base64 string but a real registry will often
// wrap to 64 or 76 columns. OpenSSL's BIO_f_base64 decoder requires clean
// input or BIO_FLAGS_BASE64_NO_NL; stripping is simpler and avoids
// heuristics about which flag to use.
std::string strip_b64_whitespace(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (const char c : s) {
        if (c != '\r' && c != '\n' && c != ' ' && c != '\t') {
            out.push_back(c);
        }
    }
    return out;
}

std::vector<unsigned char> base64_decode(std::string_view b64_in) {
    const std::string cleaned = strip_b64_whitespace(b64_in);
    if (cleaned.empty()) return {};
    bio_ptr b64(BIO_new(BIO_f_base64()));
    if (!b64) return {};
    BIO* mem =
        BIO_new_mem_buf(cleaned.data(), static_cast<int>(cleaned.size()));
    if (!mem) return {};
    BIO* chain = BIO_push(b64.release(), mem);
    bio_ptr owner(chain);
    BIO_set_flags(owner.get(), BIO_FLAGS_BASE64_NO_NL);
    std::vector<unsigned char> out(cleaned.size());
    const int n = BIO_read(owner.get(), out.data(), static_cast<int>(out.size()));
    if (n <= 0) return {};
    out.resize(static_cast<std::size_t>(n));
    return out;
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

namespace {

// The ds:SignedInfo Reference uses "#<wsu:Id>" as its URI attribute. Strip
// the leading '#' so we can match it against the actual id values emitted
// on wsu:Timestamp and soap:Body.
std::string strip_uri_fragment(const char* uri) {
    if (uri == nullptr) return {};
    if (uri[0] == '#') return std::string(uri + 1);
    return std::string(uri);
}

// pugixml's XPath evaluator has no way to bind the wsu namespace prefix
// for a `@wsu:Id` predicate without a registered xpath_variable_set, so
// walk the tree manually. The envelope is small (headers + a single body
// referenced from the Signature); a linear walk is fast.
struct wsu_id_walker : pugi::xml_tree_walker {
    std::string needle;
    pugi::xml_node match;
    bool for_each(pugi::xml_node& n) override {
        const auto attr = n.attribute("wsu:Id");
        if (!attr.empty() && attr.as_string() == needle) {
            match = n;
            return false;
        }
        return true;
    }
};

pugi::xml_node find_by_wsu_id(pugi::xml_node root, const std::string& id) {
    wsu_id_walker w;
    w.needle = id;
    root.traverse(w);
    return w.match;
}

}  // namespace

kcenon::common::Result<bool> verify_envelope_integrity(
    std::string_view signed_xml) {
    if (signed_xml.empty()) {
        return kcenon::common::make_error<bool>(
            static_cast<int>(
                error_code::consumer_signature_verification_failed),
            "signed envelope is empty", std::string(error_source));
    }

    pugi::xml_document doc;
    auto parse = doc.load_buffer(signed_xml.data(), signed_xml.size(),
                                 pugi::parse_default, pugi::encoding_utf8);
    if (!parse) {
        return kcenon::common::make_error<bool>(
            static_cast<int>(
                error_code::consumer_signature_verification_failed),
            std::string("failed to reparse signed envelope: ") +
                parse.description(),
            std::string(error_source));
    }

    auto envelope = doc.child("soap:Envelope");
    auto header = envelope.child("soap:Header");
    auto security = header.child("wsse:Security");
    auto signature = security.child("ds:Signature");
    auto bst = security.child("wsse:BinarySecurityToken");

    if (!envelope || !header || !security) {
        return kcenon::common::make_error<bool>(
            static_cast<int>(
                error_code::consumer_signature_verification_failed),
            "envelope lacks WS-Security scaffolding",
            std::string(error_source));
    }
    if (!signature) {
        return kcenon::common::make_error<bool>(
            static_cast<int>(error_code::consumer_signature_missing),
            "envelope has no ds:Signature element",
            std::string(error_source));
    }
    if (!bst) {
        return kcenon::common::make_error<bool>(
            static_cast<int>(
                error_code::consumer_signature_verification_failed),
            "envelope has no wsse:BinarySecurityToken",
            std::string(error_source));
    }

    // Enforce honest-URI policy on every Algorithm attribute within
    // SignedInfo before trusting the digests.
    auto signed_info = signature.child("ds:SignedInfo");
    if (!signed_info) {
        return kcenon::common::make_error<bool>(
            static_cast<int>(
                error_code::consumer_signature_verification_failed),
            "Signature has no SignedInfo", std::string(error_source));
    }
    auto c14n = signed_info.child("ds:CanonicalizationMethod");
    if (!c14n ||
        std::string(c14n.attribute("Algorithm").as_string()) != kKcenonC14nUri) {
        return kcenon::common::make_error<bool>(
            static_cast<int>(
                error_code::consumer_signature_verification_failed),
            "CanonicalizationMethod is not the kcenon project-local URI",
            std::string(error_source));
    }

    // Load public key from BST.
    const std::string bst_b64 = strip_b64_whitespace(bst.text().as_string());
    const auto der = base64_decode(bst_b64);
    if (der.empty()) {
        return kcenon::common::make_error<bool>(
            static_cast<int>(
                error_code::consumer_signature_verification_failed),
            "BinarySecurityToken base64 decode failed",
            std::string(error_source));
    }

    // SECURITY NOTE: the X.509 we are about to pull out of the BST is
    // used only to recover a public key for EVP_DigestVerify; it is NOT
    // validated against a trust anchor. See the Doxygen on
    // verify_envelope_integrity for the deferral and tracking issue.

    const unsigned char* der_ptr = der.data();
    x509_ptr cert(d2i_X509(nullptr, &der_ptr, static_cast<long>(der.size())));
    if (!cert) {
        return kcenon::common::make_error<bool>(
            static_cast<int>(
                error_code::consumer_signature_verification_failed),
            "d2i_X509 failed on BinarySecurityToken",
            std::string(error_source));
    }
    pkey_ptr pkey(X509_get_pubkey(cert.get()));
    if (!pkey) {
        return kcenon::common::make_error<bool>(
            static_cast<int>(
                error_code::consumer_signature_verification_failed),
            "X509_get_pubkey failed", std::string(error_source));
    }

    // Recompute digests for every Reference in SignedInfo and compare
    // against the advertised DigestValue.
    for (auto ref = signed_info.child("ds:Reference"); ref;
         ref = ref.next_sibling("ds:Reference")) {
        for (auto xform = ref.child("ds:Transforms").child("ds:Transform");
             xform; xform = xform.next_sibling("ds:Transform")) {
            if (std::string(xform.attribute("Algorithm").as_string()) !=
                kKcenonC14nUri) {
                return kcenon::common::make_error<bool>(
                    static_cast<int>(
                        error_code::consumer_signature_verification_failed),
                    "Reference Transform is not the kcenon project-local URI",
                    std::string(error_source));
            }
        }
        const std::string uri = strip_uri_fragment(
            ref.attribute("URI").as_string());
        if (uri.empty()) {
            return kcenon::common::make_error<bool>(
                static_cast<int>(
                    error_code::consumer_signature_verification_failed),
                "Reference URI is empty", std::string(error_source));
        }
        auto target = find_by_wsu_id(envelope, uri);
        if (!target) {
            return kcenon::common::make_error<bool>(
                static_cast<int>(
                    error_code::consumer_signature_verification_failed),
                "Reference target not found: " + uri,
                std::string(error_source));
        }
        const std::string bytes = serialize_subtree(target);
        const std::string want_digest = sha256_base64(bytes);
        const std::string have_digest = strip_b64_whitespace(
            ref.child("ds:DigestValue").text().as_string());
        if (want_digest != have_digest) {
            return kcenon::common::make_error<bool>(
                static_cast<int>(
                    error_code::consumer_signature_verification_failed),
                "DigestValue mismatch for reference: " + uri,
                std::string(error_source));
        }
    }

    // Verify the signature over SignedInfo.
    const std::string signed_info_bytes = serialize_subtree(signed_info);
    if (signed_info_bytes.empty()) {
        return kcenon::common::make_error<bool>(
            static_cast<int>(
                error_code::consumer_signature_verification_failed),
            "serialized SignedInfo is empty", std::string(error_source));
    }
    const auto sig_bytes = base64_decode(
        signature.child("ds:SignatureValue").text().as_string());
    if (sig_bytes.empty()) {
        return kcenon::common::make_error<bool>(
            static_cast<int>(
                error_code::consumer_signature_verification_failed),
            "SignatureValue base64 decode failed",
            std::string(error_source));
    }

    md_ctx_ptr md(EVP_MD_CTX_new());
    if (!md) {
        return kcenon::common::make_error<bool>(
            static_cast<int>(
                error_code::consumer_signature_verification_failed),
            "EVP_MD_CTX_new failed", std::string(error_source));
    }
    if (EVP_DigestVerifyInit(md.get(), nullptr, EVP_sha256(), nullptr,
                             pkey.get()) != 1) {
        return kcenon::common::make_error<bool>(
            static_cast<int>(
                error_code::consumer_signature_verification_failed),
            "EVP_DigestVerifyInit failed", std::string(error_source));
    }
    if (EVP_DigestVerifyUpdate(md.get(), signed_info_bytes.data(),
                               signed_info_bytes.size()) != 1) {
        return kcenon::common::make_error<bool>(
            static_cast<int>(
                error_code::consumer_signature_verification_failed),
            "EVP_DigestVerifyUpdate failed", std::string(error_source));
    }
    const int verdict =
        EVP_DigestVerifyFinal(md.get(), sig_bytes.data(), sig_bytes.size());
    if (verdict != 1) {
        return kcenon::common::make_error<bool>(
            static_cast<int>(
                error_code::consumer_signature_verification_failed),
            "EVP_DigestVerifyFinal rejected the signature",
            std::string(error_source));
    }
    return true;
}

std::vector<std::uint8_t> base64_decode_bytes(std::string_view b64) {
    const auto raw = base64_decode(b64);
    std::vector<std::uint8_t> out;
    out.reserve(raw.size());
    for (const unsigned char c : raw) {
        out.push_back(static_cast<std::uint8_t>(c));
    }
    return out;
}

}  // namespace kcenon::pacs::ihe::xds::detail
