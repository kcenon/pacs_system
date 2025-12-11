/**
 * @file patient_endpoints.hpp
 * @brief Patient API endpoints for REST server
 *
 * This file provides the patient endpoints for listing, searching,
 * and retrieving patient records via REST API.
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
// Registers patient endpoints with the Crow app
// Called from rest_server.cpp

} // namespace endpoints

} // namespace pacs::web
