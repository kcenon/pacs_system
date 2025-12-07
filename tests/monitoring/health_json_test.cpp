/**
 * @file health_json_test.cpp
 * @brief Unit tests for health check JSON serialization
 *
 * @see Issue #211 - Implement health check endpoint
 */

#include <pacs/monitoring/health_json.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

using namespace pacs::monitoring;
using Catch::Matchers::ContainsSubstring;

// =============================================================================
// Helper Functions Tests
// =============================================================================

TEST_CASE("to_iso8601 formats correctly", "[monitoring][health_json]") {
    // Create a known time point: 2024-01-15T10:30:00Z
    std::tm tm{};
    tm.tm_year = 124;  // 2024 - 1900
    tm.tm_mon = 0;     // January
    tm.tm_mday = 15;
    tm.tm_hour = 10;
    tm.tm_min = 30;
    tm.tm_sec = 0;

#if defined(_MSC_VER)
    auto time_t_val = _mkgmtime(&tm);
#else
    auto time_t_val = timegm(&tm);
#endif

    auto tp = std::chrono::system_clock::from_time_t(time_t_val);
    auto result = to_iso8601(tp);

    CHECK(result == "2024-01-15T10:30:00Z");
}

TEST_CASE("escape_json_string escapes special characters",
          "[monitoring][health_json]") {
    SECTION("escapes quotes") {
        CHECK(escape_json_string(R"(Hello "World")") == R"(Hello \"World\")");
    }

    SECTION("escapes backslashes") {
        CHECK(escape_json_string(R"(path\to\file)") == R"(path\\to\\file)");
    }

    SECTION("escapes newlines") {
        CHECK(escape_json_string("line1\nline2") == R"(line1\nline2)");
    }

    SECTION("escapes tabs") {
        CHECK(escape_json_string("col1\tcol2") == R"(col1\tcol2)");
    }

    SECTION("handles normal strings") {
        CHECK(escape_json_string("normal string") == "normal string");
    }

    SECTION("handles empty strings") {
        CHECK(escape_json_string("") == "");
    }
}

// =============================================================================
// database_status JSON Tests
// =============================================================================

TEST_CASE("database_status to_json", "[monitoring][health_json]") {
    database_status status;
    status.connected = true;
    status.active_connections = 5;

    auto json = to_json(status);

    CHECK_THAT(json, ContainsSubstring(R"("connected":true)"));
    CHECK_THAT(json, ContainsSubstring(R"("active_connections":5)"));
}

TEST_CASE("database_status to_json with error", "[monitoring][health_json]") {
    database_status status;
    status.connected = false;
    status.error_message = "Connection refused";

    auto json = to_json(status);

    CHECK_THAT(json, ContainsSubstring(R"("connected":false)"));
    CHECK_THAT(json, ContainsSubstring(R"("error":"Connection refused")"));
}

TEST_CASE("database_status to_json with response_time",
          "[monitoring][health_json]") {
    database_status status;
    status.connected = true;
    status.response_time = std::chrono::milliseconds{42};

    auto json = to_json(status);

    CHECK_THAT(json, ContainsSubstring(R"("response_time_ms":42)"));
}

// =============================================================================
// storage_status JSON Tests
// =============================================================================

TEST_CASE("storage_status to_json", "[monitoring][health_json]") {
    storage_status status;
    status.writable = true;
    status.readable = true;
    status.total_bytes = 1000000;
    status.used_bytes = 500000;
    status.available_bytes = 500000;

    auto json = to_json(status);

    CHECK_THAT(json, ContainsSubstring(R"("writable":true)"));
    CHECK_THAT(json, ContainsSubstring(R"("readable":true)"));
    CHECK_THAT(json, ContainsSubstring(R"("total_bytes":1000000)"));
    CHECK_THAT(json, ContainsSubstring(R"("used_bytes":500000)"));
    CHECK_THAT(json, ContainsSubstring(R"("usage_percent":50.00)"));
}

// =============================================================================
// association_metrics JSON Tests
// =============================================================================

TEST_CASE("association_metrics to_json", "[monitoring][health_json]") {
    association_metrics metrics;
    metrics.active_associations = 10;
    metrics.max_associations = 100;
    metrics.total_associations = 5000;
    metrics.failed_associations = 25;

    auto json = to_json(metrics);

    CHECK_THAT(json, ContainsSubstring(R"("active":10)"));
    CHECK_THAT(json, ContainsSubstring(R"("max":100)"));
    CHECK_THAT(json, ContainsSubstring(R"("total":5000)"));
    CHECK_THAT(json, ContainsSubstring(R"("failed":25)"));
}

// =============================================================================
// storage_metrics JSON Tests
// =============================================================================

TEST_CASE("storage_metrics to_json", "[monitoring][health_json]") {
    storage_metrics metrics;
    metrics.total_instances = 10000;
    metrics.total_studies = 500;
    metrics.total_series = 2000;
    metrics.successful_stores = 9900;
    metrics.failed_stores = 100;

    auto json = to_json(metrics);

    CHECK_THAT(json, ContainsSubstring(R"("total_instances":10000)"));
    CHECK_THAT(json, ContainsSubstring(R"("total_studies":500)"));
    CHECK_THAT(json, ContainsSubstring(R"("total_series":2000)"));
    CHECK_THAT(json, ContainsSubstring(R"("successful_stores":9900)"));
    CHECK_THAT(json, ContainsSubstring(R"("failed_stores":100)"));
}

// =============================================================================
// version_info JSON Tests
// =============================================================================

TEST_CASE("version_info to_json", "[monitoring][health_json]") {
    version_info info;
    info.major = 2;
    info.minor = 3;
    info.patch = 4;
    info.build_id = "abc123";

    auto json = to_json(info);

    CHECK_THAT(json, ContainsSubstring(R"("version":"2.3.4")"));
    CHECK_THAT(json, ContainsSubstring(R"("major":2)"));
    CHECK_THAT(json, ContainsSubstring(R"("minor":3)"));
    CHECK_THAT(json, ContainsSubstring(R"("patch":4)"));
    CHECK_THAT(json, ContainsSubstring(R"("build_id":"abc123")"));
    CHECK_THAT(json, ContainsSubstring(R"("uptime_seconds":)"));
}

TEST_CASE("version_info to_json without build_id",
          "[monitoring][health_json]") {
    version_info info;
    info.major = 1;
    info.minor = 0;
    info.patch = 0;

    auto json = to_json(info);

    CHECK_THAT(json, ContainsSubstring(R"("version":"1.0.0")"));
    // Should not contain build_id key when empty
}

// =============================================================================
// health_status JSON Tests
// =============================================================================

TEST_CASE("health_status to_json complete", "[monitoring][health_json]") {
    health_status status;
    status.level = health_level::healthy;
    status.database.connected = true;
    status.storage.writable = true;
    status.storage.readable = true;

    auto json = to_json(status);

    CHECK_THAT(json, ContainsSubstring(R"("status":"healthy")"));
    CHECK_THAT(json, ContainsSubstring(R"("healthy":true)"));
    CHECK_THAT(json, ContainsSubstring(R"("operational":true)"));
    CHECK_THAT(json, ContainsSubstring(R"("database":)"));
    CHECK_THAT(json, ContainsSubstring(R"("storage":)"));
    CHECK_THAT(json, ContainsSubstring(R"("associations":)"));
    CHECK_THAT(json, ContainsSubstring(R"("metrics":)"));
    CHECK_THAT(json, ContainsSubstring(R"("version":)"));
}

TEST_CASE("health_status to_json with message", "[monitoring][health_json]") {
    health_status status;
    status.level = health_level::degraded;
    status.message = "Storage usage high";

    auto json = to_json(status);

    CHECK_THAT(json, ContainsSubstring(R"("status":"degraded")"));
    CHECK_THAT(json, ContainsSubstring(R"("message":"Storage usage high")"));
}

TEST_CASE("health_status to_json unhealthy", "[monitoring][health_json]") {
    health_status status;
    status.level = health_level::unhealthy;
    status.database.connected = false;
    status.database.error_message = "Connection timeout";

    auto json = to_json(status);

    CHECK_THAT(json, ContainsSubstring(R"("status":"unhealthy")"));
    CHECK_THAT(json, ContainsSubstring(R"("healthy":false)"));
    CHECK_THAT(json, ContainsSubstring(R"("operational":false)"));
}

// =============================================================================
// Pretty JSON Tests
// =============================================================================

TEST_CASE("health_status to_json_pretty", "[monitoring][health_json]") {
    health_status status;
    status.level = health_level::healthy;
    status.database.connected = true;
    status.storage.writable = true;
    status.storage.readable = true;

    auto json = to_json_pretty(status);

    // Pretty print should have newlines
    CHECK_THAT(json, ContainsSubstring("\n"));

    // Should contain proper structure
    CHECK_THAT(json, ContainsSubstring(R"("status": "healthy")"));
    CHECK_THAT(json, ContainsSubstring(R"("database": {)"));
}

TEST_CASE("health_status to_json_pretty with custom indent",
          "[monitoring][health_json]") {
    health_status status;
    status.level = health_level::healthy;

    auto json = to_json_pretty(status, 4);

    // Should have 4-space indentation
    CHECK_THAT(json, ContainsSubstring("    \"status\":"));
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("JSON escaping in error messages", "[monitoring][health_json]") {
    database_status status;
    status.connected = false;
    status.error_message = R"(Error: "Connection failed" at path\to\server)";

    auto json = to_json(status);

    // Should properly escape quotes and backslashes
    CHECK_THAT(json, ContainsSubstring(R"(\"Connection failed\")"));
    CHECK_THAT(json, ContainsSubstring(R"(path\\to\\server)"));
}

TEST_CASE("JSON with large numbers", "[monitoring][health_json]") {
    storage_status status;
    status.writable = true;
    status.readable = true;
    status.total_bytes = 1'000'000'000'000;  // 1 TB
    status.used_bytes = 500'000'000'000;     // 500 GB

    auto json = to_json(status);

    CHECK_THAT(json, ContainsSubstring("1000000000000"));
    CHECK_THAT(json, ContainsSubstring("500000000000"));
}

TEST_CASE("JSON with zero values", "[monitoring][health_json]") {
    storage_metrics metrics;
    // All default to 0

    auto json = to_json(metrics);

    CHECK_THAT(json, ContainsSubstring(R"("total_instances":0)"));
    CHECK_THAT(json, ContainsSubstring(R"("total_studies":0)"));
    CHECK_THAT(json, ContainsSubstring(R"("failed_stores":0)"));
}
