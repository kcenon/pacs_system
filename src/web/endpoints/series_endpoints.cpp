/**
 * @file series_endpoints.cpp
 * @brief Series API endpoints implementation
 *
 * @copyright Copyright (c) 2025
 * @license MIT
 */

// IMPORTANT: Include Crow FIRST before any PACS headers to avoid forward
// declaration conflicts
#include "crow.h"

#include "pacs/storage/index_database.hpp"
#include "pacs/storage/instance_record.hpp"
#include "pacs/storage/series_record.hpp"
#include "pacs/web/endpoints/series_endpoints.hpp"
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

/**
 * @brief Convert series_record to JSON string
 */
std::string series_to_json(const storage::series_record &series) {
  std::ostringstream oss;
  oss << R"({"pk":)" << series.pk << R"(,"study_pk":)" << series.study_pk
      << R"(,"series_instance_uid":")" << json_escape(series.series_uid)
      << R"(","modality":")" << json_escape(series.modality)
      << R"(","series_number":)";
  if (series.series_number) {
    oss << *series.series_number;
  } else {
    oss << "null";
  }
  oss << R"(,"series_description":")" << json_escape(series.series_description)
      << R"(","body_part_examined":")" << json_escape(series.body_part_examined)
      << R"(","station_name":")" << json_escape(series.station_name)
      << R"(","num_instances":)" << series.num_instances << "}";
  return oss.str();
}

/**
 * @brief Convert instance_record to JSON string
 */
std::string instance_to_json(const storage::instance_record &instance) {
  std::ostringstream oss;
  oss << R"({"pk":)" << instance.pk << R"(,"series_pk":)" << instance.series_pk
      << R"(,"sop_instance_uid":")" << json_escape(instance.sop_uid)
      << R"(","sop_class_uid":")" << json_escape(instance.sop_class_uid)
      << R"(","transfer_syntax":")" << json_escape(instance.transfer_syntax)
      << R"(","instance_number":)";
  if (instance.instance_number) {
    oss << *instance.instance_number;
  } else {
    oss << "null";
  }
  oss << R"(,"file_size":)" << instance.file_size << "}";
  return oss.str();
}

/**
 * @brief Convert vector of instance_records to JSON array string
 */
std::string instances_to_json(const std::vector<storage::instance_record> &instances) {
  std::ostringstream oss;
  oss << R"({"data":[)";
  for (size_t i = 0; i < instances.size(); ++i) {
    if (i > 0) {
      oss << ",";
    }
    oss << instance_to_json(instances[i]);
  }
  oss << R"(],"count":)" << instances.size() << "}";
  return oss.str();
}

} // namespace

// Internal implementation function called from rest_server.cpp
void register_series_endpoints_impl(crow::SimpleApp &app,
                                    std::shared_ptr<rest_server_context> ctx) {
  // GET /api/v1/series/:uid - Get series details
  CROW_ROUTE(app, "/api/v1/series/<string>")
      .methods(crow::HTTPMethod::GET)(
          [ctx](const crow::request & /*req*/, const std::string &series_uid) {
            crow::response res;
            res.add_header("Content-Type", "application/json");
            add_cors_headers(res, *ctx);

            if (!ctx->database) {
              res.code = 503;
              res.body = make_error_json("DATABASE_UNAVAILABLE",
                                         "Database not configured");
              return res;
            }

            auto series = ctx->database->find_series(series_uid);
            if (!series) {
              res.code = 404;
              res.body = make_error_json("NOT_FOUND", "Series not found");
              return res;
            }

            res.code = 200;
            res.body = series_to_json(*series);
            return res;
          });

  // GET /api/v1/series/:uid/instances - Get series instances
  CROW_ROUTE(app, "/api/v1/series/<string>/instances")
      .methods(crow::HTTPMethod::GET)(
          [ctx](const crow::request & /*req*/, const std::string &series_uid) {
            crow::response res;
            res.add_header("Content-Type", "application/json");
            add_cors_headers(res, *ctx);

            if (!ctx->database) {
              res.code = 503;
              res.body = make_error_json("DATABASE_UNAVAILABLE",
                                         "Database not configured");
              return res;
            }

            // Verify series exists
            auto series = ctx->database->find_series(series_uid);
            if (!series) {
              res.code = 404;
              res.body = make_error_json("NOT_FOUND", "Series not found");
              return res;
            }

            auto instances = ctx->database->list_instances(series_uid);

            res.code = 200;
            res.body = instances_to_json(instances);
            return res;
          });
}

} // namespace pacs::web::endpoints
