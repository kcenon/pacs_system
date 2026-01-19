/**
 * @file routing_endpoints.hpp
 * @brief Routing rule management REST API endpoints
 *
 * This file provides the routing endpoints for listing, creating,
 * updating, deleting, and reordering routing rules via REST API.
 *
 * @see Issue #570 - Implement Routing Rules CRUD REST API
 * @see Issue #540 - Parent: Routing REST API & Storage SCP Integration
 * @copyright Copyright (c) 2025
 * @license MIT
 */

#pragma once

#include <memory>

namespace pacs::web {

struct rest_server_context;

namespace endpoints {

// Internal function - implementation in cpp file
// Registers routing endpoints with the Crow app
// Called from rest_server.cpp

} // namespace endpoints

} // namespace pacs::web
