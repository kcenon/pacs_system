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
 * @file oauth2_middleware.hpp
 * @brief OAuth 2.0 middleware for DICOMweb endpoint authorization
 *
 * Provides Bearer token extraction, JWT validation, scope-based access
 * control, and integration with the existing RBAC system. When OAuth 2.0
 * is disabled, falls back to X-User-ID header authentication.
 *
 * @see RFC 6749 - OAuth 2.0 Authorization Framework
 * @see Issue #852 - Implement OAuth 2.0 authorization for DICOMweb endpoints
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include "pacs/web/auth/jwks_provider.hpp"
#include "pacs/web/auth/jwt_validator.hpp"
#include "pacs/web/auth/oauth2_config.hpp"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Forward declarations
namespace crow {
struct request;
struct response;
}  // namespace crow

namespace pacs::security {
class access_control_manager;
}  // namespace pacs::security

// user_context must be fully defined for std::optional<user_context>
#include "pacs/security/user_context.hpp"

namespace pacs::web::auth {

// =============================================================================
// DICOMweb OAuth Scopes
// =============================================================================

/**
 * @brief Standard OAuth 2.0 scopes for DICOMweb operations
 */
namespace dicomweb_scopes {
constexpr std::string_view read = "dicomweb.read";
constexpr std::string_view search = "dicomweb.search";
constexpr std::string_view write = "dicomweb.write";
constexpr std::string_view delete_resource = "dicomweb.delete";
}  // namespace dicomweb_scopes

// =============================================================================
// OAuth 2.0 Middleware
// =============================================================================

/**
 * @brief OAuth 2.0 middleware for DICOMweb endpoint authorization
 *
 * When OAuth 2.0 is enabled, the middleware:
 * 1. Extracts the Bearer token from the Authorization header
 * 2. Decodes and validates the JWT (issuer, audience, expiration)
 * 3. Verifies the token signature using JWKS keys
 * 4. Checks required scopes for the DICOMweb operation
 * 5. Creates a user_context for the downstream endpoint
 *
 * When OAuth 2.0 is disabled, the middleware is a no-op and the system
 * falls back to the existing X-User-ID header-based authentication.
 *
 * @example
 * @code
 * oauth2_config config;
 * config.enabled = true;
 * config.issuer = "https://auth.example.com";
 *
 * oauth2_middleware middleware(config);
 * middleware.set_jwks_provider(jwks);
 *
 * // In endpoint handler:
 * auto ctx = middleware.authenticate(req, res);
 * if (!ctx) return res;  // Error response already set
 *
 * if (!middleware.require_scope(req, res, dicomweb_scopes::read)) {
 *     return res;  // 403 Forbidden
 * }
 * @endcode
 */
class oauth2_middleware {
public:
    /**
     * @brief Construct middleware with OAuth 2.0 configuration
     * @param config The OAuth 2.0 configuration
     */
    explicit oauth2_middleware(const oauth2_config& config);

    /**
     * @brief Set the JWKS provider for signature verification
     * @param provider Shared pointer to JWKS provider
     */
    void set_jwks_provider(std::shared_ptr<jwks_provider> provider);

    /**
     * @brief Set the access control manager for RBAC integration
     * @param manager Shared pointer to access control manager
     */
    void set_access_control_manager(
        std::shared_ptr<security::access_control_manager> manager);

    /**
     * @brief Authenticate a request using OAuth 2.0 Bearer token
     *
     * Extracts the Bearer token, validates JWT claims, verifies the
     * signature, and creates a user_context. On failure, sets an
     * appropriate error response (401 or 403).
     *
     * @param req The HTTP request
     * @param res The HTTP response (set on error)
     * @return user_context on success, std::nullopt on failure
     */
    [[nodiscard]] std::optional<security::user_context> authenticate(
        const crow::request& req, crow::response& res) const;

    /**
     * @brief Check if the authenticated request has a required scope
     *
     * Must be called after authenticate(). Reads the cached token claims
     * from the request. Returns false and sets 403 response if the
     * required scope is missing.
     *
     * @param claims JWT claims from a previously validated token
     * @param res The HTTP response (set on failure)
     * @param required_scope The required OAuth scope
     * @return true if the scope is present
     */
    [[nodiscard]] bool require_scope(
        const jwt_claims& claims,
        crow::response& res,
        std::string_view required_scope) const;

    /**
     * @brief Check if the request has any of the required scopes
     *
     * @param claims JWT claims from a previously validated token
     * @param res The HTTP response (set on failure)
     * @param required_scopes List of acceptable scopes
     * @return true if at least one scope is present
     */
    [[nodiscard]] bool require_any_scope(
        const jwt_claims& claims,
        crow::response& res,
        const std::vector<std::string>& required_scopes) const;

    /**
     * @brief Check if OAuth 2.0 is enabled
     */
    [[nodiscard]] bool enabled() const noexcept;

    /**
     * @brief Get the underlying JWT validator
     */
    [[nodiscard]] const jwt_validator& validator() const noexcept;

    /**
     * @brief Get the last validated claims (for scope checking after authenticate)
     */
    [[nodiscard]] const jwt_claims& last_claims() const noexcept;

private:
    oauth2_config config_;
    jwt_validator validator_;
    std::shared_ptr<jwks_provider> jwks_provider_;
    std::shared_ptr<security::access_control_manager> security_manager_;
    mutable jwt_claims last_claims_;

    /// Extract Bearer token from Authorization header
    [[nodiscard]] std::optional<std::string_view> extract_bearer_token(
        const crow::request& req) const;

    /// Verify token signature using JWKS keys
    [[nodiscard]] bool verify_signature(const jwt_token& token) const;

    /// Set 401 Unauthorized response
    static void set_unauthorized(crow::response& res,
                                 std::string_view message);

    /// Set 403 Forbidden response
    static void set_forbidden(crow::response& res,
                              std::string_view message);
};

}  // namespace pacs::web::auth
