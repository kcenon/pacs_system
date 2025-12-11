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

#include "pacs/web/endpoints/association_endpoints.hpp"
#include "pacs/web/endpoints/system_endpoints.hpp"
#include "pacs/web/rest_config.hpp"
#include "pacs/web/rest_types.hpp"

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

        // Note: This endpoint requires integration with the DICOM server
        // to get real-time association data. Currently returns empty list.
        // TODO: Integrate with dicom_server or association_registry when available

        std::ostringstream oss;
        oss << R"({"data":[],"count":0})";

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

            // Note: This endpoint requires integration with the DICOM server
            // to actually terminate associations.
            // TODO: Integrate with dicom_server when available

            if (association_id.empty()) {
              res.code = 400;
              res.body = make_error_json("INVALID_REQUEST",
                                         "Association ID is required");
              return res;
            }

            // Currently not implemented - would need DICOM server integration
            res.code = 501;
            res.body = make_error_json(
                "NOT_IMPLEMENTED",
                "Association termination requires DICOM server integration");
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

            // Note: This endpoint requires integration with the DICOM server
            // TODO: Integrate with dicom_server when available

            res.code = 404;
            res.body = make_error_json("NOT_FOUND", "Association not found");
            return res;
          });
}

}  // namespace pacs::web::endpoints
