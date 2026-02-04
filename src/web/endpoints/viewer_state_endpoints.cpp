/**
 * @file viewer_state_endpoints.cpp
 * @brief Viewer state API endpoints implementation
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
#include "pacs/storage/viewer_state_record.hpp"
#include "pacs/storage/viewer_state_repository.hpp"
#include "pacs/web/endpoints/system_endpoints.hpp"
#include "pacs/web/endpoints/viewer_state_endpoints.hpp"
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
 * @brief Generate a UUID for state_id
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
 * @brief Convert viewer_state_record to JSON string
 */
std::string viewer_state_to_json(const storage::viewer_state_record &state) {
  std::ostringstream oss;
  oss << R"({"state_id":")" << json_escape(state.state_id)
      << R"(","study_uid":")" << json_escape(state.study_uid)
      << R"(","user_id":")" << json_escape(state.user_id)
      << R"(","state":)" << state.state_json
      << R"(,"created_at":")" << format_timestamp(state.created_at)
      << R"(","updated_at":")" << format_timestamp(state.updated_at) << R"("})";
  return oss.str();
}

/**
 * @brief Convert vector of viewer states to JSON array string
 */
std::string viewer_states_to_json(
    const std::vector<storage::viewer_state_record> &states) {
  std::ostringstream oss;
  oss << R"({"data":[)";
  for (size_t i = 0; i < states.size(); ++i) {
    if (i > 0) {
      oss << ",";
    }
    oss << viewer_state_to_json(states[i]);
  }
  oss << "]}";
  return oss.str();
}

/**
 * @brief Convert recent_study_record to JSON string
 */
std::string recent_study_to_json(const storage::recent_study_record &record) {
  std::ostringstream oss;
  oss << R"({"user_id":")" << json_escape(record.user_id)
      << R"(","study_uid":")" << json_escape(record.study_uid)
      << R"(","accessed_at":")" << format_timestamp(record.accessed_at) << R"("})";
  return oss.str();
}

/**
 * @brief Convert vector of recent studies to JSON array string
 */
std::string recent_studies_to_json(
    const std::vector<storage::recent_study_record> &records,
    size_t total_count) {
  std::ostringstream oss;
  oss << R"({"data":[)";
  for (size_t i = 0; i < records.size(); ++i) {
    if (i > 0) {
      oss << ",";
    }
    oss << recent_study_to_json(records[i]);
  }
  oss << R"(],"total":)" << total_count << "}";
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
 * @brief Build full state JSON from individual fields
 */
std::string build_state_json(const std::string &body) {
  // Extract individual state components and build combined JSON
  std::string layout = parse_json_object(body, "layout");
  std::string viewports = "[]";
  std::string active_viewport = "";

  // Parse viewports array
  std::string viewports_search = "\"viewports\":";
  auto viewports_pos = body.find(viewports_search);
  if (viewports_pos != std::string::npos) {
    viewports_pos += viewports_search.length();
    while (viewports_pos < body.size() &&
           (body[viewports_pos] == ' ' || body[viewports_pos] == '\t')) {
      ++viewports_pos;
    }
    if (viewports_pos < body.size() && body[viewports_pos] == '[') {
      int depth = 0;
      size_t start = viewports_pos;
      for (; viewports_pos < body.size(); ++viewports_pos) {
        if (body[viewports_pos] == '[') {
          ++depth;
        } else if (body[viewports_pos] == ']') {
          --depth;
          if (depth == 0) {
            viewports = body.substr(start, viewports_pos - start + 1);
            break;
          }
        }
      }
    }
  }

  active_viewport = parse_json_string(body, "active_viewport");

  std::ostringstream oss;
  oss << R"({"layout":)" << layout
      << R"(,"viewports":)" << viewports
      << R"(,"active_viewport":")" << json_escape(active_viewport) << R"("})";
  return oss.str();
}

} // namespace

// Internal implementation function called from rest_server.cpp
void register_viewer_state_endpoints_impl(crow::SimpleApp &app,
                                          std::shared_ptr<rest_server_context> ctx) {
  // POST /api/v1/viewer-states - Create viewer state
  CROW_ROUTE(app, "/api/v1/viewer-states")
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

        storage::viewer_state_record state;
        state.state_id = generate_uuid();
        state.study_uid = parse_json_string(body, "study_uid");
        state.user_id = parse_json_string(body, "user_id");
        state.state_json = build_state_json(body);
        state.created_at = std::chrono::system_clock::now();
        state.updated_at = state.created_at;

        if (state.study_uid.empty()) {
          res.code = 400;
          res.body = make_error_json("MISSING_FIELD", "study_uid is required");
          return res;
        }

#ifdef PACS_WITH_DATABASE_SYSTEM
        storage::viewer_state_repository repo(ctx->database->db_adapter());
#else
        storage::viewer_state_repository repo(ctx->database->native_handle());
#endif
        auto save_result = repo.save_state(state);
        if (!save_result.is_ok()) {
          res.code = 500;
          res.body =
              make_error_json("SAVE_ERROR", save_result.error().message);
          return res;
        }

        // Also record study access if user_id is provided
        if (!state.user_id.empty()) {
          (void)repo.record_study_access(state.user_id, state.study_uid);
        }

        res.code = 201;
        std::ostringstream oss;
        oss << R"({"state_id":")" << json_escape(state.state_id)
            << R"(","created_at":")" << format_timestamp(state.created_at)
            << R"("})";
        res.body = oss.str();
        return res;
      });

  // GET /api/v1/viewer-states - List viewer states
  CROW_ROUTE(app, "/api/v1/viewer-states")
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

        storage::viewer_state_query query;

        auto study_uid = req.url_params.get("study_uid");
        if (study_uid) {
          query.study_uid = study_uid;
        }
        auto user_id = req.url_params.get("user_id");
        if (user_id) {
          query.user_id = user_id;
        }
        auto limit_param = req.url_params.get("limit");
        if (limit_param) {
          try {
            query.limit = std::stoul(limit_param);
            if (query.limit > 100) {
              query.limit = 100;
            }
          } catch (...) {
            // Use default
          }
        }

#ifdef PACS_WITH_DATABASE_SYSTEM
        storage::viewer_state_repository repo(ctx->database->db_adapter());
#else
        storage::viewer_state_repository repo(ctx->database->native_handle());
#endif
        auto states = repo.search_states(query);

        res.code = 200;
        res.body = viewer_states_to_json(states);
        return res;
      });

  // GET /api/v1/viewer-states/<stateId> - Get viewer state by ID
  CROW_ROUTE(app, "/api/v1/viewer-states/<string>")
      .methods(crow::HTTPMethod::GET)(
          [ctx](const crow::request & /*req*/, const std::string &state_id) {
            crow::response res;
            res.add_header("Content-Type", "application/json");
            add_cors_headers(res, *ctx);

            if (!ctx->database) {
              res.code = 503;
              res.body = make_error_json("DATABASE_UNAVAILABLE",
                                         "Database not configured");
              return res;
            }

#ifdef PACS_WITH_DATABASE_SYSTEM
            storage::viewer_state_repository repo(ctx->database->db_adapter());
#else
            storage::viewer_state_repository repo(ctx->database->native_handle());
#endif
            auto state = repo.find_state_by_id(state_id);
            if (!state.has_value()) {
              res.code = 404;
              res.body = make_error_json("NOT_FOUND", "Viewer state not found");
              return res;
            }

            res.code = 200;
            res.body = viewer_state_to_json(state.value());
            return res;
          });

  // DELETE /api/v1/viewer-states/<stateId> - Delete viewer state
  CROW_ROUTE(app, "/api/v1/viewer-states/<string>")
      .methods(crow::HTTPMethod::DELETE)(
          [ctx](const crow::request & /*req*/, const std::string &state_id) {
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
            storage::viewer_state_repository repo(ctx->database->db_adapter());
#else
            storage::viewer_state_repository repo(ctx->database->native_handle());
#endif
            auto existing = repo.find_state_by_id(state_id);
            if (!existing.has_value()) {
              res.code = 404;
              res.add_header("Content-Type", "application/json");
              res.body = make_error_json("NOT_FOUND", "Viewer state not found");
              return res;
            }

            auto remove_result = repo.remove_state(state_id);
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

  // GET /api/v1/users/<userId>/recent-studies - Get recent studies for user
  CROW_ROUTE(app, "/api/v1/users/<string>/recent-studies")
      .methods(crow::HTTPMethod::GET)(
          [ctx](const crow::request &req, const std::string &user_id) {
            crow::response res;
            res.add_header("Content-Type", "application/json");
            add_cors_headers(res, *ctx);

            if (!ctx->database) {
              res.code = 503;
              res.body = make_error_json("DATABASE_UNAVAILABLE",
                                         "Database not configured");
              return res;
            }

            size_t limit = 20;
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

#ifdef PACS_WITH_DATABASE_SYSTEM
            storage::viewer_state_repository repo(ctx->database->db_adapter());
#else
            storage::viewer_state_repository repo(ctx->database->native_handle());
#endif
            auto records = repo.get_recent_studies(user_id, limit);
            size_t total = repo.count_recent_studies(user_id);

            res.code = 200;
            res.body = recent_studies_to_json(records, total);
            return res;
          });
}

} // namespace pacs::web::endpoints
