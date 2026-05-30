// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file rest_server.h
 * @brief REST API server for PACS administration
 *
 * This file provides the rest_server class that implements a REST API
 * server using the Crow framework for PACS system administration and
 * monitoring.
 *
 * @copyright Copyright (c) 2025
 * @license MIT
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include "rest_config.h"

#include <cstdint>
#include <memory>

// Forward declarations for PACS monitoring
namespace kcenon::pacs::monitoring {
class health_checker;
class pacs_metrics;
} // namespace kcenon::pacs::monitoring

namespace kcenon::pacs::security {
class access_control_manager;
} // namespace kcenon::pacs::security

namespace kcenon::pacs::web::auth {
struct oauth2_config;
class oauth2_middleware;
class jwks_provider;
} // namespace kcenon::pacs::web::auth

namespace kcenon::pacs::storage {
class index_database;
class file_storage;
} // namespace kcenon::pacs::storage

namespace kcenon::pacs::network {
class dicom_server;
} // namespace kcenon::pacs::network

namespace kcenon::pacs::client {
class remote_node_manager;
class job_manager;
class routing_manager;
} // namespace kcenon::pacs::client

namespace kcenon::pacs::web {

/**
 * @class rest_server
 * @brief REST API server for PACS administration and monitoring
 *
 * The rest_server class provides a REST API for:
 * - System health status and metrics
 * - Configuration management
 * - Future: Patient/Study browsing, Worklist management
 *
 * @par Example
 * @code
 * #include <kcenon/pacs/web/rest_server.h>
 *
 * rest_server_config config;
 * config.port = 8080;
 * config.concurrency = 4;
 *
 * rest_server server(config);
 * server.set_health_checker(health_checker_instance);
 * server.set_metrics_provider(metrics_instance);
 *
 * server.start_async();  // Non-blocking
 * // ... do other work ...
 * server.stop();
 * @endcode
 */
class rest_server {
public:
  /**
   * @brief Construct REST server with default configuration
   */
  rest_server();

  /**
   * @brief Construct REST server with custom configuration
   * @param config Server configuration
   */
  explicit rest_server(const rest_server_config &config);

  /**
   * @brief Destructor - stops server if running
   */
  ~rest_server();

  /// Non-copyable
  rest_server(const rest_server &) = delete;
  rest_server &operator=(const rest_server &) = delete;

  /// Movable
  rest_server(rest_server &&other) noexcept;
  rest_server &operator=(rest_server &&other) noexcept;

  // =========================================================================
  // Configuration
  // =========================================================================

  /**
   * @brief Get current configuration
   * @return Current server configuration
   */
  [[nodiscard]] const rest_server_config &config() const noexcept;

  /**
   * @brief Update configuration (requires restart to apply)
   * @param config New configuration
   */
  void set_config(const rest_server_config &config);

  // =========================================================================
  // Integration
  // =========================================================================

  /**
   * @brief Set health checker for /api/v1/system/status endpoint
   * @param checker Health checker instance
   */
  void set_health_checker(std::shared_ptr<monitoring::health_checker> checker);

  /**
   * @brief Set metrics provider for /api/v1/system/metrics endpoint
   * @param metrics Metrics instance
   */
  void set_metrics_provider(std::shared_ptr<monitoring::pacs_metrics> metrics);

  /**
   * @brief Set access control manager for security
   * @param manager Access control manager instance
   */
  void set_access_control_manager(
      std::shared_ptr<security::access_control_manager> manager);

  /**
   * @brief Set index database for patient/study/series endpoints
   * @param database Index database instance
   */
  void set_database(std::shared_ptr<storage::index_database> database);

  /**
   * @brief Set file storage for DICOM instance persistence (STOW-RS)
   * @param storage File storage instance
   */
  void set_file_storage(std::shared_ptr<storage::file_storage> storage);

  /**
   * @brief Set remote node manager for remote PACS node management
   * @param manager Remote node manager instance
   */
  void set_node_manager(std::shared_ptr<client::remote_node_manager> manager);

  /**
   * @brief Set job manager for async DICOM operations
   * @param manager Job manager instance
   */
  void set_job_manager(std::shared_ptr<client::job_manager> manager);

  /**
   * @brief Set routing manager for auto-forwarding rules
   * @param manager Routing manager instance
   */
  void set_routing_manager(std::shared_ptr<client::routing_manager> manager);

  /**
   * @brief Set DICOM server for live association management
   * @param server DICOM server instance
   */
  void set_dicom_server(std::shared_ptr<network::dicom_server> server);

  /**
   * @brief Set OAuth 2.0 middleware for DICOMweb endpoint authorization
   * @param middleware OAuth 2.0 middleware instance
   */
  void set_oauth2_middleware(
      std::shared_ptr<auth::oauth2_middleware> middleware);

  // =========================================================================
  // Lifecycle
  // =========================================================================

  /**
   * @brief Start the server (blocking)
   *
   * This method blocks until stop() is called from another thread.
   */
  void start();

  /**
   * @brief Start the server (non-blocking)
   *
   * Starts the server in a background thread and returns immediately.
   */
  void start_async();

  /**
   * @brief Stop the server
   *
   * Gracefully shuts down the server. Safe to call multiple times.
   */
  void stop();

  /**
   * @brief Check if server is currently running
   * @return true if server is running
   */
  [[nodiscard]] bool is_running() const noexcept;

  /**
   * @brief Wait for server to stop
   *
   * Blocks until the server has completely stopped.
   * Only valid after start_async() was called.
   */
  void wait();

  /**
   * @brief Get the actual port the server is listening on
   * @return Port number, or 0 if not running
   */
  [[nodiscard]] std::uint16_t port() const noexcept;

private:
  struct impl;
  std::unique_ptr<impl> impl_;
};

} // namespace kcenon::pacs::web
