/**
 * @file database_metrics_service.hpp
 * @brief Database monitoring and metrics service
 *
 * Integrates database_system's built-in monitoring capabilities into PACS system.
 * Provides real-time query metrics, slow query detection, connection pool monitoring,
 * and exposes metrics through PACS web endpoints.
 *
 * @see Issue #611 - Phase 5: Monitoring & Observability Integration
 * @see Epic #605 - Migrate Direct Database Access to database_system Abstraction
 *
 * @copyright Copyright (c) 2025
 * @license MIT
 */

#pragma once

#include "pacs/storage/pacs_database_adapter.hpp"

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace pacs::services::monitoring {

/**
 * @brief Database health status
 */
struct database_health {
    /**
     * @brief Health status enumeration
     */
    enum class status {
        healthy,   ///< Database is operating normally
        degraded,  ///< Performance degraded but functional
        unhealthy  ///< Database unavailable or severely impaired
    };

    status current_status;                             ///< Current health status
    std::string message;                               ///< Status description
    std::chrono::milliseconds response_time{0};        ///< Health check response time
    size_t active_connections{0};                      ///< Active database connections
    double error_rate{0.0};                            ///< Current error rate (0.0-1.0)
    std::vector<std::string> warnings;                 ///< Health warnings if any
};

/**
 * @brief Database performance metrics
 */
struct database_metrics {
    // Query statistics
    size_t total_queries{0};       ///< Total queries executed
    size_t successful_queries{0};  ///< Successfully completed queries
    size_t failed_queries{0};      ///< Failed queries
    double queries_per_second{0.0};  ///< Query throughput

    // Latency (microseconds)
    uint64_t avg_latency_us{0};  ///< Average query latency
    uint64_t min_latency_us{0};  ///< Minimum query latency
    uint64_t max_latency_us{0};  ///< Maximum query latency
    uint64_t p95_latency_us{0};  ///< 95th percentile latency
    uint64_t p99_latency_us{0};  ///< 99th percentile latency

    // Connections
    size_t active_connections{0};     ///< Active connections count
    size_t pool_size{0};              ///< Connection pool size
    double connection_utilization{0.0};  ///< Pool utilization ratio

    // Errors
    double error_rate{0.0};       ///< Error rate (0.0-1.0)
    size_t slow_query_count{0};   ///< Number of slow queries detected
};

/**
 * @brief Slow query information
 */
struct slow_query {
    std::string query_hash;     ///< Anonymized query hash
    std::string query_preview;  ///< Query preview (first 100 chars)
    uint64_t duration_us;       ///< Query duration in microseconds
    std::string timestamp;      ///< Query execution timestamp (ISO 8601)
    size_t rows_affected;       ///< Rows affected/returned
};

/**
 * @brief Database metrics and monitoring service
 *
 * Provides comprehensive database monitoring capabilities including:
 * - Real-time query performance metrics
 * - Automatic slow query detection and logging
 * - Connection pool utilization tracking
 * - Health check endpoint for database
 * - Prometheus-compatible metrics export
 *
 * **Thread Safety:** This class is thread-safe for read operations.
 * Callbacks are invoked synchronously and should complete quickly.
 *
 * @example
 * @code
 * auto db = std::make_shared<pacs_database_adapter>("/path/to/db.sqlite");
 * db->connect();
 *
 * database_metrics_service metrics_svc(db);
 * metrics_svc.set_slow_query_threshold(std::chrono::milliseconds(100));
 *
 * // Register callback for slow queries
 * metrics_svc.register_slow_query_callback([](const slow_query& sq) {
 *     logger.warn("Slow query detected: " + sq.query_preview);
 * });
 *
 * // Check health
 * auto health = metrics_svc.check_health();
 * if (health.current_status != database_health::status::healthy) {
 *     logger.error("Database unhealthy: " + health.message);
 * }
 *
 * // Get metrics for monitoring
 * auto metrics = metrics_svc.get_current_metrics();
 * std::cout << "QPS: " << metrics.queries_per_second << std::endl;
 * @endcode
 */
class database_metrics_service {
public:
    /// Callback type for slow query notifications
    using slow_query_callback = std::function<void(const slow_query&)>;

    /**
     * @brief Construct metrics service with database adapter
     *
     * @param db Shared pointer to pacs_database_adapter
     */
    explicit database_metrics_service(
        std::shared_ptr<storage::pacs_database_adapter> db);

    /**
     * @brief Destructor
     */
    ~database_metrics_service();

    // Non-copyable, movable
    database_metrics_service(const database_metrics_service&) = delete;
    auto operator=(const database_metrics_service&)
        -> database_metrics_service& = delete;
    database_metrics_service(database_metrics_service&&) noexcept;
    auto operator=(database_metrics_service&&) noexcept
        -> database_metrics_service&;

    // ========================================================================
    // Configuration
    // ========================================================================

    /**
     * @brief Set slow query threshold
     *
     * Queries exceeding this duration will be logged as slow queries.
     *
     * @param threshold Duration threshold (default: 100ms)
     */
    void set_slow_query_threshold(std::chrono::microseconds threshold);

    /**
     * @brief Set metrics retention period
     *
     * @param retention How long to retain historical metrics (default: 5 minutes)
     */
    void set_metrics_retention(std::chrono::minutes retention);

    /**
     * @brief Register callback for slow query notifications
     *
     * Multiple callbacks can be registered. Callbacks are invoked synchronously
     * when a slow query is detected and should complete quickly.
     *
     * @param callback Function to call when slow query detected
     */
    void register_slow_query_callback(slow_query_callback callback);

    // ========================================================================
    // Health Checks
    // ========================================================================

    /**
     * @brief Check database health
     *
     * Performs connectivity check and analyzes current metrics to determine
     * health status. Health is determined based on:
     * - Database connectivity
     * - Error rate (<1% healthy, <10% degraded, >10% unhealthy)
     * - Connection utilization (<80% healthy)
     * - Average latency (<50ms healthy)
     *
     * @return Database health status and details
     */
    [[nodiscard]] auto check_health() -> database_health;

    /**
     * @brief Quick health check
     *
     * @return true if database is healthy, false otherwise
     */
    [[nodiscard]] auto is_healthy() -> bool;

    // ========================================================================
    // Metrics Retrieval
    // ========================================================================

    /**
     * @brief Get current performance metrics
     *
     * Returns real-time snapshot of database performance metrics.
     *
     * @return Current metrics snapshot
     */
    [[nodiscard]] auto get_current_metrics() -> database_metrics;

    /**
     * @brief Get recent slow queries
     *
     * @param since Time window to look back (default: 5 minutes)
     * @return List of slow queries within the time window
     */
    [[nodiscard]] auto get_slow_queries(
        std::chrono::minutes since = std::chrono::minutes(5))
        -> std::vector<slow_query>;

    /**
     * @brief Get top slow queries
     *
     * Returns the slowest queries ordered by duration.
     *
     * @param limit Maximum number of queries to return (default: 10)
     * @return List of slowest queries
     */
    [[nodiscard]] auto get_top_slow_queries(size_t limit = 10)
        -> std::vector<slow_query>;

    // ========================================================================
    // Metrics Export
    // ========================================================================

    /**
     * @brief Export metrics in Prometheus format
     *
     * Returns metrics formatted according to Prometheus text exposition format.
     * Includes:
     * - pacs_db_queries_total
     * - pacs_db_query_duration_microseconds
     * - pacs_db_queries_per_second
     * - pacs_db_connections
     * - pacs_db_connection_utilization
     * - pacs_db_error_rate
     * - pacs_db_slow_queries
     *
     * @return Prometheus-formatted metrics string
     */
    [[nodiscard]] auto export_prometheus_metrics() -> std::string;

private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

/**
 * @brief Convert health status to string
 *
 * @param status Health status enum value
 * @return String representation ("healthy", "degraded", "unhealthy")
 */
[[nodiscard]] auto health_status_to_string(database_health::status status)
    -> std::string;

}  // namespace pacs::services::monitoring
