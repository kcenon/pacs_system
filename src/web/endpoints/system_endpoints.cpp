// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file system_endpoints.cpp
 * @brief System API endpoints implementation
 *
 * @copyright Copyright (c) 2025
 * @license MIT
 */

// IMPORTANT: Include Crow FIRST before any PACS headers to avoid forward
// declaration conflicts
#include "crow.h"

#include "../rest_app.h"

#include "kcenon/pacs/security/access_control_manager.h"
#include "kcenon/pacs/web/auth/oauth2_middleware.h"
#include "kcenon/pacs/security/user.h"
#include "kcenon/pacs/web/endpoints/system_endpoints.h"
#include "kcenon/pacs/web/rest_config.h"
#include "kcenon/pacs/web/rest_types.h"

#ifdef PACS_WITH_MONITORING
#include "kcenon/pacs/monitoring/health_checker.h"
#include "kcenon/pacs/monitoring/health_json.h"
#include "kcenon/pacs/monitoring/pacs_metrics.h"
#endif

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

/**
 * @brief Build config JSON from rest_server_config
 */
std::string config_to_json(const rest_server_config &config) {
  std::ostringstream oss;
  oss << R"({"bind_address":")" << config.bind_address << R"(",)"
      << R"("port":)" << config.port << R"(,)"
      << R"("concurrency":)" << config.concurrency << R"(,)"
      << R"("enable_cors":)" << (config.enable_cors ? "true" : "false")
      << R"(,)"
      << R"("cors_allowed_origins":")" << config.cors_allowed_origins << R"(",)"
      << R"("enable_tls":)" << (config.enable_tls ? "true" : "false") << R"(,)"
      << R"("request_timeout_seconds":)" << config.request_timeout_seconds
      << R"(,)"
      << R"("max_body_size":)" << config.max_body_size << "}";
  return oss.str();
}

// Helper to check authentication and permission
// Returns true if allowed, false if denied (and sets response)
bool check_permission(const std::shared_ptr<rest_server_context> &ctx,
                      const crow::request &req, crow::response &res,
                      security::ResourceType resource, uint32_t action) {
  // rest_gate has checked the token and its scopes. X-User-ID is only what the client says.
  if (ctx->oauth2 && ctx->oauth2->enabled()) {
    return true;
  }

  if (!ctx->security_manager) {
    // Fail open or closed? Closed is safer, but for dev maybe open if not
    // configured? Let's fail closed.
    res.code = 503;
    res.body = make_error_json("SECURITY_UNAVAILABLE",
                               "Security manager not configured");
    return false;
  }

  // Simple Auth via Header for now (Integration step)
  auto user_id_header = req.get_header_value("X-User-ID");
  if (user_id_header.empty()) {
    res.code = 401;
    res.body = make_error_json("UNAUTHORIZED", "Missing X-User-ID header");
    return false;
  }

  std::string user_id = user_id_header;
  auto user_res = ctx->security_manager->get_user(user_id);
  if (user_res.is_err()) {
    res.code = 401;
    res.body = make_error_json("UNAUTHORIZED", "Invalid user");
    return false;
  }

  auto user = user_res.unwrap();
  if (!ctx->security_manager->check_permission(user, resource, action)) {
    res.code = 403;
    res.body = make_error_json("FORBIDDEN", "Insufficient permissions");
    return false;
  }

  return true;
}

} // namespace

// Internal implementation function called from rest_server.cpp
void register_system_endpoints_impl(rest_app &app,
                                    std::shared_ptr<rest_server_context> ctx) {
  // GET /api/v1/system/status - System health status
  CROW_ROUTE(app, "/api/v1/system/status")
      .methods(crow::HTTPMethod::GET)([ctx](const crow::request &req) {
        crow::response res;
        res.add_header("Content-Type", "application/json");
        add_cors_headers(res, *ctx);

        // Permission Check: System Read
        if (!check_permission(ctx, req, res, security::ResourceType::System,
                              security::Action::Read)) {
          return res;
        }

        crow::json::wvalue status_json;
        status_json["status"] = "unknown";                        // Default
        status_json["message"] = "Health checker not configured"; // Default
        status_json["version"] = PACS_SYSTEM_VERSION;             // Default

#ifdef PACS_WITH_MONITORING
        if (ctx->health_checker) {
          auto status = ctx->health_checker->get_status();
          // to_json is a free function in kcenon::pacs::monitoring namespace
          status_json = crow::json::load(
              monitoring::to_json(status)); // Parse existing JSON into wvalue
          status_json["version"] = PACS_SYSTEM_VERSION; // Add version
        } else {
          status_json["status"] = "unknown";
          status_json["message"] = "Health checker not configured";
        }
#endif
        res.body = status_json.dump();
        res.code = 200;
        return res;
      });

  // GET /api/v1/system/metrics - Performance metrics
  CROW_ROUTE(app, "/api/v1/system/metrics").methods(crow::HTTPMethod::GET)([ctx]() {
    crow::response res;
    res.add_header("Content-Type", "application/json");
    add_cors_headers(res, *ctx);

#ifdef PACS_WITH_MONITORING
    if (ctx->metrics) {
      // pacs_metrics provides individual counters, build simple JSON
      std::ostringstream oss;
      oss << R"({"message":"Metrics available via pacs_metrics API"})";
      res.body = oss.str();
      res.code = 200;
    } else {
      res.body =
          R"({"error":{"code":"METRICS_UNAVAILABLE","message":"Metrics provider not configured"}})";
      res.code = 503;
    }
#else
    // Basic metrics without monitoring module
    res.body =
        R"({"uptime_seconds":0,"requests_total":0,"message":"Metrics module not available"})";
    res.code = 200;
#endif
    return res;
  });

  // GET /api/v1/system/config - Current configuration
  CROW_ROUTE(app, "/api/v1/system/config")
      .methods(crow::HTTPMethod::GET)([ctx]() {
        crow::response res;
        res.add_header("Content-Type", "application/json");
        add_cors_headers(res, *ctx);

        if (ctx->config) {
          res.body = config_to_json(*ctx->config);
          res.code = 200;
        } else {
          res.body = make_error_json("CONFIG_UNAVAILABLE",
                                     "Configuration not available");
          res.code = 500;
        }
        return res;
      });

  // PUT /api/v1/system/config - Update configuration
  CROW_ROUTE(app, "/api/v1/system/config")
      .methods(crow::HTTPMethod::PUT)([ctx](const crow::request & /*req*/) {
        crow::response res;
        res.add_header("Content-Type", "application/json");
        add_cors_headers(res, *ctx);

        // This route used to answer success and change nothing.
        res.body = make_error_json("NOT_IMPLEMENTED",
                                   "Settings are read when the server starts and cannot be changed here");
        res.code = 501;
        return res;
      });

  // GET /api/v1/system/version - API version info
  CROW_ROUTE(app, "/api/v1/system/version").methods(crow::HTTPMethod::GET)([ctx]() {
    crow::response res;
    res.add_header("Content-Type", "application/json");
    add_cors_headers(res, *ctx);

    res.body = std::string(R"({"api_version":"v1","pacs_version":")") + PACS_SYSTEM_VERSION +
               R"(","crow_version":")" + crow::VERSION + R"("})";
    res.code = 200;
    return res;
  });
}

} // namespace kcenon::pacs::web::endpoints
