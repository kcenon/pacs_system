/**
 * @file migration_runner_test.cpp
 * @brief Unit tests for migration_runner class
 */

#include <catch2/catch_test_macros.hpp>

#include <pacs/storage/migration_runner.hpp>

#include <sqlite3.h>

#include <memory>

using namespace pacs::storage;

// ============================================================================
// Test Utilities
// ============================================================================

namespace {

/// RAII wrapper for SQLite database
class test_database {
public:
    test_database() {
        auto rc = sqlite3_open(":memory:", &db_);
        if (rc != SQLITE_OK) {
            throw std::runtime_error("Failed to open in-memory database");
        }
    }

    ~test_database() {
        if (db_ != nullptr) {
            sqlite3_close(db_);
        }
    }

    test_database(const test_database&) = delete;
    auto operator=(const test_database&) -> test_database& = delete;
    test_database(test_database&&) = delete;
    auto operator=(test_database&&) -> test_database& = delete;

    [[nodiscard]] auto get() const noexcept -> sqlite3* { return db_; }

    [[nodiscard]] auto table_exists(const char* table_name) const -> bool {
        const char* sql =
            "SELECT name FROM sqlite_master WHERE type='table' AND name=?;";
        sqlite3_stmt* stmt = nullptr;
        auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            return false;
        }
        sqlite3_bind_text(stmt, 1, table_name, -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return rc == SQLITE_ROW;
    }

    [[nodiscard]] auto index_exists(const char* index_name) const -> bool {
        const char* sql =
            "SELECT name FROM sqlite_master WHERE type='index' AND name=?;";
        sqlite3_stmt* stmt = nullptr;
        auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            return false;
        }
        sqlite3_bind_text(stmt, 1, index_name, -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return rc == SQLITE_ROW;
    }

    [[nodiscard]] auto trigger_exists(const char* trigger_name) const -> bool {
        const char* sql =
            "SELECT name FROM sqlite_master WHERE type='trigger' AND name=?;";
        sqlite3_stmt* stmt = nullptr;
        auto rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            return false;
        }
        sqlite3_bind_text(stmt, 1, trigger_name, -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return rc == SQLITE_ROW;
    }

private:
    sqlite3* db_ = nullptr;
};

}  // namespace

// ============================================================================
// Initial Migration Tests
// ============================================================================

TEST_CASE("migration_runner initial state", "[migration][version]") {
    test_database db;
    migration_runner runner;

    SECTION("empty database has version 0") {
        CHECK(runner.get_current_version(db.get()) == 0);
    }

    SECTION("empty database needs migration") {
        CHECK(runner.needs_migration(db.get()));
    }

    SECTION("latest version is 6") {
        CHECK(runner.get_latest_version() == 6);
    }

    SECTION("empty database has no history") {
        auto history = runner.get_history(db.get());
        CHECK(history.empty());
    }
}

// ============================================================================
// Migration Execution Tests
// ============================================================================

TEST_CASE("migration_runner run_migrations", "[migration][execute]") {
    test_database db;
    migration_runner runner;

    SECTION("successful initial migration") {
        auto result = runner.run_migrations(db.get());
        REQUIRE(result.is_ok());

        CHECK(runner.get_current_version(db.get()) == 6);
        CHECK_FALSE(runner.needs_migration(db.get()));
    }

    SECTION("migration is idempotent") {
        auto result1 = runner.run_migrations(db.get());
        REQUIRE(result1.is_ok());

        auto result2 = runner.run_migrations(db.get());
        REQUIRE(result2.is_ok());

        CHECK(runner.get_current_version(db.get()) == 6);
    }

    SECTION("migration creates schema_version table") {
        auto result = runner.run_migrations(db.get());
        REQUIRE(result.is_ok());

        CHECK(db.table_exists("schema_version"));
    }

    SECTION("migration records history") {
        auto result = runner.run_migrations(db.get());
        REQUIRE(result.is_ok());

        auto history = runner.get_history(db.get());
        REQUIRE(history.size() == 6);
        CHECK(history[0].version == 1);
        CHECK(history[0].description == "Initial schema creation");
        CHECK_FALSE(history[0].applied_at.empty());
        CHECK(history[1].version == 2);
        CHECK(history[1].description == "Add audit_log table");
        CHECK_FALSE(history[1].applied_at.empty());
        CHECK(history[2].version == 3);
        CHECK(history[2].description == "Add remote_nodes table for PACS client");
        CHECK_FALSE(history[2].applied_at.empty());
        CHECK(history[3].version == 4);
        CHECK(history[3].description == "Add jobs table for async DICOM operations");
        CHECK_FALSE(history[3].applied_at.empty());
        CHECK(history[4].version == 5);
        CHECK(history[4].description == "Add routing_rules table for auto-forwarding");
        CHECK_FALSE(history[4].applied_at.empty());
        CHECK(history[5].version == 6);
        CHECK(history[5].description == "Add sync tables for bidirectional synchronization");
        CHECK_FALSE(history[5].applied_at.empty());
    }
}

// ============================================================================
// Schema Validation Tests (V1)
// ============================================================================

TEST_CASE("migration_runner v1 creates tables", "[migration][v1][tables]") {
    test_database db;
    migration_runner runner;

    auto result = runner.run_migrations(db.get());
    REQUIRE(result.is_ok());

    SECTION("patients table exists") {
        CHECK(db.table_exists("patients"));
    }

    SECTION("studies table exists") {
        CHECK(db.table_exists("studies"));
    }

    SECTION("series table exists") {
        CHECK(db.table_exists("series"));
    }

    SECTION("instances table exists") {
        CHECK(db.table_exists("instances"));
    }

    SECTION("mpps table exists") {
        CHECK(db.table_exists("mpps"));
    }

    SECTION("worklist table exists") {
        CHECK(db.table_exists("worklist"));
    }
}

TEST_CASE("migration_runner v1 creates indexes", "[migration][v1][indexes]") {
    test_database db;
    migration_runner runner;

    auto result = runner.run_migrations(db.get());
    REQUIRE(result.is_ok());

    SECTION("patients indexes exist") {
        CHECK(db.index_exists("idx_patients_name"));
        CHECK(db.index_exists("idx_patients_birth"));
    }

    SECTION("studies indexes exist") {
        CHECK(db.index_exists("idx_studies_patient"));
        CHECK(db.index_exists("idx_studies_date"));
        CHECK(db.index_exists("idx_studies_accession"));
    }

    SECTION("series indexes exist") {
        CHECK(db.index_exists("idx_series_study"));
        CHECK(db.index_exists("idx_series_modality"));
    }

    SECTION("instances indexes exist") {
        CHECK(db.index_exists("idx_instances_series"));
        CHECK(db.index_exists("idx_instances_sop_class"));
        CHECK(db.index_exists("idx_instances_number"));
        CHECK(db.index_exists("idx_instances_created"));
    }

    SECTION("mpps indexes exist") {
        CHECK(db.index_exists("idx_mpps_status"));
        CHECK(db.index_exists("idx_mpps_station"));
        CHECK(db.index_exists("idx_mpps_study"));
        CHECK(db.index_exists("idx_mpps_date"));
    }

    SECTION("worklist indexes exist") {
        CHECK(db.index_exists("idx_worklist_station"));
        CHECK(db.index_exists("idx_worklist_modality"));
        CHECK(db.index_exists("idx_worklist_scheduled"));
        CHECK(db.index_exists("idx_worklist_patient"));
        CHECK(db.index_exists("idx_worklist_accession"));
        CHECK(db.index_exists("idx_worklist_status"));
        CHECK(db.index_exists("idx_worklist_station_date_mod"));
    }
}

TEST_CASE("migration_runner v1 creates triggers", "[migration][v1][triggers]") {
    test_database db;
    migration_runner runner;

    auto result = runner.run_migrations(db.get());
    REQUIRE(result.is_ok());

    CHECK(db.trigger_exists("trg_instances_insert"));
    CHECK(db.trigger_exists("trg_instances_delete"));
    CHECK(db.trigger_exists("trg_series_insert"));
    CHECK(db.trigger_exists("trg_series_delete"));
}

// ============================================================================
// Targeted Version Migration Tests
// ============================================================================

TEST_CASE("migration_runner run_migrations_to", "[migration][targeted]") {
    test_database db;
    migration_runner runner;

    SECTION("migrate to version 1") {
        auto result = runner.run_migrations_to(db.get(), 1);
        REQUIRE(result.is_ok());
        CHECK(runner.get_current_version(db.get()) == 1);
    }

    SECTION("migrate to version 0 is no-op") {
        auto result = runner.run_migrations_to(db.get(), 0);
        REQUIRE(result.is_ok());
        CHECK(runner.get_current_version(db.get()) == 0);
    }

    SECTION("migrate to invalid version fails") {
        auto result = runner.run_migrations_to(db.get(), 999);
        CHECK(result.is_err());
    }
}

// ============================================================================
// Trigger Functionality Tests
// ============================================================================

TEST_CASE("migration_runner triggers update counts", "[migration][triggers]") {
    test_database db;
    migration_runner runner;

    auto result = runner.run_migrations(db.get());
    REQUIRE(result.is_ok());

    // Insert test data
    const char* insert_patient = R"(
        INSERT INTO patients (patient_id, patient_name)
        VALUES ('P001', 'Test^Patient');
    )";
    sqlite3_exec(db.get(), insert_patient, nullptr, nullptr, nullptr);

    const char* insert_study = R"(
        INSERT INTO studies (patient_pk, study_uid, study_id)
        VALUES (1, '1.2.3.4.5', 'S001');
    )";
    sqlite3_exec(db.get(), insert_study, nullptr, nullptr, nullptr);

    const char* insert_series = R"(
        INSERT INTO series (study_pk, series_uid, series_number, modality)
        VALUES (1, '1.2.3.4.5.1', 1, 'CT');
    )";
    sqlite3_exec(db.get(), insert_series, nullptr, nullptr, nullptr);

    SECTION("series insert updates study count") {
        const char* query = "SELECT num_series FROM studies WHERE study_pk = 1;";
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db.get(), query, -1, &stmt, nullptr);
        sqlite3_step(stmt);
        auto count = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);

        CHECK(count == 1);
    }

    SECTION("instance insert updates parent counts") {
        const char* insert_instance = R"(
            INSERT INTO instances (series_pk, sop_uid, sop_class_uid, file_path, file_size)
            VALUES (1, '1.2.3.4.5.1.1', '1.2.840.10008.5.1.4.1.1.2', '/path/to/file.dcm', 1024);
        )";
        sqlite3_exec(db.get(), insert_instance, nullptr, nullptr, nullptr);

        // Check series count
        const char* series_query = "SELECT num_instances FROM series WHERE series_pk = 1;";
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db.get(), series_query, -1, &stmt, nullptr);
        sqlite3_step(stmt);
        auto series_count = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
        CHECK(series_count == 1);

        // Check study count
        const char* study_query = "SELECT num_instances FROM studies WHERE study_pk = 1;";
        sqlite3_prepare_v2(db.get(), study_query, -1, &stmt, nullptr);
        sqlite3_step(stmt);
        auto study_count = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
        CHECK(study_count == 1);
    }

    SECTION("instance delete updates parent counts") {
        // Insert then delete
        const char* insert_instance = R"(
            INSERT INTO instances (series_pk, sop_uid, sop_class_uid, file_path, file_size)
            VALUES (1, '1.2.3.4.5.1.1', '1.2.840.10008.5.1.4.1.1.2', '/path/to/file.dcm', 1024);
        )";
        sqlite3_exec(db.get(), insert_instance, nullptr, nullptr, nullptr);

        const char* delete_instance = R"(
            DELETE FROM instances WHERE sop_uid = '1.2.3.4.5.1.1';
        )";
        sqlite3_exec(db.get(), delete_instance, nullptr, nullptr, nullptr);

        // Check series count
        const char* series_query = "SELECT num_instances FROM series WHERE series_pk = 1;";
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(db.get(), series_query, -1, &stmt, nullptr);
        sqlite3_step(stmt);
        auto series_count = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
        CHECK(series_count == 0);
    }
}

// ============================================================================
// Schema Validation Tests (V2)
// ============================================================================

TEST_CASE("migration_runner v2 creates audit_log table", "[migration][v2][tables]") {
    test_database db;
    migration_runner runner;

    auto result = runner.run_migrations(db.get());
    REQUIRE(result.is_ok());

    CHECK(db.table_exists("audit_log"));
    CHECK(db.index_exists("idx_audit_event_type"));
    CHECK(db.index_exists("idx_audit_timestamp"));
    CHECK(db.index_exists("idx_audit_user"));
    CHECK(db.index_exists("idx_audit_source_ae"));
    CHECK(db.index_exists("idx_audit_patient"));
    CHECK(db.index_exists("idx_audit_study"));
    CHECK(db.index_exists("idx_audit_outcome"));
}

// ============================================================================
// Schema Validation Tests (V3)
// ============================================================================

TEST_CASE("migration_runner v3 creates remote_nodes table", "[migration][v3][tables]") {
    test_database db;
    migration_runner runner;

    auto result = runner.run_migrations(db.get());
    REQUIRE(result.is_ok());

    CHECK(db.table_exists("remote_nodes"));
    CHECK(db.index_exists("idx_remote_nodes_ae_title"));
    CHECK(db.index_exists("idx_remote_nodes_host"));
    CHECK(db.index_exists("idx_remote_nodes_status"));
}

// ============================================================================
// Schema Validation Tests (V4)
// ============================================================================

TEST_CASE("migration_runner v4 creates jobs table", "[migration][v4][tables]") {
    test_database db;
    migration_runner runner;

    auto result = runner.run_migrations(db.get());
    REQUIRE(result.is_ok());

    CHECK(db.table_exists("jobs"));
    CHECK(db.index_exists("idx_jobs_status"));
    CHECK(db.index_exists("idx_jobs_type"));
    CHECK(db.index_exists("idx_jobs_priority"));
    CHECK(db.index_exists("idx_jobs_created_at"));
    CHECK(db.index_exists("idx_jobs_source_node"));
    CHECK(db.index_exists("idx_jobs_destination_node"));
    CHECK(db.index_exists("idx_jobs_study"));
    CHECK(db.index_exists("idx_jobs_patient"));
}

// ============================================================================
// Schema Validation Tests (V5)
// ============================================================================

TEST_CASE("migration_runner v5 creates routing_rules table", "[migration][v5][tables]") {
    test_database db;
    migration_runner runner;

    auto result = runner.run_migrations(db.get());
    REQUIRE(result.is_ok());

    CHECK(db.table_exists("routing_rules"));
    CHECK(db.index_exists("idx_routing_rules_enabled"));
    CHECK(db.index_exists("idx_routing_rules_priority"));
}
