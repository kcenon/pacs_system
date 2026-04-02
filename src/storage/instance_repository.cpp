// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file instance_repository.cpp
 * @brief Implementation of the instance repository
 *
 * @see Issue #896 - Decompose index_database into metadata and lifecycle stores
 * @see Issue #913 - Part 2: Extract series and instance metadata repositories
 */

#include "pacs/storage/instance_repository.hpp"

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

instance_repository::instance_repository(
    std::shared_ptr<pacs_database_adapter> db)
    : base_repository(std::move(db), "instances", "instance_pk") {}

auto instance_repository::parse_timestamp(const std::string& str) const
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

auto instance_repository::format_timestamp(
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

auto instance_repository::upsert_instance(int64_t series_pk,
                                          std::string_view sop_uid,
                                          std::string_view sop_class_uid,
                                          std::string_view file_path,
                                          int64_t file_size,
                                          std::string_view transfer_syntax,
                                          std::optional<int> instance_number)
    -> Result<int64_t> {
    instance_record record;
    record.series_pk = series_pk;
    record.sop_uid = std::string(sop_uid);
    record.sop_class_uid = std::string(sop_class_uid);
    record.file_path = std::string(file_path);
    record.file_size = file_size;
    record.transfer_syntax = std::string(transfer_syntax);
    record.instance_number = instance_number;
    return upsert_instance(record);
}

auto instance_repository::upsert_instance(const instance_record& record)
    -> Result<int64_t> {
    if (record.sop_uid.empty()) {
        return make_error<int64_t>(-1, "SOP Instance UID is required",
                                   "storage");
    }

    if (record.sop_uid.length() > 64) {
        return make_error<int64_t>(
            -1, "SOP Instance UID exceeds maximum length of 64 characters",
            "storage");
    }

    if (record.series_pk <= 0) {
        return make_error<int64_t>(-1, "Valid series_pk is required",
                                   "storage");
    }

    if (record.file_path.empty()) {
        return make_error<int64_t>(-1, "File path is required", "storage");
    }

    if (record.file_size < 0) {
        return make_error<int64_t>(-1, "File size must be non-negative",
                                   "storage");
    }

    if (!db() || !db()->is_connected()) {
        return make_error<int64_t>(-1, "Database not connected", "storage");
    }

    auto builder = query_builder();
    auto check_sql = builder.select(std::vector<std::string>{"instance_pk"})
                         .from("instances")
                         .where("sop_uid", "=", record.sop_uid)
                         .build();

    auto check_result = db()->select(check_sql);
    if (check_result.is_err()) {
        return make_error<int64_t>(
            -1,
            kcenon::pacs::compat::format("Failed to check instance existence: {}",
                                 check_result.error().message),
            "storage");
    }

    if (!check_result.value().empty()) {
        auto existing = find_instance(record.sop_uid);
        if (!existing.has_value()) {
            return make_error<int64_t>(
                -1, "Instance exists but could not retrieve record", "storage");
        }

        std::map<std::string, database::core::database_value> update_data{
            {"series_pk", std::to_string(record.series_pk)},
            {"sop_class_uid", record.sop_class_uid},
            {"transfer_syntax", record.transfer_syntax},
            {"content_date", record.content_date},
            {"content_time", record.content_time},
            {"file_path", record.file_path},
            {"file_size", std::to_string(record.file_size)},
            {"file_hash", record.file_hash}};
        if (record.instance_number.has_value()) {
            update_data["instance_number"] =
                std::to_string(*record.instance_number);
        }
        if (record.rows.has_value()) {
            update_data["rows"] = std::to_string(*record.rows);
        }
        if (record.columns.has_value()) {
            update_data["columns"] = std::to_string(*record.columns);
        }
        if (record.bits_allocated.has_value()) {
            update_data["bits_allocated"] =
                std::to_string(*record.bits_allocated);
        }
        if (record.number_of_frames.has_value()) {
            update_data["number_of_frames"] =
                std::to_string(*record.number_of_frames);
        }

        database::query_builder update_builder(database::database_types::sqlite);
        auto update_sql = update_builder.update("instances")
                              .set(update_data)
                              .where("sop_uid", "=", record.sop_uid)
                              .build();
        auto update_result = db()->update(update_sql);
        if (update_result.is_err()) {
            return make_error<int64_t>(
                -1,
                kcenon::pacs::compat::format("Failed to update instance: {}",
                                     update_result.error().message),
                "storage");
        }

        return existing->pk;
    }

    std::map<std::string, database::core::database_value> insert_data{
        {"series_pk", std::to_string(record.series_pk)},
        {"sop_uid", record.sop_uid},
        {"sop_class_uid", record.sop_class_uid},
        {"transfer_syntax", record.transfer_syntax},
        {"content_date", record.content_date},
        {"content_time", record.content_time},
        {"file_path", record.file_path},
        {"file_size", std::to_string(record.file_size)},
        {"file_hash", record.file_hash}};
    if (record.instance_number.has_value()) {
        insert_data["instance_number"] =
            std::to_string(*record.instance_number);
    }
    if (record.rows.has_value()) {
        insert_data["rows"] = std::to_string(*record.rows);
    }
    if (record.columns.has_value()) {
        insert_data["columns"] = std::to_string(*record.columns);
    }
    if (record.bits_allocated.has_value()) {
        insert_data["bits_allocated"] =
            std::to_string(*record.bits_allocated);
    }
    if (record.number_of_frames.has_value()) {
        insert_data["number_of_frames"] =
            std::to_string(*record.number_of_frames);
    }

    database::query_builder insert_builder(database::database_types::sqlite);
    insert_builder.insert_into("instances").values(insert_data);

    auto insert_result = db()->insert(insert_builder.build());
    if (insert_result.is_err()) {
        return make_error<int64_t>(
            -1,
            kcenon::pacs::compat::format("Failed to insert instance: {}",
                                 insert_result.error().message),
            "storage");
    }

    auto inserted = find_instance(record.sop_uid);
    if (!inserted.has_value()) {
        return make_error<int64_t>(
            -1, "Instance inserted but could not retrieve record", "storage");
    }

    return inserted->pk;
}

auto instance_repository::find_instance(std::string_view sop_uid)
    -> std::optional<instance_record> {
    if (!db() || !db()->is_connected()) {
        return std::nullopt;
    }

    auto builder = query_builder();
    auto select_sql =
        builder.select(select_columns())
            .from(table_name())
            .where("sop_uid", "=", std::string(sop_uid))
            .build();

    auto result = db()->select(select_sql);
    if (result.is_err() || result.value().empty()) {
        return std::nullopt;
    }

    return map_row_to_entity(result.value()[0]);
}

auto instance_repository::find_instance_by_pk(int64_t pk)
    -> std::optional<instance_record> {
    auto result = find_by_id(pk);
    if (result.is_err()) {
        return std::nullopt;
    }
    return result.value();
}

auto instance_repository::list_instances(std::string_view series_uid)
    -> Result<std::vector<instance_record>> {
    if (!db() || !db()->is_connected()) {
        return make_error<std::vector<instance_record>>(
            -1, "Database not connected", "storage");
    }

    auto sql = kcenon::pacs::compat::format(
        "SELECT i.instance_pk, i.series_pk, i.sop_uid, i.sop_class_uid, "
        "i.instance_number, i.transfer_syntax, i.content_date, i.content_time, "
        "i.rows, i.columns, i.bits_allocated, i.number_of_frames, "
        "i.file_path, i.file_size, i.file_hash, i.created_at "
        "FROM instances i "
        "JOIN series s ON i.series_pk = s.series_pk "
        "WHERE s.series_uid = '{}' "
        "ORDER BY i.instance_number ASC, i.sop_uid ASC;",
        std::string(series_uid));

    auto result = db()->select(sql);
    if (result.is_err()) {
        return make_error<std::vector<instance_record>>(
            -1,
            kcenon::pacs::compat::format("Failed to list instances: {}",
                                 result.error().message),
            "storage");
    }

    std::vector<instance_record> records;
    records.reserve(result.value().size());
    for (const auto& row : result.value()) {
        records.push_back(map_row_to_entity(row));
    }

    return ok(std::move(records));
}

auto instance_repository::search_instances(const instance_query& query)
    -> Result<std::vector<instance_record>> {
    if (!db() || !db()->is_connected()) {
        return make_error<std::vector<instance_record>>(
            -1, "Database not connected", "storage");
    }

    std::vector<std::string> where_clauses;

    if (query.series_uid.has_value()) {
        where_clauses.push_back(
            kcenon::pacs::compat::format("s.series_uid = '{}'", *query.series_uid));
    }
    if (query.sop_uid.has_value()) {
        where_clauses.push_back(
            kcenon::pacs::compat::format("i.sop_uid = '{}'", *query.sop_uid));
    }
    if (query.sop_class_uid.has_value()) {
        where_clauses.push_back(
            kcenon::pacs::compat::format("i.sop_class_uid = '{}'",
                                 *query.sop_class_uid));
    }
    if (query.content_date.has_value()) {
        where_clauses.push_back(
            kcenon::pacs::compat::format("i.content_date = '{}'", *query.content_date));
    }
    if (query.content_date_from.has_value()) {
        where_clauses.push_back(kcenon::pacs::compat::format(
            "i.content_date >= '{}'", *query.content_date_from));
    }
    if (query.content_date_to.has_value()) {
        where_clauses.push_back(
            kcenon::pacs::compat::format("i.content_date <= '{}'",
                                 *query.content_date_to));
    }
    if (query.instance_number.has_value()) {
        where_clauses.push_back(kcenon::pacs::compat::format(
            "i.instance_number = {}", *query.instance_number));
    }

    std::string sql =
        "SELECT i.instance_pk, i.series_pk, i.sop_uid, i.sop_class_uid, "
        "i.instance_number, i.transfer_syntax, i.content_date, i.content_time, "
        "i.rows, i.columns, i.bits_allocated, i.number_of_frames, "
        "i.file_path, i.file_size, i.file_hash, i.created_at "
        "FROM instances i "
        "JOIN series s ON i.series_pk = s.series_pk";

    if (!where_clauses.empty()) {
        sql += " WHERE " + where_clauses.front();
        for (size_t i = 1; i < where_clauses.size(); ++i) {
            sql += " AND " + where_clauses[i];
        }
    }

    sql += " ORDER BY i.instance_number ASC, i.sop_uid ASC";

    if (query.limit > 0) {
        sql += kcenon::pacs::compat::format(" LIMIT {}", query.limit);
    }
    if (query.offset > 0) {
        sql += kcenon::pacs::compat::format(" OFFSET {}", query.offset);
    }

    auto result = db()->select(sql);
    if (result.is_err()) {
        return make_error<std::vector<instance_record>>(
            -1,
            kcenon::pacs::compat::format("Failed to search instances: {}",
                                 result.error().message),
            "storage");
    }

    std::vector<instance_record> records;
    records.reserve(result.value().size());
    for (const auto& row : result.value()) {
        records.push_back(map_row_to_entity(row));
    }

    return ok(std::move(records));
}

auto instance_repository::delete_instance(std::string_view sop_uid)
    -> VoidResult {
    if (!db() || !db()->is_connected()) {
        return make_error<std::monostate>(-1, "Database not connected",
                                          "storage");
    }

    auto builder = query_builder();
    auto delete_sql =
        builder.delete_from(table_name())
            .where("sop_uid", "=", std::string(sop_uid))
            .build();

    auto result = db()->remove(delete_sql);
    if (result.is_err()) {
        return make_error<std::monostate>(
            -1,
            kcenon::pacs::compat::format("Failed to delete instance: {}",
                                 result.error().message),
            "storage");
    }

    return ok();
}

auto instance_repository::instance_count() -> Result<size_t> {
    return count();
}

auto instance_repository::instance_count(std::string_view series_uid)
    -> Result<size_t> {
    if (!db() || !db()->is_connected()) {
        return make_error<size_t>(-1, "Database not connected", "storage");
    }

    auto sql = kcenon::pacs::compat::format(
        "SELECT COUNT(*) AS cnt "
        "FROM instances i "
        "JOIN series s ON i.series_pk = s.series_pk "
        "WHERE s.series_uid = '{}';",
        std::string(series_uid));

    auto result = db()->select(sql);
    if (result.is_err()) {
        return make_error<size_t>(
            -1,
            kcenon::pacs::compat::format("Failed to count instances: {}",
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

auto instance_repository::get_file_path(std::string_view sop_instance_uid)
    -> Result<std::optional<std::string>> {
    if (!db() || !db()->is_connected()) {
        return make_error<std::optional<std::string>>(
            -1, "Database not connected", "storage");
    }

    auto builder = query_builder();
    auto select_sql =
        builder.select(std::vector<std::string>{"file_path"})
            .from(table_name())
            .where("sop_uid", "=", std::string(sop_instance_uid))
            .build();

    auto result = db()->select(select_sql);
    if (result.is_err() || result.value().empty()) {
        return ok(std::optional<std::string>(std::nullopt));
    }

    const auto& row = result.value()[0];
    auto it = row.find("file_path");
    if (it == row.end() || it->second.empty()) {
        return ok(std::optional<std::string>(std::nullopt));
    }

    return ok(std::optional<std::string>(it->second));
}

auto instance_repository::get_study_files(std::string_view study_instance_uid)
    -> Result<std::vector<std::string>> {
    if (!db() || !db()->is_connected()) {
        return make_error<std::vector<std::string>>(
            -1, "Database not connected", "storage");
    }

    auto sql = kcenon::pacs::compat::format(
        "SELECT i.file_path "
        "FROM instances i "
        "JOIN series se ON i.series_pk = se.series_pk "
        "JOIN studies st ON se.study_pk = st.study_pk "
        "WHERE st.study_uid = '{}' "
        "ORDER BY se.series_number, i.instance_number;",
        std::string(study_instance_uid));

    auto result = db()->select(sql);
    if (result.is_err()) {
        return make_error<std::vector<std::string>>(
            -1,
            kcenon::pacs::compat::format("Failed to query study files: {}",
                                 result.error().message),
            "storage");
    }

    std::vector<std::string> files;
    files.reserve(result.value().size());
    for (const auto& row : result.value()) {
        auto it = row.find("file_path");
        if (it != row.end()) {
            files.push_back(it->second);
        }
    }

    return ok(std::move(files));
}

auto instance_repository::get_series_files(std::string_view series_instance_uid)
    -> Result<std::vector<std::string>> {
    if (!db() || !db()->is_connected()) {
        return make_error<std::vector<std::string>>(
            -1, "Database not connected", "storage");
    }

    auto sql = kcenon::pacs::compat::format(
        "SELECT i.file_path "
        "FROM instances i "
        "JOIN series se ON i.series_pk = se.series_pk "
        "WHERE se.series_uid = '{}' "
        "ORDER BY i.instance_number;",
        std::string(series_instance_uid));

    auto result = db()->select(sql);
    if (result.is_err()) {
        return make_error<std::vector<std::string>>(
            -1,
            kcenon::pacs::compat::format("Failed to query series files: {}",
                                 result.error().message),
            "storage");
    }

    std::vector<std::string> files;
    files.reserve(result.value().size());
    for (const auto& row : result.value()) {
        auto it = row.find("file_path");
        if (it != row.end()) {
            files.push_back(it->second);
        }
    }

    return ok(std::move(files));
}

auto instance_repository::map_row_to_entity(const database_row& row) const
    -> instance_record {
    instance_record record;

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

    record.pk = get_int64("instance_pk");
    record.series_pk = get_int64("series_pk");
    record.sop_uid = get_str("sop_uid");
    record.sop_class_uid = get_str("sop_class_uid");
    record.instance_number = get_optional_int("instance_number");
    record.transfer_syntax = get_str("transfer_syntax");
    record.content_date = get_str("content_date");
    record.content_time = get_str("content_time");
    record.rows = get_optional_int("rows");
    record.columns = get_optional_int("columns");
    record.bits_allocated = get_optional_int("bits_allocated");
    record.number_of_frames = get_optional_int("number_of_frames");
    record.file_path = get_str("file_path");
    record.file_size = get_int64("file_size");
    record.file_hash = get_str("file_hash");

    auto created_at_str = get_str("created_at");
    if (!created_at_str.empty()) {
        record.created_at = parse_timestamp(created_at_str);
    }

    return record;
}

auto instance_repository::entity_to_row(const instance_record& entity) const
    -> std::map<std::string, database_value> {
    std::map<std::string, database_value> row;

    row["series_pk"] = std::to_string(entity.series_pk);
    row["sop_uid"] = entity.sop_uid;
    row["sop_class_uid"] = entity.sop_class_uid;
    if (entity.instance_number.has_value()) {
        row["instance_number"] = std::to_string(*entity.instance_number);
    }
    row["transfer_syntax"] = entity.transfer_syntax;
    row["content_date"] = entity.content_date;
    row["content_time"] = entity.content_time;
    if (entity.rows.has_value()) {
        row["rows"] = std::to_string(*entity.rows);
    }
    if (entity.columns.has_value()) {
        row["columns"] = std::to_string(*entity.columns);
    }
    if (entity.bits_allocated.has_value()) {
        row["bits_allocated"] = std::to_string(*entity.bits_allocated);
    }
    if (entity.number_of_frames.has_value()) {
        row["number_of_frames"] = std::to_string(*entity.number_of_frames);
    }
    row["file_path"] = entity.file_path;
    row["file_size"] = std::to_string(entity.file_size);
    row["file_hash"] = entity.file_hash;

    auto now = std::chrono::system_clock::now();
    if (entity.created_at != std::chrono::system_clock::time_point{}) {
        row["created_at"] = format_timestamp(entity.created_at);
    } else {
        row["created_at"] = format_timestamp(now);
    }

    return row;
}

auto instance_repository::get_pk(const instance_record& entity) const
    -> int64_t {
    return entity.pk;
}

auto instance_repository::has_pk(const instance_record& entity) const -> bool {
    return entity.pk > 0;
}

auto instance_repository::select_columns() const -> std::vector<std::string> {
    return {"instance_pk",      "series_pk",      "sop_uid",
            "sop_class_uid",    "instance_number","transfer_syntax",
            "content_date",     "content_time",   "rows",
            "columns",          "bits_allocated", "number_of_frames",
            "file_path",        "file_size",      "file_hash",
            "created_at"};
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

instance_repository::instance_repository(sqlite3* db) : db_(db) {}

instance_repository::~instance_repository() = default;

instance_repository::instance_repository(instance_repository&&) noexcept = default;

auto instance_repository::operator=(instance_repository&&) noexcept
    -> instance_repository& = default;

auto instance_repository::parse_timestamp(const std::string& str)
    -> std::chrono::system_clock::time_point {
    return parse_datetime(str.c_str());
}

auto instance_repository::parse_instance_row(void* stmt_ptr) const
    -> instance_record {
    auto* stmt = static_cast<sqlite3_stmt*>(stmt_ptr);
    instance_record record;

    record.pk = sqlite3_column_int64(stmt, 0);
    record.series_pk = sqlite3_column_int64(stmt, 1);
    record.sop_uid = get_text(stmt, 2);
    record.sop_class_uid = get_text(stmt, 3);

    if (sqlite3_column_type(stmt, 4) != SQLITE_NULL) {
        record.instance_number = sqlite3_column_int(stmt, 4);
    }

    record.transfer_syntax = get_text(stmt, 5);
    record.content_date = get_text(stmt, 6);
    record.content_time = get_text(stmt, 7);

    if (sqlite3_column_type(stmt, 8) != SQLITE_NULL) {
        record.rows = sqlite3_column_int(stmt, 8);
    }
    if (sqlite3_column_type(stmt, 9) != SQLITE_NULL) {
        record.columns = sqlite3_column_int(stmt, 9);
    }
    if (sqlite3_column_type(stmt, 10) != SQLITE_NULL) {
        record.bits_allocated = sqlite3_column_int(stmt, 10);
    }
    if (sqlite3_column_type(stmt, 11) != SQLITE_NULL) {
        record.number_of_frames = sqlite3_column_int(stmt, 11);
    }

    record.file_path = get_text(stmt, 12);
    record.file_size = sqlite3_column_int64(stmt, 13);
    record.file_hash = get_text(stmt, 14);
    record.created_at = parse_datetime(get_text(stmt, 15).c_str());

    return record;
}

auto instance_repository::upsert_instance(int64_t series_pk,
                                          std::string_view sop_uid,
                                          std::string_view sop_class_uid,
                                          std::string_view file_path,
                                          int64_t file_size,
                                          std::string_view transfer_syntax,
                                          std::optional<int> instance_number)
    -> Result<int64_t> {
    instance_record record;
    record.series_pk = series_pk;
    record.sop_uid = std::string(sop_uid);
    record.sop_class_uid = std::string(sop_class_uid);
    record.file_path = std::string(file_path);
    record.file_size = file_size;
    record.transfer_syntax = std::string(transfer_syntax);
    record.instance_number = instance_number;
    return upsert_instance(record);
}

auto instance_repository::upsert_instance(const instance_record& record)
    -> Result<int64_t> {
    if (record.sop_uid.empty()) {
        return make_error<int64_t>(-1, "SOP Instance UID is required",
                                   "storage");
    }

    if (record.sop_uid.length() > 64) {
        return make_error<int64_t>(
            -1, "SOP Instance UID exceeds maximum length of 64 characters",
            "storage");
    }

    if (record.series_pk <= 0) {
        return make_error<int64_t>(-1, "Valid series_pk is required",
                                   "storage");
    }

    if (record.file_path.empty()) {
        return make_error<int64_t>(-1, "File path is required", "storage");
    }

    if (record.file_size < 0) {
        return make_error<int64_t>(-1, "File size must be non-negative",
                                   "storage");
    }

    const char* sql = R"(
        INSERT INTO instances (
            series_pk, sop_uid, sop_class_uid, instance_number,
            transfer_syntax, content_date, content_time,
            rows, columns, bits_allocated, number_of_frames,
            file_path, file_size, file_hash
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(sop_uid) DO UPDATE SET
            series_pk = excluded.series_pk,
            sop_class_uid = excluded.sop_class_uid,
            instance_number = excluded.instance_number,
            transfer_syntax = excluded.transfer_syntax,
            content_date = excluded.content_date,
            content_time = excluded.content_time,
            rows = excluded.rows,
            columns = excluded.columns,
            bits_allocated = excluded.bits_allocated,
            number_of_frames = excluded.number_of_frames,
            file_path = excluded.file_path,
            file_size = excluded.file_size,
            file_hash = excluded.file_hash
        RETURNING instance_pk;
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

    sqlite3_bind_int64(stmt, 1, record.series_pk);
    sqlite3_bind_text(stmt, 2, record.sop_uid.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, record.sop_class_uid.c_str(), -1,
                      SQLITE_TRANSIENT);

    if (record.instance_number.has_value()) {
        sqlite3_bind_int(stmt, 4, *record.instance_number);
    } else {
        sqlite3_bind_null(stmt, 4);
    }

    sqlite3_bind_text(stmt, 5, record.transfer_syntax.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, record.content_date.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, record.content_time.c_str(), -1,
                      SQLITE_TRANSIENT);

    if (record.rows.has_value()) {
        sqlite3_bind_int(stmt, 8, *record.rows);
    } else {
        sqlite3_bind_null(stmt, 8);
    }
    if (record.columns.has_value()) {
        sqlite3_bind_int(stmt, 9, *record.columns);
    } else {
        sqlite3_bind_null(stmt, 9);
    }
    if (record.bits_allocated.has_value()) {
        sqlite3_bind_int(stmt, 10, *record.bits_allocated);
    } else {
        sqlite3_bind_null(stmt, 10);
    }
    if (record.number_of_frames.has_value()) {
        sqlite3_bind_int(stmt, 11, *record.number_of_frames);
    } else {
        sqlite3_bind_null(stmt, 11);
    }

    sqlite3_bind_text(stmt, 12, record.file_path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 13, record.file_size);
    sqlite3_bind_text(stmt, 14, record.file_hash.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        auto error_msg = sqlite3_errmsg(db_);
        sqlite3_finalize(stmt);
        return make_error<int64_t>(
            rc,
            kcenon::pacs::compat::format("Failed to upsert instance: {}", error_msg),
            "storage");
    }

    auto pk = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    return pk;
}

auto instance_repository::find_instance(std::string_view sop_uid) const
    -> std::optional<instance_record> {
    const char* sql = R"(
        SELECT instance_pk, series_pk, sop_uid, sop_class_uid, instance_number,
               transfer_syntax, content_date, content_time,
               rows, columns, bits_allocated, number_of_frames,
               file_path, file_size, file_hash, created_at
        FROM instances
        WHERE sop_uid = ?;
    )";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, sop_uid.data(),
                      static_cast<int>(sop_uid.size()), SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }

    auto record = parse_instance_row(stmt);
    sqlite3_finalize(stmt);
    return record;
}

auto instance_repository::find_instance_by_pk(int64_t pk) const
    -> std::optional<instance_record> {
    const char* sql = R"(
        SELECT instance_pk, series_pk, sop_uid, sop_class_uid, instance_number,
               transfer_syntax, content_date, content_time,
               rows, columns, bits_allocated, number_of_frames,
               file_path, file_size, file_hash, created_at
        FROM instances
        WHERE instance_pk = ?;
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

    auto record = parse_instance_row(stmt);
    sqlite3_finalize(stmt);
    return record;
}

auto instance_repository::list_instances(std::string_view series_uid) const
    -> Result<std::vector<instance_record>> {
    std::vector<instance_record> results;

    const char* sql = R"(
        SELECT i.instance_pk, i.series_pk, i.sop_uid, i.sop_class_uid,
               i.instance_number, i.transfer_syntax, i.content_date,
               i.content_time, i.rows, i.columns, i.bits_allocated,
               i.number_of_frames, i.file_path, i.file_size, i.file_hash,
               i.created_at
        FROM instances i
        JOIN series s ON i.series_pk = s.series_pk
        WHERE s.series_uid = ?
        ORDER BY i.instance_number ASC, i.sop_uid ASC;
    )";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<std::vector<instance_record>>(
            database_query_error,
            kcenon::pacs::compat::format("Failed to prepare query: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    sqlite3_bind_text(stmt, 1, series_uid.data(),
                      static_cast<int>(series_uid.size()), SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(parse_instance_row(stmt));
    }

    sqlite3_finalize(stmt);
    return ok(std::move(results));
}

auto instance_repository::search_instances(const instance_query& query) const
    -> Result<std::vector<instance_record>> {
    std::vector<instance_record> results;

    std::string sql = R"(
        SELECT i.instance_pk, i.series_pk, i.sop_uid, i.sop_class_uid,
               i.instance_number, i.transfer_syntax, i.content_date,
               i.content_time, i.rows, i.columns, i.bits_allocated,
               i.number_of_frames, i.file_path, i.file_size, i.file_hash,
               i.created_at
        FROM instances i
        JOIN series s ON i.series_pk = s.series_pk
        WHERE 1=1
    )";

    std::vector<std::string> params;

    if (query.series_uid.has_value()) {
        sql += " AND s.series_uid = ?";
        params.push_back(*query.series_uid);
    }
    if (query.sop_uid.has_value()) {
        sql += " AND i.sop_uid = ?";
        params.push_back(*query.sop_uid);
    }
    if (query.sop_class_uid.has_value()) {
        sql += " AND i.sop_class_uid = ?";
        params.push_back(*query.sop_class_uid);
    }
    if (query.content_date.has_value()) {
        sql += " AND i.content_date = ?";
        params.push_back(*query.content_date);
    }
    if (query.content_date_from.has_value()) {
        sql += " AND i.content_date >= ?";
        params.push_back(*query.content_date_from);
    }
    if (query.content_date_to.has_value()) {
        sql += " AND i.content_date <= ?";
        params.push_back(*query.content_date_to);
    }

    sql += " ORDER BY i.instance_number ASC, i.sop_uid ASC";

    if (query.limit > 0) {
        sql += kcenon::pacs::compat::format(" LIMIT {}", query.limit);
    }
    if (query.offset > 0) {
        sql += kcenon::pacs::compat::format(" OFFSET {}", query.offset);
    }

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<std::vector<instance_record>>(
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
        auto record = parse_instance_row(stmt);
        if (query.instance_number.has_value()) {
            if (!record.instance_number.has_value() ||
                *record.instance_number != *query.instance_number) {
                continue;
            }
        }
        results.push_back(std::move(record));
    }

    sqlite3_finalize(stmt);
    return ok(std::move(results));
}

auto instance_repository::delete_instance(std::string_view sop_uid)
    -> VoidResult {
    const char* sql = "DELETE FROM instances WHERE sop_uid = ?;";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<std::monostate>(
            rc,
            kcenon::pacs::compat::format("Failed to prepare delete: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    sqlite3_bind_text(stmt, 1, sop_uid.data(),
                      static_cast<int>(sop_uid.size()), SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return make_error<std::monostate>(
            rc,
            kcenon::pacs::compat::format("Failed to delete instance: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    return ok();
}

auto instance_repository::instance_count() const -> Result<size_t> {
    const char* sql = "SELECT COUNT(*) FROM instances;";

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

auto instance_repository::instance_count(std::string_view series_uid) const
    -> Result<size_t> {
    const char* sql = R"(
        SELECT COUNT(*) FROM instances i
        JOIN series s ON i.series_pk = s.series_pk
        WHERE s.series_uid = ?;
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

    sqlite3_bind_text(stmt, 1, series_uid.data(),
                      static_cast<int>(series_uid.size()), SQLITE_TRANSIENT);

    size_t count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
    }

    sqlite3_finalize(stmt);
    return ok(count);
}

auto instance_repository::get_file_path(std::string_view sop_instance_uid) const
    -> Result<std::optional<std::string>> {
    const char* sql = "SELECT file_path FROM instances WHERE sop_uid = ?;";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<std::optional<std::string>>(
            database_query_error,
            kcenon::pacs::compat::format("Failed to prepare query: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    sqlite3_bind_text(stmt, 1, sop_instance_uid.data(),
                      static_cast<int>(sop_instance_uid.size()),
                      SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return ok(std::optional<std::string>(std::nullopt));
    }

    auto path = get_text(stmt, 0);
    sqlite3_finalize(stmt);
    return ok(std::optional<std::string>(path));
}

auto instance_repository::get_study_files(std::string_view study_instance_uid) const
    -> Result<std::vector<std::string>> {
    std::vector<std::string> results;

    const char* sql = R"(
        SELECT i.file_path
        FROM instances i
        JOIN series se ON i.series_pk = se.series_pk
        JOIN studies st ON se.study_pk = st.study_pk
        WHERE st.study_uid = ?
        ORDER BY se.series_number, i.instance_number;
    )";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<std::vector<std::string>>(
            database_query_error,
            kcenon::pacs::compat::format("Failed to prepare query: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    sqlite3_bind_text(stmt, 1, study_instance_uid.data(),
                      static_cast<int>(study_instance_uid.size()),
                      SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(get_text(stmt, 0));
    }

    sqlite3_finalize(stmt);
    return ok(std::move(results));
}

auto instance_repository::get_series_files(std::string_view series_instance_uid) const
    -> Result<std::vector<std::string>> {
    std::vector<std::string> results;

    const char* sql = R"(
        SELECT i.file_path
        FROM instances i
        JOIN series se ON i.series_pk = se.series_pk
        WHERE se.series_uid = ?
        ORDER BY i.instance_number;
    )";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<std::vector<std::string>>(
            database_query_error,
            kcenon::pacs::compat::format("Failed to prepare query: {}",
                                 sqlite3_errmsg(db_)),
            "storage");
    }

    sqlite3_bind_text(stmt, 1, series_instance_uid.data(),
                      static_cast<int>(series_instance_uid.size()),
                      SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(get_text(stmt, 0));
    }

    sqlite3_finalize(stmt);
    return ok(std::move(results));
}

}  // namespace kcenon::pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
