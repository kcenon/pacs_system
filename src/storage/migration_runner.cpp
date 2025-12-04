/**
 * @file migration_runner.cpp
 * @brief Implementation of database schema migration runner
 */

#include <pacs/storage/migration_runner.hpp>

#include <sqlite3.h>

#include <pacs/compat/format.hpp>

namespace pacs::storage {

// Use common_system's ok() function
using kcenon::common::ok;
using kcenon::common::make_error;

// ============================================================================
// Construction
// ============================================================================

migration_runner::migration_runner() {
    // Register all migrations
    migrations_.push_back({1, [this](sqlite3* db) { return migrate_v1(db); }});
}

// ============================================================================
// Migration Operations
// ============================================================================

auto migration_runner::run_migrations(sqlite3* db) -> VoidResult {
    return run_migrations_to(db, LATEST_VERSION);
}

auto migration_runner::run_migrations_to(sqlite3* db, int target_version)
    -> VoidResult {
    if (target_version > LATEST_VERSION) {
        return make_error<std::monostate>(
            -1,
            pacs::compat::format("Target version {} exceeds latest version {}",
                       target_version, LATEST_VERSION),
            "storage");
    }

    // Ensure schema_version table exists
    auto ensure_result = ensure_schema_version_table(db);
    if (ensure_result.is_err()) {
        return ensure_result;
    }

    auto current_version = get_current_version(db);

    // Nothing to do if already at or past target
    if (current_version >= target_version) {
        return ok();
    }

    // Apply each migration in a transaction
    while (current_version < target_version) {
        auto next_version = current_version + 1;

        // Begin transaction
        auto begin_result = execute_sql(db, "BEGIN TRANSACTION;");
        if (begin_result.is_err()) {
            return begin_result;
        }

        // Apply migration
        auto migration_result = apply_migration(db, next_version);
        if (migration_result.is_err()) {
            // Rollback on failure
            (void)execute_sql(db, "ROLLBACK;");
            return migration_result;
        }

        // Commit transaction
        auto commit_result = execute_sql(db, "COMMIT;");
        if (commit_result.is_err()) {
            (void)execute_sql(db, "ROLLBACK;");
            return commit_result;
        }

        current_version = next_version;
    }

    return ok();
}

// ============================================================================
// Version Information
// ============================================================================

auto migration_runner::get_current_version(sqlite3* db) const -> int {
    // Check if schema_version table exists
    const char* check_sql =
        "SELECT name FROM sqlite_master WHERE type='table' AND name='schema_version';";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db, check_sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return 0;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_ROW) {
        // Table doesn't exist
        return 0;
    }

    // Get max version from table
    const char* version_sql = "SELECT MAX(version) FROM schema_version;";
    rc = sqlite3_prepare_v2(db, version_sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return 0;
    }

    int version = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        // sqlite3_column_int returns 0 for NULL
        version = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    return version;
}

auto migration_runner::get_latest_version() const noexcept -> int {
    return LATEST_VERSION;
}

auto migration_runner::needs_migration(sqlite3* db) const -> bool {
    return get_current_version(db) < LATEST_VERSION;
}

// ============================================================================
// Migration History
// ============================================================================

auto migration_runner::get_history(sqlite3* db) const
    -> std::vector<migration_record> {
    std::vector<migration_record> history;

    const char* sql =
        "SELECT version, description, applied_at FROM schema_version ORDER BY version;";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return history;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        migration_record record;
        record.version = sqlite3_column_int(stmt, 0);

        const auto* desc = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        record.description = desc ? desc : "";

        const auto* applied = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        record.applied_at = applied ? applied : "";

        history.push_back(std::move(record));
    }

    sqlite3_finalize(stmt);
    return history;
}

// ============================================================================
// Internal Implementation
// ============================================================================

auto migration_runner::ensure_schema_version_table(sqlite3* db) -> VoidResult {
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS schema_version (
            version     INTEGER PRIMARY KEY,
            description TEXT NOT NULL,
            applied_at  TEXT NOT NULL DEFAULT (datetime('now'))
        );
    )";

    return execute_sql(db, sql);
}

auto migration_runner::apply_migration(sqlite3* db, int version) -> VoidResult {
    // Find the migration function
    for (const auto& [ver, func] : migrations_) {
        if (ver == version) {
            return func(db);
        }
    }

    return make_error<std::monostate>(
        -1,
        pacs::compat::format("Migration for version {} not found", version),
        "storage");
}

auto migration_runner::record_migration(sqlite3* db, int version,
                                        std::string_view description)
    -> VoidResult {
    const char* sql =
        "INSERT INTO schema_version (version, description) VALUES (?, ?);";

    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<std::monostate>(
            rc,
            pacs::compat::format("Failed to prepare statement: {}",
                       sqlite3_errmsg(db)),
            "storage");
    }

    sqlite3_bind_int(stmt, 1, version);
    sqlite3_bind_text(stmt, 2, description.data(),
                      static_cast<int>(description.size()), SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return make_error<std::monostate>(
            rc,
            pacs::compat::format("Failed to record migration: {}",
                       sqlite3_errmsg(db)),
            "storage");
    }

    return ok();
}

auto migration_runner::execute_sql(sqlite3* db, std::string_view sql)
    -> VoidResult {
    char* errmsg = nullptr;
    auto rc = sqlite3_exec(db, sql.data(), nullptr, nullptr, &errmsg);

    if (rc != SQLITE_OK) {
        auto error_str = errmsg ? std::string(errmsg) : "Unknown error";
        sqlite3_free(errmsg);

        return make_error<std::monostate>(
            rc, pacs::compat::format("SQL execution failed: {}", error_str),
            "storage");
    }

    return ok();
}

// ============================================================================
// Migration Implementations
// ============================================================================

auto migration_runner::migrate_v1(sqlite3* db) -> VoidResult {
    // V1: Initial schema - Create all base tables
    const char* sql = R"(
        -- =====================================================================
        -- PATIENTS TABLE
        -- =====================================================================
        CREATE TABLE IF NOT EXISTS patients (
            patient_pk      INTEGER PRIMARY KEY AUTOINCREMENT,
            patient_id      TEXT NOT NULL UNIQUE,
            patient_name    TEXT,
            birth_date      TEXT,
            sex             TEXT,
            other_ids       TEXT,
            ethnic_group    TEXT,
            comments        TEXT,
            created_at      TEXT NOT NULL DEFAULT (datetime('now')),
            updated_at      TEXT NOT NULL DEFAULT (datetime('now')),
            CHECK (length(patient_id) <= 64)
        );

        CREATE INDEX IF NOT EXISTS idx_patients_name ON patients(patient_name);
        CREATE INDEX IF NOT EXISTS idx_patients_birth ON patients(birth_date);

        -- =====================================================================
        -- STUDIES TABLE
        -- =====================================================================
        CREATE TABLE IF NOT EXISTS studies (
            study_pk            INTEGER PRIMARY KEY AUTOINCREMENT,
            patient_pk          INTEGER NOT NULL REFERENCES patients(patient_pk)
                                ON DELETE CASCADE,
            study_uid           TEXT NOT NULL UNIQUE,
            study_id            TEXT,
            study_date          TEXT,
            study_time          TEXT,
            accession_number    TEXT,
            referring_physician TEXT,
            study_description   TEXT,
            modalities_in_study TEXT,
            num_series          INTEGER DEFAULT 0,
            num_instances       INTEGER DEFAULT 0,
            created_at          TEXT NOT NULL DEFAULT (datetime('now')),
            updated_at          TEXT NOT NULL DEFAULT (datetime('now')),
            CHECK (length(study_uid) <= 64)
        );

        CREATE INDEX IF NOT EXISTS idx_studies_patient ON studies(patient_pk);
        CREATE INDEX IF NOT EXISTS idx_studies_date ON studies(study_date);
        CREATE INDEX IF NOT EXISTS idx_studies_accession ON studies(accession_number);

        -- =====================================================================
        -- SERIES TABLE
        -- =====================================================================
        CREATE TABLE IF NOT EXISTS series (
            series_pk           INTEGER PRIMARY KEY AUTOINCREMENT,
            study_pk            INTEGER NOT NULL REFERENCES studies(study_pk)
                                ON DELETE CASCADE,
            series_uid          TEXT NOT NULL UNIQUE,
            series_number       INTEGER,
            modality            TEXT,
            series_description  TEXT,
            body_part_examined  TEXT,
            station_name        TEXT,
            num_instances       INTEGER DEFAULT 0,
            created_at          TEXT NOT NULL DEFAULT (datetime('now')),
            updated_at          TEXT NOT NULL DEFAULT (datetime('now')),
            CHECK (length(series_uid) <= 64)
        );

        CREATE INDEX IF NOT EXISTS idx_series_study ON series(study_pk);
        CREATE INDEX IF NOT EXISTS idx_series_modality ON series(modality);

        -- =====================================================================
        -- INSTANCES TABLE
        -- =====================================================================
        CREATE TABLE IF NOT EXISTS instances (
            instance_pk     INTEGER PRIMARY KEY AUTOINCREMENT,
            series_pk       INTEGER NOT NULL REFERENCES series(series_pk)
                            ON DELETE CASCADE,
            sop_uid         TEXT NOT NULL UNIQUE,
            sop_class_uid   TEXT NOT NULL,
            instance_number INTEGER,
            transfer_syntax TEXT,
            content_date    TEXT,
            content_time    TEXT,
            rows            INTEGER,
            columns         INTEGER,
            bits_allocated  INTEGER,
            number_of_frames INTEGER,
            file_path       TEXT NOT NULL,
            file_size       INTEGER NOT NULL,
            file_hash       TEXT,
            created_at      TEXT NOT NULL DEFAULT (datetime('now')),
            CHECK (length(sop_uid) <= 64),
            CHECK (file_size >= 0)
        );

        CREATE INDEX IF NOT EXISTS idx_instances_series ON instances(series_pk);
        CREATE INDEX IF NOT EXISTS idx_instances_sop_class ON instances(sop_class_uid);
        CREATE INDEX IF NOT EXISTS idx_instances_number ON instances(instance_number);
        CREATE INDEX IF NOT EXISTS idx_instances_created ON instances(created_at);

        -- =====================================================================
        -- MPPS TABLE (Modality Performed Procedure Step)
        -- =====================================================================
        CREATE TABLE IF NOT EXISTS mpps (
            mpps_pk             INTEGER PRIMARY KEY AUTOINCREMENT,
            mpps_uid            TEXT NOT NULL UNIQUE,
            status              TEXT NOT NULL,
            start_datetime      TEXT,
            end_datetime        TEXT,
            station_ae          TEXT,
            station_name        TEXT,
            modality            TEXT,
            study_uid           TEXT,
            accession_no        TEXT,
            scheduled_step_id   TEXT,
            requested_proc_id   TEXT,
            performed_series    TEXT,
            created_at          TEXT NOT NULL DEFAULT (datetime('now')),
            updated_at          TEXT NOT NULL DEFAULT (datetime('now')),
            CHECK (status IN ('IN PROGRESS', 'COMPLETED', 'DISCONTINUED'))
        );

        CREATE INDEX IF NOT EXISTS idx_mpps_status ON mpps(status);
        CREATE INDEX IF NOT EXISTS idx_mpps_station ON mpps(station_ae);
        CREATE INDEX IF NOT EXISTS idx_mpps_study ON mpps(study_uid);
        CREATE INDEX IF NOT EXISTS idx_mpps_date ON mpps(start_datetime);

        -- =====================================================================
        -- WORKLIST TABLE (Modality Worklist)
        -- =====================================================================
        CREATE TABLE IF NOT EXISTS worklist (
            worklist_pk         INTEGER PRIMARY KEY AUTOINCREMENT,
            step_id             TEXT NOT NULL,
            step_status         TEXT DEFAULT 'SCHEDULED',
            patient_id          TEXT NOT NULL,
            patient_name        TEXT,
            birth_date          TEXT,
            sex                 TEXT,
            accession_no        TEXT,
            requested_proc_id   TEXT,
            study_uid           TEXT,
            scheduled_datetime  TEXT NOT NULL,
            station_ae          TEXT,
            station_name        TEXT,
            modality            TEXT NOT NULL,
            procedure_desc      TEXT,
            protocol_code       TEXT,
            referring_phys      TEXT,
            referring_phys_id   TEXT,
            created_at          TEXT NOT NULL DEFAULT (datetime('now')),
            updated_at          TEXT NOT NULL DEFAULT (datetime('now')),
            UNIQUE (step_id, accession_no)
        );

        CREATE INDEX IF NOT EXISTS idx_worklist_station ON worklist(station_ae);
        CREATE INDEX IF NOT EXISTS idx_worklist_modality ON worklist(modality);
        CREATE INDEX IF NOT EXISTS idx_worklist_scheduled ON worklist(scheduled_datetime);
        CREATE INDEX IF NOT EXISTS idx_worklist_patient ON worklist(patient_id);
        CREATE INDEX IF NOT EXISTS idx_worklist_accession ON worklist(accession_no);
        CREATE INDEX IF NOT EXISTS idx_worklist_status ON worklist(step_status);
        CREATE INDEX IF NOT EXISTS idx_worklist_station_date_mod
            ON worklist(station_ae, scheduled_datetime, modality);

        -- =====================================================================
        -- TRIGGERS FOR PARENT COUNT UPDATES
        -- =====================================================================
        CREATE TRIGGER IF NOT EXISTS trg_instances_insert
        AFTER INSERT ON instances
        BEGIN
            UPDATE series
            SET num_instances = num_instances + 1,
                updated_at = datetime('now')
            WHERE series_pk = NEW.series_pk;

            UPDATE studies
            SET num_instances = num_instances + 1,
                updated_at = datetime('now')
            WHERE study_pk = (SELECT study_pk FROM series WHERE series_pk = NEW.series_pk);
        END;

        CREATE TRIGGER IF NOT EXISTS trg_instances_delete
        AFTER DELETE ON instances
        BEGIN
            UPDATE series
            SET num_instances = num_instances - 1,
                updated_at = datetime('now')
            WHERE series_pk = OLD.series_pk;

            UPDATE studies
            SET num_instances = num_instances - 1,
                updated_at = datetime('now')
            WHERE study_pk = (SELECT study_pk FROM series WHERE series_pk = OLD.series_pk);
        END;

        CREATE TRIGGER IF NOT EXISTS trg_series_insert
        AFTER INSERT ON series
        BEGIN
            UPDATE studies
            SET num_series = num_series + 1,
                updated_at = datetime('now')
            WHERE study_pk = NEW.study_pk;
        END;

        CREATE TRIGGER IF NOT EXISTS trg_series_delete
        AFTER DELETE ON series
        BEGIN
            UPDATE studies
            SET num_series = num_series - 1,
                updated_at = datetime('now')
            WHERE study_pk = OLD.study_pk;
        END;
    )";

    auto result = execute_sql(db, sql);
    if (result.is_err()) {
        return result;
    }

    return record_migration(db, 1, "Initial schema creation");
}

}  // namespace pacs::storage
