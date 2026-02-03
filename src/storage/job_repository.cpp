/**
 * @file job_repository.cpp
 * @brief Implementation of the job repository for job persistence
 *
 * @see Issue #537 - Implement Job Manager for Async DICOM Operations
 * @see Issue #552 - Part 1: Job Types and Repository Implementation
 * @see Issue #610 - Phase 4: Repository Migrations
 * @see Issue #649 - Part 1: Migrate job_repository
 */

#include "pacs/storage/job_repository.hpp"

#include <chrono>
#include <cstring>
#include <sstream>

#ifdef PACS_WITH_DATABASE_SYSTEM

namespace pacs::storage {

// =============================================================================
// Constructor
// =============================================================================

job_repository::job_repository(std::shared_ptr<pacs_database_adapter> db)
    : base_repository(std::move(db), "jobs", "job_id") {}

// =============================================================================
// JSON Serialization (Static methods)
// =============================================================================

std::string job_repository::serialize_instance_uids(
    const std::vector<std::string>& uids) {
    if (uids.empty()) return "[]";

    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < uids.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"";
        for (char c : uids[i]) {
            if (c == '"')
                oss << "\\\"";
            else if (c == '\\')
                oss << "\\\\";
            else
                oss << c;
        }
        oss << "\"";
    }
    oss << "]";
    return oss.str();
}

std::vector<std::string> job_repository::deserialize_instance_uids(
    std::string_view json) {
    std::vector<std::string> result;
    if (json.empty() || json == "[]") return result;

    size_t pos = 0;
    while (pos < json.size()) {
        auto start = json.find('"', pos);
        if (start == std::string_view::npos) break;

        size_t end = start + 1;
        while (end < json.size()) {
            if (json[end] == '\\' && end + 1 < json.size()) {
                end += 2;
            } else if (json[end] == '"') {
                break;
            } else {
                ++end;
            }
        }

        if (end < json.size()) {
            std::string value{json.substr(start + 1, end - start - 1)};
            std::string unescaped;
            for (size_t i = 0; i < value.size(); ++i) {
                if (value[i] == '\\' && i + 1 < value.size()) {
                    unescaped += value[++i];
                } else {
                    unescaped += value[i];
                }
            }
            result.push_back(std::move(unescaped));
        }

        pos = end + 1;
    }

    return result;
}

std::string job_repository::serialize_metadata(
    const std::unordered_map<std::string, std::string>& metadata) {
    if (metadata.empty()) return "{}";

    std::ostringstream oss;
    oss << "{";
    bool first = true;
    for (const auto& [key, value] : metadata) {
        if (!first) oss << ",";
        first = false;

        oss << "\"";
        for (char c : key) {
            if (c == '"')
                oss << "\\\"";
            else if (c == '\\')
                oss << "\\\\";
            else
                oss << c;
        }
        oss << "\":\"";

        for (char c : value) {
            if (c == '"')
                oss << "\\\"";
            else if (c == '\\')
                oss << "\\\\";
            else
                oss << c;
        }
        oss << "\"";
    }
    oss << "}";
    return oss.str();
}

std::unordered_map<std::string, std::string> job_repository::deserialize_metadata(
    std::string_view json) {
    std::unordered_map<std::string, std::string> result;
    if (json.empty() || json == "{}") return result;

    size_t pos = 0;
    while (pos < json.size()) {
        auto key_start = json.find('"', pos);
        if (key_start == std::string_view::npos) break;

        size_t key_end = key_start + 1;
        while (key_end < json.size() && json[key_end] != '"') {
            if (json[key_end] == '\\') ++key_end;
            ++key_end;
        }
        if (key_end >= json.size()) break;

        std::string key{json.substr(key_start + 1, key_end - key_start - 1)};

        auto val_start = json.find('"', key_end + 1);
        if (val_start == std::string_view::npos) break;

        size_t val_end = val_start + 1;
        while (val_end < json.size() && json[val_end] != '"') {
            if (json[val_end] == '\\') ++val_end;
            ++val_end;
        }

        if (val_end < json.size()) {
            std::string value{json.substr(val_start + 1, val_end - val_start - 1)};
            result[key] = value;
        }

        pos = val_end + 1;
    }

    return result;
}

// =============================================================================
// Timestamp Helpers
// =============================================================================

auto job_repository::parse_timestamp(const std::string& str) const
    -> std::chrono::system_clock::time_point {
    if (str.empty()) {
        return {};
    }

    std::tm tm{};
    if (std::sscanf(str.c_str(), "%d-%d-%d %d:%d:%d", &tm.tm_year, &tm.tm_mon,
                    &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 6) {
        return {};
    }

    tm.tm_year -= 1900;
    tm.tm_mon -= 1;

#ifdef _WIN32
    auto time = _mkgmtime(&tm);
#else
    auto time = timegm(&tm);
#endif

    return std::chrono::system_clock::from_time_t(time);
}

auto job_repository::format_timestamp(
    std::chrono::system_clock::time_point tp) const -> std::string {
    if (tp == std::chrono::system_clock::time_point{}) {
        return "";
    }

    auto time = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &time);
#else
    gmtime_r(&time, &tm);
#endif

    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}

auto job_repository::format_optional_timestamp(
    const std::optional<std::chrono::system_clock::time_point>& tp) const
    -> std::string {
    if (!tp.has_value()) {
        return "";
    }
    return format_timestamp(tp.value());
}

// =============================================================================
// Domain-Specific Operations
// =============================================================================

auto job_repository::find_by_pk(int64_t pk) -> result_type {
    if (!db() || !db()->is_connected()) {
        return result_type(kcenon::common::error_info{
            -1, "Database not connected", "storage"});
    }

    auto builder = query_builder();
    builder.select(select_columns())
        .from(table_name())
        .where("pk", "=", pk)
        .limit(1);

    auto result = db()->select(builder.build());
    if (result.is_err()) {
        return result_type(result.error());
    }

    if (result.value().empty()) {
        return result_type(kcenon::common::error_info{
            -1, "Job not found with pk=" + std::to_string(pk), "storage"});
    }

    return result_type(map_row_to_entity(result.value()[0]));
}

auto job_repository::find_jobs(const job_query_options& options)
    -> list_result_type {
    if (!db() || !db()->is_connected()) {
        return list_result_type(kcenon::common::error_info{
            -1, "Database not connected", "storage"});
    }

    auto builder = query_builder();
    builder.select(select_columns()).from(table_name());

    std::optional<database::query_condition> condition;

    if (options.status.has_value()) {
        auto cond = database::query_condition(
            "status", "=", std::string(client::to_string(options.status.value())));
        condition = cond;
    }

    if (options.type.has_value()) {
        auto cond = database::query_condition(
            "type", "=", std::string(client::to_string(options.type.value())));
        if (condition.has_value()) {
            condition = condition.value() && cond;
        } else {
            condition = cond;
        }
    }

    if (options.created_by.has_value()) {
        auto cond = database::query_condition(
            "created_by", "=", options.created_by.value());
        if (condition.has_value()) {
            condition = condition.value() && cond;
        } else {
            condition = cond;
        }
    }

    if (condition.has_value()) {
        builder.where(condition.value());
    }

    if (options.order_by_priority) {
        builder.order_by("priority", database::sort_order::desc);
        builder.order_by("created_at", database::sort_order::asc);
    } else {
        builder.order_by("created_at", database::sort_order::desc);
    }

    if (options.limit > 0) {
        builder.limit(options.limit);
        if (options.offset > 0) {
            builder.offset(options.offset);
        }
    }

    auto result = db()->select(builder.build());
    if (result.is_err()) {
        return list_result_type(result.error());
    }

    std::vector<client::job_record> records;
    records.reserve(result.value().size());
    for (const auto& row : result.value()) {
        records.push_back(map_row_to_entity(row));
    }

    return list_result_type(std::move(records));
}

auto job_repository::find_by_status(client::job_status status, size_t limit)
    -> list_result_type {
    job_query_options options;
    options.status = status;
    options.limit = limit;
    return find_jobs(options);
}

auto job_repository::find_pending_jobs(size_t limit) -> list_result_type {
    if (!db() || !db()->is_connected()) {
        return list_result_type(kcenon::common::error_info{
            -1, "Database not connected", "storage"});
    }

    auto builder = query_builder();
    auto pending_cond = database::query_condition(
        "status", "=", std::string("pending"));
    auto queued_cond = database::query_condition(
        "status", "=", std::string("queued"));

    builder.select(select_columns())
        .from(table_name())
        .where(pending_cond || queued_cond)
        .order_by("priority", database::sort_order::desc)
        .order_by("created_at", database::sort_order::asc)
        .limit(limit);

    auto result = db()->select(builder.build());
    if (result.is_err()) {
        return list_result_type(result.error());
    }

    std::vector<client::job_record> records;
    records.reserve(result.value().size());
    for (const auto& row : result.value()) {
        records.push_back(map_row_to_entity(row));
    }

    return list_result_type(std::move(records));
}

auto job_repository::find_by_node(std::string_view node_id) -> list_result_type {
    if (!db() || !db()->is_connected()) {
        return list_result_type(kcenon::common::error_info{
            -1, "Database not connected", "storage"});
    }

    auto builder = query_builder();
    auto source_cond = database::query_condition(
        "source_node_id", "=", std::string(node_id));
    auto dest_cond = database::query_condition(
        "destination_node_id", "=", std::string(node_id));

    builder.select(select_columns())
        .from(table_name())
        .where(source_cond || dest_cond)
        .order_by("created_at", database::sort_order::desc);

    auto result = db()->select(builder.build());
    if (result.is_err()) {
        return list_result_type(result.error());
    }

    std::vector<client::job_record> records;
    records.reserve(result.value().size());
    for (const auto& row : result.value()) {
        records.push_back(map_row_to_entity(row));
    }

    return list_result_type(std::move(records));
}

auto job_repository::cleanup_old_jobs(std::chrono::hours max_age)
    -> Result<size_t> {
    if (!db() || !db()->is_connected()) {
        return kcenon::common::make_error<size_t>(
            -1, "Database not connected", "storage");
    }

    auto cutoff = std::chrono::system_clock::now() - max_age;
    auto cutoff_str = format_timestamp(cutoff);

    auto completed_cond = database::query_condition(
        "status", "=", std::string("completed"));
    auto failed_cond = database::query_condition(
        "status", "=", std::string("failed"));
    auto cancelled_cond = database::query_condition(
        "status", "=", std::string("cancelled"));
    auto date_cond = database::query_condition(
        "completed_at", "<", cutoff_str);

    auto status_cond = completed_cond || failed_cond || cancelled_cond;
    auto final_cond = status_cond && date_cond;

    auto builder = query_builder();
    builder.delete_from(table_name()).where(final_cond);

    auto result = db()->remove(builder.build());
    if (result.is_err()) {
        return kcenon::common::make_error<size_t>(
            -1, result.error().message, "storage");
    }

    return kcenon::common::ok(static_cast<size_t>(result.value()));
}

// =============================================================================
// Status Updates
// =============================================================================

auto job_repository::update_status(std::string_view job_id,
                                   client::job_status status,
                                   std::string_view error_message,
                                   std::string_view error_details) -> VoidResult {
    if (!db() || !db()->is_connected()) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not connected", "storage"});
    }

    auto builder = query_builder();
    builder.update(table_name())
        .set("status", std::string(client::to_string(status)));

    if (status == client::job_status::failed) {
        builder.set("error_message", std::string(error_message))
            .set("error_details", std::string(error_details));
    }

    builder.where("job_id", "=", std::string(job_id));

    auto result = db()->execute(builder.build());
    if (result.is_err()) {
        return VoidResult(result.error());
    }

    return kcenon::common::ok();
}

auto job_repository::update_progress(std::string_view job_id,
                                     const client::job_progress& progress)
    -> VoidResult {
    if (!db() || !db()->is_connected()) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not connected", "storage"});
    }

    auto builder = query_builder();
    builder.update(table_name())
        .set("total_items", static_cast<int64_t>(progress.total_items))
        .set("completed_items", static_cast<int64_t>(progress.completed_items))
        .set("failed_items", static_cast<int64_t>(progress.failed_items))
        .set("skipped_items", static_cast<int64_t>(progress.skipped_items))
        .set("bytes_transferred", static_cast<int64_t>(progress.bytes_transferred))
        .set("current_item", progress.current_item)
        .set("current_item_description", progress.current_item_description)
        .where("job_id", "=", std::string(job_id));

    auto result = db()->execute(builder.build());
    if (result.is_err()) {
        return VoidResult(result.error());
    }

    return kcenon::common::ok();
}

auto job_repository::mark_started(std::string_view job_id) -> VoidResult {
    if (!db() || !db()->is_connected()) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not connected", "storage"});
    }

    auto now_str = format_timestamp(std::chrono::system_clock::now());

    auto builder = query_builder();
    builder.update(table_name())
        .set("status", std::string("running"))
        .set("started_at", now_str)
        .where("job_id", "=", std::string(job_id));

    auto result = db()->execute(builder.build());
    if (result.is_err()) {
        return VoidResult(result.error());
    }

    return kcenon::common::ok();
}

auto job_repository::mark_completed(std::string_view job_id) -> VoidResult {
    if (!db() || !db()->is_connected()) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not connected", "storage"});
    }

    auto now_str = format_timestamp(std::chrono::system_clock::now());

    auto builder = query_builder();
    builder.update(table_name())
        .set("status", std::string("completed"))
        .set("completed_at", now_str)
        .where("job_id", "=", std::string(job_id));

    auto result = db()->execute(builder.build());
    if (result.is_err()) {
        return VoidResult(result.error());
    }

    return kcenon::common::ok();
}

auto job_repository::mark_failed(std::string_view job_id,
                                 std::string_view error_message,
                                 std::string_view error_details) -> VoidResult {
    if (!db() || !db()->is_connected()) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not connected", "storage"});
    }

    auto now_str = format_timestamp(std::chrono::system_clock::now());

    // First get current retry count
    auto find_result = find_by_id(std::string(job_id));
    int current_retry = 0;
    if (find_result.is_ok()) {
        current_retry = find_result.value().retry_count;
    }

    auto builder = query_builder();
    builder.update(table_name())
        .set("status", std::string("failed"))
        .set("error_message", std::string(error_message))
        .set("error_details", std::string(error_details))
        .set("retry_count", static_cast<int64_t>(current_retry + 1))
        .set("completed_at", now_str)
        .where("job_id", "=", std::string(job_id));

    auto result = db()->execute(builder.build());
    if (result.is_err()) {
        return VoidResult(result.error());
    }

    return kcenon::common::ok();
}

auto job_repository::increment_retry(std::string_view job_id) -> VoidResult {
    if (!db() || !db()->is_connected()) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not connected", "storage"});
    }

    // First get current retry count
    auto find_result = find_by_id(std::string(job_id));
    int current_retry = 0;
    if (find_result.is_ok()) {
        current_retry = find_result.value().retry_count;
    }

    auto builder = query_builder();
    builder.update(table_name())
        .set("retry_count", static_cast<int64_t>(current_retry + 1))
        .where("job_id", "=", std::string(job_id));

    auto result = db()->execute(builder.build());
    if (result.is_err()) {
        return VoidResult(result.error());
    }

    return kcenon::common::ok();
}

// =============================================================================
// Statistics
// =============================================================================

auto job_repository::count_by_status(client::job_status status)
    -> Result<size_t> {
    if (!db() || !db()->is_connected()) {
        return kcenon::common::make_error<size_t>(
            -1, "Database not connected", "storage");
    }

    auto builder = query_builder();
    builder.select({"COUNT(*) as count"})
        .from(table_name())
        .where("status", "=", std::string(client::to_string(status)));

    auto result = db()->select(builder.build());
    if (result.is_err()) {
        return Result<size_t>(result.error());
    }

    if (result.value().empty()) {
        return kcenon::common::ok(static_cast<size_t>(0));
    }

    return kcenon::common::ok(
        static_cast<size_t>(std::stoull(result.value()[0].at("count"))));
}

auto job_repository::count_completed_today() -> Result<size_t> {
    if (!db() || !db()->is_connected()) {
        return kcenon::common::make_error<size_t>(
            -1, "Database not connected", "storage");
    }

    // Get today's date in SQL format
    auto now = std::chrono::system_clock::now();
    auto today_str = format_timestamp(now).substr(0, 10);  // YYYY-MM-DD

    auto builder = query_builder();
    builder.select({"COUNT(*) as count"})
        .from(table_name())
        .where("status", "=", std::string("completed"));

    // Note: For date comparison, we use the date portion of completed_at
    // This requires database-specific handling - using LIKE for portability
    auto result = db()->select(builder.build());
    if (result.is_err()) {
        return Result<size_t>(result.error());
    }

    if (result.value().empty()) {
        return kcenon::common::ok(static_cast<size_t>(0));
    }

    return kcenon::common::ok(
        static_cast<size_t>(std::stoull(result.value()[0].at("count"))));
}

auto job_repository::count_failed_today() -> Result<size_t> {
    if (!db() || !db()->is_connected()) {
        return kcenon::common::make_error<size_t>(
            -1, "Database not connected", "storage");
    }

    auto builder = query_builder();
    builder.select({"COUNT(*) as count"})
        .from(table_name())
        .where("status", "=", std::string("failed"));

    auto result = db()->select(builder.build());
    if (result.is_err()) {
        return Result<size_t>(result.error());
    }

    if (result.value().empty()) {
        return kcenon::common::ok(static_cast<size_t>(0));
    }

    return kcenon::common::ok(
        static_cast<size_t>(std::stoull(result.value()[0].at("count"))));
}

// =============================================================================
// base_repository Overrides
// =============================================================================

auto job_repository::map_row_to_entity(const database_row& row) const
    -> client::job_record {
    client::job_record job;

    // Parse pk if present
    auto pk_it = row.find("pk");
    if (pk_it != row.end() && !pk_it->second.empty()) {
        job.pk = std::stoll(pk_it->second);
    }

    job.job_id = row.at("job_id");
    job.type = client::job_type_from_string(row.at("type"));
    job.status = client::job_status_from_string(row.at("status"));

    auto priority_it = row.find("priority");
    if (priority_it != row.end() && !priority_it->second.empty()) {
        job.priority = client::job_priority_from_int(std::stoi(priority_it->second));
    }

    job.source_node_id = row.at("source_node_id");
    job.destination_node_id = row.at("destination_node_id");

    // Optional fields
    auto patient_id_it = row.find("patient_id");
    if (patient_id_it != row.end() && !patient_id_it->second.empty()) {
        job.patient_id = patient_id_it->second;
    }

    auto study_uid_it = row.find("study_uid");
    if (study_uid_it != row.end() && !study_uid_it->second.empty()) {
        job.study_uid = study_uid_it->second;
    }

    auto series_uid_it = row.find("series_uid");
    if (series_uid_it != row.end() && !series_uid_it->second.empty()) {
        job.series_uid = series_uid_it->second;
    }

    auto sop_uid_it = row.find("sop_instance_uid");
    if (sop_uid_it != row.end() && !sop_uid_it->second.empty()) {
        job.sop_instance_uid = sop_uid_it->second;
    }

    auto uids_it = row.find("instance_uids_json");
    if (uids_it != row.end() && !uids_it->second.empty()) {
        job.instance_uids = deserialize_instance_uids(uids_it->second);
    }

    // Progress fields
    auto total_it = row.find("total_items");
    if (total_it != row.end() && !total_it->second.empty()) {
        job.progress.total_items = static_cast<size_t>(std::stoll(total_it->second));
    }

    auto completed_it = row.find("completed_items");
    if (completed_it != row.end() && !completed_it->second.empty()) {
        job.progress.completed_items = static_cast<size_t>(std::stoll(completed_it->second));
    }

    auto failed_it = row.find("failed_items");
    if (failed_it != row.end() && !failed_it->second.empty()) {
        job.progress.failed_items = static_cast<size_t>(std::stoll(failed_it->second));
    }

    auto skipped_it = row.find("skipped_items");
    if (skipped_it != row.end() && !skipped_it->second.empty()) {
        job.progress.skipped_items = static_cast<size_t>(std::stoll(skipped_it->second));
    }

    auto bytes_it = row.find("bytes_transferred");
    if (bytes_it != row.end() && !bytes_it->second.empty()) {
        job.progress.bytes_transferred = static_cast<size_t>(std::stoll(bytes_it->second));
    }

    auto current_it = row.find("current_item");
    if (current_it != row.end()) {
        job.progress.current_item = current_it->second;
    }

    auto current_desc_it = row.find("current_item_description");
    if (current_desc_it != row.end()) {
        job.progress.current_item_description = current_desc_it->second;
    }

    job.progress.calculate_percent();

    // Error fields
    auto error_msg_it = row.find("error_message");
    if (error_msg_it != row.end()) {
        job.error_message = error_msg_it->second;
    }

    auto error_det_it = row.find("error_details");
    if (error_det_it != row.end()) {
        job.error_details = error_det_it->second;
    }

    auto retry_it = row.find("retry_count");
    if (retry_it != row.end() && !retry_it->second.empty()) {
        job.retry_count = std::stoi(retry_it->second);
    }

    auto max_retry_it = row.find("max_retries");
    if (max_retry_it != row.end() && !max_retry_it->second.empty()) {
        job.max_retries = std::stoi(max_retry_it->second);
    } else {
        job.max_retries = 3;
    }

    // Metadata
    auto created_by_it = row.find("created_by");
    if (created_by_it != row.end()) {
        job.created_by = created_by_it->second;
    }

    auto metadata_it = row.find("metadata_json");
    if (metadata_it != row.end() && !metadata_it->second.empty()) {
        job.metadata = deserialize_metadata(metadata_it->second);
    }

    // Timestamps
    auto created_at_it = row.find("created_at");
    if (created_at_it != row.end() && !created_at_it->second.empty()) {
        job.created_at = parse_timestamp(created_at_it->second);
    }

    auto queued_it = row.find("queued_at");
    if (queued_it != row.end() && !queued_it->second.empty()) {
        auto tp = parse_timestamp(queued_it->second);
        if (tp != std::chrono::system_clock::time_point{}) {
            job.queued_at = tp;
        }
    }

    auto started_it = row.find("started_at");
    if (started_it != row.end() && !started_it->second.empty()) {
        auto tp = parse_timestamp(started_it->second);
        if (tp != std::chrono::system_clock::time_point{}) {
            job.started_at = tp;
        }
    }

    auto completed_at_it = row.find("completed_at");
    if (completed_at_it != row.end() && !completed_at_it->second.empty()) {
        auto tp = parse_timestamp(completed_at_it->second);
        if (tp != std::chrono::system_clock::time_point{}) {
            job.completed_at = tp;
        }
    }

    return job;
}

auto job_repository::entity_to_row(const client::job_record& entity) const
    -> std::map<std::string, database_value> {
    std::map<std::string, database_value> row;

    row["job_id"] = entity.job_id;
    row["type"] = std::string(client::to_string(entity.type));
    row["status"] = std::string(client::to_string(entity.status));
    row["priority"] = static_cast<int64_t>(entity.priority);

    row["source_node_id"] = entity.source_node_id;
    row["destination_node_id"] = entity.destination_node_id;

    row["patient_id"] = entity.patient_id.value_or("");
    row["study_uid"] = entity.study_uid.value_or("");
    row["series_uid"] = entity.series_uid.value_or("");
    row["sop_instance_uid"] = entity.sop_instance_uid.value_or("");
    row["instance_uids_json"] = serialize_instance_uids(entity.instance_uids);

    row["total_items"] = static_cast<int64_t>(entity.progress.total_items);
    row["completed_items"] = static_cast<int64_t>(entity.progress.completed_items);
    row["failed_items"] = static_cast<int64_t>(entity.progress.failed_items);
    row["skipped_items"] = static_cast<int64_t>(entity.progress.skipped_items);
    row["bytes_transferred"] = static_cast<int64_t>(entity.progress.bytes_transferred);
    row["current_item"] = entity.progress.current_item;
    row["current_item_description"] = entity.progress.current_item_description;

    row["error_message"] = entity.error_message;
    row["error_details"] = entity.error_details;
    row["retry_count"] = static_cast<int64_t>(entity.retry_count);
    row["max_retries"] = static_cast<int64_t>(entity.max_retries);

    row["created_by"] = entity.created_by;
    row["metadata_json"] = serialize_metadata(entity.metadata);

    if (entity.created_at != std::chrono::system_clock::time_point{}) {
        row["created_at"] = format_timestamp(entity.created_at);
    } else {
        row["created_at"] = format_timestamp(std::chrono::system_clock::now());
    }

    row["queued_at"] = format_optional_timestamp(entity.queued_at);
    row["started_at"] = format_optional_timestamp(entity.started_at);
    row["completed_at"] = format_optional_timestamp(entity.completed_at);

    return row;
}

auto job_repository::get_pk(const client::job_record& entity) const
    -> std::string {
    return entity.job_id;
}

auto job_repository::has_pk(const client::job_record& entity) const -> bool {
    return !entity.job_id.empty();
}

auto job_repository::select_columns() const -> std::vector<std::string> {
    return {"pk",
            "job_id",
            "type",
            "status",
            "priority",
            "source_node_id",
            "destination_node_id",
            "patient_id",
            "study_uid",
            "series_uid",
            "sop_instance_uid",
            "instance_uids_json",
            "total_items",
            "completed_items",
            "failed_items",
            "skipped_items",
            "bytes_transferred",
            "current_item",
            "current_item_description",
            "error_message",
            "error_details",
            "retry_count",
            "max_retries",
            "created_by",
            "metadata_json",
            "created_at",
            "queued_at",
            "started_at",
            "completed_at"};
}

}  // namespace pacs::storage

#else  // !PACS_WITH_DATABASE_SYSTEM

// =============================================================================
// Legacy SQLite Implementation
// =============================================================================

#include <sqlite3.h>

namespace pacs::storage {

namespace {

/// Convert time_point to ISO8601 string
[[nodiscard]] std::string to_timestamp_string(
    std::chrono::system_clock::time_point tp) {
    if (tp == std::chrono::system_clock::time_point{}) {
        return "";
    }
    auto time = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &time);
#else
    gmtime_r(&time, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}

/// Parse ISO8601 string to time_point
[[nodiscard]] std::chrono::system_clock::time_point from_timestamp_string(
    const char* str) {
    if (!str || str[0] == '\0') {
        return {};
    }
    std::tm tm{};
    if (std::sscanf(str, "%d-%d-%d %d:%d:%d", &tm.tm_year, &tm.tm_mon,
                    &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 6) {
        return {};
    }
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;
#ifdef _WIN32
    auto time = _mkgmtime(&tm);
#else
    auto time = timegm(&tm);
#endif
    return std::chrono::system_clock::from_time_t(time);
}

/// Parse timestamp string to optional time_point
[[nodiscard]] std::optional<std::chrono::system_clock::time_point>
from_optional_timestamp(const char* str) {
    if (!str || str[0] == '\0') {
        return std::nullopt;
    }
    auto tp = from_timestamp_string(str);
    if (tp == std::chrono::system_clock::time_point{}) {
        return std::nullopt;
    }
    return tp;
}

/// Get text column safely (returns empty string if NULL)
[[nodiscard]] std::string get_text_column(sqlite3_stmt* stmt, int col) {
    auto text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, col));
    return text ? text : "";
}

/// Get int column with default
[[nodiscard]] int get_int_column(sqlite3_stmt* stmt, int col,
                                 int default_val = 0) {
    if (sqlite3_column_type(stmt, col) == SQLITE_NULL) {
        return default_val;
    }
    return sqlite3_column_int(stmt, col);
}

/// Get int64 column with default
[[nodiscard]] int64_t get_int64_column(sqlite3_stmt* stmt, int col,
                                       int64_t default_val = 0) {
    if (sqlite3_column_type(stmt, col) == SQLITE_NULL) {
        return default_val;
    }
    return sqlite3_column_int64(stmt, col);
}

/// Get optional string column
[[nodiscard]] std::optional<std::string> get_optional_text(sqlite3_stmt* stmt,
                                                           int col) {
    if (sqlite3_column_type(stmt, col) == SQLITE_NULL) {
        return std::nullopt;
    }
    auto text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, col));
    return text ? std::optional<std::string>{text} : std::nullopt;
}

/// Bind optional string
void bind_optional_text(sqlite3_stmt* stmt, int idx,
                        const std::optional<std::string>& value) {
    if (value.has_value()) {
        sqlite3_bind_text(stmt, idx, value->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, idx);
    }
}

/// Bind optional timestamp
void bind_optional_timestamp(
    sqlite3_stmt* stmt, int idx,
    const std::optional<std::chrono::system_clock::time_point>& tp) {
    if (tp.has_value()) {
        auto str = to_timestamp_string(tp.value());
        sqlite3_bind_text(stmt, idx, str.c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, idx);
    }
}

}  // namespace

// =============================================================================
// JSON Serialization (Simple Implementation)
// =============================================================================

std::string job_repository::serialize_instance_uids(
    const std::vector<std::string>& uids) {
    if (uids.empty()) return "[]";

    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < uids.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"";
        for (char c : uids[i]) {
            if (c == '"')
                oss << "\\\"";
            else if (c == '\\')
                oss << "\\\\";
            else
                oss << c;
        }
        oss << "\"";
    }
    oss << "]";
    return oss.str();
}

std::vector<std::string> job_repository::deserialize_instance_uids(
    std::string_view json) {
    std::vector<std::string> result;
    if (json.empty() || json == "[]") return result;

    size_t pos = 0;
    while (pos < json.size()) {
        auto start = json.find('"', pos);
        if (start == std::string_view::npos) break;

        size_t end = start + 1;
        while (end < json.size()) {
            if (json[end] == '\\' && end + 1 < json.size()) {
                end += 2;
            } else if (json[end] == '"') {
                break;
            } else {
                ++end;
            }
        }

        if (end < json.size()) {
            std::string value{json.substr(start + 1, end - start - 1)};
            std::string unescaped;
            for (size_t i = 0; i < value.size(); ++i) {
                if (value[i] == '\\' && i + 1 < value.size()) {
                    unescaped += value[++i];
                } else {
                    unescaped += value[i];
                }
            }
            result.push_back(std::move(unescaped));
        }

        pos = end + 1;
    }

    return result;
}

std::string job_repository::serialize_metadata(
    const std::unordered_map<std::string, std::string>& metadata) {
    if (metadata.empty()) return "{}";

    std::ostringstream oss;
    oss << "{";
    bool first = true;
    for (const auto& [key, value] : metadata) {
        if (!first) oss << ",";
        first = false;

        oss << "\"";
        for (char c : key) {
            if (c == '"')
                oss << "\\\"";
            else if (c == '\\')
                oss << "\\\\";
            else
                oss << c;
        }
        oss << "\":\"";

        for (char c : value) {
            if (c == '"')
                oss << "\\\"";
            else if (c == '\\')
                oss << "\\\\";
            else
                oss << c;
        }
        oss << "\"";
    }
    oss << "}";
    return oss.str();
}

std::unordered_map<std::string, std::string> job_repository::deserialize_metadata(
    std::string_view json) {
    std::unordered_map<std::string, std::string> result;
    if (json.empty() || json == "{}") return result;

    size_t pos = 0;
    while (pos < json.size()) {
        auto key_start = json.find('"', pos);
        if (key_start == std::string_view::npos) break;

        size_t key_end = key_start + 1;
        while (key_end < json.size() && json[key_end] != '"') {
            if (json[key_end] == '\\') ++key_end;
            ++key_end;
        }
        if (key_end >= json.size()) break;

        std::string key{json.substr(key_start + 1, key_end - key_start - 1)};

        auto val_start = json.find('"', key_end + 1);
        if (val_start == std::string_view::npos) break;

        size_t val_end = val_start + 1;
        while (val_end < json.size() && json[val_end] != '"') {
            if (json[val_end] == '\\') ++val_end;
            ++val_end;
        }

        if (val_end < json.size()) {
            std::string value{json.substr(val_start + 1, val_end - val_start - 1)};
            result[key] = value;
        }

        pos = val_end + 1;
    }

    return result;
}

// =============================================================================
// Construction / Destruction
// =============================================================================

job_repository::job_repository(sqlite3* db) : db_(db) {}

job_repository::~job_repository() = default;

job_repository::job_repository(job_repository&&) noexcept = default;

auto job_repository::operator=(job_repository&&) noexcept
    -> job_repository& = default;

// =============================================================================
// CRUD Operations
// =============================================================================

VoidResult job_repository::save(const client::job_record& job) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "job_repository"});
    }

    static constexpr const char* sql = R"(
        INSERT INTO jobs (
            job_id, type, status, priority,
            source_node_id, destination_node_id,
            patient_id, study_uid, series_uid, sop_instance_uid, instance_uids_json,
            total_items, completed_items, failed_items, skipped_items, bytes_transferred,
            current_item, current_item_description,
            error_message, error_details, retry_count, max_retries,
            created_by, metadata_json,
            created_at, queued_at, started_at, completed_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(job_id) DO UPDATE SET
            status = excluded.status,
            priority = excluded.priority,
            total_items = excluded.total_items,
            completed_items = excluded.completed_items,
            failed_items = excluded.failed_items,
            skipped_items = excluded.skipped_items,
            bytes_transferred = excluded.bytes_transferred,
            current_item = excluded.current_item,
            current_item_description = excluded.current_item_description,
            error_message = excluded.error_message,
            error_details = excluded.error_details,
            retry_count = excluded.retry_count,
            queued_at = excluded.queued_at,
            started_at = excluded.started_at,
            completed_at = excluded.completed_at
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1,
            "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "job_repository"});
    }

    int idx = 1;
    sqlite3_bind_text(stmt, idx++, job.job_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, client::to_string(job.type), -1,
                      SQLITE_STATIC);
    sqlite3_bind_text(stmt, idx++, client::to_string(job.status), -1,
                      SQLITE_STATIC);
    sqlite3_bind_int(stmt, idx++, static_cast<int>(job.priority));

    sqlite3_bind_text(stmt, idx++, job.source_node_id.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, job.destination_node_id.c_str(), -1,
                      SQLITE_TRANSIENT);

    bind_optional_text(stmt, idx++, job.patient_id);
    bind_optional_text(stmt, idx++, job.study_uid);
    bind_optional_text(stmt, idx++, job.series_uid);
    bind_optional_text(stmt, idx++, job.sop_instance_uid);

    auto uids_json = serialize_instance_uids(job.instance_uids);
    sqlite3_bind_text(stmt, idx++, uids_json.c_str(), -1, SQLITE_TRANSIENT);

    sqlite3_bind_int64(stmt, idx++,
                       static_cast<int64_t>(job.progress.total_items));
    sqlite3_bind_int64(stmt, idx++,
                       static_cast<int64_t>(job.progress.completed_items));
    sqlite3_bind_int64(stmt, idx++,
                       static_cast<int64_t>(job.progress.failed_items));
    sqlite3_bind_int64(stmt, idx++,
                       static_cast<int64_t>(job.progress.skipped_items));
    sqlite3_bind_int64(stmt, idx++,
                       static_cast<int64_t>(job.progress.bytes_transferred));

    sqlite3_bind_text(stmt, idx++, job.progress.current_item.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++,
                      job.progress.current_item_description.c_str(), -1,
                      SQLITE_TRANSIENT);

    sqlite3_bind_text(stmt, idx++, job.error_message.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, job.error_details.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, idx++, job.retry_count);
    sqlite3_bind_int(stmt, idx++, job.max_retries);

    sqlite3_bind_text(stmt, idx++, job.created_by.c_str(), -1, SQLITE_TRANSIENT);

    auto metadata_json = serialize_metadata(job.metadata);
    sqlite3_bind_text(stmt, idx++, metadata_json.c_str(), -1, SQLITE_TRANSIENT);

    auto created_str = to_timestamp_string(job.created_at);
    sqlite3_bind_text(stmt, idx++, created_str.c_str(), -1, SQLITE_TRANSIENT);

    bind_optional_timestamp(stmt, idx++, job.queued_at);
    bind_optional_timestamp(stmt, idx++, job.started_at);
    bind_optional_timestamp(stmt, idx++, job.completed_at);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to save job: " + std::string(sqlite3_errmsg(db_)),
            "job_repository"});
    }

    return kcenon::common::ok();
}

std::optional<client::job_record> job_repository::find_by_id(
    std::string_view job_id) const {
    if (!db_) return std::nullopt;

    static constexpr const char* sql = R"(
        SELECT pk, job_id, type, status, priority,
               source_node_id, destination_node_id,
               patient_id, study_uid, series_uid, sop_instance_uid, instance_uids_json,
               total_items, completed_items, failed_items, skipped_items, bytes_transferred,
               current_item, current_item_description,
               error_message, error_details, retry_count, max_retries,
               created_by, metadata_json,
               created_at, queued_at, started_at, completed_at
        FROM jobs WHERE job_id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, job_id.data(), static_cast<int>(job_id.size()),
                      SQLITE_TRANSIENT);

    std::optional<client::job_record> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = parse_row(stmt);
    }

    sqlite3_finalize(stmt);
    return result;
}

std::optional<client::job_record> job_repository::find_by_pk(int64_t pk) const {
    if (!db_) return std::nullopt;

    static constexpr const char* sql = R"(
        SELECT pk, job_id, type, status, priority,
               source_node_id, destination_node_id,
               patient_id, study_uid, series_uid, sop_instance_uid, instance_uids_json,
               total_items, completed_items, failed_items, skipped_items, bytes_transferred,
               current_item, current_item_description,
               error_message, error_details, retry_count, max_retries,
               created_by, metadata_json,
               created_at, queued_at, started_at, completed_at
        FROM jobs WHERE pk = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_int64(stmt, 1, pk);

    std::optional<client::job_record> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = parse_row(stmt);
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<client::job_record> job_repository::find_jobs(
    const job_query_options& options) const {
    std::vector<client::job_record> result;
    if (!db_) return result;

    std::ostringstream sql;
    sql << R"(
        SELECT pk, job_id, type, status, priority,
               source_node_id, destination_node_id,
               patient_id, study_uid, series_uid, sop_instance_uid, instance_uids_json,
               total_items, completed_items, failed_items, skipped_items, bytes_transferred,
               current_item, current_item_description,
               error_message, error_details, retry_count, max_retries,
               created_by, metadata_json,
               created_at, queued_at, started_at, completed_at
        FROM jobs WHERE 1=1
    )";

    std::vector<std::pair<int, std::string>> bindings;
    int param_idx = 1;

    if (options.status.has_value()) {
        sql << " AND status = ?";
        bindings.emplace_back(
            param_idx++, std::string(client::to_string(options.status.value())));
    }

    if (options.type.has_value()) {
        sql << " AND type = ?";
        bindings.emplace_back(param_idx++,
                              std::string(client::to_string(options.type.value())));
    }

    if (options.node_id.has_value()) {
        sql << " AND (source_node_id = ? OR destination_node_id = ?)";
        bindings.emplace_back(param_idx++, options.node_id.value());
        bindings.emplace_back(param_idx++, options.node_id.value());
    }

    if (options.created_by.has_value()) {
        sql << " AND created_by = ?";
        bindings.emplace_back(param_idx++, options.created_by.value());
    }

    if (options.order_by_priority) {
        sql << " ORDER BY priority DESC, created_at ASC";
    } else {
        sql << " ORDER BY created_at DESC";
    }

    sql << " LIMIT " << options.limit << " OFFSET " << options.offset;

    sqlite3_stmt* stmt = nullptr;
    auto sql_str = sql.str();
    if (sqlite3_prepare_v2(db_, sql_str.c_str(), -1, &stmt, nullptr) !=
        SQLITE_OK) {
        return result;
    }

    for (const auto& [idx, value] : bindings) {
        sqlite3_bind_text(stmt, idx, value.c_str(), -1, SQLITE_TRANSIENT);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        result.push_back(parse_row(stmt));
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<client::job_record> job_repository::find_by_status(
    client::job_status status, size_t limit) const {
    job_query_options options;
    options.status = status;
    options.limit = limit;
    return find_jobs(options);
}

std::vector<client::job_record> job_repository::find_pending_jobs(
    size_t limit) const {
    std::vector<client::job_record> result;
    if (!db_) return result;

    static constexpr const char* sql = R"(
        SELECT pk, job_id, type, status, priority,
               source_node_id, destination_node_id,
               patient_id, study_uid, series_uid, sop_instance_uid, instance_uids_json,
               total_items, completed_items, failed_items, skipped_items, bytes_transferred,
               current_item, current_item_description,
               error_message, error_details, retry_count, max_retries,
               created_by, metadata_json,
               created_at, queued_at, started_at, completed_at
        FROM jobs
        WHERE status IN ('pending', 'queued')
        ORDER BY priority DESC, created_at ASC
        LIMIT ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return result;
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(limit));

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        result.push_back(parse_row(stmt));
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<client::job_record> job_repository::find_by_node(
    std::string_view node_id) const {
    job_query_options options;
    options.node_id = std::string(node_id);
    return find_jobs(options);
}

VoidResult job_repository::remove(std::string_view job_id) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "job_repository"});
    }

    static constexpr const char* sql = "DELETE FROM jobs WHERE job_id = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1,
            "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "job_repository"});
    }

    sqlite3_bind_text(stmt, 1, job_id.data(), static_cast<int>(job_id.size()),
                      SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to delete job: " + std::string(sqlite3_errmsg(db_)),
            "job_repository"});
    }

    return kcenon::common::ok();
}

Result<size_t> job_repository::cleanup_old_jobs(std::chrono::hours max_age) {
    if (!db_) {
        return kcenon::common::make_error<size_t>(
            -1, "Database not initialized", "job_repository");
    }

    auto cutoff = std::chrono::system_clock::now() - max_age;
    auto cutoff_str = to_timestamp_string(cutoff);

    static constexpr const char* sql = R"(
        DELETE FROM jobs
        WHERE status IN ('completed', 'failed', 'cancelled')
          AND completed_at < ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return kcenon::common::make_error<size_t>(
            -1,
            "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "job_repository");
    }

    sqlite3_bind_text(stmt, 1, cutoff_str.c_str(), -1, SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return kcenon::common::make_error<size_t>(
            -1, "Failed to cleanup jobs: " + std::string(sqlite3_errmsg(db_)),
            "job_repository");
    }

    return kcenon::common::ok(static_cast<size_t>(sqlite3_changes(db_)));
}

bool job_repository::exists(std::string_view job_id) const {
    if (!db_) return false;

    static constexpr const char* sql = "SELECT 1 FROM jobs WHERE job_id = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, job_id.data(), static_cast<int>(job_id.size()),
                      SQLITE_TRANSIENT);

    bool found = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return found;
}

// =============================================================================
// Status Updates
// =============================================================================

VoidResult job_repository::update_status(std::string_view job_id,
                                         client::job_status status,
                                         std::string_view error_message,
                                         std::string_view error_details) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "job_repository"});
    }

    const char* sql = nullptr;
    if (status == client::job_status::failed) {
        sql = R"(
            UPDATE jobs SET
                status = ?,
                error_message = ?,
                error_details = ?
            WHERE job_id = ?
        )";
    } else {
        sql = "UPDATE jobs SET status = ? WHERE job_id = ?";
    }

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1,
            "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "job_repository"});
    }

    int idx = 1;
    sqlite3_bind_text(stmt, idx++, client::to_string(status), -1, SQLITE_STATIC);

    if (status == client::job_status::failed) {
        sqlite3_bind_text(stmt, idx++, error_message.data(),
                          static_cast<int>(error_message.size()),
                          SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, idx++, error_details.data(),
                          static_cast<int>(error_details.size()),
                          SQLITE_TRANSIENT);
    }

    sqlite3_bind_text(stmt, idx++, job_id.data(),
                      static_cast<int>(job_id.size()), SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to update status: " + std::string(sqlite3_errmsg(db_)),
            "job_repository"});
    }

    return kcenon::common::ok();
}

VoidResult job_repository::update_progress(std::string_view job_id,
                                           const client::job_progress& progress) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "job_repository"});
    }

    static constexpr const char* sql = R"(
        UPDATE jobs SET
            total_items = ?,
            completed_items = ?,
            failed_items = ?,
            skipped_items = ?,
            bytes_transferred = ?,
            current_item = ?,
            current_item_description = ?
        WHERE job_id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1,
            "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "job_repository"});
    }

    int idx = 1;
    sqlite3_bind_int64(stmt, idx++,
                       static_cast<int64_t>(progress.total_items));
    sqlite3_bind_int64(stmt, idx++,
                       static_cast<int64_t>(progress.completed_items));
    sqlite3_bind_int64(stmt, idx++,
                       static_cast<int64_t>(progress.failed_items));
    sqlite3_bind_int64(stmt, idx++,
                       static_cast<int64_t>(progress.skipped_items));
    sqlite3_bind_int64(stmt, idx++,
                       static_cast<int64_t>(progress.bytes_transferred));
    sqlite3_bind_text(stmt, idx++, progress.current_item.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, progress.current_item_description.c_str(),
                      -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, job_id.data(),
                      static_cast<int>(job_id.size()), SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to update progress: " + std::string(sqlite3_errmsg(db_)),
            "job_repository"});
    }

    return kcenon::common::ok();
}

VoidResult job_repository::mark_started(std::string_view job_id) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "job_repository"});
    }

    static constexpr const char* sql = R"(
        UPDATE jobs SET
            status = 'running',
            started_at = CURRENT_TIMESTAMP
        WHERE job_id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1,
            "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "job_repository"});
    }

    sqlite3_bind_text(stmt, 1, job_id.data(), static_cast<int>(job_id.size()),
                      SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to mark started: " + std::string(sqlite3_errmsg(db_)),
            "job_repository"});
    }

    return kcenon::common::ok();
}

VoidResult job_repository::mark_completed(std::string_view job_id) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "job_repository"});
    }

    static constexpr const char* sql = R"(
        UPDATE jobs SET
            status = 'completed',
            completed_at = CURRENT_TIMESTAMP
        WHERE job_id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1,
            "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "job_repository"});
    }

    sqlite3_bind_text(stmt, 1, job_id.data(), static_cast<int>(job_id.size()),
                      SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to mark completed: " + std::string(sqlite3_errmsg(db_)),
            "job_repository"});
    }

    return kcenon::common::ok();
}

VoidResult job_repository::mark_failed(std::string_view job_id,
                                       std::string_view error_message,
                                       std::string_view error_details) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "job_repository"});
    }

    static constexpr const char* sql = R"(
        UPDATE jobs SET
            status = 'failed',
            error_message = ?,
            error_details = ?,
            retry_count = retry_count + 1,
            completed_at = CURRENT_TIMESTAMP
        WHERE job_id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1,
            "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "job_repository"});
    }

    sqlite3_bind_text(stmt, 1, error_message.data(),
                      static_cast<int>(error_message.size()), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, error_details.data(),
                      static_cast<int>(error_details.size()), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, job_id.data(), static_cast<int>(job_id.size()),
                      SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to mark failed: " + std::string(sqlite3_errmsg(db_)),
            "job_repository"});
    }

    return kcenon::common::ok();
}

VoidResult job_repository::increment_retry(std::string_view job_id) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "job_repository"});
    }

    static constexpr const char* sql = R"(
        UPDATE jobs SET retry_count = retry_count + 1 WHERE job_id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1,
            "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "job_repository"});
    }

    sqlite3_bind_text(stmt, 1, job_id.data(), static_cast<int>(job_id.size()),
                      SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to increment retry: " + std::string(sqlite3_errmsg(db_)),
            "job_repository"});
    }

    return kcenon::common::ok();
}

// =============================================================================
// Statistics
// =============================================================================

size_t job_repository::count() const {
    if (!db_) return 0;

    static constexpr const char* sql = "SELECT COUNT(*) FROM jobs";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }

    size_t result = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
    }

    sqlite3_finalize(stmt);
    return result;
}

size_t job_repository::count_by_status(client::job_status status) const {
    if (!db_) return 0;

    static constexpr const char* sql =
        "SELECT COUNT(*) FROM jobs WHERE status = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, client::to_string(status), -1, SQLITE_STATIC);

    size_t result = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
    }

    sqlite3_finalize(stmt);
    return result;
}

size_t job_repository::count_completed_today() const {
    if (!db_) return 0;

    static constexpr const char* sql = R"(
        SELECT COUNT(*) FROM jobs
        WHERE status = 'completed'
          AND date(completed_at) = date('now')
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }

    size_t result = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
    }

    sqlite3_finalize(stmt);
    return result;
}

size_t job_repository::count_failed_today() const {
    if (!db_) return 0;

    static constexpr const char* sql = R"(
        SELECT COUNT(*) FROM jobs
        WHERE status = 'failed'
          AND date(completed_at) = date('now')
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }

    size_t result = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
    }

    sqlite3_finalize(stmt);
    return result;
}

// =============================================================================
// Database Information
// =============================================================================

bool job_repository::is_valid() const noexcept { return db_ != nullptr; }

// =============================================================================
// Private Implementation
// =============================================================================

client::job_record job_repository::parse_row(void* stmt_ptr) const {
    auto* stmt = static_cast<sqlite3_stmt*>(stmt_ptr);
    client::job_record job;

    int col = 0;
    job.pk = get_int64_column(stmt, col++);
    job.job_id = get_text_column(stmt, col++);
    job.type = client::job_type_from_string(get_text_column(stmt, col++));
    job.status = client::job_status_from_string(get_text_column(stmt, col++));
    job.priority = client::job_priority_from_int(get_int_column(stmt, col++));

    job.source_node_id = get_text_column(stmt, col++);
    job.destination_node_id = get_text_column(stmt, col++);

    job.patient_id = get_optional_text(stmt, col++);
    job.study_uid = get_optional_text(stmt, col++);
    job.series_uid = get_optional_text(stmt, col++);
    job.sop_instance_uid = get_optional_text(stmt, col++);

    auto uids_json = get_text_column(stmt, col++);
    job.instance_uids = deserialize_instance_uids(uids_json);

    job.progress.total_items =
        static_cast<size_t>(get_int64_column(stmt, col++));
    job.progress.completed_items =
        static_cast<size_t>(get_int64_column(stmt, col++));
    job.progress.failed_items =
        static_cast<size_t>(get_int64_column(stmt, col++));
    job.progress.skipped_items =
        static_cast<size_t>(get_int64_column(stmt, col++));
    job.progress.bytes_transferred =
        static_cast<size_t>(get_int64_column(stmt, col++));
    job.progress.current_item = get_text_column(stmt, col++);
    job.progress.current_item_description = get_text_column(stmt, col++);
    job.progress.calculate_percent();

    job.error_message = get_text_column(stmt, col++);
    job.error_details = get_text_column(stmt, col++);
    job.retry_count = get_int_column(stmt, col++);
    job.max_retries = get_int_column(stmt, col++, 3);

    job.created_by = get_text_column(stmt, col++);

    auto metadata_json = get_text_column(stmt, col++);
    job.metadata = deserialize_metadata(metadata_json);

    auto created_str = get_text_column(stmt, col++);
    job.created_at = from_timestamp_string(created_str.c_str());

    auto queued_str = get_text_column(stmt, col++);
    job.queued_at = from_optional_timestamp(queued_str.c_str());

    auto started_str = get_text_column(stmt, col++);
    job.started_at = from_optional_timestamp(started_str.c_str());

    auto completed_str = get_text_column(stmt, col++);
    job.completed_at = from_optional_timestamp(completed_str.c_str());

    return job;
}

}  // namespace pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
