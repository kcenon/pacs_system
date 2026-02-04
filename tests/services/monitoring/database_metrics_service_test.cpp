/**
 * @file database_metrics_service_test.cpp
 * @brief Unit tests for database_metrics_service
 *
 * @copyright Copyright (c) 2025
 * @license MIT
 */

#include "pacs/services/monitoring/database_metrics_service.hpp"
#include "pacs/storage/pacs_database_adapter.hpp"

#ifdef PACS_WITH_DATABASE_SYSTEM

#include <catch2/catch_test_macros.hpp>
#include <memory>

using namespace pacs::services::monitoring;
using namespace pacs::storage;

// ============================================================================
// Test Fixture
// ============================================================================

struct DatabaseMetricsServiceTestFixture {
    DatabaseMetricsServiceTestFixture() {
        // Create in-memory SQLite database for testing
        db = std::make_shared<pacs_database_adapter>(":memory:");
        auto connect_result = db->connect();
        REQUIRE(connect_result.is_ok());

        // Create metrics service
        metrics_service = std::make_unique<database_metrics_service>(db);
    }

    ~DatabaseMetricsServiceTestFixture() {
        metrics_service.reset();
        if (db) {
            (void)db->disconnect();
        }
    }

    std::shared_ptr<pacs_database_adapter> db;
    std::unique_ptr<database_metrics_service> metrics_service;
};

// ============================================================================
// Health Check Tests
// ============================================================================

TEST_CASE_METHOD(DatabaseMetricsServiceTestFixture,
                 "database_metrics_service health check returns healthy for "
                 "connected database",
                 "[database_metrics_service]") {
    auto health = metrics_service->check_health();

    REQUIRE(health.current_status == database_health::status::healthy);
    REQUIRE(!health.message.empty());
    REQUIRE(health.response_time.count() >= 0);
}

TEST_CASE_METHOD(DatabaseMetricsServiceTestFixture,
                 "database_metrics_service is_healthy returns true for "
                 "connected database",
                 "[database_metrics_service]") {
    REQUIRE(metrics_service->is_healthy());
}

// ============================================================================
// Configuration Tests
// ============================================================================

TEST_CASE_METHOD(DatabaseMetricsServiceTestFixture,
                 "database_metrics_service can set slow query threshold",
                 "[database_metrics_service]") {
    auto threshold = std::chrono::microseconds(50000);  // 50ms
    REQUIRE_NOTHROW(metrics_service->set_slow_query_threshold(threshold));
}

TEST_CASE_METHOD(DatabaseMetricsServiceTestFixture,
                 "database_metrics_service can set metrics retention",
                 "[database_metrics_service]") {
    auto retention = std::chrono::minutes(10);
    REQUIRE_NOTHROW(metrics_service->set_metrics_retention(retention));
}

TEST_CASE_METHOD(DatabaseMetricsServiceTestFixture,
                 "database_metrics_service can register slow query callback",
                 "[database_metrics_service]") {
    bool callback_called = false;
    metrics_service->register_slow_query_callback(
        [&callback_called](const slow_query&) { callback_called = true; });

    // Callback won't be called unless there's actual slow query detection
    // This just tests registration succeeds
    REQUIRE_FALSE(callback_called);
}

// ============================================================================
// Metrics Retrieval Tests
// ============================================================================

TEST_CASE_METHOD(DatabaseMetricsServiceTestFixture,
                 "database_metrics_service returns valid current metrics",
                 "[database_metrics_service]") {
    auto metrics = metrics_service->get_current_metrics();

    // Basic sanity checks
    REQUIRE(metrics.total_queries >= 0);
    REQUIRE(metrics.successful_queries >= 0);
    REQUIRE(metrics.failed_queries >= 0);
    REQUIRE(metrics.queries_per_second >= 0.0);
    REQUIRE(metrics.active_connections >= 0);
}

TEST_CASE_METHOD(DatabaseMetricsServiceTestFixture,
                 "database_metrics_service returns empty slow queries initially",
                 "[database_metrics_service]") {
    auto slow_queries = metrics_service->get_slow_queries();

    REQUIRE(slow_queries.empty());
}

TEST_CASE_METHOD(DatabaseMetricsServiceTestFixture,
                 "database_metrics_service returns empty top slow queries "
                 "initially",
                 "[database_metrics_service]") {
    auto slow_queries = metrics_service->get_top_slow_queries(10);

    REQUIRE(slow_queries.empty());
}

// ============================================================================
// Prometheus Export Tests
// ============================================================================

TEST_CASE_METHOD(DatabaseMetricsServiceTestFixture,
                 "database_metrics_service exports valid Prometheus metrics",
                 "[database_metrics_service]") {
    auto prometheus_output = metrics_service->export_prometheus_metrics();

    // Verify it contains expected metric names
    REQUIRE(prometheus_output.find("pacs_db_queries_total") !=
            std::string::npos);
    REQUIRE(prometheus_output.find("pacs_db_query_duration_microseconds") !=
            std::string::npos);
    REQUIRE(prometheus_output.find("pacs_db_queries_per_second") !=
            std::string::npos);
    REQUIRE(prometheus_output.find("pacs_db_connections") != std::string::npos);
    REQUIRE(prometheus_output.find("pacs_db_connection_utilization") !=
            std::string::npos);
    REQUIRE(prometheus_output.find("pacs_db_error_rate") != std::string::npos);
    REQUIRE(prometheus_output.find("pacs_db_slow_queries") != std::string::npos);

    // Verify it contains HELP and TYPE lines
    REQUIRE(prometheus_output.find("# HELP") != std::string::npos);
    REQUIRE(prometheus_output.find("# TYPE") != std::string::npos);
}

// ============================================================================
// Helper Function Tests
// ============================================================================

TEST_CASE("health_status_to_string returns correct strings",
          "[database_metrics_service]") {
    REQUIRE(health_status_to_string(database_health::status::healthy) ==
            "healthy");
    REQUIRE(health_status_to_string(database_health::status::degraded) ==
            "degraded");
    REQUIRE(health_status_to_string(database_health::status::unhealthy) ==
            "unhealthy");
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_CASE_METHOD(DatabaseMetricsServiceTestFixture,
                 "database_metrics_service handles database disconnect "
                 "gracefully",
                 "[database_metrics_service]") {
    // Disconnect database
    auto disconnect_result = db->disconnect();
    REQUIRE(disconnect_result.is_ok());

    // Health check should fail gracefully
    auto health = metrics_service->check_health();
    REQUIRE(health.current_status == database_health::status::unhealthy);
}

TEST_CASE_METHOD(DatabaseMetricsServiceTestFixture,
                 "database_metrics_service returns consistent metrics across "
                 "multiple calls",
                 "[database_metrics_service]") {
    auto metrics1 = metrics_service->get_current_metrics();
    auto metrics2 = metrics_service->get_current_metrics();

    // For a fresh database with no activity, metrics should be identical
    REQUIRE(metrics1.total_queries == metrics2.total_queries);
    REQUIRE(metrics1.successful_queries == metrics2.successful_queries);
    REQUIRE(metrics1.failed_queries == metrics2.failed_queries);
}

#endif  // PACS_WITH_DATABASE_SYSTEM
