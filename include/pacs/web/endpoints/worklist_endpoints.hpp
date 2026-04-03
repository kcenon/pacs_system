// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file worklist_endpoints.hpp
 * @brief Worklist API endpoints for REST server
 *
 * This file provides the worklist endpoints for listing, creating,
 * updating, and deleting worklist items via REST API.
 *
 * @copyright Copyright (c) 2025
 * @license MIT
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <memory>

namespace kcenon::pacs::storage {
class index_database;
} // namespace kcenon::pacs::storage

namespace kcenon::pacs::web {

struct rest_server_context;

namespace endpoints {

// Internal function - implementation in cpp file
// Registers worklist endpoints with the Crow app
// Called from rest_server.cpp

} // namespace endpoints

} // namespace kcenon::pacs::web
