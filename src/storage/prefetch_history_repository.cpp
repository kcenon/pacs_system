/**
 * @file prefetch_history_repository.cpp
 * @brief Implementation of prefetch_history_repository using base_repository pattern
 *
 * @see Issue #610 - Phase 4: Repository Migrations
 */

#include "pacs/storage/prefetch_history_repository.hpp"

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

prefetch_history_repository::prefetch_history_repository(
    std::shared_ptr<pacs_database_adapter> db)
    : base_repository(std::move(db), "prefetch_history", "pk") {}

auto prefetch_history_repository::find_by_patient(
    std::string_view patient_id,
    size_t limit) -> list_result_type {
    if (!db() || !db()->is_connected()) {
        return list_result_type(kcenon::common::error_info{
            -1, "Database not connected", "prefetch_history_repository"});
    }

    auto builder = db()->create_query_builder();
    builder.select(select_columns())
        .from(table_name())
        .where("patient_id", "=", std::string(patient_id))
        .order_by("prefetched_at", database::sort_order::desc)
        .limit(limit);

    auto result = db()->select(builder.build());
    if (result.is_err()) {
        return list_result_type(result.error());
    }

    std::vector<client::prefetch_history> entities;
    entities.reserve(result.value().size());
    for (const auto& row : result.value()) {
        entities.push_back(map_row_to_entity(row));
    }

    return list_result_type(std::move(entities));
}

auto prefetch_history_repository::find_by_study(std::string_view study_uid)
    -> list_result_type {
    return find_where("study_uid", "=", std::string(study_uid));
}

auto prefetch_history_repository::find_by_rule(
    std::string_view rule_id,
    size_t limit) -> list_result_type {
    if (!db() || !db()->is_connected()) {
        return list_result_type(kcenon::common::error_info{
            -1, "Database not connected", "prefetch_history_repository"});
    }

    auto builder = db()->create_query_builder();
    builder.select(select_columns())
        .from(table_name())
        .where("rule_id", "=", std::string(rule_id))
        .order_by("prefetched_at", database::sort_order::desc)
        .limit(limit);

    auto result = db()->select(builder.build());
    if (result.is_err()) {
        return list_result_type(result.error());
    }

    std::vector<client::prefetch_history> entities;
    entities.reserve(result.value().size());
    for (const auto& row : result.value()) {
        entities.push_back(map_row_to_entity(row));
    }

    return list_result_type(std::move(entities));
}

auto prefetch_history_repository::find_by_status(
    std::string_view status,
    size_t limit) -> list_result_type {
    if (!db() || !db()->is_connected()) {
        return list_result_type(kcenon::common::error_info{
            -1, "Database not connected", "prefetch_history_repository"});
    }

    auto builder = db()->create_query_builder();
    builder.select(select_columns())
        .from(table_name())
        .where("status", "=", std::string(status))
        .order_by("prefetched_at", database::sort_order::desc)
        .limit(limit);

    auto result = db()->select(builder.build());
    if (result.is_err()) {
        return list_result_type(result.error());
    }

    std::vector<client::prefetch_history> entities;
    entities.reserve(result.value().size());
    for (const auto& row : result.value()) {
        entities.push_back(map_row_to_entity(row));
    }

    return list_result_type(std::move(entities));
}

auto prefetch_history_repository::find_recent(size_t limit) -> list_result_type {
    if (!db() || !db()->is_connected()) {
        return list_result_type(kcenon::common::error_info{
            -1, "Database not connected", "prefetch_history_repository"});
    }

    auto builder = db()->create_query_builder();
    builder.select(select_columns())
        .from(table_name())
        .order_by("prefetched_at", database::sort_order::desc)
        .limit(limit);

    auto result = db()->select(builder.build());
    if (result.is_err()) {
        return list_result_type(result.error());
    }

    std::vector<client::prefetch_history> entities;
    entities.reserve(result.value().size());
    for (const auto& row : result.value()) {
        entities.push_back(map_row_to_entity(row));
    }

    return list_result_type(std::move(entities));
}

auto prefetch_history_repository::update_status(
    int64_t pk,
    std::string_view status) -> VoidResult {
    if (!db() || !db()->is_connected()) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not connected", "prefetch_history_repository"});
    }

    std::ostringstream sql;
    sql << "UPDATE prefetch_history SET status = '" << status
        << "' WHERE pk = " << pk;

    auto result = db()->update(sql.str());
    if (result.is_err()) {
        return VoidResult(result.error());
    }

    return kcenon::common::ok();
}

auto prefetch_history_repository::cleanup_old(std::chrono::hours max_age)
    -> Result<size_t> {
    if (!db() || !db()->is_connected()) {
        return Result<size_t>(kcenon::common::error_info{
            -1, "Database not connected", "prefetch_history_repository"});
    }

    std::ostringstream sql;
    sql << R"(
        DELETE FROM prefetch_history
        WHERE prefetched_at < datetime('now', '-)"
        << max_age.count() << R"( hours'))";

    auto result = db()->remove(sql.str());
    if (result.is_err()) {
        return Result<size_t>(result.error());
    }

    return Result<size_t>(static_cast<size_t>(result.value()));
}

auto prefetch_history_repository::map_row_to_entity(
    const database_row& row) const -> client::prefetch_history {
    client::prefetch_history history;

    history.pk = std::stoll(row.at("pk"));
    history.patient_id = row.at("patient_id");
    history.study_uid = row.at("study_uid");
    history.rule_id = row.at("rule_id");
    history.source_node_id = row.at("source_node_id");
    history.job_id = row.at("job_id");
    history.status = row.at("status");
    history.prefetched_at = parse_timestamp(row.at("prefetched_at"));

    return history;
}

auto prefetch_history_repository::entity_to_row(
    const client::prefetch_history& entity) const
    -> std::map<std::string, database_value> {
    return {{"patient_id", entity.patient_id},
            {"study_uid", entity.study_uid},
            {"rule_id", entity.rule_id},
            {"source_node_id", entity.source_node_id},
            {"job_id", entity.job_id},
            {"status", entity.status},
            {"prefetched_at", format_timestamp(entity.prefetched_at)}};
}

auto prefetch_history_repository::get_pk(
    const client::prefetch_history& entity) const -> int64_t {
    return entity.pk;
}

auto prefetch_history_repository::has_pk(
    const client::prefetch_history& entity) const -> bool {
    return entity.pk > 0;
}

auto prefetch_history_repository::select_columns() const
    -> std::vector<std::string> {
    return {"pk",
            "patient_id",
            "study_uid",
            "rule_id",
            "source_node_id",
            "job_id",
            "status",
            "prefetched_at"};
}

auto prefetch_history_repository::parse_timestamp(const std::string& str) const
    -> std::chrono::system_clock::time_point {
    return from_timestamp_string(str);
}

auto prefetch_history_repository::format_timestamp(
    std::chrono::system_clock::time_point tp) const -> std::string {
    return to_timestamp_string(tp);
}

}  // namespace pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
