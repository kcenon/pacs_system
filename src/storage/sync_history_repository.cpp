/**
 * @file sync_history_repository.cpp
 * @brief Implementation of sync_history_repository using base_repository pattern
 *
 * @see Issue #610 - Phase 4: Repository Migrations
 */

#include "pacs/storage/sync_history_repository.hpp"

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

sync_history_repository::sync_history_repository(
    std::shared_ptr<pacs_database_adapter> db)
    : base_repository(std::move(db), "sync_history", "pk") {}

auto sync_history_repository::find_by_config(
    std::string_view config_id,
    size_t limit) -> list_result_type {
    if (!db() || !db()->is_connected()) {
        return list_result_type(kcenon::common::error_info{
            -1, "Database not connected", "sync_history_repository"});
    }

    auto builder = db()->create_query_builder();
    builder.select(select_columns())
        .from(table_name())
        .where("config_id", "=", std::string(config_id))
        .order_by("started_at", database::sort_order::desc)
        .limit(limit);

    auto result = db()->select(builder.build());
    if (result.is_err()) {
        return list_result_type(result.error());
    }

    std::vector<client::sync_history> entities;
    entities.reserve(result.value().size());
    for (const auto& row : result.value()) {
        entities.push_back(map_row_to_entity(row));
    }

    return list_result_type(std::move(entities));
}

auto sync_history_repository::find_last_for_config(std::string_view config_id)
    -> result_type {
    if (!db() || !db()->is_connected()) {
        return result_type(kcenon::common::error_info{
            -1, "Database not connected", "sync_history_repository"});
    }

    auto builder = db()->create_query_builder();
    builder.select(select_columns())
        .from(table_name())
        .where("config_id", "=", std::string(config_id))
        .order_by("started_at", database::sort_order::desc)
        .limit(1);

    auto result = db()->select(builder.build());
    if (result.is_err()) {
        return result_type(result.error());
    }

    if (result.value().empty()) {
        return result_type(kcenon::common::error_info{
            -1, "No history found for config", "sync_history_repository"});
    }

    return result_type(map_row_to_entity(result.value()[0]));
}

auto sync_history_repository::cleanup_old(std::chrono::hours max_age)
    -> Result<size_t> {
    if (!db() || !db()->is_connected()) {
        return Result<size_t>(kcenon::common::error_info{
            -1, "Database not connected", "sync_history_repository"});
    }

    std::ostringstream sql;
    sql << R"(
        DELETE FROM sync_history
        WHERE started_at < datetime('now', '-)"
        << max_age.count() << R"( hours'))";

    auto result = db()->remove(sql.str());
    if (result.is_err()) {
        return Result<size_t>(result.error());
    }

    return Result<size_t>(static_cast<size_t>(result.value()));
}

auto sync_history_repository::map_row_to_entity(const database_row& row) const
    -> client::sync_history {
    client::sync_history history;

    history.pk = std::stoll(row.at("pk"));
    history.config_id = row.at("config_id");
    history.job_id = row.at("job_id");
    history.success = (row.at("success") == "1");
    history.studies_checked = std::stoull(row.at("studies_checked"));
    history.studies_synced = std::stoull(row.at("studies_synced"));
    history.conflicts_found = std::stoull(row.at("conflicts_found"));
    history.errors = deserialize_errors(row.at("errors_json"));
    history.started_at = parse_timestamp(row.at("started_at"));
    history.completed_at = parse_timestamp(row.at("completed_at"));

    return history;
}

auto sync_history_repository::entity_to_row(
    const client::sync_history& entity) const
    -> std::map<std::string, database_value> {
    return {{"config_id", entity.config_id},
            {"job_id", entity.job_id},
            {"success", static_cast<int64_t>(entity.success ? 1 : 0)},
            {"studies_checked", static_cast<int64_t>(entity.studies_checked)},
            {"studies_synced", static_cast<int64_t>(entity.studies_synced)},
            {"conflicts_found", static_cast<int64_t>(entity.conflicts_found)},
            {"errors_json", serialize_errors(entity.errors)},
            {"started_at", format_timestamp(entity.started_at)},
            {"completed_at", format_timestamp(entity.completed_at)}};
}

auto sync_history_repository::get_pk(const client::sync_history& entity) const
    -> int64_t {
    return entity.pk;
}

auto sync_history_repository::has_pk(const client::sync_history& entity) const
    -> bool {
    return entity.pk > 0;
}

auto sync_history_repository::select_columns() const
    -> std::vector<std::string> {
    return {"pk",
            "config_id",
            "job_id",
            "success",
            "studies_checked",
            "studies_synced",
            "conflicts_found",
            "errors_json",
            "started_at",
            "completed_at"};
}

auto sync_history_repository::parse_timestamp(const std::string& str) const
    -> std::chrono::system_clock::time_point {
    return from_timestamp_string(str);
}

auto sync_history_repository::format_timestamp(
    std::chrono::system_clock::time_point tp) const -> std::string {
    return to_timestamp_string(tp);
}

std::string sync_history_repository::serialize_errors(
    const std::vector<std::string>& errors) {
    if (errors.empty()) return "[]";

    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < errors.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"";
        for (char c : errors[i]) {
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

std::vector<std::string> sync_history_repository::deserialize_errors(
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
