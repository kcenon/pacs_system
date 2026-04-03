// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file ups_repository.cpp
 * @brief Implementation of the UPS repository
 *
 * @see Issue #915 - Extract UPS lifecycle and subscription repository
 */

#include "kcenon/pacs/storage/ups_repository.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#include <kcenon/pacs/compat/format.h>

#ifdef PACS_WITH_DATABASE_SYSTEM

#include <database/query_builder.h>

namespace kcenon::pacs::storage {

using kcenon::common::make_error;
using kcenon::common::ok;

namespace {

auto get_string(const database_row& row, const std::string& key) -> std::string {
    auto it = row.find(key);
    return (it != row.end()) ? it->second : std::string{};
}

auto get_int64(const database_row& row, const std::string& key) -> int64_t {
    auto it = row.find(key);
    if (it == row.end() || it->second.empty()) {
        return 0;
    }

    try {
        return std::stoll(it->second);
    } catch (...) {
        return 0;
    }
}

auto get_int(const database_row& row, const std::string& key) -> int {
    auto it = row.find(key);
    if (it == row.end() || it->second.empty()) {
        return 0;
    }

    try {
        return std::stoi(it->second);
    } catch (...) {
        return 0;
    }
}

auto get_bool(const database_row& row, const std::string& key) -> bool {
    return get_int(row, key) != 0;
}

}  // namespace

ups_repository::ups_repository(std::shared_ptr<pacs_database_adapter> db)
    : base_repository(std::move(db), "ups_workitems", "workitem_pk") {}

auto ups_repository::to_like_pattern(std::string_view pattern) -> std::string {
    std::string result;
    result.reserve(pattern.size());

    for (char c : pattern) {
        if (c == '*') {
            result += '%';
        } else if (c == '?') {
            result += '_';
        } else if (c == '%' || c == '_') {
            result += '\\';
            result += c;
        } else {
            result += c;
        }
    }

    return result;
}

auto ups_repository::parse_timestamp(const std::string& str) const
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
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

auto ups_repository::format_timestamp(
    std::chrono::system_clock::time_point tp) const -> std::string {
    if (tp == std::chrono::system_clock::time_point{}) {
        return "";
    }

    auto time = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif

    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}

auto ups_repository::create_ups_workitem(const ups_workitem& workitem)
    -> Result<int64_t> {
    if (workitem.workitem_uid.empty()) {
        return make_error<int64_t>(-1, "UPS workitem UID is required",
                                   "storage");
    }
    if (!db() || !db()->is_connected()) {
        return make_error<int64_t>(-1, "Database not connected", "storage");
    }

    std::map<std::string, database::core::database_value> insert_data{
        {"workitem_uid", workitem.workitem_uid},
        {"state", std::string("SCHEDULED")},
        {"procedure_step_label", workitem.procedure_step_label},
        {"worklist_label", workitem.worklist_label},
        {"scheduled_start_datetime", workitem.scheduled_start_datetime},
        {"expected_completion_datetime",
         workitem.expected_completion_datetime},
        {"scheduled_station_name", workitem.scheduled_station_name},
        {"scheduled_station_class", workitem.scheduled_station_class},
        {"scheduled_station_geographic",
         workitem.scheduled_station_geographic},
        {"scheduled_human_performers",
         workitem.scheduled_human_performers},
        {"input_information", workitem.input_information},
        {"performing_ae", workitem.performing_ae},
        {"progress_description", workitem.progress_description},
        {"progress_percent",
         static_cast<int64_t>(workitem.progress_percent)},
        {"output_information", workitem.output_information},
        {"transaction_uid", workitem.transaction_uid},
        {"priority", workitem.priority.empty() ? std::string("MEDIUM")
                                               : workitem.priority}};

    auto builder = query_builder();
    builder.insert_into(table_name()).values(insert_data);

    auto insert_result = db()->insert(builder.build());
    if (insert_result.is_err()) {
        return make_error<int64_t>(
            -1,
            kcenon::pacs::compat::format("Failed to create UPS workitem: {}",
                                 insert_result.error().message),
            "storage");
    }

    auto inserted = find_ups_workitem(workitem.workitem_uid);
    if (!inserted.has_value()) {
        return make_error<int64_t>(
            -1, "UPS workitem inserted but could not retrieve record",
            "storage");
    }

    return inserted->pk;
}

auto ups_repository::update_ups_workitem(const ups_workitem& workitem)
    -> VoidResult {
    if (workitem.workitem_uid.empty()) {
        return make_error<std::monostate>(
            -1, "UPS workitem UID is required for update", "storage");
    }

    auto existing = find_ups_workitem(workitem.workitem_uid);
    if (!existing.has_value()) {
        return make_error<std::monostate>(
            -1,
            kcenon::pacs::compat::format("UPS workitem not found: {}",
                                 workitem.workitem_uid),
            "storage");
    }

    if (existing->is_final()) {
        return make_error<std::monostate>(
            -1,
            kcenon::pacs::compat::format("Cannot update workitem in final state: {}",
                                 existing->state),
            "storage");
    }

    if (!db() || !db()->is_connected()) {
        return make_error<std::monostate>(-1, "Database not connected",
                                          "storage");
    }

    std::map<std::string, database::core::database_value> update_data{
        {"procedure_step_label", workitem.procedure_step_label},
        {"worklist_label", workitem.worklist_label},
        {"priority", workitem.priority},
        {"scheduled_start_datetime", workitem.scheduled_start_datetime},
        {"expected_completion_datetime",
         workitem.expected_completion_datetime},
        {"scheduled_station_name", workitem.scheduled_station_name},
        {"scheduled_station_class", workitem.scheduled_station_class},
        {"scheduled_station_geographic",
         workitem.scheduled_station_geographic},
        {"scheduled_human_performers",
         workitem.scheduled_human_performers},
        {"input_information", workitem.input_information},
        {"performing_ae", workitem.performing_ae},
        {"progress_description", workitem.progress_description},
        {"progress_percent",
         static_cast<int64_t>(workitem.progress_percent)},
        {"output_information", workitem.output_information},
        {"updated_at", std::string("datetime('now')")}};

    auto builder = query_builder();
    builder.update(table_name())
        .set(update_data)
        .where("workitem_uid", "=", workitem.workitem_uid);

    auto result = db()->update(builder.build());
    if (result.is_err()) {
        return make_error<std::monostate>(
            -1,
            kcenon::pacs::compat::format("Failed to update UPS workitem: {}",
                                 result.error().message),
            "storage");
    }

    return ok();
}

auto ups_repository::change_ups_state(std::string_view workitem_uid,
                                      std::string_view new_state,
                                      std::string_view transaction_uid)
    -> VoidResult {
    if (!parse_ups_state(new_state).has_value()) {
        return make_error<std::monostate>(
            -1, kcenon::pacs::compat::format("Invalid UPS state: {}", new_state),
            "storage");
    }

    auto existing = find_ups_workitem(workitem_uid);
    if (!existing.has_value()) {
        return make_error<std::monostate>(
            -1,
            kcenon::pacs::compat::format("UPS workitem not found: {}", workitem_uid),
            "storage");
    }

    auto current = existing->get_state();
    auto target = parse_ups_state(new_state);

    if (existing->is_final()) {
        return make_error<std::monostate>(
            -1,
            kcenon::pacs::compat::format("Cannot change state from final state: {}",
                                 existing->state),
            "storage");
    }

    if (current == ups_state::scheduled) {
        if (target != ups_state::in_progress &&
            target != ups_state::canceled) {
            return make_error<std::monostate>(
                -1,
                kcenon::pacs::compat::format(
                    "Invalid transition from SCHEDULED to {}", new_state),
                "storage");
        }
    } else if (current == ups_state::in_progress) {
        if (target != ups_state::completed &&
            target != ups_state::canceled) {
            return make_error<std::monostate>(
                -1,
                kcenon::pacs::compat::format(
                    "Invalid transition from IN PROGRESS to {}", new_state),
                "storage");
        }
    }

    if (target == ups_state::in_progress && transaction_uid.empty()) {
        return make_error<std::monostate>(
            -1, "Transaction UID is required for IN PROGRESS transition",
            "storage");
    }

    if (!db() || !db()->is_connected()) {
        return make_error<std::monostate>(-1, "Database not connected",
                                          "storage");
    }

    std::map<std::string, database::core::database_value> update_data{
        {"state", std::string(new_state)},
        {"transaction_uid", std::string(transaction_uid)},
        {"updated_at", std::string("datetime('now')")}};

    auto builder = query_builder();
    builder.update(table_name())
        .set(update_data)
        .where("workitem_uid", "=", std::string(workitem_uid));

    auto result = db()->update(builder.build());
    if (result.is_err()) {
        return make_error<std::monostate>(
            -1,
            kcenon::pacs::compat::format("Failed to change UPS state: {}",
                                 result.error().message),
            "storage");
    }

    return ok();
}

auto ups_repository::find_ups_workitem(std::string_view workitem_uid)
    -> std::optional<ups_workitem> {
    if (!db() || !db()->is_connected()) {
        return std::nullopt;
    }

    auto builder = query_builder();
    auto query = builder.select(select_columns())
                     .from(table_name())
                     .where("workitem_uid", "=", std::string(workitem_uid))
                     .build();

    auto result = db()->select(query);
    if (result.is_err() || result.value().empty()) {
        return std::nullopt;
    }

    return map_row_to_entity(result.value()[0]);
}

auto ups_repository::search_ups_workitems(const ups_workitem_query& query)
    -> Result<std::vector<ups_workitem>> {
    if (!db() || !db()->is_connected()) {
        return make_error<std::vector<ups_workitem>>(-1,
                                                     "Database not connected",
                                                     "storage");
    }

    auto builder = query_builder();
    builder.select(select_columns()).from(table_name());

    if (query.workitem_uid.has_value()) {
        builder.where("workitem_uid", "=", *query.workitem_uid);
    }
    if (query.state.has_value()) {
        builder.where("state", "=", *query.state);
    }
    if (query.priority.has_value()) {
        builder.where("priority", "=", *query.priority);
    }
    if (query.procedure_step_label.has_value()) {
        builder.where("procedure_step_label", "LIKE",
                      to_like_pattern(*query.procedure_step_label));
    }
    if (query.worklist_label.has_value()) {
        builder.where("worklist_label", "LIKE",
                      to_like_pattern(*query.worklist_label));
    }
    if (query.performing_ae.has_value()) {
        builder.where("performing_ae", "=", *query.performing_ae);
    }
    if (query.scheduled_date_from.has_value()) {
        builder.where("scheduled_start_datetime", ">=",
                       *query.scheduled_date_from);
    }
    if (query.scheduled_date_to.has_value()) {
        builder.where("scheduled_start_datetime", "<=",
                       *query.scheduled_date_to + "235959");
    }

    builder.order_by("scheduled_start_datetime", database::sort_order::asc);

    if (query.limit > 0) {
        builder.limit(query.limit);
    }
    if (query.offset > 0) {
        builder.offset(query.offset);
    }

    auto result = db()->select(builder.build());
    if (result.is_err()) {
        return Result<std::vector<ups_workitem>>::err(result.error());
    }

    std::vector<ups_workitem> items;
    items.reserve(result.value().size());
    for (const auto& row : result.value()) {
        items.push_back(map_row_to_entity(row));
    }

    return ok(std::move(items));
}

auto ups_repository::delete_ups_workitem(std::string_view workitem_uid)
    -> VoidResult {
    if (!db() || !db()->is_connected()) {
        return make_error<std::monostate>(-1, "Database not connected",
                                          "storage");
    }

    auto builder = query_builder();
    auto query = builder.delete_from(table_name())
                     .where("workitem_uid", "=", std::string(workitem_uid))
                     .build();

    auto result = db()->remove(query);
    if (result.is_err()) {
        return make_error<std::monostate>(
            -1,
            kcenon::pacs::compat::format("Failed to delete UPS workitem: {}",
                                 result.error().message),
            "storage");
    }

    return ok();
}

auto ups_repository::ups_workitem_count() -> Result<size_t> {
    return count();
}

auto ups_repository::ups_workitem_count(std::string_view state)
    -> Result<size_t> {
    if (!db() || !db()->is_connected()) {
        return make_error<size_t>(-1, "Database not connected", "storage");
    }

    auto query = kcenon::pacs::compat::format(
        "SELECT COUNT(*) as count FROM {} WHERE state = '{}'",
        table_name(), std::string(state));
    auto result = db()->select(query);
    if (result.is_err()) {
        return Result<size_t>::err(result.error());
    }

    if (result.value().empty()) {
        return ok(size_t{0});
    }

    try {
        return ok(static_cast<size_t>(
            std::stoull(result.value()[0].at("count"))));
    } catch (const std::exception& e) {
        return make_error<size_t>(
            -1,
            kcenon::pacs::compat::format("Failed to parse UPS count: {}", e.what()),
            "storage");
    }
}

auto ups_repository::subscribe_ups(const ups_subscription& subscription)
    -> Result<int64_t> {
    if (subscription.subscriber_ae.empty()) {
        return make_error<int64_t>(-1, "Subscriber AE Title is required",
                                   "storage");
    }
    if (!db() || !db()->is_connected()) {
        return make_error<int64_t>(-1, "Database not connected", "storage");
    }

    auto existing = get_ups_subscriptions(subscription.subscriber_ae);
    if (existing.is_err()) {
        return Result<int64_t>::err(existing.error());
    }

    for (const auto& row : existing.value()) {
        if (row.workitem_uid == subscription.workitem_uid) {
            std::map<std::string, database::core::database_value> update_data{
                {"deletion_lock",
                 static_cast<int64_t>(subscription.deletion_lock ? 1 : 0)},
                {"filter_criteria", subscription.filter_criteria}};

            auto builder = query_builder();
            auto query = builder.update("ups_subscriptions")
                             .set(update_data)
                             .where("subscription_pk", "=", row.pk)
                             .build();

            auto update_result = db()->update(query);
            if (update_result.is_err()) {
                return make_error<int64_t>(
                    -1,
                    kcenon::pacs::compat::format("Failed to update subscription: {}",
                                         update_result.error().message),
                    "storage");
            }
            return row.pk;
        }
    }

    std::map<std::string, database::core::database_value> insert_data{
        {"subscriber_ae", subscription.subscriber_ae},
        {"deletion_lock",
         static_cast<int64_t>(subscription.deletion_lock ? 1 : 0)},
        {"filter_criteria", subscription.filter_criteria}};
    if (subscription.workitem_uid.empty()) {
        insert_data["workitem_uid"] = nullptr;
    } else {
        insert_data["workitem_uid"] = subscription.workitem_uid;
    }

    auto builder = query_builder();
    builder.insert_into("ups_subscriptions").values(insert_data);

    auto insert_result = db()->insert(builder.build());
    if (insert_result.is_err()) {
        return make_error<int64_t>(
            -1,
            kcenon::pacs::compat::format("Failed to create subscription: {}",
                                 insert_result.error().message),
            "storage");
    }

    auto inserted = get_ups_subscriptions(subscription.subscriber_ae);
    if (inserted.is_err()) {
        return Result<int64_t>::err(inserted.error());
    }

    for (const auto& row : inserted.value()) {
        if (row.workitem_uid == subscription.workitem_uid) {
            return row.pk;
        }
    }

    return make_error<int64_t>(
        -1, "Subscription inserted but could not retrieve record", "storage");
}

auto ups_repository::unsubscribe_ups(std::string_view subscriber_ae,
                                     std::string_view workitem_uid)
    -> VoidResult {
    if (!db() || !db()->is_connected()) {
        return make_error<std::monostate>(-1, "Database not connected",
                                          "storage");
    }

    auto builder = query_builder();
    builder.delete_from("ups_subscriptions")
        .where("subscriber_ae", "=", std::string(subscriber_ae));

    if (!workitem_uid.empty()) {
        builder.where("workitem_uid", "=", std::string(workitem_uid));
    }

    auto result = db()->remove(builder.build());
    if (result.is_err()) {
        return make_error<std::monostate>(
            -1,
            kcenon::pacs::compat::format("Failed to unsubscribe: {}",
                                 result.error().message),
            "storage");
    }

    return ok();
}

auto ups_repository::get_ups_subscriptions(std::string_view subscriber_ae)
    -> Result<std::vector<ups_subscription>> {
    if (!db() || !db()->is_connected()) {
        return make_error<std::vector<ups_subscription>>(
            -1, "Database not connected", "storage");
    }

    auto builder = query_builder();
    auto query =
        builder.select({"subscription_pk", "subscriber_ae", "workitem_uid",
                        "deletion_lock", "filter_criteria", "created_at"})
            .from("ups_subscriptions")
            .where("subscriber_ae", "=", std::string(subscriber_ae))
            .order_by("subscription_pk", database::sort_order::asc)
            .build();

    auto result = db()->select(query);
    if (result.is_err()) {
        return Result<std::vector<ups_subscription>>::err(result.error());
    }

    std::vector<ups_subscription> items;
    items.reserve(result.value().size());
    for (const auto& row : result.value()) {
        ups_subscription item;
        item.pk = get_int64(row, "subscription_pk");
        item.subscriber_ae = get_string(row, "subscriber_ae");
        item.workitem_uid = get_string(row, "workitem_uid");
        item.deletion_lock = get_bool(row, "deletion_lock");
        item.filter_criteria = get_string(row, "filter_criteria");

        auto created_at = get_string(row, "created_at");
        if (!created_at.empty()) {
            item.created_at = parse_timestamp(created_at);
        }

        items.push_back(std::move(item));
    }

    return ok(std::move(items));
}

auto ups_repository::get_ups_subscribers(std::string_view workitem_uid)
    -> Result<std::vector<std::string>> {
    if (!db() || !db()->is_connected()) {
        return make_error<std::vector<std::string>>(
            -1, "Database not connected", "storage");
    }

    auto workitem_cond =
        database::query_condition("workitem_uid", "=",
                                  std::string(workitem_uid));
    auto global_cond =
        database::query_condition("workitem_uid", "IS NULL", nullptr);

    auto builder = query_builder();
    auto query = builder.select({"subscriber_ae"})
                     .from("ups_subscriptions")
                     .where(workitem_cond || global_cond)
                     .build();

    auto result = db()->select(query);
    if (result.is_err()) {
        return Result<std::vector<std::string>>::err(result.error());
    }

    std::vector<std::string> subscribers;
    subscribers.reserve(result.value().size());
    for (const auto& row : result.value()) {
        subscribers.push_back(get_string(row, "subscriber_ae"));
    }

    std::sort(subscribers.begin(), subscribers.end());
    subscribers.erase(
        std::unique(subscribers.begin(), subscribers.end()),
        subscribers.end());

    return ok(std::move(subscribers));
}

auto ups_repository::map_row_to_entity(const database_row& row) const
    -> ups_workitem {
    ups_workitem item;
    item.pk = get_int64(row, "workitem_pk");
    item.workitem_uid = get_string(row, "workitem_uid");
    item.state = get_string(row, "state");
    item.procedure_step_label = get_string(row, "procedure_step_label");
    item.worklist_label = get_string(row, "worklist_label");
    item.priority = get_string(row, "priority");
    item.scheduled_start_datetime =
        get_string(row, "scheduled_start_datetime");
    item.expected_completion_datetime =
        get_string(row, "expected_completion_datetime");
    item.scheduled_station_name = get_string(row, "scheduled_station_name");
    item.scheduled_station_class = get_string(row, "scheduled_station_class");
    item.scheduled_station_geographic =
        get_string(row, "scheduled_station_geographic");
    item.scheduled_human_performers =
        get_string(row, "scheduled_human_performers");
    item.input_information = get_string(row, "input_information");
    item.performing_ae = get_string(row, "performing_ae");
    item.progress_description = get_string(row, "progress_description");
    item.progress_percent = get_int(row, "progress_percent");
    item.output_information = get_string(row, "output_information");
    item.transaction_uid = get_string(row, "transaction_uid");

    auto created_at = get_string(row, "created_at");
    if (!created_at.empty()) {
        item.created_at = parse_timestamp(created_at);
    }

    auto updated_at = get_string(row, "updated_at");
    if (!updated_at.empty()) {
        item.updated_at = parse_timestamp(updated_at);
    }

    return item;
}

auto ups_repository::entity_to_row(const ups_workitem& entity) const
    -> std::map<std::string, database_value> {
    return {
        {"workitem_uid", entity.workitem_uid},
        {"state", entity.state},
        {"procedure_step_label", entity.procedure_step_label},
        {"worklist_label", entity.worklist_label},
        {"priority", entity.priority},
        {"scheduled_start_datetime", entity.scheduled_start_datetime},
        {"expected_completion_datetime", entity.expected_completion_datetime},
        {"scheduled_station_name", entity.scheduled_station_name},
        {"scheduled_station_class", entity.scheduled_station_class},
        {"scheduled_station_geographic",
         entity.scheduled_station_geographic},
        {"scheduled_human_performers",
         entity.scheduled_human_performers},
        {"input_information", entity.input_information},
        {"performing_ae", entity.performing_ae},
        {"progress_description", entity.progress_description},
        {"progress_percent", static_cast<int64_t>(entity.progress_percent)},
        {"output_information", entity.output_information},
        {"transaction_uid", entity.transaction_uid},
        {"created_at", format_timestamp(entity.created_at)},
        {"updated_at", format_timestamp(entity.updated_at)},
    };
}

auto ups_repository::get_pk(const ups_workitem& entity) const -> int64_t {
    return entity.pk;
}

auto ups_repository::has_pk(const ups_workitem& entity) const -> bool {
    return entity.pk > 0;
}

auto ups_repository::select_columns() const -> std::vector<std::string> {
    return {"workitem_pk",
            "workitem_uid",
            "state",
            "procedure_step_label",
            "worklist_label",
            "priority",
            "scheduled_start_datetime",
            "expected_completion_datetime",
            "scheduled_station_name",
            "scheduled_station_class",
            "scheduled_station_geographic",
            "scheduled_human_performers",
            "input_information",
            "performing_ae",
            "progress_description",
            "progress_percent",
            "output_information",
            "transaction_uid",
            "created_at",
            "updated_at"};
}

}  // namespace kcenon::pacs::storage

#else  // !PACS_WITH_DATABASE_SYSTEM

#include <sqlite3.h>

namespace kcenon::pacs::storage {

using kcenon::common::make_error;
using kcenon::common::ok;

namespace {

auto parse_datetime(const char* str) -> std::chrono::system_clock::time_point {
    if (!str || *str == '\0') {
        return std::chrono::system_clock::time_point{};
    }

    std::tm tm{};
    std::istringstream ss(str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (ss.fail()) {
        return std::chrono::system_clock::time_point{};
    }

    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

auto get_text(sqlite3_stmt* stmt, int col) -> std::string {
    const auto* text =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, col));
    return text ? std::string(text) : std::string{};
}

}  // namespace

ups_repository::ups_repository(sqlite3* db) : db_(db) {}

ups_repository::~ups_repository() = default;

ups_repository::ups_repository(ups_repository&& other) noexcept : db_(other.db_) {
    other.db_ = nullptr;
}

auto ups_repository::operator=(ups_repository&& other) noexcept
    -> ups_repository& {
    if (this != &other) {
        db_ = other.db_;
        other.db_ = nullptr;
    }
    return *this;
}

auto ups_repository::to_like_pattern(std::string_view pattern)
    -> std::string {
    std::string result;
    result.reserve(pattern.size());

    for (char c : pattern) {
        if (c == '*') {
            result += '%';
        } else if (c == '?') {
            result += '_';
        } else if (c == '%' || c == '_') {
            result += '\\';
            result += c;
        } else {
            result += c;
        }
    }

    return result;
}

auto ups_repository::parse_timestamp(const std::string& str)
    -> std::chrono::system_clock::time_point {
    return parse_datetime(str.c_str());
}

auto ups_repository::parse_ups_workitem_row(void* stmt_ptr) const
    -> ups_workitem {
    auto* stmt = static_cast<sqlite3_stmt*>(stmt_ptr);
    ups_workitem item;

    item.pk = sqlite3_column_int64(stmt, 0);
    item.workitem_uid = get_text(stmt, 1);
    item.state = get_text(stmt, 2);
    item.procedure_step_label = get_text(stmt, 3);
    item.worklist_label = get_text(stmt, 4);
    item.priority = get_text(stmt, 5);
    item.scheduled_start_datetime = get_text(stmt, 6);
    item.expected_completion_datetime = get_text(stmt, 7);
    item.scheduled_station_name = get_text(stmt, 8);
    item.scheduled_station_class = get_text(stmt, 9);
    item.scheduled_station_geographic = get_text(stmt, 10);
    item.scheduled_human_performers = get_text(stmt, 11);
    item.input_information = get_text(stmt, 12);
    item.performing_ae = get_text(stmt, 13);
    item.progress_description = get_text(stmt, 14);
    item.progress_percent = sqlite3_column_int(stmt, 15);
    item.output_information = get_text(stmt, 16);
    item.transaction_uid = get_text(stmt, 17);
    item.created_at = parse_datetime(get_text(stmt, 18).c_str());
    item.updated_at = parse_datetime(get_text(stmt, 19).c_str());

    return item;
}

auto ups_repository::create_ups_workitem(const ups_workitem& workitem)
    -> Result<int64_t> {
    if (workitem.workitem_uid.empty()) {
        return make_error<int64_t>(-1, "UPS workitem UID is required",
                                   "storage");
    }

    const char* sql = R"(
        INSERT INTO ups_workitems (
            workitem_uid, state, procedure_step_label, worklist_label,
            priority, scheduled_start_datetime, expected_completion_datetime,
            scheduled_station_name, scheduled_station_class,
            scheduled_station_geographic, scheduled_human_performers,
            input_information, performing_ae, progress_description,
            progress_percent, output_information, transaction_uid,
            updated_at
        ) VALUES (?, 'SCHEDULED', ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, datetime('now'))
        RETURNING workitem_pk;
    )";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<int64_t>(
            rc,
            kcenon::pacs::compat::format("Failed to prepare UPS insert: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    sqlite3_bind_text(stmt, 1, workitem.workitem_uid.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, workitem.procedure_step_label.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, workitem.worklist_label.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(
        stmt, 4,
        workitem.priority.empty() ? "MEDIUM" : workitem.priority.c_str(), -1,
        SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, workitem.scheduled_start_datetime.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6,
                      workitem.expected_completion_datetime.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, workitem.scheduled_station_name.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, workitem.scheduled_station_class.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 9,
                      workitem.scheduled_station_geographic.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 10,
                      workitem.scheduled_human_performers.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 11, workitem.input_information.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 12, workitem.performing_ae.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 13, workitem.progress_description.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 14, workitem.progress_percent);
    sqlite3_bind_text(stmt, 15, workitem.output_information.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 16, workitem.transaction_uid.c_str(), -1,
                      SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        auto error_msg = sqlite3_errmsg(db_);
        sqlite3_finalize(stmt);
        return make_error<int64_t>(
            rc,
            kcenon::pacs::compat::format("Failed to create UPS workitem: {}",
                                 error_msg),
            "storage");
    }

    auto pk = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    return pk;
}

auto ups_repository::update_ups_workitem(const ups_workitem& workitem)
    -> VoidResult {
    if (workitem.workitem_uid.empty()) {
        return make_error<std::monostate>(
            -1, "UPS workitem UID is required for update", "storage");
    }

    auto existing = find_ups_workitem(workitem.workitem_uid);
    if (!existing.has_value()) {
        return make_error<std::monostate>(
            -1,
            kcenon::pacs::compat::format("UPS workitem not found: {}",
                                 workitem.workitem_uid),
            "storage");
    }

    if (existing->is_final()) {
        return make_error<std::monostate>(
            -1,
            kcenon::pacs::compat::format("Cannot update workitem in final state: {}",
                                 existing->state),
            "storage");
    }

    const char* sql = R"(
        UPDATE ups_workitems SET
            procedure_step_label = ?,
            worklist_label = ?,
            priority = ?,
            scheduled_start_datetime = ?,
            expected_completion_datetime = ?,
            scheduled_station_name = ?,
            scheduled_station_class = ?,
            scheduled_station_geographic = ?,
            scheduled_human_performers = ?,
            input_information = ?,
            performing_ae = ?,
            progress_description = ?,
            progress_percent = ?,
            output_information = ?,
            updated_at = datetime('now')
        WHERE workitem_uid = ?;
    )";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<std::monostate>(
            rc,
            kcenon::pacs::compat::format("Failed to prepare UPS update: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    sqlite3_bind_text(stmt, 1, workitem.procedure_step_label.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, workitem.worklist_label.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, workitem.priority.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, workitem.scheduled_start_datetime.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5,
                      workitem.expected_completion_datetime.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, workitem.scheduled_station_name.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, workitem.scheduled_station_class.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8,
                      workitem.scheduled_station_geographic.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 9,
                      workitem.scheduled_human_performers.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 10, workitem.input_information.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 11, workitem.performing_ae.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 12, workitem.progress_description.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 13, workitem.progress_percent);
    sqlite3_bind_text(stmt, 14, workitem.output_information.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 15, workitem.workitem_uid.c_str(), -1,
                      SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return make_error<std::monostate>(
            rc,
            kcenon::pacs::compat::format("Failed to update UPS workitem: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    return ok(std::monostate{});
}

auto ups_repository::change_ups_state(std::string_view workitem_uid,
                                      std::string_view new_state,
                                      std::string_view transaction_uid)
    -> VoidResult {
    if (!parse_ups_state(new_state).has_value()) {
        return make_error<std::monostate>(
            -1, kcenon::pacs::compat::format("Invalid UPS state: {}", new_state),
            "storage");
    }

    auto existing = find_ups_workitem(workitem_uid);
    if (!existing.has_value()) {
        return make_error<std::monostate>(
            -1,
            kcenon::pacs::compat::format("UPS workitem not found: {}", workitem_uid),
            "storage");
    }

    auto current = existing->get_state();
    auto target = parse_ups_state(new_state);

    if (existing->is_final()) {
        return make_error<std::monostate>(
            -1,
            kcenon::pacs::compat::format("Cannot change state from final state: {}",
                                 existing->state),
            "storage");
    }

    if (current == ups_state::scheduled) {
        if (target != ups_state::in_progress &&
            target != ups_state::canceled) {
            return make_error<std::monostate>(
                -1,
                kcenon::pacs::compat::format(
                    "Invalid transition from SCHEDULED to {}", new_state),
                "storage");
        }
    } else if (current == ups_state::in_progress) {
        if (target != ups_state::completed &&
            target != ups_state::canceled) {
            return make_error<std::monostate>(
                -1,
                kcenon::pacs::compat::format(
                    "Invalid transition from IN PROGRESS to {}", new_state),
                "storage");
        }
    }

    if (target == ups_state::in_progress && transaction_uid.empty()) {
        return make_error<std::monostate>(
            -1, "Transaction UID is required for IN PROGRESS transition",
            "storage");
    }

    const char* sql = R"(
        UPDATE ups_workitems SET
            state = ?,
            transaction_uid = ?,
            updated_at = datetime('now')
        WHERE workitem_uid = ?;
    )";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<std::monostate>(
            rc,
            kcenon::pacs::compat::format("Failed to prepare state change: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    sqlite3_bind_text(stmt, 1, new_state.data(), static_cast<int>(new_state.size()),
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, transaction_uid.data(),
                      static_cast<int>(transaction_uid.size()),
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, workitem_uid.data(),
                      static_cast<int>(workitem_uid.size()), SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return make_error<std::monostate>(
            rc,
            kcenon::pacs::compat::format("Failed to change UPS state: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    return ok(std::monostate{});
}

auto ups_repository::find_ups_workitem(std::string_view workitem_uid) const
    -> std::optional<ups_workitem> {
    const char* sql = "SELECT * FROM ups_workitems WHERE workitem_uid = ?;";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, workitem_uid.data(),
                      static_cast<int>(workitem_uid.size()), SQLITE_TRANSIENT);

    std::optional<ups_workitem> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = parse_ups_workitem_row(stmt);
    }

    sqlite3_finalize(stmt);
    return result;
}

auto ups_repository::search_ups_workitems(const ups_workitem_query& query) const
    -> Result<std::vector<ups_workitem>> {
    std::string sql = "SELECT * FROM ups_workitems WHERE 1=1";
    std::vector<std::string> params;

    if (query.workitem_uid.has_value()) {
        sql += " AND workitem_uid = ?";
        params.push_back(query.workitem_uid.value());
    }
    if (query.state.has_value()) {
        sql += " AND state = ?";
        params.push_back(query.state.value());
    }
    if (query.priority.has_value()) {
        sql += " AND priority = ?";
        params.push_back(query.priority.value());
    }
    if (query.procedure_step_label.has_value()) {
        sql += " AND procedure_step_label LIKE ?";
        params.push_back(to_like_pattern(*query.procedure_step_label));
    }
    if (query.worklist_label.has_value()) {
        sql += " AND worklist_label LIKE ?";
        params.push_back(to_like_pattern(*query.worklist_label));
    }
    if (query.performing_ae.has_value()) {
        sql += " AND performing_ae = ?";
        params.push_back(query.performing_ae.value());
    }
    if (query.scheduled_date_from.has_value()) {
        sql += " AND scheduled_start_datetime >= ?";
        params.push_back(query.scheduled_date_from.value());
    }
    if (query.scheduled_date_to.has_value()) {
        sql += " AND scheduled_start_datetime <= ?";
        params.push_back(query.scheduled_date_to.value() + "235959");
    }

    sql += " ORDER BY scheduled_start_datetime ASC";

    if (query.limit > 0) {
        sql += " LIMIT " + std::to_string(query.limit);
    }
    if (query.offset > 0) {
        sql += " OFFSET " + std::to_string(query.offset);
    }

    sql += ";";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<std::vector<ups_workitem>>(
            rc,
            kcenon::pacs::compat::format("Failed to prepare UPS search: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    for (size_t i = 0; i < params.size(); ++i) {
        sqlite3_bind_text(stmt, static_cast<int>(i + 1), params[i].c_str(), -1,
                          SQLITE_TRANSIENT);
    }

    std::vector<ups_workitem> results;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(parse_ups_workitem_row(stmt));
    }

    sqlite3_finalize(stmt);
    return ok(std::move(results));
}

auto ups_repository::delete_ups_workitem(std::string_view workitem_uid)
    -> VoidResult {
    const char* sql = "DELETE FROM ups_workitems WHERE workitem_uid = ?;";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<std::monostate>(
            rc,
            kcenon::pacs::compat::format("Failed to prepare UPS delete: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    sqlite3_bind_text(stmt, 1, workitem_uid.data(),
                      static_cast<int>(workitem_uid.size()), SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return make_error<std::monostate>(
            rc,
            kcenon::pacs::compat::format("Failed to delete UPS workitem: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    return ok(std::monostate{});
}

auto ups_repository::ups_workitem_count() const -> Result<size_t> {
    const char* sql = "SELECT COUNT(*) FROM ups_workitems;";
    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<size_t>(
            rc,
            kcenon::pacs::compat::format("Failed to prepare query: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    size_t count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
    }
    sqlite3_finalize(stmt);
    return ok(count);
}

auto ups_repository::ups_workitem_count(std::string_view state) const
    -> Result<size_t> {
    const char* sql = "SELECT COUNT(*) FROM ups_workitems WHERE state = ?;";
    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<size_t>(
            rc,
            kcenon::pacs::compat::format("Failed to prepare query: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    sqlite3_bind_text(stmt, 1, state.data(), static_cast<int>(state.size()),
                      SQLITE_TRANSIENT);

    size_t count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
    }
    sqlite3_finalize(stmt);
    return ok(count);
}

auto ups_repository::subscribe_ups(const ups_subscription& subscription)
    -> Result<int64_t> {
    if (subscription.subscriber_ae.empty()) {
        return make_error<int64_t>(-1, "Subscriber AE Title is required",
                                   "storage");
    }

    const char* sql = R"(
        INSERT OR REPLACE INTO ups_subscriptions (
            subscriber_ae, workitem_uid, deletion_lock, filter_criteria
        ) VALUES (?, ?, ?, ?)
        RETURNING subscription_pk;
    )";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<int64_t>(
            rc,
            kcenon::pacs::compat::format("Failed to prepare subscription insert: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    sqlite3_bind_text(stmt, 1, subscription.subscriber_ae.c_str(), -1,
                      SQLITE_TRANSIENT);
    if (subscription.workitem_uid.empty()) {
        sqlite3_bind_null(stmt, 2);
    } else {
        sqlite3_bind_text(stmt, 2, subscription.workitem_uid.c_str(), -1,
                          SQLITE_TRANSIENT);
    }
    sqlite3_bind_int(stmt, 3, subscription.deletion_lock ? 1 : 0);
    sqlite3_bind_text(stmt, 4, subscription.filter_criteria.c_str(), -1,
                      SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        auto error_msg = sqlite3_errmsg(db_);
        sqlite3_finalize(stmt);
        return make_error<int64_t>(
            rc, kcenon::pacs::compat::format("Failed to create subscription: {}", error_msg),
            "storage");
    }

    auto pk = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    return pk;
}

auto ups_repository::unsubscribe_ups(std::string_view subscriber_ae,
                                     std::string_view workitem_uid)
    -> VoidResult {
    std::string sql;
    if (workitem_uid.empty()) {
        sql = "DELETE FROM ups_subscriptions WHERE subscriber_ae = ?;";
    } else {
        sql = "DELETE FROM ups_subscriptions WHERE subscriber_ae = ? AND workitem_uid = ?;";
    }

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<std::monostate>(
            rc,
            kcenon::pacs::compat::format("Failed to prepare unsubscribe: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    sqlite3_bind_text(stmt, 1, subscriber_ae.data(),
                      static_cast<int>(subscriber_ae.size()), SQLITE_TRANSIENT);
    if (!workitem_uid.empty()) {
        sqlite3_bind_text(stmt, 2, workitem_uid.data(),
                          static_cast<int>(workitem_uid.size()),
                          SQLITE_TRANSIENT);
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return make_error<std::monostate>(
            rc,
            kcenon::pacs::compat::format("Failed to unsubscribe: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    return ok(std::monostate{});
}

auto ups_repository::get_ups_subscriptions(std::string_view subscriber_ae) const
    -> Result<std::vector<ups_subscription>> {
    const char* sql =
        "SELECT * FROM ups_subscriptions WHERE subscriber_ae = ?;";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<std::vector<ups_subscription>>(
            rc,
            kcenon::pacs::compat::format("Failed to prepare subscription query: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    sqlite3_bind_text(stmt, 1, subscriber_ae.data(),
                      static_cast<int>(subscriber_ae.size()), SQLITE_TRANSIENT);

    std::vector<ups_subscription> results;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ups_subscription sub;
        sub.pk = sqlite3_column_int64(stmt, 0);
        sub.subscriber_ae = get_text(stmt, 1);
        sub.workitem_uid = get_text(stmt, 2);
        sub.deletion_lock = sqlite3_column_int(stmt, 3) != 0;
        sub.filter_criteria = get_text(stmt, 4);
        sub.created_at = parse_datetime(get_text(stmt, 5).c_str());
        results.push_back(std::move(sub));
    }

    sqlite3_finalize(stmt);
    return ok(std::move(results));
}

auto ups_repository::get_ups_subscribers(std::string_view workitem_uid) const
    -> Result<std::vector<std::string>> {
    const char* sql = R"(
        SELECT DISTINCT subscriber_ae FROM ups_subscriptions
        WHERE workitem_uid = ? OR workitem_uid IS NULL;
    )";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<std::vector<std::string>>(
            rc,
            kcenon::pacs::compat::format("Failed to prepare subscriber query: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    sqlite3_bind_text(stmt, 1, workitem_uid.data(),
                      static_cast<int>(workitem_uid.size()), SQLITE_TRANSIENT);

    std::vector<std::string> results;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(get_text(stmt, 0));
    }

    sqlite3_finalize(stmt);
    return ok(std::move(results));
}

}  // namespace kcenon::pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
