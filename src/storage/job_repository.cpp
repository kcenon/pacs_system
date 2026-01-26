/**
 * @file job_repository.cpp
 * @brief Implementation of the job repository for job persistence
 *
 * @see Issue #537 - Implement Job Manager for Async DICOM Operations
 * @see Issue #552 - Part 1: Job Types and Repository Implementation
 */

#include "pacs/storage/job_repository.hpp"

#ifdef PACS_WITH_DATABASE_SYSTEM


#include <sqlite3.h>

#include <chrono>
#include <cstring>
#include <sstream>

namespace pacs::storage {

// =============================================================================
// Helper Functions
// =============================================================================

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
    if (std::sscanf(str, "%d-%d-%d %d:%d:%d",
                    &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                    &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 6) {
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

/// Convert optional time_point to timestamp string
[[nodiscard]] [[maybe_unused]] std::string to_optional_timestamp(
    const std::optional<std::chrono::system_clock::time_point>& tp) {
    if (!tp.has_value()) {
        return "";
    }
    return to_timestamp_string(tp.value());
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
[[nodiscard]] int get_int_column(sqlite3_stmt* stmt, int col, int default_val = 0) {
    if (sqlite3_column_type(stmt, col) == SQLITE_NULL) {
        return default_val;
    }
    return sqlite3_column_int(stmt, col);
}

/// Get int64 column with default
[[nodiscard]] int64_t get_int64_column(sqlite3_stmt* stmt, int col, int64_t default_val = 0) {
    if (sqlite3_column_type(stmt, col) == SQLITE_NULL) {
        return default_val;
    }
    return sqlite3_column_int64(stmt, col);
}

/// Get optional string column
[[nodiscard]] std::optional<std::string> get_optional_text(sqlite3_stmt* stmt, int col) {
    if (sqlite3_column_type(stmt, col) == SQLITE_NULL) {
        return std::nullopt;
    }
    auto text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, col));
    return text ? std::optional<std::string>{text} : std::nullopt;
}

/// Bind optional string
void bind_optional_text(sqlite3_stmt* stmt, int idx, const std::optional<std::string>& value) {
    if (value.has_value()) {
        sqlite3_bind_text(stmt, idx, value->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, idx);
    }
}

/// Bind optional timestamp
void bind_optional_timestamp(
    sqlite3_stmt* stmt,
    int idx,
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
        // Escape quotes in UID (though UIDs shouldn't have quotes)
        for (char c : uids[i]) {
            if (c == '"') oss << "\\\"";
            else if (c == '\\') oss << "\\\\";
            else oss << c;
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

    // Simple JSON array parser
    size_t pos = 0;
    while (pos < json.size()) {
        // Find opening quote
        auto start = json.find('"', pos);
        if (start == std::string_view::npos) break;

        // Find closing quote (handle escapes)
        size_t end = start + 1;
        while (end < json.size()) {
            if (json[end] == '\\' && end + 1 < json.size()) {
                end += 2;  // Skip escaped char
            } else if (json[end] == '"') {
                break;
            } else {
                ++end;
            }
        }

        if (end < json.size()) {
            std::string value{json.substr(start + 1, end - start - 1)};
            // Unescape
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

        // Escape key
        oss << "\"";
        for (char c : key) {
            if (c == '"') oss << "\\\"";
            else if (c == '\\') oss << "\\\\";
            else oss << c;
        }
        oss << "\":\"";

        // Escape value
        for (char c : value) {
            if (c == '"') oss << "\\\"";
            else if (c == '\\') oss << "\\\\";
            else oss << c;
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

    // Simple JSON object parser for string:string pairs
    size_t pos = 0;
    while (pos < json.size()) {
        // Find key opening quote
        auto key_start = json.find('"', pos);
        if (key_start == std::string_view::npos) break;

        // Find key closing quote
        size_t key_end = key_start + 1;
        while (key_end < json.size() && json[key_end] != '"') {
            if (json[key_end] == '\\') ++key_end;
            ++key_end;
        }
        if (key_end >= json.size()) break;

        std::string key{json.substr(key_start + 1, key_end - key_start - 1)};

        // Find value opening quote
        auto val_start = json.find('"', key_end + 1);
        if (val_start == std::string_view::npos) break;

        // Find value closing quote
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

auto job_repository::operator=(job_repository&&) noexcept -> job_repository& = default;

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
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "job_repository"});
    }

    int idx = 1;
    sqlite3_bind_text(stmt, idx++, job.job_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, client::to_string(job.type), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, idx++, client::to_string(job.status), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, idx++, static_cast<int>(job.priority));

    sqlite3_bind_text(stmt, idx++, job.source_node_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, job.destination_node_id.c_str(), -1, SQLITE_TRANSIENT);

    bind_optional_text(stmt, idx++, job.patient_id);
    bind_optional_text(stmt, idx++, job.study_uid);
    bind_optional_text(stmt, idx++, job.series_uid);
    bind_optional_text(stmt, idx++, job.sop_instance_uid);

    auto uids_json = serialize_instance_uids(job.instance_uids);
    sqlite3_bind_text(stmt, idx++, uids_json.c_str(), -1, SQLITE_TRANSIENT);

    sqlite3_bind_int64(stmt, idx++, static_cast<int64_t>(job.progress.total_items));
    sqlite3_bind_int64(stmt, idx++, static_cast<int64_t>(job.progress.completed_items));
    sqlite3_bind_int64(stmt, idx++, static_cast<int64_t>(job.progress.failed_items));
    sqlite3_bind_int64(stmt, idx++, static_cast<int64_t>(job.progress.skipped_items));
    sqlite3_bind_int64(stmt, idx++, static_cast<int64_t>(job.progress.bytes_transferred));

    sqlite3_bind_text(stmt, idx++, job.progress.current_item.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, job.progress.current_item_description.c_str(), -1, SQLITE_TRANSIENT);

    sqlite3_bind_text(stmt, idx++, job.error_message.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, job.error_details.c_str(), -1, SQLITE_TRANSIENT);
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

std::optional<client::job_record> job_repository::find_by_id(std::string_view job_id) const {
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

    sqlite3_bind_text(stmt, 1, job_id.data(), static_cast<int>(job_id.size()), SQLITE_TRANSIENT);

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
        bindings.emplace_back(param_idx++, std::string(client::to_string(options.status.value())));
    }

    if (options.type.has_value()) {
        sql << " AND type = ?";
        bindings.emplace_back(param_idx++, std::string(client::to_string(options.type.value())));
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
    if (sqlite3_prepare_v2(db_, sql_str.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
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

std::vector<client::job_record> job_repository::find_pending_jobs(size_t limit) const {
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

std::vector<client::job_record> job_repository::find_by_node(std::string_view node_id) const {
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
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "job_repository"});
    }

    sqlite3_bind_text(stmt, 1, job_id.data(), static_cast<int>(job_id.size()), SQLITE_TRANSIENT);

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
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
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

    sqlite3_bind_text(stmt, 1, job_id.data(), static_cast<int>(job_id.size()), SQLITE_TRANSIENT);

    bool found = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return found;
}

// =============================================================================
// Status Updates
// =============================================================================

VoidResult job_repository::update_status(
    std::string_view job_id,
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
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "job_repository"});
    }

    int idx = 1;
    sqlite3_bind_text(stmt, idx++, client::to_string(status), -1, SQLITE_STATIC);

    if (status == client::job_status::failed) {
        sqlite3_bind_text(stmt, idx++, error_message.data(),
                          static_cast<int>(error_message.size()), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, idx++, error_details.data(),
                          static_cast<int>(error_details.size()), SQLITE_TRANSIENT);
    }

    sqlite3_bind_text(stmt, idx++, job_id.data(), static_cast<int>(job_id.size()), SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to update status: " + std::string(sqlite3_errmsg(db_)),
            "job_repository"});
    }

    return kcenon::common::ok();
}

VoidResult job_repository::update_progress(
    std::string_view job_id,
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
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "job_repository"});
    }

    int idx = 1;
    sqlite3_bind_int64(stmt, idx++, static_cast<int64_t>(progress.total_items));
    sqlite3_bind_int64(stmt, idx++, static_cast<int64_t>(progress.completed_items));
    sqlite3_bind_int64(stmt, idx++, static_cast<int64_t>(progress.failed_items));
    sqlite3_bind_int64(stmt, idx++, static_cast<int64_t>(progress.skipped_items));
    sqlite3_bind_int64(stmt, idx++, static_cast<int64_t>(progress.bytes_transferred));
    sqlite3_bind_text(stmt, idx++, progress.current_item.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, progress.current_item_description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, job_id.data(), static_cast<int>(job_id.size()), SQLITE_TRANSIENT);

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
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "job_repository"});
    }

    sqlite3_bind_text(stmt, 1, job_id.data(), static_cast<int>(job_id.size()), SQLITE_TRANSIENT);

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
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "job_repository"});
    }

    sqlite3_bind_text(stmt, 1, job_id.data(), static_cast<int>(job_id.size()), SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to mark completed: " + std::string(sqlite3_errmsg(db_)),
            "job_repository"});
    }

    return kcenon::common::ok();
}

VoidResult job_repository::mark_failed(
    std::string_view job_id,
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
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "job_repository"});
    }

    sqlite3_bind_text(stmt, 1, error_message.data(),
                      static_cast<int>(error_message.size()), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, error_details.data(),
                      static_cast<int>(error_details.size()), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, job_id.data(), static_cast<int>(job_id.size()), SQLITE_TRANSIENT);

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
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "job_repository"});
    }

    sqlite3_bind_text(stmt, 1, job_id.data(), static_cast<int>(job_id.size()), SQLITE_TRANSIENT);

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

    static constexpr const char* sql = "SELECT COUNT(*) FROM jobs WHERE status = ?";

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

bool job_repository::is_valid() const noexcept {
    return db_ != nullptr;
}

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

    job.progress.total_items = static_cast<size_t>(get_int64_column(stmt, col++));
    job.progress.completed_items = static_cast<size_t>(get_int64_column(stmt, col++));
    job.progress.failed_items = static_cast<size_t>(get_int64_column(stmt, col++));
    job.progress.skipped_items = static_cast<size_t>(get_int64_column(stmt, col++));
    job.progress.bytes_transferred = static_cast<size_t>(get_int64_column(stmt, col++));
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
