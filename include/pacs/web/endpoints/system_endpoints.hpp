/**
 * @file system_endpoints.hpp
 * @brief System API endpoints for REST server
 *
 * This file provides the system endpoints for health status, metrics,
 * and configuration management.
 *
 * @copyright Copyright (c) 2025
 * @license MIT
 */

#pragma once

#include <memory>

namespace pacs::monitoring {
class health_checker;
class pacs_metrics;
} // namespace pacs::monitoring

namespace pacs::security {
class access_control_manager;
} // namespace pacs::security

namespace pacs::web {

struct rest_server_config;

/**
 * @struct rest_server_context
 * @brief Shared context for REST endpoints
 */
struct rest_server_context {
  /// Current server configuration (read-only)
  const rest_server_config *config{nullptr};

  /// Health checker for status endpoint
  std::shared_ptr<monitoring::health_checker> health_checker;

  /// Metrics provider for metrics endpoint
  std::shared_ptr<monitoring::pacs_metrics> metrics;

  /// Access control manager for security
  std::shared_ptr<security::access_control_manager> security_manager;
};

namespace endpoints {

// Internal function - implementation in cpp file
// Registers system endpoints with the Crow app
// Called from rest_server.cpp

} // namespace endpoints

} // namespace pacs::web
