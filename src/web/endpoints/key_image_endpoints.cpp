/**
 * @file key_image_endpoints.cpp
 * @brief Key image API endpoints implementation
 *
 * @see Issue #545 - Implement Annotation & Measurement APIs
 * @see Issue #583 - Part 3: Key Image & Viewer State REST Endpoints
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

#include "pacs/storage/index_database.hpp"
#include "pacs/storage/key_image_record.hpp"
#include "pacs/storage/key_image_repository.hpp"
#include "pacs/web/endpoints/key_image_endpoints.hpp"
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
 * @brief Generate a UUID for key_image_id
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
 * @brief Convert key_image_record to JSON string
 */
std::string key_image_to_json(const storage::key_image_record &ki) {
  std::ostringstream oss;
  oss << R"({"key_image_id":")" << json_escape(ki.key_image_id)
      << R"(","study_uid":")" << json_escape(ki.study_uid)
      << R"(","sop_instance_uid":")" << json_escape(ki.sop_instance_uid)
      << R"(","frame_number":)";
  if (ki.frame_number.has_value()) {
    oss << ki.frame_number.value();
  } else {
    oss << "null";
  }
  oss << R"(,"user_id":")" << json_escape(ki.user_id)
      << R"(","reason":")" << json_escape(ki.reason)
      << R"(","document_title":")" << json_escape(ki.document_title)
      << R"(","created_at":")" << format_timestamp(ki.created_at) << R"("})";
  return oss.str();
}

/**
 * @brief Convert vector of key images to JSON array string
 */
std::string key_images_to_json(
    const std::vector<storage::key_image_record> &key_images) {
  std::ostringstream oss;
  oss << R"({"data":[)";
  for (size_t i = 0; i < key_images.size(); ++i) {
    if (i > 0) {
      oss << ",";
    }
    oss << key_image_to_json(key_images[i]);
  }
  oss << "]}";
  return oss.str();
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

} // namespace

// Internal implementation function called from rest_server.cpp
void register_key_image_endpoints_impl(crow::SimpleApp &app,
                                       std::shared_ptr<rest_server_context> ctx) {
  // POST /api/v1/studies/<studyUid>/key-images - Create key image
  CROW_ROUTE(app, "/api/v1/studies/<string>/key-images")
      .methods(crow::HTTPMethod::POST)(
          [ctx](const crow::request &req, const std::string &study_uid) {
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

            storage::key_image_record ki;
            ki.key_image_id = generate_uuid();
            ki.study_uid = study_uid;
            ki.sop_instance_uid = parse_json_string(body, "sop_instance_uid");
            ki.frame_number = parse_json_int(body, "frame_number");
            ki.user_id = parse_json_string(body, "user_id");
            ki.reason = parse_json_string(body, "reason");
            ki.document_title = parse_json_string(body, "document_title");
            ki.created_at = std::chrono::system_clock::now();

            if (ki.sop_instance_uid.empty()) {
              res.code = 400;
              res.body = make_error_json("MISSING_FIELD", "sop_instance_uid is required");
              return res;
            }

#ifdef PACS_WITH_DATABASE_SYSTEM
            storage::key_image_repository repo(ctx->database->db_adapter());
#else
            storage::key_image_repository repo(ctx->database->native_handle());
#endif
            auto save_result = repo.save(ki);
            if (!save_result.is_ok()) {
              res.code = 500;
              res.body =
                  make_error_json("SAVE_ERROR", save_result.error().message);
              return res;
            }

            res.code = 201;
            std::ostringstream oss;
            oss << R"({"key_image_id":")" << json_escape(ki.key_image_id)
                << R"(","created_at":")" << format_timestamp(ki.created_at)
                << R"("})";
            res.body = oss.str();
            return res;
          });

  // GET /api/v1/studies/<studyUid>/key-images - List key images for study
  CROW_ROUTE(app, "/api/v1/studies/<string>/key-images")
      .methods(crow::HTTPMethod::GET)(
          [ctx](const crow::request & /*req*/, const std::string &study_uid) {
            crow::response res;
            res.add_header("Content-Type", "application/json");
            add_cors_headers(res, *ctx);

            if (!ctx->database) {
              res.code = 503;
              res.body =
                  make_error_json("DATABASE_UNAVAILABLE", "Database not configured");
              return res;
            }

#ifdef PACS_WITH_DATABASE_SYSTEM
            storage::key_image_repository repo(ctx->database->db_adapter());
            auto key_images_result = repo.find_by_study(study_uid);
            if (!key_images_result.is_ok()) {
              res.code = 500;
              res.body = make_error_json("QUERY_ERROR", key_images_result.error().message);
              return res;
            }
            res.code = 200;
            res.body = key_images_to_json(key_images_result.value());
#else
            storage::key_image_repository repo(ctx->database->native_handle());
            auto key_images = repo.find_by_study(study_uid);
            res.code = 200;
            res.body = key_images_to_json(key_images);
#endif
            return res;
          });

  // DELETE /api/v1/key-images/<keyImageId> - Delete key image
  CROW_ROUTE(app, "/api/v1/key-images/<string>")
      .methods(crow::HTTPMethod::DELETE)(
          [ctx](const crow::request & /*req*/, const std::string &key_image_id) {
            crow::response res;
            add_cors_headers(res, *ctx);

            if (!ctx->database) {
              res.code = 503;
              res.add_header("Content-Type", "application/json");
              res.body = make_error_json("DATABASE_UNAVAILABLE",
                                         "Database not configured");
              return res;
            }

#ifdef PACS_WITH_DATABASE_SYSTEM
            storage::key_image_repository repo(ctx->database->db_adapter());
            auto exists_result = repo.exists(key_image_id);
            if (!exists_result.is_ok()) {
              res.code = 500;
              res.add_header("Content-Type", "application/json");
              res.body = make_error_json("QUERY_ERROR", exists_result.error().message);
              return res;
            }
            if (!exists_result.value()) {
              res.code = 404;
              res.add_header("Content-Type", "application/json");
              res.body = make_error_json("NOT_FOUND", "Key image not found");
              return res;
            }
#else
            storage::key_image_repository repo(ctx->database->native_handle());
            if (!repo.exists(key_image_id)) {
              res.code = 404;
              res.add_header("Content-Type", "application/json");
              res.body = make_error_json("NOT_FOUND", "Key image not found");
              return res;
            }
#endif

            auto remove_result = repo.remove(key_image_id);
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

  // POST /api/v1/studies/<studyUid>/key-images/export-sr - Export as Key Object Selection SR
  CROW_ROUTE(app, "/api/v1/studies/<string>/key-images/export-sr")
      .methods(crow::HTTPMethod::POST)(
          [ctx](const crow::request & /*req*/, const std::string &study_uid) {
            crow::response res;
            add_cors_headers(res, *ctx);

            if (!ctx->database) {
              res.code = 503;
              res.add_header("Content-Type", "application/json");
              res.body = make_error_json("DATABASE_UNAVAILABLE",
                                         "Database not configured");
              return res;
            }

#ifdef PACS_WITH_DATABASE_SYSTEM
            storage::key_image_repository repo(ctx->database->db_adapter());
            auto key_images_result = repo.find_by_study(study_uid);
            if (!key_images_result.is_ok()) {
              res.code = 500;
              res.add_header("Content-Type", "application/json");
              res.body = make_error_json("QUERY_ERROR", key_images_result.error().message);
              return res;
            }
            const auto& key_images = key_images_result.value();
#else
            storage::key_image_repository repo(ctx->database->native_handle());
            auto key_images = repo.find_by_study(study_uid);
#endif
            if (key_images.empty()) {
              res.code = 404;
              res.add_header("Content-Type", "application/json");
              res.body = make_error_json("NO_KEY_IMAGES",
                                         "No key images found for study");
              return res;
            }

            // Build Key Object Selection Document as JSON
            // Note: Full DICOM SR encoding requires dcmtk/dicom_dataset integration
            // This endpoint returns a structured JSON that can be converted to DICOM SR
            std::ostringstream oss;
            oss << R"({"document_type":"Key Object Selection",)";
            oss << R"("study_uid":")" << json_escape(study_uid) << R"(",)";
            oss << R"("document_title":"Key Images",)";
            oss << R"("referenced_instances":[)";

            for (size_t i = 0; i < key_images.size(); ++i) {
              if (i > 0) {
                oss << ",";
              }
              const auto &ki = key_images[i];
              oss << R"({"sop_instance_uid":")" << json_escape(ki.sop_instance_uid) << R"(",)";
              oss << R"("frame_number":)";
              if (ki.frame_number.has_value()) {
                oss << ki.frame_number.value();
              } else {
                oss << "null";
              }
              oss << R"(,"reason":")" << json_escape(ki.reason) << R"("})";
            }

            oss << R"(],"created_at":")" << format_timestamp(std::chrono::system_clock::now()) << R"("})";

            res.code = 200;
            res.add_header("Content-Type", "application/json");
            res.body = oss.str();
            return res;
          });
}

} // namespace pacs::web::endpoints
