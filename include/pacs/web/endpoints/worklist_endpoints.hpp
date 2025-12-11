/**
 * @file worklist_endpoints.hpp
 * @brief Worklist API endpoints for REST server
 *
 * This file provides the worklist endpoints for listing, creating,
 * updating, and deleting worklist items via REST API.
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
// Registers worklist endpoints with the Crow app
// Called from rest_server.cpp

} // namespace endpoints

} // namespace pacs::web
