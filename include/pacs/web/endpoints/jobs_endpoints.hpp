/**
 * @file jobs_endpoints.hpp
 * @brief Job management REST API and WebSocket endpoints
 *
 * This file provides the jobs endpoints for listing, creating,
 * deleting, and controlling async DICOM jobs via REST API.
 * Also provides WebSocket endpoints for real-time progress streaming.
 *
 * @see Issue #538 - Implement Job REST API & WebSocket Progress Streaming
 * @see Issue #558 - Part 1: Jobs REST API Endpoints (CRUD)
 * @see Issue #559 - Part 2: Jobs REST API Control Endpoints
 * @see Issue #560 - Part 3: Jobs WebSocket Progress Streaming
 * @copyright Copyright (c) 2025
 * @license MIT
 */

#pragma once

#include <memory>

namespace pacs::web {

struct rest_server_context;

namespace endpoints {

// Internal function - implementation in cpp file
// Registers jobs endpoints with the Crow app (REST + WebSocket)
// Called from rest_server.cpp

} // namespace endpoints

} // namespace pacs::web
