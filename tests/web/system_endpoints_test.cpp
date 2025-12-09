/**
 * @file system_endpoints_test.cpp
 * @brief Unit tests for system API endpoints
 *
 * @copyright Copyright (c) 2025
 * @license MIT
 */

#include <catch2/catch_test_macros.hpp>

#include "pacs/web/rest_config.hpp"
#include "pacs/web/rest_types.hpp"

using namespace pacs::web;

TEST_CASE("http_status values", "[web][types]") {
  REQUIRE(static_cast<int>(http_status::ok) == 200);
  REQUIRE(static_cast<int>(http_status::created) == 201);
  REQUIRE(static_cast<int>(http_status::no_content) == 204);
  REQUIRE(static_cast<int>(http_status::bad_request) == 400);
  REQUIRE(static_cast<int>(http_status::unauthorized) == 401);
  REQUIRE(static_cast<int>(http_status::forbidden) == 403);
  REQUIRE(static_cast<int>(http_status::not_found) == 404);
  REQUIRE(static_cast<int>(http_status::internal_server_error) == 500);
}

TEST_CASE("api_error structure", "[web][types]") {
  api_error error;
  error.code = "TEST_ERROR";
  error.message = "This is a test error";
  error.details = "Additional details";

  REQUIRE(error.code == "TEST_ERROR");
  REQUIRE(error.message == "This is a test error");
  REQUIRE(error.details == "Additional details");
}

TEST_CASE("to_json for api_error", "[web][types][json]") {
  api_error error;
  error.code = "VALIDATION_ERROR";
  error.message = "Invalid input";

  auto json = to_json(error);

  REQUIRE(json.find("\"error\"") != std::string::npos);
  REQUIRE(json.find("\"code\":\"VALIDATION_ERROR\"") != std::string::npos);
  REQUIRE(json.find("\"message\":\"Invalid input\"") != std::string::npos);
}

TEST_CASE("make_error_json", "[web][types][json]") {
  auto json = make_error_json("NOT_FOUND", "Resource not found");

  REQUIRE(json.find("\"error\"") != std::string::npos);
  REQUIRE(json.find("\"code\":\"NOT_FOUND\"") != std::string::npos);
  REQUIRE(json.find("\"message\":\"Resource not found\"") != std::string::npos);
}

TEST_CASE("make_success_json", "[web][types][json]") {
  SECTION("with default message") {
    auto json = make_success_json();
    REQUIRE(json.find("\"status\":\"success\"") != std::string::npos);
    REQUIRE(json.find("\"message\":\"OK\"") != std::string::npos);
  }

  SECTION("with custom message") {
    auto json = make_success_json("Operation completed");
    REQUIRE(json.find("\"status\":\"success\"") != std::string::npos);
    REQUIRE(json.find("\"message\":\"Operation completed\"") !=
            std::string::npos);
  }
}

TEST_CASE("json_escape", "[web][types][json]") {
  SECTION("no special characters") {
    auto result = json_escape("hello world");
    REQUIRE(result == "hello world");
  }

  SECTION("with quotes") {
    auto result = json_escape(R"(say "hello")");
    REQUIRE(result == R"(say \"hello\")");
  }

  SECTION("with backslash") {
    auto result = json_escape("path\\to\\file");
    REQUIRE(result == "path\\\\to\\\\file");
  }

  SECTION("with newlines and tabs") {
    auto result = json_escape("line1\nline2\ttab");
    REQUIRE(result == "line1\\nline2\\ttab");
  }

  SECTION("with all special characters") {
    auto result = json_escape("\b\f\n\r\t\"\\");
    REQUIRE(result == "\\b\\f\\n\\r\\t\\\"\\\\");
  }
}
