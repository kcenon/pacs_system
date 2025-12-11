/**
 * @file audit_endpoints.hpp
 * @brief Audit log API endpoints for REST server
 *
 * This file provides the audit log endpoints for listing and searching
 * audit log entries via REST API.
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
// Registers audit endpoints with the Crow app
// Called from rest_server.cpp

} // namespace endpoints

} // namespace pacs::web
