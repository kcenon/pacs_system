/**
 * @file index_database.cpp
 * @brief Implementation of PACS index database
 */

#include <pacs/storage/index_database.hpp>

#include <sqlite3.h>

#include <chrono>
#include <ctime>
#include <format>
#include <iomanip>
#include <sstream>

namespace pacs::storage {

// Use common_system's result helpers
using kcenon::common::make_error;
using kcenon::common::ok;

// ============================================================================
// Helper Functions
// ============================================================================

namespace {

/**
 * @brief Parse ISO datetime string to time_point
 */
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

/**
 * @brief Get text from statement column, returning empty string for NULL
 */
auto get_text(sqlite3_stmt* stmt, int col) -> std::string {
    const auto* text =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, col));
    return text ? std::string(text) : std::string{};
}

}  // namespace

// ============================================================================
// Construction / Destruction
// ============================================================================

auto index_database::open(std::string_view db_path)
    -> Result<std::unique_ptr<index_database>> {
    sqlite3* db = nullptr;

    auto rc = sqlite3_open(std::string(db_path).c_str(), &db);
    if (rc != SQLITE_OK) {
        std::string error_msg =
            db ? sqlite3_errmsg(db) : "Failed to allocate memory";
        if (db) {
            sqlite3_close(db);
        }
        return make_error<std::unique_ptr<index_database>>(
            rc, std::format("Failed to open database: {}", error_msg),
            "storage");
    }

    // Enable foreign keys
    rc = sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr,
                      nullptr);
    if (rc != SQLITE_OK) {
        sqlite3_close(db);
        return make_error<std::unique_ptr<index_database>>(
            rc, "Failed to enable foreign keys", "storage");
    }

    // Create database instance
    auto instance = std::unique_ptr<index_database>(
        new index_database(db, std::string(db_path)));

    // Run migrations
    auto migration_result = instance->migration_runner_.run_migrations(db);
    if (migration_result.is_err()) {
        return make_error<std::unique_ptr<index_database>>(
            migration_result.error().code,
            std::format("Migration failed: {}",
                       migration_result.error().message),
            "storage");
    }

    return instance;
}

index_database::index_database(sqlite3* db, std::string path)
    : db_(db), path_(std::move(path)) {}

index_database::~index_database() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

index_database::index_database(index_database&& other) noexcept
    : db_(other.db_), path_(std::move(other.path_)) {
    other.db_ = nullptr;
}

auto index_database::operator=(index_database&& other) noexcept
    -> index_database& {
    if (this != &other) {
        if (db_) {
            sqlite3_close(db_);
        }
        db_ = other.db_;
        path_ = std::move(other.path_);
        other.db_ = nullptr;
    }
    return *this;
}

// ============================================================================
// Patient Operations
// ============================================================================

auto index_database::upsert_patient(std::string_view patient_id,
                                    std::string_view patient_name,
                                    std::string_view birth_date,
                                    std::string_view sex) -> Result<int64_t> {
    patient_record record;
    record.patient_id = std::string(patient_id);
    record.patient_name = std::string(patient_name);
    record.birth_date = std::string(birth_date);
    record.sex = std::string(sex);
    return upsert_patient(record);
}

auto index_database::upsert_patient(const patient_record& record)
    -> Result<int64_t> {
    if (record.patient_id.empty()) {
        return make_error<int64_t>(-1, "Patient ID is required", "storage");
    }

    if (record.patient_id.length() > 64) {
        return make_error<int64_t>(
            -1, "Patient ID exceeds maximum length of 64 characters",
            "storage");
    }

    // Validate sex if provided
    if (!record.sex.empty() && record.sex != "M" && record.sex != "F" &&
        record.sex != "O") {
        return make_error<int64_t>(
            -1, "Invalid sex value. Must be M, F, or O", "storage");
    }

    // Use INSERT OR REPLACE for upsert behavior
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
            std::format("Failed to prepare statement: {}", sqlite3_errmsg(db_)),
            "storage");
    }

    // Bind parameters
    sqlite3_bind_text(stmt, 1, record.patient_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, record.patient_name.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, record.birth_date.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, record.sex.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, record.other_ids.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, record.ethnic_group.c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, record.comments.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        auto error_msg = sqlite3_errmsg(db_);
        sqlite3_finalize(stmt);
        return make_error<int64_t>(
            rc, std::format("Failed to upsert patient: {}", error_msg),
            "storage");
    }

    auto pk = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);

    return pk;
}

auto index_database::find_patient(std::string_view patient_id) const
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

auto index_database::find_patient_by_pk(int64_t pk) const
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

auto index_database::search_patients(const patient_query& query) const
    -> std::vector<patient_record> {
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
        sql += std::format(" LIMIT {}", query.limit);
    }

    if (query.offset > 0) {
        sql += std::format(" OFFSET {}", query.offset);
    }

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return results;
    }

    // Bind parameters
    for (size_t i = 0; i < params.size(); ++i) {
        sqlite3_bind_text(stmt, static_cast<int>(i + 1), params[i].c_str(), -1,
                          SQLITE_TRANSIENT);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(parse_patient_row(stmt));
    }

    sqlite3_finalize(stmt);
    return results;
}

auto index_database::delete_patient(std::string_view patient_id) -> VoidResult {
    const char* sql = "DELETE FROM patients WHERE patient_id = ?;";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<std::monostate>(
            rc,
            std::format("Failed to prepare delete: {}", sqlite3_errmsg(db_)),
            "storage");
    }

    sqlite3_bind_text(stmt, 1, patient_id.data(),
                      static_cast<int>(patient_id.size()), SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return make_error<std::monostate>(
            rc, std::format("Failed to delete patient: {}", sqlite3_errmsg(db_)),
            "storage");
    }

    return ok();
}

auto index_database::patient_count() const -> size_t {
    const char* sql = "SELECT COUNT(*) FROM patients;";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return 0;
    }

    size_t count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
    }

    sqlite3_finalize(stmt);
    return count;
}

// ============================================================================
// Database Information
// ============================================================================

auto index_database::path() const -> std::string_view { return path_; }

auto index_database::schema_version() const -> int {
    return migration_runner_.get_current_version(db_);
}

auto index_database::is_open() const noexcept -> bool { return db_ != nullptr; }

// ============================================================================
// Private Helpers
// ============================================================================

auto index_database::parse_patient_row(void* stmt_ptr) const -> patient_record {
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

auto index_database::to_like_pattern(std::string_view pattern) -> std::string {
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

}  // namespace pacs::storage
