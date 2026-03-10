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
 * @file study_repository.cpp
 * @brief Implementation of the study repository
 *
 * @see Issue #896 - Decompose index_database into metadata and lifecycle stores
 * @see Issue #912 - Part 1: Extract patient and study metadata repositories
 */

#include "pacs/storage/study_repository.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <set>
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

study_repository::study_repository(std::shared_ptr<pacs_database_adapter> db)
    : base_repository(std::move(db), "studies", "study_pk") {}

// =============================================================================
// Timestamp Helpers
// =============================================================================

auto study_repository::parse_timestamp(const std::string& str) const
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

auto study_repository::format_timestamp(
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

auto study_repository::to_like_pattern(std::string_view pattern)
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

auto study_repository::upsert_study(int64_t patient_pk,
                                    std::string_view study_uid,
                                    std::string_view study_id,
                                    std::string_view study_date,
                                    std::string_view study_time,
                                    std::string_view accession_number,
                                    std::string_view referring_physician,
                                    std::string_view study_description)
    -> Result<int64_t> {
    study_record record;
    record.patient_pk = patient_pk;
    record.study_uid = std::string(study_uid);
    record.study_id = std::string(study_id);
    record.study_date = std::string(study_date);
    record.study_time = std::string(study_time);
    record.accession_number = std::string(accession_number);
    record.referring_physician = std::string(referring_physician);
    record.study_description = std::string(study_description);
    return upsert_study(record);
}

auto study_repository::upsert_study(const study_record& record)
    -> Result<int64_t> {
    if (record.study_uid.empty()) {
        return make_error<int64_t>(-1, "Study Instance UID is required",
                                   "storage");
    }

    if (record.study_uid.length() > 64) {
        return make_error<int64_t>(
            -1, "Study Instance UID exceeds maximum length of 64 characters",
            "storage");
    }

    if (record.patient_pk <= 0) {
        return make_error<int64_t>(-1, "Valid patient_pk is required",
                                   "storage");
    }

    if (!db() || !db()->is_connected()) {
        return make_error<int64_t>(-1, "Database not connected", "storage");
    }

    auto builder = query_builder();

    // Check if study exists
    auto check_sql = builder.select(std::vector<std::string>{"study_pk"})
                         .from("studies")
                         .where("study_uid", "=", record.study_uid)
                         .build();

    auto check_result = db()->select(check_sql);
    if (check_result.is_err()) {
        return make_error<int64_t>(
            -1,
            pacs::compat::format("Failed to check study existence: {}",
                                check_result.error().message),
            "storage");
    }

    if (!check_result.value().empty()) {
        // Study exists - update
        auto existing = find_study(record.study_uid);
        if (!existing.has_value()) {
            return make_error<int64_t>(
                -1, "Study exists but could not retrieve record", "storage");
        }

        auto update_builder = db()->create_query_builder();
        auto update_sql =
            update_builder.update("studies")
                .set({{"patient_pk", std::to_string(record.patient_pk)},
                      {"study_id", record.study_id},
                      {"study_date", record.study_date},
                      {"study_time", record.study_time},
                      {"accession_number", record.accession_number},
                      {"referring_physician", record.referring_physician},
                      {"study_description", record.study_description},
                      {"updated_at", "datetime('now')"}})
                .where("study_uid", "=", record.study_uid)
                .build();

        auto update_result = db()->update(update_sql);
        if (update_result.is_err()) {
            return make_error<int64_t>(
                -1,
                pacs::compat::format("Failed to update study: {}",
                                    update_result.error().message),
                "storage");
        }
        return existing->pk;
    } else {
        // Study doesn't exist - insert
        auto insert_builder = db()->create_query_builder();
        auto insert_sql =
            insert_builder.insert_into("studies")
                .values({{"patient_pk", std::to_string(record.patient_pk)},
                         {"study_uid", record.study_uid},
                         {"study_id", record.study_id},
                         {"study_date", record.study_date},
                         {"study_time", record.study_time},
                         {"accession_number", record.accession_number},
                         {"referring_physician", record.referring_physician},
                         {"study_description", record.study_description}})
                .build();

        auto insert_result = db()->insert(insert_sql);
        if (insert_result.is_err()) {
            return make_error<int64_t>(
                -1,
                pacs::compat::format("Failed to insert study: {}",
                                    insert_result.error().message),
                "storage");
        }

        auto inserted = find_study(record.study_uid);
        if (!inserted.has_value()) {
            return make_error<int64_t>(
                -1, "Study inserted but could not retrieve record", "storage");
        }
        return inserted->pk;
    }
}

auto study_repository::find_study(std::string_view study_uid)
    -> std::optional<study_record> {
    if (!db() || !db()->is_connected()) {
        return std::nullopt;
    }

    auto builder = query_builder();
    auto select_sql =
        builder
            .select(select_columns())
            .from("studies")
            .where("study_uid", "=", std::string(study_uid))
            .build();

    auto result = db()->select(select_sql);
    if (result.is_err() || result.value().empty()) {
        return std::nullopt;
    }

    return map_row_to_entity(result.value()[0]);
}

auto study_repository::find_study_by_pk(int64_t pk)
    -> std::optional<study_record> {
    if (!db() || !db()->is_connected()) {
        return std::nullopt;
    }

    auto builder = query_builder();
    auto select_sql =
        builder
            .select(select_columns())
            .from("studies")
            .where("study_pk", "=", pk)
            .build();

    auto result = db()->select(select_sql);
    if (result.is_err() || result.value().empty()) {
        return std::nullopt;
    }

    return map_row_to_entity(result.value()[0]);
}

auto study_repository::search_studies(const study_query& query)
    -> Result<std::vector<study_record>> {
    if (!db() || !db()->is_connected()) {
        return make_error<std::vector<study_record>>(
            -1, "Database not connected", "storage");
    }

    // Build SQL query with JOIN if patient filters are present
    std::string sql;
    std::vector<std::string> where_clauses;

    if (query.patient_id.has_value() || query.patient_name.has_value()) {
        sql = R"(
            SELECT s.study_pk, s.patient_pk, s.study_uid, s.study_id, s.study_date,
                   s.study_time, s.accession_number, s.referring_physician,
                   s.study_description, s.modalities_in_study, s.num_series,
                   s.num_instances, s.created_at, s.updated_at
            FROM studies s
            JOIN patients p ON s.patient_pk = p.patient_pk
        )";

        if (query.patient_id.has_value()) {
            where_clauses.push_back(
                pacs::compat::format("p.patient_id LIKE '{}'",
                                   to_like_pattern(*query.patient_id)));
        }

        if (query.patient_name.has_value()) {
            where_clauses.push_back(
                pacs::compat::format("p.patient_name LIKE '{}'",
                                   to_like_pattern(*query.patient_name)));
        }
    } else {
        sql = R"(
            SELECT study_pk, patient_pk, study_uid, study_id, study_date,
                   study_time, accession_number, referring_physician,
                   study_description, modalities_in_study, num_series,
                   num_instances, created_at, updated_at
            FROM studies
        )";
    }

    // Add study-specific filters
    std::string prefix = (query.patient_id.has_value() || query.patient_name.has_value()) ? "s." : "";

    if (query.study_uid.has_value()) {
        where_clauses.push_back(
            pacs::compat::format("{}study_uid = '{}'", prefix, *query.study_uid));
    }

    if (query.study_id.has_value()) {
        where_clauses.push_back(
            pacs::compat::format("{}study_id LIKE '{}'", prefix,
                               to_like_pattern(*query.study_id)));
    }

    if (query.study_date.has_value()) {
        where_clauses.push_back(
            pacs::compat::format("{}study_date = '{}'", prefix, *query.study_date));
    }

    if (query.study_date_from.has_value()) {
        where_clauses.push_back(
            pacs::compat::format("{}study_date >= '{}'", prefix, *query.study_date_from));
    }

    if (query.study_date_to.has_value()) {
        where_clauses.push_back(
            pacs::compat::format("{}study_date <= '{}'", prefix, *query.study_date_to));
    }

    if (query.accession_number.has_value()) {
        where_clauses.push_back(
            pacs::compat::format("{}accession_number LIKE '{}'", prefix,
                               to_like_pattern(*query.accession_number)));
    }

    if (query.referring_physician.has_value()) {
        where_clauses.push_back(
            pacs::compat::format("{}referring_physician LIKE '{}'", prefix,
                               to_like_pattern(*query.referring_physician)));
    }

    if (query.study_description.has_value()) {
        where_clauses.push_back(
            pacs::compat::format("{}study_description LIKE '{}'", prefix,
                               to_like_pattern(*query.study_description)));
    }

    if (query.modality.has_value()) {
        const auto& mod = *query.modality;
        where_clauses.push_back(pacs::compat::format(
            "({}modalities_in_study = '{}' OR "
            "{}modalities_in_study LIKE '{}\\%' OR "
            "{}modalities_in_study LIKE '%\\{}' OR "
            "{}modalities_in_study LIKE '%\\{}\\%')",
            prefix, mod, prefix, mod, prefix, mod, prefix, mod));
    }

    // Build WHERE clause
    if (!where_clauses.empty()) {
        sql += " WHERE " + where_clauses[0];
        for (size_t i = 1; i < where_clauses.size(); ++i) {
            sql += " AND " + where_clauses[i];
        }
    }

    // Add ORDER BY
    sql += pacs::compat::format(" ORDER BY {}study_date DESC, {}study_time DESC",
                               prefix, prefix);

    if (query.limit > 0) {
        sql += pacs::compat::format(" LIMIT {}", query.limit);
    }

    if (query.offset > 0) {
        sql += pacs::compat::format(" OFFSET {}", query.offset);
    }

    auto result = db()->select(sql);
    if (result.is_err()) {
        return make_error<std::vector<study_record>>(
            -1,
            pacs::compat::format("Query failed: {}", result.error().message),
            "storage");
    }

    std::vector<study_record> results;
    results.reserve(result.value().size());
    for (const auto& row : result.value()) {
        results.push_back(map_row_to_entity(row));
    }
    return ok(std::move(results));
}

auto study_repository::delete_study(std::string_view study_uid) -> VoidResult {
    if (!db() || !db()->is_connected()) {
        return make_error<std::monostate>(-1, "Database not connected", "storage");
    }

    auto builder = query_builder();
    auto delete_sql = builder.delete_from("studies")
                          .where("study_uid", "=", std::string(study_uid))
                          .build();

    auto result = db()->remove(delete_sql);
    if (result.is_err()) {
        return make_error<std::monostate>(
            -1,
            pacs::compat::format("Failed to delete study: {}",
                                 result.error().message),
            "storage");
    }
    return ok();
}

auto study_repository::study_count() -> Result<size_t> {
    if (!db() || !db()->is_connected()) {
        return make_error<size_t>(-1, "Database not connected", "storage");
    }

    auto builder = query_builder();
    auto count_sql = builder.select(std::vector<std::string>{"COUNT(*) AS cnt"})
                         .from("studies")
                         .build();

    auto result = db()->select(count_sql);
    if (result.is_err()) {
        return make_error<size_t>(
            -1,
            pacs::compat::format("Query failed: {}", result.error().message),
            "storage");
    }

    if (!result.value().empty()) {
        const auto& row = result.value()[0];
        auto it = row.find("cnt");
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

auto study_repository::study_count_for_patient(int64_t patient_pk)
    -> Result<size_t> {
    if (!db() || !db()->is_connected()) {
        return make_error<size_t>(-1, "Database not connected", "storage");
    }

    auto builder = query_builder();
    auto count_sql = builder.select(std::vector<std::string>{"COUNT(*) AS cnt"})
                         .from("studies")
                         .where("patient_pk", "=", patient_pk)
                         .build();

    auto result = db()->select(count_sql);
    if (result.is_err()) {
        return make_error<size_t>(
            -1,
            pacs::compat::format("Query failed: {}", result.error().message),
            "storage");
    }

    if (!result.value().empty()) {
        const auto& row = result.value()[0];
        auto it = row.find("cnt");
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

auto study_repository::update_modalities_in_study(int64_t study_pk)
    -> VoidResult {
    if (!db() || !db()->is_connected()) {
        return make_error<std::monostate>(-1, "Database not connected", "storage");
    }

    // Step 1: Get distinct modalities from series table
    auto select_builder = db()->create_query_builder();
    auto select_sql =
        select_builder
            .select(std::vector<std::string>{"DISTINCT modality"})
            .from("series")
            .where("study_pk", "=", study_pk)
            .build();

    auto select_result = db()->select(select_sql);
    if (select_result.is_err()) {
        return make_error<std::monostate>(
            -1,
            pacs::compat::format("Failed to query modalities: {}",
                                 select_result.error().message),
            "storage");
    }

    // Step 2: Build modalities string with backslash separator
    std::set<std::string> unique_modalities;
    for (const auto& row : select_result.value()) {
        auto it = row.find("modality");
        if (it != row.end() && !it->second.empty()) {
            unique_modalities.insert(it->second);
        }
    }

    std::string modalities_str;
    for (const auto& mod : unique_modalities) {
        if (!modalities_str.empty()) {
            modalities_str += "\\";  // DICOM multi-value separator
        }
        modalities_str += mod;
    }

    // Step 3: Update studies table
    auto update_builder = db()->create_query_builder();
    auto update_sql =
        update_builder.update("studies")
            .set({{"modalities_in_study", modalities_str},
                  {"updated_at", "datetime('now')"}})
            .where("study_pk", "=", study_pk)
            .build();

    auto update_result = db()->update(update_sql);
    if (update_result.is_err()) {
        return make_error<std::monostate>(
            -1,
            pacs::compat::format("Failed to update modalities: {}",
                                 update_result.error().message),
            "storage");
    }
    return ok();
}

// =============================================================================
// base_repository Overrides
// =============================================================================

auto study_repository::map_row_to_entity(const database_row& row) const
    -> study_record {
    study_record record;

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

    record.pk = get_int64("study_pk");
    record.patient_pk = get_int64("patient_pk");
    record.study_uid = get_str("study_uid");
    record.study_id = get_str("study_id");
    record.study_date = get_str("study_date");
    record.study_time = get_str("study_time");
    record.accession_number = get_str("accession_number");
    record.referring_physician = get_str("referring_physician");
    record.study_description = get_str("study_description");
    record.modalities_in_study = get_str("modalities_in_study");
    record.num_instances = get_optional_int("num_instances").value_or(0);
    record.num_series = get_optional_int("num_series").value_or(0);

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

auto study_repository::entity_to_row(const study_record& entity) const
    -> std::map<std::string, database_value> {
    std::map<std::string, database_value> row;

    row["patient_pk"] = std::to_string(entity.patient_pk);
    row["study_uid"] = entity.study_uid;
    row["study_id"] = entity.study_id;
    row["study_date"] = entity.study_date;
    row["study_time"] = entity.study_time;
    row["accession_number"] = entity.accession_number;
    row["referring_physician"] = entity.referring_physician;
    row["study_description"] = entity.study_description;
    row["modalities_in_study"] = entity.modalities_in_study;
    row["num_series"] = std::to_string(entity.num_series);
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

auto study_repository::get_pk(const study_record& entity) const -> int64_t {
    return entity.pk;
}

auto study_repository::has_pk(const study_record& entity) const -> bool {
    return entity.pk > 0;
}

auto study_repository::select_columns() const -> std::vector<std::string> {
    return {"study_pk",        "patient_pk",         "study_uid",
            "study_id",        "study_date",         "study_time",
            "accession_number", "referring_physician", "study_description",
            "modalities_in_study", "num_series",     "num_instances",
            "created_at",      "updated_at"};
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

study_repository::study_repository(sqlite3* db) : db_(db) {}

study_repository::~study_repository() = default;

study_repository::study_repository(study_repository&&) noexcept = default;

auto study_repository::operator=(study_repository&&) noexcept
    -> study_repository& = default;

auto study_repository::to_like_pattern(std::string_view pattern)
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

auto study_repository::parse_study_row(void* stmt_ptr) const -> study_record {
    auto* stmt = static_cast<sqlite3_stmt*>(stmt_ptr);
    study_record record;

    record.pk = sqlite3_column_int64(stmt, 0);
    record.patient_pk = sqlite3_column_int64(stmt, 1);
    record.study_uid = get_text(stmt, 2);
    record.study_id = get_text(stmt, 3);
    record.study_date = get_text(stmt, 4);
    record.study_time = get_text(stmt, 5);
    record.accession_number = get_text(stmt, 6);
    record.referring_physician = get_text(stmt, 7);
    record.study_description = get_text(stmt, 8);
    record.modalities_in_study = get_text(stmt, 9);
    record.num_series = sqlite3_column_int(stmt, 10);
    record.num_instances = sqlite3_column_int(stmt, 11);

    auto created_str = get_text(stmt, 12);
    record.created_at = parse_datetime(created_str.c_str());

    auto updated_str = get_text(stmt, 13);
    record.updated_at = parse_datetime(updated_str.c_str());

    return record;
}

auto study_repository::upsert_study(int64_t patient_pk,
                                    std::string_view study_uid,
                                    std::string_view study_id,
                                    std::string_view study_date,
                                    std::string_view study_time,
                                    std::string_view accession_number,
                                    std::string_view referring_physician,
                                    std::string_view study_description)
    -> Result<int64_t> {
    study_record record;
    record.patient_pk = patient_pk;
    record.study_uid = std::string(study_uid);
    record.study_id = std::string(study_id);
    record.study_date = std::string(study_date);
    record.study_time = std::string(study_time);
    record.accession_number = std::string(accession_number);
    record.referring_physician = std::string(referring_physician);
    record.study_description = std::string(study_description);
    return upsert_study(record);
}

auto study_repository::upsert_study(const study_record& record)
    -> Result<int64_t> {
    if (record.study_uid.empty()) {
        return make_error<int64_t>(-1, "Study Instance UID is required",
                                   "storage");
    }

    if (record.study_uid.length() > 64) {
        return make_error<int64_t>(
            -1, "Study Instance UID exceeds maximum length of 64 characters",
            "storage");
    }

    if (record.patient_pk <= 0) {
        return make_error<int64_t>(-1, "Valid patient_pk is required",
                                   "storage");
    }

    const char* sql = R"(
        INSERT INTO studies (
            patient_pk, study_uid, study_id, study_date, study_time,
            accession_number, referring_physician, study_description,
            updated_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, datetime('now'))
        ON CONFLICT(study_uid) DO UPDATE SET
            patient_pk = excluded.patient_pk,
            study_id = excluded.study_id,
            study_date = excluded.study_date,
            study_time = excluded.study_time,
            accession_number = excluded.accession_number,
            referring_physician = excluded.referring_physician,
            study_description = excluded.study_description,
            updated_at = datetime('now')
        RETURNING study_pk;
    )";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<int64_t>(
            rc,
            pacs::compat::format("Failed to prepare statement: {}", sqlite3_errmsg(db_)),
            "storage");
    }

    sqlite3_bind_int64(stmt, 1, record.patient_pk);
    sqlite3_bind_text(stmt, 2, record.study_uid.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, record.study_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, record.study_date.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, record.study_time.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, record.accession_number.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, record.referring_physician.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, record.study_description.c_str(), -1,
                      SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        auto error_msg = sqlite3_errmsg(db_);
        sqlite3_finalize(stmt);
        return make_error<int64_t>(
            rc, pacs::compat::format("Failed to upsert study: {}", error_msg),
            "storage");
    }

    auto pk = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);

    return pk;
}

auto study_repository::find_study(std::string_view study_uid) const
    -> std::optional<study_record> {
    const char* sql = R"(
        SELECT study_pk, patient_pk, study_uid, study_id, study_date,
               study_time, accession_number, referring_physician,
               study_description, modalities_in_study, num_series,
               num_instances, created_at, updated_at
        FROM studies
        WHERE study_uid = ?;
    )";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, study_uid.data(),
                      static_cast<int>(study_uid.size()), SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }

    auto record = parse_study_row(stmt);
    sqlite3_finalize(stmt);

    return record;
}

auto study_repository::find_study_by_pk(int64_t pk) const
    -> std::optional<study_record> {
    const char* sql = R"(
        SELECT study_pk, patient_pk, study_uid, study_id, study_date,
               study_time, accession_number, referring_physician,
               study_description, modalities_in_study, num_series,
               num_instances, created_at, updated_at
        FROM studies
        WHERE study_pk = ?;
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

    auto record = parse_study_row(stmt);
    sqlite3_finalize(stmt);

    return record;
}

auto study_repository::search_studies(const study_query& query) const
    -> Result<std::vector<study_record>> {
    std::vector<study_record> results;

    std::string sql = R"(
        SELECT s.study_pk, s.patient_pk, s.study_uid, s.study_id, s.study_date,
               s.study_time, s.accession_number, s.referring_physician,
               s.study_description, s.modalities_in_study, s.num_series,
               s.num_instances, s.created_at, s.updated_at
        FROM studies s
        JOIN patients p ON s.patient_pk = p.patient_pk
        WHERE 1=1
    )";

    std::vector<std::string> params;

    if (query.patient_id.has_value()) {
        sql += " AND p.patient_id LIKE ?";
        params.push_back(to_like_pattern(*query.patient_id));
    }

    if (query.patient_name.has_value()) {
        sql += " AND p.patient_name LIKE ?";
        params.push_back(to_like_pattern(*query.patient_name));
    }

    if (query.study_uid.has_value()) {
        sql += " AND s.study_uid = ?";
        params.push_back(*query.study_uid);
    }

    if (query.study_id.has_value()) {
        sql += " AND s.study_id LIKE ?";
        params.push_back(to_like_pattern(*query.study_id));
    }

    if (query.study_date.has_value()) {
        sql += " AND s.study_date = ?";
        params.push_back(*query.study_date);
    }

    if (query.study_date_from.has_value()) {
        sql += " AND s.study_date >= ?";
        params.push_back(*query.study_date_from);
    }

    if (query.study_date_to.has_value()) {
        sql += " AND s.study_date <= ?";
        params.push_back(*query.study_date_to);
    }

    if (query.accession_number.has_value()) {
        sql += " AND s.accession_number LIKE ?";
        params.push_back(to_like_pattern(*query.accession_number));
    }

    if (query.modality.has_value()) {
        sql += " AND (s.modalities_in_study = ? OR "
               "s.modalities_in_study LIKE ? OR "
               "s.modalities_in_study LIKE ? OR "
               "s.modalities_in_study LIKE ?)";
        params.push_back(*query.modality);
        params.push_back(*query.modality + "\\%");
        params.push_back("%\\" + *query.modality);
        params.push_back("%\\" + *query.modality + "\\%");
    }

    if (query.referring_physician.has_value()) {
        sql += " AND s.referring_physician LIKE ?";
        params.push_back(to_like_pattern(*query.referring_physician));
    }

    if (query.study_description.has_value()) {
        sql += " AND s.study_description LIKE ?";
        params.push_back(to_like_pattern(*query.study_description));
    }

    sql += " ORDER BY s.study_date DESC, s.study_time DESC";

    if (query.limit > 0) {
        sql += pacs::compat::format(" LIMIT {}", query.limit);
    }

    if (query.offset > 0) {
        sql += pacs::compat::format(" OFFSET {}", query.offset);
    }

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<std::vector<study_record>>(
            database_query_error,
            pacs::compat::format("Failed to prepare query: {}", sqlite3_errmsg(db_)),
            "storage");
    }

    for (size_t i = 0; i < params.size(); ++i) {
        sqlite3_bind_text(stmt, static_cast<int>(i + 1), params[i].c_str(), -1,
                          SQLITE_TRANSIENT);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(parse_study_row(stmt));
    }

    sqlite3_finalize(stmt);
    return ok(std::move(results));
}

auto study_repository::delete_study(std::string_view study_uid) -> VoidResult {
    const char* sql = "DELETE FROM studies WHERE study_uid = ?;";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<std::monostate>(
            rc,
            pacs::compat::format("Failed to prepare delete: {}", sqlite3_errmsg(db_)),
            "storage");
    }

    sqlite3_bind_text(stmt, 1, study_uid.data(),
                      static_cast<int>(study_uid.size()), SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return make_error<std::monostate>(
            rc, pacs::compat::format("Failed to delete study: {}", sqlite3_errmsg(db_)),
            "storage");
    }

    return ok();
}

auto study_repository::study_count() const -> Result<size_t> {
    const char* sql = "SELECT COUNT(*) FROM studies;";

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

auto study_repository::study_count_for_patient(int64_t patient_pk) const
    -> Result<size_t> {
    const char* sql = R"(
        SELECT COUNT(*) FROM studies
        WHERE patient_pk = ?;
    )";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<size_t>(
            database_query_error,
            pacs::compat::format("Failed to prepare query: {}", sqlite3_errmsg(db_)),
            "storage");
    }

    sqlite3_bind_int64(stmt, 1, patient_pk);

    size_t count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
    }

    sqlite3_finalize(stmt);
    return ok(count);
}

auto study_repository::update_modalities_in_study(int64_t study_pk)
    -> VoidResult {
    const char* sql = R"(
        UPDATE studies
        SET modalities_in_study = (
            SELECT GROUP_CONCAT(modality, '\')
            FROM (
                SELECT DISTINCT modality FROM series
                WHERE study_pk = ? AND modality IS NOT NULL AND modality != ''
            )
        ),
        updated_at = datetime('now')
        WHERE study_pk = ?;
    )";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<std::monostate>(
            rc,
            pacs::compat::format("Failed to prepare update: {}", sqlite3_errmsg(db_)),
            "storage");
    }

    sqlite3_bind_int64(stmt, 1, study_pk);
    sqlite3_bind_int64(stmt, 2, study_pk);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return make_error<std::monostate>(
            rc,
            pacs::compat::format("Failed to update modalities: {}", sqlite3_errmsg(db_)),
            "storage");
    }

    return ok();
}

}  // namespace pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
