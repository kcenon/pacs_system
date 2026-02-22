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
