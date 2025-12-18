/**
 * @file study_endpoints.cpp
 * @brief Study API endpoints implementation
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
#include "pacs/storage/instance_record.hpp"
#include "pacs/storage/series_record.hpp"
#include "pacs/storage/study_record.hpp"
#include "pacs/web/endpoints/study_endpoints.hpp"
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
 * @brief Convert study_record to JSON string
 */
std::string study_to_json(const storage::study_record &study) {
  std::ostringstream oss;
  oss << R"({"pk":)" << study.pk << R"(,"patient_pk":)" << study.patient_pk
      << R"(,"study_instance_uid":")" << json_escape(study.study_uid)
      << R"(","study_id":")" << json_escape(study.study_id)
      << R"(","study_date":")" << json_escape(study.study_date)
      << R"(","study_time":")" << json_escape(study.study_time)
      << R"(","accession_number":")" << json_escape(study.accession_number)
      << R"(","referring_physician":")"
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
std::string studies_to_json(const std::vector<storage::study_record> &studies,
                            size_t total_count) {
  std::ostringstream oss;
  oss << R"({"data":[)";
  for (size_t i = 0; i < studies.size(); ++i) {
    if (i > 0) {
      oss << ",";
    }
    oss << study_to_json(studies[i]);
  }
  oss << R"(],"pagination":{"total":)" << total_count << R"(,"count":)"
      << studies.size() << "}}";
  return oss.str();
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
 * @brief Convert vector of series_records to JSON array string
 */
std::string series_list_to_json(const std::vector<storage::series_record> &series_list) {
  std::ostringstream oss;
  oss << R"({"data":[)";
  for (size_t i = 0; i < series_list.size(); ++i) {
    if (i > 0) {
      oss << ",";
    }
    oss << series_to_json(series_list[i]);
  }
  oss << R"(],"count":)" << series_list.size() << "}";
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
void register_study_endpoints_impl(crow::SimpleApp &app,
                                   std::shared_ptr<rest_server_context> ctx) {
  // GET /api/v1/studies - List studies (paginated)
  CROW_ROUTE(app, "/api/v1/studies")
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
        storage::study_query query;
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

        auto study_uid = req.url_params.get("study_uid");
        if (study_uid) {
          query.study_uid = study_uid;
        }

        auto study_id = req.url_params.get("study_id");
        if (study_id) {
          query.study_id = study_id;
        }

        auto study_date = req.url_params.get("study_date");
        if (study_date) {
          query.study_date = study_date;
        }

        auto study_date_from = req.url_params.get("study_date_from");
        if (study_date_from) {
          query.study_date_from = study_date_from;
        }

        auto study_date_to = req.url_params.get("study_date_to");
        if (study_date_to) {
          query.study_date_to = study_date_to;
        }

        auto accession_number = req.url_params.get("accession_number");
        if (accession_number) {
          query.accession_number = accession_number;
        }

        auto modality = req.url_params.get("modality");
        if (modality) {
          query.modality = modality;
        }

        auto referring_physician = req.url_params.get("referring_physician");
        if (referring_physician) {
          query.referring_physician = referring_physician;
        }

        auto study_description = req.url_params.get("study_description");
        if (study_description) {
          query.study_description = study_description;
        }

        // Get total count (without pagination)
        storage::study_query count_query = query;
        count_query.limit = 0;
        count_query.offset = 0;
        auto all_studies_result = ctx->database->search_studies(count_query);
        if (!all_studies_result.is_ok()) {
          res.code = 500;
          res.body = make_error_json("QUERY_ERROR",
                                     all_studies_result.error().message);
          return res;
        }
        size_t total_count = all_studies_result.value().size();

        // Get paginated results
        auto studies_result = ctx->database->search_studies(query);
        if (!studies_result.is_ok()) {
          res.code = 500;
          res.body = make_error_json("QUERY_ERROR",
                                     studies_result.error().message);
          return res;
        }

        res.code = 200;
        res.body = studies_to_json(studies_result.value(), total_count);
        return res;
      });

  // GET /api/v1/studies/:uid - Get study details
  CROW_ROUTE(app, "/api/v1/studies/<string>")
      .methods(crow::HTTPMethod::GET)(
          [ctx](const crow::request & /*req*/, const std::string &study_uid) {
            crow::response res;
            res.add_header("Content-Type", "application/json");
            add_cors_headers(res, *ctx);

            if (!ctx->database) {
              res.code = 503;
              res.body = make_error_json("DATABASE_UNAVAILABLE",
                                         "Database not configured");
              return res;
            }

            auto study = ctx->database->find_study(study_uid);
            if (!study) {
              res.code = 404;
              res.body = make_error_json("NOT_FOUND", "Study not found");
              return res;
            }

            res.code = 200;
            res.body = study_to_json(*study);
            return res;
          });

  // GET /api/v1/studies/:uid/series - Get study's series
  CROW_ROUTE(app, "/api/v1/studies/<string>/series")
      .methods(crow::HTTPMethod::GET)(
          [ctx](const crow::request & /*req*/, const std::string &study_uid) {
            crow::response res;
            res.add_header("Content-Type", "application/json");
            add_cors_headers(res, *ctx);

            if (!ctx->database) {
              res.code = 503;
              res.body = make_error_json("DATABASE_UNAVAILABLE",
                                         "Database not configured");
              return res;
            }

            // Verify study exists
            auto study = ctx->database->find_study(study_uid);
            if (!study) {
              res.code = 404;
              res.body = make_error_json("NOT_FOUND", "Study not found");
              return res;
            }

            auto series_list_result = ctx->database->list_series(study_uid);
            if (!series_list_result.is_ok()) {
              res.code = 500;
              res.body = make_error_json("QUERY_ERROR",
                                         series_list_result.error().message);
              return res;
            }

            res.code = 200;
            res.body = series_list_to_json(series_list_result.value());
            return res;
          });

  // GET /api/v1/studies/:uid/instances - Get study's instances (all series)
  CROW_ROUTE(app, "/api/v1/studies/<string>/instances")
      .methods(crow::HTTPMethod::GET)(
          [ctx](const crow::request & /*req*/, const std::string &study_uid) {
            crow::response res;
            res.add_header("Content-Type", "application/json");
            add_cors_headers(res, *ctx);

            if (!ctx->database) {
              res.code = 503;
              res.body = make_error_json("DATABASE_UNAVAILABLE",
                                         "Database not configured");
              return res;
            }

            // Verify study exists
            auto study = ctx->database->find_study(study_uid);
            if (!study) {
              res.code = 404;
              res.body = make_error_json("NOT_FOUND", "Study not found");
              return res;
            }

            // Get all series and their instances
            std::vector<storage::instance_record> all_instances;
            auto series_list_result = ctx->database->list_series(study_uid);
            if (!series_list_result.is_ok()) {
              res.code = 500;
              res.body = make_error_json("QUERY_ERROR",
                                         series_list_result.error().message);
              return res;
            }
            for (const auto &series : series_list_result.value()) {
              auto instances_result = ctx->database->list_instances(series.series_uid);
              if (!instances_result.is_ok()) {
                res.code = 500;
                res.body = make_error_json("QUERY_ERROR",
                                           instances_result.error().message);
                return res;
              }
              const auto& instances = instances_result.value();
              all_instances.insert(all_instances.end(), instances.begin(),
                                   instances.end());
            }

            res.code = 200;
            res.body = instances_to_json(all_instances);
            return res;
          });

  // DELETE /api/v1/studies/:uid - Delete study
  CROW_ROUTE(app, "/api/v1/studies/<string>")
      .methods(crow::HTTPMethod::DELETE)(
          [ctx](const crow::request & /*req*/, const std::string &study_uid) {
            crow::response res;
            res.add_header("Content-Type", "application/json");
            add_cors_headers(res, *ctx);

            if (!ctx->database) {
              res.code = 503;
              res.body = make_error_json("DATABASE_UNAVAILABLE",
                                         "Database not configured");
              return res;
            }

            // Verify study exists
            auto study = ctx->database->find_study(study_uid);
            if (!study) {
              res.code = 404;
              res.body = make_error_json("NOT_FOUND", "Study not found");
              return res;
            }

            auto result = ctx->database->delete_study(study_uid);
            if (result.is_err()) {
              res.code = 500;
              res.body = make_error_json("DELETE_FAILED",
                                         result.error().message);
              return res;
            }

            res.code = 200;
            res.body = make_success_json("Study deleted successfully");
            return res;
          });
}

} // namespace pacs::web::endpoints
