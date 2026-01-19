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

namespace pacs::storage {
class index_database;
} // namespace pacs::storage

namespace pacs::client {
class remote_node_manager;
class job_manager;
class routing_manager;
} // namespace pacs::client

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

  /// Index database for patient/study/series data access
  std::shared_ptr<storage::index_database> database;

  /// Remote node manager for remote PACS node management
  std::shared_ptr<client::remote_node_manager> node_manager;

  /// Job manager for async DICOM operations
  std::shared_ptr<client::job_manager> job_manager;

  /// Routing manager for auto-forwarding rules
  std::shared_ptr<client::routing_manager> routing_manager;
};

namespace endpoints {

// Internal function - implementation in cpp file
// Registers system endpoints with the Crow app
// Called from rest_server.cpp

} // namespace endpoints

} // namespace pacs::web
