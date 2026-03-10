// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @file mpps_repository.cpp
 * @brief Implementation of the MPPS repository
 *
 * @see Issue #914 - Extract MPPS lifecycle repository
 */

#include "pacs/storage/mpps_repository.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#include <pacs/compat/format.hpp>

#ifdef PACS_WITH_DATABASE_SYSTEM

#include <database/query_builder.h>

namespace pacs::storage {

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

mpps_repository::mpps_repository(std::shared_ptr<pacs_database_adapter> db)
    : base_repository(std::move(db), "mpps", "mpps_pk") {}

auto mpps_repository::parse_timestamp(const std::string& str) const
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

auto mpps_repository::format_timestamp(
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

auto mpps_repository::create_mpps(std::string_view mpps_uid,
                                  std::string_view station_ae,
                                  std::string_view modality,
                                  std::string_view study_uid,
                                  std::string_view accession_no,
                                  std::string_view start_datetime)
    -> Result<int64_t> {
    mpps_record record;
    record.mpps_uid = std::string(mpps_uid);
    record.station_ae = std::string(station_ae);
    record.modality = std::string(modality);
    record.study_uid = std::string(study_uid);
    record.accession_no = std::string(accession_no);
    record.start_datetime = std::string(start_datetime);
    record.status = "IN PROGRESS";
    return create_mpps(record);
}

auto mpps_repository::create_mpps(const mpps_record& record) -> Result<int64_t> {
    if (record.mpps_uid.empty()) {
        return make_error<int64_t>(-1, "MPPS SOP Instance UID is required",
                                   "storage");
    }

    if (!record.status.empty() && record.status != "IN PROGRESS") {
        return make_error<int64_t>(
            -1, "N-CREATE must create MPPS with status 'IN PROGRESS'",
            "storage");
    }

    if (!db() || !db()->is_connected()) {
        return make_error<int64_t>(-1, "Database not connected", "storage");
    }

    auto builder = query_builder();
    builder.insert_into("mpps")
        .values({{"mpps_uid", record.mpps_uid},
                 {"status", std::string("IN PROGRESS")},
                 {"start_datetime", record.start_datetime},
                 {"end_datetime", record.end_datetime},
                 {"station_ae", record.station_ae},
                 {"station_name", record.station_name},
                 {"modality", record.modality},
                 {"study_uid", record.study_uid},
                 {"accession_no", record.accession_no},
                 {"scheduled_step_id", record.scheduled_step_id},
                 {"requested_proc_id", record.requested_proc_id},
                 {"performed_series", record.performed_series}});

    auto insert_result = db()->insert(builder.build());
    if (insert_result.is_err()) {
        return make_error<int64_t>(
            -1,
            pacs::compat::format("Failed to create MPPS: {}",
                                 insert_result.error().message),
            "storage");
    }

    auto inserted = find_mpps(record.mpps_uid);
    if (!inserted.has_value()) {
        return make_error<int64_t>(
            -1, "MPPS inserted but could not retrieve record", "storage");
    }

    return inserted->pk;
}

auto mpps_repository::update_mpps(std::string_view mpps_uid,
                                  std::string_view new_status,
                                  std::string_view end_datetime,
                                  std::string_view performed_series)
    -> VoidResult {
    if (new_status != "COMPLETED" && new_status != "DISCONTINUED" &&
        new_status != "IN PROGRESS") {
        return make_error<std::monostate>(
            -1,
            "Invalid status. Must be 'IN PROGRESS', 'COMPLETED', or "
            "'DISCONTINUED'",
            "storage");
    }

    auto current = find_mpps(mpps_uid);
    if (!current.has_value()) {
        return make_error<std::monostate>(-1, "MPPS not found", "storage");
    }

    if (current->status == "COMPLETED" || current->status == "DISCONTINUED") {
        return make_error<std::monostate>(
            -1,
            pacs::compat::format(
                "Cannot update MPPS in final state '{}'. COMPLETED and "
                "DISCONTINUED are final states.",
                current->status),
            "storage");
    }

    if (!db() || !db()->is_connected()) {
        return make_error<std::monostate>(-1, "Database not connected",
                                          "storage");
    }

    auto builder = query_builder();
    builder.update("mpps").set("status", std::string(new_status));

    if (!end_datetime.empty()) {
        builder.set("end_datetime", std::string(end_datetime));
    }

    if (!performed_series.empty()) {
        builder.set("performed_series", std::string(performed_series));
    }

    builder.set("updated_at", std::string("datetime('now')"))
        .where("mpps_uid", "=", std::string(mpps_uid));

    auto result = db()->update(builder.build());
    if (result.is_err()) {
        return make_error<std::monostate>(
            -1,
            pacs::compat::format("Failed to update MPPS: {}",
                                 result.error().message),
            "storage");
    }

    return ok();
}

auto mpps_repository::update_mpps(const mpps_record& record) -> VoidResult {
    if (record.mpps_uid.empty()) {
        return make_error<std::monostate>(-1, "MPPS UID is required for update",
                                          "storage");
    }

    return update_mpps(record.mpps_uid, record.status, record.end_datetime,
                       record.performed_series);
}

auto mpps_repository::find_mpps(std::string_view mpps_uid)
    -> std::optional<mpps_record> {
    if (!db() || !db()->is_connected()) {
        return std::nullopt;
    }

    auto builder = query_builder();
    auto query = builder.select(select_columns())
                     .from(table_name())
                     .where("mpps_uid", "=", std::string(mpps_uid))
                     .build();

    auto result = db()->select(query);
    if (result.is_err() || result.value().empty()) {
        return std::nullopt;
    }

    return map_row_to_entity(result.value()[0]);
}

auto mpps_repository::find_mpps_by_pk(int64_t pk) -> std::optional<mpps_record> {
    auto result = find_by_id(pk);
    if (result.is_err()) {
        return std::nullopt;
    }
    return result.value();
}

auto mpps_repository::list_active_mpps(std::string_view station_ae)
    -> Result<std::vector<mpps_record>> {
    if (!db() || !db()->is_connected()) {
        return make_error<std::vector<mpps_record>>(-1,
                                                    "Database not connected",
                                                    "storage");
    }

    auto builder = query_builder();
    auto query = builder.select(select_columns())
                     .from(table_name())
                     .where("status", "=", std::string("IN PROGRESS"))
                     .where("station_ae", "=", std::string(station_ae))
                     .order_by("start_datetime", database::sort_order::desc)
                     .build();

    auto result = db()->select(query);
    if (result.is_err()) {
        return Result<std::vector<mpps_record>>::err(result.error());
    }

    std::vector<mpps_record> items;
    items.reserve(result.value().size());
    for (const auto& row : result.value()) {
        items.push_back(map_row_to_entity(row));
    }

    return ok(std::move(items));
}

auto mpps_repository::find_mpps_by_study(std::string_view study_uid)
    -> Result<std::vector<mpps_record>> {
    if (!db() || !db()->is_connected()) {
        return make_error<std::vector<mpps_record>>(-1,
                                                    "Database not connected",
                                                    "storage");
    }

    auto builder = query_builder();
    auto query = builder.select(select_columns())
                     .from(table_name())
                     .where("study_uid", "=", std::string(study_uid))
                     .order_by("start_datetime", database::sort_order::desc)
                     .build();

    auto result = db()->select(query);
    if (result.is_err()) {
        return Result<std::vector<mpps_record>>::err(result.error());
    }

    std::vector<mpps_record> items;
    items.reserve(result.value().size());
    for (const auto& row : result.value()) {
        items.push_back(map_row_to_entity(row));
    }

    return ok(std::move(items));
}

auto mpps_repository::search_mpps(const mpps_query& query)
    -> Result<std::vector<mpps_record>> {
    if (!db() || !db()->is_connected()) {
        return make_error<std::vector<mpps_record>>(-1,
                                                    "Database not connected",
                                                    "storage");
    }

    auto builder = query_builder();
    builder.select(select_columns()).from(table_name());

    if (query.mpps_uid.has_value()) {
        builder.where("mpps_uid", "=", *query.mpps_uid);
    }
    if (query.status.has_value()) {
        builder.where("status", "=", *query.status);
    }
    if (query.station_ae.has_value()) {
        builder.where("station_ae", "=", *query.station_ae);
    }
    if (query.modality.has_value()) {
        builder.where("modality", "=", *query.modality);
    }
    if (query.study_uid.has_value()) {
        builder.where("study_uid", "=", *query.study_uid);
    }
    if (query.accession_no.has_value()) {
        builder.where("accession_no", "=", *query.accession_no);
    }
    if (query.start_date_from.has_value()) {
        builder.where(database::query_condition(pacs::compat::format(
            "substr(start_datetime, 1, 8) >= '{}'", *query.start_date_from)));
    }
    if (query.start_date_to.has_value()) {
        builder.where(database::query_condition(pacs::compat::format(
            "substr(start_datetime, 1, 8) <= '{}'", *query.start_date_to)));
    }

    builder.order_by("start_datetime", database::sort_order::desc);

    if (query.limit > 0) {
        builder.limit(query.limit);
    }
    if (query.offset > 0) {
        builder.offset(query.offset);
    }

    auto result = db()->select(builder.build());
    if (result.is_err()) {
        return Result<std::vector<mpps_record>>::err(result.error());
    }

    std::vector<mpps_record> items;
    items.reserve(result.value().size());
    for (const auto& row : result.value()) {
        items.push_back(map_row_to_entity(row));
    }

    return ok(std::move(items));
}

auto mpps_repository::delete_mpps(std::string_view mpps_uid) -> VoidResult {
    if (!db() || !db()->is_connected()) {
        return make_error<std::monostate>(-1, "Database not connected",
                                          "storage");
    }

    auto builder = query_builder();
    auto query = builder.delete_from(table_name())
                     .where("mpps_uid", "=", std::string(mpps_uid))
                     .build();

    auto result = db()->remove(query);
    if (result.is_err()) {
        return make_error<std::monostate>(
            -1,
            pacs::compat::format("Failed to delete MPPS: {}",
                                 result.error().message),
            "storage");
    }

    return ok();
}

auto mpps_repository::mpps_count() -> Result<size_t> {
    return count();
}

auto mpps_repository::mpps_count(std::string_view status) -> Result<size_t> {
    if (!db() || !db()->is_connected()) {
        return make_error<size_t>(-1, "Database not connected", "storage");
    }

    auto builder = query_builder();
    auto query = builder.select(std::vector<std::string>{"COUNT(*) as count"})
                     .from(table_name())
                     .where("status", "=", std::string(status))
                     .build();

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
            pacs::compat::format("Failed to parse MPPS count: {}", e.what()),
            "storage");
    }
}

auto mpps_repository::map_row_to_entity(const database_row& row) const
    -> mpps_record {
    mpps_record record;
    record.pk = get_int64(row, "mpps_pk");
    record.mpps_uid = get_string(row, "mpps_uid");
    record.status = get_string(row, "status");
    record.start_datetime = get_string(row, "start_datetime");
    record.end_datetime = get_string(row, "end_datetime");
    record.station_ae = get_string(row, "station_ae");
    record.station_name = get_string(row, "station_name");
    record.modality = get_string(row, "modality");
    record.study_uid = get_string(row, "study_uid");
    record.accession_no = get_string(row, "accession_no");
    record.scheduled_step_id = get_string(row, "scheduled_step_id");
    record.requested_proc_id = get_string(row, "requested_proc_id");
    record.performed_series = get_string(row, "performed_series");

    auto created_at = get_string(row, "created_at");
    if (!created_at.empty()) {
        record.created_at = parse_timestamp(created_at);
    }

    auto updated_at = get_string(row, "updated_at");
    if (!updated_at.empty()) {
        record.updated_at = parse_timestamp(updated_at);
    }

    return record;
}

auto mpps_repository::entity_to_row(const mpps_record& entity) const
    -> std::map<std::string, database_value> {
    return {
        {"mpps_uid", entity.mpps_uid},
        {"status", entity.status},
        {"start_datetime", entity.start_datetime},
        {"end_datetime", entity.end_datetime},
        {"station_ae", entity.station_ae},
        {"station_name", entity.station_name},
        {"modality", entity.modality},
        {"study_uid", entity.study_uid},
        {"accession_no", entity.accession_no},
        {"scheduled_step_id", entity.scheduled_step_id},
        {"requested_proc_id", entity.requested_proc_id},
        {"performed_series", entity.performed_series},
        {"created_at", format_timestamp(entity.created_at)},
        {"updated_at", format_timestamp(entity.updated_at)},
    };
}

auto mpps_repository::get_pk(const mpps_record& entity) const -> int64_t {
    return entity.pk;
}

auto mpps_repository::has_pk(const mpps_record& entity) const -> bool {
    return entity.pk > 0;
}

auto mpps_repository::select_columns() const -> std::vector<std::string> {
    return {"mpps_pk",        "mpps_uid",         "status",
            "start_datetime", "end_datetime",     "station_ae",
            "station_name",   "modality",         "study_uid",
            "accession_no",   "scheduled_step_id","requested_proc_id",
            "performed_series","created_at",      "updated_at"};
}

}  // namespace pacs::storage

#else  // !PACS_WITH_DATABASE_SYSTEM

#include <sqlite3.h>

namespace pacs::storage {

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

mpps_repository::mpps_repository(sqlite3* db) : db_(db) {}

mpps_repository::~mpps_repository() = default;

mpps_repository::mpps_repository(mpps_repository&& other) noexcept
    : db_(other.db_) {
    other.db_ = nullptr;
}

auto mpps_repository::operator=(mpps_repository&& other) noexcept
    -> mpps_repository& {
    if (this != &other) {
        db_ = other.db_;
        other.db_ = nullptr;
    }
    return *this;
}

auto mpps_repository::parse_timestamp(const std::string& str)
    -> std::chrono::system_clock::time_point {
    return parse_datetime(str.c_str());
}

auto mpps_repository::parse_mpps_row(void* stmt_ptr) const -> mpps_record {
    auto* stmt = static_cast<sqlite3_stmt*>(stmt_ptr);
    mpps_record record;

    record.pk = sqlite3_column_int64(stmt, 0);
    record.mpps_uid = get_text(stmt, 1);
    record.status = get_text(stmt, 2);
    record.start_datetime = get_text(stmt, 3);
    record.end_datetime = get_text(stmt, 4);
    record.station_ae = get_text(stmt, 5);
    record.station_name = get_text(stmt, 6);
    record.modality = get_text(stmt, 7);
    record.study_uid = get_text(stmt, 8);
    record.accession_no = get_text(stmt, 9);
    record.scheduled_step_id = get_text(stmt, 10);
    record.requested_proc_id = get_text(stmt, 11);
    record.performed_series = get_text(stmt, 12);
    record.created_at = parse_datetime(get_text(stmt, 13).c_str());
    record.updated_at = parse_datetime(get_text(stmt, 14).c_str());

    return record;
}

auto mpps_repository::create_mpps(std::string_view mpps_uid,
                                  std::string_view station_ae,
                                  std::string_view modality,
                                  std::string_view study_uid,
                                  std::string_view accession_no,
                                  std::string_view start_datetime)
    -> Result<int64_t> {
    mpps_record record;
    record.mpps_uid = std::string(mpps_uid);
    record.station_ae = std::string(station_ae);
    record.modality = std::string(modality);
    record.study_uid = std::string(study_uid);
    record.accession_no = std::string(accession_no);
    record.start_datetime = std::string(start_datetime);
    record.status = "IN PROGRESS";
    return create_mpps(record);
}

auto mpps_repository::create_mpps(const mpps_record& record) -> Result<int64_t> {
    if (record.mpps_uid.empty()) {
        return make_error<int64_t>(-1, "MPPS SOP Instance UID is required",
                                   "storage");
    }

    if (!record.status.empty() && record.status != "IN PROGRESS") {
        return make_error<int64_t>(
            -1, "N-CREATE must create MPPS with status 'IN PROGRESS'",
            "storage");
    }

    const char* sql = R"(
        INSERT INTO mpps (
            mpps_uid, status, start_datetime, station_ae, station_name,
            modality, study_uid, accession_no, scheduled_step_id,
            requested_proc_id, performed_series, updated_at
        ) VALUES (?, 'IN PROGRESS', ?, ?, ?, ?, ?, ?, ?, ?, ?, datetime('now'))
        RETURNING mpps_pk;
    )";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<int64_t>(
            rc,
            pacs::compat::format("Failed to prepare statement: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    sqlite3_bind_text(stmt, 1, record.mpps_uid.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, record.start_datetime.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, record.station_ae.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, record.station_name.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, record.modality.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, record.study_uid.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, record.accession_no.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, record.scheduled_step_id.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 9, record.requested_proc_id.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 10, record.performed_series.c_str(), -1,
                      SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        auto error_msg = sqlite3_errmsg(db_);
        sqlite3_finalize(stmt);
        return make_error<int64_t>(
            rc, pacs::compat::format("Failed to create MPPS: {}", error_msg),
            "storage");
    }

    auto pk = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    return pk;
}

auto mpps_repository::update_mpps(std::string_view mpps_uid,
                                  std::string_view new_status,
                                  std::string_view end_datetime,
                                  std::string_view performed_series)
    -> VoidResult {
    if (new_status != "COMPLETED" && new_status != "DISCONTINUED" &&
        new_status != "IN PROGRESS") {
        return make_error<std::monostate>(
            -1,
            "Invalid status. Must be 'IN PROGRESS', 'COMPLETED', or "
            "'DISCONTINUED'",
            "storage");
    }

    auto current = find_mpps(mpps_uid);
    if (!current.has_value()) {
        return make_error<std::monostate>(-1, "MPPS not found", "storage");
    }

    if (current->status == "COMPLETED" || current->status == "DISCONTINUED") {
        return make_error<std::monostate>(
            -1,
            pacs::compat::format(
                "Cannot update MPPS in final state '{}'. COMPLETED and "
                "DISCONTINUED are final states.",
                current->status),
            "storage");
    }

    const char* sql = R"(
        UPDATE mpps
        SET status = ?,
            end_datetime = CASE WHEN ? != '' THEN ? ELSE end_datetime END,
            performed_series = CASE WHEN ? != '' THEN ? ELSE performed_series END,
            updated_at = datetime('now')
        WHERE mpps_uid = ?;
    )";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<std::monostate>(
            rc,
            pacs::compat::format("Failed to prepare statement: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    sqlite3_bind_text(stmt, 1, new_status.data(),
                      static_cast<int>(new_status.size()), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, end_datetime.data(),
                      static_cast<int>(end_datetime.size()), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, end_datetime.data(),
                      static_cast<int>(end_datetime.size()), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, performed_series.data(),
                      static_cast<int>(performed_series.size()),
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, performed_series.data(),
                      static_cast<int>(performed_series.size()),
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, mpps_uid.data(),
                      static_cast<int>(mpps_uid.size()), SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return make_error<std::monostate>(
            rc,
            pacs::compat::format("Failed to update MPPS: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    return ok();
}

auto mpps_repository::update_mpps(const mpps_record& record) -> VoidResult {
    if (record.mpps_uid.empty()) {
        return make_error<std::monostate>(-1, "MPPS UID is required for update",
                                          "storage");
    }

    return update_mpps(record.mpps_uid, record.status, record.end_datetime,
                       record.performed_series);
}

auto mpps_repository::find_mpps(std::string_view mpps_uid) const
    -> std::optional<mpps_record> {
    const char* sql = R"(
        SELECT mpps_pk, mpps_uid, status, start_datetime, end_datetime,
               station_ae, station_name, modality, study_uid, accession_no,
               scheduled_step_id, requested_proc_id, performed_series,
               created_at, updated_at
        FROM mpps
        WHERE mpps_uid = ?;
    )";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, mpps_uid.data(),
                      static_cast<int>(mpps_uid.size()), SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }

    auto record = parse_mpps_row(stmt);
    sqlite3_finalize(stmt);
    return record;
}

auto mpps_repository::find_mpps_by_pk(int64_t pk) const
    -> std::optional<mpps_record> {
    const char* sql = R"(
        SELECT mpps_pk, mpps_uid, status, start_datetime, end_datetime,
               station_ae, station_name, modality, study_uid, accession_no,
               scheduled_step_id, requested_proc_id, performed_series,
               created_at, updated_at
        FROM mpps
        WHERE mpps_pk = ?;
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

    auto record = parse_mpps_row(stmt);
    sqlite3_finalize(stmt);
    return record;
}

auto mpps_repository::list_active_mpps(std::string_view station_ae) const
    -> Result<std::vector<mpps_record>> {
    std::vector<mpps_record> results;

    const char* sql = R"(
        SELECT mpps_pk, mpps_uid, status, start_datetime, end_datetime,
               station_ae, station_name, modality, study_uid, accession_no,
               scheduled_step_id, requested_proc_id, performed_series,
               created_at, updated_at
        FROM mpps
        WHERE status = 'IN PROGRESS' AND station_ae = ?
        ORDER BY start_datetime DESC;
    )";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<std::vector<mpps_record>>(
            rc,
            pacs::compat::format("Failed to prepare query: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    sqlite3_bind_text(stmt, 1, station_ae.data(),
                      static_cast<int>(station_ae.size()), SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(parse_mpps_row(stmt));
    }

    sqlite3_finalize(stmt);
    return ok(std::move(results));
}

auto mpps_repository::find_mpps_by_study(std::string_view study_uid) const
    -> Result<std::vector<mpps_record>> {
    std::vector<mpps_record> results;

    const char* sql = R"(
        SELECT mpps_pk, mpps_uid, status, start_datetime, end_datetime,
               station_ae, station_name, modality, study_uid, accession_no,
               scheduled_step_id, requested_proc_id, performed_series,
               created_at, updated_at
        FROM mpps
        WHERE study_uid = ?
        ORDER BY start_datetime DESC;
    )";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<std::vector<mpps_record>>(
            rc,
            pacs::compat::format("Failed to prepare query: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    sqlite3_bind_text(stmt, 1, study_uid.data(),
                      static_cast<int>(study_uid.size()), SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(parse_mpps_row(stmt));
    }

    sqlite3_finalize(stmt);
    return ok(std::move(results));
}

auto mpps_repository::search_mpps(const mpps_query& query) const
    -> Result<std::vector<mpps_record>> {
    std::vector<mpps_record> results;
    std::string sql = R"(
        SELECT mpps_pk, mpps_uid, status, start_datetime, end_datetime,
               station_ae, station_name, modality, study_uid, accession_no,
               scheduled_step_id, requested_proc_id, performed_series,
               created_at, updated_at
        FROM mpps
        WHERE 1=1
    )";
    std::vector<std::string> params;

    if (query.mpps_uid.has_value()) {
        sql += " AND mpps_uid = ?";
        params.push_back(*query.mpps_uid);
    }
    if (query.status.has_value()) {
        sql += " AND status = ?";
        params.push_back(*query.status);
    }
    if (query.station_ae.has_value()) {
        sql += " AND station_ae = ?";
        params.push_back(*query.station_ae);
    }
    if (query.modality.has_value()) {
        sql += " AND modality = ?";
        params.push_back(*query.modality);
    }
    if (query.study_uid.has_value()) {
        sql += " AND study_uid = ?";
        params.push_back(*query.study_uid);
    }
    if (query.accession_no.has_value()) {
        sql += " AND accession_no = ?";
        params.push_back(*query.accession_no);
    }
    if (query.start_date_from.has_value()) {
        sql += " AND substr(start_datetime, 1, 8) >= ?";
        params.push_back(*query.start_date_from);
    }
    if (query.start_date_to.has_value()) {
        sql += " AND substr(start_datetime, 1, 8) <= ?";
        params.push_back(*query.start_date_to);
    }

    sql += " ORDER BY start_datetime DESC";

    if (query.limit > 0) {
        sql += pacs::compat::format(" LIMIT {}", query.limit);
    }
    if (query.offset > 0) {
        sql += pacs::compat::format(" OFFSET {}", query.offset);
    }

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<std::vector<mpps_record>>(
            rc,
            pacs::compat::format("Failed to prepare query: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    for (size_t i = 0; i < params.size(); ++i) {
        sqlite3_bind_text(stmt, static_cast<int>(i + 1), params[i].c_str(), -1,
                          SQLITE_TRANSIENT);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(parse_mpps_row(stmt));
    }

    sqlite3_finalize(stmt);
    return ok(std::move(results));
}

auto mpps_repository::delete_mpps(std::string_view mpps_uid) -> VoidResult {
    const char* sql = "DELETE FROM mpps WHERE mpps_uid = ?;";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<std::monostate>(
            rc,
            pacs::compat::format("Failed to prepare delete: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    sqlite3_bind_text(stmt, 1, mpps_uid.data(),
                      static_cast<int>(mpps_uid.size()), SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return make_error<std::monostate>(
            rc,
            pacs::compat::format("Failed to delete MPPS: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    return ok();
}

auto mpps_repository::mpps_count() const -> Result<size_t> {
    const char* sql = "SELECT COUNT(*) FROM mpps;";
    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<size_t>(
            rc,
            pacs::compat::format("Failed to prepare query: {}",
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

auto mpps_repository::mpps_count(std::string_view status) const
    -> Result<size_t> {
    const char* sql = "SELECT COUNT(*) FROM mpps WHERE status = ?;";
    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<size_t>(
            rc,
            pacs::compat::format("Failed to prepare query: {}",
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

}  // namespace pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
