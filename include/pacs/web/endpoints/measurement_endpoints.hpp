/**
 * @file measurement_endpoints.hpp
 * @brief Measurement API endpoints for REST server
 *
 * This file provides the measurement endpoints for creating, retrieving,
 * and deleting measurements on DICOM images via REST API.
 *
 * @see Issue #545 - Implement Annotation & Measurement APIs
 * @see Issue #582 - Part 2: Annotation & Measurement REST Endpoints
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
// Registers measurement endpoints with the Crow app
// Called from rest_server.cpp

} // namespace endpoints

} // namespace pacs::web
