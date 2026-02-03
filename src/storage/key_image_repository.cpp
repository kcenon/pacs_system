/**
 * @file key_image_repository.cpp
 * @brief Implementation of the key image repository
 *
 * @see Issue #545 - Implement Annotation & Measurement APIs
 * @see Issue #581 - Part 1: Data Models and Repositories
 * @see Issue #610 - Phase 4: Repository Migrations
 */

#include "pacs/storage/key_image_repository.hpp"

#include <chrono>
#include <sstream>

#ifdef PACS_WITH_DATABASE_SYSTEM

namespace pacs::storage {

// =============================================================================
// Constructor
// =============================================================================

key_image_repository::key_image_repository(
    std::shared_ptr<pacs_database_adapter> db)
    : base_repository(std::move(db), "key_images", "key_image_id") {}

// =============================================================================
// Domain-Specific Operations
// =============================================================================

auto key_image_repository::find_by_pk(int64_t pk) -> result_type {
    if (!db() || !db()->is_connected()) {
        return result_type(
            kcenon::common::error_info{-1, "Database not connected", "storage"});
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
            -1, "Key image not found with pk=" + std::to_string(pk), "storage"});
    }

    return result_type(map_row_to_entity(result.value()[0]));
}

auto key_image_repository::find_by_study(std::string_view study_uid)
    -> list_result_type {
    return find_where("study_uid", "=", std::string(study_uid));
}

auto key_image_repository::search(const key_image_query& query)
    -> list_result_type {
    if (!db() || !db()->is_connected()) {
        return list_result_type(
            kcenon::common::error_info{-1, "Database not connected", "storage"});
    }

    auto builder = query_builder();
    builder.select(select_columns()).from(table_name());

    // Build compound condition
    std::optional<database::query_condition> condition;

    if (query.study_uid.has_value()) {
        auto cond = database::query_condition(
            "study_uid", "=", query.study_uid.value());
        condition = cond;
    }

    if (query.sop_instance_uid.has_value()) {
        auto cond = database::query_condition(
            "sop_instance_uid", "=", query.sop_instance_uid.value());
        if (condition.has_value()) {
            condition = condition.value() && cond;
        } else {
            condition = cond;
        }
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

    std::vector<key_image_record> records;
    records.reserve(result.value().size());
    for (const auto& row : result.value()) {
        records.push_back(map_row_to_entity(row));
    }

    return list_result_type(std::move(records));
}

auto key_image_repository::count_by_study(std::string_view study_uid)
    -> Result<size_t> {
    if (!db() || !db()->is_connected()) {
        return Result<size_t>(
            kcenon::common::error_info{-1, "Database not connected", "storage"});
    }

    auto builder = query_builder();
    builder.select({"COUNT(*) as count"})
        .from(table_name())
        .where("study_uid", "=", std::string(study_uid));

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

auto key_image_repository::map_row_to_entity(const database_row& row) const
    -> key_image_record {
    key_image_record record;

    record.pk = std::stoll(row.at("pk"));
    record.key_image_id = row.at("key_image_id");
    record.study_uid = row.at("study_uid");
    record.sop_instance_uid = row.at("sop_instance_uid");

    // Handle optional frame_number
    auto frame_it = row.find("frame_number");
    if (frame_it != row.end() && !frame_it->second.empty()) {
        record.frame_number = std::stoi(frame_it->second);
    }

    record.user_id = row.at("user_id");
    record.reason = row.at("reason");
    record.document_title = row.at("document_title");

    // Parse created_at timestamp
    auto created_it = row.find("created_at");
    if (created_it != row.end() && !created_it->second.empty()) {
        record.created_at = parse_timestamp(created_it->second);
    }

    return record;
}

auto key_image_repository::entity_to_row(const key_image_record& entity) const
    -> std::map<std::string, database_value> {
    std::map<std::string, database_value> row;

    row["key_image_id"] = entity.key_image_id;
    row["study_uid"] = entity.study_uid;
    row["sop_instance_uid"] = entity.sop_instance_uid;

    if (entity.frame_number.has_value()) {
        row["frame_number"] = static_cast<int64_t>(entity.frame_number.value());
    } else {
        row["frame_number"] = nullptr;
    }

    row["user_id"] = entity.user_id;
    row["reason"] = entity.reason;
    row["document_title"] = entity.document_title;

    // Format timestamp for storage
    if (entity.created_at != std::chrono::system_clock::time_point{}) {
        row["created_at"] = format_timestamp(entity.created_at);
    } else {
        row["created_at"] = format_timestamp(std::chrono::system_clock::now());
    }

    return row;
}

auto key_image_repository::get_pk(const key_image_record& entity) const
    -> std::string {
    return entity.key_image_id;
}

auto key_image_repository::has_pk(const key_image_record& entity) const
    -> bool {
    return !entity.key_image_id.empty();
}

auto key_image_repository::select_columns() const -> std::vector<std::string> {
    return {"pk",         "key_image_id",   "study_uid",   "sop_instance_uid",
            "frame_number", "user_id",      "reason",      "document_title",
            "created_at"};
}

// =============================================================================
// Private Helpers
// =============================================================================

auto key_image_repository::parse_timestamp(const std::string& str) const
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

auto key_image_repository::format_timestamp(
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

key_image_repository::key_image_repository(sqlite3* db) : db_(db) {}

key_image_repository::~key_image_repository() = default;

key_image_repository::key_image_repository(key_image_repository&&) noexcept =
    default;

auto key_image_repository::operator=(key_image_repository&&) noexcept
    -> key_image_repository& = default;

VoidResult key_image_repository::save(const key_image_record& record) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "key_image_repository"});
    }

    static constexpr const char* sql = R"(
        INSERT INTO key_images (
            key_image_id, study_uid, sop_instance_uid, frame_number,
            user_id, reason, document_title, created_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(key_image_id) DO UPDATE SET
            reason = excluded.reason,
            document_title = excluded.document_title
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1,
            "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "key_image_repository"});
    }

    auto now_str = to_timestamp_string(std::chrono::system_clock::now());

    int idx = 1;
    sqlite3_bind_text(stmt, idx++, record.key_image_id.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, record.study_uid.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, record.sop_instance_uid.c_str(), -1,
                      SQLITE_TRANSIENT);
    bind_optional_int(stmt, idx++, record.frame_number);
    sqlite3_bind_text(stmt, idx++, record.user_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, record.reason.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, record.document_title.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, now_str.c_str(), -1, SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1,
            "Failed to save key image: " + std::string(sqlite3_errmsg(db_)),
            "key_image_repository"});
    }

    return kcenon::common::ok();
}

std::optional<key_image_record> key_image_repository::find_by_id(
    std::string_view key_image_id) const {
    if (!db_) return std::nullopt;

    static constexpr const char* sql = R"(
        SELECT pk, key_image_id, study_uid, sop_instance_uid, frame_number,
               user_id, reason, document_title, created_at
        FROM key_images WHERE key_image_id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, key_image_id.data(),
                      static_cast<int>(key_image_id.size()), SQLITE_TRANSIENT);

    std::optional<key_image_record> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = parse_row(stmt);
    }

    sqlite3_finalize(stmt);
    return result;
}

std::optional<key_image_record> key_image_repository::find_by_pk(
    int64_t pk) const {
    if (!db_) return std::nullopt;

    static constexpr const char* sql = R"(
        SELECT pk, key_image_id, study_uid, sop_instance_uid, frame_number,
               user_id, reason, document_title, created_at
        FROM key_images WHERE pk = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_int64(stmt, 1, pk);

    std::optional<key_image_record> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = parse_row(stmt);
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<key_image_record> key_image_repository::find_by_study(
    std::string_view study_uid) const {
    key_image_query query;
    query.study_uid = std::string(study_uid);
    return search(query);
}

std::vector<key_image_record> key_image_repository::search(
    const key_image_query& query) const {
    std::vector<key_image_record> result;
    if (!db_) return result;

    std::ostringstream sql;
    sql << R"(
        SELECT pk, key_image_id, study_uid, sop_instance_uid, frame_number,
               user_id, reason, document_title, created_at
        FROM key_images WHERE 1=1
    )";

    std::vector<std::pair<int, std::string>> bindings;
    int param_idx = 1;

    if (query.study_uid.has_value()) {
        sql << " AND study_uid = ?";
        bindings.emplace_back(param_idx++, query.study_uid.value());
    }

    if (query.sop_instance_uid.has_value()) {
        sql << " AND sop_instance_uid = ?";
        bindings.emplace_back(param_idx++, query.sop_instance_uid.value());
    }

    if (query.user_id.has_value()) {
        sql << " AND user_id = ?";
        bindings.emplace_back(param_idx++, query.user_id.value());
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

VoidResult key_image_repository::remove(std::string_view key_image_id) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "key_image_repository"});
    }

    static constexpr const char* sql =
        "DELETE FROM key_images WHERE key_image_id = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1,
            "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "key_image_repository"});
    }

    sqlite3_bind_text(stmt, 1, key_image_id.data(),
                      static_cast<int>(key_image_id.size()), SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1,
            "Failed to delete key image: " + std::string(sqlite3_errmsg(db_)),
            "key_image_repository"});
    }

    return kcenon::common::ok();
}

bool key_image_repository::exists(std::string_view key_image_id) const {
    if (!db_) return false;

    static constexpr const char* sql =
        "SELECT 1 FROM key_images WHERE key_image_id = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, key_image_id.data(),
                      static_cast<int>(key_image_id.size()), SQLITE_TRANSIENT);

    bool found = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return found;
}

size_t key_image_repository::count() const {
    if (!db_) return 0;

    static constexpr const char* sql = "SELECT COUNT(*) FROM key_images";

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

size_t key_image_repository::count_by_study(std::string_view study_uid) const {
    if (!db_) return 0;

    static constexpr const char* sql =
        "SELECT COUNT(*) FROM key_images WHERE study_uid = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, study_uid.data(),
                      static_cast<int>(study_uid.size()), SQLITE_TRANSIENT);

    size_t result = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
    }

    sqlite3_finalize(stmt);
    return result;
}

bool key_image_repository::is_valid() const noexcept { return db_ != nullptr; }

key_image_record key_image_repository::parse_row(void* stmt_ptr) const {
    auto* stmt = static_cast<sqlite3_stmt*>(stmt_ptr);
    key_image_record record;

    int col = 0;
    record.pk = get_int64_column(stmt, col++);
    record.key_image_id = get_text_column(stmt, col++);
    record.study_uid = get_text_column(stmt, col++);
    record.sop_instance_uid = get_text_column(stmt, col++);
    record.frame_number = get_optional_int(stmt, col++);
    record.user_id = get_text_column(stmt, col++);
    record.reason = get_text_column(stmt, col++);
    record.document_title = get_text_column(stmt, col++);

    auto created_str = get_text_column(stmt, col++);
    record.created_at = from_timestamp_string(created_str.c_str());

    return record;
}

}  // namespace pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
