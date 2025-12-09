/**
 * @file rest_types.hpp
 * @brief Common types and utilities for REST API
 *
 * This file provides common types, JSON utilities, and error response
 * helpers for the REST API server.
 *
 * @copyright Copyright (c) 2025
 * @license MIT
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
