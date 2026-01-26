/**
 * @file measurement_repository.cpp
 * @brief Implementation of the measurement repository
 *
 * @see Issue #545 - Implement Annotation & Measurement APIs
 * @see Issue #581 - Part 1: Data Models and Repositories
 */

#include "pacs/storage/measurement_repository.hpp"

#ifdef PACS_WITH_DATABASE_SYSTEM


#include <sqlite3.h>

#include <chrono>
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
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}

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

[[nodiscard]] double get_double_column(sqlite3_stmt* stmt, int col, double default_val = 0.0) {
    if (sqlite3_column_type(stmt, col) == SQLITE_NULL) {
        return default_val;
    }
    return sqlite3_column_double(stmt, col);
}

[[nodiscard]] std::optional<int> get_optional_int(sqlite3_stmt* stmt, int col) {
    if (sqlite3_column_type(stmt, col) == SQLITE_NULL) {
        return std::nullopt;
    }
    return sqlite3_column_int(stmt, col);
}

void bind_optional_int(sqlite3_stmt* stmt, int idx, const std::optional<int>& value) {
    if (value.has_value()) {
        sqlite3_bind_int(stmt, idx, value.value());
    } else {
        sqlite3_bind_null(stmt, idx);
    }
}

}  // namespace

measurement_repository::measurement_repository(sqlite3* db) : db_(db) {}

measurement_repository::~measurement_repository() = default;

measurement_repository::measurement_repository(measurement_repository&&) noexcept = default;

auto measurement_repository::operator=(measurement_repository&&) noexcept
    -> measurement_repository& = default;

VoidResult measurement_repository::save(const measurement_record& record) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "measurement_repository"});
    }

    static constexpr const char* sql = R"(
        INSERT INTO measurements (
            measurement_id, sop_instance_uid, frame_number, user_id,
            measurement_type, geometry_json, value, unit, label, created_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(measurement_id) DO UPDATE SET
            geometry_json = excluded.geometry_json,
            value = excluded.value,
            unit = excluded.unit,
            label = excluded.label
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "measurement_repository"});
    }

    auto now_str = to_timestamp_string(std::chrono::system_clock::now());

    int idx = 1;
    sqlite3_bind_text(stmt, idx++, record.measurement_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, record.sop_instance_uid.c_str(), -1, SQLITE_TRANSIENT);
    bind_optional_int(stmt, idx++, record.frame_number);
    sqlite3_bind_text(stmt, idx++, record.user_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, to_string(record.type).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, record.geometry_json.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, idx++, record.value);
    sqlite3_bind_text(stmt, idx++, record.unit.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, record.label.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, now_str.c_str(), -1, SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to save measurement: " + std::string(sqlite3_errmsg(db_)),
            "measurement_repository"});
    }

    return kcenon::common::ok();
}

std::optional<measurement_record> measurement_repository::find_by_id(
    std::string_view measurement_id) const {
    if (!db_) return std::nullopt;

    static constexpr const char* sql = R"(
        SELECT pk, measurement_id, sop_instance_uid, frame_number, user_id,
               measurement_type, geometry_json, value, unit, label, created_at
        FROM measurements WHERE measurement_id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, measurement_id.data(),
                      static_cast<int>(measurement_id.size()), SQLITE_TRANSIENT);

    std::optional<measurement_record> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = parse_row(stmt);
    }

    sqlite3_finalize(stmt);
    return result;
}

std::optional<measurement_record> measurement_repository::find_by_pk(int64_t pk) const {
    if (!db_) return std::nullopt;

    static constexpr const char* sql = R"(
        SELECT pk, measurement_id, sop_instance_uid, frame_number, user_id,
               measurement_type, geometry_json, value, unit, label, created_at
        FROM measurements WHERE pk = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_int64(stmt, 1, pk);

    std::optional<measurement_record> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = parse_row(stmt);
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<measurement_record> measurement_repository::find_by_instance(
    std::string_view sop_instance_uid) const {
    measurement_query query;
    query.sop_instance_uid = std::string(sop_instance_uid);
    return search(query);
}

std::vector<measurement_record> measurement_repository::search(
    const measurement_query& query) const {
    std::vector<measurement_record> result;
    if (!db_) return result;

    std::ostringstream sql;
    sql << R"(
        SELECT pk, measurement_id, sop_instance_uid, frame_number, user_id,
               measurement_type, geometry_json, value, unit, label, created_at
        FROM measurements WHERE 1=1
    )";

    std::vector<std::pair<int, std::string>> bindings;
    int param_idx = 1;

    if (query.sop_instance_uid.has_value()) {
        sql << " AND sop_instance_uid = ?";
        bindings.emplace_back(param_idx++, query.sop_instance_uid.value());
    }

    if (query.user_id.has_value()) {
        sql << " AND user_id = ?";
        bindings.emplace_back(param_idx++, query.user_id.value());
    }

    if (query.type.has_value()) {
        sql << " AND measurement_type = ?";
        bindings.emplace_back(param_idx++, to_string(query.type.value()));
    }

    sql << " ORDER BY created_at DESC";

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
        result.push_back(parse_row(stmt));
    }

    sqlite3_finalize(stmt);
    return result;
}

VoidResult measurement_repository::remove(std::string_view measurement_id) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "measurement_repository"});
    }

    static constexpr const char* sql = "DELETE FROM measurements WHERE measurement_id = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "measurement_repository"});
    }

    sqlite3_bind_text(stmt, 1, measurement_id.data(),
                      static_cast<int>(measurement_id.size()), SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to delete measurement: " + std::string(sqlite3_errmsg(db_)),
            "measurement_repository"});
    }

    return kcenon::common::ok();
}

bool measurement_repository::exists(std::string_view measurement_id) const {
    if (!db_) return false;

    static constexpr const char* sql =
        "SELECT 1 FROM measurements WHERE measurement_id = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, measurement_id.data(),
                      static_cast<int>(measurement_id.size()), SQLITE_TRANSIENT);

    bool found = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return found;
}

size_t measurement_repository::count() const {
    if (!db_) return 0;

    static constexpr const char* sql = "SELECT COUNT(*) FROM measurements";

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

size_t measurement_repository::count(const measurement_query& query) const {
    if (!db_) return 0;

    std::ostringstream sql;
    sql << "SELECT COUNT(*) FROM measurements WHERE 1=1";

    std::vector<std::pair<int, std::string>> bindings;
    int param_idx = 1;

    if (query.sop_instance_uid.has_value()) {
        sql << " AND sop_instance_uid = ?";
        bindings.emplace_back(param_idx++, query.sop_instance_uid.value());
    }

    if (query.user_id.has_value()) {
        sql << " AND user_id = ?";
        bindings.emplace_back(param_idx++, query.user_id.value());
    }

    if (query.type.has_value()) {
        sql << " AND measurement_type = ?";
        bindings.emplace_back(param_idx++, to_string(query.type.value()));
    }

    sqlite3_stmt* stmt = nullptr;
    auto sql_str = sql.str();
    if (sqlite3_prepare_v2(db_, sql_str.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }

    for (const auto& [idx, value] : bindings) {
        sqlite3_bind_text(stmt, idx, value.c_str(), -1, SQLITE_TRANSIENT);
    }

    size_t result = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
    }

    sqlite3_finalize(stmt);
    return result;
}

bool measurement_repository::is_valid() const noexcept {
    return db_ != nullptr;
}

measurement_record measurement_repository::parse_row(void* stmt_ptr) const {
    auto* stmt = static_cast<sqlite3_stmt*>(stmt_ptr);
    measurement_record record;

    int col = 0;
    record.pk = get_int64_column(stmt, col++);
    record.measurement_id = get_text_column(stmt, col++);
    record.sop_instance_uid = get_text_column(stmt, col++);
    record.frame_number = get_optional_int(stmt, col++);
    record.user_id = get_text_column(stmt, col++);

    auto type_str = get_text_column(stmt, col++);
    record.type = measurement_type_from_string(type_str).value_or(measurement_type::length);

    record.geometry_json = get_text_column(stmt, col++);
    record.value = get_double_column(stmt, col++);
    record.unit = get_text_column(stmt, col++);
    record.label = get_text_column(stmt, col++);

    auto created_str = get_text_column(stmt, col++);
    record.created_at = from_timestamp_string(created_str.c_str());

    return record;
}

}  // namespace pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
