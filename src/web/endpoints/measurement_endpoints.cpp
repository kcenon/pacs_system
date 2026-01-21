/**
 * @file measurement_endpoints.cpp
 * @brief Measurement API endpoints implementation
 *
 * @see Issue #545 - Implement Annotation & Measurement APIs
 * @see Issue #582 - Part 2: Annotation & Measurement REST Endpoints
 *
 * @copyright Copyright (c) 2025
 * @license MIT
 */

// IMPORTANT: Include Crow FIRST before any PACS headers to avoid forward
// declaration conflicts
#include "crow.h"

#include "pacs/storage/index_database.hpp"
#include "pacs/storage/measurement_record.hpp"
#include "pacs/storage/measurement_repository.hpp"
#include "pacs/web/endpoints/measurement_endpoints.hpp"
#include "pacs/web/endpoints/system_endpoints.hpp"
#include "pacs/web/rest_config.hpp"
#include "pacs/web/rest_types.hpp"

#include <chrono>
#include <iomanip>
#include <random>
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
 * @brief Generate a UUID for measurement_id
 */
std::string generate_uuid() {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::uniform_int_distribution<> dis(0, 15);
  static const char *hex = "0123456789abcdef";

  std::string uuid = "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx";
  for (char &c : uuid) {
    if (c == 'x') {
      c = hex[dis(gen)];
    } else if (c == 'y') {
      c = hex[(dis(gen) & 0x3) | 0x8];
    }
  }
  return uuid;
}

/**
 * @brief Format time_point as ISO 8601 string
 */
std::string format_timestamp(
    const std::chrono::system_clock::time_point &tp) {
  auto time = std::chrono::system_clock::to_time_t(tp);
  std::ostringstream oss;
  oss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
  return oss.str();
}

/**
 * @brief Convert measurement_record to JSON string
 */
std::string measurement_to_json(const storage::measurement_record &meas) {
  std::ostringstream oss;
  oss << R"({"measurement_id":")" << json_escape(meas.measurement_id)
      << R"(","sop_instance_uid":")" << json_escape(meas.sop_instance_uid)
      << R"(","frame_number":)";
  if (meas.frame_number.has_value()) {
    oss << meas.frame_number.value();
  } else {
    oss << "null";
  }
  oss << R"(,"user_id":")" << json_escape(meas.user_id)
      << R"(","measurement_type":")" << json_escape(to_string(meas.type))
      << R"(","geometry":)" << meas.geometry_json
      << R"(,"value":)" << meas.value
      << R"(,"unit":")" << json_escape(meas.unit)
      << R"(","label":")" << json_escape(meas.label)
      << R"(","created_at":")" << format_timestamp(meas.created_at) << R"("})";
  return oss.str();
}

/**
 * @brief Convert vector of measurements to JSON array string
 */
std::string measurements_to_json(
    const std::vector<storage::measurement_record> &measurements,
    size_t total_count) {
  std::ostringstream oss;
  oss << R"({"data":[)";
  for (size_t i = 0; i < measurements.size(); ++i) {
    if (i > 0) {
      oss << ",";
    }
    oss << measurement_to_json(measurements[i]);
  }
  oss << R"(],"pagination":{"total":)" << total_count << R"(,"count":)"
      << measurements.size() << "}}";
  return oss.str();
}

/**
 * @brief Parse pagination parameters from request
 */
std::pair<size_t, size_t> parse_pagination(const crow::request &req) {
  size_t limit = 20;
  size_t offset = 0;

  auto limit_param = req.url_params.get("limit");
  if (limit_param) {
    try {
      limit = std::stoul(limit_param);
      if (limit > 100) {
        limit = 100;
      }
    } catch (...) {
      // Use default
    }
  }

  auto offset_param = req.url_params.get("offset");
  if (offset_param) {
    try {
      offset = std::stoul(offset_param);
    } catch (...) {
      // Use default
    }
  }

  return {limit, offset};
}

/**
 * @brief Parse JSON string to find a string value
 */
std::string parse_json_string(const std::string &json, const std::string &key) {
  std::string search = "\"" + key + "\":\"";
  auto pos = json.find(search);
  if (pos == std::string::npos) {
    return "";
  }
  pos += search.length();
  auto end = json.find("\"", pos);
  if (end == std::string::npos) {
    return "";
  }
  return json.substr(pos, end - pos);
}

/**
 * @brief Parse JSON string to find an integer value
 */
std::optional<int> parse_json_int(const std::string &json,
                                  const std::string &key) {
  std::string search = "\"" + key + "\":";
  auto pos = json.find(search);
  if (pos == std::string::npos) {
    return std::nullopt;
  }
  pos += search.length();
  while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) {
    ++pos;
  }
  if (pos >= json.size()) {
    return std::nullopt;
  }
  if (json.substr(pos, 4) == "null") {
    return std::nullopt;
  }
  try {
    return std::stoi(json.substr(pos));
  } catch (...) {
    return std::nullopt;
  }
}

/**
 * @brief Parse JSON string to find a double value
 */
double parse_json_double(const std::string &json, const std::string &key) {
  std::string search = "\"" + key + "\":";
  auto pos = json.find(search);
  if (pos == std::string::npos) {
    return 0.0;
  }
  pos += search.length();
  while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) {
    ++pos;
  }
  if (pos >= json.size()) {
    return 0.0;
  }
  try {
    return std::stod(json.substr(pos));
  } catch (...) {
    return 0.0;
  }
}

/**
 * @brief Parse JSON object value (returns raw JSON)
 */
std::string parse_json_object(const std::string &json, const std::string &key) {
  std::string search = "\"" + key + "\":";
  auto pos = json.find(search);
  if (pos == std::string::npos) {
    return "{}";
  }
  pos += search.length();
  while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) {
    ++pos;
  }
  if (pos >= json.size() || json[pos] != '{') {
    return "{}";
  }

  int depth = 0;
  size_t start = pos;
  for (; pos < json.size(); ++pos) {
    if (json[pos] == '{') {
      ++depth;
    } else if (json[pos] == '}') {
      --depth;
      if (depth == 0) {
        return json.substr(start, pos - start + 1);
      }
    }
  }
  return "{}";
}

} // namespace

// Internal implementation function called from rest_server.cpp
void register_measurement_endpoints_impl(crow::SimpleApp &app,
                                         std::shared_ptr<rest_server_context> ctx) {
  // POST /api/v1/measurements - Create measurement
  CROW_ROUTE(app, "/api/v1/measurements")
      .methods(crow::HTTPMethod::POST)([ctx](const crow::request &req) {
        crow::response res;
        res.add_header("Content-Type", "application/json");
        add_cors_headers(res, *ctx);

        if (!ctx->database) {
          res.code = 503;
          res.body =
              make_error_json("DATABASE_UNAVAILABLE", "Database not configured");
          return res;
        }

        std::string body = req.body;
        if (body.empty()) {
          res.code = 400;
          res.body = make_error_json("INVALID_REQUEST", "Request body is empty");
          return res;
        }

        storage::measurement_record meas;
        meas.measurement_id = generate_uuid();
        meas.sop_instance_uid = parse_json_string(body, "sop_instance_uid");
        meas.frame_number = parse_json_int(body, "frame_number");
        meas.user_id = parse_json_string(body, "user_id");

        auto type_str = parse_json_string(body, "measurement_type");
        auto type_opt = storage::measurement_type_from_string(type_str);
        if (!type_opt.has_value()) {
          res.code = 400;
          res.body = make_error_json("INVALID_TYPE", "Invalid measurement type");
          return res;
        }
        meas.type = type_opt.value();

        meas.geometry_json = parse_json_object(body, "geometry");
        meas.value = parse_json_double(body, "value");
        meas.unit = parse_json_string(body, "unit");
        meas.label = parse_json_string(body, "label");
        meas.created_at = std::chrono::system_clock::now();

        if (meas.sop_instance_uid.empty()) {
          res.code = 400;
          res.body =
              make_error_json("MISSING_FIELD", "sop_instance_uid is required");
          return res;
        }

        storage::measurement_repository repo(ctx->database->native_handle());
        auto save_result = repo.save(meas);
        if (!save_result.is_ok()) {
          res.code = 500;
          res.body =
              make_error_json("SAVE_ERROR", save_result.error().message);
          return res;
        }

        res.code = 201;
        std::ostringstream oss;
        oss << R"({"measurement_id":")" << json_escape(meas.measurement_id)
            << R"(","value":)" << meas.value << R"(,"unit":")"
            << json_escape(meas.unit) << R"("})";
        res.body = oss.str();
        return res;
      });

  // GET /api/v1/measurements - List measurements
  CROW_ROUTE(app, "/api/v1/measurements")
      .methods(crow::HTTPMethod::GET)([ctx](const crow::request &req) {
        crow::response res;
        res.add_header("Content-Type", "application/json");
        add_cors_headers(res, *ctx);

        if (!ctx->database) {
          res.code = 503;
          res.body =
              make_error_json("DATABASE_UNAVAILABLE", "Database not configured");
          return res;
        }

        auto [limit, offset] = parse_pagination(req);

        storage::measurement_query query;
        query.limit = limit;
        query.offset = offset;

        auto sop_instance_uid = req.url_params.get("sop_instance_uid");
        if (sop_instance_uid) {
          query.sop_instance_uid = sop_instance_uid;
        }
        auto study_uid = req.url_params.get("study_uid");
        if (study_uid) {
          query.study_uid = study_uid;
        }
        auto user_id = req.url_params.get("user_id");
        if (user_id) {
          query.user_id = user_id;
        }
        auto measurement_type = req.url_params.get("measurement_type");
        if (measurement_type) {
          auto type_opt = storage::measurement_type_from_string(measurement_type);
          if (type_opt.has_value()) {
            query.type = type_opt.value();
          }
        }

        storage::measurement_repository repo(ctx->database->native_handle());

        storage::measurement_query count_query = query;
        count_query.limit = 0;
        count_query.offset = 0;
        size_t total_count = repo.count(count_query);

        auto measurements = repo.search(query);

        res.code = 200;
        res.body = measurements_to_json(measurements, total_count);
        return res;
      });

  // GET /api/v1/measurements/<measurementId> - Get measurement by ID
  CROW_ROUTE(app, "/api/v1/measurements/<string>")
      .methods(crow::HTTPMethod::GET)(
          [ctx](const crow::request & /*req*/, const std::string &measurement_id) {
            crow::response res;
            res.add_header("Content-Type", "application/json");
            add_cors_headers(res, *ctx);

            if (!ctx->database) {
              res.code = 503;
              res.body = make_error_json("DATABASE_UNAVAILABLE",
                                         "Database not configured");
              return res;
            }

            storage::measurement_repository repo(ctx->database->native_handle());
            auto meas = repo.find_by_id(measurement_id);
            if (!meas.has_value()) {
              res.code = 404;
              res.body = make_error_json("NOT_FOUND", "Measurement not found");
              return res;
            }

            res.code = 200;
            res.body = measurement_to_json(meas.value());
            return res;
          });

  // DELETE /api/v1/measurements/<measurementId> - Delete measurement
  CROW_ROUTE(app, "/api/v1/measurements/<string>")
      .methods(crow::HTTPMethod::DELETE)(
          [ctx](const crow::request & /*req*/, const std::string &measurement_id) {
            crow::response res;
            add_cors_headers(res, *ctx);

            if (!ctx->database) {
              res.code = 503;
              res.add_header("Content-Type", "application/json");
              res.body = make_error_json("DATABASE_UNAVAILABLE",
                                         "Database not configured");
              return res;
            }

            storage::measurement_repository repo(ctx->database->native_handle());
            if (!repo.exists(measurement_id)) {
              res.code = 404;
              res.add_header("Content-Type", "application/json");
              res.body = make_error_json("NOT_FOUND", "Measurement not found");
              return res;
            }

            auto remove_result = repo.remove(measurement_id);
            if (!remove_result.is_ok()) {
              res.code = 500;
              res.add_header("Content-Type", "application/json");
              res.body =
                  make_error_json("DELETE_ERROR", remove_result.error().message);
              return res;
            }

            res.code = 204;
            return res;
          });

  // GET /api/v1/instances/<sopInstanceUid>/measurements - Get measurements for instance
  CROW_ROUTE(app, "/api/v1/instances/<string>/measurements")
      .methods(crow::HTTPMethod::GET)(
          [ctx](const crow::request & /*req*/,
                const std::string &sop_instance_uid) {
            crow::response res;
            res.add_header("Content-Type", "application/json");
            add_cors_headers(res, *ctx);

            if (!ctx->database) {
              res.code = 503;
              res.body = make_error_json("DATABASE_UNAVAILABLE",
                                         "Database not configured");
              return res;
            }

            storage::measurement_repository repo(ctx->database->native_handle());
            auto measurements = repo.find_by_instance(sop_instance_uid);

            std::ostringstream oss;
            oss << R"({"data":[)";
            for (size_t i = 0; i < measurements.size(); ++i) {
              if (i > 0) {
                oss << ",";
              }
              oss << measurement_to_json(measurements[i]);
            }
            oss << "]}";

            res.code = 200;
            res.body = oss.str();
            return res;
          });
}

} // namespace pacs::web::endpoints
