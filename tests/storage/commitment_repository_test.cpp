/**
 * @file commitment_repository_test.cpp
 * @brief Unit tests for commitment_repository class
 *
 * @see Issue #711 - Storage Commitment database tracking
 */

#include <catch2/catch_test_macros.hpp>

#include <pacs/storage/commitment_repository.hpp>
#include <pacs/storage/migration_runner.hpp>

#ifdef PACS_WITH_DATABASE_SYSTEM
#include <pacs/storage/pacs_database_adapter.hpp>
#else
#include <sqlite3.h>
#endif

using namespace pacs::storage;
using namespace pacs::services;

namespace {

#ifdef PACS_WITH_DATABASE_SYSTEM

bool is_sqlite_backend_supported() {
    pacs_database_adapter db(":memory:");
    auto result = db.connect();
    return result.is_ok();
}

class test_database {
public:
    test_database() {
        db_ = std::make_shared<pacs_database_adapter>(":memory:");
        auto conn_result = db_->connect();
        if (conn_result.is_err()) {
            throw std::runtime_error(
                "Failed to connect: " + conn_result.error().message);
        }
        migration_runner runner;
        auto result = runner.run_migrations(*db_);
        if (result.is_err()) {
            throw std::runtime_error(
                "Migration failed: " + result.error().message);
        }
    }

    [[nodiscard]] auto get() const noexcept
        -> std::shared_ptr<pacs_database_adapter> {
        return db_;
    }

private:
    std::shared_ptr<pacs_database_adapter> db_;
};

// ============================================================================
// Migration Tests
// ============================================================================

TEST_CASE("migration V8 creates commitment tables",
          "[storage][commitment][migration]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported by unified_database_system");
        return;
    }
    test_database tdb;

    SECTION("schema version is 9") {
        migration_runner runner;
        CHECK(runner.get_current_version(*tdb.get()) == 9);
    }

    SECTION("storage_commitment table exists") {
        auto result = tdb.get()->select(
            "SELECT count(*) as cnt FROM sqlite_master "
            "WHERE type='table' AND name='storage_commitment'");
        REQUIRE(result.is_ok());
        REQUIRE_FALSE(result.value().empty());
        CHECK(result.value()[0].at("cnt") == "1");
    }

    SECTION("commitment_references table exists") {
        auto result = tdb.get()->select(
            "SELECT count(*) as cnt FROM sqlite_master "
            "WHERE type='table' AND name='commitment_references'");
        REQUIRE(result.is_ok());
        REQUIRE_FALSE(result.value().empty());
        CHECK(result.value()[0].at("cnt") == "1");
    }
}

// ============================================================================
// Repository Construction
// ============================================================================

TEST_CASE("commitment_repository construction",
          "[storage][commitment][repository]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported by unified_database_system");
        return;
    }
    test_database tdb;
    commitment_repository repo(tdb.get());

    SECTION("initial count is zero") {
        auto result = repo.count();
        REQUIRE(result.is_ok());
        CHECK(result.value() == 0);
    }
}

// ============================================================================
// Record Request
// ============================================================================

TEST_CASE("commitment_repository record_request",
          "[storage][commitment][repository]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported by unified_database_system");
        return;
    }
    test_database tdb;
    commitment_repository repo(tdb.get());

    std::vector<sop_reference> refs = {
        {"1.2.840.10008.5.1.4.1.1.2", "1.2.3.4.5.1"},
        {"1.2.840.10008.5.1.4.1.1.2", "1.2.3.4.5.2"},
    };

    SECTION("inserts commitment record and references") {
        auto result = repo.record_request("2.25.111", "SCU_AE", refs);
        REQUIRE(result.is_ok());

        auto count = repo.count();
        REQUIRE(count.is_ok());
        CHECK(count.value() == 1);

        auto record = repo.find_by_id("2.25.111");
        REQUIRE(record.is_ok());
        CHECK(record.value().transaction_uid == "2.25.111");
        CHECK(record.value().requesting_ae == "SCU_AE");
        CHECK(record.value().status == commitment_status::pending);
        CHECK(record.value().total_instances == 2);
        CHECK(record.value().success_count == 0);
        CHECK(record.value().failure_count == 0);
    }

    SECTION("inserts reference records") {
        auto result = repo.record_request("2.25.222", "SCU_AE", refs);
        REQUIRE(result.is_ok());

        auto ref_result = repo.get_references("2.25.222");
        REQUIRE(ref_result.is_ok());
        CHECK(ref_result.value().size() == 2);
        CHECK(ref_result.value()[0].status == "pending");
        CHECK(ref_result.value()[1].status == "pending");
    }
}

// ============================================================================
// Duplicate Transaction Detection
// ============================================================================

TEST_CASE("commitment_repository duplicate detection",
          "[storage][commitment][repository]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported by unified_database_system");
        return;
    }
    test_database tdb;
    commitment_repository repo(tdb.get());

    std::vector<sop_reference> refs = {
        {"1.2.840.10008.5.1.4.1.1.2", "1.2.3.4.5.1"},
    };

    auto result = repo.record_request("2.25.333", "SCU_AE", refs);
    REQUIRE(result.is_ok());

    SECTION("detects existing transaction") {
        CHECK(repo.is_duplicate_transaction("2.25.333"));
    }

    SECTION("returns false for non-existent transaction") {
        CHECK_FALSE(repo.is_duplicate_transaction("2.25.999"));
    }
}

// ============================================================================
// Update Result - All Success
// ============================================================================

TEST_CASE("commitment_repository update_result all success",
          "[storage][commitment][repository]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported by unified_database_system");
        return;
    }
    test_database tdb;
    commitment_repository repo(tdb.get());

    std::vector<sop_reference> refs = {
        {"1.2.840.10008.5.1.4.1.1.2", "1.2.3.4.5.1"},
        {"1.2.840.10008.5.1.4.1.1.2", "1.2.3.4.5.2"},
    };
    auto req = repo.record_request("2.25.444", "SCU_AE", refs);
    REQUIRE(req.is_ok());

    commitment_result cr;
    cr.transaction_uid = "2.25.444";
    cr.success_references = refs;

    auto result = repo.update_result("2.25.444", cr);
    REQUIRE(result.is_ok());

    auto record = repo.find_by_id("2.25.444");
    REQUIRE(record.is_ok());
    CHECK(record.value().status == commitment_status::success);
    CHECK(record.value().success_count == 2);
    CHECK(record.value().failure_count == 0);
    CHECK_FALSE(record.value().completion_time.empty());

    // Check reference statuses
    auto ref_result = repo.get_references("2.25.444");
    REQUIRE(ref_result.is_ok());
    for (const auto& ref : ref_result.value()) {
        CHECK(ref.status == "success");
    }
}

// ============================================================================
// Update Result - Mixed Success/Failure
// ============================================================================

TEST_CASE("commitment_repository update_result partial failure",
          "[storage][commitment][repository]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported by unified_database_system");
        return;
    }
    test_database tdb;
    commitment_repository repo(tdb.get());

    std::vector<sop_reference> refs = {
        {"1.2.840.10008.5.1.4.1.1.2", "1.2.3.4.5.1"},
        {"1.2.840.10008.5.1.4.1.1.2", "1.2.3.4.5.2"},
    };
    auto req = repo.record_request("2.25.555", "SCU_AE", refs);
    REQUIRE(req.is_ok());

    commitment_result cr;
    cr.transaction_uid = "2.25.555";
    cr.success_references = {refs[0]};
    cr.failed_references = {
        {refs[1], commitment_failure_reason::no_such_object_instance}};

    auto result = repo.update_result("2.25.555", cr);
    REQUIRE(result.is_ok());

    auto record = repo.find_by_id("2.25.555");
    REQUIRE(record.is_ok());
    CHECK(record.value().status == commitment_status::partial);
    CHECK(record.value().success_count == 1);
    CHECK(record.value().failure_count == 1);
}

// ============================================================================
// Get Status
// ============================================================================

TEST_CASE("commitment_repository get_status",
          "[storage][commitment][repository]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported by unified_database_system");
        return;
    }
    test_database tdb;
    commitment_repository repo(tdb.get());

    std::vector<sop_reference> refs = {
        {"1.2.840.10008.5.1.4.1.1.2", "1.2.3.4.5.1"},
    };
    auto req = repo.record_request("2.25.666", "SCU_AE", refs);
    REQUIRE(req.is_ok());

    SECTION("returns pending for new request") {
        auto status = repo.get_status("2.25.666");
        REQUIRE(status.is_ok());
        CHECK(status.value() == commitment_status::pending);
    }

    SECTION("returns error for non-existent transaction") {
        auto status = repo.get_status("2.25.nonexistent");
        CHECK(status.is_err());
    }
}

// ============================================================================
// Find by Status
// ============================================================================

TEST_CASE("commitment_repository find_by_status",
          "[storage][commitment][repository]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported by unified_database_system");
        return;
    }
    test_database tdb;
    commitment_repository repo(tdb.get());

    std::vector<sop_reference> refs = {
        {"1.2.840.10008.5.1.4.1.1.2", "1.2.3.4.5.1"},
    };

    // Create two pending requests
    REQUIRE(repo.record_request("2.25.777", "SCU_AE", refs).is_ok());
    REQUIRE(repo.record_request("2.25.778", "SCU_AE", refs).is_ok());

    // Complete one
    commitment_result cr;
    cr.transaction_uid = "2.25.777";
    cr.success_references = refs;
    REQUIRE(repo.update_result("2.25.777", cr).is_ok());

    auto pending = repo.find_by_status(commitment_status::pending);
    REQUIRE(pending.is_ok());
    CHECK(pending.value().size() == 1);
    CHECK(pending.value()[0].transaction_uid == "2.25.778");

    auto success = repo.find_by_status(commitment_status::success);
    REQUIRE(success.is_ok());
    CHECK(success.value().size() == 1);
    CHECK(success.value()[0].transaction_uid == "2.25.777");
}

// ============================================================================
// Status Enum Conversion
// ============================================================================

TEST_CASE("commitment_status string conversion",
          "[storage][commitment]") {
    CHECK(to_string(commitment_status::pending) == "pending");
    CHECK(to_string(commitment_status::success) == "success");
    CHECK(to_string(commitment_status::partial) == "partial");
    CHECK(to_string(commitment_status::failed) == "failed");

    CHECK(commitment_status_from_string("pending") ==
          commitment_status::pending);
    CHECK(commitment_status_from_string("success") ==
          commitment_status::success);
    CHECK(commitment_status_from_string("partial") ==
          commitment_status::partial);
    CHECK(commitment_status_from_string("failed") ==
          commitment_status::failed);
    CHECK(commitment_status_from_string("unknown") ==
          commitment_status::pending);
}

#else  // !PACS_WITH_DATABASE_SYSTEM

TEST_CASE("commitment_repository requires PACS_WITH_DATABASE_SYSTEM",
          "[storage][commitment]") {
    // This test simply verifies compilation without database_system
    CHECK(true);
}

#endif  // PACS_WITH_DATABASE_SYSTEM

}  // anonymous namespace
