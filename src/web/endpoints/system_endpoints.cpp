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

#include "pacs/web/endpoints/system_endpoints.hpp"
#include "pacs/web/rest_config.hpp"
#include "pacs/web/rest_types.hpp"

#ifdef PACS_WITH_MONITORING
#include "pacs/monitoring/health_checker.hpp"
#include "pacs/monitoring/health_json.hpp"
#include "pacs/monitoring/pacs_metrics.hpp"
#endif

#include <sstream>

namespace pacs::web::endpoints {

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

} // namespace

// Internal implementation function called from rest_server.cpp
void register_system_endpoints_impl(crow::SimpleApp &app,
                                    std::shared_ptr<rest_server_context> ctx) {
  // GET /api/v1/system/status - System health status
  CROW_ROUTE(app, "/api/v1/system/status")
      .methods(crow::HTTPMethod::GET)([ctx]() {
        crow::response res;
        res.add_header("Content-Type", "application/json");
        add_cors_headers(res, *ctx);

#ifdef PACS_WITH_MONITORING
        if (ctx->health_checker) {
          auto status = ctx->health_checker->get_status();
          // to_json is a free function in pacs::monitoring namespace
          res.body = monitoring::to_json(status);
          res.code = 200;
        } else {
          res.body =
              R"({"status":"unknown","message":"Health checker not configured"})";
          res.code = 503;
        }
#else
        // Basic status without monitoring module
        res.body =
            R"({"status":"healthy","message":"REST API server running","version":"1.0.0"})";
        res.code = 200;
#endif
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
      .methods(crow::HTTPMethod::PUT)([ctx](const crow::request &req) {
        crow::response res;
        res.add_header("Content-Type", "application/json");
        add_cors_headers(res, *ctx);

        // Validate content type
        auto content_type = req.get_header_value("Content-Type");
        if (content_type.find("application/json") == std::string::npos) {
          res.body = make_error_json("INVALID_CONTENT_TYPE",
                                     "Content-Type must be application/json");
          res.code = 415;
          return res;
        }

        // Validate body is not empty
        if (req.body.empty()) {
          res.body = make_error_json("EMPTY_BODY", "Request body is required");
          res.code = 400;
          return res;
        }

        // Note: Full configuration update would require parsing JSON and
        // updating the config. For now, we return a placeholder success
        // response. Actual implementation would need a callback or direct
        // config reference.
        res.body = make_success_json("Configuration update acknowledged");
        res.code = 200;
        return res;
      });

  // GET /api/v1/system/version - API version info
  CROW_ROUTE(app, "/api/v1/system/version").methods(crow::HTTPMethod::GET)([ctx]() {
    crow::response res;
    res.add_header("Content-Type", "application/json");
    add_cors_headers(res, *ctx);

    res.body =
        R"({"api_version":"v1","pacs_version":"1.2.0","crow_version":"1.2.0"})";
    res.code = 200;
    return res;
  });
}

} // namespace pacs::web::endpoints
