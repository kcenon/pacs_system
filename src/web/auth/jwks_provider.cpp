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
 * @file jwks_provider.cpp
 * @brief Implementation of JWKS key management and JWK-to-PEM conversion
 *
 * @see RFC 7517 - JSON Web Key (JWK)
 * @see RFC 7518 - JSON Web Algorithms (JWA)
 * @see Issue #852 - Implement OAuth 2.0 authorization for DICOMweb endpoints
 */

#include "pacs/web/auth/jwks_provider.hpp"
#include "pacs/web/auth/jwt_validator.hpp"

#include <crow/json.h>

#include <algorithm>
#include <cstddef>
#include <cstring>

// OpenSSL for JWK-to-PEM conversion (conditional)
#ifdef PACS_WITH_DIGITAL_SIGNATURES
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#endif

namespace pacs::web::auth {

// =============================================================================
// JWK to PEM Conversion (OpenSSL)
// =============================================================================

#ifdef PACS_WITH_DIGITAL_SIGNATURES

namespace {

/// RAII wrapper for OpenSSL BIGNUM
struct bn_deleter {
    void operator()(BIGNUM* bn) const { BN_free(bn); }
};
using bn_ptr = std::unique_ptr<BIGNUM, bn_deleter>;

/// RAII wrapper for OpenSSL EVP_PKEY
struct pkey_deleter {
    void operator()(EVP_PKEY* key) const { EVP_PKEY_free(key); }
};
using pkey_ptr = std::unique_ptr<EVP_PKEY, pkey_deleter>;

/// RAII wrapper for OpenSSL BIO
struct bio_deleter {
    void operator()(BIO* bio) const { BIO_free_all(bio); }
};
using bio_ptr = std::unique_ptr<BIO, bio_deleter>;

/// RAII wrapper for OpenSSL EVP_PKEY_CTX
struct pkey_ctx_deleter {
    void operator()(EVP_PKEY_CTX* ctx) const { EVP_PKEY_CTX_free(ctx); }
};
using pkey_ctx_ptr = std::unique_ptr<EVP_PKEY_CTX, pkey_ctx_deleter>;

/// Convert decoded bytes to BIGNUM
bn_ptr bytes_to_bn(const std::string& bytes) {
    return bn_ptr(BN_bin2bn(
        reinterpret_cast<const unsigned char*>(bytes.data()),
        static_cast<int>(bytes.size()), nullptr));
}

/// Serialize EVP_PKEY to PEM string
std::string pkey_to_pem(EVP_PKEY* key) {
    bio_ptr bio(BIO_new(BIO_s_mem()));
    if (!bio) return {};

    if (PEM_write_bio_PUBKEY(bio.get(), key) != 1) return {};

    char* data = nullptr;
    long len = BIO_get_mem_data(bio.get(), &data);
    if (len <= 0 || !data) return {};

    return std::string(data, static_cast<size_t>(len));
}

/// Convert RSA JWK (n, e) to PEM public key
std::string rsa_jwk_to_pem(const std::string& n_b64, const std::string& e_b64) {
    auto n_bytes = base64url_decode(n_b64);
    auto e_bytes = base64url_decode(e_b64);
    if (!n_bytes || !e_bytes) return {};

    auto n = bytes_to_bn(*n_bytes);
    auto e = bytes_to_bn(*e_bytes);
    if (!n || !e) return {};

    // Create RSA key using EVP_PKEY_fromdata (OpenSSL 3.0+)
    pkey_ctx_ptr ctx(EVP_PKEY_CTX_new_from_name(nullptr, "RSA", nullptr));
    if (!ctx) return {};

    if (EVP_PKEY_fromdata_init(ctx.get()) != 1) return {};

    OSSL_PARAM params[3];
    params[0] = OSSL_PARAM_construct_BN("n",
        const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(n_bytes->data())),
        n_bytes->size());
    params[1] = OSSL_PARAM_construct_BN("e",
        const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(e_bytes->data())),
        e_bytes->size());
    params[2] = OSSL_PARAM_construct_end();

    EVP_PKEY* pkey = nullptr;
    if (EVP_PKEY_fromdata(ctx.get(), &pkey, EVP_PKEY_PUBLIC_KEY, params) != 1) {
        return {};
    }
    pkey_ptr key(pkey);

    return pkey_to_pem(key.get());
}

/// Convert EC JWK (x, y, crv) to PEM public key
std::string ec_jwk_to_pem(const std::string& x_b64, const std::string& y_b64,
                           const std::string& crv) {
    auto x_bytes = base64url_decode(x_b64);
    auto y_bytes = base64url_decode(y_b64);
    if (!x_bytes || !y_bytes) return {};

    // Only P-256 (ES256) supported
    if (crv != "P-256") return {};

    // Build uncompressed point: 0x04 || x || y
    std::string point;
    point.reserve(1 + x_bytes->size() + y_bytes->size());
    point += '\x04';
    point += *x_bytes;
    point += *y_bytes;

    // Create EC key using EVP_PKEY_fromdata (OpenSSL 3.0+)
    pkey_ctx_ptr ctx(EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr));
    if (!ctx) return {};

    if (EVP_PKEY_fromdata_init(ctx.get()) != 1) return {};

    const char* group_name = "P-256";
    OSSL_PARAM params[3];
    params[0] = OSSL_PARAM_construct_utf8_string("group",
        const_cast<char*>(group_name), 0);
    params[1] = OSSL_PARAM_construct_octet_string("pub",
        const_cast<char*>(point.data()), point.size());
    params[2] = OSSL_PARAM_construct_end();

    EVP_PKEY* pkey = nullptr;
    if (EVP_PKEY_fromdata(ctx.get(), &pkey, EVP_PKEY_PUBLIC_KEY, params) != 1) {
        return {};
    }
    pkey_ptr key(pkey);

    return pkey_to_pem(key.get());
}

/// Parse a single JWK object and convert to jwk_key
std::optional<jwk_key> parse_jwk(const crow::json::rvalue& jwk) {
    jwk_key key;

    if (jwk.has("kid") && jwk["kid"].t() == crow::json::type::String) {
        key.kid = std::string(jwk["kid"].s());
    }
    if (jwk.has("kty") && jwk["kty"].t() == crow::json::type::String) {
        key.kty = std::string(jwk["kty"].s());
    }
    if (jwk.has("alg") && jwk["alg"].t() == crow::json::type::String) {
        key.alg = std::string(jwk["alg"].s());
    }
    if (jwk.has("use") && jwk["use"].t() == crow::json::type::String) {
        key.use = std::string(jwk["use"].s());
    }

    // Skip non-signature keys
    if (!key.use.empty() && key.use != "sig") return std::nullopt;

    if (key.kty == "RSA") {
        if (!jwk.has("n") || !jwk.has("e")) return std::nullopt;
        auto n = std::string(jwk["n"].s());
        auto e = std::string(jwk["e"].s());
        key.pem = rsa_jwk_to_pem(n, e);
        if (key.pem.empty()) return std::nullopt;

        // Infer algorithm if not specified
        if (key.alg.empty()) key.alg = "RS256";
    } else if (key.kty == "EC") {
        if (!jwk.has("x") || !jwk.has("y")) return std::nullopt;
        auto x = std::string(jwk["x"].s());
        auto y = std::string(jwk["y"].s());
        auto crv = jwk.has("crv") ? std::string(jwk["crv"].s()) : std::string("P-256");
        key.pem = ec_jwk_to_pem(x, y, crv);
        if (key.pem.empty()) return std::nullopt;

        if (key.alg.empty()) key.alg = "ES256";
    } else {
        return std::nullopt;  // Unsupported key type
    }

    return key;
}

}  // namespace

#else  // !PACS_WITH_DIGITAL_SIGNATURES

namespace {

std::optional<jwk_key> parse_jwk(const crow::json::rvalue& /*jwk*/) {
    return std::nullopt;  // Cannot convert without OpenSSL
}

}  // namespace

#endif  // PACS_WITH_DIGITAL_SIGNATURES

// =============================================================================
// jwks_provider Implementation
// =============================================================================

jwks_provider::jwks_provider() = default;

bool jwks_provider::load_from_json(std::string_view jwks_json) {
    auto json = crow::json::load(std::string(jwks_json));
    if (!json || !json.has("keys")) return false;

    auto& keys_arr = json["keys"];
    if (keys_arr.t() != crow::json::type::List) return false;

    std::vector<jwk_key> new_keys;
    for (size_t i = 0; i < keys_arr.size(); ++i) {
        auto key = parse_jwk(keys_arr[i]);
        if (key) {
            new_keys.push_back(std::move(*key));
        }
    }

    if (new_keys.empty()) return false;

    std::lock_guard<std::mutex> lock(mutex_);
    keys_ = std::move(new_keys);
    last_fetch_ = std::chrono::steady_clock::now();
    return true;
}

void jwks_provider::set_fetcher(jwks_fetch_callback fetcher) {
    std::lock_guard<std::mutex> lock(mutex_);
    fetcher_ = std::move(fetcher);
}

void jwks_provider::set_jwks_url(const std::string& url) {
    std::lock_guard<std::mutex> lock(mutex_);
    jwks_url_ = url;
}

std::optional<jwk_key> jwks_provider::get_key(std::string_view kid) const {
    std::lock_guard<std::mutex> lock(mutex_);

    // Try refresh if cache expired and fetcher available
    if (is_cache_expired() && fetcher_ && !jwks_url_.empty()) {
        try_refresh();
    }

    for (const auto& key : keys_) {
        if (key.kid == kid) return key;
    }
    return std::nullopt;
}

std::optional<jwk_key> jwks_provider::get_key_by_alg(
    std::string_view alg) const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (is_cache_expired() && fetcher_ && !jwks_url_.empty()) {
        try_refresh();
    }

    for (const auto& key : keys_) {
        if (key.alg == alg) return key;
    }
    return std::nullopt;
}

bool jwks_provider::refresh() {
    std::lock_guard<std::mutex> lock(mutex_);
    return try_refresh();
}

const std::vector<jwk_key>& jwks_provider::keys() const {
    return keys_;
}

void jwks_provider::set_cache_ttl(std::uint32_t seconds) {
    cache_ttl_seconds_ = seconds;
}

bool jwks_provider::is_cache_expired() const {
    if (last_fetch_ == std::chrono::steady_clock::time_point{}) return true;

    auto elapsed = std::chrono::steady_clock::now() - last_fetch_;
    return elapsed > std::chrono::seconds(cache_ttl_seconds_);
}

bool jwks_provider::try_refresh() const {
    if (!fetcher_ || jwks_url_.empty()) return false;

    auto json_body = fetcher_(jwks_url_);
    if (!json_body) return false;

    auto json = crow::json::load(*json_body);
    if (!json || !json.has("keys")) return false;

    auto& keys_arr = json["keys"];
    if (keys_arr.t() != crow::json::type::List) return false;

    std::vector<jwk_key> new_keys;
    for (size_t i = 0; i < keys_arr.size(); ++i) {
        auto key = parse_jwk(keys_arr[i]);
        if (key) {
            new_keys.push_back(std::move(*key));
        }
    }

    if (new_keys.empty()) return false;

    keys_ = std::move(new_keys);
    last_fetch_ = std::chrono::steady_clock::now();
    return true;
}

}  // namespace pacs::web::auth
