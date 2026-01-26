/**
 * @file sync_repository.cpp
 * @brief Implementation of the sync repository for sync persistence
 *
 * @see Issue #542 - Implement Sync Manager for Bidirectional Synchronization
 * @see Issue #530 - PACS Client System Support (Parent Epic)
 */

#include "pacs/storage/sync_repository.hpp"

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

}  // namespace

// =============================================================================
// JSON Serialization
// =============================================================================

std::string sync_repository::serialize_vector(const std::vector<std::string>& vec) {
    if (vec.empty()) return "[]";

    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"";
        for (char c : vec[i]) {
            if (c == '"') oss << "\\\"";
            else if (c == '\\') oss << "\\\\";
            else oss << c;
        }
        oss << "\"";
    }
    oss << "]";
    return oss.str();
}

std::vector<std::string> sync_repository::deserialize_vector(std::string_view json) {
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

// =============================================================================
// Construction / Destruction
// =============================================================================

sync_repository::sync_repository(sqlite3* db) : db_(db) {}

sync_repository::~sync_repository() = default;

sync_repository::sync_repository(sync_repository&&) noexcept = default;
auto sync_repository::operator=(sync_repository&&) noexcept -> sync_repository& = default;

// =============================================================================
// Config Operations
// =============================================================================

VoidResult sync_repository::save_config(const client::sync_config& config) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "sync_repository"});
    }

    const char* sql = R"(
        INSERT INTO sync_configs (
            config_id, source_node_id, name, enabled,
            lookback_hours, modalities_json, patient_patterns_json,
            sync_direction, delete_missing, overwrite_existing, sync_metadata_only,
            schedule_cron, last_sync, last_successful_sync,
            total_syncs, studies_synced
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(config_id) DO UPDATE SET
            source_node_id = excluded.source_node_id,
            name = excluded.name,
            enabled = excluded.enabled,
            lookback_hours = excluded.lookback_hours,
            modalities_json = excluded.modalities_json,
            patient_patterns_json = excluded.patient_patterns_json,
            sync_direction = excluded.sync_direction,
            delete_missing = excluded.delete_missing,
            overwrite_existing = excluded.overwrite_existing,
            sync_metadata_only = excluded.sync_metadata_only,
            schedule_cron = excluded.schedule_cron,
            last_sync = excluded.last_sync,
            last_successful_sync = excluded.last_successful_sync,
            total_syncs = excluded.total_syncs,
            studies_synced = excluded.studies_synced,
            updated_at = datetime('now')
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "sync_repository"});
    }

    sqlite3_bind_text(stmt, 1, config.config_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, config.source_node_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, config.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, config.enabled ? 1 : 0);
    sqlite3_bind_int(stmt, 5, static_cast<int>(config.lookback.count()));
    sqlite3_bind_text(stmt, 6, serialize_vector(config.modalities).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, serialize_vector(config.patient_id_patterns).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, to_string(config.direction), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 9, config.delete_missing ? 1 : 0);
    sqlite3_bind_int(stmt, 10, config.overwrite_existing ? 1 : 0);
    sqlite3_bind_int(stmt, 11, config.sync_metadata_only ? 1 : 0);
    sqlite3_bind_text(stmt, 12, config.schedule_cron.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 13, to_timestamp_string(config.last_sync).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 14, to_timestamp_string(config.last_successful_sync).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 15, static_cast<int64_t>(config.total_syncs));
    sqlite3_bind_int64(stmt, 16, static_cast<int64_t>(config.studies_synced));

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to save config: " + std::string(sqlite3_errmsg(db_)),
            "sync_repository"});
    }

    return kcenon::common::ok();
}

std::optional<client::sync_config> sync_repository::find_config(
    std::string_view config_id) const {
    if (!db_) return std::nullopt;

    const char* sql = R"(
        SELECT pk, config_id, source_node_id, name, enabled,
               lookback_hours, modalities_json, patient_patterns_json,
               sync_direction, delete_missing, overwrite_existing, sync_metadata_only,
               schedule_cron, last_sync, last_successful_sync,
               total_syncs, studies_synced
        FROM sync_configs WHERE config_id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, config_id.data(), static_cast<int>(config_id.size()), SQLITE_TRANSIENT);

    std::optional<client::sync_config> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = parse_config_row(stmt);
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<client::sync_config> sync_repository::list_configs() const {
    std::vector<client::sync_config> result;
    if (!db_) return result;

    const char* sql = R"(
        SELECT pk, config_id, source_node_id, name, enabled,
               lookback_hours, modalities_json, patient_patterns_json,
               sync_direction, delete_missing, overwrite_existing, sync_metadata_only,
               schedule_cron, last_sync, last_successful_sync,
               total_syncs, studies_synced
        FROM sync_configs ORDER BY name
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return result;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        result.push_back(parse_config_row(stmt));
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<client::sync_config> sync_repository::list_enabled_configs() const {
    std::vector<client::sync_config> result;
    if (!db_) return result;

    const char* sql = R"(
        SELECT pk, config_id, source_node_id, name, enabled,
               lookback_hours, modalities_json, patient_patterns_json,
               sync_direction, delete_missing, overwrite_existing, sync_metadata_only,
               schedule_cron, last_sync, last_successful_sync,
               total_syncs, studies_synced
        FROM sync_configs WHERE enabled = 1 ORDER BY name
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return result;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        result.push_back(parse_config_row(stmt));
    }

    sqlite3_finalize(stmt);
    return result;
}

VoidResult sync_repository::remove_config(std::string_view config_id) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "sync_repository"});
    }

    const char* sql = "DELETE FROM sync_configs WHERE config_id = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "sync_repository"});
    }

    sqlite3_bind_text(stmt, 1, config_id.data(), static_cast<int>(config_id.size()), SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to delete config: " + std::string(sqlite3_errmsg(db_)),
            "sync_repository"});
    }

    return kcenon::common::ok();
}

VoidResult sync_repository::update_config_stats(
    std::string_view config_id,
    bool success,
    size_t studies_synced) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "sync_repository"});
    }

    std::string sql;
    if (success) {
        sql = R"(
            UPDATE sync_configs SET
                total_syncs = total_syncs + 1,
                studies_synced = studies_synced + ?,
                last_sync = datetime('now'),
                last_successful_sync = datetime('now'),
                updated_at = datetime('now')
            WHERE config_id = ?
        )";
    } else {
        sql = R"(
            UPDATE sync_configs SET
                total_syncs = total_syncs + 1,
                last_sync = datetime('now'),
                updated_at = datetime('now')
            WHERE config_id = ?
        )";
    }

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "sync_repository"});
    }

    if (success) {
        sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(studies_synced));
        sqlite3_bind_text(stmt, 2, config_id.data(), static_cast<int>(config_id.size()), SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_text(stmt, 1, config_id.data(), static_cast<int>(config_id.size()), SQLITE_TRANSIENT);
    }

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to update config stats: " + std::string(sqlite3_errmsg(db_)),
            "sync_repository"});
    }

    return kcenon::common::ok();
}

// =============================================================================
// Conflict Operations
// =============================================================================

VoidResult sync_repository::save_conflict(const client::sync_conflict& conflict) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "sync_repository"});
    }

    const char* sql = R"(
        INSERT INTO sync_conflicts (
            config_id, study_uid, patient_id, conflict_type,
            local_modified, remote_modified,
            local_instance_count, remote_instance_count,
            resolved, resolution, detected_at, resolved_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(config_id, study_uid) DO UPDATE SET
            patient_id = excluded.patient_id,
            conflict_type = excluded.conflict_type,
            local_modified = excluded.local_modified,
            remote_modified = excluded.remote_modified,
            local_instance_count = excluded.local_instance_count,
            remote_instance_count = excluded.remote_instance_count,
            resolved = excluded.resolved,
            resolution = excluded.resolution,
            detected_at = excluded.detected_at,
            resolved_at = excluded.resolved_at
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "sync_repository"});
    }

    sqlite3_bind_text(stmt, 1, conflict.config_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, conflict.study_uid.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, conflict.patient_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, to_string(conflict.conflict_type), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, to_timestamp_string(conflict.local_modified).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, to_timestamp_string(conflict.remote_modified).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 7, static_cast<int64_t>(conflict.local_instance_count));
    sqlite3_bind_int64(stmt, 8, static_cast<int64_t>(conflict.remote_instance_count));
    sqlite3_bind_int(stmt, 9, conflict.resolved ? 1 : 0);
    sqlite3_bind_text(stmt, 10, conflict.resolved ? to_string(conflict.resolution_used) : "", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 11, to_timestamp_string(conflict.detected_at).c_str(), -1, SQLITE_TRANSIENT);
    if (conflict.resolved_at.has_value()) {
        sqlite3_bind_text(stmt, 12, to_timestamp_string(conflict.resolved_at.value()).c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, 12);
    }

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to save conflict: " + std::string(sqlite3_errmsg(db_)),
            "sync_repository"});
    }

    return kcenon::common::ok();
}

std::optional<client::sync_conflict> sync_repository::find_conflict(
    std::string_view study_uid) const {
    if (!db_) return std::nullopt;

    const char* sql = R"(
        SELECT pk, config_id, study_uid, patient_id, conflict_type,
               local_modified, remote_modified,
               local_instance_count, remote_instance_count,
               resolved, resolution, detected_at, resolved_at
        FROM sync_conflicts WHERE study_uid = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, study_uid.data(), static_cast<int>(study_uid.size()), SQLITE_TRANSIENT);

    std::optional<client::sync_conflict> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = parse_conflict_row(stmt);
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<client::sync_conflict> sync_repository::list_conflicts(
    std::string_view config_id) const {
    std::vector<client::sync_conflict> result;
    if (!db_) return result;

    const char* sql = R"(
        SELECT pk, config_id, study_uid, patient_id, conflict_type,
               local_modified, remote_modified,
               local_instance_count, remote_instance_count,
               resolved, resolution, detected_at, resolved_at
        FROM sync_conflicts WHERE config_id = ? ORDER BY detected_at DESC
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return result;
    }

    sqlite3_bind_text(stmt, 1, config_id.data(), static_cast<int>(config_id.size()), SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        result.push_back(parse_conflict_row(stmt));
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<client::sync_conflict> sync_repository::list_unresolved_conflicts() const {
    std::vector<client::sync_conflict> result;
    if (!db_) return result;

    const char* sql = R"(
        SELECT pk, config_id, study_uid, patient_id, conflict_type,
               local_modified, remote_modified,
               local_instance_count, remote_instance_count,
               resolved, resolution, detected_at, resolved_at
        FROM sync_conflicts WHERE resolved = 0 ORDER BY detected_at DESC
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return result;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        result.push_back(parse_conflict_row(stmt));
    }

    sqlite3_finalize(stmt);
    return result;
}

VoidResult sync_repository::resolve_conflict(
    std::string_view study_uid,
    client::conflict_resolution resolution) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "sync_repository"});
    }

    const char* sql = R"(
        UPDATE sync_conflicts SET
            resolved = 1,
            resolution = ?,
            resolved_at = datetime('now')
        WHERE study_uid = ? AND resolved = 0
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "sync_repository"});
    }

    sqlite3_bind_text(stmt, 1, to_string(resolution), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, study_uid.data(), static_cast<int>(study_uid.size()), SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to resolve conflict: " + std::string(sqlite3_errmsg(db_)),
            "sync_repository"});
    }

    return kcenon::common::ok();
}

Result<size_t> sync_repository::cleanup_old_conflicts(std::chrono::hours max_age) {
    if (!db_) {
        return kcenon::common::make_error<size_t>(-1,
            "Database not initialized", "sync_repository");
    }

    auto cutoff = std::chrono::system_clock::now() - max_age;
    auto cutoff_str = to_timestamp_string(cutoff);

    const char* sql = R"(
        DELETE FROM sync_conflicts WHERE resolved = 1 AND resolved_at < ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return kcenon::common::make_error<size_t>(-1,
            "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "sync_repository");
    }

    sqlite3_bind_text(stmt, 1, cutoff_str.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return kcenon::common::make_error<size_t>(-1,
            "Failed to cleanup conflicts: " + std::string(sqlite3_errmsg(db_)),
            "sync_repository");
    }

    return static_cast<size_t>(sqlite3_changes(db_));
}

// =============================================================================
// History Operations
// =============================================================================

VoidResult sync_repository::save_history(const client::sync_history& history) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "sync_repository"});
    }

    const char* sql = R"(
        INSERT INTO sync_history (
            config_id, job_id, success,
            studies_checked, studies_synced, conflicts_found,
            errors_json, started_at, completed_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "sync_repository"});
    }

    sqlite3_bind_text(stmt, 1, history.config_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, history.job_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, history.success ? 1 : 0);
    sqlite3_bind_int64(stmt, 4, static_cast<int64_t>(history.studies_checked));
    sqlite3_bind_int64(stmt, 5, static_cast<int64_t>(history.studies_synced));
    sqlite3_bind_int64(stmt, 6, static_cast<int64_t>(history.conflicts_found));
    sqlite3_bind_text(stmt, 7, serialize_vector(history.errors).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, to_timestamp_string(history.started_at).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 9, to_timestamp_string(history.completed_at).c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to save history: " + std::string(sqlite3_errmsg(db_)),
            "sync_repository"});
    }

    return kcenon::common::ok();
}

std::vector<client::sync_history> sync_repository::list_history(
    std::string_view config_id, size_t limit) const {
    std::vector<client::sync_history> result;
    if (!db_) return result;

    const char* sql = R"(
        SELECT pk, config_id, job_id, success,
               studies_checked, studies_synced, conflicts_found,
               errors_json, started_at, completed_at
        FROM sync_history WHERE config_id = ? ORDER BY started_at DESC LIMIT ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return result;
    }

    sqlite3_bind_text(stmt, 1, config_id.data(), static_cast<int>(config_id.size()), SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(limit));

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        result.push_back(parse_history_row(stmt));
    }

    sqlite3_finalize(stmt);
    return result;
}

std::optional<client::sync_history> sync_repository::get_last_history(
    std::string_view config_id) const {
    if (!db_) return std::nullopt;

    const char* sql = R"(
        SELECT pk, config_id, job_id, success,
               studies_checked, studies_synced, conflicts_found,
               errors_json, started_at, completed_at
        FROM sync_history WHERE config_id = ? ORDER BY started_at DESC LIMIT 1
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, config_id.data(), static_cast<int>(config_id.size()), SQLITE_TRANSIENT);

    std::optional<client::sync_history> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = parse_history_row(stmt);
    }

    sqlite3_finalize(stmt);
    return result;
}

Result<size_t> sync_repository::cleanup_old_history(std::chrono::hours max_age) {
    if (!db_) {
        return kcenon::common::make_error<size_t>(-1,
            "Database not initialized", "sync_repository");
    }

    auto cutoff = std::chrono::system_clock::now() - max_age;
    auto cutoff_str = to_timestamp_string(cutoff);

    const char* sql = "DELETE FROM sync_history WHERE completed_at < ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return kcenon::common::make_error<size_t>(-1,
            "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "sync_repository");
    }

    sqlite3_bind_text(stmt, 1, cutoff_str.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return kcenon::common::make_error<size_t>(-1,
            "Failed to cleanup history: " + std::string(sqlite3_errmsg(db_)),
            "sync_repository");
    }

    return static_cast<size_t>(sqlite3_changes(db_));
}

// =============================================================================
// Statistics
// =============================================================================

size_t sync_repository::count_configs() const {
    if (!db_) return 0;

    const char* sql = "SELECT COUNT(*) FROM sync_configs";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }

    size_t count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
    }

    sqlite3_finalize(stmt);
    return count;
}

size_t sync_repository::count_unresolved_conflicts() const {
    if (!db_) return 0;

    const char* sql = "SELECT COUNT(*) FROM sync_conflicts WHERE resolved = 0";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }

    size_t count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
    }

    sqlite3_finalize(stmt);
    return count;
}

size_t sync_repository::count_syncs_today() const {
    if (!db_) return 0;

    const char* sql = R"(
        SELECT COUNT(*) FROM sync_history
        WHERE date(completed_at) = date('now')
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }

    size_t count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
    }

    sqlite3_finalize(stmt);
    return count;
}

// =============================================================================
// Database Information
// =============================================================================

bool sync_repository::is_valid() const noexcept {
    return db_ != nullptr;
}

// =============================================================================
// Private Implementation
// =============================================================================

client::sync_config sync_repository::parse_config_row(void* stmt_ptr) const {
    auto* stmt = static_cast<sqlite3_stmt*>(stmt_ptr);
    client::sync_config config;

    config.pk = get_int64_column(stmt, 0);
    config.config_id = get_text_column(stmt, 1);
    config.source_node_id = get_text_column(stmt, 2);
    config.name = get_text_column(stmt, 3);
    config.enabled = get_int_column(stmt, 4) != 0;
    config.lookback = std::chrono::hours(get_int_column(stmt, 5, 24));
    config.modalities = deserialize_vector(get_text_column(stmt, 6));
    config.patient_id_patterns = deserialize_vector(get_text_column(stmt, 7));
    config.direction = client::sync_direction_from_string(get_text_column(stmt, 8));
    config.delete_missing = get_int_column(stmt, 9) != 0;
    config.overwrite_existing = get_int_column(stmt, 10) != 0;
    config.sync_metadata_only = get_int_column(stmt, 11) != 0;
    config.schedule_cron = get_text_column(stmt, 12);
    config.last_sync = from_timestamp_string(
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 13)));
    config.last_successful_sync = from_timestamp_string(
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 14)));
    config.total_syncs = static_cast<size_t>(get_int64_column(stmt, 15));
    config.studies_synced = static_cast<size_t>(get_int64_column(stmt, 16));

    return config;
}

client::sync_conflict sync_repository::parse_conflict_row(void* stmt_ptr) const {
    auto* stmt = static_cast<sqlite3_stmt*>(stmt_ptr);
    client::sync_conflict conflict;

    conflict.pk = get_int64_column(stmt, 0);
    conflict.config_id = get_text_column(stmt, 1);
    conflict.study_uid = get_text_column(stmt, 2);
    conflict.patient_id = get_text_column(stmt, 3);
    conflict.conflict_type = client::sync_conflict_type_from_string(get_text_column(stmt, 4));
    conflict.local_modified = from_timestamp_string(
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5)));
    conflict.remote_modified = from_timestamp_string(
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6)));
    conflict.local_instance_count = static_cast<size_t>(get_int64_column(stmt, 7));
    conflict.remote_instance_count = static_cast<size_t>(get_int64_column(stmt, 8));
    conflict.resolved = get_int_column(stmt, 9) != 0;
    conflict.resolution_used = client::conflict_resolution_from_string(get_text_column(stmt, 10));
    conflict.detected_at = from_timestamp_string(
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11)));

    auto resolved_at_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 12));
    if (resolved_at_str && resolved_at_str[0] != '\0') {
        conflict.resolved_at = from_timestamp_string(resolved_at_str);
    }

    return conflict;
}

client::sync_history sync_repository::parse_history_row(void* stmt_ptr) const {
    auto* stmt = static_cast<sqlite3_stmt*>(stmt_ptr);
    client::sync_history history;

    history.pk = get_int64_column(stmt, 0);
    history.config_id = get_text_column(stmt, 1);
    history.job_id = get_text_column(stmt, 2);
    history.success = get_int_column(stmt, 3) != 0;
    history.studies_checked = static_cast<size_t>(get_int64_column(stmt, 4));
    history.studies_synced = static_cast<size_t>(get_int64_column(stmt, 5));
    history.conflicts_found = static_cast<size_t>(get_int64_column(stmt, 6));
    history.errors = deserialize_vector(get_text_column(stmt, 7));
    history.started_at = from_timestamp_string(
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8)));
    history.completed_at = from_timestamp_string(
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9)));

    return history;
}

}  // namespace pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
