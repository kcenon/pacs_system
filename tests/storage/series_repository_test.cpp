/**
 * @file series_repository_test.cpp
 * @brief Unit tests for series_repository class
 *
 * Verifies CRUD operations, search, and study-scoped counts
 * for the extracted series_repository.
 *
 * @see Issue #913 - Extract series and instance metadata repositories
 */

#include <catch2/catch_test_macros.hpp>

#include <pacs/storage/migration_runner.hpp>
#include <pacs/storage/patient_repository.hpp>
#include <pacs/storage/series_repository.hpp>
#include <pacs/storage/study_repository.hpp>

#ifdef PACS_WITH_DATABASE_SYSTEM
#include <pacs/storage/pacs_database_adapter.hpp>
#else
#include <sqlite3.h>
#endif

using namespace pacs::storage;

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

auto insert_test_patient(auto& repo, const std::string& id = "SER-PAT-001")
    -> int64_t {
    auto result = repo.upsert_patient(id, "Series^Patient", "19800101", "M");
    if (result.is_err()) {
        throw std::runtime_error("Failed to insert test patient");
    }
    return result.value();
}

auto insert_test_study(auto& repo, int64_t patient_pk,
                       const std::string& uid = "1.2.840.series.study")
    -> int64_t {
    auto result = repo.upsert_study(patient_pk, uid, "SERIES-STUDY");
    if (result.is_err()) {
        throw std::runtime_error("Failed to insert test study");
    }
    return result.value();
}

}  // namespace

#ifdef PACS_WITH_DATABASE_SYSTEM

TEST_CASE("series_repository construction", "[storage][series]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported");
        return;
    }

    test_database db;
    series_repository repo(db.get());
    SUCCEED("Construction succeeded");
}

TEST_CASE("series_repository upsert and find", "[storage][series]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported");
        return;
    }

    test_database db;
    patient_repository patient_repo(db.get());
    study_repository study_repo(db.get());
    series_repository repo(db.get());
    auto patient_pk = insert_test_patient(patient_repo);
    auto study_pk = insert_test_study(study_repo, patient_pk);

    SECTION("upsert new series returns pk") {
        auto result = repo.upsert_series(study_pk, "1.2.840.series.1", "CT", 1,
                                         "Head CT", "HEAD", "SCN-A");
        REQUIRE(result.is_ok());
        CHECK(result.value() > 0);
    }

    SECTION("find by uid and pk") {
        auto inserted =
            repo.upsert_series(study_pk, "1.2.840.series.2", "MR", 2);
        REQUIRE(inserted.is_ok());

        auto found = repo.find_series("1.2.840.series.2");
        REQUIRE(found.has_value());
        CHECK(found->modality == "MR");
        CHECK(found->study_pk == study_pk);

        auto by_pk = repo.find_series_by_pk(inserted.value());
        REQUIRE(by_pk.has_value());
        CHECK(by_pk->series_uid == "1.2.840.series.2");
    }

    SECTION("upsert updates existing row") {
        auto first = repo.upsert_series(study_pk, "1.2.840.series.3", "CT", 3,
                                        "Old", "CHEST", "SCN-B");
        REQUIRE(first.is_ok());

        auto second = repo.upsert_series(study_pk, "1.2.840.series.3", "CT", 3,
                                         "Updated", "CHEST", "SCN-B");
        REQUIRE(second.is_ok());
        CHECK(first.value() == second.value());

        auto found = repo.find_series("1.2.840.series.3");
        REQUIRE(found.has_value());
        CHECK(found->series_description == "Updated");
    }
}

TEST_CASE("series_repository search and count", "[storage][series]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported");
        return;
    }

    test_database db;
    patient_repository patient_repo(db.get());
    study_repository study_repo(db.get());
    series_repository repo(db.get());
    auto patient_pk = insert_test_patient(patient_repo);
    (void)insert_test_study(study_repo, patient_pk, "1.2.840.series.study.a");
    auto study_b = insert_test_study(study_repo, patient_pk, "1.2.840.series.study.b");

    REQUIRE(repo.upsert_series(study_b, "1.2.840.series.10", "CT", 1, "Head CT",
                               "HEAD", "SCN-A")
                .is_ok());
    REQUIRE(repo.upsert_series(study_b, "1.2.840.series.11", "CT", 2, "Chest CT",
                               "CHEST", "SCN-A")
                .is_ok());

    series_query query;
    query.study_uid = "1.2.840.series.study.b";
    query.modality = "CT";
    auto results = repo.search_series(query);
    REQUIRE(results.is_ok());
    CHECK(results.value().size() == 2);

    CHECK(repo.list_series("1.2.840.series.study.b").value().size() == 2);
    CHECK(repo.series_count().value() == 2);
    CHECK(repo.series_count("1.2.840.series.study.b").value() == 2);
    CHECK(repo.series_count("1.2.840.series.study.a").value() == 0);
}

TEST_CASE("series_repository delete", "[storage][series]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported");
        return;
    }

    test_database db;
    patient_repository patient_repo(db.get());
    study_repository study_repo(db.get());
    series_repository repo(db.get());
    auto patient_pk = insert_test_patient(patient_repo);
    auto study_pk = insert_test_study(study_repo, patient_pk);

    REQUIRE(repo.upsert_series(study_pk, "1.2.840.series.del", "CT").is_ok());
    CHECK(repo.series_count().value() == 1);

    REQUIRE(repo.delete_series("1.2.840.series.del").is_ok());
    CHECK(repo.series_count().value() == 0);
    CHECK_FALSE(repo.find_series("1.2.840.series.del").has_value());
}

#else

TEST_CASE("series_repository construction", "[storage][series]") {
    test_database db;
    series_repository repo(db.get());
    SUCCEED("Construction succeeded");
}

TEST_CASE("series_repository upsert and find", "[storage][series]") {
    test_database db;
    patient_repository patient_repo(db.get());
    study_repository study_repo(db.get());
    series_repository repo(db.get());
    auto patient_pk = insert_test_patient(patient_repo);
    auto study_pk = insert_test_study(study_repo, patient_pk);

    auto inserted =
        repo.upsert_series(study_pk, "1.2.840.series.2", "MR", 2);
    REQUIRE(inserted.is_ok());

    auto found = repo.find_series("1.2.840.series.2");
    REQUIRE(found.has_value());
    CHECK(found->modality == "MR");
}

TEST_CASE("series_repository search and delete", "[storage][series]") {
    test_database db;
    patient_repository patient_repo(db.get());
    study_repository study_repo(db.get());
    series_repository repo(db.get());
    auto patient_pk = insert_test_patient(patient_repo);
    auto study_pk = insert_test_study(study_repo, patient_pk);

    REQUIRE(repo.upsert_series(study_pk, "1.2.840.series.10", "CT").is_ok());
    REQUIRE(repo.upsert_series(study_pk, "1.2.840.series.11", "MR").is_ok());

    series_query query;
    query.modality = "CT";
    auto results = repo.search_series(query);
    REQUIRE(results.is_ok());
    CHECK(results.value().size() == 1);

    REQUIRE(repo.delete_series("1.2.840.series.10").is_ok());
    CHECK(repo.series_count().value() == 1);
}

#endif
