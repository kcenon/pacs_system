// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file worklist_repository.cpp
 * @brief Implementation of the worklist repository
 *
 * @see Issue #914 - Extract worklist lifecycle repository
 */

#include "kcenon/pacs/storage/worklist_repository.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#include <kcenon/pacs/compat/format.h>
#include <kcenon/pacs/compat/time.h>

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

}  // namespace

worklist_repository::worklist_repository(std::shared_ptr<pacs_database_adapter> db)
    : base_repository(std::move(db), "worklist", "worklist_pk") {}

auto worklist_repository::to_like_pattern(std::string_view pattern)
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

auto worklist_repository::parse_timestamp(const std::string& str) const
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

auto worklist_repository::format_timestamp(
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

auto worklist_repository::add_worklist_item(const worklist_item& item)
    -> Result<int64_t> {
    if (item.step_id.empty()) {
        return make_error<int64_t>(-1, "Step ID is required", "storage");
    }
    if (item.patient_id.empty()) {
        return make_error<int64_t>(-1, "Patient ID is required", "storage");
    }
    if (item.modality.empty()) {
        return make_error<int64_t>(-1, "Modality is required", "storage");
    }
    if (item.scheduled_datetime.empty()) {
        return make_error<int64_t>(-1, "Scheduled datetime is required",
                                   "storage");
    }
    if (!db() || !db()->is_connected()) {
        return make_error<int64_t>(-1, "Database not connected", "storage");
    }

    auto builder = query_builder();
    builder.insert_into(table_name())
        .values({{"step_id", item.step_id},
                 {"step_status", std::string("SCHEDULED")},
                 {"patient_id", item.patient_id},
                 {"patient_name", item.patient_name},
                 {"birth_date", item.birth_date},
                 {"sex", item.sex},
                 {"accession_no", item.accession_no},
                 {"requested_proc_id", item.requested_proc_id},
                 {"study_uid", item.study_uid},
                 {"scheduled_datetime", item.scheduled_datetime},
                 {"station_ae", item.station_ae},
                 {"station_name", item.station_name},
                 {"modality", item.modality},
                 {"procedure_desc", item.procedure_desc},
                 {"protocol_code", item.protocol_code},
                 {"referring_phys", item.referring_phys},
                 {"referring_phys_id", item.referring_phys_id}});

    auto insert_result = db()->insert(builder.build());
    if (insert_result.is_err()) {
        return make_error<int64_t>(
            -1,
            kcenon::pacs::compat::format("Failed to add worklist item: {}",
                                 insert_result.error().message),
            "storage");
    }

    auto inserted = find_worklist_item(item.step_id, item.accession_no);
    if (!inserted.has_value()) {
        return make_error<int64_t>(
            -1, "Worklist item inserted but could not retrieve record",
            "storage");
    }

    return inserted->pk;
}

auto worklist_repository::update_worklist_status(std::string_view step_id,
                                                 std::string_view accession_no,
                                                 std::string_view new_status)
    -> VoidResult {
    if (new_status != "SCHEDULED" && new_status != "STARTED" &&
        new_status != "COMPLETED") {
        return make_error<std::monostate>(
            -1,
            "Invalid status. Must be 'SCHEDULED', 'STARTED', or 'COMPLETED'",
            "storage");
    }
    if (!db() || !db()->is_connected()) {
        return make_error<std::monostate>(-1, "Database not connected",
                                          "storage");
    }

    std::map<std::string, database::core::database_value> update_data{
        {"step_status", std::string(new_status)},
        {"updated_at", std::string("datetime('now')")}};

    auto builder = query_builder();
    builder.update(table_name())
        .set(update_data)
        .where("step_id", "=", std::string(step_id))
        .where("accession_no", "=", std::string(accession_no));

    auto result = db()->update(builder.build());
    if (result.is_err()) {
        return make_error<std::monostate>(
            -1,
            kcenon::pacs::compat::format("Failed to update worklist status: {}",
                                 result.error().message),
            "storage");
    }

    return ok();
}

auto worklist_repository::query_worklist(const worklist_query& query)
    -> Result<std::vector<worklist_item>> {
    if (!db() || !db()->is_connected()) {
        return make_error<std::vector<worklist_item>>(-1,
                                                      "Database not connected",
                                                      "storage");
    }

    auto builder = query_builder();
    builder.select(select_columns()).from(table_name());

    if (!query.include_all_status) {
        builder.where("step_status", "=", std::string("SCHEDULED"));
    }
    if (query.station_ae.has_value()) {
        builder.where("station_ae", "=", *query.station_ae);
    }
    if (query.modality.has_value()) {
        builder.where("modality", "=", *query.modality);
    }
    if (query.scheduled_date_from.has_value()) {
        builder.where(database::query_condition(
            "substr(scheduled_datetime, 1, 8)", ">=",
            *query.scheduled_date_from));
    }
    if (query.scheduled_date_to.has_value()) {
        builder.where(database::query_condition(
            "substr(scheduled_datetime, 1, 8)", "<=",
            *query.scheduled_date_to));
    }
    if (query.patient_id.has_value()) {
        builder.where("patient_id", "LIKE",
                      to_like_pattern(*query.patient_id));
    }
    if (query.patient_name.has_value()) {
        builder.where("patient_name", "LIKE",
                      to_like_pattern(*query.patient_name));
    }
    if (query.accession_no.has_value()) {
        builder.where("accession_no", "=", *query.accession_no);
    }
    if (query.step_id.has_value()) {
        builder.where("step_id", "=", *query.step_id);
    }

    builder.order_by("scheduled_datetime", database::sort_order::asc);

    if (query.limit > 0) {
        builder.limit(query.limit);
    }
    if (query.offset > 0) {
        builder.offset(query.offset);
    }

    auto result = db()->select(builder.build());
    if (result.is_err()) {
        return Result<std::vector<worklist_item>>::err(result.error());
    }

    std::vector<worklist_item> items;
    items.reserve(result.value().size());
    for (const auto& row : result.value()) {
        items.push_back(map_row_to_entity(row));
    }

    return ok(std::move(items));
}

auto worklist_repository::find_worklist_item(std::string_view step_id,
                                             std::string_view accession_no)
    -> std::optional<worklist_item> {
    if (!db() || !db()->is_connected()) {
        return std::nullopt;
    }

    auto builder = query_builder();
    auto query = builder.select(select_columns())
                     .from(table_name())
                     .where("step_id", "=", std::string(step_id))
                     .where("accession_no", "=", std::string(accession_no))
                     .build();

    auto result = db()->select(query);
    if (result.is_err() || result.value().empty()) {
        return std::nullopt;
    }

    return map_row_to_entity(result.value()[0]);
}

auto worklist_repository::find_worklist_by_pk(int64_t pk)
    -> std::optional<worklist_item> {
    auto result = find_by_id(pk);
    if (result.is_err()) {
        return std::nullopt;
    }
    return result.value();
}

auto worklist_repository::delete_worklist_item(std::string_view step_id,
                                               std::string_view accession_no)
    -> VoidResult {
    if (!db() || !db()->is_connected()) {
        return make_error<std::monostate>(-1, "Database not connected",
                                          "storage");
    }

    auto builder = query_builder();
    auto query = builder.delete_from(table_name())
                     .where("step_id", "=", std::string(step_id))
                     .where("accession_no", "=", std::string(accession_no))
                     .build();

    auto result = db()->remove(query);
    if (result.is_err()) {
        return make_error<std::monostate>(
            -1,
            kcenon::pacs::compat::format("Failed to delete worklist item: {}",
                                 result.error().message),
            "storage");
    }

    return ok();
}

auto worklist_repository::cleanup_old_worklist_items(std::chrono::hours age)
    -> Result<size_t> {
    auto cutoff = std::chrono::system_clock::now() - age;
    auto cutoff_time = std::chrono::system_clock::to_time_t(cutoff);
    std::tm tm{};
    kcenon::pacs::compat::gmtime_safe(&cutoff_time, &tm);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    auto cutoff_str = oss.str();

    if (!db() || !db()->is_connected()) {
        return make_error<size_t>(-1, "Database not connected", "storage");
    }

    auto builder = query_builder();
    builder.delete_from(table_name())
        .where("step_status", "!=", std::string("SCHEDULED"))
        .where(database::query_condition("created_at", "<", cutoff_str));

    auto result = db()->remove(builder.build());
    if (result.is_err()) {
        return make_error<size_t>(
            -1,
            kcenon::pacs::compat::format("Failed to cleanup old worklist items: {}",
                                 result.error().message),
            "storage");
    }

    return ok(static_cast<size_t>(result.value()));
}

auto worklist_repository::cleanup_worklist_items_before(
    std::chrono::system_clock::time_point before) -> Result<size_t> {
    auto before_time = std::chrono::system_clock::to_time_t(before);
    std::tm tm{};
    kcenon::pacs::compat::localtime_safe(&before_time, &tm);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    auto before_str = oss.str();

    if (!db() || !db()->is_connected()) {
        return make_error<size_t>(-1, "Database not connected", "storage");
    }

    auto builder = query_builder();
    builder.delete_from(table_name())
        .where("step_status", "!=", std::string("SCHEDULED"))
        .where(database::query_condition("scheduled_datetime", "<", before_str));

    auto result = db()->remove(builder.build());
    if (result.is_err()) {
        return make_error<size_t>(
            -1,
            kcenon::pacs::compat::format(
                "Failed to cleanup worklist items before {}: {}",
                before_str, result.error().message),
            "storage");
    }

    return ok(static_cast<size_t>(result.value()));
}

auto worklist_repository::worklist_count() -> Result<size_t> {
    return count();
}

auto worklist_repository::worklist_count(std::string_view status)
    -> Result<size_t> {
    if (!db() || !db()->is_connected()) {
        return make_error<size_t>(-1, "Database not connected", "storage");
    }

    auto query = kcenon::pacs::compat::format(
        "SELECT COUNT(*) as count FROM {} WHERE step_status = '{}'",
        table_name(), std::string(status));
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
            kcenon::pacs::compat::format("Failed to parse worklist count: {}",
                                 e.what()),
            "storage");
    }
}

auto worklist_repository::map_row_to_entity(const database_row& row) const
    -> worklist_item {
    worklist_item item;
    item.pk = get_int64(row, "worklist_pk");
    item.step_id = get_string(row, "step_id");
    item.step_status = get_string(row, "step_status");
    item.patient_id = get_string(row, "patient_id");
    item.patient_name = get_string(row, "patient_name");
    item.birth_date = get_string(row, "birth_date");
    item.sex = get_string(row, "sex");
    item.accession_no = get_string(row, "accession_no");
    item.requested_proc_id = get_string(row, "requested_proc_id");
    item.study_uid = get_string(row, "study_uid");
    item.scheduled_datetime = get_string(row, "scheduled_datetime");
    item.station_ae = get_string(row, "station_ae");
    item.station_name = get_string(row, "station_name");
    item.modality = get_string(row, "modality");
    item.procedure_desc = get_string(row, "procedure_desc");
    item.protocol_code = get_string(row, "protocol_code");
    item.referring_phys = get_string(row, "referring_phys");
    item.referring_phys_id = get_string(row, "referring_phys_id");

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

auto worklist_repository::entity_to_row(const worklist_item& entity) const
    -> std::map<std::string, database_value> {
    return {
        {"step_id", entity.step_id},
        {"step_status", entity.step_status},
        {"patient_id", entity.patient_id},
        {"patient_name", entity.patient_name},
        {"birth_date", entity.birth_date},
        {"sex", entity.sex},
        {"accession_no", entity.accession_no},
        {"requested_proc_id", entity.requested_proc_id},
        {"study_uid", entity.study_uid},
        {"scheduled_datetime", entity.scheduled_datetime},
        {"station_ae", entity.station_ae},
        {"station_name", entity.station_name},
        {"modality", entity.modality},
        {"procedure_desc", entity.procedure_desc},
        {"protocol_code", entity.protocol_code},
        {"referring_phys", entity.referring_phys},
        {"referring_phys_id", entity.referring_phys_id},
        {"created_at", format_timestamp(entity.created_at)},
        {"updated_at", format_timestamp(entity.updated_at)},
    };
}

auto worklist_repository::get_pk(const worklist_item& entity) const -> int64_t {
    return entity.pk;
}

auto worklist_repository::has_pk(const worklist_item& entity) const -> bool {
    return entity.pk > 0;
}

auto worklist_repository::select_columns() const -> std::vector<std::string> {
    return {"worklist_pk",       "step_id",          "step_status",
            "patient_id",        "patient_name",     "birth_date",
            "sex",               "accession_no",     "requested_proc_id",
            "study_uid",         "scheduled_datetime","station_ae",
            "station_name",      "modality",         "procedure_desc",
            "protocol_code",     "referring_phys",   "referring_phys_id",
            "created_at",        "updated_at"};
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

worklist_repository::worklist_repository(sqlite3* db) : db_(db) {}

worklist_repository::~worklist_repository() = default;

worklist_repository::worklist_repository(worklist_repository&& other) noexcept
    : db_(other.db_) {
    other.db_ = nullptr;
}

auto worklist_repository::operator=(worklist_repository&& other) noexcept
    -> worklist_repository& {
    if (this != &other) {
        db_ = other.db_;
        other.db_ = nullptr;
    }
    return *this;
}

auto worklist_repository::to_like_pattern(std::string_view pattern)
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

auto worklist_repository::parse_timestamp(const std::string& str)
    -> std::chrono::system_clock::time_point {
    return parse_datetime(str.c_str());
}

auto worklist_repository::parse_worklist_row(void* stmt_ptr) const
    -> worklist_item {
    auto* stmt = static_cast<sqlite3_stmt*>(stmt_ptr);
    worklist_item item;

    item.pk = sqlite3_column_int64(stmt, 0);
    item.step_id = get_text(stmt, 1);
    item.step_status = get_text(stmt, 2);
    item.patient_id = get_text(stmt, 3);
    item.patient_name = get_text(stmt, 4);
    item.birth_date = get_text(stmt, 5);
    item.sex = get_text(stmt, 6);
    item.accession_no = get_text(stmt, 7);
    item.requested_proc_id = get_text(stmt, 8);
    item.study_uid = get_text(stmt, 9);
    item.scheduled_datetime = get_text(stmt, 10);
    item.station_ae = get_text(stmt, 11);
    item.station_name = get_text(stmt, 12);
    item.modality = get_text(stmt, 13);
    item.procedure_desc = get_text(stmt, 14);
    item.protocol_code = get_text(stmt, 15);
    item.referring_phys = get_text(stmt, 16);
    item.referring_phys_id = get_text(stmt, 17);
    item.created_at = parse_datetime(get_text(stmt, 18).c_str());
    item.updated_at = parse_datetime(get_text(stmt, 19).c_str());

    return item;
}

auto worklist_repository::add_worklist_item(const worklist_item& item)
    -> Result<int64_t> {
    if (item.step_id.empty()) {
        return make_error<int64_t>(-1, "Step ID is required", "storage");
    }
    if (item.patient_id.empty()) {
        return make_error<int64_t>(-1, "Patient ID is required", "storage");
    }
    if (item.modality.empty()) {
        return make_error<int64_t>(-1, "Modality is required", "storage");
    }
    if (item.scheduled_datetime.empty()) {
        return make_error<int64_t>(-1, "Scheduled datetime is required",
                                   "storage");
    }

    const char* sql = R"(
        INSERT INTO worklist (
            step_id, step_status, patient_id, patient_name, birth_date, sex,
            accession_no, requested_proc_id, study_uid, scheduled_datetime,
            station_ae, station_name, modality, procedure_desc, protocol_code,
            referring_phys, referring_phys_id, updated_at
        ) VALUES (?, 'SCHEDULED', ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?,
                  datetime('now'))
        RETURNING worklist_pk;
    )";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<int64_t>(
            rc,
            kcenon::pacs::compat::format("Failed to prepare statement: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    sqlite3_bind_text(stmt, 1, item.step_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, item.patient_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, item.patient_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, item.birth_date.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, item.sex.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, item.accession_no.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, item.requested_proc_id.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, item.study_uid.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 9, item.scheduled_datetime.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 10, item.station_ae.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 11, item.station_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 12, item.modality.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 13, item.procedure_desc.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 14, item.protocol_code.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 15, item.referring_phys.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 16, item.referring_phys_id.c_str(), -1,
                      SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        auto error_msg = sqlite3_errmsg(db_);
        sqlite3_finalize(stmt);
        return make_error<int64_t>(
            rc, kcenon::pacs::compat::format("Failed to add worklist item: {}", error_msg),
            "storage");
    }

    auto pk = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    return pk;
}

auto worklist_repository::update_worklist_status(std::string_view step_id,
                                                 std::string_view accession_no,
                                                 std::string_view new_status)
    -> VoidResult {
    if (new_status != "SCHEDULED" && new_status != "STARTED" &&
        new_status != "COMPLETED") {
        return make_error<std::monostate>(
            -1,
            "Invalid status. Must be 'SCHEDULED', 'STARTED', or 'COMPLETED'",
            "storage");
    }

    const char* sql = R"(
        UPDATE worklist
        SET step_status = ?,
            updated_at = datetime('now')
        WHERE step_id = ? AND accession_no = ?;
    )";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<std::monostate>(
            rc,
            kcenon::pacs::compat::format("Failed to prepare statement: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    sqlite3_bind_text(stmt, 1, new_status.data(),
                      static_cast<int>(new_status.size()), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, step_id.data(), static_cast<int>(step_id.size()),
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, accession_no.data(),
                      static_cast<int>(accession_no.size()), SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return make_error<std::monostate>(
            rc,
            kcenon::pacs::compat::format("Failed to update worklist status: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    return ok();
}

auto worklist_repository::query_worklist(const worklist_query& query) const
    -> Result<std::vector<worklist_item>> {
    std::vector<worklist_item> results;
    std::string sql = R"(
        SELECT worklist_pk, step_id, step_status, patient_id, patient_name,
               birth_date, sex, accession_no, requested_proc_id, study_uid,
               scheduled_datetime, station_ae, station_name, modality,
               procedure_desc, protocol_code, referring_phys, referring_phys_id,
               created_at, updated_at
        FROM worklist
        WHERE 1=1
    )";
    std::vector<std::string> params;

    if (!query.include_all_status) {
        sql += " AND step_status = 'SCHEDULED'";
    }
    if (query.station_ae.has_value()) {
        sql += " AND station_ae = ?";
        params.push_back(*query.station_ae);
    }
    if (query.modality.has_value()) {
        sql += " AND modality = ?";
        params.push_back(*query.modality);
    }
    if (query.scheduled_date_from.has_value()) {
        sql += " AND substr(scheduled_datetime, 1, 8) >= ?";
        params.push_back(*query.scheduled_date_from);
    }
    if (query.scheduled_date_to.has_value()) {
        sql += " AND substr(scheduled_datetime, 1, 8) <= ?";
        params.push_back(*query.scheduled_date_to);
    }
    if (query.patient_id.has_value()) {
        sql += " AND patient_id LIKE ?";
        params.push_back(to_like_pattern(*query.patient_id));
    }
    if (query.patient_name.has_value()) {
        sql += " AND patient_name LIKE ?";
        params.push_back(to_like_pattern(*query.patient_name));
    }
    if (query.accession_no.has_value()) {
        sql += " AND accession_no = ?";
        params.push_back(*query.accession_no);
    }
    if (query.step_id.has_value()) {
        sql += " AND step_id = ?";
        params.push_back(*query.step_id);
    }

    sql += " ORDER BY scheduled_datetime ASC";

    if (query.limit > 0) {
        sql += kcenon::pacs::compat::format(" LIMIT {}", query.limit);
    }
    if (query.offset > 0) {
        sql += kcenon::pacs::compat::format(" OFFSET {}", query.offset);
    }

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<std::vector<worklist_item>>(
            rc,
            kcenon::pacs::compat::format("Failed to prepare query: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    for (size_t i = 0; i < params.size(); ++i) {
        sqlite3_bind_text(stmt, static_cast<int>(i + 1), params[i].c_str(), -1,
                          SQLITE_TRANSIENT);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(parse_worklist_row(stmt));
    }

    sqlite3_finalize(stmt);
    return ok(std::move(results));
}

auto worklist_repository::find_worklist_item(std::string_view step_id,
                                             std::string_view accession_no) const
    -> std::optional<worklist_item> {
    const char* sql = R"(
        SELECT worklist_pk, step_id, step_status, patient_id, patient_name,
               birth_date, sex, accession_no, requested_proc_id, study_uid,
               scheduled_datetime, station_ae, station_name, modality,
               procedure_desc, protocol_code, referring_phys, referring_phys_id,
               created_at, updated_at
        FROM worklist
        WHERE step_id = ? AND accession_no = ?;
    )";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, step_id.data(), static_cast<int>(step_id.size()),
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, accession_no.data(),
                      static_cast<int>(accession_no.size()), SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }

    auto record = parse_worklist_row(stmt);
    sqlite3_finalize(stmt);
    return record;
}

auto worklist_repository::find_worklist_by_pk(int64_t pk) const
    -> std::optional<worklist_item> {
    const char* sql = R"(
        SELECT worklist_pk, step_id, step_status, patient_id, patient_name,
               birth_date, sex, accession_no, requested_proc_id, study_uid,
               scheduled_datetime, station_ae, station_name, modality,
               procedure_desc, protocol_code, referring_phys, referring_phys_id,
               created_at, updated_at
        FROM worklist
        WHERE worklist_pk = ?;
    )";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_int64(stmt, 1, pk);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }

    auto record = parse_worklist_row(stmt);
    sqlite3_finalize(stmt);
    return record;
}

auto worklist_repository::delete_worklist_item(std::string_view step_id,
                                               std::string_view accession_no)
    -> VoidResult {
    const char* sql =
        "DELETE FROM worklist WHERE step_id = ? AND accession_no = ?;";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<std::monostate>(
            rc,
            kcenon::pacs::compat::format("Failed to prepare delete: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    sqlite3_bind_text(stmt, 1, step_id.data(), static_cast<int>(step_id.size()),
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, accession_no.data(),
                      static_cast<int>(accession_no.size()), SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return make_error<std::monostate>(
            rc,
            kcenon::pacs::compat::format("Failed to delete worklist item: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    return ok();
}

auto worklist_repository::cleanup_old_worklist_items(std::chrono::hours age)
    -> Result<size_t> {
    auto cutoff = std::chrono::system_clock::now() - age;
    auto cutoff_time = std::chrono::system_clock::to_time_t(cutoff);
    std::tm tm{};
    kcenon::pacs::compat::gmtime_safe(&cutoff_time, &tm);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    auto cutoff_str = oss.str();

    const char* sql = R"(
        DELETE FROM worklist
        WHERE step_status != 'SCHEDULED'
          AND created_at < ?;
    )";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<size_t>(
            rc,
            kcenon::pacs::compat::format("Failed to prepare cleanup: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    sqlite3_bind_text(stmt, 1, cutoff_str.c_str(), -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return make_error<size_t>(
            rc,
            kcenon::pacs::compat::format("Failed to cleanup old worklist items: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    return static_cast<size_t>(sqlite3_changes(db_));
}

auto worklist_repository::cleanup_worklist_items_before(
    std::chrono::system_clock::time_point before) -> Result<size_t> {
    auto before_time = std::chrono::system_clock::to_time_t(before);
    std::tm tm{};
    kcenon::pacs::compat::localtime_safe(&before_time, &tm);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    auto before_str = oss.str();

    const char* sql = R"(
        DELETE FROM worklist
        WHERE step_status != 'SCHEDULED'
          AND scheduled_datetime < ?;
    )";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<size_t>(
            rc,
            kcenon::pacs::compat::format("Failed to prepare cleanup: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    sqlite3_bind_text(stmt, 1, before_str.c_str(), -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return make_error<size_t>(
            rc,
            kcenon::pacs::compat::format(
                "Failed to cleanup worklist items before {}: {}",
                before_str, sqlite3_errmsg(db_)),
            "storage");
    }

    return static_cast<size_t>(sqlite3_changes(db_));
}

auto worklist_repository::worklist_count() const -> Result<size_t> {
    const char* sql = "SELECT COUNT(*) FROM worklist;";
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

auto worklist_repository::worklist_count(std::string_view status) const
    -> Result<size_t> {
    const char* sql = "SELECT COUNT(*) FROM worklist WHERE step_status = ?;";
    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<size_t>(
            rc,
            kcenon::pacs::compat::format("Failed to prepare query: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    sqlite3_bind_text(stmt, 1, status.data(), static_cast<int>(status.size()),
                      SQLITE_TRANSIENT);

    size_t count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
    }
    sqlite3_finalize(stmt);
    return ok(count);
}

}  // namespace kcenon::pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
