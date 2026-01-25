/**
 * @file index_database.cpp
 * @brief Implementation of PACS index database
 *
 * When compiled with PACS_WITH_DATABASE_SYSTEM, uses database_system's
 * query builder for parameterized queries. Otherwise, uses direct SQLite
 * with prepared statements.
 */

#include <pacs/storage/index_database.hpp>

#include <pacs/core/result.hpp>
#include <sqlite3.h>

#ifdef PACS_WITH_DATABASE_SYSTEM
#include <database/query_builder.h>
#endif

#include <chrono>
#include <ctime>
#include <filesystem>
#include <pacs/compat/format.hpp>
#include <pacs/compat/time.hpp>
#include <iomanip>
#include <set>
#include <sstream>
#include <variant>

namespace pacs::storage {

// Use common_system's result helpers
using kcenon::common::make_error;
using kcenon::common::ok;

// Use pacs error codes
using namespace pacs::error_codes;

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

#ifdef PACS_WITH_DATABASE_SYSTEM
/**
 * @brief Helper to extract string from database_value
 */
auto get_string_value(
    const std::map<std::string, database::core::database_value>& row,
    const std::string& key) -> std::string {
    auto it = row.find(key);
    if (it == row.end()) {
        return {};
    }
    if (std::holds_alternative<std::string>(it->second)) {
        return std::get<std::string>(it->second);
    }
    return {};
}

/**
 * @brief Helper to extract int64 from database_value
 */
auto get_int64_value(
    const std::map<std::string, database::core::database_value>& row,
    const std::string& key) -> int64_t {
    auto it = row.find(key);
    if (it == row.end()) {
        return 0;
    }
    if (std::holds_alternative<int64_t>(it->second)) {
        return std::get<int64_t>(it->second);
    }
    if (std::holds_alternative<std::string>(it->second)) {
        try {
            return std::stoll(std::get<std::string>(it->second));
        } catch (...) {
            return 0;
        }
    }
    return 0;
}
#endif

}  // namespace

// ============================================================================
// Construction / Destruction
// ============================================================================

auto index_database::open(std::string_view db_path)
    -> Result<std::unique_ptr<index_database>> {
    return open(db_path, index_config{});
}

auto index_database::open(std::string_view db_path, const index_config& config)
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
            rc, pacs::compat::format("Failed to open database: {}", error_msg),
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

    // Configure WAL mode for better concurrency (except for in-memory DB)
    if (config.wal_mode && db_path != ":memory:") {
        rc = sqlite3_exec(db, "PRAGMA journal_mode = WAL;", nullptr, nullptr,
                          nullptr);
        if (rc != SQLITE_OK) {
            sqlite3_close(db);
            return make_error<std::unique_ptr<index_database>>(
                rc, "Failed to enable WAL mode", "storage");
        }
    }

    // Configure cache size (negative value means KB)
    auto cache_sql =
        pacs::compat::format("PRAGMA cache_size = -{};", config.cache_size_mb * 1024);
    rc = sqlite3_exec(db, cache_sql.c_str(), nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) {
        sqlite3_close(db);
        return make_error<std::unique_ptr<index_database>>(
            rc, "Failed to set cache size", "storage");
    }

    // Configure memory-mapped I/O
    if (config.mmap_enabled && db_path != ":memory:") {
        auto mmap_sql = pacs::compat::format("PRAGMA mmap_size = {};", config.mmap_size);
        rc = sqlite3_exec(db, mmap_sql.c_str(), nullptr, nullptr, nullptr);
        if (rc != SQLITE_OK) {
            // mmap failure is not critical, continue with regular I/O
        }
    }

    // Set synchronous mode for durability with WAL
    rc = sqlite3_exec(db, "PRAGMA synchronous = NORMAL;", nullptr, nullptr,
                      nullptr);
    if (rc != SQLITE_OK) {
        // Not critical, continue
    }

    // Create database instance
    auto instance = std::unique_ptr<index_database>(
        new index_database(db, std::string(db_path)));

    // Run migrations
    auto migration_result = instance->migration_runner_.run_migrations(db);
    if (migration_result.is_err()) {
        return make_error<std::unique_ptr<index_database>>(
            migration_result.error().code,
            pacs::compat::format("Migration failed: {}",
                       migration_result.error().message),
            "storage");
    }

#ifdef PACS_WITH_DATABASE_SYSTEM
    // Initialize database_system for query building
    // Support in-memory databases for testing (Issue #625)
    auto db_init_result = instance->initialize_database_system();
    if (db_init_result.is_err()) {
        // Log warning but continue - fallback to direct SQLite
        // This allows graceful degradation if database_system fails
    }
#endif

    return instance;
}

index_database::index_database(sqlite3* db, std::string path)
    : db_(db), path_(std::move(path)) {}

index_database::~index_database() {
#ifdef PACS_WITH_DATABASE_SYSTEM
    if (db_adapter_) {
        (void)db_adapter_->disconnect();
        db_adapter_.reset();
    }
    if (db_manager_) {
        (void)db_manager_->disconnect_result();
    }
#endif
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

#ifdef PACS_WITH_DATABASE_SYSTEM
auto index_database::initialize_database_system() -> VoidResult {
    db_context_ = std::make_shared<database::database_context>();
    db_manager_ = std::make_shared<database::database_manager>(db_context_);

    if (!db_manager_->set_mode(database::database_types::sqlite)) {
        return make_error<std::monostate>(
            database_connection_error, "Failed to set SQLite mode", "storage");
    }

    auto connect_result = db_manager_->connect_result(path_);
    if (connect_result.is_err()) {
        return make_error<std::monostate>(
            database_connection_error,
            pacs::compat::format("Failed to connect: {}",
                                 connect_result.error().message),
            "storage");
    }

    // Initialize pacs_database_adapter as well
    auto adapter_result = initialize_database_adapter();
    if (adapter_result.is_err()) {
        // Log warning but continue - can fallback to database_manager
        // Note: For now, in-memory databases may not work with unified_database_system
        // TODO: Investigate unified_database_system in-memory support (Issue #625)
    }

    return ok();
}

auto index_database::initialize_database_adapter() -> VoidResult {
    db_adapter_ = std::make_unique<pacs_database_adapter>(
        std::filesystem::path(path_));

    auto connect_result = db_adapter_->connect();
    if (connect_result.is_err()) {
        db_adapter_.reset();
        return make_error<std::monostate>(
            database_connection_error,
            pacs::compat::format("Failed to connect adapter: {}",
                                 connect_result.error().message),
            "storage");
    }

    return ok();
}

auto index_database::parse_patient_from_adapter_row(const database_row& row) const
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
        record.created_at = parse_datetime(created_at_str.c_str());
    }

    auto updated_at_str = get_str("updated_at");
    if (!updated_at_str.empty()) {
        record.updated_at = parse_datetime(updated_at_str.c_str());
    }

    return record;
}

auto index_database::parse_study_from_adapter_row(const database_row& row) const
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
        record.created_at = parse_datetime(created_at_str.c_str());
    }

    auto updated_at_str = get_str("updated_at");
    if (!updated_at_str.empty()) {
        record.updated_at = parse_datetime(updated_at_str.c_str());
    }

    return record;
}

auto index_database::parse_series_from_adapter_row(const database_row& row) const
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
        record.created_at = parse_datetime(created_at_str.c_str());
    }

    auto updated_at_str = get_str("updated_at");
    if (!updated_at_str.empty()) {
        record.updated_at = parse_datetime(updated_at_str.c_str());
    }

    return record;
}

auto index_database::parse_instance_from_adapter_row(const database_row& row) const
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
        record.created_at = parse_datetime(created_at_str.c_str());
    }

    return record;
}

auto index_database::parse_mpps_from_adapter_row(const database_row& row) const
    -> mpps_record {
    mpps_record record;

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

    record.pk = get_int64("mpps_pk");
    record.mpps_uid = get_str("mpps_uid");
    record.status = get_str("status");
    record.start_datetime = get_str("start_datetime");
    record.end_datetime = get_str("end_datetime");
    record.station_ae = get_str("station_ae");
    record.station_name = get_str("station_name");
    record.modality = get_str("modality");
    record.study_uid = get_str("study_uid");
    record.accession_no = get_str("accession_no");
    record.scheduled_step_id = get_str("scheduled_step_id");
    record.requested_proc_id = get_str("requested_proc_id");
    record.performed_series = get_str("performed_series");

    auto created_at_str = get_str("created_at");
    if (!created_at_str.empty()) {
        record.created_at = parse_datetime(created_at_str.c_str());
    }

    auto updated_at_str = get_str("updated_at");
    if (!updated_at_str.empty()) {
        record.updated_at = parse_datetime(updated_at_str.c_str());
    }

    return record;
}

auto index_database::parse_worklist_from_adapter_row(const database_row& row) const
    -> worklist_item {
    worklist_item item;

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

    item.pk = get_int64("worklist_pk");
    item.step_id = get_str("step_id");
    item.step_status = get_str("step_status");
    item.patient_id = get_str("patient_id");
    item.patient_name = get_str("patient_name");
    item.birth_date = get_str("birth_date");
    item.sex = get_str("sex");
    item.accession_no = get_str("accession_no");
    item.requested_proc_id = get_str("requested_proc_id");
    item.study_uid = get_str("study_uid");
    item.scheduled_datetime = get_str("scheduled_datetime");
    item.station_ae = get_str("station_ae");
    item.station_name = get_str("station_name");
    item.modality = get_str("modality");
    item.procedure_desc = get_str("procedure_desc");
    item.protocol_code = get_str("protocol_code");
    item.referring_phys = get_str("referring_phys");
    item.referring_phys_id = get_str("referring_phys_id");

    auto created_at_str = get_str("created_at");
    if (!created_at_str.empty()) {
        item.created_at = parse_datetime(created_at_str.c_str());
    }

    auto updated_at_str = get_str("updated_at");
    if (!updated_at_str.empty()) {
        item.updated_at = parse_datetime(updated_at_str.c_str());
    }

    return item;
}
#endif

index_database::index_database(index_database&& other) noexcept
    : db_(other.db_), path_(std::move(other.path_)) {
#ifdef PACS_WITH_DATABASE_SYSTEM
    db_context_ = std::move(other.db_context_);
    db_manager_ = std::move(other.db_manager_);
    db_adapter_ = std::move(other.db_adapter_);
#endif
    other.db_ = nullptr;
}

auto index_database::operator=(index_database&& other) noexcept
    -> index_database& {
    if (this != &other) {
#ifdef PACS_WITH_DATABASE_SYSTEM
        if (db_adapter_) {
            (void)db_adapter_->disconnect();
        }
        if (db_manager_) {
            (void)db_manager_->disconnect_result();
        }
        db_context_ = std::move(other.db_context_);
        db_manager_ = std::move(other.db_manager_);
        db_adapter_ = std::move(other.db_adapter_);
#endif
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

#ifdef PACS_WITH_DATABASE_SYSTEM
    // Use pacs_database_adapter for unified database operations (Issue #606, #608, #621)
    // Fallback to direct SQLite if adapter not available (e.g., in-memory databases)
    if (db_adapter_ && db_adapter_->is_connected()) {

    auto builder = db_adapter_->create_query_builder();

    // Check if patient exists
    auto check_sql = builder.select(std::vector<std::string>{"patient_pk"})
                         .from("patients")
                         .where("patient_id", "=", record.patient_id)
                         .build();

    auto check_result = db_adapter_->select(check_sql);
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

        auto update_result = db_adapter_->update(update_sql);
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

        auto insert_result = db_adapter_->insert(insert_sql);
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
    } else {
        // Fallback to direct SQLite when adapter unavailable (e.g., in-memory DB)
        // Issue #625 - unified_database_system doesn't support in-memory databases yet
#endif
    // Direct SQLite implementation
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
            rc, pacs::compat::format("Failed to upsert patient: {}", error_msg),
            "storage");
    }

    auto pk = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);

    return pk;
#ifdef PACS_WITH_DATABASE_SYSTEM
    }
#endif
}

auto index_database::find_patient(std::string_view patient_id) const
    -> std::optional<patient_record> {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Use pacs_database_adapter for unified database operations (Issue #608, #621)
    if (!db_adapter_ || !db_adapter_->is_connected()) {
        return std::nullopt;
    }

    auto builder = db_adapter_->create_query_builder();
    auto select_sql =
        builder
            .select(std::vector<std::string>{
                "patient_pk", "patient_id", "patient_name", "birth_date",
                "sex", "other_ids", "ethnic_group", "comments",
                "created_at", "updated_at"})
            .from("patients")
            .where("patient_id", "=", std::string(patient_id))
            .build();

    auto result = db_adapter_->select(select_sql);
    if (result.is_err() || result.value().empty()) {
        return std::nullopt;
    }

    return parse_patient_from_adapter_row(result.value()[0]);
#else
    // PACS_WITH_DATABASE_SYSTEM not defined - use direct SQLite
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
#endif
}

auto index_database::find_patient_by_pk(int64_t pk) const
    -> std::optional<patient_record> {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Use pacs_database_adapter for unified database operations (Issue #608, #622)
    if (!db_adapter_ || !db_adapter_->is_connected()) {
        return std::nullopt;
    }

    auto builder = db_adapter_->create_query_builder();
    auto select_sql =
        builder
            .select(std::vector<std::string>{
                "patient_pk", "patient_id", "patient_name", "birth_date",
                "sex", "other_ids", "ethnic_group", "comments",
                "created_at", "updated_at"})
            .from("patients")
            .where("patient_pk", "=", pk)
            .build();

    auto result = db_adapter_->select(select_sql);
    if (result.is_err() || result.value().empty()) {
        return std::nullopt;
    }

    return parse_patient_from_adapter_row(result.value()[0]);
#else
    // PACS_WITH_DATABASE_SYSTEM not defined - use direct SQLite
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
#endif
}

auto index_database::search_patients(const patient_query& query) const
    -> Result<std::vector<patient_record>> {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter (Issue #613)
    if (db_adapter_ && db_adapter_->is_connected()) {
        auto builder = db_adapter_->create_query_builder();
        builder.select(std::vector<std::string>{
            "patient_pk", "patient_id", "patient_name", "birth_date",
            "sex", "other_ids", "ethnic_group", "comments",
            "created_at", "updated_at"});
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
        auto result = db_adapter_->select(select_sql);
        if (result.is_ok()) {
            std::vector<patient_record> results;
            for (const auto& row : result.value()) {
                results.push_back(parse_patient_from_adapter_row(row));
            }
            return ok(std::move(results));
        }
        // Fall through on error
    }

    // Legacy fallback: database_manager
    if (db_manager_) {
        database::query_builder builder(database::database_types::sqlite);
        builder.select(std::vector<std::string>{
            "patient_pk", "patient_id", "patient_name", "birth_date",
            "sex", "other_ids", "ethnic_group", "comments",
            "created_at", "updated_at"});
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
        auto result = db_manager_->select_query_result(select_sql);
        if (result.is_err()) {
            return pacs_error<std::vector<patient_record>>(
                error_codes::database_query_error,
                pacs::compat::format("Query failed: {}", result.error().message));
        }

        std::vector<patient_record> results;
        for (const auto& row : result.value()) {
            results.push_back(parse_patient_from_row(row));
        }
        return ok(std::move(results));
    }
#endif

    // Fallback to direct SQLite
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
        return pacs_error<std::vector<patient_record>>(
            error_codes::database_query_error,
            pacs::compat::format("Failed to prepare query: {}", sqlite3_errmsg(db_)));
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
    return ok(std::move(results));
}

auto index_database::delete_patient(std::string_view patient_id) -> VoidResult {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Use pacs_database_adapter for unified database operations (Issue #608, #622)
    if (!db_adapter_ || !db_adapter_->is_connected()) {
        return make_error<std::monostate>(
            -1, "Database adapter not available or not connected", "storage");
    }

    auto builder = db_adapter_->create_query_builder();
    auto delete_sql = builder.delete_from("patients")
                          .where("patient_id", "=", std::string(patient_id))
                          .build();

    auto result = db_adapter_->remove(delete_sql);
    if (result.is_err()) {
        return make_error<std::monostate>(
            error_codes::database_query_error,
            pacs::compat::format("Failed to delete patient: {}",
                                 result.error().message),
            "storage");
    }
    return ok();
#else
    // PACS_WITH_DATABASE_SYSTEM not defined - use direct SQLite
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
#endif
}

auto index_database::patient_count() const -> Result<size_t> {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Use pacs_database_adapter for unified database operations (Issue #608, #622)
    if (!db_adapter_ || !db_adapter_->is_connected()) {
        return pacs_error<size_t>(
            -1, "Database adapter not available or not connected");
    }

    auto builder = db_adapter_->create_query_builder();
    auto count_sql = builder.select(std::vector<std::string>{"COUNT(*) AS cnt"})
                         .from("patients")
                         .build();

    auto result = db_adapter_->select(count_sql);
    if (result.is_err()) {
        return pacs_error<size_t>(
            error_codes::database_query_error,
            pacs::compat::format("Query failed: {}", result.error().message));
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
#else
    // PACS_WITH_DATABASE_SYSTEM not defined - use direct SQLite
    const char* sql = "SELECT COUNT(*) FROM patients;";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return pacs_error<size_t>(
            error_codes::database_query_error,
            pacs::compat::format("Failed to prepare query: {}", sqlite3_errmsg(db_)));
    }

    size_t count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
    }

    sqlite3_finalize(stmt);
    return ok(count);
#endif
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

// ============================================================================
// Study Operations
// ============================================================================

auto index_database::upsert_study(int64_t patient_pk,
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

auto index_database::upsert_study(const study_record& record)
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

#ifdef PACS_WITH_DATABASE_SYSTEM
    // Use pacs_database_adapter for unified database operations (Issue #608, #622)
    if (!db_adapter_ || !db_adapter_->is_connected()) {
        return make_error<int64_t>(
            -1, "Database adapter not available or not connected", "storage");
    }

    auto builder = db_adapter_->create_query_builder();

    // Check if study exists
    auto check_sql = builder.select(std::vector<std::string>{"study_pk"})
                         .from("studies")
                         .where("study_uid", "=", record.study_uid)
                         .build();

    auto check_result = db_adapter_->select(check_sql);
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

        auto update_builder = db_adapter_->create_query_builder();
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

        auto update_result = db_adapter_->update(update_sql);
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
        auto insert_builder = db_adapter_->create_query_builder();
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

        auto insert_result = db_adapter_->insert(insert_sql);
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
#else
    // PACS_WITH_DATABASE_SYSTEM not defined - use direct SQLite
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

    // Bind parameters
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
#endif
}

auto index_database::find_study(std::string_view study_uid) const
    -> std::optional<study_record> {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Use pacs_database_adapter for unified database operations (Issue #608, #622)
    if (!db_adapter_ || !db_adapter_->is_connected()) {
        return std::nullopt;
    }

    auto builder = db_adapter_->create_query_builder();
    auto select_sql =
        builder
            .select(std::vector<std::string>{
                "study_pk", "patient_pk", "study_uid", "study_id",
                "study_date", "study_time", "accession_number",
                "referring_physician", "study_description",
                "modalities_in_study", "num_series", "num_instances",
                "created_at", "updated_at"})
            .from("studies")
            .where("study_uid", "=", std::string(study_uid))
            .build();

    auto result = db_adapter_->select(select_sql);
    if (result.is_err() || result.value().empty()) {
        return std::nullopt;
    }

    return parse_study_from_adapter_row(result.value()[0]);
#else
    // PACS_WITH_DATABASE_SYSTEM not defined - use direct SQLite
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
#endif
}

auto index_database::find_study_by_pk(int64_t pk) const
    -> std::optional<study_record> {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Use pacs_database_adapter for unified database operations (Issue #608, #622)
    if (!db_adapter_ || !db_adapter_->is_connected()) {
        return std::nullopt;
    }

    auto builder = db_adapter_->create_query_builder();
    auto select_sql =
        builder
            .select(std::vector<std::string>{
                "study_pk", "patient_pk", "study_uid", "study_id",
                "study_date", "study_time", "accession_number",
                "referring_physician", "study_description",
                "modalities_in_study", "num_series", "num_instances",
                "created_at", "updated_at"})
            .from("studies")
            .where("study_pk", "=", pk)
            .build();

    auto result = db_adapter_->select(select_sql);
    if (result.is_err() || result.value().empty()) {
        return std::nullopt;
    }

    return parse_study_from_adapter_row(result.value()[0]);
#else
    // PACS_WITH_DATABASE_SYSTEM not defined - use direct SQLite
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
#endif
}

auto index_database::list_studies(std::string_view patient_id) const
    -> Result<std::vector<study_record>> {
#ifdef PACS_WITH_DATABASE_SYSTEM
    if (db_manager_) {
        // First find patient_pk by patient_id
        auto patient = find_patient(patient_id);
        if (!patient.has_value()) {
            return ok(std::vector<study_record>{});  // No patient, no studies
        }

        database::query_builder builder(database::database_types::sqlite);
        builder.select(std::vector<std::string>{
            "study_pk", "patient_pk", "study_uid", "study_id", "study_date",
            "study_time", "accession_number", "referring_physician",
            "study_description", "modalities_in_study", "num_series",
            "num_instances", "created_at", "updated_at"});
        builder.from("studies");
        builder.where("patient_pk", "=", patient->pk);
        builder.order_by("study_date", database::sort_order::desc);
        builder.order_by("study_time", database::sort_order::desc);

        auto select_sql = builder.build();
        auto result = db_manager_->select_query_result(select_sql);
        if (result.is_err()) {
            return pacs_error<std::vector<study_record>>(
                error_codes::database_query_error,
                pacs::compat::format("Query failed: {}", result.error().message));
        }

        std::vector<study_record> results;
        for (const auto& row : result.value()) {
            results.push_back(parse_study_from_row(row));
        }
        return ok(std::move(results));
    }
#endif

    // Fallback to direct SQLite
    std::vector<study_record> results;

    const char* sql = R"(
        SELECT s.study_pk, s.patient_pk, s.study_uid, s.study_id, s.study_date,
               s.study_time, s.accession_number, s.referring_physician,
               s.study_description, s.modalities_in_study, s.num_series,
               s.num_instances, s.created_at, s.updated_at
        FROM studies s
        JOIN patients p ON s.patient_pk = p.patient_pk
        WHERE p.patient_id = ?
        ORDER BY s.study_date DESC, s.study_time DESC;
    )";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return pacs_error<std::vector<study_record>>(
            error_codes::database_query_error,
            pacs::compat::format("Failed to prepare query: {}", sqlite3_errmsg(db_)));
    }

    sqlite3_bind_text(stmt, 1, patient_id.data(),
                      static_cast<int>(patient_id.size()), SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(parse_study_row(stmt));
    }

    sqlite3_finalize(stmt);
    return ok(std::move(results));
}

auto index_database::search_studies(const study_query& query) const
    -> Result<std::vector<study_record>> {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Use Query Builder when no patient-related filters are present
    // (patient filters require JOIN with patients table)
    if (db_manager_ && !query.patient_id.has_value() &&
        !query.patient_name.has_value()) {
        database::query_builder builder(database::database_types::sqlite);
        builder.select(std::vector<std::string>{
            "study_pk", "patient_pk", "study_uid", "study_id", "study_date",
            "study_time", "accession_number", "referring_physician",
            "study_description", "modalities_in_study", "num_series",
            "num_instances", "created_at", "updated_at"});
        builder.from("studies");

        if (query.study_uid.has_value()) {
            builder.where("study_uid", "=", *query.study_uid);
        }

        if (query.study_id.has_value()) {
            builder.where("study_id", "LIKE", to_like_pattern(*query.study_id));
        }

        if (query.study_date.has_value()) {
            builder.where("study_date", "=", *query.study_date);
        }

        if (query.study_date_from.has_value()) {
            builder.where("study_date", ">=", *query.study_date_from);
        }

        if (query.study_date_to.has_value()) {
            builder.where("study_date", "<=", *query.study_date_to);
        }

        if (query.accession_number.has_value()) {
            builder.where("accession_number", "LIKE",
                          to_like_pattern(*query.accession_number));
        }

        if (query.referring_physician.has_value()) {
            builder.where("referring_physician", "LIKE",
                          to_like_pattern(*query.referring_physician));
        }

        if (query.study_description.has_value()) {
            builder.where("study_description", "LIKE",
                          to_like_pattern(*query.study_description));
        }

        // Note: modality filter requires complex OR conditions
        // which Query Builder may not support well, fallback handles this

        builder.order_by("study_date", database::sort_order::desc);
        builder.order_by("study_time", database::sort_order::desc);

        if (query.limit > 0) {
            builder.limit(static_cast<int>(query.limit));
        }

        if (query.offset > 0) {
            builder.offset(static_cast<int>(query.offset));
        }

        auto select_sql =
            builder.build();
        auto result = db_manager_->select_query_result(select_sql);
        if (result.is_ok()) {
            std::vector<study_record> results;
            for (const auto& row : result.value()) {
                auto record = parse_study_from_row(row);
                // Apply modality filter manually if present
                if (query.modality.has_value()) {
                    const auto& mod = *query.modality;
                    const auto& mods = record.modalities_in_study;
                    if (mods != mod && mods.find("\\" + mod) == std::string::npos &&
                        mods.find(mod + "\\") == std::string::npos) {
                        continue;  // Skip non-matching modality
                    }
                }
                results.push_back(std::move(record));
            }
            return ok(std::move(results));
        }
        // Query failed, fall through to SQLite
    }
#endif

    // Fallback to direct SQLite for complex queries with JOINs
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
        // modalities_in_study contains backslash-separated list
        sql += " AND (s.modalities_in_study = ? OR "
               "s.modalities_in_study LIKE ? OR "
               "s.modalities_in_study LIKE ? OR "
               "s.modalities_in_study LIKE ?)";
        params.push_back(*query.modality);              // Exact match
        params.push_back(*query.modality + "\\%");      // Start
        params.push_back("%\\" + *query.modality);      // End
        params.push_back("%\\" + *query.modality + "\\%");  // Middle
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
        return pacs_error<std::vector<study_record>>(
            error_codes::database_query_error,
            pacs::compat::format("Failed to prepare query: {}", sqlite3_errmsg(db_)));
    }

    // Bind parameters
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

auto index_database::delete_study(std::string_view study_uid) -> VoidResult {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Use pacs_database_adapter for unified database operations (Issue #608, #622)
    if (!db_adapter_ || !db_adapter_->is_connected()) {
        return make_error<std::monostate>(
            -1, "Database adapter not available or not connected", "storage");
    }

    auto builder = db_adapter_->create_query_builder();
    auto delete_sql = builder.delete_from("studies")
                          .where("study_uid", "=", std::string(study_uid))
                          .build();

    auto result = db_adapter_->remove(delete_sql);
    if (result.is_err()) {
        return make_error<std::monostate>(
            error_codes::database_query_error,
            pacs::compat::format("Failed to delete study: {}",
                                 result.error().message),
            "storage");
    }
    return ok();
#else
    // PACS_WITH_DATABASE_SYSTEM not defined - use direct SQLite
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
#endif
}

auto index_database::study_count() const -> Result<size_t> {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Use pacs_database_adapter for unified database operations (Issue #608, #622)
    if (!db_adapter_ || !db_adapter_->is_connected()) {
        return pacs_error<size_t>(
            -1, "Database adapter not available or not connected");
    }

    auto builder = db_adapter_->create_query_builder();
    auto count_sql = builder.select(std::vector<std::string>{"COUNT(*) AS cnt"})
                         .from("studies")
                         .build();

    auto result = db_adapter_->select(count_sql);
    if (result.is_err()) {
        return pacs_error<size_t>(
            error_codes::database_query_error,
            pacs::compat::format("Query failed: {}", result.error().message));
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
#else
    // PACS_WITH_DATABASE_SYSTEM not defined - use direct SQLite
    const char* sql = "SELECT COUNT(*) FROM studies;";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return pacs_error<size_t>(
            error_codes::database_query_error,
            pacs::compat::format("Failed to prepare query: {}", sqlite3_errmsg(db_)));
    }

    size_t count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
    }

    sqlite3_finalize(stmt);
    return ok(count);
#endif
}

auto index_database::study_count(std::string_view patient_id) const -> Result<size_t> {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Use pacs_database_adapter for unified database operations (Issue #608, #622)
    if (!db_adapter_ || !db_adapter_->is_connected()) {
        return pacs_error<size_t>(
            -1, "Database adapter not available or not connected");
    }

    // First find patient_pk by patient_id
    auto patient = find_patient(patient_id);
    if (!patient.has_value()) {
        return ok(static_cast<size_t>(0));  // No patient, no studies
    }

    auto builder = db_adapter_->create_query_builder();
    auto count_sql = builder.select(std::vector<std::string>{"COUNT(*) AS cnt"})
                         .from("studies")
                         .where("patient_pk", "=", patient->pk)
                         .build();

    auto result = db_adapter_->select(count_sql);
    if (result.is_err()) {
        return pacs_error<size_t>(
            error_codes::database_query_error,
            pacs::compat::format("Query failed: {}", result.error().message));
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
#else
    // PACS_WITH_DATABASE_SYSTEM not defined - use direct SQLite
    const char* sql = R"(
        SELECT COUNT(*) FROM studies s
        JOIN patients p ON s.patient_pk = p.patient_pk
        WHERE p.patient_id = ?;
    )";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return pacs_error<size_t>(
            error_codes::database_query_error,
            pacs::compat::format("Failed to prepare query: {}", sqlite3_errmsg(db_)));
    }

    sqlite3_bind_text(stmt, 1, patient_id.data(),
                      static_cast<int>(patient_id.size()), SQLITE_TRANSIENT);

    size_t count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
    }

    sqlite3_finalize(stmt);
    return ok(count);
#endif
}

auto index_database::update_modalities_in_study(int64_t study_pk)
    -> VoidResult {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Use pacs_database_adapter for unified database operations (Issue #608, #622)
    if (!db_adapter_ || !db_adapter_->is_connected()) {
        return make_error<std::monostate>(
            -1, "Database adapter not available or not connected", "storage");
    }

    // Step 1: Get distinct modalities from series table
    auto select_builder = db_adapter_->create_query_builder();
    auto select_sql =
        select_builder
            .select(std::vector<std::string>{"DISTINCT modality"})
            .from("series")
            .where("study_pk", "=", study_pk)
            .build();

    auto select_result = db_adapter_->select(select_sql);
    if (select_result.is_err()) {
        return make_error<std::monostate>(
            error_codes::database_query_error,
            pacs::compat::format("Failed to query modalities: {}",
                                 select_result.error().message),
            "storage");
    }

    // Step 2: Build modalities string with backslash separator
    std::set<std::string> unique_modalities;
    for (const auto& row : select_result.value()) {
        auto it = row.find("modality");
        if (it != row.end() && !it->second.empty()) {
            auto mod = it->second;
            if (!mod.empty()) {
                unique_modalities.insert(mod);
            }
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
    auto update_builder = db_adapter_->create_query_builder();
    auto update_sql =
        update_builder.update("studies")
            .set({{"modalities_in_study", modalities_str},
                  {"updated_at", "datetime('now')"}})
            .where("study_pk", "=", study_pk)
            .build();

    auto update_result = db_adapter_->update(update_sql);
    if (update_result.is_err()) {
        return make_error<std::monostate>(
            error_codes::database_query_error,
            pacs::compat::format("Failed to update modalities: {}",
                                 update_result.error().message),
            "storage");
    }
    return ok();
#else
    // PACS_WITH_DATABASE_SYSTEM not defined - use direct SQLite
    // Note: DICOM uses backslash (\) as multi-value separator
    // SQLite doesn't support GROUP_CONCAT(DISTINCT col, separator), so we use
    // a subquery
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
#endif
}

auto index_database::parse_study_row(void* stmt_ptr) const -> study_record {
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

// ============================================================================
// Series Operations
// ============================================================================

auto index_database::upsert_series(int64_t study_pk,
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

auto index_database::upsert_series(const series_record& record)
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

#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter for unified database operations (Issue #606, #614)
    if (db_adapter_ && db_adapter_->is_connected()) {
        auto builder = db_adapter_->create_query_builder();

        // Check if series exists
        auto check_sql = builder.select(std::vector<std::string>{"series_pk"})
                             .from("series")
                             .where("series_uid", "=", record.series_uid)
                             .build();

        auto check_result = db_adapter_->select(check_sql);
        if (check_result.is_ok()) {
            if (!check_result.value().empty()) {
                // Series exists - update
                auto existing = find_series(record.series_uid);
                if (existing.has_value()) {
                    database::query_builder update_builder(database::database_types::sqlite);
                    update_builder.update("series")
                        .set({{"study_pk", std::to_string(record.study_pk)},
                              {"modality", record.modality},
                              {"series_description", record.series_description},
                              {"body_part_examined", record.body_part_examined},
                              {"station_name", record.station_name},
                              {"updated_at", "datetime('now')"}});

                    if (record.series_number.has_value()) {
                        update_builder.set({{"series_number",
                                             std::to_string(*record.series_number)}});
                    }

                    auto update_sql =
                        update_builder.where("series_uid", "=", record.series_uid)
                            .build();

                    auto update_result = db_adapter_->update(update_sql);
                    if (update_result.is_ok()) {
                        // Update modalities in study
                        (void)update_modalities_in_study(record.study_pk);
                        return existing->pk;
                    }
                    // Update failed, fall through to SQLite
                }
            } else {
                // Series doesn't exist - insert
                database::query_builder insert_builder(database::database_types::sqlite);
                insert_builder.insert_into("series")
                    .values({{"study_pk", std::to_string(record.study_pk)},
                             {"series_uid", record.series_uid},
                             {"modality", record.modality},
                             {"series_description", record.series_description},
                             {"body_part_examined", record.body_part_examined},
                             {"station_name", record.station_name}});

                if (record.series_number.has_value()) {
                    insert_builder.values(
                        {{"series_number", std::to_string(*record.series_number)}});
                }

                auto insert_sql = insert_builder.build();

                auto insert_result = db_adapter_->insert(insert_sql);
                if (insert_result.is_ok()) {
                    // Retrieve the inserted series to get pk
                    auto inserted = find_series(record.series_uid);
                    if (inserted.has_value()) {
                        // Update modalities in study
                        (void)update_modalities_in_study(record.study_pk);
                        return inserted->pk;
                    }
                }
            }
        }
        // Query failed, fall through to direct SQLite
    }

    // Legacy fallback: use database_manager if adapter not available
    if (db_manager_) {
        // Check if series exists
        database::query_builder check_builder(database::database_types::sqlite);
        auto check_sql =
            check_builder
                .select(std::vector<std::string>{"series_pk"})
                .from("series")
                .where("series_uid", "=", record.series_uid)
                .build();

        auto check_result = db_manager_->select_query_result(check_sql);

        if (check_result.is_ok()) {
            if (!check_result.value().empty()) {
                // Series exists - update
                auto existing = find_series(record.series_uid);
                if (existing.has_value()) {
                    database::query_builder update_builder(database::database_types::sqlite);
                    update_builder.update("series")
                        .set({{"study_pk", std::to_string(record.study_pk)},
                              {"modality", record.modality},
                              {"series_description", record.series_description},
                              {"body_part_examined", record.body_part_examined},
                              {"station_name", record.station_name},
                              {"updated_at", "datetime('now')"}});

                    if (record.series_number.has_value()) {
                        update_builder.set({{"series_number",
                                             std::to_string(*record.series_number)}});
                    }

                    auto update_sql =
                        update_builder.where("series_uid", "=", record.series_uid)
                            .build();

                    auto update_result = db_manager_->update_query_result(update_sql);
                    if (update_result.is_ok()) {
                        // Update modalities in study
                        (void)update_modalities_in_study(record.study_pk);
                        return existing->pk;
                    }
                    // Update failed, fall through to SQLite
                }
            } else {
                // Series doesn't exist - insert
                database::query_builder insert_builder(database::database_types::sqlite);
                insert_builder.insert_into("series")
                    .values({{"study_pk", std::to_string(record.study_pk)},
                             {"series_uid", record.series_uid},
                             {"modality", record.modality},
                             {"series_description", record.series_description},
                             {"body_part_examined", record.body_part_examined},
                             {"station_name", record.station_name}});

                if (record.series_number.has_value()) {
                    insert_builder.values(
                        {{"series_number", std::to_string(*record.series_number)}});
                }

                auto insert_sql =
                    insert_builder.build();

                auto insert_result = db_manager_->insert_query_result(insert_sql);
                if (insert_result.is_ok()) {
                    // Retrieve the inserted series to get pk
                    auto inserted = find_series(record.series_uid);
                    if (inserted.has_value()) {
                        // Update modalities in study
                        (void)update_modalities_in_study(record.study_pk);
                        return inserted->pk;
                    }
                }
            }
        }
        // Fall through to SQLite fallback on any error
    }
#endif

    // Fallback to direct SQLite - Use INSERT OR REPLACE for upsert behavior
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
            pacs::compat::format("Failed to prepare statement: {}", sqlite3_errmsg(db_)),
            "storage");
    }

    // Bind parameters
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
            rc, pacs::compat::format("Failed to upsert series: {}", error_msg),
            "storage");
    }

    auto pk = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);

    // Update modalities in study
    (void)update_modalities_in_study(record.study_pk);

    return pk;
}

auto index_database::find_series(std::string_view series_uid) const
    -> std::optional<series_record> {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter for unified database operations (Issue #606, #614)
    if (db_adapter_ && db_adapter_->is_connected()) {
        auto builder = db_adapter_->create_query_builder();
        auto select_sql =
            builder
                .select(std::vector<std::string>{
                    "series_pk", "study_pk", "series_uid", "modality",
                    "series_number", "series_description", "body_part_examined",
                    "station_name", "num_instances", "created_at", "updated_at"})
                .from("series")
                .where("series_uid", "=", std::string(series_uid))
                .build();

        auto result = db_adapter_->select(select_sql);
        if (result.is_ok() && !result.value().empty()) {
            return parse_series_from_adapter_row(result.value()[0]);
        }
        // Fall through to db_manager_ or SQLite on failure
    }

    // Legacy fallback: use database_manager if adapter not available
    if (db_manager_) {
        database::query_builder builder(database::database_types::sqlite);
        auto select_sql =
            builder
                .select(std::vector<std::string>{
                    "series_pk", "study_pk", "series_uid", "modality",
                    "series_number", "series_description", "body_part_examined",
                    "station_name", "num_instances", "created_at", "updated_at"})
                .from("series")
                .where("series_uid", "=", std::string(series_uid))
                .build();

        auto result = db_manager_->select_query_result(select_sql);
        if (result.is_err() || result.value().empty()) {
            return std::nullopt;
        }

        return parse_series_from_row(result.value()[0]);
    }
#endif

    // Fallback to direct SQLite
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

auto index_database::find_series_by_pk(int64_t pk) const
    -> std::optional<series_record> {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter for unified database operations (Issue #606, #614)
    if (db_adapter_ && db_adapter_->is_connected()) {
        auto builder = db_adapter_->create_query_builder();
        auto select_sql =
            builder
                .select(std::vector<std::string>{
                    "series_pk", "study_pk", "series_uid", "modality",
                    "series_number", "series_description", "body_part_examined",
                    "station_name", "num_instances", "created_at", "updated_at"})
                .from("series")
                .where("series_pk", "=", pk)
                .build();

        auto result = db_adapter_->select(select_sql);
        if (result.is_ok() && !result.value().empty()) {
            return parse_series_from_adapter_row(result.value()[0]);
        }
        // Fall through to db_manager_ or SQLite on failure
    }

    // Legacy fallback: use database_manager if adapter not available
    if (db_manager_) {
        database::query_builder builder(database::database_types::sqlite);
        auto select_sql =
            builder
                .select(std::vector<std::string>{
                    "series_pk", "study_pk", "series_uid", "modality",
                    "series_number", "series_description", "body_part_examined",
                    "station_name", "num_instances", "created_at", "updated_at"})
                .from("series")
                .where("series_pk", "=", pk)
                .build();

        auto result = db_manager_->select_query_result(select_sql);
        if (result.is_err() || result.value().empty()) {
            return std::nullopt;
        }

        return parse_series_from_row(result.value()[0]);
    }
#endif

    // Fallback to direct SQLite
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

auto index_database::list_series(std::string_view study_uid) const
    -> Result<std::vector<series_record>> {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter for unified database operations (Issue #606, #614)
    if (db_adapter_ && db_adapter_->is_connected()) {
        // First get study_pk from study_uid
        auto study = find_study(study_uid);
        if (!study.has_value()) {
            return ok(std::vector<series_record>{});
        }

        auto builder = db_adapter_->create_query_builder();
        auto select_sql =
            builder
                .select(std::vector<std::string>{
                    "series_pk", "study_pk", "series_uid", "modality",
                    "series_number", "series_description", "body_part_examined",
                    "station_name", "num_instances", "created_at", "updated_at"})
                .from("series")
                .where("study_pk", "=", study->pk)
                .order_by("series_number")
                .order_by("series_uid")
                .build();

        auto result = db_adapter_->select(select_sql);
        if (result.is_ok()) {
            std::vector<series_record> results;
            for (const auto& row : result.value()) {
                results.push_back(parse_series_from_adapter_row(row));
            }
            return ok(std::move(results));
        }
        // Fall through to db_manager_ or SQLite on failure
    }

    // Legacy fallback: use database_manager if adapter not available
    if (db_manager_) {
        // First get study_pk from study_uid
        auto study = find_study(study_uid);
        if (!study.has_value()) {
            return ok(std::vector<series_record>{});
        }

        database::query_builder builder(database::database_types::sqlite);
        auto select_sql =
            builder
                .select(std::vector<std::string>{
                    "series_pk", "study_pk", "series_uid", "modality",
                    "series_number", "series_description", "body_part_examined",
                    "station_name", "num_instances", "created_at", "updated_at"})
                .from("series")
                .where("study_pk", "=", study->pk)
                .order_by("series_number")
                .order_by("series_uid")
                .build();

        auto result = db_manager_->select_query_result(select_sql);
        if (result.is_err()) {
            return pacs_error<std::vector<series_record>>(
                error_codes::database_query_error,
                pacs::compat::format("Query failed: {}", result.error().message));
        }

        std::vector<series_record> results;
        for (const auto& row : result.value()) {
            results.push_back(parse_series_from_row(row));
        }
        return ok(std::move(results));
    }
#endif

    // Fallback to direct SQLite
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
        return pacs_error<std::vector<series_record>>(
            error_codes::database_query_error,
            pacs::compat::format("Failed to prepare query: {}", sqlite3_errmsg(db_)));
    }

    sqlite3_bind_text(stmt, 1, study_uid.data(),
                      static_cast<int>(study_uid.size()), SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(parse_series_row(stmt));
    }

    sqlite3_finalize(stmt);
    return ok(std::move(results));
}

auto index_database::search_series(const series_query& query) const
    -> Result<std::vector<series_record>> {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter for unified database operations (Issue #606, #614)
    if (db_adapter_ && db_adapter_->is_connected()) {
        auto builder = db_adapter_->create_query_builder();
        builder.select(std::vector<std::string>{
            "series_pk", "study_pk", "series_uid", "modality",
            "series_number", "series_description", "body_part_examined",
            "station_name", "num_instances", "created_at", "updated_at"});
        builder.from("series");

        // If study_uid filter is specified, get study_pk first
        if (query.study_uid.has_value()) {
            auto study = find_study(*query.study_uid);
            if (!study.has_value()) {
                return ok(std::vector<series_record>{});
            }
            builder.where("study_pk", "=", study->pk);
        }

        if (query.series_uid.has_value()) {
            builder.where("series_uid", "=", *query.series_uid);
        }

        if (query.modality.has_value()) {
            builder.where("modality", "=", *query.modality);
        }

        if (query.series_description.has_value()) {
            builder.where("series_description", "LIKE",
                          to_like_pattern(*query.series_description));
        }

        if (query.body_part_examined.has_value()) {
            builder.where("body_part_examined", "=", *query.body_part_examined);
        }

        if (query.series_number.has_value()) {
            builder.where("series_number", "=", *query.series_number);
        }

        builder.order_by("series_number");
        builder.order_by("series_uid");

        if (query.limit > 0) {
            builder.limit(static_cast<int>(query.limit));
        }

        if (query.offset > 0) {
            builder.offset(static_cast<int>(query.offset));
        }

        auto select_sql = builder.build();
        auto result = db_adapter_->select(select_sql);
        if (result.is_ok()) {
            std::vector<series_record> results;
            for (const auto& row : result.value()) {
                results.push_back(parse_series_from_adapter_row(row));
            }
            return ok(std::move(results));
        }
        // Fall through to db_manager_ or SQLite on failure
    }

    // Legacy fallback: use database_manager if adapter not available
    if (db_manager_) {
        database::query_builder builder(database::database_types::sqlite);
        builder.select(std::vector<std::string>{
            "series_pk", "study_pk", "series_uid", "modality",
            "series_number", "series_description", "body_part_examined",
            "station_name", "num_instances", "created_at", "updated_at"});
        builder.from("series");

        // If study_uid filter is specified, get study_pk first
        if (query.study_uid.has_value()) {
            auto study = find_study(*query.study_uid);
            if (!study.has_value()) {
                return ok(std::vector<series_record>{});
            }
            builder.where("study_pk", "=", study->pk);
        }

        if (query.series_uid.has_value()) {
            builder.where("series_uid", "=", *query.series_uid);
        }

        if (query.modality.has_value()) {
            builder.where("modality", "=", *query.modality);
        }

        if (query.series_description.has_value()) {
            builder.where("series_description", "LIKE",
                          to_like_pattern(*query.series_description));
        }

        if (query.body_part_examined.has_value()) {
            builder.where("body_part_examined", "=", *query.body_part_examined);
        }

        if (query.series_number.has_value()) {
            builder.where("series_number", "=", *query.series_number);
        }

        builder.order_by("series_number");
        builder.order_by("series_uid");

        if (query.limit > 0) {
            builder.limit(static_cast<int>(query.limit));
        }

        if (query.offset > 0) {
            builder.offset(static_cast<int>(query.offset));
        }

        auto select_sql = builder.build();
        auto result = db_manager_->select_query_result(select_sql);
        if (result.is_err()) {
            return pacs_error<std::vector<series_record>>(
                error_codes::database_query_error,
                pacs::compat::format("Query failed: {}", result.error().message));
        }

        std::vector<series_record> results;
        for (const auto& row : result.value()) {
            results.push_back(parse_series_from_row(row));
        }
        return ok(std::move(results));
    }
#endif

    // Fallback to direct SQLite
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
        sql += pacs::compat::format(" LIMIT {}", query.limit);
    }

    if (query.offset > 0) {
        sql += pacs::compat::format(" OFFSET {}", query.offset);
    }

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return pacs_error<std::vector<series_record>>(
            error_codes::database_query_error,
            pacs::compat::format("Failed to prepare query: {}", sqlite3_errmsg(db_)));
    }

    // Bind parameters
    int bind_index = 1;
    for (const auto& param : params) {
        sqlite3_bind_text(stmt, bind_index++, param.c_str(), -1,
                          SQLITE_TRANSIENT);
    }

    // Handle series_number separately since it's an int
    if (query.series_number.has_value()) {
        // Need to rebuild query with series_number filter
        // For simplicity, we filter in code for now
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        auto record = parse_series_row(stmt);

        // Filter by series_number if specified
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

auto index_database::delete_series(std::string_view series_uid) -> VoidResult {
    // Get study_pk before deleting
    auto series = find_series(series_uid);
    int64_t study_pk = series ? series->study_pk : 0;

#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter for unified database operations (Issue #606, #614)
    if (db_adapter_ && db_adapter_->is_connected()) {
        auto builder = db_adapter_->create_query_builder();
        auto delete_sql = builder.delete_from("series")
                              .where("series_uid", "=", std::string(series_uid))
                              .build();

        auto result = db_adapter_->remove(delete_sql);
        if (result.is_ok()) {
            // Update modalities in study
            if (study_pk > 0) {
                (void)update_modalities_in_study(study_pk);
            }
            return ok();
        }
        // Fall through to db_manager_ or SQLite on failure
    }

    // Legacy fallback: use database_manager if adapter not available
    if (db_manager_) {
        database::query_builder builder(database::database_types::sqlite);
        auto delete_sql = builder.delete_from("series")
                              .where("series_uid", "=", std::string(series_uid))
                              .build();

        auto result = db_manager_->delete_query_result(delete_sql);
        if (result.is_err()) {
            return make_error<std::monostate>(
                error_codes::database_query_error,
                pacs::compat::format("Failed to delete series: {}",
                                     result.error().message),
                "storage");
        }

        // Update modalities in study
        if (study_pk > 0) {
            (void)update_modalities_in_study(study_pk);
        }
        return ok();
    }
#endif

    // Fallback to direct SQLite
    const char* sql = "DELETE FROM series WHERE series_uid = ?;";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<std::monostate>(
            rc,
            pacs::compat::format("Failed to prepare delete: {}", sqlite3_errmsg(db_)),
            "storage");
    }

    sqlite3_bind_text(stmt, 1, series_uid.data(),
                      static_cast<int>(series_uid.size()), SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return make_error<std::monostate>(
            rc, pacs::compat::format("Failed to delete series: {}", sqlite3_errmsg(db_)),
            "storage");
    }

    // Update modalities in study
    if (study_pk > 0) {
        (void)update_modalities_in_study(study_pk);
    }

    return ok();
}

auto index_database::series_count() const -> Result<size_t> {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter for unified database operations (Issue #606, #614)
    if (db_adapter_ && db_adapter_->is_connected()) {
        auto builder = db_adapter_->create_query_builder();
        auto count_sql = builder.select(std::vector<std::string>{"COUNT(*) AS cnt"})
                             .from("series")
                             .build();

        auto result = db_adapter_->select(count_sql);
        if (result.is_ok() && !result.value().empty()) {
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
        // Fall through to db_manager_ or SQLite on failure
    }

    // Legacy fallback: use database_manager if adapter not available
    if (db_manager_) {
        database::query_builder builder(database::database_types::sqlite);
        auto count_sql = builder.select(std::vector<std::string>{"COUNT(*) AS cnt"})
                             .from("series")
                             .build();

        auto result = db_manager_->select_query_result(count_sql);
        if (result.is_err()) {
            return pacs_error<size_t>(
                error_codes::database_query_error,
                pacs::compat::format("Query failed: {}", result.error().message));
        }

        if (!result.value().empty()) {
            const auto& row = result.value()[0];
            auto it = row.find("cnt");
            if (it != row.end()) {
                if (std::holds_alternative<int64_t>(it->second)) {
                    return ok(static_cast<size_t>(std::get<int64_t>(it->second)));
                }
                if (std::holds_alternative<std::string>(it->second)) {
                    return ok(static_cast<size_t>(
                        std::stoll(std::get<std::string>(it->second))));
                }
            }
        }
        return ok(static_cast<size_t>(0));
    }
#endif

    // Fallback to direct SQLite
    const char* sql = "SELECT COUNT(*) FROM series;";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return pacs_error<size_t>(
            error_codes::database_query_error,
            pacs::compat::format("Failed to prepare query: {}", sqlite3_errmsg(db_)));
    }

    size_t count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
    }

    sqlite3_finalize(stmt);
    return ok(count);
}

auto index_database::series_count(std::string_view study_uid) const -> Result<size_t> {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter for unified database operations (Issue #606, #614)
    if (db_adapter_ && db_adapter_->is_connected()) {
        // First get study_pk from study_uid
        auto study = find_study(study_uid);
        if (!study.has_value()) {
            return ok(static_cast<size_t>(0));
        }

        auto builder = db_adapter_->create_query_builder();
        auto count_sql = builder.select(std::vector<std::string>{"COUNT(*) AS cnt"})
                             .from("series")
                             .where("study_pk", "=", study->pk)
                             .build();

        auto result = db_adapter_->select(count_sql);
        if (result.is_ok() && !result.value().empty()) {
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
        // Fall through to db_manager_ or SQLite on failure
    }

    // Legacy fallback: use database_manager if adapter not available
    if (db_manager_) {
        // First get study_pk from study_uid
        auto study = find_study(study_uid);
        if (!study.has_value()) {
            return ok(static_cast<size_t>(0));
        }

        database::query_builder builder(database::database_types::sqlite);
        auto count_sql = builder.select(std::vector<std::string>{"COUNT(*) AS cnt"})
                             .from("series")
                             .where("study_pk", "=", study->pk)
                             .build();

        auto result = db_manager_->select_query_result(count_sql);
        if (result.is_err()) {
            return pacs_error<size_t>(
                error_codes::database_query_error,
                pacs::compat::format("Query failed: {}", result.error().message));
        }

        if (!result.value().empty()) {
            const auto& row = result.value()[0];
            auto it = row.find("cnt");
            if (it != row.end()) {
                if (std::holds_alternative<int64_t>(it->second)) {
                    return ok(static_cast<size_t>(std::get<int64_t>(it->second)));
                }
                if (std::holds_alternative<std::string>(it->second)) {
                    return ok(static_cast<size_t>(
                        std::stoll(std::get<std::string>(it->second))));
                }
            }
        }
        return ok(static_cast<size_t>(0));
    }
#endif

    // Fallback to direct SQLite
    const char* sql = R"(
        SELECT COUNT(*) FROM series se
        JOIN studies st ON se.study_pk = st.study_pk
        WHERE st.study_uid = ?;
    )";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return pacs_error<size_t>(
            error_codes::database_query_error,
            pacs::compat::format("Failed to prepare query: {}", sqlite3_errmsg(db_)));
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

auto index_database::parse_series_row(void* stmt_ptr) const -> series_record {
    auto* stmt = static_cast<sqlite3_stmt*>(stmt_ptr);
    series_record record;

    record.pk = sqlite3_column_int64(stmt, 0);
    record.study_pk = sqlite3_column_int64(stmt, 1);
    record.series_uid = get_text(stmt, 2);
    record.modality = get_text(stmt, 3);

    // Handle nullable series_number
    if (sqlite3_column_type(stmt, 4) != SQLITE_NULL) {
        record.series_number = sqlite3_column_int(stmt, 4);
    }

    record.series_description = get_text(stmt, 5);
    record.body_part_examined = get_text(stmt, 6);
    record.station_name = get_text(stmt, 7);
    record.num_instances = sqlite3_column_int(stmt, 8);

    auto created_str = get_text(stmt, 9);
    record.created_at = parse_datetime(created_str.c_str());

    auto updated_str = get_text(stmt, 10);
    record.updated_at = parse_datetime(updated_str.c_str());

    return record;
}

// ============================================================================
// Instance Operations
// ============================================================================

auto index_database::upsert_instance(int64_t series_pk,
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

auto index_database::upsert_instance(const instance_record& record)
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

#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter for unified database operations (Issue #606, #614)
    if (db_adapter_ && db_adapter_->is_connected()) {
        auto builder = db_adapter_->create_query_builder();

        // Check if instance exists
        auto check_sql = builder.select(std::vector<std::string>{"instance_pk"})
                             .from("instances")
                             .where("sop_uid", "=", record.sop_uid)
                             .build();

        auto check_result = db_adapter_->select(check_sql);
        if (check_result.is_ok()) {
            if (!check_result.value().empty()) {
                // Instance exists - update
                auto existing = find_instance(record.sop_uid);
                if (existing.has_value()) {
                    database::query_builder update_builder(database::database_types::sqlite);
                    update_builder.update("instances")
                        .set({{"series_pk", std::to_string(record.series_pk)},
                              {"sop_class_uid", record.sop_class_uid},
                              {"transfer_syntax", record.transfer_syntax},
                              {"content_date", record.content_date},
                              {"content_time", record.content_time},
                              {"file_path", record.file_path},
                              {"file_size", std::to_string(record.file_size)},
                              {"file_hash", record.file_hash}});

                    if (record.instance_number.has_value()) {
                        update_builder.set({{"instance_number",
                                             std::to_string(*record.instance_number)}});
                    }
                    if (record.rows.has_value()) {
                        update_builder.set({{"rows",
                                             std::to_string(*record.rows)}});
                    }
                    if (record.columns.has_value()) {
                        update_builder.set({{"columns",
                                             std::to_string(*record.columns)}});
                    }
                    if (record.bits_allocated.has_value()) {
                        update_builder.set({{"bits_allocated",
                                             std::to_string(*record.bits_allocated)}});
                    }
                    if (record.number_of_frames.has_value()) {
                        update_builder.set({{"number_of_frames",
                                             std::to_string(*record.number_of_frames)}});
                    }

                    auto update_sql =
                        update_builder.where("sop_uid", "=", record.sop_uid)
                            .build();

                    auto update_result = db_adapter_->update(update_sql);
                    if (update_result.is_ok()) {
                        return existing->pk;
                    }
                    // Update failed, fall through to SQLite
                }
            } else {
                // Instance doesn't exist - insert
                database::query_builder insert_builder(database::database_types::sqlite);
                insert_builder.insert_into("instances")
                    .values({{"series_pk", std::to_string(record.series_pk)},
                             {"sop_uid", record.sop_uid},
                             {"sop_class_uid", record.sop_class_uid},
                             {"transfer_syntax", record.transfer_syntax},
                             {"content_date", record.content_date},
                             {"content_time", record.content_time},
                             {"file_path", record.file_path},
                             {"file_size", std::to_string(record.file_size)},
                             {"file_hash", record.file_hash}});

                if (record.instance_number.has_value()) {
                    insert_builder.values(
                        {{"instance_number", std::to_string(*record.instance_number)}});
                }
                if (record.rows.has_value()) {
                    insert_builder.values(
                        {{"rows", std::to_string(*record.rows)}});
                }
                if (record.columns.has_value()) {
                    insert_builder.values(
                        {{"columns", std::to_string(*record.columns)}});
                }
                if (record.bits_allocated.has_value()) {
                    insert_builder.values(
                        {{"bits_allocated", std::to_string(*record.bits_allocated)}});
                }
                if (record.number_of_frames.has_value()) {
                    insert_builder.values(
                        {{"number_of_frames", std::to_string(*record.number_of_frames)}});
                }

                auto insert_sql = insert_builder.build();

                auto insert_result = db_adapter_->insert(insert_sql);
                if (insert_result.is_ok()) {
                    // Retrieve the inserted instance to get pk
                    auto inserted = find_instance(record.sop_uid);
                    if (inserted.has_value()) {
                        return inserted->pk;
                    }
                }
            }
        }
        // Query failed, fall through to direct SQLite
    }
#endif

    // Fallback to direct SQLite: Use INSERT OR REPLACE for upsert behavior
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
            pacs::compat::format("Failed to prepare statement: {}", sqlite3_errmsg(db_)),
            "storage");
    }

    // Bind parameters
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
            rc, pacs::compat::format("Failed to upsert instance: {}", error_msg),
            "storage");
    }

    auto pk = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);

    return pk;
}

auto index_database::find_instance(std::string_view sop_uid) const
    -> std::optional<instance_record> {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter for unified database operations (Issue #606, #614)
    if (db_adapter_ && db_adapter_->is_connected()) {
        auto builder = db_adapter_->create_query_builder();
        auto select_sql =
            builder
                .select(std::vector<std::string>{
                    "instance_pk", "series_pk", "sop_uid", "sop_class_uid",
                    "instance_number", "transfer_syntax", "content_date",
                    "content_time", "rows", "columns", "bits_allocated",
                    "number_of_frames", "file_path", "file_size", "file_hash",
                    "created_at"})
                .from("instances")
                .where("sop_uid", "=", std::string(sop_uid))
                .build();

        auto result = db_adapter_->select(select_sql);
        if (result.is_ok() && !result.value().empty()) {
            return parse_instance_from_adapter_row(result.value()[0]);
        }
        // Fall through to db_manager_ or SQLite on failure
    }

    // Legacy fallback: use database_manager if adapter not available
    if (db_manager_) {
        database::query_builder builder(database::database_types::sqlite);
        auto select_sql =
            builder
                .select(std::vector<std::string>{
                    "instance_pk", "series_pk", "sop_uid", "sop_class_uid",
                    "instance_number", "transfer_syntax", "content_date",
                    "content_time", "rows", "columns", "bits_allocated",
                    "number_of_frames", "file_path", "file_size", "file_hash",
                    "created_at"})
                .from("instances")
                .where("sop_uid", "=", std::string(sop_uid))
                .build();

        auto result = db_manager_->select_query_result(select_sql);
        if (result.is_err() || result.value().empty()) {
            return std::nullopt;
        }

        return parse_instance_from_row(result.value()[0]);
    }
#endif

    // Fallback to direct SQLite
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

auto index_database::find_instance_by_pk(int64_t pk) const
    -> std::optional<instance_record> {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter for unified database operations (Issue #606, #614)
    if (db_adapter_ && db_adapter_->is_connected()) {
        auto builder = db_adapter_->create_query_builder();
        auto select_sql =
            builder
                .select(std::vector<std::string>{
                    "instance_pk", "series_pk", "sop_uid", "sop_class_uid",
                    "instance_number", "transfer_syntax", "content_date",
                    "content_time", "rows", "columns", "bits_allocated",
                    "number_of_frames", "file_path", "file_size", "file_hash",
                    "created_at"})
                .from("instances")
                .where("instance_pk", "=", pk)
                .build();

        auto result = db_adapter_->select(select_sql);
        if (result.is_ok() && !result.value().empty()) {
            return parse_instance_from_adapter_row(result.value()[0]);
        }
        // Fall through to db_manager_ or SQLite on failure
    }

    // Legacy fallback: use database_manager if adapter not available
    if (db_manager_) {
        database::query_builder builder(database::database_types::sqlite);
        auto select_sql =
            builder
                .select(std::vector<std::string>{
                    "instance_pk", "series_pk", "sop_uid", "sop_class_uid",
                    "instance_number", "transfer_syntax", "content_date",
                    "content_time", "rows", "columns", "bits_allocated",
                    "number_of_frames", "file_path", "file_size", "file_hash",
                    "created_at"})
                .from("instances")
                .where("instance_pk", "=", pk)
                .build();

        auto result = db_manager_->select_query_result(select_sql);
        if (result.is_err() || result.value().empty()) {
            return std::nullopt;
        }

        return parse_instance_from_row(result.value()[0]);
    }
#endif

    // Fallback to direct SQLite
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

auto index_database::list_instances(std::string_view series_uid) const
    -> Result<std::vector<instance_record>> {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter for unified database operations (Issue #606, #614)
    if (db_adapter_ && db_adapter_->is_connected()) {
        // First get series_pk from series_uid
        auto series = find_series(series_uid);
        if (!series.has_value()) {
            return ok(std::vector<instance_record>{});
        }

        auto builder = db_adapter_->create_query_builder();
        builder.select(std::vector<std::string>{
            "instance_pk", "series_pk", "sop_uid", "sop_class_uid",
            "instance_number", "transfer_syntax", "content_date",
            "content_time", "rows", "columns", "bits_allocated",
            "number_of_frames", "file_path", "file_size", "file_hash",
            "created_at"});
        builder.from("instances");
        builder.where("series_pk", "=", series->pk);
        builder.order_by("instance_number");
        builder.order_by("sop_uid");

        auto select_sql = builder.build();
        auto result = db_adapter_->select(select_sql);
        if (result.is_ok()) {
            std::vector<instance_record> results;
            for (const auto& row : result.value()) {
                results.push_back(parse_instance_from_adapter_row(row));
            }
            return ok(std::move(results));
        }
        // Fall through to db_manager_ or SQLite on failure
    }

    // Legacy fallback: use database_manager if adapter not available
    if (db_manager_) {
        // First get series_pk from series_uid
        auto series = find_series(series_uid);
        if (!series.has_value()) {
            return ok(std::vector<instance_record>{});
        }

        database::query_builder builder(database::database_types::sqlite);
        builder.select(std::vector<std::string>{
            "instance_pk", "series_pk", "sop_uid", "sop_class_uid",
            "instance_number", "transfer_syntax", "content_date",
            "content_time", "rows", "columns", "bits_allocated",
            "number_of_frames", "file_path", "file_size", "file_hash",
            "created_at"});
        builder.from("instances");
        builder.where("series_pk", "=", series->pk);
        builder.order_by("instance_number");
        builder.order_by("sop_uid");

        auto select_sql = builder.build();
        auto result = db_manager_->select_query_result(select_sql);
        if (result.is_err()) {
            return pacs_error<std::vector<instance_record>>(
                error_codes::database_query_error,
                pacs::compat::format("Query failed: {}", result.error().message));
        }

        std::vector<instance_record> results;
        for (const auto& row : result.value()) {
            results.push_back(parse_instance_from_row(row));
        }
        return ok(std::move(results));
    }
#endif

    // Fallback to direct SQLite
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
        return pacs_error<std::vector<instance_record>>(
            error_codes::database_query_error,
            pacs::compat::format("Failed to prepare query: {}", sqlite3_errmsg(db_)));
    }

    sqlite3_bind_text(stmt, 1, series_uid.data(),
                      static_cast<int>(series_uid.size()), SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(parse_instance_row(stmt));
    }

    sqlite3_finalize(stmt);
    return ok(std::move(results));
}

auto index_database::search_instances(const instance_query& query) const
    -> Result<std::vector<instance_record>> {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter for unified database operations (Issue #606, #614)
    if (db_adapter_ && db_adapter_->is_connected()) {
        auto builder = db_adapter_->create_query_builder();
        builder.select(std::vector<std::string>{
            "instance_pk", "series_pk", "sop_uid", "sop_class_uid",
            "instance_number", "transfer_syntax", "content_date",
            "content_time", "rows", "columns", "bits_allocated",
            "number_of_frames", "file_path", "file_size", "file_hash",
            "created_at"});
        builder.from("instances");

        // Handle series_uid filter by getting series_pk first
        if (query.series_uid.has_value()) {
            auto series = find_series(*query.series_uid);
            if (!series.has_value()) {
                return ok(std::vector<instance_record>{});
            }
            builder.where("series_pk", "=", series->pk);
        }

        if (query.sop_uid.has_value()) {
            builder.where("sop_uid", "=", *query.sop_uid);
        }

        if (query.sop_class_uid.has_value()) {
            builder.where("sop_class_uid", "=", *query.sop_class_uid);
        }

        if (query.content_date.has_value()) {
            builder.where("content_date", "=", *query.content_date);
        }

        if (query.content_date_from.has_value()) {
            builder.where("content_date", ">=", *query.content_date_from);
        }

        if (query.content_date_to.has_value()) {
            builder.where("content_date", "<=", *query.content_date_to);
        }

        if (query.instance_number.has_value()) {
            builder.where("instance_number", "=", *query.instance_number);
        }

        builder.order_by("instance_number");
        builder.order_by("sop_uid");

        if (query.limit > 0) {
            builder.limit(static_cast<int>(query.limit));
        }

        if (query.offset > 0) {
            builder.offset(static_cast<int>(query.offset));
        }

        auto select_sql = builder.build();
        auto result = db_adapter_->select(select_sql);
        if (result.is_ok()) {
            std::vector<instance_record> results;
            for (const auto& row : result.value()) {
                results.push_back(parse_instance_from_adapter_row(row));
            }
            return ok(std::move(results));
        }
        // Fall through to db_manager_ or SQLite on failure
    }

    // Legacy fallback: use database_manager if adapter not available
    if (db_manager_) {
        database::query_builder builder(database::database_types::sqlite);
        builder.select(std::vector<std::string>{
            "instance_pk", "series_pk", "sop_uid", "sop_class_uid",
            "instance_number", "transfer_syntax", "content_date",
            "content_time", "rows", "columns", "bits_allocated",
            "number_of_frames", "file_path", "file_size", "file_hash",
            "created_at"});
        builder.from("instances");

        // Handle series_uid filter by getting series_pk first
        if (query.series_uid.has_value()) {
            auto series = find_series(*query.series_uid);
            if (!series.has_value()) {
                return ok(std::vector<instance_record>{});
            }
            builder.where("series_pk", "=", series->pk);
        }

        if (query.sop_uid.has_value()) {
            builder.where("sop_uid", "=", *query.sop_uid);
        }

        if (query.sop_class_uid.has_value()) {
            builder.where("sop_class_uid", "=", *query.sop_class_uid);
        }

        if (query.content_date.has_value()) {
            builder.where("content_date", "=", *query.content_date);
        }

        if (query.content_date_from.has_value()) {
            builder.where("content_date", ">=", *query.content_date_from);
        }

        if (query.content_date_to.has_value()) {
            builder.where("content_date", "<=", *query.content_date_to);
        }

        if (query.instance_number.has_value()) {
            builder.where("instance_number", "=", *query.instance_number);
        }

        builder.order_by("instance_number");
        builder.order_by("sop_uid");

        if (query.limit > 0) {
            builder.limit(static_cast<int>(query.limit));
        }

        if (query.offset > 0) {
            builder.offset(static_cast<int>(query.offset));
        }

        auto select_sql = builder.build();
        auto result = db_manager_->select_query_result(select_sql);
        if (result.is_err()) {
            return pacs_error<std::vector<instance_record>>(
                error_codes::database_query_error,
                pacs::compat::format("Query failed: {}", result.error().message));
        }

        std::vector<instance_record> results;
        for (const auto& row : result.value()) {
            results.push_back(parse_instance_from_row(row));
        }
        return ok(std::move(results));
    }
#endif

    // Fallback to direct SQLite
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
        sql += pacs::compat::format(" LIMIT {}", query.limit);
    }

    if (query.offset > 0) {
        sql += pacs::compat::format(" OFFSET {}", query.offset);
    }

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return pacs_error<std::vector<instance_record>>(
            error_codes::database_query_error,
            pacs::compat::format("Failed to prepare query: {}", sqlite3_errmsg(db_)));
    }

    // Bind parameters
    int bind_index = 1;
    for (const auto& param : params) {
        sqlite3_bind_text(stmt, bind_index++, param.c_str(), -1,
                          SQLITE_TRANSIENT);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        auto record = parse_instance_row(stmt);

        // Filter by instance_number if specified
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

auto index_database::delete_instance(std::string_view sop_uid) -> VoidResult {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter for unified database operations (Issue #606, #614)
    if (db_adapter_ && db_adapter_->is_connected()) {
        auto builder = db_adapter_->create_query_builder();
        auto delete_sql = builder.delete_from("instances")
                              .where("sop_uid", "=", std::string(sop_uid))
                              .build();

        auto result = db_adapter_->remove(delete_sql);
        if (result.is_ok()) {
            return ok();
        }
        // Fall through to db_manager_ or SQLite on failure
    }

    // Legacy fallback: use database_manager if adapter not available
    if (db_manager_) {
        database::query_builder builder(database::database_types::sqlite);
        auto delete_sql = builder.delete_from("instances")
                              .where("sop_uid", "=", std::string(sop_uid))
                              .build();

        auto result = db_manager_->delete_query_result(delete_sql);
        if (result.is_err()) {
            return make_error<std::monostate>(
                database_query_error,
                pacs::compat::format("Delete failed: {}", result.error().message),
                "storage");
        }
        return ok();
    }
#endif

    // Fallback to direct SQLite
    const char* sql = "DELETE FROM instances WHERE sop_uid = ?;";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<std::monostate>(
            rc,
            pacs::compat::format("Failed to prepare delete: {}", sqlite3_errmsg(db_)),
            "storage");
    }

    sqlite3_bind_text(stmt, 1, sop_uid.data(),
                      static_cast<int>(sop_uid.size()), SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return make_error<std::monostate>(
            rc,
            pacs::compat::format("Failed to delete instance: {}", sqlite3_errmsg(db_)),
            "storage");
    }

    return ok();
}

auto index_database::instance_count() const -> Result<size_t> {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter for unified database operations (Issue #606, #614)
    if (db_adapter_ && db_adapter_->is_connected()) {
        auto builder = db_adapter_->create_query_builder();
        auto count_sql = builder.select(std::vector<std::string>{"COUNT(*) AS cnt"})
                             .from("instances")
                             .build();

        auto result = db_adapter_->select(count_sql);
        if (result.is_ok() && !result.value().empty()) {
            const auto& row = result.value()[0];
            auto it = row.find("cnt");
            if (it != row.end() && !it->second.empty()) {
                try {
                    return ok(static_cast<size_t>(std::stoll(it->second)));
                } catch (...) {
                    // Fall through to db_manager_
                }
            }
        }
        // Fall through to db_manager_ on failure
    }

    // Legacy fallback: use database_manager if adapter not available
    if (db_manager_) {
        database::query_builder builder(database::database_types::sqlite);
        auto count_sql = builder.select(std::vector<std::string>{"COUNT(*)"})
                             .from("instances")
                             .build();

        auto result = db_manager_->select_query_result(count_sql);
        if (result.is_err()) {
            return pacs_error<size_t>(
                error_codes::database_query_error,
                pacs::compat::format("Count query failed: {}", result.error().message));
        }

        if (!result.value().empty()) {
            const auto& row = result.value()[0];
            auto it = row.find("COUNT(*)");
            if (it != row.end()) {
                if (std::holds_alternative<int64_t>(it->second)) {
                    return ok(static_cast<size_t>(std::get<int64_t>(it->second)));
                } else if (std::holds_alternative<std::string>(it->second)) {
                    return ok(static_cast<size_t>(std::stoll(std::get<std::string>(it->second))));
                }
            }
        }
        return ok(size_t{0});
    }
#endif

    // Fallback to direct SQLite
    const char* sql = "SELECT COUNT(*) FROM instances;";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return pacs_error<size_t>(
            error_codes::database_query_error,
            pacs::compat::format("Failed to prepare query: {}", sqlite3_errmsg(db_)));
    }

    size_t count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
    }

    sqlite3_finalize(stmt);
    return ok(count);
}

auto index_database::instance_count(std::string_view series_uid) const
    -> Result<size_t> {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter for unified database operations (Issue #606, #614)
    if (db_adapter_ && db_adapter_->is_connected()) {
        // First get series_pk from series_uid
        auto series = find_series(series_uid);
        if (!series.has_value()) {
            return ok(size_t{0});
        }

        auto builder = db_adapter_->create_query_builder();
        auto count_sql = builder.select(std::vector<std::string>{"COUNT(*) AS cnt"})
                             .from("instances")
                             .where("series_pk", "=", series->pk)
                             .build();

        auto result = db_adapter_->select(count_sql);
        if (result.is_ok() && !result.value().empty()) {
            const auto& row = result.value()[0];
            auto it = row.find("cnt");
            if (it != row.end() && !it->second.empty()) {
                try {
                    return ok(static_cast<size_t>(std::stoll(it->second)));
                } catch (...) {
                    // Fall through to db_manager_
                }
            }
        }
        // Fall through to db_manager_ on failure
    }

    // Legacy fallback: use database_manager if adapter not available
    if (db_manager_) {
        // First get series_pk from series_uid
        auto series = find_series(series_uid);
        if (!series.has_value()) {
            return ok(size_t{0});
        }

        database::query_builder builder(database::database_types::sqlite);
        auto count_sql = builder.select(std::vector<std::string>{"COUNT(*)"})
                             .from("instances")
                             .where("series_pk", "=", series->pk)
                             .build();

        auto result = db_manager_->select_query_result(count_sql);
        if (result.is_err()) {
            return pacs_error<size_t>(
                error_codes::database_query_error,
                pacs::compat::format("Count query failed: {}", result.error().message));
        }

        if (!result.value().empty()) {
            const auto& row = result.value()[0];
            auto it = row.find("COUNT(*)");
            if (it != row.end()) {
                if (std::holds_alternative<int64_t>(it->second)) {
                    return ok(static_cast<size_t>(std::get<int64_t>(it->second)));
                } else if (std::holds_alternative<std::string>(it->second)) {
                    return ok(static_cast<size_t>(std::stoll(std::get<std::string>(it->second))));
                }
            }
        }
        return ok(size_t{0});
    }
#endif

    // Fallback to direct SQLite
    const char* sql = R"(
        SELECT COUNT(*) FROM instances i
        JOIN series s ON i.series_pk = s.series_pk
        WHERE s.series_uid = ?;
    )";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return pacs_error<size_t>(
            error_codes::database_query_error,
            pacs::compat::format("Failed to prepare query: {}", sqlite3_errmsg(db_)));
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

auto index_database::parse_instance_row(void* stmt_ptr) const
    -> instance_record {
    auto* stmt = static_cast<sqlite3_stmt*>(stmt_ptr);
    instance_record record;

    record.pk = sqlite3_column_int64(stmt, 0);
    record.series_pk = sqlite3_column_int64(stmt, 1);
    record.sop_uid = get_text(stmt, 2);
    record.sop_class_uid = get_text(stmt, 3);

    // Handle nullable instance_number
    if (sqlite3_column_type(stmt, 4) != SQLITE_NULL) {
        record.instance_number = sqlite3_column_int(stmt, 4);
    }

    record.transfer_syntax = get_text(stmt, 5);
    record.content_date = get_text(stmt, 6);
    record.content_time = get_text(stmt, 7);

    // Handle nullable image properties
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

    auto created_str = get_text(stmt, 15);
    record.created_at = parse_datetime(created_str.c_str());

    return record;
}

// ============================================================================
// File Path Lookup Operations
// ============================================================================

auto index_database::get_file_path(std::string_view sop_instance_uid) const
    -> Result<std::optional<std::string>> {
#ifdef PACS_WITH_DATABASE_SYSTEM
    if (db_manager_) {
        database::query_builder builder(database::database_types::sqlite);
        auto select_sql = builder.select(std::vector<std::string>{"file_path"})
                              .from("instances")
                              .where("sop_uid", "=", std::string(sop_instance_uid))
                              .build();

        auto result = db_manager_->select_query_result(select_sql);
        if (result.is_err() || result.value().empty()) {
            return ok(std::optional<std::string>(std::nullopt));
        }

        auto path = get_string_value(result.value()[0], "file_path");
        return ok(std::optional<std::string>(path));
    }
#endif

    // Fallback to direct SQLite
    const char* sql = "SELECT file_path FROM instances WHERE sop_uid = ?;";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return pacs_error<std::optional<std::string>>(
            error_codes::database_query_error,
            pacs::compat::format("Failed to prepare query: {}", sqlite3_errmsg(db_)));
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

auto index_database::get_study_files(std::string_view study_instance_uid) const
    -> Result<std::vector<std::string>> {
#ifdef PACS_WITH_DATABASE_SYSTEM
    if (db_manager_) {
        // Get all series for this study, then get all instances for each series
        auto study = find_study(study_instance_uid);
        if (!study.has_value()) {
            return ok(std::vector<std::string>{});
        }

        // Get all series PKs for this study
        database::query_builder series_builder(database::database_types::sqlite);
        auto series_sql = series_builder.select(std::vector<std::string>{"series_pk"})
                              .from("series")
                              .where("study_pk", "=", study->pk)
                              .order_by("series_number")
                              .build();

        auto series_result = db_manager_->select_query_result(series_sql);
        if (series_result.is_err() || series_result.value().empty()) {
            return ok(std::vector<std::string>{});
        }

        std::vector<std::string> results;
        for (const auto& series_row : series_result.value()) {
            auto series_pk = get_int64_value(series_row, "series_pk");

            database::query_builder inst_builder(database::database_types::sqlite);
            auto inst_sql = inst_builder.select(std::vector<std::string>{"file_path"})
                                .from("instances")
                                .where("series_pk", "=", series_pk)
                                .order_by("instance_number")
                                .build();

            auto inst_result = db_manager_->select_query_result(inst_sql);
            if (inst_result.is_ok()) {
                for (const auto& inst_row : inst_result.value()) {
                    results.push_back(get_string_value(inst_row, "file_path"));
                }
            }
        }
        return ok(std::move(results));
    }
#endif

    // Fallback to direct SQLite
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
        return pacs_error<std::vector<std::string>>(
            error_codes::database_query_error,
            pacs::compat::format("Failed to prepare query: {}", sqlite3_errmsg(db_)));
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

auto index_database::get_series_files(std::string_view series_instance_uid)
    const -> Result<std::vector<std::string>> {
#ifdef PACS_WITH_DATABASE_SYSTEM
    if (db_manager_) {
        auto series = find_series(series_instance_uid);
        if (!series.has_value()) {
            return ok(std::vector<std::string>{});
        }

        database::query_builder builder(database::database_types::sqlite);
        auto select_sql = builder.select(std::vector<std::string>{"file_path"})
                              .from("instances")
                              .where("series_pk", "=", series->pk)
                              .order_by("instance_number")
                              .build();

        auto result = db_manager_->select_query_result(select_sql);
        if (result.is_err()) {
            return pacs_error<std::vector<std::string>>(
                error_codes::database_query_error,
                pacs::compat::format("Query failed: {}", result.error().message));
        }

        std::vector<std::string> results;
        for (const auto& row : result.value()) {
            results.push_back(get_string_value(row, "file_path"));
        }
        return ok(std::move(results));
    }
#endif

    // Fallback to direct SQLite
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
        return pacs_error<std::vector<std::string>>(
            error_codes::database_query_error,
            pacs::compat::format("Failed to prepare query: {}", sqlite3_errmsg(db_)));
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

// ============================================================================
// Database Maintenance Operations
// ============================================================================

auto index_database::vacuum() -> VoidResult {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter for unified database operations (Issue #616)
    if (db_adapter_ && db_adapter_->is_connected()) {
        return db_adapter_->execute("VACUUM;");
    }
#endif

    auto rc = sqlite3_exec(db_, "VACUUM;", nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<std::monostate>(
            rc, pacs::compat::format("VACUUM failed: {}", sqlite3_errmsg(db_)),
            "storage");
    }
    return ok();
}

auto index_database::analyze() -> VoidResult {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter for unified database operations (Issue #616)
    if (db_adapter_ && db_adapter_->is_connected()) {
        return db_adapter_->execute("ANALYZE;");
    }
#endif

    auto rc = sqlite3_exec(db_, "ANALYZE;", nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<std::monostate>(
            rc, pacs::compat::format("ANALYZE failed: {}", sqlite3_errmsg(db_)),
            "storage");
    }
    return ok();
}

auto index_database::verify_integrity() const -> VoidResult {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter for unified database operations (Issue #616)
    if (db_adapter_ && db_adapter_->is_connected()) {
        auto result = db_adapter_->select("PRAGMA integrity_check;");
        if (result.is_err()) {
            return make_error<std::monostate>(
                result.error().code, result.error().message, "storage");
        }

        for (const auto& row : result.value()) {
            auto it = row.find("integrity_check");
            if (it != row.end() && it->second != "ok") {
                return make_error<std::monostate>(
                    -1, pacs::compat::format("Integrity check failed: {}", it->second),
                    "storage");
            }
        }
        return ok();
    }
#endif

    const char* sql = "PRAGMA integrity_check;";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<std::monostate>(
            rc,
            pacs::compat::format("Failed to prepare integrity check: {}",
                       sqlite3_errmsg(db_)),
            "storage");
    }

    std::string result;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        result = get_text(stmt, 0);
        if (result != "ok") {
            sqlite3_finalize(stmt);
            return make_error<std::monostate>(
                -1, pacs::compat::format("Integrity check failed: {}", result),
                "storage");
        }
    }

    sqlite3_finalize(stmt);
    return ok();
}

auto index_database::checkpoint(bool truncate) -> VoidResult {
    const char* sql =
        truncate ? "PRAGMA wal_checkpoint(TRUNCATE);"
                 : "PRAGMA wal_checkpoint(PASSIVE);";

#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter for unified database operations (Issue #616)
    if (db_adapter_ && db_adapter_->is_connected()) {
        return db_adapter_->execute(sql);
    }
#endif

    auto rc = sqlite3_exec(db_, sql, nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<std::monostate>(
            rc, pacs::compat::format("Checkpoint failed: {}", sqlite3_errmsg(db_)),
            "storage");
    }
    return ok();
}

auto index_database::native_handle() const noexcept -> sqlite3* {
    return db_;
}

#ifdef PACS_WITH_DATABASE_SYSTEM
auto index_database::db_manager() const noexcept
    -> std::shared_ptr<database::database_manager> {
    return db_manager_;
}
#endif

// ============================================================================
// Storage Statistics
// ============================================================================

auto index_database::get_storage_stats() const -> Result<storage_stats> {
    storage_stats stats;

    auto patient_count_result = patient_count();
    if (patient_count_result.is_err()) {
        return Result<storage_stats>::err(patient_count_result.error());
    }
    stats.total_patients = patient_count_result.value();

    auto study_count_result = study_count();
    if (study_count_result.is_err()) {
        return Result<storage_stats>::err(study_count_result.error());
    }
    stats.total_studies = study_count_result.value();

    auto series_count_result = series_count();
    if (series_count_result.is_err()) {
        return Result<storage_stats>::err(series_count_result.error());
    }
    stats.total_series = series_count_result.value();

    auto instance_count_result = instance_count();
    if (instance_count_result.is_err()) {
        return Result<storage_stats>::err(instance_count_result.error());
    }
    stats.total_instances = instance_count_result.value();

    // Get total file size
    const char* file_size_sql =
        "SELECT COALESCE(SUM(file_size), 0) AS total_size FROM instances;";

#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter for unified database operations (Issue #616)
    if (db_adapter_ && db_adapter_->is_connected()) {
        auto result = db_adapter_->select(file_size_sql);
        if (result.is_err()) {
            return Result<storage_stats>::err(result.error());
        }
        if (!result.value().empty()) {
            auto it = result.value()[0].find("total_size");
            if (it != result.value()[0].end()) {
                try {
                    stats.total_file_size = std::stoll(it->second);
                } catch (const std::exception&) {
                    stats.total_file_size = 0;
                }
            }
        }
    } else {
#endif
        sqlite3_stmt* stmt = nullptr;
        auto rc = sqlite3_prepare_v2(db_, file_size_sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            return pacs_error<storage_stats>(
                error_codes::database_query_error,
                pacs::compat::format("Failed to prepare query: {}",
                                     sqlite3_errmsg(db_)));
        }

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            stats.total_file_size = sqlite3_column_int64(stmt, 0);
        }
        sqlite3_finalize(stmt);
#ifdef PACS_WITH_DATABASE_SYSTEM
    }
#endif

    // Get database file size
    if (path_ != ":memory:") {
        std::error_code ec;
        auto size = std::filesystem::file_size(path_, ec);
        if (!ec) {
            stats.database_size = static_cast<int64_t>(size);
        }
    }

    return ok(std::move(stats));
}

// ============================================================================
// MPPS Operations
// ============================================================================

auto index_database::create_mpps(std::string_view mpps_uid,
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
    record.status = "IN PROGRESS";  // N-CREATE always starts with IN PROGRESS
    return create_mpps(record);
}

auto index_database::create_mpps(const mpps_record& record) -> Result<int64_t> {
    if (record.mpps_uid.empty()) {
        return make_error<int64_t>(-1, "MPPS SOP Instance UID is required",
                                   "storage");
    }

    // Validate status - N-CREATE must start with IN PROGRESS
    if (!record.status.empty() && record.status != "IN PROGRESS") {
        return make_error<int64_t>(
            -1, "N-CREATE must create MPPS with status 'IN PROGRESS'",
            "storage");
    }

#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter (Issue #615)
    if (db_adapter_ && db_adapter_->is_connected()) {
        auto builder = db_adapter_->create_query_builder();
        auto insert_sql =
            builder.insert_into("mpps")
                .values({{"mpps_uid", record.mpps_uid},
                         {"status", std::string("IN PROGRESS")},
                         {"start_datetime", record.start_datetime},
                         {"station_ae", record.station_ae},
                         {"station_name", record.station_name},
                         {"modality", record.modality},
                         {"study_uid", record.study_uid},
                         {"accession_no", record.accession_no},
                         {"scheduled_step_id", record.scheduled_step_id},
                         {"requested_proc_id", record.requested_proc_id},
                         {"performed_series", record.performed_series}})
                .build();

        auto insert_result = db_adapter_->insert(insert_sql);
        if (insert_result.is_ok()) {
            // Retrieve the inserted MPPS to get pk
            auto inserted = find_mpps(record.mpps_uid);
            if (inserted.has_value()) {
                return inserted->pk;
            }
        }
        // Fall through if insert failed
    }

    // Legacy fallback: database_manager
    if (db_manager_) {
        database::query_builder builder(database::database_types::sqlite);
        auto insert_sql =
            builder.insert_into("mpps")
                .values({{"mpps_uid", record.mpps_uid},
                         {"status", std::string("IN PROGRESS")},
                         {"start_datetime", record.start_datetime},
                         {"station_ae", record.station_ae},
                         {"station_name", record.station_name},
                         {"modality", record.modality},
                         {"study_uid", record.study_uid},
                         {"accession_no", record.accession_no},
                         {"scheduled_step_id", record.scheduled_step_id},
                         {"requested_proc_id", record.requested_proc_id},
                         {"performed_series", record.performed_series}})
                .build();

        auto insert_result = db_manager_->insert_query_result(insert_sql);
        if (insert_result.is_ok()) {
            // Retrieve the inserted MPPS to get pk
            auto inserted = find_mpps(record.mpps_uid);
            if (inserted.has_value()) {
                return inserted->pk;
            }
        }
        // Insert failed, fall through to SQLite
    }

    // database_system is required
    return make_error<int64_t>(-1,
                               "database_system is not initialized for MPPS creation",
                               "storage");
#endif
}

auto index_database::update_mpps(std::string_view mpps_uid,
                                 std::string_view new_status,
                                 std::string_view end_datetime,
                                 std::string_view performed_series)
    -> VoidResult {
    // Validate new status
    if (new_status != "COMPLETED" && new_status != "DISCONTINUED" &&
        new_status != "IN PROGRESS") {
        return make_error<std::monostate>(
            -1,
            "Invalid status. Must be 'IN PROGRESS', 'COMPLETED', or "
            "'DISCONTINUED'",
            "storage");
    }

    // Check current status to validate transition
    auto current = find_mpps(mpps_uid);
    if (!current.has_value()) {
        return make_error<std::monostate>(-1, "MPPS not found", "storage");
    }

    // Validate status transition
    if (current->status == "COMPLETED" || current->status == "DISCONTINUED") {
        return make_error<std::monostate>(
            -1,
            pacs::compat::format("Cannot update MPPS in final state '{}'. COMPLETED and "
                       "DISCONTINUED are final states.",
                       current->status),
            "storage");
    }

#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter (Issue #615)
    if (db_adapter_ && db_adapter_->is_connected()) {
        auto builder = db_adapter_->create_query_builder();
        builder.update("mpps");
        builder.set({{"status", std::string(new_status)}});

        // Only update end_datetime if not empty
        if (!end_datetime.empty()) {
            builder.set({{"end_datetime", std::string(end_datetime)}});
        }

        // Only update performed_series if not empty
        if (!performed_series.empty()) {
            builder.set({{"performed_series", std::string(performed_series)}});
        }

        builder.where("mpps_uid", "=", std::string(mpps_uid));

        auto update_sql = builder.build();
        auto result = db_adapter_->update(update_sql);
        if (result.is_ok()) {
            return ok();
        }
        // Fall through if update failed
    }

    // Legacy fallback: database_manager
    if (db_manager_) {
        database::query_builder builder(database::database_types::sqlite);
        builder.update("mpps");
        builder.set({{"status", std::string(new_status)}});

        // Only update end_datetime if not empty
        if (!end_datetime.empty()) {
            builder.set({{"end_datetime", std::string(end_datetime)}});
        }

        // Only update performed_series if not empty
        if (!performed_series.empty()) {
            builder.set({{"performed_series", std::string(performed_series)}});
        }

        builder.where("mpps_uid", "=", std::string(mpps_uid));

        auto update_sql =
            builder.build();
        auto result = db_manager_->update_query_result(update_sql);
        if (result.is_ok()) {
            return ok();
        }
        // Update failed
    }

    // database_system is required
    return make_error<std::monostate>(-1,
                                      "database_system is not initialized for MPPS update",
                                      "storage");
#endif
}

auto index_database::update_mpps(const mpps_record& record) -> VoidResult {
    if (record.mpps_uid.empty()) {
        return make_error<std::monostate>(-1, "MPPS UID is required for update",
                                          "storage");
    }

    return update_mpps(record.mpps_uid, record.status, record.end_datetime,
                       record.performed_series);
}

auto index_database::find_mpps(std::string_view mpps_uid) const
    -> std::optional<mpps_record> {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter (Issue #615)
    if (db_adapter_ && db_adapter_->is_connected()) {
        auto builder = db_adapter_->create_query_builder();
        auto select_sql =
            builder
                .select(std::vector<std::string>{
                    "mpps_pk", "mpps_uid", "status", "start_datetime",
                    "end_datetime", "station_ae", "station_name", "modality",
                    "study_uid", "accession_no", "scheduled_step_id",
                    "requested_proc_id", "performed_series", "created_at",
                    "updated_at"})
                .from("mpps")
                .where("mpps_uid", "=", std::string(mpps_uid))
                .build();

        auto result = db_adapter_->select(select_sql);
        if (result.is_ok() && !result.value().empty()) {
            return parse_mpps_from_adapter_row(result.value()[0]);
        }
        // Fall through if query failed
    }

    // Legacy fallback: database_manager
    if (db_manager_) {
        database::query_builder builder(database::database_types::sqlite);
        auto select_sql =
            builder
                .select(std::vector<std::string>{
                    "mpps_pk", "mpps_uid", "status", "start_datetime",
                    "end_datetime", "station_ae", "station_name", "modality",
                    "study_uid", "accession_no", "scheduled_step_id",
                    "requested_proc_id", "performed_series", "created_at",
                    "updated_at"})
                .from("mpps")
                .where("mpps_uid", "=", std::string(mpps_uid))
                .build();

        auto result = db_manager_->select_query_result(select_sql);
        if (result.is_err() || result.value().empty()) {
            return std::nullopt;
        }

        return parse_mpps_from_row(result.value()[0]);
    }

    // database_system is required
    return std::nullopt;
#endif
}

auto index_database::find_mpps_by_pk(int64_t pk) const
    -> std::optional<mpps_record> {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter (Issue #615)
    if (db_adapter_ && db_adapter_->is_connected()) {
        auto builder = db_adapter_->create_query_builder();
        auto select_sql =
            builder
                .select(std::vector<std::string>{
                    "mpps_pk", "mpps_uid", "status", "start_datetime",
                    "end_datetime", "station_ae", "station_name", "modality",
                    "study_uid", "accession_no", "scheduled_step_id",
                    "requested_proc_id", "performed_series", "created_at",
                    "updated_at"})
                .from("mpps")
                .where("mpps_pk", "=", pk)
                .build();

        auto result = db_adapter_->select(select_sql);
        if (result.is_ok() && !result.value().empty()) {
            return parse_mpps_from_adapter_row(result.value()[0]);
        }
        // Fall through if query failed
    }

    // Legacy fallback: database_manager
    if (db_manager_) {
        database::query_builder builder(database::database_types::sqlite);
        auto select_sql =
            builder
                .select(std::vector<std::string>{
                    "mpps_pk", "mpps_uid", "status", "start_datetime",
                    "end_datetime", "station_ae", "station_name", "modality",
                    "study_uid", "accession_no", "scheduled_step_id",
                    "requested_proc_id", "performed_series", "created_at",
                    "updated_at"})
                .from("mpps")
                .where("mpps_pk", "=", pk)
                .build();

        auto result = db_manager_->select_query_result(select_sql);
        if (result.is_err() || result.value().empty()) {
            return std::nullopt;
        }

        return parse_mpps_from_row(result.value()[0]);
    }

    // database_system is required
    return std::nullopt;
#endif
}

auto index_database::list_active_mpps(std::string_view station_ae) const
    -> Result<std::vector<mpps_record>> {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter (Issue #615)
    if (db_adapter_ && db_adapter_->is_connected()) {
        auto builder = db_adapter_->create_query_builder();
        builder.select(std::vector<std::string>{
            "mpps_pk", "mpps_uid", "status", "start_datetime", "end_datetime",
            "station_ae", "station_name", "modality", "study_uid",
            "accession_no", "scheduled_step_id", "requested_proc_id",
            "performed_series", "created_at", "updated_at"});
        builder.from("mpps");
        builder.where("status", "=", std::string("IN PROGRESS"));
        builder.where("station_ae", "=", std::string(station_ae));
        builder.order_by("start_datetime", database::sort_order::desc);

        auto select_sql = builder.build();
        auto result = db_adapter_->select(select_sql);
        if (result.is_ok()) {
            std::vector<mpps_record> results;
            for (const auto& row : result.value()) {
                results.push_back(parse_mpps_from_adapter_row(row));
            }
            return ok(std::move(results));
        }
        // Fall through if query failed
    }

    // Legacy fallback: database_manager
    if (db_manager_) {
        database::query_builder builder(database::database_types::sqlite);
        builder.select(std::vector<std::string>{
            "mpps_pk", "mpps_uid", "status", "start_datetime", "end_datetime",
            "station_ae", "station_name", "modality", "study_uid",
            "accession_no", "scheduled_step_id", "requested_proc_id",
            "performed_series", "created_at", "updated_at"});
        builder.from("mpps");
        builder.where("status", "=", std::string("IN PROGRESS"));
        builder.where("station_ae", "=", std::string(station_ae));
        builder.order_by("start_datetime", database::sort_order::desc);

        auto select_sql =
            builder.build();
        auto result = db_manager_->select_query_result(select_sql);
        if (result.is_err()) {
            return pacs_error<std::vector<mpps_record>>(
                error_codes::database_query_error,
                pacs::compat::format("Query failed: {}", result.error().message));
        }

        std::vector<mpps_record> results;
        for (const auto& row : result.value()) {
            results.push_back(parse_mpps_from_row(row));
        }
        return ok(std::move(results));
    }

    // database_system is required
    return make_error<std::vector<mpps_record>>(
        -1, "database_system is not initialized for MPPS query", "storage");
#endif
}

auto index_database::find_mpps_by_study(std::string_view study_uid) const
    -> Result<std::vector<mpps_record>> {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter (Issue #615)
    if (db_adapter_ && db_adapter_->is_connected()) {
        auto builder = db_adapter_->create_query_builder();
        builder.select(std::vector<std::string>{
            "mpps_pk", "mpps_uid", "status", "start_datetime", "end_datetime",
            "station_ae", "station_name", "modality", "study_uid",
            "accession_no", "scheduled_step_id", "requested_proc_id",
            "performed_series", "created_at", "updated_at"});
        builder.from("mpps");
        builder.where("study_uid", "=", std::string(study_uid));
        builder.order_by("start_datetime", database::sort_order::desc);

        auto select_sql = builder.build();
        auto result = db_adapter_->select(select_sql);
        if (result.is_ok()) {
            std::vector<mpps_record> results;
            for (const auto& row : result.value()) {
                results.push_back(parse_mpps_from_adapter_row(row));
            }
            return ok(std::move(results));
        }
        // Fall through if query failed
    }

    // Legacy fallback: database_manager
    if (db_manager_) {
        database::query_builder builder(database::database_types::sqlite);
        builder.select(std::vector<std::string>{
            "mpps_pk", "mpps_uid", "status", "start_datetime", "end_datetime",
            "station_ae", "station_name", "modality", "study_uid",
            "accession_no", "scheduled_step_id", "requested_proc_id",
            "performed_series", "created_at", "updated_at"});
        builder.from("mpps");
        builder.where("study_uid", "=", std::string(study_uid));
        builder.order_by("start_datetime", database::sort_order::desc);

        auto select_sql =
            builder.build();
        auto result = db_manager_->select_query_result(select_sql);
        if (result.is_err()) {
            return pacs_error<std::vector<mpps_record>>(
                error_codes::database_query_error,
                pacs::compat::format("Query failed: {}", result.error().message));
        }

        std::vector<mpps_record> results;
        for (const auto& row : result.value()) {
            results.push_back(parse_mpps_from_row(row));
        }
        return ok(std::move(results));
    }

    // database_system is required
    return make_error<std::vector<mpps_record>>(
        -1, "database_system is not initialized for MPPS query", "storage");
#endif
}

auto index_database::search_mpps(const mpps_query& query) const
    -> Result<std::vector<mpps_record>> {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter (Issue #615)
    if (db_adapter_ && db_adapter_->is_connected()) {
        auto builder = db_adapter_->create_query_builder();
        builder.select(std::vector<std::string>{
            "mpps_pk", "mpps_uid", "status", "start_datetime", "end_datetime",
            "station_ae", "station_name", "modality", "study_uid",
            "accession_no", "scheduled_step_id", "requested_proc_id",
            "performed_series", "created_at", "updated_at"});
        builder.from("mpps");

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
            builder.where(database::query_condition(
                pacs::compat::format("substr(start_datetime, 1, 8) >= '{}'",
                                     *query.start_date_from)));
        }

        if (query.start_date_to.has_value()) {
            builder.where(database::query_condition(pacs::compat::format(
                "substr(start_datetime, 1, 8) <= '{}'", *query.start_date_to)));
        }

        builder.order_by("start_datetime", database::sort_order::desc);

        if (query.limit > 0) {
            builder.limit(static_cast<int>(query.limit));
        }

        if (query.offset > 0) {
            builder.offset(static_cast<int>(query.offset));
        }

        auto select_sql = builder.build();
        auto result = db_adapter_->select(select_sql);
        if (result.is_ok()) {
            std::vector<mpps_record> results;
            for (const auto& row : result.value()) {
                results.push_back(parse_mpps_from_adapter_row(row));
            }
            return ok(std::move(results));
        }
        // Fall through if query failed
    }

    // Legacy fallback: database_manager
    if (db_manager_) {
        database::query_builder builder(database::database_types::sqlite);
        builder.select(std::vector<std::string>{
            "mpps_pk", "mpps_uid", "status", "start_datetime", "end_datetime",
            "station_ae", "station_name", "modality", "study_uid",
            "accession_no", "scheduled_step_id", "requested_proc_id",
            "performed_series", "created_at", "updated_at"});
        builder.from("mpps");

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
            builder.where(database::query_condition(
                pacs::compat::format("substr(start_datetime, 1, 8) >= '{}'",
                                     *query.start_date_from)));
        }

        if (query.start_date_to.has_value()) {
            builder.where(database::query_condition(pacs::compat::format(
                "substr(start_datetime, 1, 8) <= '{}'", *query.start_date_to)));
        }

        builder.order_by("start_datetime", database::sort_order::desc);

        if (query.limit > 0) {
            builder.limit(static_cast<int>(query.limit));
        }

        if (query.offset > 0) {
            builder.offset(static_cast<int>(query.offset));
        }

        auto select_sql =
            builder.build();
        auto result = db_manager_->select_query_result(select_sql);
        if (result.is_err()) {
            return pacs_error<std::vector<mpps_record>>(
                error_codes::database_query_error,
                pacs::compat::format("Query failed: {}", result.error().message));
        }

        std::vector<mpps_record> results;
        for (const auto& row : result.value()) {
            results.push_back(parse_mpps_from_row(row));
        }
        return ok(std::move(results));
    }

    // database_system is required
    return pacs_error<std::vector<mpps_record>>(
        error_codes::database_query_error,
        "database_system is not initialized for MPPS search");
#endif
}

auto index_database::delete_mpps(std::string_view mpps_uid) -> VoidResult {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter (Issue #615)
    if (db_adapter_ && db_adapter_->is_connected()) {
        auto builder = db_adapter_->create_query_builder();
        auto delete_sql = builder.delete_from("mpps")
                              .where("mpps_uid", "=", std::string(mpps_uid))
                              .build();

        auto result = db_adapter_->remove(delete_sql);
        if (result.is_ok()) {
            return ok();
        }
        // Fall through if delete failed
    }

    // Legacy fallback: database_manager
    if (db_manager_) {
        database::query_builder builder(database::database_types::sqlite);
        auto delete_sql = builder.delete_from("mpps")
                              .where("mpps_uid", "=", std::string(mpps_uid))
                              .build();

        auto result = db_manager_->delete_query_result(delete_sql);
        if (result.is_err()) {
            return make_error<std::monostate>(
                database_query_error,
                pacs::compat::format("Failed to delete MPPS: {}",
                                     result.error().message),
                "storage");
        }
        return ok();
    }

    // database_system is required
    return make_error<std::monostate>(
        database_query_error,
        "database_system is not initialized for MPPS deletion",
        "storage");
#endif
}

auto index_database::mpps_count() const -> Result<size_t> {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter (Issue #615)
    if (db_adapter_ && db_adapter_->is_connected()) {
        auto builder = db_adapter_->create_query_builder();
        auto count_sql = builder.select(std::vector<std::string>{"COUNT(*)"})
                             .from("mpps")
                             .build();

        auto result = db_adapter_->select(count_sql);
        if (result.is_ok() && !result.value().empty()) {
            const auto& row = result.value()[0];
            auto it = row.find("COUNT(*)");
            if (it != row.end() && !it->second.empty()) {
                try {
                    return ok(static_cast<size_t>(std::stoll(it->second)));
                } catch (...) {
                    return ok(size_t{0});
                }
            }
        }
        // Fall through if query failed
    }

    // Legacy fallback: database_manager
    if (db_manager_) {
        database::query_builder builder(database::database_types::sqlite);
        auto count_sql = builder.select(std::vector<std::string>{"COUNT(*)"})
                             .from("mpps")
                             .build();

        auto result = db_manager_->select_query_result(count_sql);
        if (result.is_err()) {
            return pacs_error<size_t>(
                error_codes::database_query_error,
                pacs::compat::format("Query failed: {}", result.error().message));
        }

        if (!result.value().empty()) {
            const auto& row = result.value()[0];
            auto it = row.find("COUNT(*)");
            if (it != row.end()) {
                if (std::holds_alternative<int64_t>(it->second)) {
                    return ok(static_cast<size_t>(std::get<int64_t>(it->second)));
                } else if (std::holds_alternative<std::string>(it->second)) {
                    return ok(static_cast<size_t>(
                        std::stoll(std::get<std::string>(it->second))));
                }
            }
        }
        return ok(size_t{0});
    }

    // database_system is required
    return pacs_error<size_t>(
        error_codes::database_query_error,
        "database_system is not initialized for MPPS count");
#endif
}

auto index_database::mpps_count(std::string_view status) const -> Result<size_t> {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter (Issue #615)
    if (db_adapter_ && db_adapter_->is_connected()) {
        auto builder = db_adapter_->create_query_builder();
        auto count_sql = builder.select(std::vector<std::string>{"COUNT(*)"})
                             .from("mpps")
                             .where("status", "=", std::string(status))
                             .build();

        auto result = db_adapter_->select(count_sql);
        if (result.is_ok() && !result.value().empty()) {
            const auto& row = result.value()[0];
            auto it = row.find("COUNT(*)");
            if (it != row.end() && !it->second.empty()) {
                try {
                    return ok(static_cast<size_t>(std::stoll(it->second)));
                } catch (...) {
                    return ok(size_t{0});
                }
            }
        }
        // Fall through if query failed
    }

    // Legacy fallback: database_manager
    if (db_manager_) {
        database::query_builder builder(database::database_types::sqlite);
        auto count_sql = builder.select(std::vector<std::string>{"COUNT(*)"})
                             .from("mpps")
                             .where("status", "=", std::string(status))
                             .build();

        auto result = db_manager_->select_query_result(count_sql);
        if (result.is_err()) {
            return pacs_error<size_t>(
                error_codes::database_query_error,
                pacs::compat::format("Query failed: {}", result.error().message));
        }

        if (!result.value().empty()) {
            const auto& row = result.value()[0];
            auto it = row.find("COUNT(*)");
            if (it != row.end()) {
                if (std::holds_alternative<int64_t>(it->second)) {
                    return ok(static_cast<size_t>(std::get<int64_t>(it->second)));
                } else if (std::holds_alternative<std::string>(it->second)) {
                    return ok(static_cast<size_t>(
                        std::stoll(std::get<std::string>(it->second))));
                }
            }
        }
        return ok(size_t{0});
    }

    // database_system is required
    return pacs_error<size_t>(
        error_codes::database_query_error,
        "database_system is not initialized for MPPS count");
#endif
}

auto index_database::parse_mpps_row(void* stmt_ptr) const -> mpps_record {
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

    auto created_str = get_text(stmt, 13);
    record.created_at = parse_datetime(created_str.c_str());

    auto updated_str = get_text(stmt, 14);
    record.updated_at = parse_datetime(updated_str.c_str());

    return record;
}

#ifdef PACS_WITH_DATABASE_SYSTEM
auto index_database::parse_mpps_from_row(
    const std::map<std::string, database::core::database_value>& row) const
    -> mpps_record {
    mpps_record record;

    record.pk = get_int64_value(row, "mpps_pk");
    record.mpps_uid = get_string_value(row, "mpps_uid");
    record.status = get_string_value(row, "status");
    record.start_datetime = get_string_value(row, "start_datetime");
    record.end_datetime = get_string_value(row, "end_datetime");
    record.station_ae = get_string_value(row, "station_ae");
    record.station_name = get_string_value(row, "station_name");
    record.modality = get_string_value(row, "modality");
    record.study_uid = get_string_value(row, "study_uid");
    record.accession_no = get_string_value(row, "accession_no");
    record.scheduled_step_id = get_string_value(row, "scheduled_step_id");
    record.requested_proc_id = get_string_value(row, "requested_proc_id");
    record.performed_series = get_string_value(row, "performed_series");

    auto created_str = get_string_value(row, "created_at");
    record.created_at = parse_datetime(created_str.c_str());

    auto updated_str = get_string_value(row, "updated_at");
    record.updated_at = parse_datetime(updated_str.c_str());

    return record;
}

auto index_database::parse_worklist_from_row(
    const std::map<std::string, database::core::database_value>& row) const
    -> worklist_item {
    worklist_item item;

    item.pk = get_int64_value(row, "worklist_pk");
    item.step_id = get_string_value(row, "step_id");
    item.step_status = get_string_value(row, "step_status");
    item.patient_id = get_string_value(row, "patient_id");
    item.patient_name = get_string_value(row, "patient_name");
    item.birth_date = get_string_value(row, "birth_date");
    item.sex = get_string_value(row, "sex");
    item.accession_no = get_string_value(row, "accession_no");
    item.requested_proc_id = get_string_value(row, "requested_proc_id");
    item.study_uid = get_string_value(row, "study_uid");
    item.scheduled_datetime = get_string_value(row, "scheduled_datetime");
    item.station_ae = get_string_value(row, "station_ae");
    item.station_name = get_string_value(row, "station_name");
    item.modality = get_string_value(row, "modality");
    item.procedure_desc = get_string_value(row, "procedure_desc");
    item.protocol_code = get_string_value(row, "protocol_code");
    item.referring_phys = get_string_value(row, "referring_phys");
    item.referring_phys_id = get_string_value(row, "referring_phys_id");

    auto created_str = get_string_value(row, "created_at");
    item.created_at = parse_datetime(created_str.c_str());

    auto updated_str = get_string_value(row, "updated_at");
    item.updated_at = parse_datetime(updated_str.c_str());

    return item;
}

auto index_database::parse_audit_from_row(
    const std::map<std::string, database::core::database_value>& row) const
    -> audit_record {
    audit_record record;

    record.pk = get_int64_value(row, "audit_pk");
    record.event_type = get_string_value(row, "event_type");
    record.outcome = get_string_value(row, "outcome");

    auto timestamp_str = get_string_value(row, "timestamp");
    record.timestamp = parse_datetime(timestamp_str.c_str());

    record.user_id = get_string_value(row, "user_id");
    record.source_ae = get_string_value(row, "source_ae");
    record.target_ae = get_string_value(row, "target_ae");
    record.source_ip = get_string_value(row, "source_ip");
    record.patient_id = get_string_value(row, "patient_id");
    record.study_uid = get_string_value(row, "study_uid");
    record.message = get_string_value(row, "message");
    record.details = get_string_value(row, "details");

    return record;
}
#endif

// ============================================================================
// Worklist Operations
// ============================================================================

auto index_database::add_worklist_item(const worklist_item& item)
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

#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter (Issue #615)
    if (db_adapter_ && db_adapter_->is_connected()) {
        auto builder = db_adapter_->create_query_builder();
        auto insert_sql =
            builder.insert_into("worklist")
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
                         {"referring_phys_id", item.referring_phys_id}})
                .build();

        auto insert_result = db_adapter_->insert(insert_sql);
        if (insert_result.is_ok()) {
            // Retrieve the inserted worklist item to get pk
            auto inserted = find_worklist_item(item.step_id, item.accession_no);
            if (inserted.has_value()) {
                return inserted->pk;
            }
        }
        // Insert failed, fall through to legacy
    }

    // Legacy fallback: database_manager
    if (db_manager_) {
        database::query_builder builder(database::database_types::sqlite);
        auto insert_sql =
            builder.insert_into("worklist")
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
                         {"referring_phys_id", item.referring_phys_id}})
                .build();

        auto insert_result = db_manager_->insert_query_result(insert_sql);
        if (insert_result.is_ok()) {
            // Retrieve the inserted worklist item to get pk
            auto inserted = find_worklist_item(item.step_id, item.accession_no);
            if (inserted.has_value()) {
                return inserted->pk;
            }
        }
        // Insert failed, fall through to SQLite
    }

    // database_system is required
    return make_error<int64_t>(-1,
                               "database_system is not initialized for worklist creation",
                               "storage");
#endif
}

auto index_database::update_worklist_status(std::string_view step_id,
                                            std::string_view accession_no,
                                            std::string_view new_status)
    -> VoidResult {
    // Validate status
    if (new_status != "SCHEDULED" && new_status != "STARTED" &&
        new_status != "COMPLETED") {
        return make_error<std::monostate>(
            -1, "Invalid status. Must be 'SCHEDULED', 'STARTED', or 'COMPLETED'",
            "storage");
    }

#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter (Issue #615)
    if (db_adapter_ && db_adapter_->is_connected()) {
        auto builder = db_adapter_->create_query_builder();
        builder.update("worklist");
        builder.set({{"step_status", std::string(new_status)}});
        builder.where("step_id", "=", std::string(step_id));
        builder.where("accession_no", "=", std::string(accession_no));

        auto update_sql = builder.build();
        auto result = db_adapter_->update(update_sql);
        if (result.is_ok()) {
            return ok();
        }
        // Update failed, fall through to legacy
    }

    // Legacy fallback: database_manager
    if (db_manager_) {
        database::query_builder builder(database::database_types::sqlite);
        builder.update("worklist");
        builder.set({{"step_status", std::string(new_status)}});
        builder.where("step_id", "=", std::string(step_id));
        builder.where("accession_no", "=", std::string(accession_no));

        auto update_sql = builder.build();
        auto result = db_manager_->update_query_result(update_sql);
        if (result.is_ok()) {
            return ok();
        }
    }

    // database_system is required
    return make_error<std::monostate>(
        database_query_error,
        "database_system is not initialized for worklist update",
        "storage");
#endif
}

auto index_database::query_worklist(const worklist_query& query) const
    -> Result<std::vector<worklist_item>> {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter (Issue #615)
    if (db_adapter_ && db_adapter_->is_connected()) {
        auto builder = db_adapter_->create_query_builder();
        builder.select(std::vector<std::string>{
            "worklist_pk", "step_id", "step_status", "patient_id",
            "patient_name", "birth_date", "sex", "accession_no",
            "requested_proc_id", "study_uid", "scheduled_datetime",
            "station_ae", "station_name", "modality", "procedure_desc",
            "protocol_code", "referring_phys", "referring_phys_id",
            "created_at", "updated_at"});
        builder.from("worklist");

        // Default: only return SCHEDULED items unless include_all_status is set
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
                pacs::compat::format("substr(scheduled_datetime, 1, 8) >= '{}'",
                                     *query.scheduled_date_from)));
        }

        if (query.scheduled_date_to.has_value()) {
            builder.where(database::query_condition(
                pacs::compat::format("substr(scheduled_datetime, 1, 8) <= '{}'",
                                     *query.scheduled_date_to)));
        }

        if (query.patient_id.has_value()) {
            builder.where("patient_id", "LIKE", to_like_pattern(*query.patient_id));
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
            builder.limit(static_cast<int>(query.limit));
        }

        if (query.offset > 0) {
            builder.offset(static_cast<int>(query.offset));
        }

        auto select_sql = builder.build();
        auto result = db_adapter_->select(select_sql);
        if (result.is_ok()) {
            std::vector<worklist_item> results;
            for (const auto& row : result.value()) {
                results.push_back(parse_worklist_from_adapter_row(row));
            }
            return ok(std::move(results));
        }
        // Query failed, fall through to legacy
    }

    // Legacy fallback: database_manager
    if (db_manager_) {
        database::query_builder builder(database::database_types::sqlite);
        builder.select(std::vector<std::string>{
            "worklist_pk", "step_id", "step_status", "patient_id",
            "patient_name", "birth_date", "sex", "accession_no",
            "requested_proc_id", "study_uid", "scheduled_datetime",
            "station_ae", "station_name", "modality", "procedure_desc",
            "protocol_code", "referring_phys", "referring_phys_id",
            "created_at", "updated_at"});
        builder.from("worklist");

        // Default: only return SCHEDULED items unless include_all_status is set
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
                pacs::compat::format("substr(scheduled_datetime, 1, 8) >= '{}'",
                                     *query.scheduled_date_from)));
        }

        if (query.scheduled_date_to.has_value()) {
            builder.where(database::query_condition(
                pacs::compat::format("substr(scheduled_datetime, 1, 8) <= '{}'",
                                     *query.scheduled_date_to)));
        }

        if (query.patient_id.has_value()) {
            builder.where("patient_id", "LIKE", to_like_pattern(*query.patient_id));
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
            builder.limit(static_cast<int>(query.limit));
        }

        if (query.offset > 0) {
            builder.offset(static_cast<int>(query.offset));
        }

        auto select_sql = builder.build();
        auto result = db_manager_->select_query_result(select_sql);
        if (result.is_err()) {
            return pacs_error<std::vector<worklist_item>>(
                error_codes::database_query_error,
                pacs::compat::format("Query failed: {}", result.error().message));
        }

        std::vector<worklist_item> results;
        for (const auto& row : result.value()) {
            results.push_back(parse_worklist_from_row(row));
        }
        return ok(std::move(results));
    }

    // database_system is required
    return pacs_error<std::vector<worklist_item>>(
        error_codes::database_query_error,
        "database_system is not initialized for worklist query");
#endif
}

auto index_database::find_worklist_item(std::string_view step_id,
                                        std::string_view accession_no) const
    -> std::optional<worklist_item> {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter (Issue #615)
    if (db_adapter_ && db_adapter_->is_connected()) {
        auto builder = db_adapter_->create_query_builder();
        auto select_sql =
            builder
                .select(std::vector<std::string>{
                    "worklist_pk", "step_id", "step_status", "patient_id",
                    "patient_name", "birth_date", "sex", "accession_no",
                    "requested_proc_id", "study_uid", "scheduled_datetime",
                    "station_ae", "station_name", "modality", "procedure_desc",
                    "protocol_code", "referring_phys", "referring_phys_id",
                    "created_at", "updated_at"})
                .from("worklist")
                .where("step_id", "=", std::string(step_id))
                .where("accession_no", "=", std::string(accession_no))
                .build();

        auto result = db_adapter_->select(select_sql);
        if (result.is_ok() && !result.value().empty()) {
            return parse_worklist_from_adapter_row(result.value()[0]);
        }
        // Query failed or empty, fall through to legacy
    }

    // Legacy fallback: database_manager
    if (db_manager_) {
        database::query_builder builder(database::database_types::sqlite);
        auto select_sql =
            builder
                .select(std::vector<std::string>{
                    "worklist_pk", "step_id", "step_status", "patient_id",
                    "patient_name", "birth_date", "sex", "accession_no",
                    "requested_proc_id", "study_uid", "scheduled_datetime",
                    "station_ae", "station_name", "modality", "procedure_desc",
                    "protocol_code", "referring_phys", "referring_phys_id",
                    "created_at", "updated_at"})
                .from("worklist")
                .where("step_id", "=", std::string(step_id))
                .where("accession_no", "=", std::string(accession_no))
                .build();

        auto result = db_manager_->select_query_result(select_sql);
        if (result.is_err() || result.value().empty()) {
            return std::nullopt;
        }

        return parse_worklist_from_row(result.value()[0]);
    }

    // database_system is required
    return std::nullopt;
#endif
}

auto index_database::find_worklist_by_pk(int64_t pk) const
    -> std::optional<worklist_item> {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter (Issue #615)
    if (db_adapter_ && db_adapter_->is_connected()) {
        auto builder = db_adapter_->create_query_builder();
        auto select_sql =
            builder
                .select(std::vector<std::string>{
                    "worklist_pk", "step_id", "step_status", "patient_id",
                    "patient_name", "birth_date", "sex", "accession_no",
                    "requested_proc_id", "study_uid", "scheduled_datetime",
                    "station_ae", "station_name", "modality", "procedure_desc",
                    "protocol_code", "referring_phys", "referring_phys_id",
                    "created_at", "updated_at"})
                .from("worklist")
                .where("worklist_pk", "=", pk)
                .build();

        auto result = db_adapter_->select(select_sql);
        if (result.is_ok() && !result.value().empty()) {
            return parse_worklist_from_adapter_row(result.value()[0]);
        }
        // Query failed or empty, fall through to legacy
    }

    // Legacy fallback: database_manager
    if (db_manager_) {
        database::query_builder builder(database::database_types::sqlite);
        auto select_sql =
            builder
                .select(std::vector<std::string>{
                    "worklist_pk", "step_id", "step_status", "patient_id",
                    "patient_name", "birth_date", "sex", "accession_no",
                    "requested_proc_id", "study_uid", "scheduled_datetime",
                    "station_ae", "station_name", "modality", "procedure_desc",
                    "protocol_code", "referring_phys", "referring_phys_id",
                    "created_at", "updated_at"})
                .from("worklist")
                .where("worklist_pk", "=", pk)
                .build();

        auto result = db_manager_->select_query_result(select_sql);
        if (result.is_err() || result.value().empty()) {
            return std::nullopt;
        }

        return parse_worklist_from_row(result.value()[0]);
    }

    // database_system is required
    return std::nullopt;
#endif
}

auto index_database::delete_worklist_item(std::string_view step_id,
                                          std::string_view accession_no)
    -> VoidResult {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter (Issue #615)
    if (db_adapter_ && db_adapter_->is_connected()) {
        auto builder = db_adapter_->create_query_builder();
        builder.delete_from("worklist");
        builder.where("step_id", "=", std::string(step_id));
        builder.where("accession_no", "=", std::string(accession_no));

        auto delete_sql = builder.build();
        auto result = db_adapter_->remove(delete_sql);
        if (result.is_ok()) {
            return ok();
        }
        // Delete failed, fall through to legacy
    }

    // Legacy fallback: database_manager
    if (db_manager_) {
        database::query_builder builder(database::database_types::sqlite);
        builder.delete_from("worklist");
        builder.where("step_id", "=", std::string(step_id));
        builder.where("accession_no", "=", std::string(accession_no));

        auto delete_sql = builder.build();
        auto result = db_manager_->delete_query_result(delete_sql);
        if (result.is_ok()) {
            return ok();
        }
    }

    // database_system is required
    return make_error<std::monostate>(
        database_query_error,
        "database_system is not initialized for worklist deletion",
        "storage");
#endif
}

auto index_database::cleanup_old_worklist_items(std::chrono::hours age)
    -> Result<size_t> {
    // Calculate the cutoff datetime
    auto now = std::chrono::system_clock::now();
    auto cutoff = now - age;
    auto cutoff_time = std::chrono::system_clock::to_time_t(cutoff);

    // Use gmtime_safe for UTC consistency with SQLite's datetime('now')
    std::tm tm{};
    pacs::compat::gmtime_safe(&cutoff_time, &tm);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    auto cutoff_str = oss.str();

#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter (Issue #615)
    if (db_adapter_ && db_adapter_->is_connected()) {
        auto builder = db_adapter_->create_query_builder();
        builder.delete_from("worklist");
        builder.where("step_status", "!=", std::string("SCHEDULED"));
        builder.where(database::query_condition(
            pacs::compat::format("created_at < '{}'", cutoff_str)));

        auto delete_sql = builder.build();
        auto result = db_adapter_->remove(delete_sql);
        if (result.is_ok()) {
            return ok(static_cast<size_t>(result.value()));
        }
        // Delete failed, fall through to legacy
    }

    // Legacy fallback: database_manager
    if (db_manager_) {
        database::query_builder builder(database::database_types::sqlite);
        builder.delete_from("worklist");
        builder.where("step_status", "!=", std::string("SCHEDULED"));
        builder.where(database::query_condition(
            pacs::compat::format("created_at < '{}'", cutoff_str)));

        auto delete_sql = builder.build();
        auto result = db_manager_->delete_query_result(delete_sql);
        if (result.is_ok()) {
            return ok(static_cast<size_t>(result.value()));
        }
    }

    // database_system is required
    return pacs_error<size_t>(
        error_codes::database_query_error,
        "database_system is not initialized for worklist cleanup");
#endif
}

auto index_database::cleanup_worklist_items_before(
    std::chrono::system_clock::time_point before) -> Result<size_t> {
    // Convert time_point to datetime string
    auto before_time = std::chrono::system_clock::to_time_t(before);

    // Use localtime_safe since scheduled_datetime is typically stored as local time
    // (DICOM datetime values don't include timezone information)
    std::tm tm{};
    pacs::compat::localtime_safe(&before_time, &tm);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    auto before_str = oss.str();

#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter (Issue #615)
    if (db_adapter_ && db_adapter_->is_connected()) {
        auto builder = db_adapter_->create_query_builder();
        builder.delete_from("worklist");
        builder.where("step_status", "!=", std::string("SCHEDULED"));
        builder.where(database::query_condition(
            pacs::compat::format("scheduled_datetime < '{}'", before_str)));

        auto delete_sql = builder.build();
        auto result = db_adapter_->remove(delete_sql);
        if (result.is_ok()) {
            return ok(static_cast<size_t>(result.value()));
        }
        // Delete failed, fall through to legacy
    }

    // Legacy fallback: database_manager
    if (db_manager_) {
        database::query_builder builder(database::database_types::sqlite);
        builder.delete_from("worklist");
        builder.where("step_status", "!=", std::string("SCHEDULED"));
        builder.where(database::query_condition(
            pacs::compat::format("scheduled_datetime < '{}'", before_str)));

        auto delete_sql = builder.build();
        auto result = db_manager_->delete_query_result(delete_sql);
        if (result.is_ok()) {
            return ok(static_cast<size_t>(result.value()));
        }
    }

    // database_system is required
    return pacs_error<size_t>(
        error_codes::database_query_error,
        "database_system is not initialized for worklist cleanup");
#endif
}

auto index_database::worklist_count() const -> Result<size_t> {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter (Issue #615)
    if (db_adapter_ && db_adapter_->is_connected()) {
        auto builder = db_adapter_->create_query_builder();
        auto select_sql = builder.select(std::vector<std::string>{"COUNT(*)"})
                              .from("worklist")
                              .build();

        auto result = db_adapter_->select(select_sql);
        if (result.is_ok() && !result.value().empty()) {
            const auto& row = result.value()[0];
            auto it = row.find("COUNT(*)");
            if (it != row.end()) {
                try {
                    return ok(static_cast<size_t>(std::stoll(it->second)));
                } catch (...) {
                    // Parse failed, fall through
                }
            }
        }
        // Query failed, fall through to legacy
    }

    // Legacy fallback: database_manager
    if (db_manager_) {
        database::query_builder builder(database::database_types::sqlite);
        auto select_sql = builder.select(std::vector<std::string>{"COUNT(*)"})
                              .from("worklist")
                              .build();

        auto result = db_manager_->select_query_result(select_sql);
        if (result.is_ok() && !result.value().empty()) {
            const auto& row = result.value()[0];
            auto it = row.find("COUNT(*)");
            if (it != row.end()) {
                return ok(static_cast<size_t>(std::get<int64_t>(it->second)));
            }
        }
        // Query failed, fall through to SQLite
    }
#endif

    // Fallback to direct SQLite
    const char* sql = "SELECT COUNT(*) FROM worklist;";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<size_t>(
            rc,
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

auto index_database::worklist_count(std::string_view status) const -> Result<size_t> {
#ifdef PACS_WITH_DATABASE_SYSTEM
    // Prefer pacs_database_adapter (Issue #615)
    if (db_adapter_ && db_adapter_->is_connected()) {
        auto builder = db_adapter_->create_query_builder();
        auto select_sql =
            builder.select(std::vector<std::string>{"COUNT(*)"})
                .from("worklist")
                .where("step_status", "=", std::string(status))
                .build();

        auto result = db_adapter_->select(select_sql);
        if (result.is_ok() && !result.value().empty()) {
            const auto& row = result.value()[0];
            auto it = row.find("COUNT(*)");
            if (it != row.end()) {
                try {
                    return ok(static_cast<size_t>(std::stoll(it->second)));
                } catch (...) {
                    // Parse failed, fall through
                }
            }
        }
        // Query failed, fall through to legacy
    }

    // Legacy fallback: database_manager
    if (db_manager_) {
        database::query_builder builder(database::database_types::sqlite);
        auto select_sql =
            builder.select(std::vector<std::string>{"COUNT(*)"})
                .from("worklist")
                .where("step_status", "=", std::string(status))
                .build();

        auto result = db_manager_->select_query_result(select_sql);
        if (result.is_ok() && !result.value().empty()) {
            const auto& row = result.value()[0];
            auto it = row.find("COUNT(*)");
            if (it != row.end()) {
                return ok(static_cast<size_t>(std::get<int64_t>(it->second)));
            }
        }
        // Query failed, fall through to SQLite
    }
#endif

    // Fallback to direct SQLite
    const char* sql = "SELECT COUNT(*) FROM worklist WHERE step_status = ?;";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<size_t>(
            rc,
            pacs::compat::format("Failed to prepare query: {}", sqlite3_errmsg(db_)),
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

auto index_database::parse_worklist_row(void* stmt_ptr) const -> worklist_item {
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

    auto created_str = get_text(stmt, 18);
    item.created_at = parse_datetime(created_str.c_str());

    auto updated_str = get_text(stmt, 19);
    item.updated_at = parse_datetime(updated_str.c_str());

    return item;
}

// ============================================================================
// Audit Log Operations
// ============================================================================

auto index_database::add_audit_log(const audit_record& record)
    -> Result<int64_t> {
#ifdef PACS_WITH_DATABASE_SYSTEM
    if (db_manager_) {
        database::query_builder builder(database::database_types::sqlite);
        auto insert_sql =
            builder.insert_into("audit_log")
                .values({{"event_type", record.event_type},
                         {"outcome", record.outcome},
                         {"user_id", record.user_id},
                         {"source_ae", record.source_ae},
                         {"target_ae", record.target_ae},
                         {"source_ip", record.source_ip},
                         {"patient_id", record.patient_id},
                         {"study_uid", record.study_uid},
                         {"message", record.message},
                         {"details", record.details}})
                .build();

        auto result = db_manager_->insert_query_result(insert_sql);
        if (result.is_err()) {
            return pacs_error<int64_t>(
                error_codes::database_query_error,
                pacs::compat::format("Failed to insert audit log: {}",
                                     result.error().message));
        }

        return result.value();
    }
#endif

    // Fallback to direct SQLite
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

auto index_database::query_audit_log(const audit_query& query) const
    -> Result<std::vector<audit_record>> {
#ifdef PACS_WITH_DATABASE_SYSTEM
    if (db_manager_) {
        database::query_builder builder(database::database_types::sqlite);
        builder.select(std::vector<std::string>{
            "audit_pk", "event_type", "outcome", "timestamp", "user_id",
            "source_ae", "target_ae", "source_ip", "patient_id", "study_uid",
            "message", "details"});
        builder.from("audit_log");

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
            builder.where(database::query_condition(
                pacs::compat::format("date(timestamp) >= date('{}')",
                                     *query.date_from)));
        }

        if (query.date_to.has_value()) {
            builder.where(database::query_condition(
                pacs::compat::format("date(timestamp) <= date('{}')",
                                     *query.date_to)));
        }

        builder.order_by("timestamp", database::sort_order::desc);

        if (query.limit > 0) {
            builder.limit(static_cast<int>(query.limit));
        }

        if (query.offset > 0) {
            builder.offset(static_cast<int>(query.offset));
        }

        auto select_sql =
            builder.build();
        auto result = db_manager_->select_query_result(select_sql);
        if (result.is_err()) {
            return pacs_error<std::vector<audit_record>>(
                error_codes::database_query_error,
                pacs::compat::format("Query failed: {}", result.error().message));
        }

        std::vector<audit_record> results;
        for (const auto& row : result.value()) {
            results.push_back(parse_audit_from_row(row));
        }
        return ok(std::move(results));
    }
#endif

    // Fallback to direct SQLite
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
            pacs::compat::format("Failed to prepare query: {}", sqlite3_errmsg(db_)),
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

auto index_database::find_audit_by_pk(int64_t pk) const
    -> std::optional<audit_record> {
#ifdef PACS_WITH_DATABASE_SYSTEM
    if (db_manager_) {
        database::query_builder builder(database::database_types::sqlite);
        auto select_sql =
            builder
                .select(std::vector<std::string>{
                    "audit_pk", "event_type", "outcome", "timestamp", "user_id",
                    "source_ae", "target_ae", "source_ip", "patient_id",
                    "study_uid", "message", "details"})
                .from("audit_log")
                .where("audit_pk", "=", pk)
                .build();

        auto result = db_manager_->select_query_result(select_sql);
        if (result.is_err() || result.value().empty()) {
            return std::nullopt;
        }

        return parse_audit_from_row(result.value()[0]);
    }
#endif

    // Fallback to direct SQLite
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

auto index_database::audit_count() const -> Result<size_t> {
#ifdef PACS_WITH_DATABASE_SYSTEM
    if (db_manager_) {
        database::query_builder builder(database::database_types::sqlite);
        auto count_sql = builder.select(std::vector<std::string>{"COUNT(*)"})
                             .from("audit_log")
                             .build();

        auto result = db_manager_->select_query_result(count_sql);
        if (result.is_err()) {
            return pacs_error<size_t>(
                error_codes::database_query_error,
                pacs::compat::format("Query failed: {}", result.error().message));
        }

        if (!result.value().empty()) {
            return ok(static_cast<size_t>(
                get_int64_value(result.value()[0], "COUNT(*)")));
        }
        return ok(static_cast<size_t>(0));
    }
#endif

    // Fallback to direct SQLite
    const char* sql = "SELECT COUNT(*) FROM audit_log;";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<size_t>(
            rc,
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

auto index_database::cleanup_old_audit_logs(std::chrono::hours age)
    -> Result<size_t> {
    auto cutoff = std::chrono::system_clock::now() - age;
    auto cutoff_time = std::chrono::system_clock::to_time_t(cutoff);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&cutoff_time), "%Y-%m-%d %H:%M:%S");
    auto cutoff_str = oss.str();

#ifdef PACS_WITH_DATABASE_SYSTEM
    if (db_manager_) {
        database::query_builder builder(database::database_types::sqlite);
        builder.delete_from("audit_log");
        builder.where(database::query_condition(pacs::compat::format("timestamp < '{}'", cutoff_str)));

        auto delete_sql =
            builder.build();
        auto result = db_manager_->delete_query_result(delete_sql);
        if (result.is_ok()) {
            return ok(static_cast<size_t>(result.value()));
        }
        // Delete failed, fall through to SQLite
    }
#endif

    // Fallback to direct SQLite
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

auto index_database::parse_audit_row(void* stmt_ptr) const -> audit_record {
    auto* stmt = static_cast<sqlite3_stmt*>(stmt_ptr);
    audit_record record;

    record.pk = sqlite3_column_int64(stmt, 0);
    record.event_type = get_text(stmt, 1);
    record.outcome = get_text(stmt, 2);

    auto timestamp_str = get_text(stmt, 3);
    record.timestamp = parse_datetime(timestamp_str.c_str());

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

#ifdef PACS_WITH_DATABASE_SYSTEM
auto index_database::parse_patient_from_row(
    const std::map<std::string, database::core::database_value>& row) const
    -> patient_record {
    patient_record record;

    record.pk = get_int64_value(row, "patient_pk");
    record.patient_id = get_string_value(row, "patient_id");
    record.patient_name = get_string_value(row, "patient_name");
    record.birth_date = get_string_value(row, "birth_date");
    record.sex = get_string_value(row, "sex");
    record.other_ids = get_string_value(row, "other_ids");
    record.ethnic_group = get_string_value(row, "ethnic_group");
    record.comments = get_string_value(row, "comments");

    auto created_str = get_string_value(row, "created_at");
    record.created_at = parse_datetime(created_str.c_str());

    auto updated_str = get_string_value(row, "updated_at");
    record.updated_at = parse_datetime(updated_str.c_str());

    return record;
}

auto index_database::parse_study_from_row(
    const std::map<std::string, database::core::database_value>& row) const
    -> study_record {
    study_record record;

    record.pk = get_int64_value(row, "study_pk");
    record.patient_pk = get_int64_value(row, "patient_pk");
    record.study_uid = get_string_value(row, "study_uid");
    record.study_id = get_string_value(row, "study_id");
    record.study_date = get_string_value(row, "study_date");
    record.study_time = get_string_value(row, "study_time");
    record.accession_number = get_string_value(row, "accession_number");
    record.referring_physician = get_string_value(row, "referring_physician");
    record.study_description = get_string_value(row, "study_description");
    record.modalities_in_study = get_string_value(row, "modalities_in_study");
    record.num_series = static_cast<int>(get_int64_value(row, "num_series"));
    record.num_instances =
        static_cast<int>(get_int64_value(row, "num_instances"));

    auto created_str = get_string_value(row, "created_at");
    record.created_at = parse_datetime(created_str.c_str());

    auto updated_str = get_string_value(row, "updated_at");
    record.updated_at = parse_datetime(updated_str.c_str());

    return record;
}

auto index_database::parse_series_from_row(
    const std::map<std::string, database::core::database_value>& row) const
    -> series_record {
    series_record record;

    record.pk = get_int64_value(row, "series_pk");
    record.study_pk = get_int64_value(row, "study_pk");
    record.series_uid = get_string_value(row, "series_uid");
    record.modality = get_string_value(row, "modality");

    // Handle nullable series_number
    auto it = row.find("series_number");
    if (it != row.end()) {
        if (std::holds_alternative<int64_t>(it->second)) {
            record.series_number = static_cast<int>(std::get<int64_t>(it->second));
        } else if (std::holds_alternative<std::string>(it->second)) {
            const auto& str = std::get<std::string>(it->second);
            if (!str.empty()) {
                record.series_number = std::stoi(str);
            }
        }
    }

    record.series_description = get_string_value(row, "series_description");
    record.body_part_examined = get_string_value(row, "body_part_examined");
    record.station_name = get_string_value(row, "station_name");
    record.num_instances =
        static_cast<int>(get_int64_value(row, "num_instances"));

    auto created_str = get_string_value(row, "created_at");
    record.created_at = parse_datetime(created_str.c_str());

    auto updated_str2 = get_string_value(row, "updated_at");
    record.updated_at = parse_datetime(updated_str2.c_str());

    return record;
}

auto index_database::parse_instance_from_row(
    const std::map<std::string, database::core::database_value>& row) const
    -> instance_record {
    instance_record record;

    record.pk = get_int64_value(row, "instance_pk");
    record.series_pk = get_int64_value(row, "series_pk");
    record.sop_uid = get_string_value(row, "sop_uid");
    record.sop_class_uid = get_string_value(row, "sop_class_uid");

    // Handle nullable instance_number
    auto it = row.find("instance_number");
    if (it != row.end()) {
        if (std::holds_alternative<int64_t>(it->second)) {
            record.instance_number = static_cast<int>(std::get<int64_t>(it->second));
        } else if (std::holds_alternative<std::string>(it->second)) {
            const auto& str = std::get<std::string>(it->second);
            if (!str.empty()) {
                record.instance_number = std::stoi(str);
            }
        }
    }

    record.transfer_syntax = get_string_value(row, "transfer_syntax");
    record.content_date = get_string_value(row, "content_date");
    record.content_time = get_string_value(row, "content_time");

    // Handle nullable image properties
    auto rows_it = row.find("rows");
    if (rows_it != row.end()) {
        if (std::holds_alternative<int64_t>(rows_it->second)) {
            record.rows = static_cast<int>(std::get<int64_t>(rows_it->second));
        } else if (std::holds_alternative<std::string>(rows_it->second)) {
            const auto& str = std::get<std::string>(rows_it->second);
            if (!str.empty()) {
                record.rows = std::stoi(str);
            }
        }
    }

    auto cols_it = row.find("columns");
    if (cols_it != row.end()) {
        if (std::holds_alternative<int64_t>(cols_it->second)) {
            record.columns = static_cast<int>(std::get<int64_t>(cols_it->second));
        } else if (std::holds_alternative<std::string>(cols_it->second)) {
            const auto& str = std::get<std::string>(cols_it->second);
            if (!str.empty()) {
                record.columns = std::stoi(str);
            }
        }
    }

    auto bits_it = row.find("bits_allocated");
    if (bits_it != row.end()) {
        if (std::holds_alternative<int64_t>(bits_it->second)) {
            record.bits_allocated = static_cast<int>(std::get<int64_t>(bits_it->second));
        } else if (std::holds_alternative<std::string>(bits_it->second)) {
            const auto& str = std::get<std::string>(bits_it->second);
            if (!str.empty()) {
                record.bits_allocated = std::stoi(str);
            }
        }
    }

    auto frames_it = row.find("number_of_frames");
    if (frames_it != row.end()) {
        if (std::holds_alternative<int64_t>(frames_it->second)) {
            record.number_of_frames = static_cast<int>(std::get<int64_t>(frames_it->second));
        } else if (std::holds_alternative<std::string>(frames_it->second)) {
            const auto& str = std::get<std::string>(frames_it->second);
            if (!str.empty()) {
                record.number_of_frames = std::stoi(str);
            }
        }
    }

    record.file_path = get_string_value(row, "file_path");
    record.file_size = get_int64_value(row, "file_size");
    record.file_hash = get_string_value(row, "file_hash");

    auto created_str = get_string_value(row, "created_at");
    record.created_at = parse_datetime(created_str.c_str());

    return record;
}
#endif

}  // namespace pacs::storage
