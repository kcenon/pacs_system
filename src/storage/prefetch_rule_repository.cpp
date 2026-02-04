/**
 * @file prefetch_rule_repository.cpp
 * @brief Implementation of prefetch_rule_repository using base_repository pattern
 *
 * @see Issue #610 - Phase 4: Repository Migrations
 */

#include "pacs/storage/prefetch_rule_repository.hpp"

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

prefetch_rule_repository::prefetch_rule_repository(
    std::shared_ptr<pacs_database_adapter> db)
    : base_repository(std::move(db), "prefetch_rules", "rule_id") {}

auto prefetch_rule_repository::find_by_rule_id(std::string_view rule_id)
    -> result_type {
    return find_by_id(std::string(rule_id));
}

auto prefetch_rule_repository::find_enabled() -> list_result_type {
    return find_where("enabled", "=", static_cast<int64_t>(1));
}

auto prefetch_rule_repository::find_by_trigger(client::prefetch_trigger trigger)
    -> list_result_type {
    return find_where(
        "trigger_type", "=", std::string(client::to_string(trigger)));
}

auto prefetch_rule_repository::enable(std::string_view rule_id) -> VoidResult {
    if (!db() || !db()->is_connected()) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not connected", "prefetch_rule_repository"});
    }

    std::ostringstream sql;
    sql << "UPDATE prefetch_rules SET enabled = 1 WHERE rule_id = '"
        << rule_id << "'";

    auto result = db()->update(sql.str());
    if (result.is_err()) {
        return VoidResult(result.error());
    }

    return kcenon::common::ok();
}

auto prefetch_rule_repository::disable(std::string_view rule_id) -> VoidResult {
    if (!db() || !db()->is_connected()) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not connected", "prefetch_rule_repository"});
    }

    std::ostringstream sql;
    sql << "UPDATE prefetch_rules SET enabled = 0 WHERE rule_id = '"
        << rule_id << "'";

    auto result = db()->update(sql.str());
    if (result.is_err()) {
        return VoidResult(result.error());
    }

    return kcenon::common::ok();
}

auto prefetch_rule_repository::increment_triggered(std::string_view rule_id)
    -> VoidResult {
    if (!db() || !db()->is_connected()) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not connected", "prefetch_rule_repository"});
    }

    std::ostringstream sql;
    sql << R"(
        UPDATE prefetch_rules SET
            triggered_count = triggered_count + 1,
            last_triggered = datetime('now')
        WHERE rule_id = ')"
        << rule_id << "'";

    auto result = db()->update(sql.str());
    if (result.is_err()) {
        return VoidResult(result.error());
    }

    return kcenon::common::ok();
}

auto prefetch_rule_repository::increment_studies_prefetched(
    std::string_view rule_id,
    size_t count) -> VoidResult {
    if (!db() || !db()->is_connected()) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not connected", "prefetch_rule_repository"});
    }

    std::ostringstream sql;
    sql << "UPDATE prefetch_rules SET studies_prefetched = studies_prefetched + "
        << count << " WHERE rule_id = '" << rule_id << "'";

    auto result = db()->update(sql.str());
    if (result.is_err()) {
        return VoidResult(result.error());
    }

    return kcenon::common::ok();
}

auto prefetch_rule_repository::map_row_to_entity(const database_row& row) const
    -> client::prefetch_rule {
    client::prefetch_rule rule;

    rule.pk = std::stoll(row.at("pk"));
    rule.rule_id = row.at("rule_id");
    rule.name = row.at("name");
    rule.enabled = (row.at("enabled") == "1");
    rule.trigger = client::prefetch_trigger_from_string(row.at("trigger_type"));
    rule.modality_filter = row.at("modality_filter");
    rule.body_part_filter = row.at("body_part_filter");
    rule.station_ae_filter = row.at("station_ae_filter");
    rule.prior_lookback =
        std::chrono::hours(std::stoll(row.at("prior_lookback_hours")));
    rule.max_prior_studies = std::stoull(row.at("max_prior_studies"));
    rule.prior_modalities = deserialize_vector(row.at("prior_modalities_json"));
    rule.source_node_ids = deserialize_vector(row.at("source_node_ids_json"));
    rule.schedule_cron = row.at("schedule_cron");
    rule.advance_time =
        std::chrono::minutes(std::stoll(row.at("advance_time_minutes")));
    rule.triggered_count = std::stoull(row.at("triggered_count"));
    rule.studies_prefetched = std::stoull(row.at("studies_prefetched"));
    rule.last_triggered = parse_timestamp(row.at("last_triggered"));

    return rule;
}

auto prefetch_rule_repository::entity_to_row(
    const client::prefetch_rule& entity) const
    -> std::map<std::string, database_value> {
    return {
        {"rule_id", entity.rule_id},
        {"name", entity.name},
        {"enabled", static_cast<int64_t>(entity.enabled ? 1 : 0)},
        {"trigger_type", std::string(client::to_string(entity.trigger))},
        {"modality_filter", entity.modality_filter},
        {"body_part_filter", entity.body_part_filter},
        {"station_ae_filter", entity.station_ae_filter},
        {"prior_lookback_hours",
         static_cast<int64_t>(entity.prior_lookback.count())},
        {"max_prior_studies", static_cast<int64_t>(entity.max_prior_studies)},
        {"prior_modalities_json", serialize_vector(entity.prior_modalities)},
        {"source_node_ids_json", serialize_vector(entity.source_node_ids)},
        {"schedule_cron", entity.schedule_cron},
        {"advance_time_minutes",
         static_cast<int64_t>(entity.advance_time.count())},
        {"triggered_count", static_cast<int64_t>(entity.triggered_count)},
        {"studies_prefetched", static_cast<int64_t>(entity.studies_prefetched)},
        {"last_triggered", format_timestamp(entity.last_triggered)}};
}

auto prefetch_rule_repository::get_pk(const client::prefetch_rule& entity) const
    -> std::string {
    return entity.rule_id;
}

auto prefetch_rule_repository::has_pk(const client::prefetch_rule& entity) const
    -> bool {
    return !entity.rule_id.empty();
}

auto prefetch_rule_repository::select_columns() const
    -> std::vector<std::string> {
    return {"pk",
            "rule_id",
            "name",
            "enabled",
            "trigger_type",
            "modality_filter",
            "body_part_filter",
            "station_ae_filter",
            "prior_lookback_hours",
            "max_prior_studies",
            "prior_modalities_json",
            "source_node_ids_json",
            "schedule_cron",
            "advance_time_minutes",
            "triggered_count",
            "studies_prefetched",
            "last_triggered"};
}

auto prefetch_rule_repository::parse_timestamp(const std::string& str) const
    -> std::chrono::system_clock::time_point {
    return from_timestamp_string(str);
}

auto prefetch_rule_repository::format_timestamp(
    std::chrono::system_clock::time_point tp) const -> std::string {
    return to_timestamp_string(tp);
}

std::string prefetch_rule_repository::serialize_vector(
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

std::vector<std::string> prefetch_rule_repository::deserialize_vector(
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
