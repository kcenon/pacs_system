/**
 * @file jobs_endpoints.cpp
 * @brief Job management REST API and WebSocket endpoints implementation
 *
 * @see Issue #538 - Implement Job REST API & WebSocket Progress Streaming
 * @see Issue #558 - Part 1: Jobs REST API Endpoints (CRUD)
 * @see Issue #559 - Part 2: Jobs REST API Control Endpoints
 * @see Issue #560 - Part 3: Jobs WebSocket Progress Streaming
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

#include "pacs/client/job_manager.hpp"
#include "pacs/client/job_types.hpp"
#include "pacs/web/endpoints/jobs_endpoints.hpp"
#include "pacs/web/endpoints/system_endpoints.hpp"
#include "pacs/web/rest_config.hpp"
#include "pacs/web/rest_types.hpp"

#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace pacs::web::endpoints {

namespace {

// =============================================================================
// WebSocket Subscriber Management (Issue #560)
// =============================================================================

/**
 * @brief Global state for WebSocket subscribers
 *
 * Manages connections for job progress streaming.
 * Thread-safe via shared_mutex for concurrent read access.
 */
struct ws_subscriber_state {
    /// Connections subscribed to specific job progress
    std::unordered_map<std::string, std::unordered_set<crow::websocket::connection*>>
        job_subscribers;

    /// Connections subscribed to all job updates
    std::unordered_set<crow::websocket::connection*> all_jobs_subscribers;

    /// Mutex for thread-safe access
    mutable std::shared_mutex mutex;

    /// Singleton instance
    static ws_subscriber_state& instance() {
        static ws_subscriber_state state;
        return state;
    }

    /**
     * @brief Add subscriber for specific job
     */
    void add_job_subscriber(const std::string& job_id,
                            crow::websocket::connection* conn) {
        std::unique_lock lock(mutex);
        job_subscribers[job_id].insert(conn);
    }

    /**
     * @brief Remove subscriber for specific job
     */
    void remove_job_subscriber(const std::string& job_id,
                               crow::websocket::connection* conn) {
        std::unique_lock lock(mutex);
        auto it = job_subscribers.find(job_id);
        if (it != job_subscribers.end()) {
            it->second.erase(conn);
            if (it->second.empty()) {
                job_subscribers.erase(it);
            }
        }
    }

    /**
     * @brief Add subscriber for all jobs
     */
    void add_all_jobs_subscriber(crow::websocket::connection* conn) {
        std::unique_lock lock(mutex);
        all_jobs_subscribers.insert(conn);
    }

    /**
     * @brief Remove subscriber for all jobs
     */
    void remove_all_jobs_subscriber(crow::websocket::connection* conn) {
        std::unique_lock lock(mutex);
        all_jobs_subscribers.erase(conn);
    }

    /**
     * @brief Remove connection from all subscriptions
     */
    void remove_connection(crow::websocket::connection* conn) {
        std::unique_lock lock(mutex);
        all_jobs_subscribers.erase(conn);
        for (auto& [job_id, subscribers] : job_subscribers) {
            subscribers.erase(conn);
        }
    }

    /**
     * @brief Broadcast progress update to job subscribers
     */
    void broadcast_progress(const std::string& job_id, const std::string& message) {
        std::shared_lock lock(mutex);

        // Send to specific job subscribers
        auto it = job_subscribers.find(job_id);
        if (it != job_subscribers.end()) {
            for (auto* conn : it->second) {
                conn->send_text(message);
            }
        }

        // Send to all-jobs subscribers
        for (auto* conn : all_jobs_subscribers) {
            conn->send_text(message);
        }
    }
};

/**
 * @brief Format progress update as WebSocket message JSON
 */
std::string make_progress_message(const std::string& job_id,
                                  const client::job_progress& progress) {
    std::ostringstream oss;
    oss << R"({"type":"progress","job_id":")" << json_escape(job_id)
        << R"(","progress":{)"
        << R"("total_items":)" << progress.total_items
        << R"(,"completed_items":)" << progress.completed_items
        << R"(,"failed_items":)" << progress.failed_items
        << R"(,"skipped_items":)" << progress.skipped_items
        << R"(,"bytes_transferred":)" << progress.bytes_transferred
        << R"(,"percent_complete":)" << progress.percent_complete;

    if (!progress.current_item.empty()) {
        oss << R"(,"current_item":")" << json_escape(progress.current_item) << '"';
    }

    if (!progress.current_item_description.empty()) {
        oss << R"(,"current_item_description":")"
            << json_escape(progress.current_item_description) << '"';
    }

    oss << R"(,"elapsed_ms":)" << progress.elapsed.count()
        << R"(,"estimated_remaining_ms":)" << progress.estimated_remaining.count()
        << R"(}})";
    return oss.str();
}

/**
 * @brief Format status change as WebSocket message JSON
 */
std::string make_status_message(const std::string& job_id,
                                client::job_status old_status,
                                client::job_status new_status) {
    std::ostringstream oss;
    oss << R"({"type":"status_change","job_id":")" << json_escape(job_id)
        << R"(","old_status":")" << client::to_string(old_status)
        << R"(","new_status":")" << client::to_string(new_status)
        << R"("})";
    return oss.str();
}

/**
 * @brief Format job completion as WebSocket message JSON
 */
std::string make_completion_message(const std::string& job_id,
                                    const client::job_record& record) {
    std::ostringstream oss;
    oss << R"({"type":"completed","job_id":")" << json_escape(job_id)
        << R"(","status":")" << client::to_string(record.status)
        << R"(","progress":{)"
        << R"("total_items":)" << record.progress.total_items
        << R"(,"completed_items":)" << record.progress.completed_items
        << R"(,"failed_items":)" << record.progress.failed_items
        << R"(,"percent_complete":)" << record.progress.percent_complete
        << R"(}})";
    return oss.str();
}

// Flag to track if callbacks are registered
static std::atomic<bool> g_callbacks_registered{false};

/**
 * @brief Add CORS headers to response
 */
void add_cors_headers(crow::response& res, const rest_server_context& ctx) {
    if (ctx.config && !ctx.config->cors_allowed_origins.empty()) {
        res.add_header("Access-Control-Allow-Origin",
                       ctx.config->cors_allowed_origins);
    }
}

/**
 * @brief Format ISO 8601 timestamp
 */
std::string format_timestamp(std::chrono::system_clock::time_point tp) {
    if (tp == std::chrono::system_clock::time_point{}) {
        return "";
    }
    auto time_t_val = std::chrono::system_clock::to_time_t(tp);
    std::tm tm_val{};
#ifdef _WIN32
    gmtime_s(&tm_val, &time_t_val);
#else
    gmtime_r(&time_t_val, &tm_val);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_val);
    return buf;
}

/**
 * @brief Convert job_progress to JSON string
 */
std::string progress_to_json(const client::job_progress& progress) {
    std::ostringstream oss;
    oss << R"({"total_items":)" << progress.total_items
        << R"(,"completed_items":)" << progress.completed_items
        << R"(,"failed_items":)" << progress.failed_items
        << R"(,"skipped_items":)" << progress.skipped_items
        << R"(,"bytes_transferred":)" << progress.bytes_transferred
        << R"(,"percent_complete":)" << progress.percent_complete;

    if (!progress.current_item.empty()) {
        oss << R"(,"current_item":")" << json_escape(progress.current_item) << '"';
    }

    if (!progress.current_item_description.empty()) {
        oss << R"(,"current_item_description":")"
            << json_escape(progress.current_item_description) << '"';
    }

    oss << R"(,"elapsed_ms":)" << progress.elapsed.count()
        << R"(,"estimated_remaining_ms":)" << progress.estimated_remaining.count()
        << '}';
    return oss.str();
}

/**
 * @brief Convert job_record to JSON string
 */
std::string job_to_json(const client::job_record& job) {
    std::ostringstream oss;
    oss << R"({"job_id":")" << json_escape(job.job_id)
        << R"(","type":")" << client::to_string(job.type)
        << R"(","status":")" << client::to_string(job.status)
        << R"(","priority":")" << client::to_string(job.priority) << '"';

    if (!job.source_node_id.empty()) {
        oss << R"(,"source_node_id":")" << json_escape(job.source_node_id) << '"';
    }

    if (!job.destination_node_id.empty()) {
        oss << R"(,"destination_node_id":")" << json_escape(job.destination_node_id) << '"';
    }

    if (job.patient_id.has_value()) {
        oss << R"(,"patient_id":")" << json_escape(*job.patient_id) << '"';
    }

    if (job.study_uid.has_value()) {
        oss << R"(,"study_uid":")" << json_escape(*job.study_uid) << '"';
    }

    if (job.series_uid.has_value()) {
        oss << R"(,"series_uid":")" << json_escape(*job.series_uid) << '"';
    }

    // Progress
    oss << R"(,"progress":)" << progress_to_json(job.progress);

    // Error info
    if (!job.error_message.empty()) {
        oss << R"(,"error_message":")" << json_escape(job.error_message) << '"';
    }

    if (!job.error_details.empty()) {
        oss << R"(,"error_details":")" << json_escape(job.error_details) << '"';
    }

    oss << R"(,"retry_count":)" << job.retry_count
        << R"(,"max_retries":)" << job.max_retries;

    // Timestamps
    auto created_at = format_timestamp(job.created_at);
    if (!created_at.empty()) {
        oss << R"(,"created_at":")" << created_at << '"';
    }

    if (job.queued_at.has_value()) {
        auto queued_at = format_timestamp(*job.queued_at);
        if (!queued_at.empty()) {
            oss << R"(,"queued_at":")" << queued_at << '"';
        }
    }

    if (job.started_at.has_value()) {
        auto started_at = format_timestamp(*job.started_at);
        if (!started_at.empty()) {
            oss << R"(,"started_at":")" << started_at << '"';
        }
    }

    if (job.completed_at.has_value()) {
        auto completed_at = format_timestamp(*job.completed_at);
        if (!completed_at.empty()) {
            oss << R"(,"completed_at":")" << completed_at << '"';
        }
    }

    if (!job.created_by.empty()) {
        oss << R"(,"created_by":")" << json_escape(job.created_by) << '"';
    }

    oss << '}';
    return oss.str();
}

/**
 * @brief Convert vector of jobs to JSON array string
 */
std::string jobs_to_json(const std::vector<client::job_record>& jobs,
                         size_t total_count) {
    std::ostringstream oss;
    oss << R"({"jobs":[)";
    for (size_t i = 0; i < jobs.size(); ++i) {
        if (i > 0) {
            oss << ',';
        }
        oss << job_to_json(jobs[i]);
    }
    oss << R"(],"total":)" << total_count << '}';
    return oss.str();
}

/**
 * @brief Parse pagination parameters from request
 */
std::pair<size_t, size_t> parse_pagination(const crow::request& req) {
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
 * @brief Helper for simple JSON string value extraction
 */
std::string get_json_string_value(const std::string& body, const std::string& key) {
    std::string search = "\"" + key + "\":\"";
    auto pos = body.find(search);
    if (pos == std::string::npos) {
        return "";
    }
    pos += search.length();
    auto end_pos = body.find('"', pos);
    if (end_pos == std::string::npos) {
        return "";
    }
    return body.substr(pos, end_pos - pos);
}

/**
 * @brief Helper for JSON string array extraction
 */
std::vector<std::string> get_json_string_array(const std::string& body,
                                                const std::string& key) {
    std::vector<std::string> result;
    std::string search = "\"" + key + "\":[";
    auto pos = body.find(search);
    if (pos == std::string::npos) {
        return result;
    }
    pos += search.length();
    auto end_pos = body.find(']', pos);
    if (end_pos == std::string::npos) {
        return result;
    }

    std::string array_content = body.substr(pos, end_pos - pos);

    size_t start = 0;
    while (start < array_content.length()) {
        auto quote_start = array_content.find('"', start);
        if (quote_start == std::string::npos) break;
        auto quote_end = array_content.find('"', quote_start + 1);
        if (quote_end == std::string::npos) break;
        result.push_back(array_content.substr(quote_start + 1, quote_end - quote_start - 1));
        start = quote_end + 1;
    }

    return result;
}

} // namespace

// Internal implementation function called from rest_server.cpp
void register_jobs_endpoints_impl(crow::SimpleApp& app,
                                   std::shared_ptr<rest_server_context> ctx) {
    // GET /api/v1/jobs - List jobs (paginated with filters)
    CROW_ROUTE(app, "/api/v1/jobs")
        .methods(crow::HTTPMethod::GET)([ctx](const crow::request& req) {
            crow::response res;
            res.add_header("Content-Type", "application/json");
            add_cors_headers(res, *ctx);

            if (!ctx->job_manager) {
                res.code = 503;
                res.body = make_error_json("SERVICE_UNAVAILABLE",
                                           "Job manager not configured");
                return res;
            }

            // Parse pagination
            auto [limit, offset] = parse_pagination(req);

            // Parse filters
            std::optional<client::job_status> status_filter;
            std::optional<client::job_type> type_filter;

            auto status_param = req.url_params.get("status");
            if (status_param) {
                status_filter = client::job_status_from_string(status_param);
            }

            auto type_param = req.url_params.get("type");
            if (type_param) {
                type_filter = client::job_type_from_string(type_param);
            }

            // Get jobs with filters
            auto jobs = ctx->job_manager->list_jobs(status_filter, type_filter,
                                                     limit, offset);

            // Get total count (without pagination for now, we use jobs.size())
            // TODO: Add count method to job_manager for accurate total
            size_t total_count = jobs.size();

            res.code = 200;
            res.body = jobs_to_json(jobs, total_count);
            return res;
        });

    // POST /api/v1/jobs - Create a new job
    CROW_ROUTE(app, "/api/v1/jobs")
        .methods(crow::HTTPMethod::POST)([ctx](const crow::request& req) {
            crow::response res;
            res.add_header("Content-Type", "application/json");
            add_cors_headers(res, *ctx);

            if (!ctx->job_manager) {
                res.code = 503;
                res.body = make_error_json("SERVICE_UNAVAILABLE",
                                           "Job manager not configured");
                return res;
            }

            // Parse job type
            auto type_str = get_json_string_value(req.body, "type");
            if (type_str.empty()) {
                res.code = 400;
                res.body = make_error_json("INVALID_REQUEST", "type is required");
                return res;
            }

            auto type = client::job_type_from_string(type_str);

            // Parse priority (optional, defaults to normal)
            auto priority_str = get_json_string_value(req.body, "priority");
            auto priority = priority_str.empty()
                ? client::job_priority::normal
                : client::job_priority_from_string(priority_str);

            std::string job_id;

            // Create job based on type
            switch (type) {
                case client::job_type::retrieve: {
                    auto source_node_id = get_json_string_value(req.body, "source_node_id");
                    auto study_uid = get_json_string_value(req.body, "study_uid");

                    if (source_node_id.empty()) {
                        res.code = 400;
                        res.body = make_error_json("INVALID_REQUEST",
                                                   "source_node_id is required for retrieve job");
                        return res;
                    }

                    if (study_uid.empty()) {
                        res.code = 400;
                        res.body = make_error_json("INVALID_REQUEST",
                                                   "study_uid is required for retrieve job");
                        return res;
                    }

                    auto series_uid = get_json_string_value(req.body, "series_uid");
                    std::optional<std::string_view> series_opt;
                    if (!series_uid.empty()) {
                        series_opt = series_uid;
                    }

                    job_id = ctx->job_manager->create_retrieve_job(
                        source_node_id, study_uid, series_opt, priority);
                    break;
                }

                case client::job_type::store: {
                    auto destination_node_id = get_json_string_value(req.body, "destination_node_id");
                    auto instance_uids = get_json_string_array(req.body, "instance_uids");

                    if (destination_node_id.empty()) {
                        res.code = 400;
                        res.body = make_error_json("INVALID_REQUEST",
                                                   "destination_node_id is required for store job");
                        return res;
                    }

                    if (instance_uids.empty()) {
                        res.code = 400;
                        res.body = make_error_json("INVALID_REQUEST",
                                                   "instance_uids is required for store job");
                        return res;
                    }

                    job_id = ctx->job_manager->create_store_job(
                        destination_node_id, instance_uids, priority);
                    break;
                }

                case client::job_type::query: {
                    auto node_id = get_json_string_value(req.body, "node_id");
                    auto query_level = get_json_string_value(req.body, "query_level");

                    if (node_id.empty()) {
                        res.code = 400;
                        res.body = make_error_json("INVALID_REQUEST",
                                                   "node_id is required for query job");
                        return res;
                    }

                    if (query_level.empty()) {
                        query_level = "STUDY";  // Default to study level
                    }

                    // Parse query keys (simplified - in production use proper JSON parser)
                    std::unordered_map<std::string, std::string> query_keys;
                    auto patient_id = get_json_string_value(req.body, "patient_id");
                    if (!patient_id.empty()) {
                        query_keys["PatientID"] = patient_id;
                    }
                    auto patient_name = get_json_string_value(req.body, "patient_name");
                    if (!patient_name.empty()) {
                        query_keys["PatientName"] = patient_name;
                    }

                    job_id = ctx->job_manager->create_query_job(
                        node_id, query_level, query_keys, priority);
                    break;
                }

                case client::job_type::sync: {
                    auto source_node_id = get_json_string_value(req.body, "source_node_id");

                    if (source_node_id.empty()) {
                        res.code = 400;
                        res.body = make_error_json("INVALID_REQUEST",
                                                   "source_node_id is required for sync job");
                        return res;
                    }

                    auto patient_id = get_json_string_value(req.body, "patient_id");
                    std::optional<std::string_view> patient_opt;
                    if (!patient_id.empty()) {
                        patient_opt = patient_id;
                    }

                    job_id = ctx->job_manager->create_sync_job(
                        source_node_id, patient_opt, priority);
                    break;
                }

                case client::job_type::prefetch: {
                    auto source_node_id = get_json_string_value(req.body, "source_node_id");
                    auto patient_id = get_json_string_value(req.body, "patient_id");

                    if (source_node_id.empty()) {
                        res.code = 400;
                        res.body = make_error_json("INVALID_REQUEST",
                                                   "source_node_id is required for prefetch job");
                        return res;
                    }

                    if (patient_id.empty()) {
                        res.code = 400;
                        res.body = make_error_json("INVALID_REQUEST",
                                                   "patient_id is required for prefetch job");
                        return res;
                    }

                    job_id = ctx->job_manager->create_prefetch_job(
                        source_node_id, patient_id, priority);
                    break;
                }

                default: {
                    res.code = 400;
                    res.body = make_error_json("INVALID_REQUEST",
                                               "Unsupported job type: " + type_str);
                    return res;
                }
            }

            // Retrieve and return the created job
            auto created_job = ctx->job_manager->get_job(job_id);
            if (!created_job) {
                res.code = 201;
                res.body = R"({"job_id":")" + json_escape(job_id) + R"(","status":"pending"})";
                return res;
            }

            res.code = 201;
            res.body = job_to_json(*created_job);
            return res;
        });

    // GET /api/v1/jobs/<jobId> - Get a specific job
    CROW_ROUTE(app, "/api/v1/jobs/<string>")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& /*req*/, const std::string& job_id) {
                crow::response res;
                res.add_header("Content-Type", "application/json");
                add_cors_headers(res, *ctx);

                if (!ctx->job_manager) {
                    res.code = 503;
                    res.body = make_error_json("SERVICE_UNAVAILABLE",
                                               "Job manager not configured");
                    return res;
                }

                auto job = ctx->job_manager->get_job(job_id);
                if (!job) {
                    res.code = 404;
                    res.body = make_error_json("NOT_FOUND", "Job not found");
                    return res;
                }

                res.code = 200;
                res.body = job_to_json(*job);
                return res;
            });

    // DELETE /api/v1/jobs/<jobId> - Delete a job
    CROW_ROUTE(app, "/api/v1/jobs/<string>")
        .methods(crow::HTTPMethod::DELETE)(
            [ctx](const crow::request& /*req*/, const std::string& job_id) {
                crow::response res;
                add_cors_headers(res, *ctx);

                if (!ctx->job_manager) {
                    res.add_header("Content-Type", "application/json");
                    res.code = 503;
                    res.body = make_error_json("SERVICE_UNAVAILABLE",
                                               "Job manager not configured");
                    return res;
                }

                // Check if job exists
                auto job = ctx->job_manager->get_job(job_id);
                if (!job) {
                    res.add_header("Content-Type", "application/json");
                    res.code = 404;
                    res.body = make_error_json("NOT_FOUND", "Job not found");
                    return res;
                }

                auto result = ctx->job_manager->delete_job(job_id);
                if (!result.is_ok()) {
                    res.add_header("Content-Type", "application/json");
                    res.code = 500;
                    res.body = make_error_json("DELETE_FAILED", result.error().message);
                    return res;
                }

                res.code = 204;
                return res;
            });

    // GET /api/v1/jobs/<jobId>/progress - Get job progress
    CROW_ROUTE(app, "/api/v1/jobs/<string>/progress")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& /*req*/, const std::string& job_id) {
                crow::response res;
                res.add_header("Content-Type", "application/json");
                add_cors_headers(res, *ctx);

                if (!ctx->job_manager) {
                    res.code = 503;
                    res.body = make_error_json("SERVICE_UNAVAILABLE",
                                               "Job manager not configured");
                    return res;
                }

                // Check if job exists
                auto job = ctx->job_manager->get_job(job_id);
                if (!job) {
                    res.code = 404;
                    res.body = make_error_json("NOT_FOUND", "Job not found");
                    return res;
                }

                auto progress = ctx->job_manager->get_progress(job_id);

                res.code = 200;
                res.body = progress_to_json(progress);
                return res;
            });

    // =========================================================================
    // Job Control Endpoints (Issue #559)
    // =========================================================================

    // POST /api/v1/jobs/<jobId>/start - Start a pending job
    CROW_ROUTE(app, "/api/v1/jobs/<string>/start")
        .methods(crow::HTTPMethod::POST)(
            [ctx](const crow::request& /*req*/, const std::string& job_id) {
                crow::response res;
                res.add_header("Content-Type", "application/json");
                add_cors_headers(res, *ctx);

                if (!ctx->job_manager) {
                    res.code = 503;
                    res.body = make_error_json("SERVICE_UNAVAILABLE",
                                               "Job manager not configured");
                    return res;
                }

                // Check if job exists
                auto job = ctx->job_manager->get_job(job_id);
                if (!job) {
                    res.code = 404;
                    res.body = make_error_json("NOT_FOUND", "Job not found");
                    return res;
                }

                auto result = ctx->job_manager->start_job(job_id);
                if (!result.is_ok()) {
                    res.code = 409;
                    res.body = make_error_json("INVALID_STATE_TRANSITION",
                                               result.error().message);
                    return res;
                }

                // Return updated job
                auto updated_job = ctx->job_manager->get_job(job_id);
                if (updated_job) {
                    res.code = 200;
                    res.body = job_to_json(*updated_job);
                } else {
                    res.code = 200;
                    res.body = R"({"job_id":")" + json_escape(job_id) +
                               R"(","message":"Job started"})";
                }
                return res;
            });

    // POST /api/v1/jobs/<jobId>/pause - Pause a running job
    CROW_ROUTE(app, "/api/v1/jobs/<string>/pause")
        .methods(crow::HTTPMethod::POST)(
            [ctx](const crow::request& /*req*/, const std::string& job_id) {
                crow::response res;
                res.add_header("Content-Type", "application/json");
                add_cors_headers(res, *ctx);

                if (!ctx->job_manager) {
                    res.code = 503;
                    res.body = make_error_json("SERVICE_UNAVAILABLE",
                                               "Job manager not configured");
                    return res;
                }

                // Check if job exists
                auto job = ctx->job_manager->get_job(job_id);
                if (!job) {
                    res.code = 404;
                    res.body = make_error_json("NOT_FOUND", "Job not found");
                    return res;
                }

                auto result = ctx->job_manager->pause_job(job_id);
                if (!result.is_ok()) {
                    res.code = 409;
                    res.body = make_error_json("INVALID_STATE_TRANSITION",
                                               result.error().message);
                    return res;
                }

                // Return updated job
                auto updated_job = ctx->job_manager->get_job(job_id);
                if (updated_job) {
                    res.code = 200;
                    res.body = job_to_json(*updated_job);
                } else {
                    res.code = 200;
                    res.body = R"({"job_id":")" + json_escape(job_id) +
                               R"(","message":"Job paused"})";
                }
                return res;
            });

    // POST /api/v1/jobs/<jobId>/resume - Resume a paused job
    CROW_ROUTE(app, "/api/v1/jobs/<string>/resume")
        .methods(crow::HTTPMethod::POST)(
            [ctx](const crow::request& /*req*/, const std::string& job_id) {
                crow::response res;
                res.add_header("Content-Type", "application/json");
                add_cors_headers(res, *ctx);

                if (!ctx->job_manager) {
                    res.code = 503;
                    res.body = make_error_json("SERVICE_UNAVAILABLE",
                                               "Job manager not configured");
                    return res;
                }

                // Check if job exists
                auto job = ctx->job_manager->get_job(job_id);
                if (!job) {
                    res.code = 404;
                    res.body = make_error_json("NOT_FOUND", "Job not found");
                    return res;
                }

                auto result = ctx->job_manager->resume_job(job_id);
                if (!result.is_ok()) {
                    res.code = 409;
                    res.body = make_error_json("INVALID_STATE_TRANSITION",
                                               result.error().message);
                    return res;
                }

                // Return updated job
                auto updated_job = ctx->job_manager->get_job(job_id);
                if (updated_job) {
                    res.code = 200;
                    res.body = job_to_json(*updated_job);
                } else {
                    res.code = 200;
                    res.body = R"({"job_id":")" + json_escape(job_id) +
                               R"(","message":"Job resumed"})";
                }
                return res;
            });

    // POST /api/v1/jobs/<jobId>/cancel - Cancel a job
    CROW_ROUTE(app, "/api/v1/jobs/<string>/cancel")
        .methods(crow::HTTPMethod::POST)(
            [ctx](const crow::request& /*req*/, const std::string& job_id) {
                crow::response res;
                res.add_header("Content-Type", "application/json");
                add_cors_headers(res, *ctx);

                if (!ctx->job_manager) {
                    res.code = 503;
                    res.body = make_error_json("SERVICE_UNAVAILABLE",
                                               "Job manager not configured");
                    return res;
                }

                // Check if job exists
                auto job = ctx->job_manager->get_job(job_id);
                if (!job) {
                    res.code = 404;
                    res.body = make_error_json("NOT_FOUND", "Job not found");
                    return res;
                }

                auto result = ctx->job_manager->cancel_job(job_id);
                if (!result.is_ok()) {
                    res.code = 409;
                    res.body = make_error_json("INVALID_STATE_TRANSITION",
                                               result.error().message);
                    return res;
                }

                // Return updated job
                auto updated_job = ctx->job_manager->get_job(job_id);
                if (updated_job) {
                    res.code = 200;
                    res.body = job_to_json(*updated_job);
                } else {
                    res.code = 200;
                    res.body = R"({"job_id":")" + json_escape(job_id) +
                               R"(","message":"Job cancelled"})";
                }
                return res;
            });

    // POST /api/v1/jobs/<jobId>/retry - Retry a failed job
    CROW_ROUTE(app, "/api/v1/jobs/<string>/retry")
        .methods(crow::HTTPMethod::POST)(
            [ctx](const crow::request& /*req*/, const std::string& job_id) {
                crow::response res;
                res.add_header("Content-Type", "application/json");
                add_cors_headers(res, *ctx);

                if (!ctx->job_manager) {
                    res.code = 503;
                    res.body = make_error_json("SERVICE_UNAVAILABLE",
                                               "Job manager not configured");
                    return res;
                }

                // Check if job exists
                auto job = ctx->job_manager->get_job(job_id);
                if (!job) {
                    res.code = 404;
                    res.body = make_error_json("NOT_FOUND", "Job not found");
                    return res;
                }

                auto result = ctx->job_manager->retry_job(job_id);
                if (!result.is_ok()) {
                    res.code = 409;
                    res.body = make_error_json("INVALID_STATE_TRANSITION",
                                               result.error().message);
                    return res;
                }

                // Return updated job
                auto updated_job = ctx->job_manager->get_job(job_id);
                if (updated_job) {
                    res.code = 200;
                    res.body = job_to_json(*updated_job);
                } else {
                    res.code = 200;
                    res.body = R"({"job_id":")" + json_escape(job_id) +
                               R"(","message":"Job retry queued"})";
                }
                return res;
            });

    // =========================================================================
    // WebSocket Endpoints (Issue #560)
    // =========================================================================

    // Register job_manager callbacks for broadcasting (only once)
    if (ctx->job_manager && !g_callbacks_registered.exchange(true)) {
        // Progress callback - broadcasts to all subscribers
        ctx->job_manager->set_progress_callback(
            [](const std::string& job_id, const client::job_progress& progress) {
                auto message = make_progress_message(job_id, progress);
                ws_subscriber_state::instance().broadcast_progress(job_id, message);
            });

        // Completion callback - broadcasts final status
        ctx->job_manager->set_completion_callback(
            [](const std::string& job_id, const client::job_record& record) {
                auto message = make_completion_message(job_id, record);
                ws_subscriber_state::instance().broadcast_progress(job_id, message);
            });
    }

    // WS /api/v1/jobs/<jobId>/progress/stream - Stream progress for specific job
    // Note: URL parameters in WebSocket routes require using onaccept to extract
    // the job_id from the request URL and store it in userdata
    CROW_WEBSOCKET_ROUTE(app, "/api/v1/jobs/<string>/progress/stream")
        .onaccept([ctx](const crow::request& req, void** userdata) -> bool {
            // Extract job_id from URL: /api/v1/jobs/{job_id}/progress/stream
            std::string url = req.url;
            // Find the job_id between /jobs/ and /progress
            const std::string prefix = "/api/v1/jobs/";
            const std::string suffix = "/progress/stream";

            if (url.find(prefix) != 0) {
                return false;
            }

            auto suffix_pos = url.rfind(suffix);
            if (suffix_pos == std::string::npos) {
                return false;
            }

            std::string job_id = url.substr(prefix.length(),
                                            suffix_pos - prefix.length());

            if (job_id.empty()) {
                return false;
            }

            // Verify job manager is available
            if (!ctx->job_manager) {
                return false;
            }

            // Verify job exists
            auto job = ctx->job_manager->get_job(job_id);
            if (!job) {
                return false;
            }

            // Store job_id in userdata (allocate on heap)
            *userdata = new std::string(job_id);
            return true;
        })
        .onopen([ctx](crow::websocket::connection& conn) {
            auto* job_id_ptr = static_cast<std::string*>(conn.userdata());
            if (!job_id_ptr || !ctx->job_manager) {
                conn.send_text(R"({"error":"Invalid connection state"})");
                conn.close("Invalid state");
                return;
            }

            const std::string& job_id = *job_id_ptr;

            // Subscribe to job updates
            ws_subscriber_state::instance().add_job_subscriber(job_id, &conn);

            // Send initial progress
            auto progress = ctx->job_manager->get_progress(job_id);
            conn.send_text(make_progress_message(job_id, progress));
        })
        .onclose([](crow::websocket::connection& conn,
                    const std::string& /*reason*/, uint16_t /*code*/) {
            auto* job_id_ptr = static_cast<std::string*>(conn.userdata());
            if (job_id_ptr) {
                ws_subscriber_state::instance().remove_job_subscriber(*job_id_ptr, &conn);
                delete job_id_ptr;
                conn.userdata(nullptr);
            }
        })
        .onmessage([](crow::websocket::connection& /*conn*/,
                      const std::string& /*data*/, bool /*is_binary*/) {
            // Client messages not expected, but handle gracefully
        });

    // WS /api/v1/jobs/stream - Stream all job updates
    CROW_WEBSOCKET_ROUTE(app, "/api/v1/jobs/stream")
        .onaccept([ctx](const crow::request& /*req*/, void** /*userdata*/) -> bool {
            // Only accept if job_manager is available
            return ctx->job_manager != nullptr;
        })
        .onopen([](crow::websocket::connection& conn) {
            // Subscribe to all job updates
            ws_subscriber_state::instance().add_all_jobs_subscriber(&conn);

            // Send initial connected message
            conn.send_text(R"({"type":"connected","message":"Subscribed to all job updates"})");
        })
        .onclose([](crow::websocket::connection& conn,
                    const std::string& /*reason*/, uint16_t /*code*/) {
            ws_subscriber_state::instance().remove_all_jobs_subscriber(&conn);
        })
        .onmessage([](crow::websocket::connection& /*conn*/,
                      const std::string& /*data*/, bool /*is_binary*/) {
            // Client messages not expected, but handle gracefully
        });
}

} // namespace pacs::web::endpoints
