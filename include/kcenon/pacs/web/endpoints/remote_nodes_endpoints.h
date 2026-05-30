// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file remote_nodes_endpoints.h
 * @brief Remote PACS node management REST API endpoints
 *
 * This file provides the remote nodes endpoints for listing, creating,
 * updating, deleting, and querying remote PACS nodes via REST API.
 *
 * @see Issue #536 - Implement Remote Node REST API Endpoints
 * @copyright Copyright (c) 2025
 * @license MIT
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <memory>

namespace kcenon::pacs::web {

struct rest_server_context;

namespace endpoints {

// Internal function - implementation in cpp file
// Registers remote node endpoints with the Crow app
// Called from rest_server.cpp

} // namespace endpoints

} // namespace kcenon::pacs::web
