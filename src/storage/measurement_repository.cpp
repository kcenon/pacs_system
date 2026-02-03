/**
 * @file measurement_repository.cpp
 * @brief Implementation of the measurement repository
 *
 * @see Issue #545 - Implement Annotation & Measurement APIs
 * @see Issue #581 - Part 1: Data Models and Repositories
 * @see Issue #610 - Phase 4: Repository Migrations
 */

#include "pacs/storage/measurement_repository.hpp"

#include <chrono>
#include <sstream>

#ifdef PACS_WITH_DATABASE_SYSTEM

namespace pacs::storage {

// =============================================================================
// Constructor
// =============================================================================

measurement_repository::measurement_repository(
    std::shared_ptr<pacs_database_adapter> db)
    : base_repository(std::move(db), "measurements", "measurement_id") {}

// =============================================================================
// Domain-Specific Operations
// =============================================================================

auto measurement_repository::find_by_pk(int64_t pk) -> result_type {
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
            -1, "Measurement not found with pk=" + std::to_string(pk),
            "storage"});
    }

    return result_type(map_row_to_entity(result.value()[0]));
}

auto measurement_repository::find_by_instance(std::string_view sop_instance_uid)
    -> list_result_type {
    return find_where("sop_instance_uid", "=", std::string(sop_instance_uid));
}

auto measurement_repository::search(const measurement_query& query)
    -> list_result_type {
    if (!db() || !db()->is_connected()) {
        return list_result_type(kcenon::common::error_info{
            -1, "Database not connected", "storage"});
    }

    auto builder = query_builder();
    builder.select(select_columns()).from(table_name());

    // Build compound condition
    std::optional<database::query_condition> condition;

    if (query.sop_instance_uid.has_value()) {
        auto cond = database::query_condition(
            "sop_instance_uid", "=", query.sop_instance_uid.value());
        condition = cond;
    }

    if (query.user_id.has_value()) {
        auto cond =
            database::query_condition("user_id", "=", query.user_id.value());
        if (condition.has_value()) {
            condition = condition.value() && cond;
        } else {
            condition = cond;
        }
    }

    if (query.type.has_value()) {
        std::string type_str = to_string(query.type.value());
        auto cond =
            database::query_condition("measurement_type", "=", type_str);
        if (condition.has_value()) {
            condition = condition.value() && cond;
        } else {
            condition = cond;
        }
    }

    if (condition.has_value()) {
        builder.where(condition.value());
    }

    // Apply ordering
    builder.order_by("created_at", database::sort_order::desc);

    // Apply pagination
    if (query.limit > 0) {
        builder.limit(query.limit);
        if (query.offset > 0) {
            builder.offset(query.offset);
        }
    }

    auto result = db()->select(builder.build());
    if (result.is_err()) {
        return list_result_type(result.error());
    }

    std::vector<measurement_record> records;
    records.reserve(result.value().size());
    for (const auto& row : result.value()) {
        records.push_back(map_row_to_entity(row));
    }

    return list_result_type(std::move(records));
}

auto measurement_repository::count(const measurement_query& query)
    -> Result<size_t> {
    if (!db() || !db()->is_connected()) {
        return Result<size_t>(kcenon::common::error_info{
            -1, "Database not connected", "storage"});
    }

    auto builder = query_builder();
    builder.select({"COUNT(*) as count"}).from(table_name());

    // Build compound condition
    std::optional<database::query_condition> condition;

    if (query.sop_instance_uid.has_value()) {
        auto cond = database::query_condition(
            "sop_instance_uid", "=", query.sop_instance_uid.value());
        condition = cond;
    }

    if (query.user_id.has_value()) {
        auto cond =
            database::query_condition("user_id", "=", query.user_id.value());
        if (condition.has_value()) {
            condition = condition.value() && cond;
        } else {
            condition = cond;
        }
    }

    if (query.type.has_value()) {
        std::string type_str = to_string(query.type.value());
        auto cond =
            database::query_condition("measurement_type", "=", type_str);
        if (condition.has_value()) {
            condition = condition.value() && cond;
        } else {
            condition = cond;
        }
    }

    if (condition.has_value()) {
        builder.where(condition.value());
    }

    auto result = db()->select(builder.build());
    if (result.is_err()) {
        return Result<size_t>(result.error());
    }

    if (result.value().empty()) {
        return Result<size_t>(static_cast<size_t>(0));
    }

    return Result<size_t>(std::stoull(result.value()[0].at("count")));
}

// =============================================================================
// base_repository Overrides
// =============================================================================

auto measurement_repository::map_row_to_entity(const database_row& row) const
    -> measurement_record {
    measurement_record record;

    record.pk = std::stoll(row.at("pk"));
    record.measurement_id = row.at("measurement_id");
    record.sop_instance_uid = row.at("sop_instance_uid");

    // Handle optional frame_number
    auto frame_it = row.find("frame_number");
    if (frame_it != row.end() && !frame_it->second.empty()) {
        record.frame_number = std::stoi(frame_it->second);
    }

    record.user_id = row.at("user_id");

    // Parse measurement type
    auto type_str = row.at("measurement_type");
    record.type =
        measurement_type_from_string(type_str).value_or(measurement_type::length);

    record.geometry_json = row.at("geometry_json");
    record.value = std::stod(row.at("value"));
    record.unit = row.at("unit");
    record.label = row.at("label");

    // Parse created_at timestamp
    auto created_it = row.find("created_at");
    if (created_it != row.end() && !created_it->second.empty()) {
        record.created_at = parse_timestamp(created_it->second);
    }

    return record;
}

auto measurement_repository::entity_to_row(
    const measurement_record& entity) const
    -> std::map<std::string, database_value> {
    std::map<std::string, database_value> row;

    row["measurement_id"] = entity.measurement_id;
    row["sop_instance_uid"] = entity.sop_instance_uid;

    if (entity.frame_number.has_value()) {
        row["frame_number"] =
            static_cast<int64_t>(entity.frame_number.value());
    } else {
        row["frame_number"] = nullptr;
    }

    row["user_id"] = entity.user_id;
    row["measurement_type"] = to_string(entity.type);
    row["geometry_json"] = entity.geometry_json;
    row["value"] = entity.value;
    row["unit"] = entity.unit;
    row["label"] = entity.label;

    // Format timestamp for storage
    if (entity.created_at != std::chrono::system_clock::time_point{}) {
        row["created_at"] = format_timestamp(entity.created_at);
    } else {
        row["created_at"] = format_timestamp(std::chrono::system_clock::now());
    }

    return row;
}

auto measurement_repository::get_pk(const measurement_record& entity) const
    -> std::string {
    return entity.measurement_id;
}

auto measurement_repository::has_pk(const measurement_record& entity) const
    -> bool {
    return !entity.measurement_id.empty();
}

auto measurement_repository::select_columns() const
    -> std::vector<std::string> {
    return {"pk",
            "measurement_id",
            "sop_instance_uid",
            "frame_number",
            "user_id",
            "measurement_type",
            "geometry_json",
            "value",
            "unit",
            "label",
            "created_at"};
}

// =============================================================================
// Private Helpers
// =============================================================================

auto measurement_repository::parse_timestamp(const std::string& str) const
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

auto measurement_repository::format_timestamp(
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

}  // namespace pacs::storage

#else  // !PACS_WITH_DATABASE_SYSTEM

// =============================================================================
// Legacy SQLite Implementation
// =============================================================================

#include <sqlite3.h>

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

[[nodiscard]] std::string get_text_column(sqlite3_stmt* stmt, int col) {
    auto text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, col));
    return text ? text : "";
}

[[nodiscard]] int64_t get_int64_column(sqlite3_stmt* stmt, int col,
                                       int64_t default_val = 0) {
    if (sqlite3_column_type(stmt, col) == SQLITE_NULL) {
        return default_val;
    }
    return sqlite3_column_int64(stmt, col);
}

[[nodiscard]] double get_double_column(sqlite3_stmt* stmt, int col,
                                       double default_val = 0.0) {
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

void bind_optional_int(sqlite3_stmt* stmt, int idx,
                       const std::optional<int>& value) {
    if (value.has_value()) {
        sqlite3_bind_int(stmt, idx, value.value());
    } else {
        sqlite3_bind_null(stmt, idx);
    }
}

}  // namespace

measurement_repository::measurement_repository(sqlite3* db) : db_(db) {}

measurement_repository::~measurement_repository() = default;

measurement_repository::measurement_repository(
    measurement_repository&&) noexcept = default;

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
            -1,
            "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "measurement_repository"});
    }

    auto now_str = to_timestamp_string(std::chrono::system_clock::now());

    int idx = 1;
    sqlite3_bind_text(stmt, idx++, record.measurement_id.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, record.sop_instance_uid.c_str(), -1,
                      SQLITE_TRANSIENT);
    bind_optional_int(stmt, idx++, record.frame_number);
    sqlite3_bind_text(stmt, idx++, record.user_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, to_string(record.type).c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, record.geometry_json.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, idx++, record.value);
    sqlite3_bind_text(stmt, idx++, record.unit.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, record.label.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, now_str.c_str(), -1, SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1,
            "Failed to save measurement: " + std::string(sqlite3_errmsg(db_)),
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

std::optional<measurement_record> measurement_repository::find_by_pk(
    int64_t pk) const {
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

VoidResult measurement_repository::remove(std::string_view measurement_id) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "measurement_repository"});
    }

    static constexpr const char* sql =
        "DELETE FROM measurements WHERE measurement_id = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1,
            "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "measurement_repository"});
    }

    sqlite3_bind_text(stmt, 1, measurement_id.data(),
                      static_cast<int>(measurement_id.size()), SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1,
            "Failed to delete measurement: " + std::string(sqlite3_errmsg(db_)),
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
    if (sqlite3_prepare_v2(db_, sql_str.c_str(), -1, &stmt, nullptr) !=
        SQLITE_OK) {
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
    record.type =
        measurement_type_from_string(type_str).value_or(measurement_type::length);

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
