// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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

namespace pacs::services::monitoring {
class database_metrics_service;
} // namespace pacs::services::monitoring

namespace pacs::security {
class access_control_manager;
} // namespace pacs::security

namespace pacs::storage {
class index_database;
class file_storage;
} // namespace pacs::storage

namespace pacs::network {
class dicom_server;
} // namespace pacs::network

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

  /// File storage for DICOM instance persistence
  std::shared_ptr<storage::file_storage> file_storage;

  /// Remote node manager for remote PACS node management
  std::shared_ptr<client::remote_node_manager> node_manager;

  /// Job manager for async DICOM operations
  std::shared_ptr<client::job_manager> job_manager;

  /// Routing manager for auto-forwarding rules
  std::shared_ptr<client::routing_manager> routing_manager;

  /// DICOM server for live association management
  std::shared_ptr<network::dicom_server> dicom_server;

  /// Database metrics service for monitoring
  std::shared_ptr<services::monitoring::database_metrics_service> database_metrics;
};

namespace endpoints {

// Internal function - implementation in cpp file
// Registers system endpoints with the Crow app
// Called from rest_server.cpp

} // namespace endpoints

} // namespace pacs::web
