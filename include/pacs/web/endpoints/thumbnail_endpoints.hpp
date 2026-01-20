/**
 * @file thumbnail_endpoints.hpp
 * @brief Thumbnail REST API endpoints
 *
 * Provides REST endpoints for thumbnail generation and retrieval.
 *
 * @see Issue #543 - Implement Thumbnail API for DICOM Viewer
 * @copyright Copyright (c) 2025
 * @license MIT
 */

#pragma once

#include <memory>

namespace pacs::storage {
class index_database;
}  // namespace pacs::storage

namespace pacs::web {

struct rest_server_context;
class thumbnail_service;

namespace endpoints {

// Internal function - implementation in cpp file
// Registers thumbnail endpoints with the Crow app
// Called from rest_server.cpp

}  // namespace endpoints

}  // namespace pacs::web
