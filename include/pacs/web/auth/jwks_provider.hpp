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
 * @file jwks_provider.hpp
 * @brief JSON Web Key Set (JWKS) provider with key caching
 *
 * Manages public keys for JWT signature verification. Supports loading
 * JWKS from JSON strings and callback-based URL fetching with TTL caching.
 *
 * @see RFC 7517 - JSON Web Key (JWK)
 * @see Issue #852 - Implement OAuth 2.0 authorization for DICOMweb endpoints
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace kcenon::pacs::web::auth {

/**
 * @brief Represents a single JSON Web Key converted to PEM format
 */
struct jwk_key {
    std::string kid;    ///< Key ID
    std::string kty;    ///< Key type (RSA, EC)
    std::string alg;    ///< Algorithm (RS256, ES256)
    std::string use;    ///< Key use (sig)
    std::string pem;    ///< PEM-encoded public key
};

/**
 * @brief Callback type for fetching JWKS JSON from a URL
 *
 * The callback receives a URL string and should return the JWKS JSON
 * response body, or std::nullopt on failure.
 */
using jwks_fetch_callback =
    std::function<std::optional<std::string>(const std::string& url)>;

/**
 * @brief JWKS provider with key caching and lazy refresh
 *
 * Manages JSON Web Key Sets for JWT signature verification. Keys can be
 * loaded statically from JSON or fetched dynamically via a callback.
 *
 * @example
 * @code
 * jwks_provider provider;
 * provider.load_from_json(R"({"keys":[...]})");
 *
 * auto key = provider.get_key("key-1");
 * if (key) {
 *     validator.verify_rs256(token, key->pem);
 * }
 * @endcode
 */
class jwks_provider {
public:
    jwks_provider();

    /**
     * @brief Load keys from a JWKS JSON string
     *
     * Parses the JWKS JSON and converts JWK keys to PEM format.
     * Replaces any previously loaded keys.
     *
     * @param jwks_json JSON string containing {"keys": [...]}
     * @return true if at least one key was successfully loaded
     */
    bool load_from_json(std::string_view jwks_json);

    /**
     * @brief Set a callback for fetching JWKS from a URL
     * @param fetcher Callback that receives URL and returns JSON body
     */
    void set_fetcher(jwks_fetch_callback fetcher);

    /**
     * @brief Set the JWKS endpoint URL for fetching
     * @param url JWKS endpoint URL
     */
    void set_jwks_url(const std::string& url);

    /**
     * @brief Get a key by Key ID (kid)
     *
     * If keys are not loaded or cache has expired, attempts to refresh
     * using the configured fetcher and JWKS URL.
     *
     * @param kid Key ID to look up
     * @return Key if found, std::nullopt otherwise
     */
    [[nodiscard]] std::optional<jwk_key> get_key(
        std::string_view kid) const;

    /**
     * @brief Get the first key matching the given algorithm
     * @param alg Algorithm name (e.g., "RS256")
     * @return Key if found, std::nullopt otherwise
     */
    [[nodiscard]] std::optional<jwk_key> get_key_by_alg(
        std::string_view alg) const;

    /**
     * @brief Force refresh keys from the configured JWKS URL
     * @return true if refresh was successful
     */
    bool refresh();

    /**
     * @brief Get all currently loaded keys
     */
    [[nodiscard]] const std::vector<jwk_key>& keys() const;

    /**
     * @brief Set cache TTL in seconds (default: 3600)
     */
    void set_cache_ttl(std::uint32_t seconds);

    /**
     * @brief Check if the cache has expired
     */
    [[nodiscard]] bool is_cache_expired() const;

private:
    mutable std::vector<jwk_key> keys_;
    jwks_fetch_callback fetcher_;
    std::string jwks_url_;
    std::uint32_t cache_ttl_seconds_{3600};
    mutable std::chrono::steady_clock::time_point last_fetch_{};
    mutable std::mutex mutex_;

    bool try_refresh() const;
};

}  // namespace kcenon::pacs::web::auth
