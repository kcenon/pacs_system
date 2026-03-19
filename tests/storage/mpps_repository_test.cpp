/**
 * @file mpps_repository_test.cpp
 * @brief Unit tests for mpps_repository class
 *
 * Verifies CRUD operations, search, counts, and MPPS state validation
 * for the extracted mpps_repository.
 *
 * @see Issue #914 - Extract MPPS lifecycle repository
 */

#include <catch2/catch_test_macros.hpp>

#include <pacs/storage/migration_runner.hpp>
#include <pacs/storage/mpps_repository.hpp>

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

auto create_test_mpps(auto& repo, const std::string& uid,
                      const std::string& station = "CT_SCANNER_1",
                      const std::string& modality = "CT",
                      const std::string& study_uid = "1.2.840.mpps.study")
    -> int64_t {
    auto result =
        repo.create_mpps(uid, station, modality, study_uid, "ACC001",
                         "20240115120000");
    if (result.is_err()) {
        throw std::runtime_error("Failed to create test MPPS");
    }
    return result.value();
}

}  // namespace

#ifdef PACS_WITH_DATABASE_SYSTEM

TEST_CASE("mpps_repository construction", "[storage][mpps_repository]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported");
        return;
    }

    test_database db;
    mpps_repository repo(db.get());
    SUCCEED("Construction succeeded");
}

TEST_CASE("mpps_repository create, find, and update",
          "[storage][mpps_repository]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported");
        return;
    }

    test_database db;
    mpps_repository repo(db.get());

    SECTION("create and find") {
        auto pk =
            repo.create_mpps("1.2.840.mpps.1", "CT_SCANNER_1", "CT",
                             "1.2.840.study.1", "ACC001", "20240115120000");
        REQUIRE(pk.is_ok());
        CHECK(pk.value() > 0);

        auto found = repo.find_mpps("1.2.840.mpps.1");
        REQUIRE(found.has_value());
        CHECK(found->status == "IN PROGRESS");
        CHECK(found->station_ae == "CT_SCANNER_1");

        auto by_pk = repo.find_mpps_by_pk(pk.value());
        REQUIRE(by_pk.has_value());
        CHECK(by_pk->mpps_uid == "1.2.840.mpps.1");
    }

    SECTION("update final state and reject further changes") {
        create_test_mpps(repo, "1.2.840.mpps.2");

        auto update = repo.update_mpps(
            "1.2.840.mpps.2", "COMPLETED", "20240115123000",
            R"([{"series_uid":"1.2.840.series.1","num_instances":4}])");
        REQUIRE(update.is_ok());

        auto found = repo.find_mpps("1.2.840.mpps.2");
        REQUIRE(found.has_value());
        CHECK(found->status == "COMPLETED");
        CHECK(found->end_datetime == "20240115123000");

        auto second_update = repo.update_mpps("1.2.840.mpps.2", "IN PROGRESS");
        REQUIRE(second_update.is_err());
    }
}

TEST_CASE("mpps_repository search and count", "[storage][mpps_repository]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported");
        return;
    }

    test_database db;
    mpps_repository repo(db.get());

    create_test_mpps(repo, "1.2.840.mpps.10", "CT_SCANNER_1", "CT",
                     "1.2.840.study.shared");
    create_test_mpps(repo, "1.2.840.mpps.11", "CT_SCANNER_1", "CT",
                     "1.2.840.study.shared");
    create_test_mpps(repo, "1.2.840.mpps.12", "MR_SCANNER_1", "MR",
                     "1.2.840.study.other");
    REQUIRE(repo.update_mpps("1.2.840.mpps.10", "COMPLETED",
                             "20240115123000")
                .is_ok());

    auto active = repo.list_active_mpps("CT_SCANNER_1");
    REQUIRE(active.is_ok());
    CHECK(active.value().size() == 1);

    auto by_study = repo.find_mpps_by_study("1.2.840.study.shared");
    REQUIRE(by_study.is_ok());
    CHECK(by_study.value().size() == 2);

    mpps_query query;
    query.modality = "CT";
    query.status = "IN PROGRESS";
    auto search = repo.search_mpps(query);
    REQUIRE(search.is_ok());
    CHECK(search.value().size() == 1);

    CHECK(repo.mpps_count().value() == 3);
    CHECK(repo.mpps_count("COMPLETED").value() == 1);
}

TEST_CASE("mpps_repository delete", "[storage][mpps_repository]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported");
        return;
    }

    test_database db;
    mpps_repository repo(db.get());

    create_test_mpps(repo, "1.2.840.mpps.del");
    CHECK(repo.mpps_count().value() == 1);

    REQUIRE(repo.delete_mpps("1.2.840.mpps.del").is_ok());
    CHECK(repo.mpps_count().value() == 0);
    CHECK_FALSE(repo.find_mpps("1.2.840.mpps.del").has_value());
}

#else

TEST_CASE("mpps_repository construction", "[storage][mpps_repository]") {
    test_database db;
    mpps_repository repo(db.get());
    SUCCEED("Construction succeeded");
}

TEST_CASE("mpps_repository create and search", "[storage][mpps_repository]") {
    test_database db;
    mpps_repository repo(db.get());

    auto pk = repo.create_mpps("1.2.840.mpps.1", "CT_SCANNER_1", "CT");
    REQUIRE(pk.is_ok());

    auto found = repo.find_mpps("1.2.840.mpps.1");
    REQUIRE(found.has_value());
    CHECK(found->status == "IN PROGRESS");

    REQUIRE(repo.update_mpps("1.2.840.mpps.1", "COMPLETED").is_ok());
    CHECK(repo.mpps_count("COMPLETED").value() == 1);
}

#endif
