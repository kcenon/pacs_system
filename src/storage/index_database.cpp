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
 * @file index_database.cpp
 * @brief Implementation of PACS index database
 *
 * When compiled with PACS_WITH_DATABASE_SYSTEM, uses database_system's
 * query builder for parameterized queries. Otherwise, uses direct SQLite
 * with prepared statements.
 */

#include <pacs/storage/index_database.hpp>

#include <pacs/core/result.hpp>
#include <pacs/storage/audit_repository.hpp>
#include <pacs/storage/instance_repository.hpp>
#include <pacs/storage/mpps_repository.hpp>
#include <pacs/storage/patient_repository.hpp>
#include <pacs/storage/series_repository.hpp>
#include <pacs/storage/study_repository.hpp>
#include <pacs/storage/ups_repository.hpp>
#include <pacs/storage/worklist_repository.hpp>
#include <sqlite3.h>

#ifdef PACS_WITH_DATABASE_SYSTEM
#include <database/query_builder.h>
#endif

#include <atomic>
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

auto create_adapter_compatible_memory_path() -> std::string {
    static std::atomic<uint64_t> counter{0};

    const auto unique_id =
        std::chrono::steady_clock::now().time_since_epoch().count() +
        static_cast<std::chrono::steady_clock::rep>(counter.fetch_add(1));

    const auto file_name =
        pacs::compat::format("pacs_index_memory_{}.sqlite", unique_id);
    return (std::filesystem::temp_directory_path() / file_name).string();
}
#endif

void remove_database_sidecars(const std::string& path) {
    if (path.empty()) {
        return;
    }

    std::error_code ec;
    std::filesystem::remove(path, ec);
    std::filesystem::remove(path + "-wal", ec);
    std::filesystem::remove(path + "-shm", ec);
}

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

    std::string effective_path;
#ifdef PACS_WITH_DATABASE_SYSTEM
    bool remove_on_close = false;
    if (db_path == ":memory:") {
        // database_system's SQLite backend expects a filesystem path, so use
        // a temporary file that both sqlite3 and pacs_database_adapter can open.
        effective_path = create_adapter_compatible_memory_path();
        remove_on_close = true;
    } else {
        effective_path = std::string(db_path);
    }
#else
    effective_path = std::string(db_path);
#endif

    auto rc = sqlite3_open_v2(effective_path.c_str(), &db,
                              SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_URI,
                              nullptr);
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
        new index_database(db, effective_path));
#ifdef PACS_WITH_DATABASE_SYSTEM
    instance->remove_on_close_ = remove_on_close;
#endif

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
        // unified_database_system may not have SQLite backend support in the
        // current build, so keep the direct SQLite path available.
    }
#endif

    auto repository_result = instance->initialize_repositories();
    if (repository_result.is_err()) {
        return make_error<std::unique_ptr<index_database>>(
            repository_result.error().code,
            pacs::compat::format("Repository initialization failed: {}",
                                 repository_result.error().message),
            "storage");
    }

    return instance;
}

index_database::index_database(sqlite3* db, std::string path)
    : db_(db), path_(std::move(path)) {}

index_database::~index_database() {
    patient_repository_.reset();
    study_repository_.reset();
    series_repository_.reset();
    instance_repository_.reset();
    mpps_repository_.reset();
    worklist_repository_.reset();
    ups_repository_.reset();
    audit_repository_.reset();
#ifdef PACS_WITH_DATABASE_SYSTEM
    if (db_adapter_) {
        (void)db_adapter_->disconnect();
        db_adapter_.reset();
    }
#endif
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
    if (remove_on_close_) {
        remove_database_sidecars(path_);
    }
}

#ifdef PACS_WITH_DATABASE_SYSTEM
auto index_database::initialize_database_system() -> VoidResult {
    return initialize_database_adapter();
}

auto index_database::initialize_database_adapter() -> VoidResult {
    db_adapter_ = std::make_shared<pacs_database_adapter>(
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

auto index_database::initialize_repositories() -> VoidResult {
#ifdef PACS_WITH_DATABASE_SYSTEM
    if (!db_adapter_ || !db_adapter_->is_connected()) {
        return make_error<std::monostate>(
            database_connection_error,
            "PACS database adapter is not connected",
            "storage");
    }

    patient_repository_ = std::make_shared<patient_repository>(db_adapter_);
    study_repository_ = std::make_shared<study_repository>(db_adapter_);
    series_repository_ = std::make_shared<series_repository>(db_adapter_);
    instance_repository_ = std::make_shared<instance_repository>(db_adapter_);
    mpps_repository_ = std::make_shared<mpps_repository>(db_adapter_);
    worklist_repository_ = std::make_shared<worklist_repository>(db_adapter_);
    ups_repository_ = std::make_shared<ups_repository>(db_adapter_);
    audit_repository_ = std::make_shared<audit_repository>(db_adapter_);
#else
    if (db_ == nullptr) {
        return make_error<std::monostate>(
            database_connection_error,
            "SQLite database handle is not available",
            "storage");
    }

    patient_repository_ = std::make_shared<patient_repository>(db_);
    study_repository_ = std::make_shared<study_repository>(db_);
    series_repository_ = std::make_shared<series_repository>(db_);
    instance_repository_ = std::make_shared<instance_repository>(db_);
    mpps_repository_ = std::make_shared<mpps_repository>(db_);
    worklist_repository_ = std::make_shared<worklist_repository>(db_);
    ups_repository_ = std::make_shared<ups_repository>(db_);
    audit_repository_ = std::make_shared<audit_repository>(db_);
#endif

    return ok();
}

index_database::index_database(index_database&& other) noexcept
    : db_(other.db_),
      path_(std::move(other.path_)),
      remove_on_close_(other.remove_on_close_),
      patient_repository_(std::move(other.patient_repository_)),
      study_repository_(std::move(other.study_repository_)),
      series_repository_(std::move(other.series_repository_)),
      instance_repository_(std::move(other.instance_repository_)),
      mpps_repository_(std::move(other.mpps_repository_)),
      worklist_repository_(std::move(other.worklist_repository_)),
      ups_repository_(std::move(other.ups_repository_)),
      audit_repository_(std::move(other.audit_repository_)) {
#ifdef PACS_WITH_DATABASE_SYSTEM
    db_adapter_ = std::move(other.db_adapter_);
#endif
    other.db_ = nullptr;
    other.remove_on_close_ = false;
}

auto index_database::operator=(index_database&& other) noexcept
    -> index_database& {
    if (this != &other) {
        patient_repository_.reset();
        study_repository_.reset();
        series_repository_.reset();
        instance_repository_.reset();
        mpps_repository_.reset();
        worklist_repository_.reset();
        ups_repository_.reset();
        audit_repository_.reset();
#ifdef PACS_WITH_DATABASE_SYSTEM
        if (db_adapter_) {
            (void)db_adapter_->disconnect();
        }
        db_adapter_ = std::move(other.db_adapter_);
#endif
        if (db_) {
            sqlite3_close(db_);
        }
        if (remove_on_close_) {
            remove_database_sidecars(path_);
        }
        db_ = other.db_;
        path_ = std::move(other.path_);
        remove_on_close_ = other.remove_on_close_;
        patient_repository_ = std::move(other.patient_repository_);
        study_repository_ = std::move(other.study_repository_);
        series_repository_ = std::move(other.series_repository_);
        instance_repository_ = std::move(other.instance_repository_);
        mpps_repository_ = std::move(other.mpps_repository_);
        worklist_repository_ = std::move(other.worklist_repository_);
        ups_repository_ = std::move(other.ups_repository_);
        audit_repository_ = std::move(other.audit_repository_);
        other.db_ = nullptr;
        other.remove_on_close_ = false;
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
    return patient_repository_->upsert_patient(patient_id, patient_name,
                                               birth_date, sex);
}

auto index_database::upsert_patient(const patient_record& record)
    -> Result<int64_t> {
    return patient_repository_->upsert_patient(record);
}

auto index_database::find_patient(std::string_view patient_id) const
    -> std::optional<patient_record> {
    return patient_repository_->find_patient(patient_id);
}

auto index_database::find_patient_by_pk(int64_t pk) const
    -> std::optional<patient_record> {
    return patient_repository_->find_patient_by_pk(pk);
}

auto index_database::search_patients(const patient_query& query) const
    -> Result<std::vector<patient_record>> {
    return patient_repository_->search_patients(query);
}

auto index_database::delete_patient(std::string_view patient_id) -> VoidResult {
    return patient_repository_->delete_patient(patient_id);
}

auto index_database::patient_count() const -> Result<size_t> {
    return patient_repository_->patient_count();
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
    return study_repository_->upsert_study(
        patient_pk, study_uid, study_id, study_date, study_time,
        accession_number, referring_physician, study_description);
}

auto index_database::upsert_study(const study_record& record)
    -> Result<int64_t> {
    return study_repository_->upsert_study(record);
}

auto index_database::find_study(std::string_view study_uid) const
    -> std::optional<study_record> {
    return study_repository_->find_study(study_uid);
}

auto index_database::find_study_by_pk(int64_t pk) const
    -> std::optional<study_record> {
    return study_repository_->find_study_by_pk(pk);
}

auto index_database::list_studies(std::string_view patient_id) const
    -> Result<std::vector<study_record>> {
    study_query query;
    query.patient_id = std::string(patient_id);
    return study_repository_->search_studies(query);
}

auto index_database::search_studies(const study_query& query) const
    -> Result<std::vector<study_record>> {
    return study_repository_->search_studies(query);
}

auto index_database::delete_study(std::string_view study_uid) -> VoidResult {
    return study_repository_->delete_study(study_uid);
}

auto index_database::study_count() const -> Result<size_t> {
    return study_repository_->study_count();
}

auto index_database::study_count(std::string_view patient_id) const -> Result<size_t> {
    auto patient = patient_repository_->find_patient(patient_id);
    if (!patient.has_value()) {
        return ok(static_cast<size_t>(0));
    }
    return study_repository_->study_count_for_patient(patient->pk);
}

auto index_database::update_modalities_in_study(int64_t study_pk)
    -> VoidResult {
    return study_repository_->update_modalities_in_study(study_pk);
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
    auto existing = find_series(record.series_uid);
    auto result = series_repository_->upsert_series(record);
    if (result.is_err()) {
        return result;
    }

    if (existing.has_value() && existing->study_pk > 0 &&
        existing->study_pk != record.study_pk) {
        (void)update_modalities_in_study(existing->study_pk);
    }
    if (record.study_pk > 0) {
        (void)update_modalities_in_study(record.study_pk);
    }

    return result;
}

auto index_database::find_series(std::string_view series_uid) const
    -> std::optional<series_record> {
    return series_repository_->find_series(series_uid);
}

auto index_database::find_series_by_pk(int64_t pk) const
    -> std::optional<series_record> {
    return series_repository_->find_series_by_pk(pk);
}

auto index_database::list_series(std::string_view study_uid) const
    -> Result<std::vector<series_record>> {
    return series_repository_->list_series(study_uid);
}

auto index_database::search_series(const series_query& query) const
    -> Result<std::vector<series_record>> {
    return series_repository_->search_series(query);
}

auto index_database::delete_series(std::string_view series_uid) -> VoidResult {
    auto existing = find_series(series_uid);
    auto result = series_repository_->delete_series(series_uid);
    if (result.is_ok() && existing.has_value() && existing->study_pk > 0) {
        (void)update_modalities_in_study(existing->study_pk);
    }
    return result;
}

auto index_database::series_count() const -> Result<size_t> {
    return series_repository_->series_count();
}

auto index_database::series_count(std::string_view study_uid) const -> Result<size_t> {
    return series_repository_->series_count(study_uid);
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
    return instance_repository_->upsert_instance(
        series_pk, sop_uid, sop_class_uid, file_path, file_size,
        transfer_syntax, instance_number);
}

auto index_database::upsert_instance(const instance_record& record)
    -> Result<int64_t> {
    return instance_repository_->upsert_instance(record);
}

auto index_database::find_instance(std::string_view sop_uid) const
    -> std::optional<instance_record> {
    return instance_repository_->find_instance(sop_uid);
}

auto index_database::find_instance_by_pk(int64_t pk) const
    -> std::optional<instance_record> {
    return instance_repository_->find_instance_by_pk(pk);
}

auto index_database::list_instances(std::string_view series_uid) const
    -> Result<std::vector<instance_record>> {
    return instance_repository_->list_instances(series_uid);
}

auto index_database::search_instances(const instance_query& query) const
    -> Result<std::vector<instance_record>> {
    return instance_repository_->search_instances(query);
}

auto index_database::delete_instance(std::string_view sop_uid) -> VoidResult {
    return instance_repository_->delete_instance(sop_uid);
}

auto index_database::instance_count() const -> Result<size_t> {
    return instance_repository_->instance_count();
}

auto index_database::instance_count(std::string_view series_uid) const
    -> Result<size_t> {
    return instance_repository_->instance_count(series_uid);
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
    return instance_repository_->get_file_path(sop_instance_uid);
}

auto index_database::get_study_files(std::string_view study_instance_uid) const
    -> Result<std::vector<std::string>> {
    return instance_repository_->get_study_files(study_instance_uid);
}

auto index_database::get_series_files(std::string_view series_instance_uid)
    const -> Result<std::vector<std::string>> {
    return instance_repository_->get_series_files(series_instance_uid);
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
auto index_database::db_adapter() const noexcept
    -> std::shared_ptr<pacs_database_adapter> {
    return db_adapter_;
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
    return mpps_repository_->create_mpps(mpps_uid, station_ae, modality,
                                         study_uid, accession_no,
                                         start_datetime);
}

auto index_database::create_mpps(const mpps_record& record) -> Result<int64_t> {
    return mpps_repository_->create_mpps(record);
}

auto index_database::update_mpps(std::string_view mpps_uid,
                                 std::string_view new_status,
                                 std::string_view end_datetime,
                                 std::string_view performed_series)
    -> VoidResult {
    return mpps_repository_->update_mpps(mpps_uid, new_status, end_datetime,
                                         performed_series);
}

auto index_database::update_mpps(const mpps_record& record) -> VoidResult {
    return mpps_repository_->update_mpps(record);
}

auto index_database::find_mpps(std::string_view mpps_uid) const
    -> std::optional<mpps_record> {
    return mpps_repository_->find_mpps(mpps_uid);
}

auto index_database::find_mpps_by_pk(int64_t pk) const
    -> std::optional<mpps_record> {
    return mpps_repository_->find_mpps_by_pk(pk);
}

auto index_database::list_active_mpps(std::string_view station_ae) const
    -> Result<std::vector<mpps_record>> {
    return mpps_repository_->list_active_mpps(station_ae);
}

auto index_database::find_mpps_by_study(std::string_view study_uid) const
    -> Result<std::vector<mpps_record>> {
    return mpps_repository_->find_mpps_by_study(study_uid);
}

auto index_database::search_mpps(const mpps_query& query) const
    -> Result<std::vector<mpps_record>> {
    return mpps_repository_->search_mpps(query);
}

auto index_database::delete_mpps(std::string_view mpps_uid) -> VoidResult {
    return mpps_repository_->delete_mpps(mpps_uid);
}

auto index_database::mpps_count() const -> Result<size_t> {
    return mpps_repository_->mpps_count();
}

auto index_database::mpps_count(std::string_view status) const -> Result<size_t> {
    return mpps_repository_->mpps_count(status);
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
    return worklist_repository_->add_worklist_item(item);
}

auto index_database::update_worklist_status(std::string_view step_id,
                                            std::string_view accession_no,
                                            std::string_view new_status)
    -> VoidResult {
    return worklist_repository_->update_worklist_status(step_id, accession_no,
                                                        new_status);
}

auto index_database::query_worklist(const worklist_query& query) const
    -> Result<std::vector<worklist_item>> {
    return worklist_repository_->query_worklist(query);
}

auto index_database::find_worklist_item(std::string_view step_id,
                                        std::string_view accession_no) const
    -> std::optional<worklist_item> {
    return worklist_repository_->find_worklist_item(step_id, accession_no);
}

auto index_database::find_worklist_by_pk(int64_t pk) const
    -> std::optional<worklist_item> {
    return worklist_repository_->find_worklist_by_pk(pk);
}

auto index_database::delete_worklist_item(std::string_view step_id,
                                          std::string_view accession_no)
    -> VoidResult {
    return worklist_repository_->delete_worklist_item(step_id, accession_no);
}

auto index_database::cleanup_old_worklist_items(std::chrono::hours age)
    -> Result<size_t> {
    return worklist_repository_->cleanup_old_worklist_items(age);
}

auto index_database::cleanup_worklist_items_before(
    std::chrono::system_clock::time_point before) -> Result<size_t> {
    return worklist_repository_->cleanup_worklist_items_before(before);
}

auto index_database::worklist_count() const -> Result<size_t> {
    return worklist_repository_->worklist_count();
}

auto index_database::worklist_count(std::string_view status) const -> Result<size_t> {
    return worklist_repository_->worklist_count(status);
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
// UPS Workitem Operations
// ============================================================================

auto index_database::create_ups_workitem(const ups_workitem& workitem)
    -> Result<int64_t> {
    return ups_repository_->create_ups_workitem(workitem);
}

auto index_database::update_ups_workitem(const ups_workitem& workitem)
    -> VoidResult {
    return ups_repository_->update_ups_workitem(workitem);
}

auto index_database::change_ups_state(std::string_view workitem_uid,
                                       std::string_view new_state,
                                       std::string_view transaction_uid)
    -> VoidResult {
    return ups_repository_->change_ups_state(workitem_uid, new_state,
                                             transaction_uid);
}

auto index_database::find_ups_workitem(std::string_view workitem_uid) const
    -> std::optional<ups_workitem> {
    return ups_repository_->find_ups_workitem(workitem_uid);
}

auto index_database::search_ups_workitems(const ups_workitem_query& query) const
    -> Result<std::vector<ups_workitem>> {
    return ups_repository_->search_ups_workitems(query);
}

auto index_database::delete_ups_workitem(std::string_view workitem_uid)
    -> VoidResult {
    return ups_repository_->delete_ups_workitem(workitem_uid);
}

auto index_database::ups_workitem_count() const -> Result<size_t> {
    return ups_repository_->ups_workitem_count();
}

auto index_database::ups_workitem_count(std::string_view state) const
    -> Result<size_t> {
    return ups_repository_->ups_workitem_count(state);
}

// ============================================================================
// UPS Subscription Operations
// ============================================================================

auto index_database::subscribe_ups(const ups_subscription& subscription)
    -> Result<int64_t> {
    return ups_repository_->subscribe_ups(subscription);
}

auto index_database::unsubscribe_ups(std::string_view subscriber_ae,
                                      std::string_view workitem_uid)
    -> VoidResult {
    return ups_repository_->unsubscribe_ups(subscriber_ae, workitem_uid);
}

auto index_database::get_ups_subscriptions(std::string_view subscriber_ae) const
    -> Result<std::vector<ups_subscription>> {
    return ups_repository_->get_ups_subscriptions(subscriber_ae);
}

auto index_database::get_ups_subscribers(std::string_view workitem_uid) const
    -> Result<std::vector<std::string>> {
    return ups_repository_->get_ups_subscribers(workitem_uid);
}

auto index_database::parse_ups_workitem_row(void* stmt_ptr) const -> ups_workitem {
    auto* stmt = static_cast<sqlite3_stmt*>(stmt_ptr);
    ups_workitem item;

    item.pk = sqlite3_column_int64(stmt, 0);
    item.workitem_uid = get_text(stmt, 1);
    item.state = get_text(stmt, 2);
    item.procedure_step_label = get_text(stmt, 3);
    item.worklist_label = get_text(stmt, 4);
    item.priority = get_text(stmt, 5);
    item.scheduled_start_datetime = get_text(stmt, 6);
    item.expected_completion_datetime = get_text(stmt, 7);
    item.scheduled_station_name = get_text(stmt, 8);
    item.scheduled_station_class = get_text(stmt, 9);
    item.scheduled_station_geographic = get_text(stmt, 10);
    item.scheduled_human_performers = get_text(stmt, 11);
    item.input_information = get_text(stmt, 12);
    item.performing_ae = get_text(stmt, 13);
    item.progress_description = get_text(stmt, 14);
    item.progress_percent = sqlite3_column_int(stmt, 15);
    item.output_information = get_text(stmt, 16);
    item.transaction_uid = get_text(stmt, 17);

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
    return audit_repository_->add_audit_log(record);
}

auto index_database::query_audit_log(const audit_query& query) const
    -> Result<std::vector<audit_record>> {
    return audit_repository_->query_audit_log(query);
}

auto index_database::find_audit_by_pk(int64_t pk) const
    -> std::optional<audit_record> {
    return audit_repository_->find_audit_by_pk(pk);
}

auto index_database::audit_count() const -> Result<size_t> {
    return audit_repository_->audit_count();
}

auto index_database::cleanup_old_audit_logs(std::chrono::hours age)
    -> Result<size_t> {
    return audit_repository_->cleanup_old_audit_logs(age);
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
