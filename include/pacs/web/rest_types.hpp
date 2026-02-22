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
 * @file rest_types.hpp
 * @brief Common types and utilities for REST API
 *
 * This file provides common types, JSON utilities, and error response
 * helpers for the REST API server.
 *
 * @copyright Copyright (c) 2025
 * @license MIT
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace pacs::web {

/**
 * @enum http_status
 * @brief Common HTTP status codes
 */
enum class http_status : std::uint16_t {
  // Success
  ok = 200,
  created = 201,
  accepted = 202,
  no_content = 204,

  // Client errors
  bad_request = 400,
  unauthorized = 401,
  forbidden = 403,
  not_found = 404,
  method_not_allowed = 405,
  conflict = 409,
  unprocessable_entity = 422,

  // Server errors
  internal_server_error = 500,
  not_implemented = 501,
  service_unavailable = 503
};

/**
 * @struct api_error
 * @brief Standard API error structure
 */
struct api_error {
  std::string code;
  std::string message;
  std::string details;
};

/**
 * @brief Create JSON error response body
 * @param error The API error
 * @return JSON string
 */
[[nodiscard]] inline std::string to_json(const api_error &error) {
  std::string json = R"({"error":{"code":")" + error.code + R"(","message":")" +
                     error.message + R"("})";
  return json;
}

/**
 * @brief Create JSON error response body with details
 * @param code Error code
 * @param message Error message
 * @return JSON string
 */
[[nodiscard]] inline std::string make_error_json(std::string_view code,
                                                 std::string_view message) {
  return std::string(R"({"error":{"code":")") + std::string(code) +
         R"(","message":")" + std::string(message) + R"("}})";
}

/**
 * @brief Create success response with optional message
 * @param message Optional message
 * @return JSON string
 */
[[nodiscard]] inline std::string
make_success_json(std::string_view message = "OK") {
  return std::string(R"({"status":"success","message":")") +
         std::string(message) + R"("})";
}

/**
 * @brief Escape a string for JSON
 * @param s Input string
 * @return JSON-escaped string
 */
[[nodiscard]] inline std::string json_escape(std::string_view s) {
  std::string result;
  result.reserve(s.size() + 10);
  for (char c : s) {
    switch (c) {
    case '"':
      result += "\\\"";
      break;
    case '\\':
      result += "\\\\";
      break;
    case '\b':
      result += "\\b";
      break;
    case '\f':
      result += "\\f";
      break;
    case '\n':
      result += "\\n";
      break;
    case '\r':
      result += "\\r";
      break;
    case '\t':
      result += "\\t";
      break;
    default:
      result += c;
      break;
    }
  }
  return result;
}

} // namespace pacs::web
