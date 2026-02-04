/**
 * @file sync_conflict_repository.cpp
 * @brief Implementation of sync_conflict_repository using base_repository pattern
 *
 * @see Issue #610 - Phase 4: Repository Migrations
 */

#include "pacs/storage/sync_conflict_repository.hpp"

#include <chrono>
#include <cstdio>
#include <sstream>

#ifdef PACS_WITH_DATABASE_SYSTEM

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
    const std::string& str) {
    if (str.empty()) {
        return {};
    }
    std::tm tm{};
    if (std::sscanf(str.c_str(),
                    "%d-%d-%d %d:%d:%d",
                    &tm.tm_year,
                    &tm.tm_mon,
                    &tm.tm_mday,
                    &tm.tm_hour,
                    &tm.tm_min,
                    &tm.tm_sec) < 6) {
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

}  // namespace

sync_conflict_repository::sync_conflict_repository(
    std::shared_ptr<pacs_database_adapter> db)
    : base_repository(std::move(db), "sync_conflicts", "study_uid") {}

auto sync_conflict_repository::find_by_study_uid(std::string_view study_uid)
    -> result_type {
    return find_by_id(std::string(study_uid));
}

auto sync_conflict_repository::find_by_config(std::string_view config_id)
    -> list_result_type {
    return find_where("config_id", "=", std::string(config_id));
}

auto sync_conflict_repository::find_unresolved() -> list_result_type {
    return find_where("resolved", "=", static_cast<int64_t>(0));
}

auto sync_conflict_repository::resolve(
    std::string_view study_uid,
    client::conflict_resolution resolution) -> VoidResult {
    if (!db() || !db()->is_connected()) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not connected", "sync_conflict_repository"});
    }

    std::ostringstream sql;
    sql << R"(
        UPDATE sync_conflicts SET
            resolved = 1,
            resolution = ')"
        << client::to_string(resolution) << R"(',
            resolved_at = datetime('now')
        WHERE study_uid = ')"
        << study_uid << "'";

    auto result = db()->update(sql.str());
    if (result.is_err()) {
        return VoidResult(result.error());
    }

    return kcenon::common::ok();
}

auto sync_conflict_repository::cleanup_old(std::chrono::hours max_age)
    -> Result<size_t> {
    if (!db() || !db()->is_connected()) {
        return Result<size_t>(kcenon::common::error_info{
            -1, "Database not connected", "sync_conflict_repository"});
    }

    std::ostringstream sql;
    sql << R"(
        DELETE FROM sync_conflicts
        WHERE resolved = 1 AND resolved_at < datetime('now', '-)"
        << max_age.count() << R"( hours'))";

    auto result = db()->remove(sql.str());
    if (result.is_err()) {
        return Result<size_t>(result.error());
    }

    return Result<size_t>(static_cast<size_t>(result.value()));
}

auto sync_conflict_repository::map_row_to_entity(const database_row& row) const
    -> client::sync_conflict {
    client::sync_conflict conflict;

    conflict.pk = std::stoll(row.at("pk"));
    conflict.config_id = row.at("config_id");
    conflict.study_uid = row.at("study_uid");
    conflict.patient_id = row.at("patient_id");
    conflict.conflict_type =
        client::sync_conflict_type_from_string(row.at("conflict_type"));
    conflict.local_modified = parse_timestamp(row.at("local_modified"));
    conflict.remote_modified = parse_timestamp(row.at("remote_modified"));
    conflict.local_instance_count =
        std::stoull(row.at("local_instance_count"));
    conflict.remote_instance_count =
        std::stoull(row.at("remote_instance_count"));
    conflict.resolved = (row.at("resolved") == "1");
    conflict.resolution_used =
        client::conflict_resolution_from_string(row.at("resolution"));
    conflict.detected_at = parse_timestamp(row.at("detected_at"));

    auto resolved_at_str = row.at("resolved_at");
    if (!resolved_at_str.empty()) {
        conflict.resolved_at = parse_timestamp(resolved_at_str);
    }

    return conflict;
}

auto sync_conflict_repository::entity_to_row(
    const client::sync_conflict& entity) const
    -> std::map<std::string, database_value> {
    std::map<std::string, database_value> row = {
        {"config_id", entity.config_id},
        {"study_uid", entity.study_uid},
        {"patient_id", entity.patient_id},
        {"conflict_type", std::string(client::to_string(entity.conflict_type))},
        {"local_modified", format_timestamp(entity.local_modified)},
        {"remote_modified", format_timestamp(entity.remote_modified)},
        {"local_instance_count",
         static_cast<int64_t>(entity.local_instance_count)},
        {"remote_instance_count",
         static_cast<int64_t>(entity.remote_instance_count)},
        {"resolved", static_cast<int64_t>(entity.resolved ? 1 : 0)},
        {"resolution",
         entity.resolved ? std::string(client::to_string(entity.resolution_used))
                         : std::string("")},
        {"detected_at", format_timestamp(entity.detected_at)}};

    if (entity.resolved_at.has_value()) {
        row["resolved_at"] = format_timestamp(entity.resolved_at.value());
    }

    return row;
}

auto sync_conflict_repository::get_pk(const client::sync_conflict& entity) const
    -> std::string {
    return entity.study_uid;
}

auto sync_conflict_repository::has_pk(const client::sync_conflict& entity) const
    -> bool {
    return !entity.study_uid.empty();
}

auto sync_conflict_repository::select_columns() const
    -> std::vector<std::string> {
    return {"pk",
            "config_id",
            "study_uid",
            "patient_id",
            "conflict_type",
            "local_modified",
            "remote_modified",
            "local_instance_count",
            "remote_instance_count",
            "resolved",
            "resolution",
            "detected_at",
            "resolved_at"};
}

auto sync_conflict_repository::parse_timestamp(const std::string& str) const
    -> std::chrono::system_clock::time_point {
    return from_timestamp_string(str);
}

auto sync_conflict_repository::format_timestamp(
    std::chrono::system_clock::time_point tp) const -> std::string {
    return to_timestamp_string(tp);
}

}  // namespace pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
