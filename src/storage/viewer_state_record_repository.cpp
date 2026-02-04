/**
 * @file viewer_state_record_repository.cpp
 * @brief Implementation of viewer_state_record_repository using base_repository pattern
 *
 * @see Issue #610 - Phase 4: Repository Migrations
 * @see Issue #607 - Phase 2: Base Repository Pattern Implementation
 */

#include "pacs/storage/viewer_state_record_repository.hpp"

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

viewer_state_record_repository::viewer_state_record_repository(
    std::shared_ptr<pacs_database_adapter> db)
    : base_repository(std::move(db), "viewer_states", "state_id") {}

// =============================================================================
// Domain-Specific Queries
// =============================================================================

auto viewer_state_record_repository::find_by_study(std::string_view study_uid)
    -> list_result_type {
    return find_where("study_uid", "=", std::string(study_uid));
}

auto viewer_state_record_repository::find_by_user(std::string_view user_id)
    -> list_result_type {
    return find_where("user_id", "=", std::string(user_id));
}

auto viewer_state_record_repository::find_by_study_and_user(
    std::string_view study_uid,
    std::string_view user_id) -> list_result_type {
    if (!db() || !db()->is_connected()) {
        return list_result_type(kcenon::common::error_info{
            -1, "Database not connected", "viewer_state_record_repository"});
    }

    auto study_cond = database::query_condition(
        "study_uid", "=", std::string(study_uid));
    auto user_cond = database::query_condition(
        "user_id", "=", std::string(user_id));

    auto builder = db()->create_query_builder();
    builder.select(select_columns())
        .from(table_name())
        .where(study_cond && user_cond)
        .order_by("updated_at", database::sort_order::desc);

    auto query = builder.build();
    auto result = db()->select(query);

    if (result.is_err()) {
        return list_result_type(result.error());
    }

    std::vector<viewer_state_record> entities;
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
            "viewer_state_record_repository"});
    }
}

auto viewer_state_record_repository::search(const viewer_state_query& query)
    -> list_result_type {
    if (!db() || !db()->is_connected()) {
        return list_result_type(kcenon::common::error_info{
            -1, "Database not connected", "viewer_state_record_repository"});
    }

    auto builder = db()->create_query_builder();
    builder.select(select_columns()).from(table_name());

    // Build condition based on query filters
    std::optional<database::query_condition> condition;

    if (query.study_uid.has_value()) {
        condition = database::query_condition(
            "study_uid", "=", query.study_uid.value());
    }

    if (query.user_id.has_value()) {
        auto user_cond = database::query_condition(
            "user_id", "=", query.user_id.value());
        if (condition.has_value()) {
            condition = condition.value() && user_cond;
        } else {
            condition = user_cond;
        }
    }

    if (condition.has_value()) {
        builder.where(condition.value());
    }

    builder.order_by("updated_at", database::sort_order::desc);

    if (query.limit > 0) {
        builder.limit(query.limit);
        if (query.offset > 0) {
            builder.offset(query.offset);
        }
    }

    auto sql = builder.build();
    auto result = db()->select(sql);

    if (result.is_err()) {
        return list_result_type(result.error());
    }

    std::vector<viewer_state_record> entities;
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
            "viewer_state_record_repository"});
    }
}

// =============================================================================
// base_repository Overrides
// =============================================================================

auto viewer_state_record_repository::map_row_to_entity(
    const database_row& row) const -> viewer_state_record {
    viewer_state_record record;

    record.pk = std::stoll(row.at("pk"));
    record.state_id = row.at("state_id");
    record.study_uid = row.at("study_uid");
    record.user_id = row.at("user_id");
    record.state_json = row.at("state_json");
    record.created_at = parse_timestamp(row.at("created_at"));
    record.updated_at = parse_timestamp(row.at("updated_at"));

    return record;
}

auto viewer_state_record_repository::entity_to_row(
    const viewer_state_record& entity) const
    -> std::map<std::string, database_value> {
    auto now_str = format_timestamp(std::chrono::system_clock::now());

    return {{"state_id", entity.state_id},
            {"study_uid", entity.study_uid},
            {"user_id", entity.user_id},
            {"state_json", entity.state_json},
            {"created_at", now_str},
            {"updated_at", now_str}};
}

auto viewer_state_record_repository::get_pk(
    const viewer_state_record& entity) const -> std::string {
    return entity.state_id;
}

auto viewer_state_record_repository::has_pk(
    const viewer_state_record& entity) const -> bool {
    return !entity.state_id.empty();
}

auto viewer_state_record_repository::select_columns() const
    -> std::vector<std::string> {
    return {"pk",
            "state_id",
            "study_uid",
            "user_id",
            "state_json",
            "created_at",
            "updated_at"};
}

// =============================================================================
// Private Helpers
// =============================================================================

auto viewer_state_record_repository::parse_timestamp(const std::string& str)
    const -> std::chrono::system_clock::time_point {
    return from_timestamp_string(str);
}

auto viewer_state_record_repository::format_timestamp(
    std::chrono::system_clock::time_point tp) const -> std::string {
    return to_timestamp_string(tp);
}

}  // namespace pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
