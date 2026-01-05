// BSD 3-Clause License
// Copyright (c) 2025, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file pacs-web.cppm
 * @brief C++20 module partition for REST API and DICOMweb.
 *
 * This module partition exports web-related types:
 * - rest_server: HTTP/REST server
 * - rest_types: Request/Response types
 * - Endpoint handlers for DICOMweb and custom APIs
 *
 * Part of the kcenon.pacs module.
 */

module;

// Standard library imports
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// PACS web headers
#include <pacs/web/rest_server.hpp>
#include <pacs/web/rest_types.hpp>

export module kcenon.pacs:web;

// ============================================================================
// Re-export pacs::web namespace
// ============================================================================

export namespace pacs::web {

// REST server
using pacs::web::rest_server;
using pacs::web::rest_config;

// REST types
using pacs::web::rest_request;
using pacs::web::rest_response;

} // namespace pacs::web
