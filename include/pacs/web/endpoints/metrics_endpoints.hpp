/**
 * @file metrics_endpoints.hpp
 * @brief Database metrics REST API endpoints
 *
 * Provides HTTP endpoints for database monitoring and observability:
 * - GET /api/health/database - Database health check
 * - GET /api/metrics/database - JSON metrics
 * - GET /api/metrics/database/slow-queries - Slow query list
 * - GET /metrics - Prometheus format metrics
 *
 * @see Issue #611 - Phase 5: Monitoring & Observability Integration
 *
 * @copyright Copyright (c) 2025
 * @license MIT
 */

#pragma once

#include "pacs/services/monitoring/database_metrics_service.hpp"

#ifdef PACS_WITH_DATABASE_SYSTEM

#include "crow.h"

#include <memory>

namespace pacs::web {
struct rest_server_context;
}

namespace pacs::web::endpoints {

/**
 * @brief Register database metrics endpoints
 *
 * Registers all database monitoring endpoints with the Crow application.
 *
 * Endpoints:
 * - GET /api/health/database - Health check with status code indication
 * - GET /api/metrics/database - Current metrics in JSON format
 * - GET /api/metrics/database/slow-queries?limit=10&since_minutes=5 - Slow queries
 * - GET /metrics - Prometheus text exposition format
 *
 * @param app Crow application instance
 * @param ctx REST server context containing metrics service
 */
void register_metrics_endpoints_impl(
    crow::SimpleApp& app,
    std::shared_ptr<rest_server_context> ctx);

}  // namespace pacs::web::endpoints

#endif  // PACS_WITH_DATABASE_SYSTEM
