// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file association_endpoints.cpp
 * @brief DICOM Association API endpoints implementation
 *
 * @copyright Copyright (c) 2025
 * @license MIT
 */

// IMPORTANT: Include Crow FIRST before any PACS headers to avoid forward
// declaration conflicts
#include "crow.h"

// Workaround for Windows: DELETE is defined as a macro in <winnt.h>
// which conflicts with crow::HTTPMethod::DELETE
#ifdef DELETE
#undef DELETE
#endif

#include "pacs/network/dicom_server.hpp"
#include "pacs/network/server_config.hpp"
#include "pacs/web/endpoints/association_endpoints.hpp"
#include "pacs/web/endpoints/system_endpoints.hpp"
#include "pacs/web/rest_config.hpp"
#include "pacs/web/rest_types.hpp"

#include <sstream>

namespace kcenon::pacs::web::endpoints {

namespace {

/**
 * @brief Add CORS headers to response
 */
void add_cors_headers(crow::response &res, const rest_server_context &ctx) {
  if (ctx.config && !ctx.config->cors_allowed_origins.empty()) {
    res.add_header("Access-Control-Allow-Origin",
                   ctx.config->cors_allowed_origins);
  }
}

}  // namespace

// Internal implementation function called from rest_server.cpp
void register_association_endpoints_impl(
    crow::SimpleApp &app,
    std::shared_ptr<rest_server_context> ctx) {
  // GET /api/v1/associations/active - List active DICOM associations
  CROW_ROUTE(app, "/api/v1/associations/active")
      .methods(crow::HTTPMethod::GET)([ctx](const crow::request & /*req*/) {
        crow::response res;
        res.add_header("Content-Type", "application/json");
        add_cors_headers(res, *ctx);

        if (!ctx->dicom_server) {
            res.code = 503;
            res.body = make_error_json("SERVICE_UNAVAILABLE",
                                       "DICOM server not configured");
            return res;
        }

        auto stats = ctx->dicom_server->get_statistics();
        auto active_count = ctx->dicom_server->active_associations();
        auto uptime_sec = stats.uptime().count();

        std::ostringstream oss;
        oss << R"({"active_count":)" << active_count
            << R"(,"total_associations":)" << stats.total_associations
            << R"(,"rejected_associations":)" << stats.rejected_associations
            << R"(,"messages_processed":)" << stats.messages_processed
            << R"(,"bytes_received":)" << stats.bytes_received
            << R"(,"bytes_sent":)" << stats.bytes_sent
            << R"(,"uptime_seconds":)" << uptime_sec
            << R"(,"server_running":)"
            << (ctx->dicom_server->is_running() ? "true" : "false")
            << '}';

        res.code = 200;
        res.body = oss.str();
        return res;
      });

  // DELETE /api/v1/associations/:id - Terminate a DICOM association
  CROW_ROUTE(app, "/api/v1/associations/<string>")
      .methods(crow::HTTPMethod::DELETE)(
          [ctx](const crow::request & /*req*/,
                const std::string &association_id) {
            crow::response res;
            res.add_header("Content-Type", "application/json");
            add_cors_headers(res, *ctx);

            if (association_id.empty()) {
              res.code = 400;
              res.body = make_error_json("INVALID_REQUEST",
                                         "Association ID is required");
              return res;
            }

            if (!ctx->dicom_server) {
              res.code = 503;
              res.body = make_error_json("SERVICE_UNAVAILABLE",
                                         "DICOM server not configured");
              return res;
            }

            // Individual association termination is not supported via the
            // dicom_server public API. The server manages association
            // lifecycles internally through idle timeouts and graceful
            // shutdown.
            res.code = 501;
            res.body = make_error_json(
                "NOT_IMPLEMENTED",
                "Individual association termination is not supported. "
                "Associations are managed by the DICOM server via idle "
                "timeouts and graceful shutdown.");
            return res;
          });

  // GET /api/v1/associations/:id - Get specific association details
  CROW_ROUTE(app, "/api/v1/associations/<string>")
      .methods(crow::HTTPMethod::GET)(
          [ctx](const crow::request & /*req*/,
                const std::string &association_id) {
            crow::response res;
            res.add_header("Content-Type", "application/json");
            add_cors_headers(res, *ctx);

            if (association_id.empty()) {
              res.code = 400;
              res.body = make_error_json("INVALID_REQUEST",
                                         "Association ID is required");
              return res;
            }

            if (!ctx->dicom_server) {
              res.code = 503;
              res.body = make_error_json("SERVICE_UNAVAILABLE",
                                         "DICOM server not configured");
              return res;
            }

            // Individual association lookup is not supported via the
            // dicom_server public API. Use GET /associations/active for
            // aggregate statistics.
            res.code = 501;
            res.body = make_error_json(
                "NOT_IMPLEMENTED",
                "Individual association lookup is not supported. "
                "Use GET /api/v1/associations/active for aggregate "
                "statistics.");
            return res;
          });
}

}  // namespace kcenon::pacs::web::endpoints
