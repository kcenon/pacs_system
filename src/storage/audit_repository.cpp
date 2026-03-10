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
 * @file audit_repository.cpp
 * @brief Implementation of the audit repository
 *
 * @see Issue #915 - Extract audit lifecycle repository
 */

#include "pacs/storage/audit_repository.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#include <pacs/compat/format.hpp>
#include <pacs/compat/time.hpp>

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

audit_repository::audit_repository(std::shared_ptr<pacs_database_adapter> db)
    : base_repository(std::move(db), "audit_log", "audit_pk") {}

auto audit_repository::to_like_pattern(std::string_view pattern)
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

auto audit_repository::parse_timestamp(const std::string& str) const
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

auto audit_repository::format_timestamp(
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

auto audit_repository::add_audit_log(const audit_record& record)
    -> Result<int64_t> {
    if (!db() || !db()->is_connected()) {
        return make_error<int64_t>(-1, "Database not connected", "storage");
    }

    auto builder = query_builder();
    builder.insert_into(table_name())
        .values({{"event_type", record.event_type},
                 {"user_id", record.user_id},
                 {"source_ae", record.source_ae},
                 {"target_ae", record.target_ae},
                 {"source_ip", record.source_ip},
                 {"patient_id", record.patient_id},
                 {"study_uid", record.study_uid},
                 {"message", record.message},
                 {"details", record.details}});

    if (record.outcome.empty()) {
        builder.values({{"outcome", std::string("SUCCESS")}});
    } else {
        builder.values({{"outcome", record.outcome}});
    }

    if (record.timestamp != std::chrono::system_clock::time_point{}) {
        builder.values({{"timestamp", format_timestamp(record.timestamp)}});
    }

    auto insert_result = db()->insert(builder.build());
    if (insert_result.is_err()) {
        return make_error<int64_t>(
            -1,
            pacs::compat::format("Failed to insert audit log: {}",
                                 insert_result.error().message),
            "storage");
    }

    return db()->last_insert_rowid();
}

auto audit_repository::query_audit_log(const audit_query& query)
    -> Result<std::vector<audit_record>> {
    if (!db() || !db()->is_connected()) {
        return make_error<std::vector<audit_record>>(-1,
                                                     "Database not connected",
                                                     "storage");
    }

    auto builder = query_builder();
    builder.select(select_columns()).from(table_name());

    if (query.event_type.has_value()) {
        builder.where("event_type", "=", *query.event_type);
    }
    if (query.outcome.has_value()) {
        builder.where("outcome", "=", *query.outcome);
    }
    if (query.user_id.has_value()) {
        builder.where("user_id", "LIKE", to_like_pattern(*query.user_id));
    }
    if (query.source_ae.has_value()) {
        builder.where("source_ae", "=", *query.source_ae);
    }
    if (query.patient_id.has_value()) {
        builder.where("patient_id", "=", *query.patient_id);
    }
    if (query.study_uid.has_value()) {
        builder.where("study_uid", "=", *query.study_uid);
    }
    if (query.date_from.has_value()) {
        builder.where(database::query_condition(pacs::compat::format(
            "date(timestamp) >= date('{}')", *query.date_from)));
    }
    if (query.date_to.has_value()) {
        builder.where(database::query_condition(pacs::compat::format(
            "date(timestamp) <= date('{}')", *query.date_to)));
    }

    builder.order_by("timestamp", database::sort_order::desc);

    if (query.limit > 0) {
        builder.limit(query.limit);
    }
    if (query.offset > 0) {
        builder.offset(query.offset);
    }

    auto result = db()->select(builder.build());
    if (result.is_err()) {
        return Result<std::vector<audit_record>>::err(result.error());
    }

    std::vector<audit_record> items;
    items.reserve(result.value().size());
    for (const auto& row : result.value()) {
        items.push_back(map_row_to_entity(row));
    }

    return ok(std::move(items));
}

auto audit_repository::find_audit_by_pk(int64_t pk)
    -> std::optional<audit_record> {
    auto result = find_by_id(pk);
    if (result.is_err()) {
        return std::nullopt;
    }
    return result.value();
}

auto audit_repository::audit_count() -> Result<size_t> {
    return count();
}

auto audit_repository::cleanup_old_audit_logs(std::chrono::hours age)
    -> Result<size_t> {
    auto cutoff = std::chrono::system_clock::now() - age;
    auto cutoff_time = std::chrono::system_clock::to_time_t(cutoff);
    std::tm tm{};
    pacs::compat::gmtime_safe(&cutoff_time, &tm);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    auto cutoff_str = oss.str();

    if (!db() || !db()->is_connected()) {
        return make_error<size_t>(-1, "Database not connected", "storage");
    }

    auto builder = query_builder();
    builder.delete_from(table_name())
        .where(database::query_condition(
            pacs::compat::format("timestamp < '{}'", cutoff_str)));

    auto result = db()->remove(builder.build());
    if (result.is_err()) {
        return make_error<size_t>(
            -1,
            pacs::compat::format("Failed to cleanup old audit logs: {}",
                                 result.error().message),
            "storage");
    }

    return ok(static_cast<size_t>(result.value()));
}

auto audit_repository::map_row_to_entity(const database_row& row) const
    -> audit_record {
    audit_record record;
    record.pk = get_int64(row, "audit_pk");
    record.event_type = get_string(row, "event_type");
    record.outcome = get_string(row, "outcome");

    auto timestamp = get_string(row, "timestamp");
    if (!timestamp.empty()) {
        record.timestamp = parse_timestamp(timestamp);
    }

    record.user_id = get_string(row, "user_id");
    record.source_ae = get_string(row, "source_ae");
    record.target_ae = get_string(row, "target_ae");
    record.source_ip = get_string(row, "source_ip");
    record.patient_id = get_string(row, "patient_id");
    record.study_uid = get_string(row, "study_uid");
    record.message = get_string(row, "message");
    record.details = get_string(row, "details");
    return record;
}

auto audit_repository::entity_to_row(const audit_record& entity) const
    -> std::map<std::string, database_value> {
    return {
        {"event_type", entity.event_type},
        {"outcome", entity.outcome},
        {"timestamp", format_timestamp(entity.timestamp)},
        {"user_id", entity.user_id},
        {"source_ae", entity.source_ae},
        {"target_ae", entity.target_ae},
        {"source_ip", entity.source_ip},
        {"patient_id", entity.patient_id},
        {"study_uid", entity.study_uid},
        {"message", entity.message},
        {"details", entity.details},
    };
}

auto audit_repository::get_pk(const audit_record& entity) const -> int64_t {
    return entity.pk;
}

auto audit_repository::has_pk(const audit_record& entity) const -> bool {
    return entity.pk > 0;
}

auto audit_repository::select_columns() const -> std::vector<std::string> {
    return {"audit_pk",  "event_type", "outcome",   "timestamp",
            "user_id",   "source_ae",  "target_ae", "source_ip",
            "patient_id","study_uid",  "message",   "details"};
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

audit_repository::audit_repository(sqlite3* db) : db_(db) {}

audit_repository::~audit_repository() = default;

audit_repository::audit_repository(audit_repository&& other) noexcept
    : db_(other.db_) {
    other.db_ = nullptr;
}

auto audit_repository::operator=(audit_repository&& other) noexcept
    -> audit_repository& {
    if (this != &other) {
        db_ = other.db_;
        other.db_ = nullptr;
    }
    return *this;
}

auto audit_repository::to_like_pattern(std::string_view pattern)
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

auto audit_repository::parse_timestamp(const std::string& str)
    -> std::chrono::system_clock::time_point {
    return parse_datetime(str.c_str());
}

auto audit_repository::parse_audit_row(void* stmt_ptr) const -> audit_record {
    auto* stmt = static_cast<sqlite3_stmt*>(stmt_ptr);
    audit_record record;

    record.pk = sqlite3_column_int64(stmt, 0);
    record.event_type = get_text(stmt, 1);
    record.outcome = get_text(stmt, 2);
    record.timestamp = parse_datetime(get_text(stmt, 3).c_str());
    record.user_id = get_text(stmt, 4);
    record.source_ae = get_text(stmt, 5);
    record.target_ae = get_text(stmt, 6);
    record.source_ip = get_text(stmt, 7);
    record.patient_id = get_text(stmt, 8);
    record.study_uid = get_text(stmt, 9);
    record.message = get_text(stmt, 10);
    record.details = get_text(stmt, 11);
    return record;
}

auto audit_repository::add_audit_log(const audit_record& record)
    -> Result<int64_t> {
    const char* sql = R"(
        INSERT INTO audit_log (
            event_type, outcome, user_id, source_ae, target_ae,
            source_ip, patient_id, study_uid, message, details
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
    )";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<int64_t>(
            rc,
            pacs::compat::format("Failed to prepare audit insert: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    sqlite3_bind_text(stmt, 1, record.event_type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, record.outcome.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, record.user_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, record.source_ae.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, record.target_ae.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, record.source_ip.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, record.patient_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, record.study_uid.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 9, record.message.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 10, record.details.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return make_error<int64_t>(
            rc,
            pacs::compat::format("Failed to insert audit log: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    return sqlite3_last_insert_rowid(db_);
}

auto audit_repository::query_audit_log(const audit_query& query) const
    -> Result<std::vector<audit_record>> {
    std::vector<audit_record> results;
    std::ostringstream sql;
    sql << "SELECT audit_pk, event_type, outcome, timestamp, user_id, "
        << "source_ae, target_ae, source_ip, patient_id, study_uid, "
        << "message, details FROM audit_log WHERE 1=1";

    std::vector<std::string> params;

    if (query.event_type) {
        sql << " AND event_type = ?";
        params.push_back(*query.event_type);
    }
    if (query.outcome) {
        sql << " AND outcome = ?";
        params.push_back(*query.outcome);
    }
    if (query.user_id) {
        sql << " AND user_id LIKE ?";
        params.push_back(to_like_pattern(*query.user_id));
    }
    if (query.source_ae) {
        sql << " AND source_ae = ?";
        params.push_back(*query.source_ae);
    }
    if (query.patient_id) {
        sql << " AND patient_id = ?";
        params.push_back(*query.patient_id);
    }
    if (query.study_uid) {
        sql << " AND study_uid = ?";
        params.push_back(*query.study_uid);
    }
    if (query.date_from) {
        sql << " AND date(timestamp) >= date(?)";
        params.push_back(*query.date_from);
    }
    if (query.date_to) {
        sql << " AND date(timestamp) <= date(?)";
        params.push_back(*query.date_to);
    }

    sql << " ORDER BY timestamp DESC";

    if (query.limit > 0) {
        sql << " LIMIT " << query.limit;
    }
    if (query.offset > 0) {
        sql << " OFFSET " << query.offset;
    }
    sql << ";";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql.str().c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<std::vector<audit_record>>(
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
        results.push_back(parse_audit_row(stmt));
    }

    sqlite3_finalize(stmt);
    return ok(std::move(results));
}

auto audit_repository::find_audit_by_pk(int64_t pk) const
    -> std::optional<audit_record> {
    const char* sql =
        "SELECT audit_pk, event_type, outcome, timestamp, user_id, "
        "source_ae, target_ae, source_ip, patient_id, study_uid, "
        "message, details FROM audit_log WHERE audit_pk = ?;";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_int64(stmt, 1, pk);

    std::optional<audit_record> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = parse_audit_row(stmt);
    }

    sqlite3_finalize(stmt);
    return result;
}

auto audit_repository::audit_count() const -> Result<size_t> {
    const char* sql = "SELECT COUNT(*) FROM audit_log;";
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

auto audit_repository::cleanup_old_audit_logs(std::chrono::hours age)
    -> Result<size_t> {
    auto cutoff = std::chrono::system_clock::now() - age;
    auto cutoff_time = std::chrono::system_clock::to_time_t(cutoff);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&cutoff_time), "%Y-%m-%d %H:%M:%S");
    auto cutoff_str = oss.str();

    const char* sql = "DELETE FROM audit_log WHERE timestamp < ?;";
    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<size_t>(
            rc,
            pacs::compat::format("Failed to prepare audit cleanup: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    sqlite3_bind_text(stmt, 1, cutoff_str.c_str(), -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return make_error<size_t>(
            rc,
            pacs::compat::format("Failed to cleanup old audit logs: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    return static_cast<size_t>(sqlite3_changes(db_));
}

}  // namespace pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
