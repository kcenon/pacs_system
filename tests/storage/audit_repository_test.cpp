/**
 * @file audit_repository_test.cpp
 * @brief Unit tests for audit_repository class
 *
 * Verifies audit insert/query/count/cleanup behavior
 * for the extracted audit_repository.
 *
 * @see Issue #915 - Extract audit lifecycle repository
 */

#include <catch2/catch_test_macros.hpp>

#include <pacs/storage/audit_repository.hpp>
#include <pacs/storage/migration_runner.hpp>

#ifdef PACS_WITH_DATABASE_SYSTEM
#include <pacs/storage/pacs_database_adapter.hpp>
#else
#include <sqlite3.h>
#endif

using namespace kcenon::pacs::storage;

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

#else

class test_database {
public:
    test_database() {
        auto rc = sqlite3_open(":memory:", &db_);
        if (rc != SQLITE_OK) {
            throw std::runtime_error("Failed to open in-memory database");
        }
        migration_runner runner;
        auto result = runner.run_migrations(db_);
        if (result.is_err()) {
            throw std::runtime_error(
                "Migration failed: " + result.error().message);
        }
    }

    ~test_database() {
        if (db_ != nullptr) {
            sqlite3_close(db_);
        }
    }

    test_database(const test_database&) = delete;
    auto operator=(const test_database&) -> test_database& = delete;

    [[nodiscard]] auto get() const noexcept -> sqlite3* { return db_; }

private:
    sqlite3* db_ = nullptr;
};

#endif

auto create_record(const std::string& event_type = "C_STORE",
                   const std::string& user_id = "user-a")
    -> audit_record {
    audit_record record;
    record.event_type = event_type;
    record.outcome = "SUCCESS";
    record.user_id = user_id;
    record.source_ae = "SCU_AE";
    record.target_ae = "SCP_AE";
    record.patient_id = "PAT001";
    record.study_uid = "1.2.840.audit.study";
    record.message = "Stored object";
    record.details = R"({"status":"ok"})";
    return record;
}

}  // namespace

#ifdef PACS_WITH_DATABASE_SYSTEM

TEST_CASE("audit_repository construction", "[storage][audit_repository]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported");
        return;
    }

    test_database db;
    audit_repository repo(db.get());
    SUCCEED("Construction succeeded");
}

TEST_CASE("audit_repository add, query, and count",
          "[storage][audit_repository]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported");
        return;
    }

    test_database db;
    audit_repository repo(db.get());

    auto first = repo.add_audit_log(create_record("C_STORE", "user-a"));
    REQUIRE(first.is_ok());
    CHECK(first.value() > 0);

    auto second = repo.add_audit_log(create_record("C_FIND", "user-b"));
    REQUIRE(second.is_ok());

    auto by_pk = repo.find_audit_by_pk(first.value());
    REQUIRE(by_pk.has_value());
    CHECK(by_pk->event_type == "C_STORE");

    audit_query query;
    query.event_type = "C_FIND";
    auto found = repo.query_audit_log(query);
    REQUIRE(found.is_ok());
    CHECK(found.value().size() == 1);

    audit_query wildcard_query;
    wildcard_query.user_id = "user-*";
    auto wildcard = repo.query_audit_log(wildcard_query);
    REQUIRE(wildcard.is_ok());
    CHECK(wildcard.value().size() == 2);

    CHECK(repo.audit_count().value() == 2);
}

TEST_CASE("audit_repository cleanup", "[storage][audit_repository]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported");
        return;
    }

    test_database db;
    audit_repository repo(db.get());

    REQUIRE(repo.add_audit_log(create_record()).is_ok());
    CHECK(repo.audit_count().value() == 1);

    auto cleanup = repo.cleanup_old_audit_logs(std::chrono::hours(-1));
    REQUIRE(cleanup.is_ok());
    CHECK(cleanup.value() == 1);
    CHECK(repo.audit_count().value() == 0);
}

#else

TEST_CASE("audit_repository construction", "[storage][audit_repository]") {
    test_database db;
    audit_repository repo(db.get());
    SUCCEED("Construction succeeded");
}

TEST_CASE("audit_repository add and cleanup", "[storage][audit_repository]") {
    test_database db;
    audit_repository repo(db.get());

    REQUIRE(repo.add_audit_log(create_record()).is_ok());
    CHECK(repo.audit_count().value() == 1);
    CHECK(repo.cleanup_old_audit_logs(std::chrono::hours(-1)).value() == 1);
}

#endif
