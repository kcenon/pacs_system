/**
 * @file viewer_state_repository.cpp
 * @brief Implementation of the viewer state repository
 *
 * @see Issue #545 - Implement Annotation & Measurement APIs
 * @see Issue #581 - Part 1: Data Models and Repositories
 */

#include "pacs/storage/viewer_state_repository.hpp"

#include <sqlite3.h>

#include <chrono>
#include <cstdio>
#include <sstream>

namespace pacs::storage {

namespace {

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
    // Calculate milliseconds
    auto since_epoch = tp.time_since_epoch();
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(since_epoch);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(since_epoch - secs);

    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    // Append milliseconds
    char result[40];
    std::snprintf(result, sizeof(result), "%s.%03d", buf, static_cast<int>(ms.count()));
    return result;
}

[[nodiscard]] std::chrono::system_clock::time_point from_timestamp_string(
    const char* str) {
    if (!str || str[0] == '\0') {
        return {};
    }
    std::tm tm{};
    int ms = 0;
    // Try parsing with milliseconds first, then without
    if (std::sscanf(str, "%d-%d-%d %d:%d:%d.%d",
                    &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                    &tm.tm_hour, &tm.tm_min, &tm.tm_sec, &ms) < 6) {
        return {};
    }
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;
#ifdef _WIN32
    auto time = _mkgmtime(&tm);
#else
    auto time = timegm(&tm);
#endif
    auto tp = std::chrono::system_clock::from_time_t(time);
    return tp + std::chrono::milliseconds(ms);
}

[[nodiscard]] std::string get_text_column(sqlite3_stmt* stmt, int col) {
    auto text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, col));
    return text ? text : "";
}

[[nodiscard]] int64_t get_int64_column(sqlite3_stmt* stmt, int col, int64_t default_val = 0) {
    if (sqlite3_column_type(stmt, col) == SQLITE_NULL) {
        return default_val;
    }
    return sqlite3_column_int64(stmt, col);
}

}  // namespace

viewer_state_repository::viewer_state_repository(sqlite3* db) : db_(db) {}

viewer_state_repository::~viewer_state_repository() = default;

viewer_state_repository::viewer_state_repository(viewer_state_repository&&) noexcept = default;

auto viewer_state_repository::operator=(viewer_state_repository&&) noexcept
    -> viewer_state_repository& = default;

// =============================================================================
// Viewer State Operations
// =============================================================================

VoidResult viewer_state_repository::save_state(const viewer_state_record& record) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "viewer_state_repository"});
    }

    static constexpr const char* sql = R"(
        INSERT INTO viewer_states (
            state_id, study_uid, user_id, state_json, created_at, updated_at
        ) VALUES (?, ?, ?, ?, ?, ?)
        ON CONFLICT(state_id) DO UPDATE SET
            state_json = excluded.state_json,
            updated_at = excluded.updated_at
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "viewer_state_repository"});
    }

    auto now_str = to_timestamp_string(std::chrono::system_clock::now());

    int idx = 1;
    sqlite3_bind_text(stmt, idx++, record.state_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, record.study_uid.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, record.user_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, record.state_json.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, now_str.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, now_str.c_str(), -1, SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to save viewer state: " + std::string(sqlite3_errmsg(db_)),
            "viewer_state_repository"});
    }

    return kcenon::common::ok();
}

std::optional<viewer_state_record> viewer_state_repository::find_state_by_id(
    std::string_view state_id) const {
    if (!db_) return std::nullopt;

    static constexpr const char* sql = R"(
        SELECT pk, state_id, study_uid, user_id, state_json, created_at, updated_at
        FROM viewer_states WHERE state_id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, state_id.data(),
                      static_cast<int>(state_id.size()), SQLITE_TRANSIENT);

    std::optional<viewer_state_record> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = parse_state_row(stmt);
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<viewer_state_record> viewer_state_repository::find_states_by_study(
    std::string_view study_uid) const {
    viewer_state_query query;
    query.study_uid = std::string(study_uid);
    return search_states(query);
}

std::vector<viewer_state_record> viewer_state_repository::search_states(
    const viewer_state_query& query) const {
    std::vector<viewer_state_record> result;
    if (!db_) return result;

    std::ostringstream sql;
    sql << R"(
        SELECT pk, state_id, study_uid, user_id, state_json, created_at, updated_at
        FROM viewer_states WHERE 1=1
    )";

    std::vector<std::pair<int, std::string>> bindings;
    int param_idx = 1;

    if (query.study_uid.has_value()) {
        sql << " AND study_uid = ?";
        bindings.emplace_back(param_idx++, query.study_uid.value());
    }

    if (query.user_id.has_value()) {
        sql << " AND user_id = ?";
        bindings.emplace_back(param_idx++, query.user_id.value());
    }

    sql << " ORDER BY updated_at DESC";

    if (query.limit > 0) {
        sql << " LIMIT " << query.limit << " OFFSET " << query.offset;
    }

    sqlite3_stmt* stmt = nullptr;
    auto sql_str = sql.str();
    if (sqlite3_prepare_v2(db_, sql_str.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return result;
    }

    for (const auto& [idx, value] : bindings) {
        sqlite3_bind_text(stmt, idx, value.c_str(), -1, SQLITE_TRANSIENT);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        result.push_back(parse_state_row(stmt));
    }

    sqlite3_finalize(stmt);
    return result;
}

VoidResult viewer_state_repository::remove_state(std::string_view state_id) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "viewer_state_repository"});
    }

    static constexpr const char* sql = "DELETE FROM viewer_states WHERE state_id = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "viewer_state_repository"});
    }

    sqlite3_bind_text(stmt, 1, state_id.data(),
                      static_cast<int>(state_id.size()), SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to delete viewer state: " + std::string(sqlite3_errmsg(db_)),
            "viewer_state_repository"});
    }

    return kcenon::common::ok();
}

size_t viewer_state_repository::count_states() const {
    if (!db_) return 0;

    static constexpr const char* sql = "SELECT COUNT(*) FROM viewer_states";

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
// Recent Studies Operations
// =============================================================================

VoidResult viewer_state_repository::record_study_access(
    std::string_view user_id,
    std::string_view study_uid) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "viewer_state_repository"});
    }

    static constexpr const char* sql = R"(
        INSERT INTO recent_studies (user_id, study_uid, accessed_at)
        VALUES (?, ?, ?)
        ON CONFLICT(user_id, study_uid) DO UPDATE SET
            accessed_at = excluded.accessed_at
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "viewer_state_repository"});
    }

    auto now_str = to_timestamp_string(std::chrono::system_clock::now());

    sqlite3_bind_text(stmt, 1, user_id.data(),
                      static_cast<int>(user_id.size()), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, study_uid.data(),
                      static_cast<int>(study_uid.size()), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, now_str.c_str(), -1, SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to record study access: " + std::string(sqlite3_errmsg(db_)),
            "viewer_state_repository"});
    }

    return kcenon::common::ok();
}

std::vector<recent_study_record> viewer_state_repository::get_recent_studies(
    std::string_view user_id,
    size_t limit) const {
    std::vector<recent_study_record> result;
    if (!db_) return result;

    static constexpr const char* sql = R"(
        SELECT pk, user_id, study_uid, accessed_at
        FROM recent_studies
        WHERE user_id = ?
        ORDER BY accessed_at DESC, pk DESC
        LIMIT ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return result;
    }

    sqlite3_bind_text(stmt, 1, user_id.data(),
                      static_cast<int>(user_id.size()), SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(limit));

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        result.push_back(parse_recent_study_row(stmt));
    }

    sqlite3_finalize(stmt);
    return result;
}

VoidResult viewer_state_repository::clear_recent_studies(std::string_view user_id) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "viewer_state_repository"});
    }

    static constexpr const char* sql = "DELETE FROM recent_studies WHERE user_id = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "viewer_state_repository"});
    }

    sqlite3_bind_text(stmt, 1, user_id.data(),
                      static_cast<int>(user_id.size()), SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to clear recent studies: " + std::string(sqlite3_errmsg(db_)),
            "viewer_state_repository"});
    }

    return kcenon::common::ok();
}

size_t viewer_state_repository::count_recent_studies(std::string_view user_id) const {
    if (!db_) return 0;

    static constexpr const char* sql =
        "SELECT COUNT(*) FROM recent_studies WHERE user_id = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, user_id.data(),
                      static_cast<int>(user_id.size()), SQLITE_TRANSIENT);

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

bool viewer_state_repository::is_valid() const noexcept {
    return db_ != nullptr;
}

// =============================================================================
// Private Implementation
// =============================================================================

viewer_state_record viewer_state_repository::parse_state_row(void* stmt_ptr) const {
    auto* stmt = static_cast<sqlite3_stmt*>(stmt_ptr);
    viewer_state_record record;

    int col = 0;
    record.pk = get_int64_column(stmt, col++);
    record.state_id = get_text_column(stmt, col++);
    record.study_uid = get_text_column(stmt, col++);
    record.user_id = get_text_column(stmt, col++);
    record.state_json = get_text_column(stmt, col++);

    auto created_str = get_text_column(stmt, col++);
    record.created_at = from_timestamp_string(created_str.c_str());

    auto updated_str = get_text_column(stmt, col++);
    record.updated_at = from_timestamp_string(updated_str.c_str());

    return record;
}

recent_study_record viewer_state_repository::parse_recent_study_row(void* stmt_ptr) const {
    auto* stmt = static_cast<sqlite3_stmt*>(stmt_ptr);
    recent_study_record record;

    int col = 0;
    record.pk = get_int64_column(stmt, col++);
    record.user_id = get_text_column(stmt, col++);
    record.study_uid = get_text_column(stmt, col++);

    auto accessed_str = get_text_column(stmt, col++);
    record.accessed_at = from_timestamp_string(accessed_str.c_str());

    return record;
}

}  // namespace pacs::storage
