/**
 * @file metadata_endpoints.hpp
 * @brief Metadata REST API endpoints
 *
 * Provides REST endpoints for selective metadata retrieval,
 * series navigation, and window/level presets.
 *
 * @see Issue #544 - Implement Selective Metadata & Navigation APIs
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
class metadata_service;

namespace endpoints {

// Internal function - implementation in cpp file
// Registers metadata endpoints with the Crow app
// Called from rest_server.cpp

}  // namespace endpoints

}  // namespace pacs::web
