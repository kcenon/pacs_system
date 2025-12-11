/**
 * @file audit_endpoints.cpp
 * @brief Audit log API endpoints implementation
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

#include "pacs/storage/audit_record.hpp"
#include "pacs/storage/index_database.hpp"
#include "pacs/web/endpoints/audit_endpoints.hpp"
#include "pacs/web/endpoints/system_endpoints.hpp"
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
 * @brief Convert audit_record to JSON string
 */
std::string audit_record_to_json(const storage::audit_record &record) {
  std::ostringstream oss;
  oss << R"({"pk":)" << record.pk
      << R"(,"event_type":")" << json_escape(record.event_type)
      << R"(","outcome":")" << json_escape(record.outcome)
      << R"(","timestamp":")" << format_datetime(record.timestamp)
      << R"(","user_id":")" << json_escape(record.user_id)
      << R"(","source_ae":")" << json_escape(record.source_ae)
      << R"(","target_ae":")" << json_escape(record.target_ae)
      << R"(","source_ip":")" << json_escape(record.source_ip)
      << R"(","patient_id":")" << json_escape(record.patient_id)
      << R"(","study_uid":")" << json_escape(record.study_uid)
      << R"(","message":")" << json_escape(record.message)
      << R"(","details":")" << json_escape(record.details)
      << R"("})";
  return oss.str();
}

/**
 * @brief Convert vector of audit_records to JSON array string
 */
std::string audit_records_to_json(
    const std::vector<storage::audit_record> &records,
    size_t total_count) {
  std::ostringstream oss;
  oss << R"({"data":[)";
  for (size_t i = 0; i < records.size(); ++i) {
    if (i > 0) {
      oss << ",";
    }
    oss << audit_record_to_json(records[i]);
  }
  oss << R"(],"pagination":{"total":)" << total_count
      << R"(,"count":)" << records.size() << "}}";
  return oss.str();
}

/**
 * @brief Convert audit records to CSV format
 */
std::string audit_records_to_csv(
    const std::vector<storage::audit_record> &records) {
  std::ostringstream oss;
  // Header
  oss << "pk,event_type,outcome,timestamp,user_id,source_ae,target_ae,"
      << "source_ip,patient_id,study_uid,message\n";

  for (const auto &record : records) {
    oss << record.pk << ","
        << "\"" << record.event_type << "\","
        << "\"" << record.outcome << "\","
        << "\"" << format_datetime(record.timestamp) << "\","
        << "\"" << record.user_id << "\","
        << "\"" << record.source_ae << "\","
        << "\"" << record.target_ae << "\","
        << "\"" << record.source_ip << "\","
        << "\"" << record.patient_id << "\","
        << "\"" << record.study_uid << "\","
        << "\"" << record.message << "\"\n";
  }
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

}  // namespace

// Internal implementation function called from rest_server.cpp
void register_audit_endpoints_impl(
    crow::SimpleApp &app,
    std::shared_ptr<rest_server_context> ctx) {
  // GET /api/v1/audit/logs - List audit log entries (paginated)
  CROW_ROUTE(app, "/api/v1/audit/logs")
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
        storage::audit_query query;
        query.limit = limit;
        query.offset = offset;

        auto event_type = req.url_params.get("event_type");
        if (event_type) {
          query.event_type = event_type;
        }

        auto outcome = req.url_params.get("outcome");
        if (outcome) {
          query.outcome = outcome;
        }

        auto user_id = req.url_params.get("user_id");
        if (user_id) {
          query.user_id = user_id;
        }

        auto source_ae = req.url_params.get("source_ae");
        if (source_ae) {
          query.source_ae = source_ae;
        }

        auto patient_id = req.url_params.get("patient_id");
        if (patient_id) {
          query.patient_id = patient_id;
        }

        auto study_uid = req.url_params.get("study_uid");
        if (study_uid) {
          query.study_uid = study_uid;
        }

        auto date_from = req.url_params.get("date_from");
        if (date_from) {
          query.date_from = date_from;
        }

        auto date_to = req.url_params.get("date_to");
        if (date_to) {
          query.date_to = date_to;
        }

        // Check export format
        auto format_param = req.url_params.get("format");
        bool export_csv = format_param && std::string(format_param) == "csv";

        // Get total count (without pagination)
        storage::audit_query count_query = query;
        count_query.limit = 0;
        count_query.offset = 0;
        auto all_records = ctx->database->query_audit_log(count_query);
        size_t total_count = all_records.size();

        // Get paginated results
        auto records = ctx->database->query_audit_log(query);

        if (export_csv) {
          res.add_header("Content-Type", "text/csv");
          res.add_header("Content-Disposition",
                         "attachment; filename=\"audit_logs.csv\"");
          res.code = 200;
          res.body = audit_records_to_csv(records);
        } else {
          res.code = 200;
          res.body = audit_records_to_json(records, total_count);
        }
        return res;
      });

  // GET /api/v1/audit/logs/:id - Get specific audit log entry
  CROW_ROUTE(app, "/api/v1/audit/logs/<int>")
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

            auto record = ctx->database->find_audit_by_pk(pk);
            if (!record) {
              res.code = 404;
              res.body = make_error_json("NOT_FOUND", "Audit log entry not found");
              return res;
            }

            res.code = 200;
            res.body = audit_record_to_json(*record);
            return res;
          });

  // GET /api/v1/audit/export - Export audit logs (CSV or JSON)
  CROW_ROUTE(app, "/api/v1/audit/export")
      .methods(crow::HTTPMethod::GET)([ctx](const crow::request &req) {
        crow::response res;
        add_cors_headers(res, *ctx);

        if (!ctx->database) {
          res.add_header("Content-Type", "application/json");
          res.code = 503;
          res.body =
              make_error_json("DATABASE_UNAVAILABLE", "Database not configured");
          return res;
        }

        // Build query from URL parameters (no pagination for export)
        storage::audit_query query;

        auto event_type = req.url_params.get("event_type");
        if (event_type) {
          query.event_type = event_type;
        }

        auto outcome = req.url_params.get("outcome");
        if (outcome) {
          query.outcome = outcome;
        }

        auto user_id = req.url_params.get("user_id");
        if (user_id) {
          query.user_id = user_id;
        }

        auto date_from = req.url_params.get("date_from");
        if (date_from) {
          query.date_from = date_from;
        }

        auto date_to = req.url_params.get("date_to");
        if (date_to) {
          query.date_to = date_to;
        }

        auto records = ctx->database->query_audit_log(query);

        // Check export format
        auto format_param = req.url_params.get("format");
        std::string format = format_param ? format_param : "json";

        if (format == "csv") {
          res.add_header("Content-Type", "text/csv");
          res.add_header("Content-Disposition",
                         "attachment; filename=\"audit_logs.csv\"");
          res.code = 200;
          res.body = audit_records_to_csv(records);
        } else {
          res.add_header("Content-Type", "application/json");
          res.add_header("Content-Disposition",
                         "attachment; filename=\"audit_logs.json\"");
          res.code = 200;
          res.body = audit_records_to_json(records, records.size());
        }
        return res;
      });
}

}  // namespace pacs::web::endpoints
