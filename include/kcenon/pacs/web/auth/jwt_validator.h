// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file jwt_validator.h
 * @brief JWT (JSON Web Token) validation for OAuth 2.0
 *
 * Provides JWT parsing, claim validation, and cryptographic signature
 * verification using OpenSSL. Supports RS256 and ES256 algorithms.
 *
 * @see RFC 7519 - JSON Web Token (JWT)
 * @see RFC 7518 - JSON Web Algorithms (JWA)
 * @see Issue #860 - Add OAuth 2.0 configuration and JWT token validation core
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include "kcenon/pacs/web/auth/oauth2_config.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace kcenon::pacs::web::auth {

// =============================================================================
// JWT Token Structures
// =============================================================================

/**
 * @brief Decoded JWT header (JOSE header)
 */
struct jwt_header {
    std::string alg;    ///< Algorithm (RS256, ES256)
    std::string typ;    ///< Token type (JWT)
    std::string kid;    ///< Key ID for key selection
};

/**
 * @brief Decoded JWT claims (payload)
 */
struct jwt_claims {
    std::string iss;                    ///< Issuer
    std::string sub;                    ///< Subject (user identifier)
    std::string aud;                    ///< Audience
    std::int64_t exp = 0;              ///< Expiration time (Unix timestamp)
    std::int64_t iat = 0;              ///< Issued at (Unix timestamp)
    std::int64_t nbf = 0;              ///< Not before (Unix timestamp)
    std::string jti;                    ///< JWT ID
    std::vector<std::string> scopes;    ///< OAuth 2.0 scopes (from "scope" claim)
};

/**
 * @brief Decoded JWT token with raw segments for signature verification
 */
struct jwt_token {
    jwt_header header;
    jwt_claims claims;
    std::string header_payload;     ///< "header.payload" for signature input
    std::string signature_bytes;    ///< Decoded signature bytes
};

// =============================================================================
// JWT Error Types
// =============================================================================

/**
 * @brief JWT validation error codes
 */
enum class jwt_error {
    none,                       ///< No error
    malformed_token,            ///< Token doesn't have 3 dot-separated parts
    invalid_base64,             ///< Base64url decoding failed
    invalid_json,               ///< JSON parsing failed
    unsupported_algorithm,      ///< Algorithm not in allowed list
    invalid_signature,          ///< Signature verification failed
    token_expired,              ///< Token has expired (exp claim)
    token_not_yet_valid,        ///< Token not yet valid (nbf claim)
    invalid_issuer,             ///< Issuer doesn't match expected value
    invalid_audience,           ///< Audience doesn't match expected value
    missing_required_claim,     ///< Required claim is missing
    signature_not_available     ///< OpenSSL not available for verification
};

/**
 * @brief Get human-readable description for a JWT error
 * @param error The error code
 * @return Error description string
 */
[[nodiscard]] std::string_view jwt_error_message(jwt_error error) noexcept;

// =============================================================================
// JWT Validator
// =============================================================================

/**
 * @brief JWT token validator for OAuth 2.0 Bearer tokens
 *
 * Provides JWT decoding, claim validation, and signature verification.
 * Signature verification requires OpenSSL (PACS_WITH_DIGITAL_SIGNATURES).
 *
 * @par Example:
 * @code
 * oauth2_config config;
 * config.issuer = "https://auth.example.com";
 * config.audience = "pacs-api";
 *
 * jwt_validator validator(config);
 *
 * auto [token, decode_err] = validator.decode(bearer_token);
 * if (decode_err != jwt_error::none) { ... }
 *
 * auto claims_err = validator.validate_claims(token.claims);
 * if (claims_err != jwt_error::none) { ... }
 *
 * if (!validator.verify_rs256(token, public_key_pem)) { ... }
 * @endcode
 */
class jwt_validator {
public:
    /**
     * @brief Construct validator with OAuth 2.0 configuration
     * @param config The OAuth 2.0 configuration
     */
    explicit jwt_validator(const oauth2_config& config);

    /**
     * @brief Decode a JWT token string into its components
     *
     * Splits the token, decodes Base64url segments, and parses JSON.
     * Does NOT verify the signature.
     *
     * @param token_string The raw JWT string (header.payload.signature)
     * @return Pair of decoded token and error code
     */
    [[nodiscard]] std::pair<jwt_token, jwt_error> decode(
        std::string_view token_string) const;

    /**
     * @brief Validate JWT claims against configuration
     *
     * Checks issuer, audience, expiration, and not-before claims.
     *
     * @param claims The decoded claims to validate
     * @return jwt_error::none if all claims are valid
     */
    [[nodiscard]] jwt_error validate_claims(
        const jwt_claims& claims) const;

    /**
     * @brief Verify RS256 (RSA-SHA256) signature
     * @param token The decoded token with signature data
     * @param public_key_pem RSA public key in PEM format
     * @return true if signature is valid
     */
    [[nodiscard]] bool verify_rs256(
        const jwt_token& token,
        std::string_view public_key_pem) const;

    /**
     * @brief Verify ES256 (ECDSA-SHA256) signature
     * @param token The decoded token with signature data
     * @param public_key_pem EC public key in PEM format
     * @return true if signature is valid
     */
    [[nodiscard]] bool verify_es256(
        const jwt_token& token,
        std::string_view public_key_pem) const;

    /**
     * @brief Check if token has a specific scope
     * @param claims The token claims
     * @param scope The required scope
     * @return true if the scope is present
     */
    [[nodiscard]] static bool has_scope(
        const jwt_claims& claims,
        std::string_view scope) noexcept;

    /**
     * @brief Check if token has any of the specified scopes
     * @param claims The token claims
     * @param scopes List of acceptable scopes
     * @return true if at least one scope matches
     */
    [[nodiscard]] static bool has_any_scope(
        const jwt_claims& claims,
        const std::vector<std::string>& scopes) noexcept;

    /**
     * @brief Get the OAuth 2.0 configuration
     */
    [[nodiscard]] const oauth2_config& config() const noexcept;

private:
    oauth2_config config_;
};

// =============================================================================
// Base64url Utilities
// =============================================================================

/**
 * @brief Decode a Base64url-encoded string (RFC 4648 Section 5)
 * @param input Base64url string (no padding required)
 * @return Decoded bytes, or empty optional on error
 */
[[nodiscard]] std::optional<std::string> base64url_decode(
    std::string_view input);

}  // namespace kcenon::pacs::web::auth
