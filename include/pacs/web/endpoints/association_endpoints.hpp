/**
 * @file association_endpoints.hpp
 * @brief DICOM Association API endpoints for REST server
 *
 * This file provides endpoints for monitoring and managing active
 * DICOM associations via REST API.
 *
 * @copyright Copyright (c) 2025
 * @license MIT
 */

#pragma once

#include <memory>

namespace pacs::web {

struct rest_server_context;

namespace endpoints {

// Internal function - implementation in cpp file
// Registers association endpoints with the Crow app
// Called from rest_server.cpp

} // namespace endpoints

} // namespace pacs::web
