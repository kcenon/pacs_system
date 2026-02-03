/**
 * @file migration_runner.cpp
 * @brief Implementation of database schema migration runner
 *
 * When compiled with PACS_WITH_DATABASE_SYSTEM, uses pacs_database_adapter
 * for consistent database abstraction. Otherwise, uses direct SQLite
 * prepared statements.
 *
 * @see Issue #643 - Update migration_runner to use pacs_database_adapter
 */

#include <pacs/storage/migration_runner.hpp>

#include <sqlite3.h>

#include <pacs/compat/format.hpp>

#ifdef PACS_WITH_DATABASE_SYSTEM
#include <pacs/storage/pacs_database_adapter.hpp>
#endif

namespace pacs::storage {

// Use common_system's ok() function
using kcenon::common::ok;
using kcenon::common::make_error;

// ============================================================================
// Construction
// ============================================================================

migration_runner::migration_runner() {
    // Register all migrations (SQLite version)
    migrations_.push_back({1, [this](sqlite3* db) { return migrate_v1(db); }});
    migrations_.push_back({2, [this](sqlite3* db) { return migrate_v2(db); }});
    migrations_.push_back({3, [this](sqlite3* db) { return migrate_v3(db); }});
    migrations_.push_back({4, [this](sqlite3* db) { return migrate_v4(db); }});
    migrations_.push_back({5, [this](sqlite3* db) { return migrate_v5(db); }});
    migrations_.push_back({6, [this](sqlite3* db) { return migrate_v6(db); }});
    migrations_.push_back({7, [this](sqlite3* db) { return migrate_v7(db); }});

#ifdef PACS_WITH_DATABASE_SYSTEM
    // Register all migrations (pacs_database_adapter version)
    adapter_migrations_.push_back(
        {1, [this](pacs_database_adapter& db) { return migrate_v1(db); }});
    adapter_migrations_.push_back(
        {2, [this](pacs_database_adapter& db) { return migrate_v2(db); }});
    adapter_migrations_.push_back(
        {3, [this](pacs_database_adapter& db) { return migrate_v3(db); }});
    adapter_migrations_.push_back(
        {4, [this](pacs_database_adapter& db) { return migrate_v4(db); }});
    adapter_migrations_.push_back(
        {5, [this](pacs_database_adapter& db) { return migrate_v5(db); }});
    adapter_migrations_.push_back(
        {6, [this](pacs_database_adapter& db) { return migrate_v6(db); }});
    adapter_migrations_.push_back(
        {7, [this](pacs_database_adapter& db) { return migrate_v7(db); }});
#endif
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

auto migration_runner::migrate_v2(sqlite3* db) -> VoidResult {
    // V2: Add audit_log table for REST API audit endpoints
    const char* sql = R"(
        -- =====================================================================
        -- AUDIT_LOG TABLE (for REST API and HIPAA compliance)
        -- =====================================================================
        CREATE TABLE IF NOT EXISTS audit_log (
            audit_pk        INTEGER PRIMARY KEY AUTOINCREMENT,
            event_type      TEXT NOT NULL,
            outcome         TEXT DEFAULT 'SUCCESS',
            timestamp       TEXT NOT NULL DEFAULT (datetime('now')),
            user_id         TEXT,
            source_ae       TEXT,
            target_ae       TEXT,
            source_ip       TEXT,
            patient_id      TEXT,
            study_uid       TEXT,
            message         TEXT,
            details         TEXT,
            CHECK (outcome IN ('SUCCESS', 'FAILURE', 'WARNING'))
        );

        CREATE INDEX IF NOT EXISTS idx_audit_event_type ON audit_log(event_type);
        CREATE INDEX IF NOT EXISTS idx_audit_timestamp ON audit_log(timestamp);
        CREATE INDEX IF NOT EXISTS idx_audit_user ON audit_log(user_id);
        CREATE INDEX IF NOT EXISTS idx_audit_source_ae ON audit_log(source_ae);
        CREATE INDEX IF NOT EXISTS idx_audit_patient ON audit_log(patient_id);
        CREATE INDEX IF NOT EXISTS idx_audit_study ON audit_log(study_uid);
        CREATE INDEX IF NOT EXISTS idx_audit_outcome ON audit_log(outcome);
    )";

    auto result = execute_sql(db, sql);
    if (result.is_err()) {
        return result;
    }

    return record_migration(db, 2, "Add audit_log table");
}

auto migration_runner::migrate_v3(sqlite3* db) -> VoidResult {
    // V3: Add remote_nodes table for PACS client remote node management
    const char* sql = R"(
        -- =====================================================================
        -- REMOTE_NODES TABLE (for PACS client SCU operations)
        -- =====================================================================
        CREATE TABLE IF NOT EXISTS remote_nodes (
            pk                      INTEGER PRIMARY KEY AUTOINCREMENT,
            node_id                 TEXT NOT NULL UNIQUE,
            name                    TEXT,
            ae_title                TEXT NOT NULL,
            host                    TEXT NOT NULL,
            port                    INTEGER NOT NULL DEFAULT 104,
            supports_find           INTEGER NOT NULL DEFAULT 1,
            supports_move           INTEGER NOT NULL DEFAULT 1,
            supports_get            INTEGER NOT NULL DEFAULT 0,
            supports_store          INTEGER NOT NULL DEFAULT 1,
            supports_worklist       INTEGER NOT NULL DEFAULT 0,
            connection_timeout_sec  INTEGER NOT NULL DEFAULT 30,
            dimse_timeout_sec       INTEGER NOT NULL DEFAULT 60,
            max_associations        INTEGER NOT NULL DEFAULT 4,
            status                  TEXT NOT NULL DEFAULT 'unknown',
            last_verified           TEXT,
            last_error              TEXT,
            last_error_message      TEXT,
            created_at              TEXT NOT NULL DEFAULT (datetime('now')),
            updated_at              TEXT NOT NULL DEFAULT (datetime('now')),
            CHECK (port > 0 AND port <= 65535),
            CHECK (status IN ('unknown', 'online', 'offline', 'error', 'verifying'))
        );

        CREATE INDEX IF NOT EXISTS idx_remote_nodes_ae_title ON remote_nodes(ae_title);
        CREATE INDEX IF NOT EXISTS idx_remote_nodes_host ON remote_nodes(host);
        CREATE INDEX IF NOT EXISTS idx_remote_nodes_status ON remote_nodes(status);
    )";

    auto result = execute_sql(db, sql);
    if (result.is_err()) {
        return result;
    }

    return record_migration(db, 3, "Add remote_nodes table for PACS client");
}

auto migration_runner::migrate_v4(sqlite3* db) -> VoidResult {
    // V4: Add jobs table for async DICOM operations
    const char* sql = R"(
        -- =====================================================================
        -- JOBS TABLE (for async DICOM operations - Job Manager)
        -- =====================================================================
        CREATE TABLE IF NOT EXISTS jobs (
            pk                          INTEGER PRIMARY KEY AUTOINCREMENT,
            job_id                      TEXT NOT NULL UNIQUE,
            type                        TEXT NOT NULL,
            status                      TEXT NOT NULL DEFAULT 'pending',
            priority                    INTEGER NOT NULL DEFAULT 1,
            source_node_id              TEXT,
            destination_node_id         TEXT,
            patient_id                  TEXT,
            study_uid                   TEXT,
            series_uid                  TEXT,
            sop_instance_uid            TEXT,
            instance_uids_json          TEXT DEFAULT '[]',
            total_items                 INTEGER DEFAULT 0,
            completed_items             INTEGER DEFAULT 0,
            failed_items                INTEGER DEFAULT 0,
            skipped_items               INTEGER DEFAULT 0,
            bytes_transferred           INTEGER DEFAULT 0,
            current_item                TEXT,
            current_item_description    TEXT,
            error_message               TEXT,
            error_details               TEXT,
            retry_count                 INTEGER DEFAULT 0,
            max_retries                 INTEGER DEFAULT 3,
            created_by                  TEXT,
            metadata_json               TEXT DEFAULT '{}',
            created_at                  TEXT NOT NULL DEFAULT (datetime('now')),
            queued_at                   TEXT,
            started_at                  TEXT,
            completed_at                TEXT,
            CHECK (type IN ('query', 'retrieve', 'store', 'export', 'import', 'prefetch', 'sync')),
            CHECK (status IN ('pending', 'queued', 'running', 'completed', 'failed', 'cancelled', 'paused')),
            CHECK (priority >= 0 AND priority <= 3)
        );

        CREATE INDEX IF NOT EXISTS idx_jobs_status ON jobs(status);
        CREATE INDEX IF NOT EXISTS idx_jobs_type ON jobs(type);
        CREATE INDEX IF NOT EXISTS idx_jobs_priority ON jobs(priority DESC);
        CREATE INDEX IF NOT EXISTS idx_jobs_created_at ON jobs(created_at DESC);
        CREATE INDEX IF NOT EXISTS idx_jobs_source_node ON jobs(source_node_id);
        CREATE INDEX IF NOT EXISTS idx_jobs_destination_node ON jobs(destination_node_id);
        CREATE INDEX IF NOT EXISTS idx_jobs_study ON jobs(study_uid);
        CREATE INDEX IF NOT EXISTS idx_jobs_patient ON jobs(patient_id);
    )";

    auto result = execute_sql(db, sql);
    if (result.is_err()) {
        return result;
    }

    return record_migration(db, 4, "Add jobs table for async DICOM operations");
}

auto migration_runner::migrate_v5(sqlite3* db) -> VoidResult {
    // V5: Add routing_rules table for auto-forwarding
    const char* sql = R"(
        -- =====================================================================
        -- ROUTING_RULES TABLE (for auto-forwarding - Routing Manager)
        -- =====================================================================
        CREATE TABLE IF NOT EXISTS routing_rules (
            pk                  INTEGER PRIMARY KEY AUTOINCREMENT,
            rule_id             TEXT NOT NULL UNIQUE,
            name                TEXT NOT NULL,
            description         TEXT,
            enabled             INTEGER NOT NULL DEFAULT 1,
            priority            INTEGER NOT NULL DEFAULT 0,
            conditions_json     TEXT NOT NULL DEFAULT '[]',
            actions_json        TEXT NOT NULL DEFAULT '[]',
            schedule_cron       TEXT,
            effective_from      TEXT,
            effective_until     TEXT,
            triggered_count     INTEGER DEFAULT 0,
            success_count       INTEGER DEFAULT 0,
            failure_count       INTEGER DEFAULT 0,
            last_triggered      TEXT,
            created_at          TEXT NOT NULL DEFAULT (datetime('now')),
            updated_at          TEXT NOT NULL DEFAULT (datetime('now'))
        );

        CREATE INDEX IF NOT EXISTS idx_routing_rules_enabled ON routing_rules(enabled);
        CREATE INDEX IF NOT EXISTS idx_routing_rules_priority ON routing_rules(priority DESC);
    )";

    auto result = execute_sql(db, sql);
    if (result.is_err()) {
        return result;
    }

    return record_migration(db, 5, "Add routing_rules table for auto-forwarding");
}

auto migration_runner::migrate_v6(sqlite3* db) -> VoidResult {
    // V6: Add sync tables for bidirectional synchronization
    const char* sql = R"(
        -- =====================================================================
        -- SYNC_CONFIGS TABLE (for Sync Manager)
        -- =====================================================================
        CREATE TABLE IF NOT EXISTS sync_configs (
            pk                      INTEGER PRIMARY KEY AUTOINCREMENT,
            config_id               TEXT NOT NULL UNIQUE,
            source_node_id          TEXT NOT NULL,
            name                    TEXT NOT NULL,
            enabled                 INTEGER NOT NULL DEFAULT 1,
            lookback_hours          INTEGER NOT NULL DEFAULT 24,
            modalities_json         TEXT DEFAULT '[]',
            patient_patterns_json   TEXT DEFAULT '[]',
            sync_direction          TEXT NOT NULL DEFAULT 'pull',
            delete_missing          INTEGER NOT NULL DEFAULT 0,
            overwrite_existing      INTEGER NOT NULL DEFAULT 0,
            sync_metadata_only      INTEGER NOT NULL DEFAULT 0,
            schedule_cron           TEXT,
            last_sync               TEXT,
            last_successful_sync    TEXT,
            total_syncs             INTEGER DEFAULT 0,
            studies_synced          INTEGER DEFAULT 0,
            created_at              TEXT NOT NULL DEFAULT (datetime('now')),
            updated_at              TEXT NOT NULL DEFAULT (datetime('now')),
            CHECK (sync_direction IN ('pull', 'push', 'bidirectional'))
        );

        CREATE INDEX IF NOT EXISTS idx_sync_configs_enabled ON sync_configs(enabled);
        CREATE INDEX IF NOT EXISTS idx_sync_configs_source ON sync_configs(source_node_id);

        -- =====================================================================
        -- SYNC_CONFLICTS TABLE (for conflict tracking)
        -- =====================================================================
        CREATE TABLE IF NOT EXISTS sync_conflicts (
            pk                      INTEGER PRIMARY KEY AUTOINCREMENT,
            config_id               TEXT NOT NULL,
            study_uid               TEXT NOT NULL,
            patient_id              TEXT,
            conflict_type           TEXT NOT NULL,
            local_modified          TEXT,
            remote_modified         TEXT,
            local_instance_count    INTEGER DEFAULT 0,
            remote_instance_count   INTEGER DEFAULT 0,
            resolved                INTEGER NOT NULL DEFAULT 0,
            resolution              TEXT,
            detected_at             TEXT NOT NULL DEFAULT (datetime('now')),
            resolved_at             TEXT,
            UNIQUE (config_id, study_uid),
            CHECK (conflict_type IN ('missing_local', 'missing_remote', 'modified', 'count_mismatch')),
            CHECK (resolution IS NULL OR resolution IN ('prefer_local', 'prefer_remote', 'prefer_newer'))
        );

        CREATE INDEX IF NOT EXISTS idx_sync_conflicts_config ON sync_conflicts(config_id);
        CREATE INDEX IF NOT EXISTS idx_sync_conflicts_resolved ON sync_conflicts(resolved);
        CREATE INDEX IF NOT EXISTS idx_sync_conflicts_study ON sync_conflicts(study_uid);

        -- =====================================================================
        -- SYNC_HISTORY TABLE (for sync operation history)
        -- =====================================================================
        CREATE TABLE IF NOT EXISTS sync_history (
            pk                  INTEGER PRIMARY KEY AUTOINCREMENT,
            config_id           TEXT NOT NULL,
            job_id              TEXT NOT NULL,
            success             INTEGER NOT NULL DEFAULT 0,
            studies_checked     INTEGER DEFAULT 0,
            studies_synced      INTEGER DEFAULT 0,
            conflicts_found     INTEGER DEFAULT 0,
            errors_json         TEXT DEFAULT '[]',
            started_at          TEXT NOT NULL,
            completed_at        TEXT NOT NULL
        );

        CREATE INDEX IF NOT EXISTS idx_sync_history_config ON sync_history(config_id);
        CREATE INDEX IF NOT EXISTS idx_sync_history_started ON sync_history(started_at DESC);
    )";

    auto result = execute_sql(db, sql);
    if (result.is_err()) {
        return result;
    }

    return record_migration(db, 6, "Add sync tables for bidirectional synchronization");
}

auto migration_runner::migrate_v7(sqlite3* db) -> VoidResult {
    // V7: Add annotation and measurement tables for viewer functionality
    const char* sql = R"(
        -- =====================================================================
        -- ANNOTATIONS TABLE (for image annotations)
        -- =====================================================================
        CREATE TABLE IF NOT EXISTS annotations (
            pk                  INTEGER PRIMARY KEY AUTOINCREMENT,
            annotation_id       TEXT NOT NULL UNIQUE,
            study_uid           TEXT NOT NULL,
            series_uid          TEXT,
            sop_instance_uid    TEXT,
            frame_number        INTEGER,
            user_id             TEXT NOT NULL,
            annotation_type     TEXT NOT NULL,
            geometry_json       TEXT NOT NULL,
            text                TEXT,
            style_json          TEXT,
            created_at          TEXT NOT NULL DEFAULT (datetime('now')),
            updated_at          TEXT NOT NULL DEFAULT (datetime('now')),
            CHECK (annotation_type IN ('arrow', 'line', 'rectangle', 'ellipse', 'polygon', 'freehand', 'text', 'angle', 'roi'))
        );

        CREATE INDEX IF NOT EXISTS idx_annotations_study ON annotations(study_uid);
        CREATE INDEX IF NOT EXISTS idx_annotations_instance ON annotations(sop_instance_uid);
        CREATE INDEX IF NOT EXISTS idx_annotations_user ON annotations(user_id);

        -- =====================================================================
        -- MEASUREMENTS TABLE (for image measurements)
        -- =====================================================================
        CREATE TABLE IF NOT EXISTS measurements (
            pk                  INTEGER PRIMARY KEY AUTOINCREMENT,
            measurement_id      TEXT NOT NULL UNIQUE,
            sop_instance_uid    TEXT NOT NULL,
            frame_number        INTEGER,
            user_id             TEXT NOT NULL,
            measurement_type    TEXT NOT NULL,
            geometry_json       TEXT NOT NULL,
            value               REAL NOT NULL,
            unit                TEXT NOT NULL,
            label               TEXT,
            created_at          TEXT NOT NULL DEFAULT (datetime('now')),
            CHECK (measurement_type IN ('length', 'area', 'angle', 'hounsfield', 'suv', 'ellipse_area', 'polygon_area'))
        );

        CREATE INDEX IF NOT EXISTS idx_measurements_instance ON measurements(sop_instance_uid);
        CREATE INDEX IF NOT EXISTS idx_measurements_user ON measurements(user_id);

        -- =====================================================================
        -- KEY_IMAGES TABLE (for key image markers)
        -- =====================================================================
        CREATE TABLE IF NOT EXISTS key_images (
            pk                  INTEGER PRIMARY KEY AUTOINCREMENT,
            key_image_id        TEXT NOT NULL UNIQUE,
            study_uid           TEXT NOT NULL,
            sop_instance_uid    TEXT NOT NULL,
            frame_number        INTEGER,
            user_id             TEXT NOT NULL,
            reason              TEXT,
            document_title      TEXT,
            created_at          TEXT NOT NULL DEFAULT (datetime('now'))
        );

        CREATE INDEX IF NOT EXISTS idx_key_images_study ON key_images(study_uid);

        -- =====================================================================
        -- VIEWER_STATES TABLE (for saved viewer configurations)
        -- =====================================================================
        CREATE TABLE IF NOT EXISTS viewer_states (
            pk                  INTEGER PRIMARY KEY AUTOINCREMENT,
            state_id            TEXT NOT NULL UNIQUE,
            study_uid           TEXT NOT NULL,
            user_id             TEXT NOT NULL,
            state_json          TEXT NOT NULL,
            created_at          TEXT NOT NULL DEFAULT (datetime('now')),
            updated_at          TEXT NOT NULL DEFAULT (datetime('now'))
        );

        CREATE INDEX IF NOT EXISTS idx_viewer_states_study ON viewer_states(study_uid);
        CREATE INDEX IF NOT EXISTS idx_viewer_states_user ON viewer_states(user_id);

        -- =====================================================================
        -- RECENT_STUDIES TABLE (for tracking user study access)
        -- =====================================================================
        CREATE TABLE IF NOT EXISTS recent_studies (
            pk                  INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id             TEXT NOT NULL,
            study_uid           TEXT NOT NULL,
            accessed_at         TEXT NOT NULL DEFAULT (datetime('now')),
            UNIQUE (user_id, study_uid)
        );

        CREATE INDEX IF NOT EXISTS idx_recent_studies_user ON recent_studies(user_id, accessed_at DESC);
    )";

    auto result = execute_sql(db, sql);
    if (result.is_err()) {
        return result;
    }

    return record_migration(db, 7, "Add annotation and measurement tables");
}

#ifdef PACS_WITH_DATABASE_SYSTEM
// ============================================================================
// Migration Operations (pacs_database_adapter)
// ============================================================================

auto migration_runner::run_migrations(pacs_database_adapter& db) -> VoidResult {
    return run_migrations_to(db, LATEST_VERSION);
}

auto migration_runner::run_migrations_to(pacs_database_adapter& db,
                                         int target_version) -> VoidResult {
    if (!db.is_connected()) {
        return make_error<std::monostate>(
            -1, "Database adapter is not connected", "storage");
    }

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
        auto begin_result = db.begin_transaction();
        if (begin_result.is_err()) {
            return begin_result;
        }

        // Apply migration
        auto migration_result = apply_migration(db, next_version);
        if (migration_result.is_err()) {
            // Rollback on failure
            (void)db.rollback();
            return migration_result;
        }

        // Commit transaction
        auto commit_result = db.commit();
        if (commit_result.is_err()) {
            (void)db.rollback();
            return commit_result;
        }

        current_version = next_version;
    }

    return ok();
}

// ============================================================================
// Version Information (pacs_database_adapter)
// ============================================================================

auto migration_runner::get_current_version(pacs_database_adapter& db) const
    -> int {
    if (!db.is_connected()) {
        return 0;
    }

    // Check if schema_version table exists by querying sqlite_master
    const std::string check_sql =
        "SELECT name FROM sqlite_master WHERE type='table' AND "
        "name='schema_version';";

    auto check_result = db.select(check_sql);
    if (check_result.is_err() || check_result.value().empty()) {
        // Table doesn't exist
        return 0;
    }

    // Get max version from table
    const std::string version_sql =
        "SELECT MAX(version) AS max_ver FROM schema_version;";
    auto version_result = db.select(version_sql);
    if (version_result.is_err() || version_result.value().empty()) {
        return 0;
    }

    const auto& row = version_result.value()[0];
    auto it = row.find("max_ver");
    if (it == row.end()) {
        // Try alternative column names
        it = row.find("MAX(version)");
        if (it == row.end()) {
            it = row.find("max(version)");
        }
    }

    if (it != row.end()) {
        try {
            return std::stoi(it->second);
        } catch (...) {
            return 0;
        }
    }

    return 0;
}

auto migration_runner::needs_migration(pacs_database_adapter& db) const
    -> bool {
    return get_current_version(db) < LATEST_VERSION;
}

// ============================================================================
// Migration History (pacs_database_adapter)
// ============================================================================

auto migration_runner::get_history(pacs_database_adapter& db) const
    -> std::vector<migration_record> {
    std::vector<migration_record> history;

    if (!db.is_connected()) {
        return history;
    }

    const std::string sql =
        "SELECT version, description, applied_at FROM schema_version ORDER BY "
        "version;";

    auto result = db.select(sql);
    if (result.is_err()) {
        return history;
    }

    for (const auto& row : result.value()) {
        migration_record record;

        // Get version
        if (auto it = row.find("version"); it != row.end()) {
            try {
                record.version = std::stoi(it->second);
            } catch (...) {
                record.version = 0;
            }
        }

        // Get description
        if (auto it = row.find("description"); it != row.end()) {
            record.description = it->second;
        }

        // Get applied_at
        if (auto it = row.find("applied_at"); it != row.end()) {
            record.applied_at = it->second;
        }

        history.push_back(std::move(record));
    }

    return history;
}

// ============================================================================
// Internal Implementation (pacs_database_adapter)
// ============================================================================

auto migration_runner::ensure_schema_version_table(pacs_database_adapter& db)
    -> VoidResult {
    const std::string sql = R"(
        CREATE TABLE IF NOT EXISTS schema_version (
            version     INTEGER PRIMARY KEY,
            description TEXT NOT NULL,
            applied_at  TEXT NOT NULL DEFAULT (datetime('now'))
        );
    )";

    return execute_sql(db, sql);
}

auto migration_runner::apply_migration(pacs_database_adapter& db, int version)
    -> VoidResult {
    // Find the migration function
    for (const auto& [ver, func] : adapter_migrations_) {
        if (ver == version) {
            return func(db);
        }
    }

    return make_error<std::monostate>(
        -1,
        pacs::compat::format("Migration for version {} not found", version),
        "storage");
}

auto migration_runner::record_migration(pacs_database_adapter& db, int version,
                                        std::string_view description)
    -> VoidResult {
    // Use raw SQL for INSERT (simpler and more reliable for migrations)
    // Note: description is sanitized by the caller and controlled internally
    const std::string sql = pacs::compat::format(
        "INSERT INTO schema_version (version, description) VALUES ({}, '{}');",
        version, description);

    auto result = db.insert(sql);
    if (result.is_err()) {
        return make_error<std::monostate>(
            result.error().code,
            pacs::compat::format("Failed to record migration: {}",
                                 result.error().message),
            "storage");
    }

    return ok();
}

auto migration_runner::execute_sql(pacs_database_adapter& db,
                                   std::string_view sql) -> VoidResult {
    auto result = db.execute(std::string(sql));

    if (result.is_err()) {
        return result;
    }

    return ok();
}

// ============================================================================
// Migration Implementations (pacs_database_adapter)
// ============================================================================

auto migration_runner::migrate_v1(pacs_database_adapter& db) -> VoidResult {
    // V1: Initial schema - Create all base tables
    // Note: Same DDL as SQLite version since pacs_database_adapter supports raw
    // SQL
    const std::string sql = R"(
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

auto migration_runner::migrate_v2(pacs_database_adapter& db) -> VoidResult {
    // V2: Add audit_log table for REST API audit endpoints
    const std::string sql = R"(
        -- =====================================================================
        -- AUDIT_LOG TABLE (for REST API and HIPAA compliance)
        -- =====================================================================
        CREATE TABLE IF NOT EXISTS audit_log (
            audit_pk        INTEGER PRIMARY KEY AUTOINCREMENT,
            event_type      TEXT NOT NULL,
            outcome         TEXT DEFAULT 'SUCCESS',
            timestamp       TEXT NOT NULL DEFAULT (datetime('now')),
            user_id         TEXT,
            source_ae       TEXT,
            target_ae       TEXT,
            source_ip       TEXT,
            patient_id      TEXT,
            study_uid       TEXT,
            message         TEXT,
            details         TEXT,
            CHECK (outcome IN ('SUCCESS', 'FAILURE', 'WARNING'))
        );

        CREATE INDEX IF NOT EXISTS idx_audit_event_type ON audit_log(event_type);
        CREATE INDEX IF NOT EXISTS idx_audit_timestamp ON audit_log(timestamp);
        CREATE INDEX IF NOT EXISTS idx_audit_user ON audit_log(user_id);
        CREATE INDEX IF NOT EXISTS idx_audit_source_ae ON audit_log(source_ae);
        CREATE INDEX IF NOT EXISTS idx_audit_patient ON audit_log(patient_id);
        CREATE INDEX IF NOT EXISTS idx_audit_study ON audit_log(study_uid);
        CREATE INDEX IF NOT EXISTS idx_audit_outcome ON audit_log(outcome);
    )";

    auto result = execute_sql(db, sql);
    if (result.is_err()) {
        return result;
    }

    return record_migration(db, 2, "Add audit_log table");
}

auto migration_runner::migrate_v3(pacs_database_adapter& db) -> VoidResult {
    // V3: Add remote_nodes table for PACS client remote node management
    const std::string sql = R"(
        -- =====================================================================
        -- REMOTE_NODES TABLE (for PACS client SCU operations)
        -- =====================================================================
        CREATE TABLE IF NOT EXISTS remote_nodes (
            pk                      INTEGER PRIMARY KEY AUTOINCREMENT,
            node_id                 TEXT NOT NULL UNIQUE,
            name                    TEXT,
            ae_title                TEXT NOT NULL,
            host                    TEXT NOT NULL,
            port                    INTEGER NOT NULL DEFAULT 104,
            supports_find           INTEGER NOT NULL DEFAULT 1,
            supports_move           INTEGER NOT NULL DEFAULT 1,
            supports_get            INTEGER NOT NULL DEFAULT 0,
            supports_store          INTEGER NOT NULL DEFAULT 1,
            supports_worklist       INTEGER NOT NULL DEFAULT 0,
            connection_timeout_sec  INTEGER NOT NULL DEFAULT 30,
            dimse_timeout_sec       INTEGER NOT NULL DEFAULT 60,
            max_associations        INTEGER NOT NULL DEFAULT 4,
            status                  TEXT NOT NULL DEFAULT 'unknown',
            last_verified           TEXT,
            last_error              TEXT,
            last_error_message      TEXT,
            created_at              TEXT NOT NULL DEFAULT (datetime('now')),
            updated_at              TEXT NOT NULL DEFAULT (datetime('now')),
            CHECK (port > 0 AND port <= 65535),
            CHECK (status IN ('unknown', 'online', 'offline', 'error', 'verifying'))
        );

        CREATE INDEX IF NOT EXISTS idx_remote_nodes_ae_title ON remote_nodes(ae_title);
        CREATE INDEX IF NOT EXISTS idx_remote_nodes_host ON remote_nodes(host);
        CREATE INDEX IF NOT EXISTS idx_remote_nodes_status ON remote_nodes(status);
    )";

    auto result = execute_sql(db, sql);
    if (result.is_err()) {
        return result;
    }

    return record_migration(db, 3, "Add remote_nodes table for PACS client");
}

auto migration_runner::migrate_v4(pacs_database_adapter& db) -> VoidResult {
    // V4: Add jobs table for async DICOM operations
    const std::string sql = R"(
        -- =====================================================================
        -- JOBS TABLE (for async DICOM operations - Job Manager)
        -- =====================================================================
        CREATE TABLE IF NOT EXISTS jobs (
            pk                          INTEGER PRIMARY KEY AUTOINCREMENT,
            job_id                      TEXT NOT NULL UNIQUE,
            type                        TEXT NOT NULL,
            status                      TEXT NOT NULL DEFAULT 'pending',
            priority                    INTEGER NOT NULL DEFAULT 1,
            source_node_id              TEXT,
            destination_node_id         TEXT,
            patient_id                  TEXT,
            study_uid                   TEXT,
            series_uid                  TEXT,
            sop_instance_uid            TEXT,
            instance_uids_json          TEXT DEFAULT '[]',
            total_items                 INTEGER DEFAULT 0,
            completed_items             INTEGER DEFAULT 0,
            failed_items                INTEGER DEFAULT 0,
            skipped_items               INTEGER DEFAULT 0,
            bytes_transferred           INTEGER DEFAULT 0,
            current_item                TEXT,
            current_item_description    TEXT,
            error_message               TEXT,
            error_details               TEXT,
            retry_count                 INTEGER DEFAULT 0,
            max_retries                 INTEGER DEFAULT 3,
            created_by                  TEXT,
            metadata_json               TEXT DEFAULT '{}',
            created_at                  TEXT NOT NULL DEFAULT (datetime('now')),
            queued_at                   TEXT,
            started_at                  TEXT,
            completed_at                TEXT,
            CHECK (type IN ('query', 'retrieve', 'store', 'export', 'import', 'prefetch', 'sync')),
            CHECK (status IN ('pending', 'queued', 'running', 'completed', 'failed', 'cancelled', 'paused')),
            CHECK (priority >= 0 AND priority <= 3)
        );

        CREATE INDEX IF NOT EXISTS idx_jobs_status ON jobs(status);
        CREATE INDEX IF NOT EXISTS idx_jobs_type ON jobs(type);
        CREATE INDEX IF NOT EXISTS idx_jobs_priority ON jobs(priority DESC);
        CREATE INDEX IF NOT EXISTS idx_jobs_created_at ON jobs(created_at DESC);
        CREATE INDEX IF NOT EXISTS idx_jobs_source_node ON jobs(source_node_id);
        CREATE INDEX IF NOT EXISTS idx_jobs_destination_node ON jobs(destination_node_id);
        CREATE INDEX IF NOT EXISTS idx_jobs_study ON jobs(study_uid);
        CREATE INDEX IF NOT EXISTS idx_jobs_patient ON jobs(patient_id);
    )";

    auto result = execute_sql(db, sql);
    if (result.is_err()) {
        return result;
    }

    return record_migration(db, 4, "Add jobs table for async DICOM operations");
}

auto migration_runner::migrate_v5(pacs_database_adapter& db) -> VoidResult {
    // V5: Add routing_rules table for auto-forwarding
    const std::string sql = R"(
        -- =====================================================================
        -- ROUTING_RULES TABLE (for auto-forwarding - Routing Manager)
        -- =====================================================================
        CREATE TABLE IF NOT EXISTS routing_rules (
            pk                  INTEGER PRIMARY KEY AUTOINCREMENT,
            rule_id             TEXT NOT NULL UNIQUE,
            name                TEXT NOT NULL,
            description         TEXT,
            enabled             INTEGER NOT NULL DEFAULT 1,
            priority            INTEGER NOT NULL DEFAULT 0,
            conditions_json     TEXT NOT NULL DEFAULT '[]',
            actions_json        TEXT NOT NULL DEFAULT '[]',
            schedule_cron       TEXT,
            effective_from      TEXT,
            effective_until     TEXT,
            triggered_count     INTEGER DEFAULT 0,
            success_count       INTEGER DEFAULT 0,
            failure_count       INTEGER DEFAULT 0,
            last_triggered      TEXT,
            created_at          TEXT NOT NULL DEFAULT (datetime('now')),
            updated_at          TEXT NOT NULL DEFAULT (datetime('now'))
        );

        CREATE INDEX IF NOT EXISTS idx_routing_rules_enabled ON routing_rules(enabled);
        CREATE INDEX IF NOT EXISTS idx_routing_rules_priority ON routing_rules(priority DESC);
    )";

    auto result = execute_sql(db, sql);
    if (result.is_err()) {
        return result;
    }

    return record_migration(db, 5, "Add routing_rules table for auto-forwarding");
}

auto migration_runner::migrate_v6(pacs_database_adapter& db) -> VoidResult {
    // V6: Add sync tables for bidirectional synchronization
    const std::string sql = R"(
        -- =====================================================================
        -- SYNC_CONFIGS TABLE (for Sync Manager)
        -- =====================================================================
        CREATE TABLE IF NOT EXISTS sync_configs (
            pk                      INTEGER PRIMARY KEY AUTOINCREMENT,
            config_id               TEXT NOT NULL UNIQUE,
            source_node_id          TEXT NOT NULL,
            name                    TEXT NOT NULL,
            enabled                 INTEGER NOT NULL DEFAULT 1,
            lookback_hours          INTEGER NOT NULL DEFAULT 24,
            modalities_json         TEXT DEFAULT '[]',
            patient_patterns_json   TEXT DEFAULT '[]',
            sync_direction          TEXT NOT NULL DEFAULT 'pull',
            delete_missing          INTEGER NOT NULL DEFAULT 0,
            overwrite_existing      INTEGER NOT NULL DEFAULT 0,
            sync_metadata_only      INTEGER NOT NULL DEFAULT 0,
            schedule_cron           TEXT,
            last_sync               TEXT,
            last_successful_sync    TEXT,
            total_syncs             INTEGER DEFAULT 0,
            studies_synced          INTEGER DEFAULT 0,
            created_at              TEXT NOT NULL DEFAULT (datetime('now')),
            updated_at              TEXT NOT NULL DEFAULT (datetime('now')),
            CHECK (sync_direction IN ('pull', 'push', 'bidirectional'))
        );

        CREATE INDEX IF NOT EXISTS idx_sync_configs_enabled ON sync_configs(enabled);
        CREATE INDEX IF NOT EXISTS idx_sync_configs_source ON sync_configs(source_node_id);

        -- =====================================================================
        -- SYNC_CONFLICTS TABLE (for conflict tracking)
        -- =====================================================================
        CREATE TABLE IF NOT EXISTS sync_conflicts (
            pk                      INTEGER PRIMARY KEY AUTOINCREMENT,
            config_id               TEXT NOT NULL,
            study_uid               TEXT NOT NULL,
            patient_id              TEXT,
            conflict_type           TEXT NOT NULL,
            local_modified          TEXT,
            remote_modified         TEXT,
            local_instance_count    INTEGER DEFAULT 0,
            remote_instance_count   INTEGER DEFAULT 0,
            resolved                INTEGER NOT NULL DEFAULT 0,
            resolution              TEXT,
            detected_at             TEXT NOT NULL DEFAULT (datetime('now')),
            resolved_at             TEXT,
            UNIQUE (config_id, study_uid),
            CHECK (conflict_type IN ('missing_local', 'missing_remote', 'modified', 'count_mismatch')),
            CHECK (resolution IS NULL OR resolution IN ('prefer_local', 'prefer_remote', 'prefer_newer'))
        );

        CREATE INDEX IF NOT EXISTS idx_sync_conflicts_config ON sync_conflicts(config_id);
        CREATE INDEX IF NOT EXISTS idx_sync_conflicts_resolved ON sync_conflicts(resolved);
        CREATE INDEX IF NOT EXISTS idx_sync_conflicts_study ON sync_conflicts(study_uid);

        -- =====================================================================
        -- SYNC_HISTORY TABLE (for sync operation history)
        -- =====================================================================
        CREATE TABLE IF NOT EXISTS sync_history (
            pk                  INTEGER PRIMARY KEY AUTOINCREMENT,
            config_id           TEXT NOT NULL,
            job_id              TEXT NOT NULL,
            success             INTEGER NOT NULL DEFAULT 0,
            studies_checked     INTEGER DEFAULT 0,
            studies_synced      INTEGER DEFAULT 0,
            conflicts_found     INTEGER DEFAULT 0,
            errors_json         TEXT DEFAULT '[]',
            started_at          TEXT NOT NULL,
            completed_at        TEXT NOT NULL
        );

        CREATE INDEX IF NOT EXISTS idx_sync_history_config ON sync_history(config_id);
        CREATE INDEX IF NOT EXISTS idx_sync_history_started ON sync_history(started_at DESC);
    )";

    auto result = execute_sql(db, sql);
    if (result.is_err()) {
        return result;
    }

    return record_migration(db, 6, "Add sync tables for bidirectional synchronization");
}

auto migration_runner::migrate_v7(pacs_database_adapter& db) -> VoidResult {
    // V7: Add annotation and measurement tables for viewer functionality
    const std::string sql = R"(
        -- =====================================================================
        -- ANNOTATIONS TABLE (for image annotations)
        -- =====================================================================
        CREATE TABLE IF NOT EXISTS annotations (
            pk                  INTEGER PRIMARY KEY AUTOINCREMENT,
            annotation_id       TEXT NOT NULL UNIQUE,
            study_uid           TEXT NOT NULL,
            series_uid          TEXT,
            sop_instance_uid    TEXT,
            frame_number        INTEGER,
            user_id             TEXT NOT NULL,
            annotation_type     TEXT NOT NULL,
            geometry_json       TEXT NOT NULL,
            text                TEXT,
            style_json          TEXT,
            created_at          TEXT NOT NULL DEFAULT (datetime('now')),
            updated_at          TEXT NOT NULL DEFAULT (datetime('now')),
            CHECK (annotation_type IN ('arrow', 'line', 'rectangle', 'ellipse', 'polygon', 'freehand', 'text', 'angle', 'roi'))
        );

        CREATE INDEX IF NOT EXISTS idx_annotations_study ON annotations(study_uid);
        CREATE INDEX IF NOT EXISTS idx_annotations_instance ON annotations(sop_instance_uid);
        CREATE INDEX IF NOT EXISTS idx_annotations_user ON annotations(user_id);

        -- =====================================================================
        -- MEASUREMENTS TABLE (for image measurements)
        -- =====================================================================
        CREATE TABLE IF NOT EXISTS measurements (
            pk                  INTEGER PRIMARY KEY AUTOINCREMENT,
            measurement_id      TEXT NOT NULL UNIQUE,
            sop_instance_uid    TEXT NOT NULL,
            frame_number        INTEGER,
            user_id             TEXT NOT NULL,
            measurement_type    TEXT NOT NULL,
            geometry_json       TEXT NOT NULL,
            value               REAL NOT NULL,
            unit                TEXT NOT NULL,
            label               TEXT,
            created_at          TEXT NOT NULL DEFAULT (datetime('now')),
            CHECK (measurement_type IN ('length', 'area', 'angle', 'hounsfield', 'suv', 'ellipse_area', 'polygon_area'))
        );

        CREATE INDEX IF NOT EXISTS idx_measurements_instance ON measurements(sop_instance_uid);
        CREATE INDEX IF NOT EXISTS idx_measurements_user ON measurements(user_id);

        -- =====================================================================
        -- KEY_IMAGES TABLE (for key image markers)
        -- =====================================================================
        CREATE TABLE IF NOT EXISTS key_images (
            pk                  INTEGER PRIMARY KEY AUTOINCREMENT,
            key_image_id        TEXT NOT NULL UNIQUE,
            study_uid           TEXT NOT NULL,
            sop_instance_uid    TEXT NOT NULL,
            frame_number        INTEGER,
            user_id             TEXT NOT NULL,
            reason              TEXT,
            document_title      TEXT,
            created_at          TEXT NOT NULL DEFAULT (datetime('now'))
        );

        CREATE INDEX IF NOT EXISTS idx_key_images_study ON key_images(study_uid);

        -- =====================================================================
        -- VIEWER_STATES TABLE (for saved viewer configurations)
        -- =====================================================================
        CREATE TABLE IF NOT EXISTS viewer_states (
            pk                  INTEGER PRIMARY KEY AUTOINCREMENT,
            state_id            TEXT NOT NULL UNIQUE,
            study_uid           TEXT NOT NULL,
            user_id             TEXT NOT NULL,
            state_json          TEXT NOT NULL,
            created_at          TEXT NOT NULL DEFAULT (datetime('now')),
            updated_at          TEXT NOT NULL DEFAULT (datetime('now'))
        );

        CREATE INDEX IF NOT EXISTS idx_viewer_states_study ON viewer_states(study_uid);
        CREATE INDEX IF NOT EXISTS idx_viewer_states_user ON viewer_states(user_id);

        -- =====================================================================
        -- RECENT_STUDIES TABLE (for tracking user study access)
        -- =====================================================================
        CREATE TABLE IF NOT EXISTS recent_studies (
            pk                  INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id             TEXT NOT NULL,
            study_uid           TEXT NOT NULL,
            accessed_at         TEXT NOT NULL DEFAULT (datetime('now')),
            UNIQUE (user_id, study_uid)
        );

        CREATE INDEX IF NOT EXISTS idx_recent_studies_user ON recent_studies(user_id, accessed_at DESC);
    )";

    auto result = execute_sql(db, sql);
    if (result.is_err()) {
        return result;
    }

    return record_migration(db, 7, "Add annotation and measurement tables");
}

#endif  // PACS_WITH_DATABASE_SYSTEM

}  // namespace pacs::storage
