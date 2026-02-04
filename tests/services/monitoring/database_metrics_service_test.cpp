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
// Helper Functions
// ============================================================================

namespace {
/**
 * @brief Check if SQLite backend is supported by unified_database_system
 *
 * Creates a test adapter to verify SQLite connectivity works.
 * This is needed because some platforms (like Windows CI) may not support
 * the SQLite backend in unified_database_system.
 */
bool is_sqlite_backend_supported() {
    pacs_database_adapter db(":memory:");
    auto result = db.connect();
    return result.is_ok();
}

/**
 * @brief Helper to create metrics service with connected database
 *
 * Returns nullptr if SQLite backend is not supported.
 */
std::pair<std::shared_ptr<pacs_database_adapter>,
          std::unique_ptr<database_metrics_service>>
create_test_metrics_service() {
    auto db = std::make_shared<pacs_database_adapter>(":memory:");
    auto connect_result = db->connect();
    if (!connect_result.is_ok()) {
        return {nullptr, nullptr};
    }
    auto metrics_service = std::make_unique<database_metrics_service>(db);
    return {db, std::move(metrics_service)};
}
}  // namespace

// ============================================================================
// Health Check Tests
// ============================================================================

TEST_CASE("database_metrics_service health check returns healthy for "
          "connected database",
          "[database_metrics_service]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported by unified_database_system");
        return;
    }

    auto [db, metrics_service] = create_test_metrics_service();
    REQUIRE(db != nullptr);
    REQUIRE(metrics_service != nullptr);

    auto health = metrics_service->check_health();

    REQUIRE(health.current_status == database_health::status::healthy);
    REQUIRE(!health.message.empty());
    REQUIRE(health.response_time.count() >= 0);

    (void)db->disconnect();
}

TEST_CASE("database_metrics_service is_healthy returns true for "
          "connected database",
          "[database_metrics_service]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported by unified_database_system");
        return;
    }

    auto [db, metrics_service] = create_test_metrics_service();
    REQUIRE(db != nullptr);
    REQUIRE(metrics_service != nullptr);

    REQUIRE(metrics_service->is_healthy());

    (void)db->disconnect();
}

// ============================================================================
// Configuration Tests
// ============================================================================

TEST_CASE("database_metrics_service can set slow query threshold",
          "[database_metrics_service]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported by unified_database_system");
        return;
    }

    auto [db, metrics_service] = create_test_metrics_service();
    REQUIRE(db != nullptr);
    REQUIRE(metrics_service != nullptr);

    auto threshold = std::chrono::microseconds(50000);  // 50ms
    REQUIRE_NOTHROW(metrics_service->set_slow_query_threshold(threshold));

    (void)db->disconnect();
}

TEST_CASE("database_metrics_service can set metrics retention",
          "[database_metrics_service]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported by unified_database_system");
        return;
    }

    auto [db, metrics_service] = create_test_metrics_service();
    REQUIRE(db != nullptr);
    REQUIRE(metrics_service != nullptr);

    auto retention = std::chrono::minutes(10);
    REQUIRE_NOTHROW(metrics_service->set_metrics_retention(retention));

    (void)db->disconnect();
}

TEST_CASE("database_metrics_service can register slow query callback",
          "[database_metrics_service]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported by unified_database_system");
        return;
    }

    auto [db, metrics_service] = create_test_metrics_service();
    REQUIRE(db != nullptr);
    REQUIRE(metrics_service != nullptr);

    bool callback_called = false;
    metrics_service->register_slow_query_callback(
        [&callback_called](const slow_query&) { callback_called = true; });

    // Callback won't be called unless there's actual slow query detection
    // This just tests registration succeeds
    REQUIRE_FALSE(callback_called);

    (void)db->disconnect();
}

// ============================================================================
// Metrics Retrieval Tests
// ============================================================================

TEST_CASE("database_metrics_service returns valid current metrics",
          "[database_metrics_service]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported by unified_database_system");
        return;
    }

    auto [db, metrics_service] = create_test_metrics_service();
    REQUIRE(db != nullptr);
    REQUIRE(metrics_service != nullptr);

    auto metrics = metrics_service->get_current_metrics();

    // Basic sanity checks
    REQUIRE(metrics.total_queries >= 0);
    REQUIRE(metrics.successful_queries >= 0);
    REQUIRE(metrics.failed_queries >= 0);
    REQUIRE(metrics.queries_per_second >= 0.0);
    REQUIRE(metrics.active_connections >= 0);

    (void)db->disconnect();
}

TEST_CASE("database_metrics_service returns empty slow queries initially",
          "[database_metrics_service]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported by unified_database_system");
        return;
    }

    auto [db, metrics_service] = create_test_metrics_service();
    REQUIRE(db != nullptr);
    REQUIRE(metrics_service != nullptr);

    auto slow_queries = metrics_service->get_slow_queries();

    REQUIRE(slow_queries.empty());

    (void)db->disconnect();
}

TEST_CASE("database_metrics_service returns empty top slow queries "
          "initially",
          "[database_metrics_service]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported by unified_database_system");
        return;
    }

    auto [db, metrics_service] = create_test_metrics_service();
    REQUIRE(db != nullptr);
    REQUIRE(metrics_service != nullptr);

    auto slow_queries = metrics_service->get_top_slow_queries(10);

    REQUIRE(slow_queries.empty());

    (void)db->disconnect();
}

// ============================================================================
// Prometheus Export Tests
// ============================================================================

TEST_CASE("database_metrics_service exports valid Prometheus metrics",
          "[database_metrics_service]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported by unified_database_system");
        return;
    }

    auto [db, metrics_service] = create_test_metrics_service();
    REQUIRE(db != nullptr);
    REQUIRE(metrics_service != nullptr);

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

    (void)db->disconnect();
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

TEST_CASE("database_metrics_service handles database disconnect "
          "gracefully",
          "[database_metrics_service]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported by unified_database_system");
        return;
    }

    auto [db, metrics_service] = create_test_metrics_service();
    REQUIRE(db != nullptr);
    REQUIRE(metrics_service != nullptr);

    // Disconnect database
    auto disconnect_result = db->disconnect();
    REQUIRE(disconnect_result.is_ok());

    // Health check should fail gracefully
    auto health = metrics_service->check_health();
    REQUIRE(health.current_status == database_health::status::unhealthy);
}

TEST_CASE("database_metrics_service returns consistent metrics across "
          "multiple calls",
          "[database_metrics_service]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported by unified_database_system");
        return;
    }

    auto [db, metrics_service] = create_test_metrics_service();
    REQUIRE(db != nullptr);
    REQUIRE(metrics_service != nullptr);

    auto metrics1 = metrics_service->get_current_metrics();
    auto metrics2 = metrics_service->get_current_metrics();

    // For a fresh database with no activity, metrics should be identical
    REQUIRE(metrics1.total_queries == metrics2.total_queries);
    REQUIRE(metrics1.successful_queries == metrics2.successful_queries);
    REQUIRE(metrics1.failed_queries == metrics2.failed_queries);

    (void)db->disconnect();
}

#endif  // PACS_WITH_DATABASE_SYSTEM
