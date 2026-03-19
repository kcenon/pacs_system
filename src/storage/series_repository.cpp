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
 * @file series_repository.cpp
 * @brief Implementation of the series repository
 *
 * @see Issue #896 - Decompose index_database into metadata and lifecycle stores
 * @see Issue #913 - Part 2: Extract series and instance metadata repositories
 */

#include "pacs/storage/series_repository.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#ifdef PACS_WITH_DATABASE_SYSTEM

#include <database/query_builder.h>
#include <pacs/compat/format.hpp>

namespace kcenon::pacs::storage {

using kcenon::common::make_error;
using kcenon::common::ok;

series_repository::series_repository(std::shared_ptr<pacs_database_adapter> db)
    : base_repository(std::move(db), "series", "series_pk") {}

auto series_repository::parse_timestamp(const std::string& str) const
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

auto series_repository::format_timestamp(
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

auto series_repository::to_like_pattern(std::string_view pattern)
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

auto series_repository::upsert_series(int64_t study_pk,
                                      std::string_view series_uid,
                                      std::string_view modality,
                                      std::optional<int> series_number,
                                      std::string_view series_description,
                                      std::string_view body_part_examined,
                                      std::string_view station_name)
    -> Result<int64_t> {
    series_record record;
    record.study_pk = study_pk;
    record.series_uid = std::string(series_uid);
    record.modality = std::string(modality);
    record.series_number = series_number;
    record.series_description = std::string(series_description);
    record.body_part_examined = std::string(body_part_examined);
    record.station_name = std::string(station_name);
    return upsert_series(record);
}

auto series_repository::upsert_series(const series_record& record)
    -> Result<int64_t> {
    if (record.series_uid.empty()) {
        return make_error<int64_t>(-1, "Series Instance UID is required",
                                   "storage");
    }

    if (record.series_uid.length() > 64) {
        return make_error<int64_t>(
            -1, "Series Instance UID exceeds maximum length of 64 characters",
            "storage");
    }

    if (record.study_pk <= 0) {
        return make_error<int64_t>(-1, "Valid study_pk is required", "storage");
    }

    if (!db() || !db()->is_connected()) {
        return make_error<int64_t>(-1, "Database not connected", "storage");
    }

    auto builder = query_builder();
    auto check_sql = builder.select(std::vector<std::string>{"series_pk"})
                         .from("series")
                         .where("series_uid", "=", record.series_uid)
                         .build();

    auto check_result = db()->select(check_sql);
    if (check_result.is_err()) {
        return make_error<int64_t>(
            -1,
            kcenon::pacs::compat::format("Failed to check series existence: {}",
                                 check_result.error().message),
            "storage");
    }

    if (!check_result.value().empty()) {
        auto existing = find_series(record.series_uid);
        if (!existing.has_value()) {
            return make_error<int64_t>(
                -1, "Series exists but could not retrieve record", "storage");
        }

        std::map<std::string, database::core::database_value> update_data{
            {"study_pk", std::to_string(record.study_pk)},
            {"modality", record.modality},
            {"series_description", record.series_description},
            {"body_part_examined", record.body_part_examined},
            {"station_name", record.station_name},
            {"updated_at", std::string("datetime('now')")}};
        if (record.series_number.has_value()) {
            update_data["series_number"] =
                std::to_string(*record.series_number);
        }

        database::query_builder update_builder(database::database_types::sqlite);
        auto update_sql = update_builder.update("series")
                              .set(update_data)
                              .where("series_uid", "=", record.series_uid)
                              .build();
        auto update_result = db()->update(update_sql);
        if (update_result.is_err()) {
            return make_error<int64_t>(
                -1,
                kcenon::pacs::compat::format("Failed to update series: {}",
                                     update_result.error().message),
                "storage");
        }

        return existing->pk;
    }

    std::map<std::string, database::core::database_value> insert_data{
        {"study_pk", std::to_string(record.study_pk)},
        {"series_uid", record.series_uid},
        {"modality", record.modality},
        {"series_description", record.series_description},
        {"body_part_examined", record.body_part_examined},
        {"station_name", record.station_name}};
    if (record.series_number.has_value()) {
        insert_data["series_number"] = std::to_string(*record.series_number);
    }

    database::query_builder insert_builder(database::database_types::sqlite);
    insert_builder.insert_into("series").values(insert_data);

    auto insert_result = db()->insert(insert_builder.build());
    if (insert_result.is_err()) {
        return make_error<int64_t>(
            -1,
            kcenon::pacs::compat::format("Failed to insert series: {}",
                                 insert_result.error().message),
            "storage");
    }

    auto inserted = find_series(record.series_uid);
    if (!inserted.has_value()) {
        return make_error<int64_t>(
            -1, "Series inserted but could not retrieve record", "storage");
    }

    return inserted->pk;
}

auto series_repository::find_series(std::string_view series_uid)
    -> std::optional<series_record> {
    if (!db() || !db()->is_connected()) {
        return std::nullopt;
    }

    auto builder = query_builder();
    auto select_sql =
        builder.select(select_columns())
            .from(table_name())
            .where("series_uid", "=", std::string(series_uid))
            .build();

    auto result = db()->select(select_sql);
    if (result.is_err() || result.value().empty()) {
        return std::nullopt;
    }

    return map_row_to_entity(result.value()[0]);
}

auto series_repository::find_series_by_pk(int64_t pk)
    -> std::optional<series_record> {
    auto result = find_by_id(pk);
    if (result.is_err()) {
        return std::nullopt;
    }
    return result.value();
}

auto series_repository::list_series(std::string_view study_uid)
    -> Result<std::vector<series_record>> {
    if (!db() || !db()->is_connected()) {
        return make_error<std::vector<series_record>>(
            -1, "Database not connected", "storage");
    }

    auto sql = kcenon::pacs::compat::format(
        "SELECT se.series_pk, se.study_pk, se.series_uid, se.modality, "
        "se.series_number, se.series_description, se.body_part_examined, "
        "se.station_name, se.num_instances, se.created_at, se.updated_at "
        "FROM series se "
        "JOIN studies st ON se.study_pk = st.study_pk "
        "WHERE st.study_uid = '{}' "
        "ORDER BY se.series_number ASC, se.series_uid ASC;",
        std::string(study_uid));

    auto result = db()->select(sql);
    if (result.is_err()) {
        return make_error<std::vector<series_record>>(
            -1,
            kcenon::pacs::compat::format("Failed to list series: {}",
                                 result.error().message),
            "storage");
    }

    std::vector<series_record> records;
    records.reserve(result.value().size());
    for (const auto& row : result.value()) {
        records.push_back(map_row_to_entity(row));
    }

    return ok(std::move(records));
}

auto series_repository::search_series(const series_query& query)
    -> Result<std::vector<series_record>> {
    if (!db() || !db()->is_connected()) {
        return make_error<std::vector<series_record>>(
            -1, "Database not connected", "storage");
    }

    std::vector<std::string> where_clauses;

    if (query.study_uid.has_value()) {
        where_clauses.push_back(
            kcenon::pacs::compat::format("st.study_uid = '{}'", *query.study_uid));
    }
    if (query.series_uid.has_value()) {
        where_clauses.push_back(
            kcenon::pacs::compat::format("se.series_uid = '{}'", *query.series_uid));
    }
    if (query.modality.has_value()) {
        where_clauses.push_back(
            kcenon::pacs::compat::format("se.modality = '{}'", *query.modality));
    }
    if (query.series_description.has_value()) {
        where_clauses.push_back(kcenon::pacs::compat::format(
            "se.series_description LIKE '{}'",
            to_like_pattern(*query.series_description)));
    }
    if (query.body_part_examined.has_value()) {
        where_clauses.push_back(kcenon::pacs::compat::format(
            "se.body_part_examined = '{}'", *query.body_part_examined));
    }
    if (query.series_number.has_value()) {
        where_clauses.push_back(kcenon::pacs::compat::format(
            "se.series_number = {}", *query.series_number));
    }

    std::string sql =
        "SELECT se.series_pk, se.study_pk, se.series_uid, se.modality, "
        "se.series_number, se.series_description, se.body_part_examined, "
        "se.station_name, se.num_instances, se.created_at, se.updated_at "
        "FROM series se "
        "JOIN studies st ON se.study_pk = st.study_pk";

    if (!where_clauses.empty()) {
        sql += " WHERE " + where_clauses.front();
        for (size_t i = 1; i < where_clauses.size(); ++i) {
            sql += " AND " + where_clauses[i];
        }
    }

    sql += " ORDER BY se.series_number ASC, se.series_uid ASC";

    if (query.limit > 0) {
        sql += kcenon::pacs::compat::format(" LIMIT {}", query.limit);
    }
    if (query.offset > 0) {
        sql += kcenon::pacs::compat::format(" OFFSET {}", query.offset);
    }

    auto result = db()->select(sql);
    if (result.is_err()) {
        return make_error<std::vector<series_record>>(
            -1,
            kcenon::pacs::compat::format("Failed to search series: {}",
                                 result.error().message),
            "storage");
    }

    std::vector<series_record> records;
    records.reserve(result.value().size());
    for (const auto& row : result.value()) {
        records.push_back(map_row_to_entity(row));
    }

    return ok(std::move(records));
}

auto series_repository::delete_series(std::string_view series_uid)
    -> VoidResult {
    if (!db() || !db()->is_connected()) {
        return make_error<std::monostate>(-1, "Database not connected",
                                          "storage");
    }

    auto builder = query_builder();
    auto delete_sql =
        builder.delete_from(table_name())
            .where("series_uid", "=", std::string(series_uid))
            .build();

    auto result = db()->remove(delete_sql);
    if (result.is_err()) {
        return make_error<std::monostate>(
            -1,
            kcenon::pacs::compat::format("Failed to delete series: {}",
                                 result.error().message),
            "storage");
    }

    return ok();
}

auto series_repository::series_count() -> Result<size_t> {
    return count();
}

auto series_repository::series_count(std::string_view study_uid)
    -> Result<size_t> {
    if (!db() || !db()->is_connected()) {
        return make_error<size_t>(-1, "Database not connected", "storage");
    }

    auto sql = kcenon::pacs::compat::format(
        "SELECT COUNT(*) AS cnt "
        "FROM series se "
        "JOIN studies st ON se.study_pk = st.study_pk "
        "WHERE st.study_uid = '{}';",
        std::string(study_uid));

    auto result = db()->select(sql);
    if (result.is_err()) {
        return make_error<size_t>(
            -1,
            kcenon::pacs::compat::format("Failed to count series: {}",
                                 result.error().message),
            "storage");
    }

    if (result.value().empty()) {
        return ok(static_cast<size_t>(0));
    }

    const auto& row = result.value()[0];
    auto it = row.find("cnt");
    if (it == row.end() && !row.empty()) {
        it = row.begin();
    }
    if (it == row.end() || it->second.empty()) {
        return ok(static_cast<size_t>(0));
    }

    try {
        return ok(static_cast<size_t>(std::stoull(it->second)));
    } catch (...) {
        return ok(static_cast<size_t>(0));
    }
}

auto series_repository::map_row_to_entity(const database_row& row) const
    -> series_record {
    series_record record;

    auto get_str = [&row](const std::string& key) -> std::string {
        auto it = row.find(key);
        return (it != row.end()) ? it->second : std::string{};
    };

    auto get_int64 = [&row](const std::string& key) -> int64_t {
        auto it = row.find(key);
        if (it != row.end() && !it->second.empty()) {
            try {
                return std::stoll(it->second);
            } catch (...) {
                return 0;
            }
        }
        return 0;
    };

    auto get_optional_int = [&row](const std::string& key) -> std::optional<int> {
        auto it = row.find(key);
        if (it != row.end() && !it->second.empty()) {
            try {
                return std::stoi(it->second);
            } catch (...) {
                return std::nullopt;
            }
        }
        return std::nullopt;
    };

    record.pk = get_int64("series_pk");
    record.study_pk = get_int64("study_pk");
    record.series_uid = get_str("series_uid");
    record.modality = get_str("modality");
    record.series_number = get_optional_int("series_number");
    record.series_description = get_str("series_description");
    record.body_part_examined = get_str("body_part_examined");
    record.station_name = get_str("station_name");
    record.num_instances = get_optional_int("num_instances").value_or(0);

    auto created_at_str = get_str("created_at");
    if (!created_at_str.empty()) {
        record.created_at = parse_timestamp(created_at_str);
    }

    auto updated_at_str = get_str("updated_at");
    if (!updated_at_str.empty()) {
        record.updated_at = parse_timestamp(updated_at_str);
    }

    return record;
}

auto series_repository::entity_to_row(const series_record& entity) const
    -> std::map<std::string, database_value> {
    std::map<std::string, database_value> row;

    row["study_pk"] = std::to_string(entity.study_pk);
    row["series_uid"] = entity.series_uid;
    row["modality"] = entity.modality;
    if (entity.series_number.has_value()) {
        row["series_number"] = std::to_string(*entity.series_number);
    }
    row["series_description"] = entity.series_description;
    row["body_part_examined"] = entity.body_part_examined;
    row["station_name"] = entity.station_name;
    row["num_instances"] = std::to_string(entity.num_instances);

    auto now = std::chrono::system_clock::now();
    if (entity.created_at != std::chrono::system_clock::time_point{}) {
        row["created_at"] = format_timestamp(entity.created_at);
    } else {
        row["created_at"] = format_timestamp(now);
    }

    if (entity.updated_at != std::chrono::system_clock::time_point{}) {
        row["updated_at"] = format_timestamp(entity.updated_at);
    } else {
        row["updated_at"] = format_timestamp(now);
    }

    return row;
}

auto series_repository::get_pk(const series_record& entity) const -> int64_t {
    return entity.pk;
}

auto series_repository::has_pk(const series_record& entity) const -> bool {
    return entity.pk > 0;
}

auto series_repository::select_columns() const -> std::vector<std::string> {
    return {"series_pk",          "study_pk",          "series_uid",
            "modality",           "series_number",     "series_description",
            "body_part_examined", "station_name",      "num_instances",
            "created_at",         "updated_at"};
}

}  // namespace kcenon::pacs::storage

#else  // !PACS_WITH_DATABASE_SYSTEM

#include <pacs/compat/format.hpp>
#include <pacs/core/result.hpp>
#include <sqlite3.h>

namespace kcenon::pacs::storage {

using kcenon::common::make_error;
using kcenon::common::ok;
using namespace kcenon::pacs::error_codes;

namespace {

auto parse_datetime(const char* str)
    -> std::chrono::system_clock::time_point {
    if (!str || *str == '\0') {
        return std::chrono::system_clock::now();
    }

    std::tm tm{};
    std::istringstream ss(str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");

    if (ss.fail()) {
        return std::chrono::system_clock::now();
    }

    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

auto get_text(sqlite3_stmt* stmt, int col) -> std::string {
    const auto* text =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, col));
    return text ? std::string(text) : std::string{};
}

}  // namespace

series_repository::series_repository(sqlite3* db) : db_(db) {}

series_repository::~series_repository() = default;

series_repository::series_repository(series_repository&&) noexcept = default;

auto series_repository::operator=(series_repository&&) noexcept
    -> series_repository& = default;

auto series_repository::to_like_pattern(std::string_view pattern)
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

auto series_repository::parse_timestamp(const std::string& str)
    -> std::chrono::system_clock::time_point {
    return parse_datetime(str.c_str());
}

auto series_repository::parse_series_row(void* stmt_ptr) const -> series_record {
    auto* stmt = static_cast<sqlite3_stmt*>(stmt_ptr);
    series_record record;

    record.pk = sqlite3_column_int64(stmt, 0);
    record.study_pk = sqlite3_column_int64(stmt, 1);
    record.series_uid = get_text(stmt, 2);
    record.modality = get_text(stmt, 3);

    if (sqlite3_column_type(stmt, 4) != SQLITE_NULL) {
        record.series_number = sqlite3_column_int(stmt, 4);
    }

    record.series_description = get_text(stmt, 5);
    record.body_part_examined = get_text(stmt, 6);
    record.station_name = get_text(stmt, 7);
    record.num_instances = sqlite3_column_int(stmt, 8);
    record.created_at = parse_datetime(get_text(stmt, 9).c_str());
    record.updated_at = parse_datetime(get_text(stmt, 10).c_str());

    return record;
}

auto series_repository::upsert_series(int64_t study_pk,
                                      std::string_view series_uid,
                                      std::string_view modality,
                                      std::optional<int> series_number,
                                      std::string_view series_description,
                                      std::string_view body_part_examined,
                                      std::string_view station_name)
    -> Result<int64_t> {
    series_record record;
    record.study_pk = study_pk;
    record.series_uid = std::string(series_uid);
    record.modality = std::string(modality);
    record.series_number = series_number;
    record.series_description = std::string(series_description);
    record.body_part_examined = std::string(body_part_examined);
    record.station_name = std::string(station_name);
    return upsert_series(record);
}

auto series_repository::upsert_series(const series_record& record)
    -> Result<int64_t> {
    if (record.series_uid.empty()) {
        return make_error<int64_t>(-1, "Series Instance UID is required",
                                   "storage");
    }

    if (record.series_uid.length() > 64) {
        return make_error<int64_t>(
            -1, "Series Instance UID exceeds maximum length of 64 characters",
            "storage");
    }

    if (record.study_pk <= 0) {
        return make_error<int64_t>(-1, "Valid study_pk is required", "storage");
    }

    const char* sql = R"(
        INSERT INTO series (
            study_pk, series_uid, modality, series_number,
            series_description, body_part_examined, station_name,
            updated_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?, datetime('now'))
        ON CONFLICT(series_uid) DO UPDATE SET
            study_pk = excluded.study_pk,
            modality = excluded.modality,
            series_number = excluded.series_number,
            series_description = excluded.series_description,
            body_part_examined = excluded.body_part_examined,
            station_name = excluded.station_name,
            updated_at = datetime('now')
        RETURNING series_pk;
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

    sqlite3_bind_int64(stmt, 1, record.study_pk);
    sqlite3_bind_text(stmt, 2, record.series_uid.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, record.modality.c_str(), -1, SQLITE_TRANSIENT);

    if (record.series_number.has_value()) {
        sqlite3_bind_int(stmt, 4, *record.series_number);
    } else {
        sqlite3_bind_null(stmt, 4);
    }

    sqlite3_bind_text(stmt, 5, record.series_description.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, record.body_part_examined.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, record.station_name.c_str(), -1,
                      SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        auto error_msg = sqlite3_errmsg(db_);
        sqlite3_finalize(stmt);
        return make_error<int64_t>(
            rc, kcenon::pacs::compat::format("Failed to upsert series: {}", error_msg),
            "storage");
    }

    auto pk = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    return pk;
}

auto series_repository::find_series(std::string_view series_uid) const
    -> std::optional<series_record> {
    const char* sql = R"(
        SELECT series_pk, study_pk, series_uid, modality, series_number,
               series_description, body_part_examined, station_name,
               num_instances, created_at, updated_at
        FROM series
        WHERE series_uid = ?;
    )";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, series_uid.data(),
                      static_cast<int>(series_uid.size()), SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }

    auto record = parse_series_row(stmt);
    sqlite3_finalize(stmt);
    return record;
}

auto series_repository::find_series_by_pk(int64_t pk) const
    -> std::optional<series_record> {
    const char* sql = R"(
        SELECT series_pk, study_pk, series_uid, modality, series_number,
               series_description, body_part_examined, station_name,
               num_instances, created_at, updated_at
        FROM series
        WHERE series_pk = ?;
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

    auto record = parse_series_row(stmt);
    sqlite3_finalize(stmt);
    return record;
}

auto series_repository::list_series(std::string_view study_uid) const
    -> Result<std::vector<series_record>> {
    std::vector<series_record> results;

    const char* sql = R"(
        SELECT se.series_pk, se.study_pk, se.series_uid, se.modality,
               se.series_number, se.series_description, se.body_part_examined,
               se.station_name, se.num_instances, se.created_at, se.updated_at
        FROM series se
        JOIN studies st ON se.study_pk = st.study_pk
        WHERE st.study_uid = ?
        ORDER BY se.series_number ASC, se.series_uid ASC;
    )";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<std::vector<series_record>>(
            database_query_error,
            kcenon::pacs::compat::format("Failed to prepare query: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    sqlite3_bind_text(stmt, 1, study_uid.data(),
                      static_cast<int>(study_uid.size()), SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(parse_series_row(stmt));
    }

    sqlite3_finalize(stmt);
    return ok(std::move(results));
}

auto series_repository::search_series(const series_query& query) const
    -> Result<std::vector<series_record>> {
    std::vector<series_record> results;

    std::string sql = R"(
        SELECT se.series_pk, se.study_pk, se.series_uid, se.modality,
               se.series_number, se.series_description, se.body_part_examined,
               se.station_name, se.num_instances, se.created_at, se.updated_at
        FROM series se
        JOIN studies st ON se.study_pk = st.study_pk
        WHERE 1=1
    )";

    std::vector<std::string> params;

    if (query.study_uid.has_value()) {
        sql += " AND st.study_uid = ?";
        params.push_back(*query.study_uid);
    }
    if (query.series_uid.has_value()) {
        sql += " AND se.series_uid = ?";
        params.push_back(*query.series_uid);
    }
    if (query.modality.has_value()) {
        sql += " AND se.modality = ?";
        params.push_back(*query.modality);
    }
    if (query.series_description.has_value()) {
        sql += " AND se.series_description LIKE ?";
        params.push_back(to_like_pattern(*query.series_description));
    }
    if (query.body_part_examined.has_value()) {
        sql += " AND se.body_part_examined = ?";
        params.push_back(*query.body_part_examined);
    }

    sql += " ORDER BY se.series_number ASC, se.series_uid ASC";

    if (query.limit > 0) {
        sql += kcenon::pacs::compat::format(" LIMIT {}", query.limit);
    }
    if (query.offset > 0) {
        sql += kcenon::pacs::compat::format(" OFFSET {}", query.offset);
    }

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<std::vector<series_record>>(
            database_query_error,
            kcenon::pacs::compat::format("Failed to prepare query: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    int bind_index = 1;
    for (const auto& param : params) {
        sqlite3_bind_text(stmt, bind_index++, param.c_str(), -1,
                          SQLITE_TRANSIENT);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        auto record = parse_series_row(stmt);
        if (query.series_number.has_value()) {
            if (!record.series_number.has_value() ||
                *record.series_number != *query.series_number) {
                continue;
            }
        }
        results.push_back(std::move(record));
    }

    sqlite3_finalize(stmt);
    return ok(std::move(results));
}

auto series_repository::delete_series(std::string_view series_uid)
    -> VoidResult {
    const char* sql = "DELETE FROM series WHERE series_uid = ?;";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<std::monostate>(
            rc,
            kcenon::pacs::compat::format("Failed to prepare delete: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    sqlite3_bind_text(stmt, 1, series_uid.data(),
                      static_cast<int>(series_uid.size()), SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return make_error<std::monostate>(
            rc,
            kcenon::pacs::compat::format("Failed to delete series: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    return ok();
}

auto series_repository::series_count() const -> Result<size_t> {
    const char* sql = "SELECT COUNT(*) FROM series;";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<size_t>(
            database_query_error,
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

auto series_repository::series_count(std::string_view study_uid) const
    -> Result<size_t> {
    const char* sql = R"(
        SELECT COUNT(*) FROM series se
        JOIN studies st ON se.study_pk = st.study_pk
        WHERE st.study_uid = ?;
    )";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<size_t>(
            database_query_error,
            kcenon::pacs::compat::format("Failed to prepare query: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    sqlite3_bind_text(stmt, 1, study_uid.data(),
                      static_cast<int>(study_uid.size()), SQLITE_TRANSIENT);

    size_t count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
    }

    sqlite3_finalize(stmt);
    return ok(count);
}

}  // namespace kcenon::pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
