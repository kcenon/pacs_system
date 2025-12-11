/**
 * @file worklist_endpoints.cpp
 * @brief Worklist API endpoints implementation
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
#include "pacs/storage/worklist_record.hpp"
#include "pacs/web/endpoints/system_endpoints.hpp"
#include "pacs/web/endpoints/worklist_endpoints.hpp"
#include "pacs/web/rest_config.hpp"
#include "pacs/web/rest_types.hpp"

#include <chrono>
#include <iomanip>
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
 * @brief Format time_point to ISO 8601 string
 */
std::string format_datetime(
    const std::chrono::system_clock::time_point &tp) {
  auto time = std::chrono::system_clock::to_time_t(tp);
  std::ostringstream oss;
  oss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
  return oss.str();
}

/**
 * @brief Convert worklist_item to JSON string
 */
std::string worklist_item_to_json(const storage::worklist_item &item) {
  std::ostringstream oss;
  oss << R"({"pk":)" << item.pk
      << R"(,"step_id":")" << json_escape(item.step_id)
      << R"(","step_status":")" << json_escape(item.step_status)
      << R"(","patient_id":")" << json_escape(item.patient_id)
      << R"(","patient_name":")" << json_escape(item.patient_name)
      << R"(","birth_date":")" << json_escape(item.birth_date)
      << R"(","sex":")" << json_escape(item.sex)
      << R"(","accession_no":")" << json_escape(item.accession_no)
      << R"(","requested_proc_id":")" << json_escape(item.requested_proc_id)
      << R"(","study_uid":")" << json_escape(item.study_uid)
      << R"(","scheduled_datetime":")" << json_escape(item.scheduled_datetime)
      << R"(","station_ae":")" << json_escape(item.station_ae)
      << R"(","station_name":")" << json_escape(item.station_name)
      << R"(","modality":")" << json_escape(item.modality)
      << R"(","procedure_desc":")" << json_escape(item.procedure_desc)
      << R"(","protocol_code":")" << json_escape(item.protocol_code)
      << R"(","referring_phys":")" << json_escape(item.referring_phys)
      << R"(","referring_phys_id":")" << json_escape(item.referring_phys_id)
      << R"(","created_at":")" << format_datetime(item.created_at)
      << R"(","updated_at":")" << format_datetime(item.updated_at)
      << R"("})";
  return oss.str();
}

/**
 * @brief Convert vector of worklist_items to JSON array string
 */
std::string worklist_items_to_json(
    const std::vector<storage::worklist_item> &items,
    size_t total_count) {
  std::ostringstream oss;
  oss << R"({"data":[)";
  for (size_t i = 0; i < items.size(); ++i) {
    if (i > 0) {
      oss << ",";
    }
    oss << worklist_item_to_json(items[i]);
  }
  oss << R"(],"pagination":{"total":)" << total_count
      << R"(,"count":)" << items.size() << "}}";
  return oss.str();
}

/**
 * @brief Parse pagination parameters from request
 */
std::pair<size_t, size_t> parse_pagination(const crow::request &req) {
  size_t limit = 20;  // Default limit
  size_t offset = 0;

  auto limit_param = req.url_params.get("limit");
  if (limit_param) {
    try {
      limit = std::stoul(limit_param);
      if (limit > 100) {
        limit = 100;  // Cap at 100
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
 * @brief Parse worklist_item from JSON request body
 */
std::optional<storage::worklist_item> parse_worklist_item_json(
    const std::string &body) {
  storage::worklist_item item;

  // Simple JSON parsing for required fields
  auto get_json_string = [&body](const std::string &key) -> std::string {
    std::string search_key = "\"" + key + "\":\"";
    auto pos = body.find(search_key);
    if (pos == std::string::npos) {
      return "";
    }
    pos += search_key.length();
    auto end_pos = body.find("\"", pos);
    if (end_pos == std::string::npos) {
      return "";
    }
    return body.substr(pos, end_pos - pos);
  };

  item.step_id = get_json_string("step_id");
  item.patient_id = get_json_string("patient_id");
  item.patient_name = get_json_string("patient_name");
  item.birth_date = get_json_string("birth_date");
  item.sex = get_json_string("sex");
  item.accession_no = get_json_string("accession_no");
  item.requested_proc_id = get_json_string("requested_proc_id");
  item.study_uid = get_json_string("study_uid");
  item.scheduled_datetime = get_json_string("scheduled_datetime");
  item.station_ae = get_json_string("station_ae");
  item.station_name = get_json_string("station_name");
  item.modality = get_json_string("modality");
  item.procedure_desc = get_json_string("procedure_desc");
  item.protocol_code = get_json_string("protocol_code");
  item.referring_phys = get_json_string("referring_phys");
  item.referring_phys_id = get_json_string("referring_phys_id");
  item.step_status = get_json_string("step_status");

  // Set default status if not provided
  if (item.step_status.empty()) {
    item.step_status = "SCHEDULED";
  }

  // Validate required fields
  if (!item.is_valid()) {
    return std::nullopt;
  }

  return item;
}

}  // namespace

// Internal implementation function called from rest_server.cpp
void register_worklist_endpoints_impl(
    crow::SimpleApp &app,
    std::shared_ptr<rest_server_context> ctx) {
  // GET /api/v1/worklist - List scheduled procedures (paginated)
  CROW_ROUTE(app, "/api/v1/worklist")
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

        // Parse pagination
        auto [limit, offset] = parse_pagination(req);

        // Build query from URL parameters
        storage::worklist_query query;
        query.limit = limit;
        query.offset = offset;

        auto station_ae = req.url_params.get("station_ae");
        if (station_ae) {
          query.station_ae = station_ae;
        }

        auto modality = req.url_params.get("modality");
        if (modality) {
          query.modality = modality;
        }

        auto scheduled_date_from = req.url_params.get("scheduled_date_from");
        if (scheduled_date_from) {
          query.scheduled_date_from = scheduled_date_from;
        }

        auto scheduled_date_to = req.url_params.get("scheduled_date_to");
        if (scheduled_date_to) {
          query.scheduled_date_to = scheduled_date_to;
        }

        auto patient_id = req.url_params.get("patient_id");
        if (patient_id) {
          query.patient_id = patient_id;
        }

        auto patient_name = req.url_params.get("patient_name");
        if (patient_name) {
          query.patient_name = patient_name;
        }

        auto accession_no = req.url_params.get("accession_no");
        if (accession_no) {
          query.accession_no = accession_no;
        }

        auto step_id = req.url_params.get("step_id");
        if (step_id) {
          query.step_id = step_id;
        }

        // Check if we should include all statuses
        auto include_all = req.url_params.get("include_all_status");
        if (include_all && std::string(include_all) == "true") {
          query.include_all_status = true;
        }

        // Get total count (without pagination)
        storage::worklist_query count_query = query;
        count_query.limit = 0;
        count_query.offset = 0;
        auto all_items = ctx->database->query_worklist(count_query);
        size_t total_count = all_items.size();

        // Get paginated results
        auto items = ctx->database->query_worklist(query);

        res.code = 200;
        res.body = worklist_items_to_json(items, total_count);
        return res;
      });

  // POST /api/v1/worklist - Create worklist item
  CROW_ROUTE(app, "/api/v1/worklist")
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

        // Parse request body
        auto item = parse_worklist_item_json(req.body);
        if (!item) {
          res.code = 400;
          res.body = make_error_json(
              "INVALID_REQUEST",
              "Missing required fields: step_id, patient_id, modality, "
              "scheduled_datetime");
          return res;
        }

        // Add worklist item
        auto result = ctx->database->add_worklist_item(*item);
        if (result.is_err()) {
          res.code = 500;
          res.body = make_error_json("CREATE_FAILED", result.error().message);
          return res;
        }

        // Fetch the created item to return
        auto created_item = ctx->database->find_worklist_by_pk(result.value());
        if (created_item) {
          res.code = 201;
          res.body = worklist_item_to_json(*created_item);
        } else {
          res.code = 201;
          res.body = make_success_json("Worklist item created");
        }
        return res;
      });

  // GET /api/v1/worklist/:id - Get worklist item by pk
  CROW_ROUTE(app, "/api/v1/worklist/<int>")
      .methods(crow::HTTPMethod::GET)(
          [ctx](const crow::request & /*req*/, int pk) {
            crow::response res;
            res.add_header("Content-Type", "application/json");
            add_cors_headers(res, *ctx);

            if (!ctx->database) {
              res.code = 503;
              res.body = make_error_json("DATABASE_UNAVAILABLE",
                                         "Database not configured");
              return res;
            }

            auto item = ctx->database->find_worklist_by_pk(pk);
            if (!item) {
              res.code = 404;
              res.body = make_error_json("NOT_FOUND", "Worklist item not found");
              return res;
            }

            res.code = 200;
            res.body = worklist_item_to_json(*item);
            return res;
          });

  // PUT /api/v1/worklist/:id - Update worklist item
  CROW_ROUTE(app, "/api/v1/worklist/<int>")
      .methods(crow::HTTPMethod::PUT)(
          [ctx](const crow::request &req, int pk) {
            crow::response res;
            res.add_header("Content-Type", "application/json");
            add_cors_headers(res, *ctx);

            if (!ctx->database) {
              res.code = 503;
              res.body = make_error_json("DATABASE_UNAVAILABLE",
                                         "Database not configured");
              return res;
            }

            // Verify item exists
            auto existing_item = ctx->database->find_worklist_by_pk(pk);
            if (!existing_item) {
              res.code = 404;
              res.body = make_error_json("NOT_FOUND", "Worklist item not found");
              return res;
            }

            // Parse new status from request body
            auto get_json_string = [&req](const std::string &key) -> std::string {
              std::string search_key = "\"" + key + "\":\"";
              auto pos = req.body.find(search_key);
              if (pos == std::string::npos) {
                return "";
              }
              pos += search_key.length();
              auto end_pos = req.body.find("\"", pos);
              if (end_pos == std::string::npos) {
                return "";
              }
              return req.body.substr(pos, end_pos - pos);
            };

            std::string new_status = get_json_string("step_status");
            if (new_status.empty()) {
              res.code = 400;
              res.body = make_error_json("INVALID_REQUEST",
                                         "step_status is required for update");
              return res;
            }

            // Update status
            auto result = ctx->database->update_worklist_status(
                existing_item->step_id,
                existing_item->accession_no,
                new_status);

            if (result.is_err()) {
              res.code = 500;
              res.body = make_error_json("UPDATE_FAILED", result.error().message);
              return res;
            }

            // Fetch updated item
            auto updated_item = ctx->database->find_worklist_by_pk(pk);
            if (updated_item) {
              res.code = 200;
              res.body = worklist_item_to_json(*updated_item);
            } else {
              res.code = 200;
              res.body = make_success_json("Worklist item updated");
            }
            return res;
          });

  // DELETE /api/v1/worklist/:id - Delete worklist item
  CROW_ROUTE(app, "/api/v1/worklist/<int>")
      .methods(crow::HTTPMethod::DELETE)(
          [ctx](const crow::request & /*req*/, int pk) {
            crow::response res;
            res.add_header("Content-Type", "application/json");
            add_cors_headers(res, *ctx);

            if (!ctx->database) {
              res.code = 503;
              res.body = make_error_json("DATABASE_UNAVAILABLE",
                                         "Database not configured");
              return res;
            }

            // Verify item exists
            auto item = ctx->database->find_worklist_by_pk(pk);
            if (!item) {
              res.code = 404;
              res.body = make_error_json("NOT_FOUND", "Worklist item not found");
              return res;
            }

            auto result = ctx->database->delete_worklist_item(
                item->step_id, item->accession_no);
            if (result.is_err()) {
              res.code = 500;
              res.body = make_error_json("DELETE_FAILED", result.error().message);
              return res;
            }

            res.code = 200;
            res.body = make_success_json("Worklist item deleted successfully");
            return res;
          });
}

}  // namespace pacs::web::endpoints
