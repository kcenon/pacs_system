// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file oauth2_config.hpp
 * @brief OAuth 2.0 configuration for DICOMweb endpoints
 *
 * @see RFC 6749 - OAuth 2.0 Authorization Framework
 * @see Issue #860 - Add OAuth 2.0 configuration and JWT token validation core
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace kcenon::pacs::web::auth {

/**
 * @brief OAuth 2.0 configuration for DICOMweb authorization
 *
 * When enabled, all DICOMweb endpoints require a valid JWT Bearer token
 * in the Authorization header. When disabled, the system falls back to
 * the existing X-User-ID header-based authentication.
 */
struct oauth2_config {
    /// Enable OAuth 2.0 authorization (disabled by default for backward compat)
    bool enabled = false;

    /// Expected token issuer (iss claim). Empty = skip issuer validation
    std::string issuer;

    /// Expected audience (aud claim). Empty = skip audience validation
    std::string audience;

    /// JWKS endpoint URL for public key retrieval
    std::string jwks_url;

    /// Allowed clock skew in seconds for exp/nbf validation
    std::uint32_t clock_skew_seconds = 60;

    /// Allowed signing algorithms (default: RS256, ES256)
    std::vector<std::string> allowed_algorithms = {"RS256", "ES256"};

    /// Allow unknown OAuth users not found in RBAC to access as Viewer
    /// When false (default): unknown users receive 401 Unauthorized
    /// When true: unknown users are granted Role::Viewer (backward compatibility)
    bool allow_unknown_users = false;
};

}  // namespace kcenon::pacs::web::auth
