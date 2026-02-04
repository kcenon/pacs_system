/**
 * @file recent_study_repository.cpp
 * @brief Implementation of recent_study_repository using base_repository pattern
 *
 * @see Issue #610 - Phase 4: Repository Migrations
 * @see Issue #607 - Phase 2: Base Repository Pattern Implementation
 */

#include "pacs/storage/recent_study_repository.hpp"

#include <chrono>
#include <cstdio>
#include <sstream>

#ifdef PACS_WITH_DATABASE_SYSTEM

namespace pacs::storage {

namespace {

/// Convert time_point to ISO8601 string with milliseconds
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
    auto since_epoch = tp.time_since_epoch();
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(since_epoch);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        since_epoch - secs);

    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    char result[40];
    std::snprintf(
        result, sizeof(result), "%s.%03d", buf, static_cast<int>(ms.count()));
    return result;
}

/// Parse ISO8601 string to time_point
[[nodiscard]] std::chrono::system_clock::time_point from_timestamp_string(
    const std::string& str) {
    if (str.empty()) {
        return {};
    }
    std::tm tm{};
    int ms = 0;
    if (std::sscanf(str.c_str(),
                    "%d-%d-%d %d:%d:%d.%d",
                    &tm.tm_year,
                    &tm.tm_mon,
                    &tm.tm_mday,
                    &tm.tm_hour,
                    &tm.tm_min,
                    &tm.tm_sec,
                    &ms) < 6) {
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

}  // namespace

// =============================================================================
// Constructor
// =============================================================================

recent_study_repository::recent_study_repository(
    std::shared_ptr<pacs_database_adapter> db)
    : base_repository(std::move(db), "recent_studies", "pk") {}

// =============================================================================
// Domain-Specific Queries
// =============================================================================

auto recent_study_repository::record_access(
    std::string_view user_id,
    std::string_view study_uid) -> VoidResult {
    if (!db() || !db()->is_connected()) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not connected", "recent_study_repository"});
    }

    auto now_str = format_timestamp(std::chrono::system_clock::now());

    // Use UPSERT to insert or update
    std::ostringstream sql;
    sql << R"(
        INSERT INTO recent_studies (user_id, study_uid, accessed_at)
        VALUES (')"
        << user_id << "', '" << study_uid << "', '" << now_str
        << R"(')
        ON CONFLICT(user_id, study_uid) DO UPDATE SET
            accessed_at = excluded.accessed_at
    )";

    auto result = db()->insert(sql.str());
    if (result.is_err()) {
        return VoidResult(result.error());
    }

    return kcenon::common::ok();
}

auto recent_study_repository::find_by_user(
    std::string_view user_id,
    size_t limit) -> list_result_type {
    if (!db() || !db()->is_connected()) {
        return list_result_type(kcenon::common::error_info{
            -1, "Database not connected", "recent_study_repository"});
    }

    auto builder = db()->create_query_builder();
    builder.select(select_columns())
        .from(table_name())
        .where("user_id", "=", std::string(user_id))
        .order_by("accessed_at", database::sort_order::desc)
        .order_by("pk", database::sort_order::desc)
        .limit(limit);

    auto query = builder.build();
    auto result = db()->select(query);

    if (result.is_err()) {
        return list_result_type(result.error());
    }

    std::vector<recent_study_record> entities;
    entities.reserve(result.value().size());

    try {
        for (const auto& row : result.value()) {
            entities.push_back(map_row_to_entity(row));
        }
        return list_result_type(std::move(entities));
    } catch (const std::exception& e) {
        return list_result_type(kcenon::common::error_info{
            -1,
            std::string("Failed to map rows to entities: ") + e.what(),
            "recent_study_repository"});
    }
}

auto recent_study_repository::clear_for_user(std::string_view user_id)
    -> VoidResult {
    auto result = remove_where("user_id", "=", std::string(user_id));
    if (result.is_err()) {
        return VoidResult(result.error());
    }
    return kcenon::common::ok();
}

auto recent_study_repository::count_for_user(std::string_view user_id)
    -> Result<size_t> {
    if (!db() || !db()->is_connected()) {
        return Result<size_t>(kcenon::common::error_info{
            -1, "Database not connected", "recent_study_repository"});
    }

    auto builder = db()->create_query_builder();
    builder.select({"COUNT(*) as count"})
        .from(table_name())
        .where("user_id", "=", std::string(user_id));

    auto query = builder.build();
    auto result = db()->select(query);

    if (result.is_err()) {
        return Result<size_t>(result.error());
    }

    if (result.value().empty()) {
        return Result<size_t>(static_cast<size_t>(0));
    }

    try {
        size_t cnt = std::stoull(result.value()[0].at("count"));
        return Result<size_t>(cnt);
    } catch (const std::exception& e) {
        return Result<size_t>(kcenon::common::error_info{
            -1,
            std::string("Failed to parse count: ") + e.what(),
            "recent_study_repository"});
    }
}

auto recent_study_repository::was_recently_accessed(
    std::string_view user_id,
    std::string_view study_uid) -> Result<bool> {
    if (!db() || !db()->is_connected()) {
        return Result<bool>(kcenon::common::error_info{
            -1, "Database not connected", "recent_study_repository"});
    }

    auto user_cond = database::query_condition(
        "user_id", "=", std::string(user_id));
    auto study_cond = database::query_condition(
        "study_uid", "=", std::string(study_uid));

    auto builder = db()->create_query_builder();
    builder.select({"COUNT(*) as count"})
        .from(table_name())
        .where(user_cond && study_cond);

    auto query = builder.build();
    auto result = db()->select(query);

    if (result.is_err()) {
        return Result<bool>(result.error());
    }

    if (result.value().empty()) {
        return Result<bool>(false);
    }

    try {
        int cnt = std::stoi(result.value()[0].at("count"));
        return Result<bool>(cnt > 0);
    } catch (const std::exception& e) {
        return Result<bool>(kcenon::common::error_info{
            -1,
            std::string("Failed to parse count: ") + e.what(),
            "recent_study_repository"});
    }
}

// =============================================================================
// base_repository Overrides
// =============================================================================

auto recent_study_repository::map_row_to_entity(const database_row& row) const
    -> recent_study_record {
    recent_study_record record;

    record.pk = std::stoll(row.at("pk"));
    record.user_id = row.at("user_id");
    record.study_uid = row.at("study_uid");
    record.accessed_at = parse_timestamp(row.at("accessed_at"));

    return record;
}

auto recent_study_repository::entity_to_row(
    const recent_study_record& entity) const
    -> std::map<std::string, database_value> {
    return {{"user_id", entity.user_id},
            {"study_uid", entity.study_uid},
            {"accessed_at", format_timestamp(entity.accessed_at)}};
}

auto recent_study_repository::get_pk(const recent_study_record& entity) const
    -> int64_t {
    return entity.pk;
}

auto recent_study_repository::has_pk(const recent_study_record& entity) const
    -> bool {
    return entity.pk > 0;
}

auto recent_study_repository::select_columns() const
    -> std::vector<std::string> {
    return {"pk", "user_id", "study_uid", "accessed_at"};
}

// =============================================================================
// Private Helpers
// =============================================================================

auto recent_study_repository::parse_timestamp(const std::string& str) const
    -> std::chrono::system_clock::time_point {
    return from_timestamp_string(str);
}

auto recent_study_repository::format_timestamp(
    std::chrono::system_clock::time_point tp) const -> std::string {
    return to_timestamp_string(tp);
}

}  // namespace pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
