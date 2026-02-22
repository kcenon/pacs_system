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
 * @file rest_config.hpp
 * @brief Configuration for REST API server
 *
 * This file provides configuration options for the REST API server
 * including bind address, port, concurrency, CORS, and TLS settings.
 *
 * @copyright Copyright (c) 2025
 * @license MIT
 */

#pragma once

#include <cstdint>
#include <string>

namespace pacs::web {

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

  /// CORS allowed origins (empty = allow all)
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
};

} // namespace pacs::web
