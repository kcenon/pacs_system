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
 * @file patient_repository.cpp
 * @brief Implementation of the patient repository
 *
 * @see Issue #896 - Decompose index_database into metadata and lifecycle stores
 * @see Issue #912 - Part 1: Extract patient and study metadata repositories
 */

#include "pacs/storage/patient_repository.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#ifdef PACS_WITH_DATABASE_SYSTEM

#include <database/query_builder.h>
#include <pacs/compat/format.hpp>

namespace pacs::storage {

using kcenon::common::make_error;
using kcenon::common::ok;

// =============================================================================
// Constructor
// =============================================================================

patient_repository::patient_repository(
    std::shared_ptr<pacs_database_adapter> db)
    : base_repository(std::move(db), "patients", "patient_pk") {}

// =============================================================================
// Timestamp Helpers
// =============================================================================

auto patient_repository::parse_timestamp(const std::string& str) const
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

auto patient_repository::format_timestamp(
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

// =============================================================================
// Wildcard Pattern Helper
// =============================================================================

auto patient_repository::to_like_pattern(std::string_view pattern)
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

// =============================================================================
// Domain-Specific Operations
// =============================================================================

auto patient_repository::upsert_patient(std::string_view patient_id,
                                        std::string_view patient_name,
                                        std::string_view birth_date,
                                        std::string_view sex)
    -> Result<int64_t> {
    patient_record record;
    record.patient_id = std::string(patient_id);
    record.patient_name = std::string(patient_name);
    record.birth_date = std::string(birth_date);
    record.sex = std::string(sex);
    return upsert_patient(record);
}

auto patient_repository::upsert_patient(const patient_record& record)
    -> Result<int64_t> {
    if (record.patient_id.empty()) {
        return make_error<int64_t>(-1, "Patient ID is required", "storage");
    }

    if (record.patient_id.length() > 64) {
        return make_error<int64_t>(
            -1, "Patient ID exceeds maximum length of 64 characters",
            "storage");
    }

    if (!record.sex.empty() && record.sex != "M" && record.sex != "F" &&
        record.sex != "O") {
        return make_error<int64_t>(
            -1, "Invalid sex value. Must be M, F, or O", "storage");
    }

    if (!db() || !db()->is_connected()) {
        return make_error<int64_t>(-1, "Database not connected", "storage");
    }

    auto builder = query_builder();

    // Check if patient exists
    auto check_sql = builder.select(std::vector<std::string>{"patient_pk"})
                         .from("patients")
                         .where("patient_id", "=", record.patient_id)
                         .build();

    auto check_result = db()->select(check_sql);
    if (check_result.is_err()) {
        return make_error<int64_t>(
            -1,
            pacs::compat::format("Failed to check patient existence: {}",
                                check_result.error().message),
            "storage");
    }

    if (!check_result.value().empty()) {
        // Patient exists - update
        auto existing = find_patient(record.patient_id);
        if (!existing.has_value()) {
            return make_error<int64_t>(
                -1, "Patient exists but could not retrieve record", "storage");
        }

        database::query_builder update_builder(database::database_types::sqlite);
        auto update_sql =
            update_builder.update("patients")
                .set({{"patient_name", record.patient_name},
                      {"birth_date", record.birth_date},
                      {"sex", record.sex},
                      {"other_ids", record.other_ids},
                      {"ethnic_group", record.ethnic_group},
                      {"comments", record.comments},
                      {"updated_at", "datetime('now')"}})
                .where("patient_id", "=", record.patient_id)
                .build();

        auto update_result = db()->update(update_sql);
        if (update_result.is_err()) {
            return make_error<int64_t>(
                -1,
                pacs::compat::format("Failed to update patient: {}",
                                    update_result.error().message),
                "storage");
        }

        return existing->pk;
    } else {
        // Patient doesn't exist - insert
        database::query_builder insert_builder(database::database_types::sqlite);
        auto insert_sql =
            insert_builder.insert_into("patients")
                .values({{"patient_id", record.patient_id},
                         {"patient_name", record.patient_name},
                         {"birth_date", record.birth_date},
                         {"sex", record.sex},
                         {"other_ids", record.other_ids},
                         {"ethnic_group", record.ethnic_group},
                         {"comments", record.comments}})
                .build();

        auto insert_result = db()->insert(insert_sql);
        if (insert_result.is_err()) {
            return make_error<int64_t>(
                -1,
                pacs::compat::format("Failed to insert patient: {}",
                                    insert_result.error().message),
                "storage");
        }

        // Retrieve the inserted patient to get pk
        auto inserted = find_patient(record.patient_id);
        if (!inserted.has_value()) {
            return make_error<int64_t>(
                -1, "Patient inserted but could not retrieve record", "storage");
        }

        return inserted->pk;
    }
}

auto patient_repository::find_patient(std::string_view patient_id)
    -> std::optional<patient_record> {
    if (!db() || !db()->is_connected()) {
        return std::nullopt;
    }

    auto builder = query_builder();
    auto select_sql =
        builder
            .select(select_columns())
            .from("patients")
            .where("patient_id", "=", std::string(patient_id))
            .build();

    auto result = db()->select(select_sql);
    if (result.is_err() || result.value().empty()) {
        return std::nullopt;
    }

    return map_row_to_entity(result.value()[0]);
}

auto patient_repository::find_patient_by_pk(int64_t pk)
    -> std::optional<patient_record> {
    if (!db() || !db()->is_connected()) {
        return std::nullopt;
    }

    auto builder = query_builder();
    auto select_sql =
        builder
            .select(select_columns())
            .from("patients")
            .where("patient_pk", "=", pk)
            .build();

    auto result = db()->select(select_sql);
    if (result.is_err() || result.value().empty()) {
        return std::nullopt;
    }

    return map_row_to_entity(result.value()[0]);
}

auto patient_repository::search_patients(const patient_query& query)
    -> Result<std::vector<patient_record>> {
    if (!db() || !db()->is_connected()) {
        return make_error<std::vector<patient_record>>(
            -1, "Database not connected", "storage");
    }

    auto builder = query_builder();
    builder.select(select_columns());
    builder.from("patients");

    if (query.patient_id.has_value()) {
        builder.where("patient_id", "LIKE", to_like_pattern(*query.patient_id));
    }

    if (query.patient_name.has_value()) {
        builder.where("patient_name", "LIKE", to_like_pattern(*query.patient_name));
    }

    if (query.birth_date.has_value()) {
        builder.where("birth_date", "=", *query.birth_date);
    }

    if (query.birth_date_from.has_value()) {
        builder.where("birth_date", ">=", *query.birth_date_from);
    }

    if (query.birth_date_to.has_value()) {
        builder.where("birth_date", "<=", *query.birth_date_to);
    }

    if (query.sex.has_value()) {
        builder.where("sex", "=", *query.sex);
    }

    builder.order_by("patient_name");
    builder.order_by("patient_id");

    if (query.limit > 0) {
        builder.limit(static_cast<int>(query.limit));
    }

    if (query.offset > 0) {
        builder.offset(static_cast<int>(query.offset));
    }

    auto select_sql = builder.build();
    auto result = db()->select(select_sql);
    if (result.is_err()) {
        return make_error<std::vector<patient_record>>(
            -1,
            pacs::compat::format("Query failed: {}", result.error().message),
            "storage");
    }

    std::vector<patient_record> results;
    results.reserve(result.value().size());
    for (const auto& row : result.value()) {
        results.push_back(map_row_to_entity(row));
    }
    return ok(std::move(results));
}

auto patient_repository::delete_patient(std::string_view patient_id)
    -> VoidResult {
    if (!db() || !db()->is_connected()) {
        return make_error<std::monostate>(-1, "Database not connected", "storage");
    }

    auto builder = query_builder();
    auto delete_sql = builder.delete_from("patients")
                          .where("patient_id", "=", std::string(patient_id))
                          .build();

    auto result = db()->remove(delete_sql);
    if (result.is_err()) {
        return make_error<std::monostate>(
            -1,
            pacs::compat::format("Failed to delete patient: {}",
                                 result.error().message),
            "storage");
    }
    return ok();
}

auto patient_repository::patient_count() -> Result<size_t> {
    if (!db() || !db()->is_connected()) {
        return make_error<size_t>(-1, "Database not connected", "storage");
    }

    auto result = db()->select("SELECT COUNT(*) AS count FROM patients;");
    if (result.is_err()) {
        return make_error<size_t>(
            -1,
            pacs::compat::format("Query failed: {}", result.error().message),
            "storage");
    }

    if (!result.value().empty()) {
        const auto& row = result.value()[0];
        auto it = row.find("count");
        if (it == row.end() && !row.empty()) {
            it = row.begin();
        }

        if (it != row.end() && !it->second.empty()) {
            try {
                return ok(static_cast<size_t>(std::stoll(it->second)));
            } catch (...) {
                return ok(static_cast<size_t>(0));
            }
        }
    }
    return ok(static_cast<size_t>(0));
}

// =============================================================================
// base_repository Overrides
// =============================================================================

auto patient_repository::map_row_to_entity(const database_row& row) const
    -> patient_record {
    patient_record record;

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

    record.pk = get_int64("patient_pk");
    record.patient_id = get_str("patient_id");
    record.patient_name = get_str("patient_name");
    record.birth_date = get_str("birth_date");
    record.sex = get_str("sex");
    record.other_ids = get_str("other_ids");
    record.ethnic_group = get_str("ethnic_group");
    record.comments = get_str("comments");

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

auto patient_repository::entity_to_row(const patient_record& entity) const
    -> std::map<std::string, database_value> {
    std::map<std::string, database_value> row;

    row["patient_id"] = entity.patient_id;
    row["patient_name"] = entity.patient_name;
    row["birth_date"] = entity.birth_date;
    row["sex"] = entity.sex;
    row["other_ids"] = entity.other_ids;
    row["ethnic_group"] = entity.ethnic_group;
    row["comments"] = entity.comments;

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

auto patient_repository::get_pk(const patient_record& entity) const
    -> int64_t {
    return entity.pk;
}

auto patient_repository::has_pk(const patient_record& entity) const -> bool {
    return entity.pk > 0;
}

auto patient_repository::select_columns() const -> std::vector<std::string> {
    return {"patient_pk", "patient_id",   "patient_name", "birth_date",
            "sex",        "other_ids",    "ethnic_group", "comments",
            "created_at", "updated_at"};
}

}  // namespace pacs::storage

#else  // !PACS_WITH_DATABASE_SYSTEM

// =============================================================================
// Legacy SQLite Implementation
// =============================================================================

#include <pacs/compat/format.hpp>
#include <pacs/core/result.hpp>
#include <sqlite3.h>

namespace pacs::storage {

using kcenon::common::make_error;
using kcenon::common::ok;
using namespace pacs::error_codes;

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

patient_repository::patient_repository(sqlite3* db) : db_(db) {}

patient_repository::~patient_repository() = default;

patient_repository::patient_repository(patient_repository&&) noexcept = default;

auto patient_repository::operator=(patient_repository&&) noexcept
    -> patient_repository& = default;

auto patient_repository::to_like_pattern(std::string_view pattern)
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

auto patient_repository::parse_patient_row(void* stmt_ptr) const
    -> patient_record {
    auto* stmt = static_cast<sqlite3_stmt*>(stmt_ptr);
    patient_record record;

    record.pk = sqlite3_column_int64(stmt, 0);
    record.patient_id = get_text(stmt, 1);
    record.patient_name = get_text(stmt, 2);
    record.birth_date = get_text(stmt, 3);
    record.sex = get_text(stmt, 4);
    record.other_ids = get_text(stmt, 5);
    record.ethnic_group = get_text(stmt, 6);
    record.comments = get_text(stmt, 7);

    auto created_str = get_text(stmt, 8);
    record.created_at = parse_datetime(created_str.c_str());

    auto updated_str = get_text(stmt, 9);
    record.updated_at = parse_datetime(updated_str.c_str());

    return record;
}

auto patient_repository::upsert_patient(std::string_view patient_id,
                                        std::string_view patient_name,
                                        std::string_view birth_date,
                                        std::string_view sex)
    -> Result<int64_t> {
    patient_record record;
    record.patient_id = std::string(patient_id);
    record.patient_name = std::string(patient_name);
    record.birth_date = std::string(birth_date);
    record.sex = std::string(sex);
    return upsert_patient(record);
}

auto patient_repository::upsert_patient(const patient_record& record)
    -> Result<int64_t> {
    if (record.patient_id.empty()) {
        return make_error<int64_t>(-1, "Patient ID is required", "storage");
    }

    if (record.patient_id.length() > 64) {
        return make_error<int64_t>(
            -1, "Patient ID exceeds maximum length of 64 characters",
            "storage");
    }

    if (!record.sex.empty() && record.sex != "M" && record.sex != "F" &&
        record.sex != "O") {
        return make_error<int64_t>(
            -1, "Invalid sex value. Must be M, F, or O", "storage");
    }

    const char* sql = R"(
        INSERT INTO patients (
            patient_id, patient_name, birth_date, sex,
            other_ids, ethnic_group, comments, updated_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?, datetime('now'))
        ON CONFLICT(patient_id) DO UPDATE SET
            patient_name = excluded.patient_name,
            birth_date = excluded.birth_date,
            sex = excluded.sex,
            other_ids = excluded.other_ids,
            ethnic_group = excluded.ethnic_group,
            comments = excluded.comments,
            updated_at = datetime('now')
        RETURNING patient_pk;
    )";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<int64_t>(
            rc,
            pacs::compat::format("Failed to prepare statement: {}", sqlite3_errmsg(db_)),
            "storage");
    }

    sqlite3_bind_text(stmt, 1, record.patient_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, record.patient_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, record.birth_date.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, record.sex.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, record.other_ids.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, record.ethnic_group.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, record.comments.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        auto error_msg = sqlite3_errmsg(db_);
        sqlite3_finalize(stmt);
        return make_error<int64_t>(
            rc, pacs::compat::format("Failed to upsert patient: {}", error_msg),
            "storage");
    }

    auto pk = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);

    return pk;
}

auto patient_repository::find_patient(std::string_view patient_id) const
    -> std::optional<patient_record> {
    const char* sql = R"(
        SELECT patient_pk, patient_id, patient_name, birth_date, sex,
               other_ids, ethnic_group, comments, created_at, updated_at
        FROM patients
        WHERE patient_id = ?;
    )";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, patient_id.data(),
                      static_cast<int>(patient_id.size()), SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }

    auto record = parse_patient_row(stmt);
    sqlite3_finalize(stmt);

    return record;
}

auto patient_repository::find_patient_by_pk(int64_t pk) const
    -> std::optional<patient_record> {
    const char* sql = R"(
        SELECT patient_pk, patient_id, patient_name, birth_date, sex,
               other_ids, ethnic_group, comments, created_at, updated_at
        FROM patients
        WHERE patient_pk = ?;
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

    auto record = parse_patient_row(stmt);
    sqlite3_finalize(stmt);

    return record;
}

auto patient_repository::search_patients(const patient_query& query) const
    -> Result<std::vector<patient_record>> {
    std::vector<patient_record> results;

    std::string sql = R"(
        SELECT patient_pk, patient_id, patient_name, birth_date, sex,
               other_ids, ethnic_group, comments, created_at, updated_at
        FROM patients
        WHERE 1=1
    )";

    std::vector<std::string> params;

    if (query.patient_id.has_value()) {
        sql += " AND patient_id LIKE ?";
        params.push_back(to_like_pattern(*query.patient_id));
    }

    if (query.patient_name.has_value()) {
        sql += " AND patient_name LIKE ?";
        params.push_back(to_like_pattern(*query.patient_name));
    }

    if (query.birth_date.has_value()) {
        sql += " AND birth_date = ?";
        params.push_back(*query.birth_date);
    }

    if (query.birth_date_from.has_value()) {
        sql += " AND birth_date >= ?";
        params.push_back(*query.birth_date_from);
    }

    if (query.birth_date_to.has_value()) {
        sql += " AND birth_date <= ?";
        params.push_back(*query.birth_date_to);
    }

    if (query.sex.has_value()) {
        sql += " AND sex = ?";
        params.push_back(*query.sex);
    }

    sql += " ORDER BY patient_name, patient_id";

    if (query.limit > 0) {
        sql += pacs::compat::format(" LIMIT {}", query.limit);
    }

    if (query.offset > 0) {
        sql += pacs::compat::format(" OFFSET {}", query.offset);
    }

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<std::vector<patient_record>>(
            database_query_error,
            pacs::compat::format("Failed to prepare query: {}", sqlite3_errmsg(db_)),
            "storage");
    }

    for (size_t i = 0; i < params.size(); ++i) {
        sqlite3_bind_text(stmt, static_cast<int>(i + 1), params[i].c_str(), -1,
                          SQLITE_TRANSIENT);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(parse_patient_row(stmt));
    }

    sqlite3_finalize(stmt);
    return ok(std::move(results));
}

auto patient_repository::delete_patient(std::string_view patient_id)
    -> VoidResult {
    const char* sql = "DELETE FROM patients WHERE patient_id = ?;";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<std::monostate>(
            rc,
            pacs::compat::format("Failed to prepare delete: {}", sqlite3_errmsg(db_)),
            "storage");
    }

    sqlite3_bind_text(stmt, 1, patient_id.data(),
                      static_cast<int>(patient_id.size()), SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return make_error<std::monostate>(
            rc, pacs::compat::format("Failed to delete patient: {}", sqlite3_errmsg(db_)),
            "storage");
    }

    return ok();
}

auto patient_repository::patient_count() const -> Result<size_t> {
    const char* sql = "SELECT COUNT(*) FROM patients;";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<size_t>(
            database_query_error,
            pacs::compat::format("Failed to prepare query: {}", sqlite3_errmsg(db_)),
            "storage");
    }

    size_t count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
    }

    sqlite3_finalize(stmt);
    return ok(count);
}

}  // namespace pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
