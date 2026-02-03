/**
 * @file annotation_endpoints.cpp
 * @brief Annotation API endpoints implementation
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

// Workaround for Windows: DELETE is defined as a macro in <winnt.h>
// which conflicts with crow::HTTPMethod::DELETE
#ifdef DELETE
#undef DELETE
#endif

#include "pacs/storage/annotation_record.hpp"
#include "pacs/storage/annotation_repository.hpp"
#include "pacs/storage/index_database.hpp"
#include "pacs/web/endpoints/annotation_endpoints.hpp"
#include "pacs/web/endpoints/system_endpoints.hpp"
#include "pacs/web/rest_config.hpp"
#include "pacs/web/rest_types.hpp"

#include <chrono>
#include <iomanip>
#include <random>
#include <sstream>

namespace pacs::web::endpoints {

namespace {

#ifdef PACS_WITH_DATABASE_SYSTEM
/**
 * @brief Create annotation_repository from index_database
 * @note Uses database_adapter() for base_repository pattern
 */
inline auto make_annotation_repo(storage::index_database* db) {
    return storage::annotation_repository(db->db_adapter());
}

/**
 * @brief Search annotations (database_system version)
 */
inline std::vector<storage::annotation_record> search_annotations(
    storage::annotation_repository& repo,
    const storage::annotation_query& query) {
    auto result = repo.search(query);
    return result.is_ok() ? std::move(result.value()) : std::vector<storage::annotation_record>{};
}

/**
 * @brief Count annotations (database_system version)
 */
inline size_t count_annotations(
    storage::annotation_repository& repo,
    const storage::annotation_query& query) {
    auto result = repo.count_matching(query);
    return result.is_ok() ? result.value() : 0;
}

/**
 * @brief Find annotation by ID (database_system version)
 */
inline std::optional<storage::annotation_record> find_annotation(
    storage::annotation_repository& repo,
    const std::string& annotation_id) {
    auto result = repo.find_by_id(annotation_id);
    return result.is_ok() ? std::make_optional(std::move(result.value())) : std::nullopt;
}

/**
 * @brief Check if annotation exists (database_system version)
 */
inline bool annotation_exists(
    storage::annotation_repository& repo,
    const std::string& annotation_id) {
    auto result = repo.exists(annotation_id);
    return result.is_ok() && result.value();
}

/**
 * @brief Find annotations by instance (database_system version)
 */
inline std::vector<storage::annotation_record> find_by_instance(
    storage::annotation_repository& repo,
    const std::string& sop_instance_uid) {
    auto result = repo.find_by_instance(sop_instance_uid);
    return result.is_ok() ? std::move(result.value()) : std::vector<storage::annotation_record>{};
}
#else
/**
 * @brief Create annotation_repository from index_database
 * @note Uses native_handle() for legacy SQLite interface
 */
inline auto make_annotation_repo(storage::index_database* db) {
    return storage::annotation_repository(db->native_handle());
}

/**
 * @brief Search annotations (legacy version)
 */
inline std::vector<storage::annotation_record> search_annotations(
    storage::annotation_repository& repo,
    const storage::annotation_query& query) {
    return repo.search(query);
}

/**
 * @brief Count annotations (legacy version)
 */
inline size_t count_annotations(
    storage::annotation_repository& repo,
    const storage::annotation_query& query) {
    return repo.count(query);
}

/**
 * @brief Find annotation by ID (legacy version)
 */
inline std::optional<storage::annotation_record> find_annotation(
    storage::annotation_repository& repo,
    const std::string& annotation_id) {
    return repo.find_by_id(annotation_id);
}

/**
 * @brief Check if annotation exists (legacy version)
 */
inline bool annotation_exists(
    storage::annotation_repository& repo,
    const std::string& annotation_id) {
    return repo.exists(annotation_id);
}

/**
 * @brief Find annotations by instance (legacy version)
 */
inline std::vector<storage::annotation_record> find_by_instance(
    storage::annotation_repository& repo,
    const std::string& sop_instance_uid) {
    return repo.find_by_instance(sop_instance_uid);
}
#endif

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
 * @brief Generate a UUID for annotation_id
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
 * @brief Convert annotation_style to JSON string
 */
std::string style_to_json(const storage::annotation_style &style) {
  std::ostringstream oss;
  oss << R"({"color":")" << json_escape(style.color)
      << R"(","line_width":)" << style.line_width
      << R"(,"fill_color":")" << json_escape(style.fill_color)
      << R"(","fill_opacity":)" << style.fill_opacity
      << R"(,"font_family":")" << json_escape(style.font_family)
      << R"(","font_size":)" << style.font_size << "}";
  return oss.str();
}

/**
 * @brief Convert annotation_record to JSON string
 */
std::string annotation_to_json(const storage::annotation_record &ann) {
  std::ostringstream oss;
  oss << R"({"annotation_id":")" << json_escape(ann.annotation_id)
      << R"(","study_uid":")" << json_escape(ann.study_uid)
      << R"(","series_uid":")" << json_escape(ann.series_uid)
      << R"(","sop_instance_uid":")" << json_escape(ann.sop_instance_uid)
      << R"(","frame_number":)";
  if (ann.frame_number.has_value()) {
    oss << ann.frame_number.value();
  } else {
    oss << "null";
  }
  oss << R"(,"user_id":")" << json_escape(ann.user_id)
      << R"(","annotation_type":")" << json_escape(to_string(ann.type))
      << R"(","geometry":)" << ann.geometry_json
      << R"(,"text":")" << json_escape(ann.text)
      << R"(","style":)" << style_to_json(ann.style)
      << R"(,"created_at":")" << format_timestamp(ann.created_at)
      << R"(","updated_at":")" << format_timestamp(ann.updated_at) << R"("})";
  return oss.str();
}

/**
 * @brief Convert vector of annotations to JSON array string
 */
std::string annotations_to_json(
    const std::vector<storage::annotation_record> &annotations,
    size_t total_count) {
  std::ostringstream oss;
  oss << R"({"data":[)";
  for (size_t i = 0; i < annotations.size(); ++i) {
    if (i > 0) {
      oss << ",";
    }
    oss << annotation_to_json(annotations[i]);
  }
  oss << R"(],"pagination":{"total":)" << total_count << R"(,"count":)"
      << annotations.size() << "}}";
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

/**
 * @brief Parse style from JSON
 */
storage::annotation_style parse_style(const std::string &style_json) {
  storage::annotation_style style;
  auto color = parse_json_string(style_json, "color");
  if (!color.empty()) {
    style.color = color;
  }
  auto line_width = parse_json_int(style_json, "line_width");
  if (line_width.has_value()) {
    style.line_width = line_width.value();
  }
  auto fill_color = parse_json_string(style_json, "fill_color");
  style.fill_color = fill_color;

  std::string search = "\"fill_opacity\":";
  auto pos = style_json.find(search);
  if (pos != std::string::npos) {
    pos += search.length();
    try {
      style.fill_opacity = std::stof(style_json.substr(pos));
    } catch (...) {
      // Use default
    }
  }

  auto font_family = parse_json_string(style_json, "font_family");
  if (!font_family.empty()) {
    style.font_family = font_family;
  }
  auto font_size = parse_json_int(style_json, "font_size");
  if (font_size.has_value()) {
    style.font_size = font_size.value();
  }
  return style;
}

} // namespace

// Internal implementation function called from rest_server.cpp
void register_annotation_endpoints_impl(crow::SimpleApp &app,
                                        std::shared_ptr<rest_server_context> ctx) {
  // POST /api/v1/annotations - Create annotation
  CROW_ROUTE(app, "/api/v1/annotations")
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

        storage::annotation_record ann;
        ann.annotation_id = generate_uuid();
        ann.study_uid = parse_json_string(body, "study_uid");
        ann.series_uid = parse_json_string(body, "series_uid");
        ann.sop_instance_uid = parse_json_string(body, "sop_instance_uid");
        ann.frame_number = parse_json_int(body, "frame_number");
        ann.user_id = parse_json_string(body, "user_id");

        auto type_str = parse_json_string(body, "annotation_type");
        auto type_opt = storage::annotation_type_from_string(type_str);
        if (!type_opt.has_value()) {
          res.code = 400;
          res.body = make_error_json("INVALID_TYPE", "Invalid annotation type");
          return res;
        }
        ann.type = type_opt.value();

        ann.geometry_json = parse_json_object(body, "geometry");
        ann.text = parse_json_string(body, "text");
        ann.style = parse_style(parse_json_object(body, "style"));
        ann.created_at = std::chrono::system_clock::now();
        ann.updated_at = ann.created_at;

        if (ann.study_uid.empty()) {
          res.code = 400;
          res.body = make_error_json("MISSING_FIELD", "study_uid is required");
          return res;
        }

        auto repo = make_annotation_repo(ctx->database.get());
        auto save_result = repo.save(ann);
        if (!save_result.is_ok()) {
          res.code = 500;
          res.body =
              make_error_json("SAVE_ERROR", save_result.error().message);
          return res;
        }

        res.code = 201;
        std::ostringstream oss;
        oss << R"({"annotation_id":")" << json_escape(ann.annotation_id)
            << R"(","created_at":")" << format_timestamp(ann.created_at)
            << R"("})";
        res.body = oss.str();
        return res;
      });

  // GET /api/v1/annotations - List annotations
  CROW_ROUTE(app, "/api/v1/annotations")
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

        storage::annotation_query query;
        query.limit = limit;
        query.offset = offset;

        auto study_uid = req.url_params.get("study_uid");
        if (study_uid) {
          query.study_uid = study_uid;
        }
        auto series_uid = req.url_params.get("series_uid");
        if (series_uid) {
          query.series_uid = series_uid;
        }
        auto sop_instance_uid = req.url_params.get("sop_instance_uid");
        if (sop_instance_uid) {
          query.sop_instance_uid = sop_instance_uid;
        }
        auto user_id = req.url_params.get("user_id");
        if (user_id) {
          query.user_id = user_id;
        }

        auto repo = make_annotation_repo(ctx->database.get());

        storage::annotation_query count_query = query;
        count_query.limit = 0;
        count_query.offset = 0;
        size_t total_count = count_annotations(repo, count_query);

        auto annotations = search_annotations(repo, query);

        res.code = 200;
        res.body = annotations_to_json(annotations, total_count);
        return res;
      });

  // GET /api/v1/annotations/<annotationId> - Get annotation by ID
  CROW_ROUTE(app, "/api/v1/annotations/<string>")
      .methods(crow::HTTPMethod::GET)(
          [ctx](const crow::request & /*req*/, const std::string &annotation_id) {
            crow::response res;
            res.add_header("Content-Type", "application/json");
            add_cors_headers(res, *ctx);

            if (!ctx->database) {
              res.code = 503;
              res.body = make_error_json("DATABASE_UNAVAILABLE",
                                         "Database not configured");
              return res;
            }

            auto repo = make_annotation_repo(ctx->database.get());
            auto ann = find_annotation(repo, annotation_id);
            if (!ann.has_value()) {
              res.code = 404;
              res.body = make_error_json("NOT_FOUND", "Annotation not found");
              return res;
            }

            res.code = 200;
            res.body = annotation_to_json(ann.value());
            return res;
          });

  // PUT /api/v1/annotations/<annotationId> - Update annotation
  CROW_ROUTE(app, "/api/v1/annotations/<string>")
      .methods(crow::HTTPMethod::PUT)(
          [ctx](const crow::request &req, const std::string &annotation_id) {
            crow::response res;
            res.add_header("Content-Type", "application/json");
            add_cors_headers(res, *ctx);

            if (!ctx->database) {
              res.code = 503;
              res.body = make_error_json("DATABASE_UNAVAILABLE",
                                         "Database not configured");
              return res;
            }

            auto repo = make_annotation_repo(ctx->database.get());
            auto existing = find_annotation(repo, annotation_id);
            if (!existing.has_value()) {
              res.code = 404;
              res.body = make_error_json("NOT_FOUND", "Annotation not found");
              return res;
            }

            std::string body = req.body;
            if (body.empty()) {
              res.code = 400;
              res.body =
                  make_error_json("INVALID_REQUEST", "Request body is empty");
              return res;
            }

            storage::annotation_record ann = existing.value();
            auto geometry = parse_json_object(body, "geometry");
            if (geometry != "{}") {
              ann.geometry_json = geometry;
            }
            auto text = parse_json_string(body, "text");
            if (!text.empty() || body.find("\"text\":\"\"") != std::string::npos) {
              ann.text = text;
            }
            auto style_json = parse_json_object(body, "style");
            if (style_json != "{}") {
              ann.style = parse_style(style_json);
            }
            ann.updated_at = std::chrono::system_clock::now();

            auto update_result = repo.update(ann);
            if (!update_result.is_ok()) {
              res.code = 500;
              res.body =
                  make_error_json("UPDATE_ERROR", update_result.error().message);
              return res;
            }

            res.code = 200;
            std::ostringstream oss;
            oss << R"({"annotation_id":")" << json_escape(ann.annotation_id)
                << R"(","updated_at":")" << format_timestamp(ann.updated_at)
                << R"("})";
            res.body = oss.str();
            return res;
          });

  // DELETE /api/v1/annotations/<annotationId> - Delete annotation
  CROW_ROUTE(app, "/api/v1/annotations/<string>")
      .methods(crow::HTTPMethod::DELETE)(
          [ctx](const crow::request & /*req*/, const std::string &annotation_id) {
            crow::response res;
            add_cors_headers(res, *ctx);

            if (!ctx->database) {
              res.code = 503;
              res.add_header("Content-Type", "application/json");
              res.body = make_error_json("DATABASE_UNAVAILABLE",
                                         "Database not configured");
              return res;
            }

            auto repo = make_annotation_repo(ctx->database.get());
            if (!annotation_exists(repo, annotation_id)) {
              res.code = 404;
              res.add_header("Content-Type", "application/json");
              res.body = make_error_json("NOT_FOUND", "Annotation not found");
              return res;
            }

            auto remove_result = repo.remove(annotation_id);
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

  // GET /api/v1/instances/<sopInstanceUid>/annotations - Get annotations for instance
  CROW_ROUTE(app, "/api/v1/instances/<string>/annotations")
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

            auto repo = make_annotation_repo(ctx->database.get());
            auto annotations = find_by_instance(repo, sop_instance_uid);

            std::ostringstream oss;
            oss << R"({"data":[)";
            for (size_t i = 0; i < annotations.size(); ++i) {
              if (i > 0) {
                oss << ",";
              }
              oss << annotation_to_json(annotations[i]);
            }
            oss << "]}";

            res.code = 200;
            res.body = oss.str();
            return res;
          });
}

} // namespace pacs::web::endpoints
