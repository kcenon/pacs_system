// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file rest_config.h
 * @brief Configuration for REST API server
 *
 * This file provides configuration options for the REST API server
 * including bind address, port, concurrency, CORS, and TLS settings.
 *
 * @copyright Copyright (c) 2025
 * @license MIT
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <cstdint>
#include <string>

namespace kcenon::pacs::web {

/**
 * @struct rest_server_config
 * @brief Configuration options for the REST server
 */
struct rest_server_config {
  /// Address to bind the server to
  std::string bind_address{"0.0.0.0"};

  /// Port to listen on
  std::uint16_t port{8080};

  /// Number of worker threads for handling requests
  std::size_t concurrency{4};

  /// Enable CORS (Cross-Origin Resource Sharing) headers
  bool enable_cors{true};

  /// CORS allowed origins (empty = no Access-Control-Allow-Origin header, so no other origin may read responses)
  std::string cors_allowed_origins{"*"};

  /// Enable TLS/SSL encryption (future feature)
  bool enable_tls{false};

  /// Path to TLS certificate file
  std::string tls_cert_path;

  /// Path to TLS private key file
  std::string tls_key_path;

  /// Request timeout in seconds
  std::uint32_t request_timeout_seconds{30};

  /// Maximum request body size in bytes (default 10MB)
  std::size_t max_body_size{10 * 1024 * 1024};

  /// Serve the REST API under /api. With false, start_async() serves only DICOMweb and WADO-URI.
  /// Most /api routes check no credentials, so a host that needs authorization turns this off.
  bool enable_rest_api{true};
};

} // namespace kcenon::pacs::web
