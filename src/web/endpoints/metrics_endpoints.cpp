/**
 * @file metrics_endpoints.cpp
 * @brief Database metrics REST API endpoints implementation
 *
 * @copyright Copyright (c) 2025
 * @license MIT
 */

// IMPORTANT: Include Crow FIRST before any PACS headers to avoid forward
// declaration conflicts
#include "crow.h"

#include "pacs/services/monitoring/database_metrics_service.hpp"
#include "pacs/web/endpoints/metrics_endpoints.hpp"
#include "pacs/web/endpoints/system_endpoints.hpp"
#include "pacs/web/rest_config.hpp"
#include "pacs/web/rest_types.hpp"

#ifdef PACS_WITH_DATABASE_SYSTEM

#include <sstream>

namespace pacs::web::endpoints {

namespace {

/**
 * @brief Add CORS headers to response
 */
void add_cors_headers(crow::response& res, const rest_server_context& ctx) {
    if (ctx.config && !ctx.config->cors_allowed_origins.empty()) {
        res.add_header("Access-Control-Allow-Origin",
                       ctx.config->cors_allowed_origins);
    }
}

/**
 * @brief Convert health status to string
 */
std::string health_status_to_json_string(
    services::monitoring::database_health::status status) {
    switch (status) {
        case services::monitoring::database_health::status::healthy:
            return "healthy";
        case services::monitoring::database_health::status::degraded:
            return "degraded";
        case services::monitoring::database_health::status::unhealthy:
            return "unhealthy";
        default:
            return "unknown";
    }
}

/**
 * @brief Get int parameter from query string with default value
 */
int get_query_param_int(const crow::request& req, const std::string& key,
                        int default_value) {
    auto value = req.url_params.get(key);
    if (value == nullptr) {
        return default_value;
    }
    try {
        return std::stoi(value);
    } catch (...) {
        return default_value;
    }
}

}  // namespace

void register_metrics_endpoints_impl(
    crow::SimpleApp& app,
    std::shared_ptr<rest_server_context> ctx) {

    // GET /api/health/database - Database health check
    CROW_ROUTE(app, "/api/health/database")
        .methods(crow::HTTPMethod::GET)([ctx]([[maybe_unused]] const crow::request& req) {
            crow::response res;
            res.add_header("Content-Type", "application/json");
            add_cors_headers(res, *ctx);

            // Check if database metrics service is available
            if (!ctx->database_metrics) {
                res.body = make_error_json(
                    "METRICS_UNAVAILABLE",
                    "Database metrics service not configured");
                res.code = 503;
                return res;
            }

            // Perform health check
            auto health = ctx->database_metrics->check_health();

            // Build JSON response
            std::ostringstream oss;
            oss << R"({"status":")" << health_status_to_json_string(health.current_status)
                << R"(","message":")" << json_escape(health.message)
                << R"(","response_time_ms":)" << health.response_time.count()
                << R"(,"active_connections":)" << health.active_connections
                << R"(,"error_rate":)" << health.error_rate;

            if (!health.warnings.empty()) {
                oss << R"(,"warnings":[)";
                for (size_t i = 0; i < health.warnings.size(); ++i) {
                    if (i > 0) oss << ",";
                    oss << R"(")" << json_escape(health.warnings[i]) << R"(")";
                }
                oss << "]";
            }

            oss << "}";
            res.body = oss.str();

            // Set status code based on health
            if (health.current_status ==
                services::monitoring::database_health::status::healthy) {
                res.code = 200;
            } else if (health.current_status ==
                       services::monitoring::database_health::status::degraded) {
                res.code = 200;  // Degraded but still operational
            } else {
                res.code = 503;  // Unhealthy - service unavailable
            }

            return res;
        });

    // GET /api/metrics/database - Current metrics in JSON format
    CROW_ROUTE(app, "/api/metrics/database")
        .methods(crow::HTTPMethod::GET)([ctx]([[maybe_unused]] const crow::request& req) {
            crow::response res;
            res.add_header("Content-Type", "application/json");
            add_cors_headers(res, *ctx);

            if (!ctx->database_metrics) {
                res.body = make_error_json(
                    "METRICS_UNAVAILABLE",
                    "Database metrics service not configured");
                res.code = 503;
                return res;
            }

            auto metrics = ctx->database_metrics->get_current_metrics();

            // Build JSON response
            std::ostringstream oss;
            oss << R"({"total_queries":)" << metrics.total_queries
                << R"(,"successful_queries":)" << metrics.successful_queries
                << R"(,"failed_queries":)" << metrics.failed_queries
                << R"(,"queries_per_second":)" << metrics.queries_per_second
                << R"(,"latency":{)"
                << R"("avg_us":)" << metrics.avg_latency_us
                << R"(,"min_us":)" << metrics.min_latency_us
                << R"(,"max_us":)" << metrics.max_latency_us
                << R"(,"p95_us":)" << metrics.p95_latency_us
                << R"(,"p99_us":)" << metrics.p99_latency_us << R"(})"
                << R"(,"connections":{)"
                << R"("active":)" << metrics.active_connections
                << R"(,"pool_size":)" << metrics.pool_size
                << R"(,"utilization":)" << metrics.connection_utilization << R"(})"
                << R"(,"error_rate":)" << metrics.error_rate
                << R"(,"slow_query_count":)" << metrics.slow_query_count << "}";

            res.body = oss.str();
            res.code = 200;
            return res;
        });

    // GET /api/metrics/database/slow-queries - Slow query list
    CROW_ROUTE(app, "/api/metrics/database/slow-queries")
        .methods(crow::HTTPMethod::GET)([ctx](const crow::request& req) {
            crow::response res;
            res.add_header("Content-Type", "application/json");
            add_cors_headers(res, *ctx);

            if (!ctx->database_metrics) {
                res.body = make_error_json(
                    "METRICS_UNAVAILABLE",
                    "Database metrics service not configured");
                res.code = 503;
                return res;
            }

            // Parse query parameters
            int limit = get_query_param_int(req, "limit", 10);
            int since_minutes = get_query_param_int(req, "since_minutes", 5);

            auto slow_queries = ctx->database_metrics->get_slow_queries(
                std::chrono::minutes(since_minutes));

            // Build JSON array
            std::ostringstream oss;
            oss << "[";

            size_t count = 0;
            for (const auto& sq : slow_queries) {
                if (count >= static_cast<size_t>(limit)) break;

                if (count > 0) oss << ",";

                oss << R"({"query_preview":")" << json_escape(sq.query_preview)
                    << R"(","duration_us":)" << sq.duration_us
                    << R"(,"timestamp":")" << sq.timestamp
                    << R"(","rows_affected":)" << sq.rows_affected << "}";

                ++count;
            }

            oss << "]";
            res.body = oss.str();
            res.code = 200;
            return res;
        });

    // GET /metrics - Prometheus format
    CROW_ROUTE(app, "/metrics")
        .methods(crow::HTTPMethod::GET)([ctx]([[maybe_unused]] const crow::request& req) {
            crow::response res;
            add_cors_headers(res, *ctx);

            if (!ctx->database_metrics) {
                res.code = 503;
                res.add_header("Content-Type", "text/plain");
                res.body = "# Database metrics unavailable\n";
                return res;
            }

            auto prometheus_output =
                ctx->database_metrics->export_prometheus_metrics();
            res.add_header("Content-Type", "text/plain; version=0.0.4");
            res.body = prometheus_output;
            res.code = 200;
            return res;
        });
}

}  // namespace pacs::web::endpoints

#endif  // PACS_WITH_DATABASE_SYSTEM
