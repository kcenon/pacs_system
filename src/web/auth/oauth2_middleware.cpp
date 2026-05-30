// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file oauth2_middleware.cpp
 * @brief Implementation of OAuth 2.0 middleware for DICOMweb endpoints
 *
 * @see RFC 6749 - OAuth 2.0 Authorization Framework
 * @see Issue #852 - Implement OAuth 2.0 authorization for DICOMweb endpoints
 */

// IMPORTANT: Include Crow FIRST before any PACS headers
#include "crow.h"

#include "kcenon/pacs/web/auth/oauth2_middleware.h"

#include "kcenon/pacs/security/access_control_manager.h"
#include "kcenon/pacs/security/role.h"
#include "kcenon/pacs/security/user.h"
#include "kcenon/pacs/security/user_context.h"
#include "kcenon/pacs/web/rest_types.h"

#include <string>

namespace kcenon::pacs::web::auth {

// =============================================================================
// oauth2_middleware Implementation
// =============================================================================

oauth2_middleware::oauth2_middleware(const oauth2_config& config)
    : config_(config), validator_(config) {}

void oauth2_middleware::set_jwks_provider(
    std::shared_ptr<jwks_provider> provider) {
    jwks_provider_ = std::move(provider);
}

void oauth2_middleware::set_access_control_manager(
    std::shared_ptr<security::access_control_manager> manager) {
    security_manager_ = std::move(manager);
}

std::optional<auth_result> oauth2_middleware::authenticate(
    const crow::request& req, crow::response& res) const {

    // Extract Bearer token from Authorization header
    auto bearer = extract_bearer_token(req);
    if (!bearer) {
        set_unauthorized(res, "Missing or invalid Authorization header");
        return std::nullopt;
    }

    // Decode JWT
    auto [token, decode_err] = validator_.decode(*bearer);
    if (decode_err != jwt_error::none) {
        set_unauthorized(res, std::string(jwt_error_message(decode_err)));
        return std::nullopt;
    }

    // Validate claims (issuer, audience, expiration)
    auto claims_err = validator_.validate_claims(token.claims);
    if (claims_err != jwt_error::none) {
        set_unauthorized(res, std::string(jwt_error_message(claims_err)));
        return std::nullopt;
    }

    // Verify signature using JWKS
    if (!verify_signature(token)) {
        set_unauthorized(res, "Token signature verification failed");
        return std::nullopt;
    }

    // Create user_context from JWT subject
    // Try to find existing user in RBAC system, or create an OAuth-based context
    security::User user;
    user.id = token.claims.sub;
    user.username = token.claims.sub;
    user.active = true;

    // If we have an access control manager, try to look up the user
    if (security_manager_) {
        auto user_result = security_manager_->get_user(token.claims.sub);
        if (user_result.is_ok()) {
            user = user_result.unwrap();
        } else if (config_.allow_unknown_users) {
            // Backward compatibility: grant Viewer role to unknown OAuth users
            user.roles = {security::Role::Viewer};
        } else {
            set_unauthorized(res, "Unknown user: not registered in access control system");
            return std::nullopt;
        }
    } else if (config_.allow_unknown_users) {
        user.roles = {security::Role::Viewer};
    } else {
        set_unauthorized(res, "Unknown user: no access control manager configured");
        return std::nullopt;
    }

    auto ctx = security::user_context(std::move(user), token.claims.jti);
    ctx.set_source_ip(std::string(req.remote_ip_address));
    ctx.touch();

    return auth_result{std::move(ctx), std::move(token.claims)};
}

bool oauth2_middleware::require_scope(
    const jwt_claims& claims,
    crow::response& res,
    std::string_view required_scope) const {

    if (!jwt_validator::has_scope(claims, required_scope)) {
        set_forbidden(res,
            "Insufficient scope: requires " + std::string(required_scope));
        return false;
    }
    return true;
}

bool oauth2_middleware::require_any_scope(
    const jwt_claims& claims,
    crow::response& res,
    const std::vector<std::string>& required_scopes) const {

    if (!jwt_validator::has_any_scope(claims, required_scopes)) {
        std::string scope_list;
        for (size_t i = 0; i < required_scopes.size(); ++i) {
            if (i > 0) scope_list += " or ";
            scope_list += required_scopes[i];
        }
        set_forbidden(res, "Insufficient scope: requires " + scope_list);
        return false;
    }
    return true;
}

bool oauth2_middleware::enabled() const noexcept {
    return config_.enabled;
}

const jwt_validator& oauth2_middleware::validator() const noexcept {
    return validator_;
}

// =============================================================================
// Private Helpers
// =============================================================================

std::optional<std::string_view> oauth2_middleware::extract_bearer_token(
    const crow::request& req) const {

    auto auth_header = req.get_header_value("Authorization");
    if (auth_header.empty()) return std::nullopt;

    std::string_view header_view(auth_header);

    // Check for "Bearer " prefix (case-insensitive for "Bearer")
    constexpr std::string_view bearer_prefix = "Bearer ";
    if (header_view.size() <= bearer_prefix.size()) return std::nullopt;

    // Case-sensitive check per RFC 6750
    if (header_view.substr(0, bearer_prefix.size()) != bearer_prefix) {
        return std::nullopt;
    }

    auto token = header_view.substr(bearer_prefix.size());
    if (token.empty()) return std::nullopt;

    return token;
}

bool oauth2_middleware::verify_signature(const jwt_token& token) const {
    if (!jwks_provider_) return false;

    // Try to get key by kid from the token header
    std::optional<jwk_key> key;
    if (!token.header.kid.empty()) {
        key = jwks_provider_->get_key(token.header.kid);
    }

    // Fall back to algorithm-based lookup
    if (!key) {
        key = jwks_provider_->get_key_by_alg(token.header.alg);
    }

    if (!key) return false;

    // Verify based on algorithm
    if (token.header.alg == "RS256") {
        return validator_.verify_rs256(token, key->pem);
    } else if (token.header.alg == "ES256") {
        return validator_.verify_es256(token, key->pem);
    }

    return false;
}

void oauth2_middleware::set_unauthorized(crow::response& res,
                                         std::string_view message) {
    res.code = 401;
    res.add_header("Content-Type", "application/json");
    res.add_header("WWW-Authenticate", "Bearer");
    res.body = make_error_json("UNAUTHORIZED", message);
}

void oauth2_middleware::set_forbidden(crow::response& res,
                                      std::string_view message) {
    res.code = 403;
    res.add_header("Content-Type", "application/json");
    res.body = make_error_json("FORBIDDEN", message);
}

}  // namespace kcenon::pacs::web::auth
