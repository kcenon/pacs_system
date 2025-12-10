/**
 * @file database_cursor.cpp
 * @brief Implementation of database cursor for streaming query results
 */

#include "pacs/services/cache/database_cursor.hpp"

#include <sqlite3.h>

#include <iomanip>
#include <sstream>

namespace pacs::services {

// Use common_system's result helpers
using kcenon::common::ok;

// =============================================================================
// Construction / Destruction
// =============================================================================

database_cursor::database_cursor(sqlite3_stmt* stmt, record_type type)
    : stmt_(stmt), type_(type) {}

database_cursor::~database_cursor() {
    if (stmt_) {
        sqlite3_finalize(stmt_);
        stmt_ = nullptr;
    }
}

database_cursor::database_cursor(database_cursor&& other) noexcept
    : stmt_(other.stmt_),
      type_(other.type_),
      position_(other.position_),
      has_more_(other.has_more_),
      stepped_(other.stepped_) {
    other.stmt_ = nullptr;
    other.has_more_ = false;
}

auto database_cursor::operator=(database_cursor&& other) noexcept -> database_cursor& {
    if (this != &other) {
        if (stmt_) {
            sqlite3_finalize(stmt_);
        }
        stmt_ = other.stmt_;
        type_ = other.type_;
        position_ = other.position_;
        has_more_ = other.has_more_;
        stepped_ = other.stepped_;

        other.stmt_ = nullptr;
        other.has_more_ = false;
    }
    return *this;
}

// =============================================================================
// Helper Functions
// =============================================================================

auto database_cursor::to_like_pattern(std::string_view pattern) -> std::string {
    std::string result;
    result.reserve(pattern.size());

    for (char c : pattern) {
        if (c == '*') {
            result += '%';
        } else if (c == '?') {
            result += '_';
        } else if (c == '%' || c == '_') {
            // Escape SQL wildcards
            result += '\\';
            result += c;
        } else {
            result += c;
        }
    }

    return result;
}

// =============================================================================
// WHERE Clause Builders
// =============================================================================

auto database_cursor::build_patient_where(const storage::patient_query& query)
    -> std::pair<std::string, std::vector<std::string>> {
    std::vector<std::string> conditions;
    std::vector<std::string> params;

    if (query.patient_id.has_value()) {
        if (query.patient_id->find('*') != std::string::npos) {
            conditions.push_back("patient_id LIKE ? ESCAPE '\\'");
            params.push_back(to_like_pattern(*query.patient_id));
        } else {
            conditions.push_back("patient_id = ?");
            params.push_back(*query.patient_id);
        }
    }

    if (query.patient_name.has_value()) {
        if (query.patient_name->find('*') != std::string::npos) {
            conditions.push_back("patient_name LIKE ? ESCAPE '\\'");
            params.push_back(to_like_pattern(*query.patient_name));
        } else {
            conditions.push_back("patient_name = ?");
            params.push_back(*query.patient_name);
        }
    }

    if (query.birth_date.has_value()) {
        conditions.push_back("birth_date = ?");
        params.push_back(*query.birth_date);
    }

    if (query.birth_date_from.has_value()) {
        conditions.push_back("birth_date >= ?");
        params.push_back(*query.birth_date_from);
    }

    if (query.birth_date_to.has_value()) {
        conditions.push_back("birth_date <= ?");
        params.push_back(*query.birth_date_to);
    }

    if (query.sex.has_value()) {
        conditions.push_back("sex = ?");
        params.push_back(*query.sex);
    }

    std::string where_clause;
    if (!conditions.empty()) {
        where_clause = " WHERE ";
        for (size_t i = 0; i < conditions.size(); ++i) {
            if (i > 0) {
                where_clause += " AND ";
            }
            where_clause += conditions[i];
        }
    }

    return {where_clause, params};
}

auto database_cursor::build_study_where(const storage::study_query& query)
    -> std::pair<std::string, std::vector<std::string>> {
    std::vector<std::string> conditions;
    std::vector<std::string> params;

    if (query.patient_id.has_value()) {
        if (query.patient_id->find('*') != std::string::npos) {
            conditions.push_back("p.patient_id LIKE ? ESCAPE '\\'");
            params.push_back(to_like_pattern(*query.patient_id));
        } else {
            conditions.push_back("p.patient_id = ?");
            params.push_back(*query.patient_id);
        }
    }

    if (query.patient_name.has_value()) {
        if (query.patient_name->find('*') != std::string::npos) {
            conditions.push_back("p.patient_name LIKE ? ESCAPE '\\'");
            params.push_back(to_like_pattern(*query.patient_name));
        } else {
            conditions.push_back("p.patient_name = ?");
            params.push_back(*query.patient_name);
        }
    }

    if (query.study_uid.has_value()) {
        conditions.push_back("s.study_uid = ?");
        params.push_back(*query.study_uid);
    }

    if (query.study_id.has_value()) {
        if (query.study_id->find('*') != std::string::npos) {
            conditions.push_back("s.study_id LIKE ? ESCAPE '\\'");
            params.push_back(to_like_pattern(*query.study_id));
        } else {
            conditions.push_back("s.study_id = ?");
            params.push_back(*query.study_id);
        }
    }

    if (query.study_date.has_value()) {
        conditions.push_back("s.study_date = ?");
        params.push_back(*query.study_date);
    }

    if (query.study_date_from.has_value()) {
        conditions.push_back("s.study_date >= ?");
        params.push_back(*query.study_date_from);
    }

    if (query.study_date_to.has_value()) {
        conditions.push_back("s.study_date <= ?");
        params.push_back(*query.study_date_to);
    }

    if (query.accession_number.has_value()) {
        if (query.accession_number->find('*') != std::string::npos) {
            conditions.push_back("s.accession_number LIKE ? ESCAPE '\\'");
            params.push_back(to_like_pattern(*query.accession_number));
        } else {
            conditions.push_back("s.accession_number = ?");
            params.push_back(*query.accession_number);
        }
    }

    if (query.modality.has_value()) {
        conditions.push_back("s.modalities_in_study LIKE ?");
        params.push_back("%" + *query.modality + "%");
    }

    if (query.referring_physician.has_value()) {
        if (query.referring_physician->find('*') != std::string::npos) {
            conditions.push_back("s.referring_physician LIKE ? ESCAPE '\\'");
            params.push_back(to_like_pattern(*query.referring_physician));
        } else {
            conditions.push_back("s.referring_physician = ?");
            params.push_back(*query.referring_physician);
        }
    }

    if (query.study_description.has_value()) {
        if (query.study_description->find('*') != std::string::npos) {
            conditions.push_back("s.study_description LIKE ? ESCAPE '\\'");
            params.push_back(to_like_pattern(*query.study_description));
        } else {
            conditions.push_back("s.study_description = ?");
            params.push_back(*query.study_description);
        }
    }

    std::string where_clause;
    if (!conditions.empty()) {
        where_clause = " WHERE ";
        for (size_t i = 0; i < conditions.size(); ++i) {
            if (i > 0) {
                where_clause += " AND ";
            }
            where_clause += conditions[i];
        }
    }

    return {where_clause, params};
}

auto database_cursor::build_series_where(const storage::series_query& query)
    -> std::pair<std::string, std::vector<std::string>> {
    std::vector<std::string> conditions;
    std::vector<std::string> params;

    if (query.study_uid.has_value()) {
        conditions.push_back("st.study_uid = ?");
        params.push_back(*query.study_uid);
    }

    if (query.series_uid.has_value()) {
        conditions.push_back("se.series_uid = ?");
        params.push_back(*query.series_uid);
    }

    if (query.modality.has_value()) {
        conditions.push_back("se.modality = ?");
        params.push_back(*query.modality);
    }

    if (query.series_number.has_value()) {
        conditions.push_back("se.series_number = ?");
        params.push_back(std::to_string(*query.series_number));
    }

    if (query.series_description.has_value()) {
        if (query.series_description->find('*') != std::string::npos) {
            conditions.push_back("se.series_description LIKE ? ESCAPE '\\'");
            params.push_back(to_like_pattern(*query.series_description));
        } else {
            conditions.push_back("se.series_description = ?");
            params.push_back(*query.series_description);
        }
    }

    std::string where_clause;
    if (!conditions.empty()) {
        where_clause = " WHERE ";
        for (size_t i = 0; i < conditions.size(); ++i) {
            if (i > 0) {
                where_clause += " AND ";
            }
            where_clause += conditions[i];
        }
    }

    return {where_clause, params};
}

auto database_cursor::build_instance_where(const storage::instance_query& query)
    -> std::pair<std::string, std::vector<std::string>> {
    std::vector<std::string> conditions;
    std::vector<std::string> params;

    if (query.series_uid.has_value()) {
        conditions.push_back("se.series_uid = ?");
        params.push_back(*query.series_uid);
    }

    if (query.sop_uid.has_value()) {
        conditions.push_back("i.sop_uid = ?");
        params.push_back(*query.sop_uid);
    }

    if (query.sop_class_uid.has_value()) {
        conditions.push_back("i.sop_class_uid = ?");
        params.push_back(*query.sop_class_uid);
    }

    if (query.instance_number.has_value()) {
        conditions.push_back("i.instance_number = ?");
        params.push_back(std::to_string(*query.instance_number));
    }

    std::string where_clause;
    if (!conditions.empty()) {
        where_clause = " WHERE ";
        for (size_t i = 0; i < conditions.size(); ++i) {
            if (i > 0) {
                where_clause += " AND ";
            }
            where_clause += conditions[i];
        }
    }

    return {where_clause, params};
}

// =============================================================================
// Factory Methods
// =============================================================================

auto database_cursor::create_patient_cursor(sqlite3* db,
                                            const storage::patient_query& query)
    -> Result<std::unique_ptr<database_cursor>> {
    auto [where_clause, params] = build_patient_where(query);

    std::string sql =
        "SELECT patient_pk, patient_id, patient_name, birth_date, sex, "
        "other_ids, ethnic_group, comments, created_at, updated_at "
        "FROM patients" +
        where_clause + " ORDER BY patient_name";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);

    if (rc != SQLITE_OK) {
        return kcenon::common::error_info(
            std::string("Failed to prepare patient cursor: ") + sqlite3_errmsg(db));
    }

    // Bind parameters
    for (size_t i = 0; i < params.size(); ++i) {
        rc = sqlite3_bind_text(stmt, static_cast<int>(i + 1), params[i].c_str(), -1,
                               SQLITE_TRANSIENT);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            return kcenon::common::error_info(
                std::string("Failed to bind parameter: ") + sqlite3_errmsg(db));
        }
    }

    return std::unique_ptr<database_cursor>(new database_cursor(stmt, record_type::patient));
}

auto database_cursor::create_study_cursor(sqlite3* db, const storage::study_query& query)
    -> Result<std::unique_ptr<database_cursor>> {
    auto [where_clause, params] = build_study_where(query);

    std::string sql =
        "SELECT s.study_pk, s.patient_pk, s.study_uid, s.study_id, s.study_date, "
        "s.study_time, s.accession_number, s.referring_physician, "
        "s.study_description, s.modalities_in_study, s.num_series, "
        "s.num_instances, s.created_at, s.updated_at "
        "FROM studies s "
        "JOIN patients p ON s.patient_pk = p.patient_pk" +
        where_clause + " ORDER BY s.study_date DESC, s.study_time DESC";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);

    if (rc != SQLITE_OK) {
        return kcenon::common::error_info(
            std::string("Failed to prepare study cursor: ") + sqlite3_errmsg(db));
    }

    // Bind parameters
    for (size_t i = 0; i < params.size(); ++i) {
        rc = sqlite3_bind_text(stmt, static_cast<int>(i + 1), params[i].c_str(), -1,
                               SQLITE_TRANSIENT);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            return kcenon::common::error_info(
                std::string("Failed to bind parameter: ") + sqlite3_errmsg(db));
        }
    }

    return std::unique_ptr<database_cursor>(new database_cursor(stmt, record_type::study));
}

auto database_cursor::create_series_cursor(sqlite3* db,
                                           const storage::series_query& query)
    -> Result<std::unique_ptr<database_cursor>> {
    auto [where_clause, params] = build_series_where(query);

    std::string sql =
        "SELECT se.series_pk, se.study_pk, se.series_uid, se.modality, "
        "se.series_number, se.series_description, se.body_part_examined, "
        "se.station_name, se.num_instances, se.created_at, se.updated_at "
        "FROM series se "
        "JOIN studies st ON se.study_pk = st.study_pk" +
        where_clause + " ORDER BY se.series_number";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);

    if (rc != SQLITE_OK) {
        return kcenon::common::error_info(
            std::string("Failed to prepare series cursor: ") + sqlite3_errmsg(db));
    }

    // Bind parameters
    for (size_t i = 0; i < params.size(); ++i) {
        rc = sqlite3_bind_text(stmt, static_cast<int>(i + 1), params[i].c_str(), -1,
                               SQLITE_TRANSIENT);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            return kcenon::common::error_info(
                std::string("Failed to bind parameter: ") + sqlite3_errmsg(db));
        }
    }

    return std::unique_ptr<database_cursor>(new database_cursor(stmt, record_type::series));
}

auto database_cursor::create_instance_cursor(sqlite3* db,
                                             const storage::instance_query& query)
    -> Result<std::unique_ptr<database_cursor>> {
    auto [where_clause, params] = build_instance_where(query);

    std::string sql =
        "SELECT i.instance_pk, i.series_pk, i.sop_uid, i.sop_class_uid, "
        "i.file_path, i.file_size, i.transfer_syntax, i.instance_number, "
        "i.created_at "
        "FROM instances i "
        "JOIN series se ON i.series_pk = se.series_pk" +
        where_clause + " ORDER BY i.instance_number";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);

    if (rc != SQLITE_OK) {
        return kcenon::common::error_info(
            std::string("Failed to prepare instance cursor: ") + sqlite3_errmsg(db));
    }

    // Bind parameters
    for (size_t i = 0; i < params.size(); ++i) {
        rc = sqlite3_bind_text(stmt, static_cast<int>(i + 1), params[i].c_str(), -1,
                               SQLITE_TRANSIENT);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            return kcenon::common::error_info(
                std::string("Failed to bind parameter: ") + sqlite3_errmsg(db));
        }
    }

    return std::unique_ptr<database_cursor>(
        new database_cursor(stmt, record_type::instance));
}

// =============================================================================
// Cursor Operations
// =============================================================================

auto database_cursor::has_more() const noexcept -> bool {
    return has_more_;
}

auto database_cursor::fetch_next() -> std::optional<query_record> {
    if (!stmt_ || !has_more_) {
        return std::nullopt;
    }

    int rc = sqlite3_step(stmt_);
    stepped_ = true;

    if (rc == SQLITE_ROW) {
        ++position_;
        switch (type_) {
            case record_type::patient:
                return parse_patient_row();
            case record_type::study:
                return parse_study_row();
            case record_type::series:
                return parse_series_row();
            case record_type::instance:
                return parse_instance_row();
        }
    } else if (rc == SQLITE_DONE) {
        has_more_ = false;
    }

    return std::nullopt;
}

auto database_cursor::fetch_batch(size_t batch_size) -> std::vector<query_record> {
    std::vector<query_record> results;
    results.reserve(batch_size);

    while (results.size() < batch_size && has_more_) {
        auto record = fetch_next();
        if (record.has_value()) {
            results.push_back(std::move(record.value()));
        }
    }

    return results;
}

auto database_cursor::position() const noexcept -> size_t {
    return position_;
}

auto database_cursor::type() const noexcept -> record_type {
    return type_;
}

auto database_cursor::reset() -> VoidResult {
    if (!stmt_) {
        return kcenon::common::error_info(std::string("Cursor is not valid"));
    }

    int rc = sqlite3_reset(stmt_);
    if (rc != SQLITE_OK) {
        return kcenon::common::error_info(std::string("Failed to reset cursor"));
    }

    position_ = 0;
    has_more_ = true;
    stepped_ = false;

    return ok();
}

auto database_cursor::serialize() const -> std::string {
    std::ostringstream oss;
    oss << static_cast<int>(type_) << ":" << position_;
    return oss.str();
}

// =============================================================================
// Row Parsing
// =============================================================================

namespace {

auto get_column_text(sqlite3_stmt* stmt, int col) -> std::string {
    auto* text = sqlite3_column_text(stmt, col);
    return text ? reinterpret_cast<const char*>(text) : "";
}

auto get_column_int64(sqlite3_stmt* stmt, int col) -> int64_t {
    return sqlite3_column_int64(stmt, col);
}

auto get_column_int(sqlite3_stmt* stmt, int col) -> int {
    return sqlite3_column_int(stmt, col);
}

auto parse_timestamp(const std::string& timestamp)
    -> std::chrono::system_clock::time_point {
    // Simple ISO 8601 parsing (YYYY-MM-DD HH:MM:SS)
    if (timestamp.empty()) {
        return std::chrono::system_clock::now();
    }

    std::tm tm = {};
    std::istringstream ss(timestamp);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");

    if (ss.fail()) {
        return std::chrono::system_clock::now();
    }

    auto time_t_val = std::mktime(&tm);
    return std::chrono::system_clock::from_time_t(time_t_val);
}

}  // namespace

auto database_cursor::parse_patient_row() const -> storage::patient_record {
    storage::patient_record record;

    record.pk = get_column_int64(stmt_, 0);
    record.patient_id = get_column_text(stmt_, 1);
    record.patient_name = get_column_text(stmt_, 2);
    record.birth_date = get_column_text(stmt_, 3);
    record.sex = get_column_text(stmt_, 4);
    record.other_ids = get_column_text(stmt_, 5);
    record.ethnic_group = get_column_text(stmt_, 6);
    record.comments = get_column_text(stmt_, 7);
    record.created_at = parse_timestamp(get_column_text(stmt_, 8));
    record.updated_at = parse_timestamp(get_column_text(stmt_, 9));

    return record;
}

auto database_cursor::parse_study_row() const -> storage::study_record {
    storage::study_record record;

    record.pk = get_column_int64(stmt_, 0);
    record.patient_pk = get_column_int64(stmt_, 1);
    record.study_uid = get_column_text(stmt_, 2);
    record.study_id = get_column_text(stmt_, 3);
    record.study_date = get_column_text(stmt_, 4);
    record.study_time = get_column_text(stmt_, 5);
    record.accession_number = get_column_text(stmt_, 6);
    record.referring_physician = get_column_text(stmt_, 7);
    record.study_description = get_column_text(stmt_, 8);
    record.modalities_in_study = get_column_text(stmt_, 9);
    record.num_series = get_column_int(stmt_, 10);
    record.num_instances = get_column_int(stmt_, 11);
    record.created_at = parse_timestamp(get_column_text(stmt_, 12));
    record.updated_at = parse_timestamp(get_column_text(stmt_, 13));

    return record;
}

auto database_cursor::parse_series_row() const -> storage::series_record {
    storage::series_record record;

    record.pk = get_column_int64(stmt_, 0);
    record.study_pk = get_column_int64(stmt_, 1);
    record.series_uid = get_column_text(stmt_, 2);
    record.modality = get_column_text(stmt_, 3);

    int series_num = get_column_int(stmt_, 4);
    if (sqlite3_column_type(stmt_, 4) != SQLITE_NULL) {
        record.series_number = series_num;
    }

    record.series_description = get_column_text(stmt_, 5);
    record.body_part_examined = get_column_text(stmt_, 6);
    record.station_name = get_column_text(stmt_, 7);
    record.num_instances = get_column_int(stmt_, 8);
    record.created_at = parse_timestamp(get_column_text(stmt_, 9));
    record.updated_at = parse_timestamp(get_column_text(stmt_, 10));

    return record;
}

auto database_cursor::parse_instance_row() const -> storage::instance_record {
    storage::instance_record record;

    record.pk = get_column_int64(stmt_, 0);
    record.series_pk = get_column_int64(stmt_, 1);
    record.sop_uid = get_column_text(stmt_, 2);
    record.sop_class_uid = get_column_text(stmt_, 3);
    record.file_path = get_column_text(stmt_, 4);
    record.file_size = get_column_int64(stmt_, 5);
    record.transfer_syntax = get_column_text(stmt_, 6);

    int instance_num = get_column_int(stmt_, 7);
    if (sqlite3_column_type(stmt_, 7) != SQLITE_NULL) {
        record.instance_number = instance_num;
    }

    record.created_at = parse_timestamp(get_column_text(stmt_, 8));

    return record;
}

}  // namespace pacs::services
