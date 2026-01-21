/**
 * @file viewer_state_endpoints.hpp
 * @brief Viewer state API endpoints for REST server
 *
 * This file provides the viewer state endpoints for saving, retrieving,
 * and deleting viewer states, and tracking recent studies via REST API.
 *
 * @see Issue #545 - Implement Annotation & Measurement APIs
 * @see Issue #583 - Part 3: Key Image & Viewer State REST Endpoints
 *
 * @copyright Copyright (c) 2025
 * @license MIT
 */

#pragma once

#include <memory>

namespace pacs::storage {
class index_database;
} // namespace pacs::storage

namespace pacs::web {

struct rest_server_context;

namespace endpoints {

// Internal function - implementation in cpp file
// Registers viewer state endpoints with the Crow app
// Called from rest_server.cpp

} // namespace endpoints

} // namespace pacs::web
