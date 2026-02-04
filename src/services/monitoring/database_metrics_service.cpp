/**
 * @file database_metrics_service.cpp
 * @brief Database monitoring and metrics service implementation
 *
 * @copyright Copyright (c) 2025
 * @license MIT
 */

#include "pacs/services/monitoring/database_metrics_service.hpp"

#ifdef PACS_WITH_DATABASE_SYSTEM

#include <algorithm>
#include <iomanip>
#include <mutex>
#include <sstream>

namespace pacs::services::monitoring {

// ============================================================================
// database_metrics_service::impl
// ============================================================================

struct database_metrics_service::impl {
    std::shared_ptr<storage::pacs_database_adapter> db;
    std::chrono::microseconds slow_query_threshold{100000};  // 100ms
    std::chrono::minutes metrics_retention{5};
    std::vector<slow_query_callback> slow_query_callbacks;
    std::vector<slow_query> slow_query_history;
    std::mutex mutex;  // Protects callbacks and history

    explicit impl(std::shared_ptr<storage::pacs_database_adapter> database)
        : db(std::move(database)) {}

    void add_slow_query(slow_query sq) {
        std::lock_guard<std::mutex> lock(mutex);

        // Add to history
        slow_query_history.push_back(sq);

        // Notify callbacks
        for (const auto& callback : slow_query_callbacks) {
            try {
                callback(sq);
            } catch (const std::exception&) {
                // Ignore callback exceptions
            }
        }

        // Cleanup old entries (keep last 1000)
        if (slow_query_history.size() > 1000) {
            slow_query_history.erase(slow_query_history.begin(),
                                      slow_query_history.begin() + 500);
        }
    }
};

// ============================================================================
// database_metrics_service
// ============================================================================

database_metrics_service::database_metrics_service(
    std::shared_ptr<storage::pacs_database_adapter> db)
    : impl_(std::make_unique<impl>(std::move(db))) {}

database_metrics_service::~database_metrics_service() = default;

database_metrics_service::database_metrics_service(
    database_metrics_service&&) noexcept = default;

auto database_metrics_service::operator=(database_metrics_service&&) noexcept
    -> database_metrics_service& = default;

// ============================================================================
// Configuration
// ============================================================================

void database_metrics_service::set_slow_query_threshold(
    std::chrono::microseconds threshold) {
    impl_->slow_query_threshold = threshold;
}

void database_metrics_service::set_metrics_retention(
    std::chrono::minutes retention) {
    impl_->metrics_retention = retention;
}

void database_metrics_service::register_slow_query_callback(
    slow_query_callback callback) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->slow_query_callbacks.push_back(std::move(callback));
}

// ============================================================================
// Health Checks
// ============================================================================

auto database_metrics_service::check_health() -> database_health {
    database_health health;

    auto start = std::chrono::steady_clock::now();

    // Simple connectivity check
    auto result = impl_->db->select("SELECT 1");

    auto end = std::chrono::steady_clock::now();
    health.response_time =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    if (result.is_err()) {
        health.current_status = database_health::status::unhealthy;
        health.message =
            "Database connection failed: " + result.error().message;
        return health;
    }

    // Get metrics for additional checks
    auto metrics = get_current_metrics();
    health.active_connections = metrics.active_connections;
    health.error_rate = metrics.error_rate;

    // Determine status based on metrics
    if (metrics.error_rate > 0.1) {  // >10% error rate
        health.current_status = database_health::status::unhealthy;
        health.message = "High error rate detected";
        health.warnings.push_back(
            "Error rate: " + std::to_string(metrics.error_rate * 100) + "%");
    } else if (metrics.error_rate > 0.01 ||
               metrics.connection_utilization > 0.8 ||
               metrics.avg_latency_us > 50000) {
        health.current_status = database_health::status::degraded;
        health.message = "Database performance degraded";

        if (metrics.error_rate > 0.01) {
            health.warnings.push_back("Elevated error rate");
        }
        if (metrics.connection_utilization > 0.8) {
            health.warnings.push_back("High connection utilization");
        }
        if (metrics.avg_latency_us > 50000) {
            health.warnings.push_back("High average latency");
        }
    } else {
        health.current_status = database_health::status::healthy;
        health.message = "Database is healthy";
    }

    return health;
}

auto database_metrics_service::is_healthy() -> bool {
    auto health = check_health();
    return health.current_status == database_health::status::healthy;
}

// ============================================================================
// Metrics Retrieval
// ============================================================================

auto database_metrics_service::get_current_metrics() -> database_metrics {
    database_metrics metrics;

    // TODO(Phase 1): Integrate with database_system's performance_monitor
    // For now, return placeholder values
    // This will be populated when database_system monitoring API is available

    // Placeholder implementation - will be replaced with:
    // auto context = impl_->db->context();
    // auto monitor = context->get_performance_monitor();
    // auto summary = monitor->get_performance_summary();

    metrics.total_queries = 0;
    metrics.successful_queries = 0;
    metrics.failed_queries = 0;
    metrics.queries_per_second = 0.0;
    metrics.avg_latency_us = 0;
    metrics.min_latency_us = 0;
    metrics.max_latency_us = 0;
    metrics.p95_latency_us = 0;
    metrics.p99_latency_us = 0;
    metrics.active_connections = 1;  // Assume at least 1 connection
    metrics.pool_size = 1;
    metrics.connection_utilization = 0.0;
    metrics.error_rate = 0.0;
    metrics.slow_query_count = get_slow_queries().size();

    return metrics;
}

auto database_metrics_service::get_slow_queries(
    [[maybe_unused]] std::chrono::minutes since) -> std::vector<slow_query> {
    std::lock_guard<std::mutex> lock(impl_->mutex);

    // TODO: Filter by timestamp when slow query tracking is integrated
    // For now, return recent slow queries from history

    std::vector<slow_query> result;
    result.reserve(impl_->slow_query_history.size());

    // Return all queries for now
    // Will be filtered by timestamp in future implementation
    result = impl_->slow_query_history;

    return result;
}

auto database_metrics_service::get_top_slow_queries(size_t limit)
    -> std::vector<slow_query> {
    std::lock_guard<std::mutex> lock(impl_->mutex);

    std::vector<slow_query> sorted = impl_->slow_query_history;

    // Sort by duration (descending)
    std::sort(sorted.begin(), sorted.end(),
              [](const slow_query& a, const slow_query& b) {
                  return a.duration_us > b.duration_us;
              });

    // Return top N
    if (sorted.size() > limit) {
        sorted.resize(limit);
    }

    return sorted;
}

// ============================================================================
// Metrics Export
// ============================================================================

auto database_metrics_service::export_prometheus_metrics() -> std::string {
    auto metrics = get_current_metrics();
    std::ostringstream oss;

    oss << "# HELP pacs_db_queries_total Total database queries\n"
        << "# TYPE pacs_db_queries_total counter\n"
        << "pacs_db_queries_total{status=\"success\"} "
        << metrics.successful_queries << "\n"
        << "pacs_db_queries_total{status=\"failure\"} " << metrics.failed_queries
        << "\n\n";

    oss << "# HELP pacs_db_query_duration_microseconds Query duration\n"
        << "# TYPE pacs_db_query_duration_microseconds summary\n"
        << "pacs_db_query_duration_microseconds{quantile=\"0.5\"} "
        << metrics.avg_latency_us << "\n"
        << "pacs_db_query_duration_microseconds{quantile=\"0.95\"} "
        << metrics.p95_latency_us << "\n"
        << "pacs_db_query_duration_microseconds{quantile=\"0.99\"} "
        << metrics.p99_latency_us << "\n\n";

    oss << "# HELP pacs_db_queries_per_second Query throughput\n"
        << "# TYPE pacs_db_queries_per_second gauge\n"
        << "pacs_db_queries_per_second " << std::fixed << std::setprecision(2)
        << metrics.queries_per_second << "\n\n";

    oss << "# HELP pacs_db_connections Active connections\n"
        << "# TYPE pacs_db_connections gauge\n"
        << "pacs_db_connections{state=\"active\"} " << metrics.active_connections
        << "\n\n";

    oss << "# HELP pacs_db_connection_utilization Connection pool utilization\n"
        << "# TYPE pacs_db_connection_utilization gauge\n"
        << "pacs_db_connection_utilization " << std::fixed
        << std::setprecision(4) << metrics.connection_utilization << "\n\n";

    oss << "# HELP pacs_db_error_rate Database error rate\n"
        << "# TYPE pacs_db_error_rate gauge\n"
        << "pacs_db_error_rate " << std::fixed << std::setprecision(4)
        << metrics.error_rate << "\n\n";

    oss << "# HELP pacs_db_slow_queries Slow queries count\n"
        << "# TYPE pacs_db_slow_queries gauge\n"
        << "pacs_db_slow_queries " << metrics.slow_query_count << "\n";

    return oss.str();
}

// ============================================================================
// Helper Functions
// ============================================================================

auto health_status_to_string(database_health::status status) -> std::string {
    switch (status) {
        case database_health::status::healthy:
            return "healthy";
        case database_health::status::degraded:
            return "degraded";
        case database_health::status::unhealthy:
            return "unhealthy";
        default:
            return "unknown";
    }
}

}  // namespace pacs::services::monitoring

#endif  // PACS_WITH_DATABASE_SYSTEM
