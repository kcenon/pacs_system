/**
 * @file remote_nodes_endpoints.hpp
 * @brief Remote PACS node management REST API endpoints
 *
 * This file provides the remote nodes endpoints for listing, creating,
 * updating, deleting, and querying remote PACS nodes via REST API.
 *
 * @see Issue #536 - Implement Remote Node REST API Endpoints
 * @copyright Copyright (c) 2025
 * @license MIT
 */

#pragma once

#include <memory>

namespace pacs::web {

struct rest_server_context;

namespace endpoints {

// Internal function - implementation in cpp file
// Registers remote node endpoints with the Crow app
// Called from rest_server.cpp

} // namespace endpoints

} // namespace pacs::web
