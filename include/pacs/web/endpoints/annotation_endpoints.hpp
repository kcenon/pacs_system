/**
 * @file annotation_endpoints.hpp
 * @brief Annotation API endpoints for REST server
 *
 * This file provides the annotation endpoints for creating, retrieving,
 * updating, and deleting annotations on DICOM images via REST API.
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
// Registers annotation endpoints with the Crow app
// Called from rest_server.cpp

} // namespace endpoints

} // namespace pacs::web
