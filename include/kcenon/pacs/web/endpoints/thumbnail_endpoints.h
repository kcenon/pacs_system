// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file thumbnail_endpoints.h
 * @brief Thumbnail REST API endpoints
 *
 * Provides REST endpoints for thumbnail generation and retrieval.
 *
 * @see Issue #543 - Implement Thumbnail API for DICOM Viewer
 * @copyright Copyright (c) 2025
 * @license MIT
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <memory>

namespace kcenon::pacs::storage {
class index_database;
}  // namespace kcenon::pacs::storage

namespace kcenon::pacs::web {

struct rest_server_context;
class thumbnail_service;

namespace endpoints {

// Internal function - implementation in cpp file
// Registers thumbnail endpoints with the Crow app
// Called from rest_server.cpp

}  // namespace endpoints

}  // namespace kcenon::pacs::web
