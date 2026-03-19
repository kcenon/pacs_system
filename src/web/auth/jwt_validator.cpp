// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @file jwt_validator.cpp
 * @brief Implementation of JWT token validation
 *
 * @see RFC 7519 - JSON Web Token (JWT)
 * @see Issue #860 - Add OAuth 2.0 configuration and JWT token validation core
 */

#include "pacs/web/auth/jwt_validator.hpp"

#include <algorithm>
#include <array>
#include <set>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

// Crow JSON for parsing JWT header/payload
#include <crow/json.h>

// OpenSSL for signature verification (conditional)
#ifdef PACS_WITH_DIGITAL_SIGNATURES
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#endif

namespace kcenon::pacs::web::auth {

// =============================================================================
// Base64url Decode
// =============================================================================

namespace {

// Base64url alphabet (RFC 4648 Section 5)
constexpr std::array<int, 256> make_decode_table() {
    std::array<int, 256> table{};
    for (auto& v : table) v = -1;

    for (int i = 0; i < 26; ++i) table[static_cast<unsigned char>('A' + i)] = i;
    for (int i = 0; i < 26; ++i) table[static_cast<unsigned char>('a' + i)] = 26 + i;
    for (int i = 0; i < 10; ++i) table[static_cast<unsigned char>('0' + i)] = 52 + i;
    table[static_cast<unsigned char>('-')] = 62;  // Base64url uses - instead of +
    table[static_cast<unsigned char>('_')] = 63;  // Base64url uses _ instead of /

    return table;
}

constexpr auto kDecodeTable = make_decode_table();

/// Parse a JSON string value from crow::json
std::string json_string(const crow::json::rvalue& json, const char* key) {
    if (!json.has(key)) return {};
    auto& v = json[key];
    if (v.t() == crow::json::type::String) {
        return std::string(v.s());
    }
    return {};
}

/// Parse a JSON integer value from crow::json
std::int64_t json_int64(const crow::json::rvalue& json, const char* key) {
    if (!json.has(key)) return 0;
    auto& v = json[key];
    if (v.t() == crow::json::type::Number) {
        return v.i();
    }
    return 0;
}

/// Get current Unix timestamp
std::int64_t current_timestamp() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

/// Parse space-delimited scope string into vector
std::vector<std::string> parse_scopes(const std::string& scope_string) {
    std::vector<std::string> scopes;
    std::string current;
    for (char c : scope_string) {
        if (c == ' ') {
            if (!current.empty()) {
                scopes.push_back(std::move(current));
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        scopes.push_back(std::move(current));
    }
    return scopes;
}

}  // namespace

std::optional<std::string> base64url_decode(std::string_view input) {
    if (input.empty()) return std::string{};

    // Calculate output size (3 bytes per 4 chars)
    size_t padding = (4 - (input.size() % 4)) % 4;
    size_t total_len = input.size() + padding;
    size_t output_len = (total_len / 4) * 3;

    std::string result;
    result.reserve(output_len);

    uint32_t buffer = 0;
    int bits_collected = 0;

    for (char c : input) {
        if (c == '=') continue;  // Skip padding if present

        int val = kDecodeTable[static_cast<unsigned char>(c)];
        if (val < 0) return std::nullopt;  // Invalid character

        buffer = (buffer << 6) | static_cast<uint32_t>(val);
        bits_collected += 6;

        if (bits_collected >= 8) {
            bits_collected -= 8;
            result += static_cast<char>((buffer >> bits_collected) & 0xFF);
        }
    }

    return result;
}

// =============================================================================
// JWT Error Messages
// =============================================================================

std::string_view jwt_error_message(jwt_error error) noexcept {
    switch (error) {
    case jwt_error::none:
        return "no error";
    case jwt_error::malformed_token:
        return "malformed JWT token: expected 3 dot-separated segments";
    case jwt_error::invalid_base64:
        return "invalid Base64url encoding in JWT segment";
    case jwt_error::invalid_json:
        return "invalid JSON in JWT segment";
    case jwt_error::unsupported_algorithm:
        return "unsupported JWT signing algorithm";
    case jwt_error::invalid_signature:
        return "JWT signature verification failed";
    case jwt_error::token_expired:
        return "JWT token has expired";
    case jwt_error::token_not_yet_valid:
        return "JWT token is not yet valid";
    case jwt_error::invalid_issuer:
        return "JWT issuer does not match expected value";
    case jwt_error::invalid_audience:
        return "JWT audience does not match expected value";
    case jwt_error::missing_required_claim:
        return "required JWT claim is missing";
    case jwt_error::signature_not_available:
        return "signature verification not available (OpenSSL required)";
    }
    return "unknown JWT error";
}

// =============================================================================
// jwt_validator Implementation
// =============================================================================

jwt_validator::jwt_validator(const oauth2_config& config)
    : config_(config) {}

std::pair<jwt_token, jwt_error> jwt_validator::decode(
    std::string_view token_string) const {

    jwt_token token;

    // Split into 3 parts: header.payload.signature
    auto dot1 = token_string.find('.');
    if (dot1 == std::string_view::npos) {
        return {token, jwt_error::malformed_token};
    }

    auto dot2 = token_string.find('.', dot1 + 1);
    if (dot2 == std::string_view::npos) {
        return {token, jwt_error::malformed_token};
    }

    // Check no additional dots
    if (token_string.find('.', dot2 + 1) != std::string_view::npos) {
        return {token, jwt_error::malformed_token};
    }

    auto header_b64 = token_string.substr(0, dot1);
    auto payload_b64 = token_string.substr(dot1 + 1, dot2 - dot1 - 1);
    auto signature_b64 = token_string.substr(dot2 + 1);

    // Store header.payload for signature verification
    token.header_payload = std::string(token_string.substr(0, dot2));

    // Decode header
    auto header_json = base64url_decode(header_b64);
    if (!header_json) {
        return {token, jwt_error::invalid_base64};
    }

    // Decode payload
    auto payload_json = base64url_decode(payload_b64);
    if (!payload_json) {
        return {token, jwt_error::invalid_base64};
    }

    // Decode signature
    auto sig_bytes = base64url_decode(signature_b64);
    if (!sig_bytes) {
        return {token, jwt_error::invalid_base64};
    }
    token.signature_bytes = std::move(*sig_bytes);

    // Parse header JSON
    auto header_rv = crow::json::load(*header_json);
    if (!header_rv) {
        return {token, jwt_error::invalid_json};
    }

    token.header.alg = json_string(header_rv, "alg");
    token.header.typ = json_string(header_rv, "typ");
    token.header.kid = json_string(header_rv, "kid");

    if (token.header.alg.empty()) {
        return {token, jwt_error::missing_required_claim};
    }

    // Block dangerous algorithms unconditionally (CVE: JWT alg:none attack)
    static const std::set<std::string> kDangerousAlgorithms = {
        "none", "None", "NONE", "HS256", "HS384", "HS512"};
    if (kDangerousAlgorithms.count(token.header.alg)) {
        return {token, jwt_error::unsupported_algorithm};
    }

    // Check algorithm is allowed
    auto& allowed = config_.allowed_algorithms;
    if (!allowed.empty()) {
        bool alg_allowed = std::any_of(
            allowed.begin(), allowed.end(),
            [&](const std::string& a) { return a == token.header.alg; });
        if (!alg_allowed) {
            return {token, jwt_error::unsupported_algorithm};
        }
    }

    // Parse payload JSON
    auto payload_rv = crow::json::load(*payload_json);
    if (!payload_rv) {
        return {token, jwt_error::invalid_json};
    }

    token.claims.iss = json_string(payload_rv, "iss");
    token.claims.sub = json_string(payload_rv, "sub");
    token.claims.aud = json_string(payload_rv, "aud");
    token.claims.exp = json_int64(payload_rv, "exp");
    token.claims.iat = json_int64(payload_rv, "iat");
    token.claims.nbf = json_int64(payload_rv, "nbf");
    token.claims.jti = json_string(payload_rv, "jti");

    // Parse scopes from "scope" claim (space-delimited string per RFC 6749)
    auto scope_str = json_string(payload_rv, "scope");
    if (!scope_str.empty()) {
        token.claims.scopes = parse_scopes(scope_str);
    }

    return {token, jwt_error::none};
}

jwt_error jwt_validator::validate_claims(const jwt_claims& claims) const {
    auto now = current_timestamp();
    auto skew = static_cast<std::int64_t>(config_.clock_skew_seconds);

    // Check expiration
    if (claims.exp > 0 && now > claims.exp + skew) {
        return jwt_error::token_expired;
    }

    // Check not-before
    if (claims.nbf > 0 && now < claims.nbf - skew) {
        return jwt_error::token_not_yet_valid;
    }

    // Check issuer
    if (!config_.issuer.empty() && claims.iss != config_.issuer) {
        return jwt_error::invalid_issuer;
    }

    // Check audience
    if (!config_.audience.empty() && claims.aud != config_.audience) {
        return jwt_error::invalid_audience;
    }

    // Sub claim is typically required
    if (claims.sub.empty()) {
        return jwt_error::missing_required_claim;
    }

    return jwt_error::none;
}

// =============================================================================
// Signature Verification (OpenSSL)
// =============================================================================

#ifdef PACS_WITH_DIGITAL_SIGNATURES

namespace {

/// RAII wrapper for OpenSSL BIO
struct bio_deleter {
    void operator()(BIO* bio) const { BIO_free_all(bio); }
};
using bio_ptr = std::unique_ptr<BIO, bio_deleter>;

/// RAII wrapper for OpenSSL EVP_PKEY
struct pkey_deleter {
    void operator()(EVP_PKEY* key) const { EVP_PKEY_free(key); }
};
using pkey_ptr = std::unique_ptr<EVP_PKEY, pkey_deleter>;

/// RAII wrapper for OpenSSL EVP_MD_CTX
struct md_ctx_deleter {
    void operator()(EVP_MD_CTX* ctx) const { EVP_MD_CTX_free(ctx); }
};
using md_ctx_ptr = std::unique_ptr<EVP_MD_CTX, md_ctx_deleter>;

/// Load a PEM public key into EVP_PKEY
pkey_ptr load_public_key(std::string_view pem) {
    bio_ptr bio(BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size())));
    if (!bio) return nullptr;

    EVP_PKEY* key = PEM_read_bio_PUBKEY(bio.get(), nullptr, nullptr, nullptr);
    return pkey_ptr(key);
}

/// Verify a signature using EVP_DigestVerify
bool verify_evp_signature(
    EVP_PKEY* key,
    const EVP_MD* md,
    const std::string& signed_data,
    const std::string& signature) {

    md_ctx_ptr ctx(EVP_MD_CTX_new());
    if (!ctx) return false;

    if (EVP_DigestVerifyInit(ctx.get(), nullptr, md, nullptr, key) != 1) {
        return false;
    }

    if (EVP_DigestVerifyUpdate(ctx.get(), signed_data.data(),
                                signed_data.size()) != 1) {
        return false;
    }

    int rc = EVP_DigestVerifyFinal(
        ctx.get(),
        reinterpret_cast<const unsigned char*>(signature.data()),
        signature.size());

    return rc == 1;
}

/// Convert ES256 raw R||S signature (64 bytes) to DER-encoded ECDSA signature
/// ES256 JWT signatures are raw concatenation of R and S (each 32 bytes)
/// OpenSSL expects DER-encoded ASN.1 SEQUENCE { INTEGER R, INTEGER S }
std::string es256_raw_to_der(const std::string& raw_sig) {
    if (raw_sig.size() != 64) return {};

    auto encode_integer = [](const unsigned char* data, size_t len,
                             std::string& out) {
        // Skip leading zeros but keep at least one byte
        size_t start = 0;
        while (start < len - 1 && data[start] == 0) ++start;

        // Add leading zero if high bit set (positive integer)
        bool need_pad = (data[start] & 0x80) != 0;

        out += static_cast<char>(0x02);  // INTEGER tag
        out += static_cast<char>(len - start + (need_pad ? 1 : 0));
        if (need_pad) out += '\0';
        out.append(reinterpret_cast<const char*>(data + start), len - start);
    };

    const auto* sig_data = reinterpret_cast<const unsigned char*>(raw_sig.data());

    std::string r_encoded, s_encoded;
    encode_integer(sig_data, 32, r_encoded);
    encode_integer(sig_data + 32, 32, s_encoded);

    std::string der;
    der += static_cast<char>(0x30);  // SEQUENCE tag
    der += static_cast<char>(r_encoded.size() + s_encoded.size());
    der += r_encoded;
    der += s_encoded;

    return der;
}

}  // namespace

bool jwt_validator::verify_rs256(
    const jwt_token& token,
    std::string_view public_key_pem) const {

    auto key = load_public_key(public_key_pem);
    if (!key) return false;

    return verify_evp_signature(
        key.get(), EVP_sha256(),
        token.header_payload, token.signature_bytes);
}

bool jwt_validator::verify_es256(
    const jwt_token& token,
    std::string_view public_key_pem) const {

    auto key = load_public_key(public_key_pem);
    if (!key) return false;

    // Convert raw R||S to DER format for OpenSSL
    auto der_sig = es256_raw_to_der(token.signature_bytes);
    if (der_sig.empty()) return false;

    return verify_evp_signature(
        key.get(), EVP_sha256(),
        token.header_payload, der_sig);
}

#else  // !PACS_WITH_DIGITAL_SIGNATURES

bool jwt_validator::verify_rs256(
    const jwt_token& /*token*/,
    std::string_view /*public_key_pem*/) const {
    return false;
}

bool jwt_validator::verify_es256(
    const jwt_token& /*token*/,
    std::string_view /*public_key_pem*/) const {
    return false;
}

#endif  // PACS_WITH_DIGITAL_SIGNATURES

// =============================================================================
// Scope Checking
// =============================================================================

bool jwt_validator::has_scope(
    const jwt_claims& claims,
    std::string_view scope) noexcept {

    return std::any_of(claims.scopes.begin(), claims.scopes.end(),
                       [scope](const std::string& s) { return s == scope; });
}

bool jwt_validator::has_any_scope(
    const jwt_claims& claims,
    const std::vector<std::string>& scopes) noexcept {

    return std::any_of(scopes.begin(), scopes.end(),
                       [&claims](const std::string& scope) {
                           return has_scope(claims, scope);
                       });
}

const oauth2_config& jwt_validator::config() const noexcept {
    return config_;
}

}  // namespace kcenon::pacs::web::auth
