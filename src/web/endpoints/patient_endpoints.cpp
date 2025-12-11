/**
 * @file patient_endpoints.cpp
 * @brief Patient API endpoints implementation
 *
 * @copyright Copyright (c) 2025
 * @license MIT
 */

// IMPORTANT: Include Crow FIRST before any PACS headers to avoid forward
// declaration conflicts
#include "crow.h"

#include "pacs/storage/index_database.hpp"
#include "pacs/storage/patient_record.hpp"
#include "pacs/storage/study_record.hpp"
#include "pacs/web/endpoints/patient_endpoints.hpp"
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
 * @brief Convert patient_record to JSON string
 */
std::string patient_to_json(const storage::patient_record &patient) {
  std::ostringstream oss;
  oss << R"({"pk":)" << patient.pk << R"(,"patient_id":")"
      << json_escape(patient.patient_id) << R"(","patient_name":")"
      << json_escape(patient.patient_name) << R"(","birth_date":")"
      << json_escape(patient.birth_date) << R"(","sex":")"
      << json_escape(patient.sex) << R"(","other_ids":")"
      << json_escape(patient.other_ids) << R"(","ethnic_group":")"
      << json_escape(patient.ethnic_group) << R"(","comments":")"
      << json_escape(patient.comments) << R"("})";
  return oss.str();
}

/**
 * @brief Convert vector of patient_records to JSON array string
 */
std::string patients_to_json(const std::vector<storage::patient_record> &patients,
                             size_t total_count) {
  std::ostringstream oss;
  oss << R"({"data":[)";
  for (size_t i = 0; i < patients.size(); ++i) {
    if (i > 0) {
      oss << ",";
    }
    oss << patient_to_json(patients[i]);
  }
  oss << R"(],"pagination":{"total":)" << total_count << R"(,"count":)"
      << patients.size() << "}}";
  return oss.str();
}

/**
 * @brief Convert study_record to JSON string (for patient's studies)
 */
std::string study_to_json(const storage::study_record &study) {
  std::ostringstream oss;
  oss << R"({"pk":)" << study.pk << R"(,"study_instance_uid":")"
      << json_escape(study.study_uid) << R"(","study_id":")"
      << json_escape(study.study_id) << R"(","study_date":")"
      << json_escape(study.study_date) << R"(","study_time":")"
      << json_escape(study.study_time) << R"(","accession_number":")"
      << json_escape(study.accession_number) << R"(","referring_physician":")"
      << json_escape(study.referring_physician) << R"(","study_description":")"
      << json_escape(study.study_description) << R"(","modalities_in_study":")"
      << json_escape(study.modalities_in_study) << R"(","num_series":)"
      << study.num_series << R"(,"num_instances":)" << study.num_instances
      << "}";
  return oss.str();
}

/**
 * @brief Convert vector of study_records to JSON array string
 */
std::string studies_to_json(const std::vector<storage::study_record> &studies) {
  std::ostringstream oss;
  oss << R"({"data":[)";
  for (size_t i = 0; i < studies.size(); ++i) {
    if (i > 0) {
      oss << ",";
    }
    oss << study_to_json(studies[i]);
  }
  oss << R"(],"count":)" << studies.size() << "}";
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
        limit = 100; // Cap at 100
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

} // namespace

// Internal implementation function called from rest_server.cpp
void register_patient_endpoints_impl(crow::SimpleApp &app,
                                     std::shared_ptr<rest_server_context> ctx) {
  // GET /api/v1/patients - List patients (paginated)
  CROW_ROUTE(app, "/api/v1/patients")
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
        storage::patient_query query;
        query.limit = limit;
        query.offset = offset;

        auto patient_id = req.url_params.get("patient_id");
        if (patient_id) {
          query.patient_id = patient_id;
        }

        auto patient_name = req.url_params.get("patient_name");
        if (patient_name) {
          query.patient_name = patient_name;
        }

        auto birth_date = req.url_params.get("birth_date");
        if (birth_date) {
          query.birth_date = birth_date;
        }

        auto birth_date_from = req.url_params.get("birth_date_from");
        if (birth_date_from) {
          query.birth_date_from = birth_date_from;
        }

        auto birth_date_to = req.url_params.get("birth_date_to");
        if (birth_date_to) {
          query.birth_date_to = birth_date_to;
        }

        auto sex = req.url_params.get("sex");
        if (sex) {
          query.sex = sex;
        }

        // Get total count (without pagination for accurate count)
        storage::patient_query count_query = query;
        count_query.limit = 0;
        count_query.offset = 0;
        auto all_patients = ctx->database->search_patients(count_query);
        size_t total_count = all_patients.size();

        // Get paginated results
        auto patients = ctx->database->search_patients(query);

        res.code = 200;
        res.body = patients_to_json(patients, total_count);
        return res;
      });

  // GET /api/v1/patients/:id - Get patient details
  CROW_ROUTE(app, "/api/v1/patients/<string>")
      .methods(crow::HTTPMethod::GET)(
          [ctx](const crow::request & /*req*/, const std::string &patient_id) {
            crow::response res;
            res.add_header("Content-Type", "application/json");
            add_cors_headers(res, *ctx);

            if (!ctx->database) {
              res.code = 503;
              res.body = make_error_json("DATABASE_UNAVAILABLE",
                                         "Database not configured");
              return res;
            }

            auto patient = ctx->database->find_patient(patient_id);
            if (!patient) {
              res.code = 404;
              res.body = make_error_json("NOT_FOUND", "Patient not found");
              return res;
            }

            res.code = 200;
            res.body = patient_to_json(*patient);
            return res;
          });

  // GET /api/v1/patients/:id/studies - Get patient's studies
  CROW_ROUTE(app, "/api/v1/patients/<string>/studies")
      .methods(crow::HTTPMethod::GET)(
          [ctx](const crow::request & /*req*/, const std::string &patient_id) {
            crow::response res;
            res.add_header("Content-Type", "application/json");
            add_cors_headers(res, *ctx);

            if (!ctx->database) {
              res.code = 503;
              res.body = make_error_json("DATABASE_UNAVAILABLE",
                                         "Database not configured");
              return res;
            }

            // Verify patient exists
            auto patient = ctx->database->find_patient(patient_id);
            if (!patient) {
              res.code = 404;
              res.body = make_error_json("NOT_FOUND", "Patient not found");
              return res;
            }

            auto studies = ctx->database->list_studies(patient_id);

            res.code = 200;
            res.body = studies_to_json(studies);
            return res;
          });
}

} // namespace pacs::web::endpoints
