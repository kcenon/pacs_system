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

namespace pacs::web::auth {

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

}  // namespace pacs::web::auth
