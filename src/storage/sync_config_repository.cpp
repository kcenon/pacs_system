/**
 * @file sync_config_repository.cpp
 * @brief Implementation of sync_config_repository using base_repository pattern
 *
 * @see Issue #610 - Phase 4: Repository Migrations
 * @see Issue #607 - Phase 2: Base Repository Pattern Implementation
 */

#include "pacs/storage/sync_config_repository.hpp"

#include <chrono>
#include <cstdio>
#include <sstream>

#ifdef PACS_WITH_DATABASE_SYSTEM

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

// =============================================================================
// Constructor
// =============================================================================

sync_config_repository::sync_config_repository(
    std::shared_ptr<pacs_database_adapter> db)
    : base_repository(std::move(db), "sync_configs", "config_id") {}

// =============================================================================
// Domain-Specific Queries
// =============================================================================

auto sync_config_repository::find_by_config_id(std::string_view config_id)
    -> result_type {
    return find_by_id(std::string(config_id));
}

auto sync_config_repository::find_enabled() -> list_result_type {
    return find_where("enabled", "=", static_cast<int64_t>(1));
}

auto sync_config_repository::find_by_source_node(std::string_view node_id)
    -> list_result_type {
    return find_where("source_node_id", "=", std::string(node_id));
}

auto sync_config_repository::update_stats(
    std::string_view config_id,
    bool success,
    size_t studies_synced) -> VoidResult {
    if (!db() || !db()->is_connected()) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not connected", "sync_config_repository"});
    }

    std::ostringstream sql;
    if (success) {
        sql << R"(
            UPDATE sync_configs SET
                total_syncs = total_syncs + 1,
                studies_synced = studies_synced + )"
            << studies_synced << R"(,
                last_sync = datetime('now'),
                last_successful_sync = datetime('now'),
                updated_at = datetime('now')
            WHERE config_id = ')"
            << config_id << "'";
    } else {
        sql << R"(
            UPDATE sync_configs SET
                total_syncs = total_syncs + 1,
                last_sync = datetime('now'),
                updated_at = datetime('now')
            WHERE config_id = ')"
            << config_id << "'";
    }

    auto result = db()->update(sql.str());
    if (result.is_err()) {
        return VoidResult(result.error());
    }

    return kcenon::common::ok();
}

// =============================================================================
// base_repository Overrides
// =============================================================================

auto sync_config_repository::map_row_to_entity(const database_row& row) const
    -> client::sync_config {
    client::sync_config config;

    config.pk = std::stoll(row.at("pk"));
    config.config_id = row.at("config_id");
    config.source_node_id = row.at("source_node_id");
    config.name = row.at("name");
    config.enabled = (row.at("enabled") == "1");
    config.lookback = std::chrono::hours(std::stoi(row.at("lookback_hours")));
    config.modalities = deserialize_vector(row.at("modalities_json"));
    config.patient_id_patterns =
        deserialize_vector(row.at("patient_patterns_json"));
    config.direction =
        client::sync_direction_from_string(row.at("sync_direction"));
    config.delete_missing = (row.at("delete_missing") == "1");
    config.overwrite_existing = (row.at("overwrite_existing") == "1");
    config.sync_metadata_only = (row.at("sync_metadata_only") == "1");
    config.schedule_cron = row.at("schedule_cron");
    config.last_sync = parse_timestamp(row.at("last_sync"));
    config.last_successful_sync =
        parse_timestamp(row.at("last_successful_sync"));
    config.total_syncs = std::stoull(row.at("total_syncs"));
    config.studies_synced = std::stoull(row.at("studies_synced"));

    return config;
}

auto sync_config_repository::entity_to_row(const client::sync_config& entity)
    const -> std::map<std::string, database_value> {
    return {{"config_id", entity.config_id},
            {"source_node_id", entity.source_node_id},
            {"name", entity.name},
            {"enabled", static_cast<int64_t>(entity.enabled ? 1 : 0)},
            {"lookback_hours", static_cast<int64_t>(entity.lookback.count())},
            {"modalities_json", serialize_vector(entity.modalities)},
            {"patient_patterns_json",
             serialize_vector(entity.patient_id_patterns)},
            {"sync_direction", std::string(client::to_string(entity.direction))},
            {"delete_missing", static_cast<int64_t>(entity.delete_missing ? 1 : 0)},
            {"overwrite_existing",
             static_cast<int64_t>(entity.overwrite_existing ? 1 : 0)},
            {"sync_metadata_only",
             static_cast<int64_t>(entity.sync_metadata_only ? 1 : 0)},
            {"schedule_cron", entity.schedule_cron},
            {"last_sync", format_timestamp(entity.last_sync)},
            {"last_successful_sync",
             format_timestamp(entity.last_successful_sync)},
            {"total_syncs", static_cast<int64_t>(entity.total_syncs)},
            {"studies_synced", static_cast<int64_t>(entity.studies_synced)}};
}

auto sync_config_repository::get_pk(const client::sync_config& entity) const
    -> std::string {
    return entity.config_id;
}

auto sync_config_repository::has_pk(const client::sync_config& entity) const
    -> bool {
    return !entity.config_id.empty();
}

auto sync_config_repository::select_columns() const
    -> std::vector<std::string> {
    return {"pk",
            "config_id",
            "source_node_id",
            "name",
            "enabled",
            "lookback_hours",
            "modalities_json",
            "patient_patterns_json",
            "sync_direction",
            "delete_missing",
            "overwrite_existing",
            "sync_metadata_only",
            "schedule_cron",
            "last_sync",
            "last_successful_sync",
            "total_syncs",
            "studies_synced"};
}

// =============================================================================
// Private Helpers
// =============================================================================

auto sync_config_repository::parse_timestamp(const std::string& str) const
    -> std::chrono::system_clock::time_point {
    return from_timestamp_string(str);
}

auto sync_config_repository::format_timestamp(
    std::chrono::system_clock::time_point tp) const -> std::string {
    return to_timestamp_string(tp);
}

std::string sync_config_repository::serialize_vector(
    const std::vector<std::string>& vec) {
    if (vec.empty()) return "[]";

    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"";
        for (char c : vec[i]) {
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

std::vector<std::string> sync_config_repository::deserialize_vector(
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

}  // namespace pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
