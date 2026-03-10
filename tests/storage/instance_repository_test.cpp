/**
 * @file instance_repository_test.cpp
 * @brief Unit tests for instance_repository class
 *
 * Verifies CRUD operations, search, and file path lookups
 * for the extracted instance_repository.
 *
 * @see Issue #913 - Extract series and instance metadata repositories
 */

#include <catch2/catch_test_macros.hpp>

#include <pacs/storage/instance_repository.hpp>
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

auto insert_test_series(auto& patient_repo, auto& study_repo, auto& series_repo,
                        const std::string& patient_id = "INS-PAT-001",
                        const std::string& study_uid = "1.2.840.instance.study",
                        const std::string& series_uid = "1.2.840.instance.series")
    -> int64_t {
    auto patient_pk =
        patient_repo.upsert_patient(patient_id, "Instance^Patient", "19800101", "M");
    if (patient_pk.is_err()) {
        throw std::runtime_error("Failed to insert patient");
    }

    auto study_pk = study_repo.upsert_study(patient_pk.value(), study_uid, "INST-STUDY");
    if (study_pk.is_err()) {
        throw std::runtime_error("Failed to insert study");
    }

    auto series_pk = series_repo.upsert_series(study_pk.value(), series_uid, "CT", 1);
    if (series_pk.is_err()) {
        throw std::runtime_error("Failed to insert series");
    }

    return series_pk.value();
}

}  // namespace

#ifdef PACS_WITH_DATABASE_SYSTEM

TEST_CASE("instance_repository construction", "[storage][instance]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported");
        return;
    }

    test_database db;
    instance_repository repo(db.get());
    SUCCEED("Construction succeeded");
}

TEST_CASE("instance_repository upsert and find", "[storage][instance]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported");
        return;
    }

    test_database db;
    patient_repository patient_repo(db.get());
    study_repository study_repo(db.get());
    series_repository series_repo(db.get());
    instance_repository repo(db.get());
    auto series_pk =
        insert_test_series(patient_repo, study_repo, series_repo);

    SECTION("upsert new instance returns pk") {
        auto result =
            repo.upsert_instance(series_pk, "1.2.840.instance.1",
                                 "1.2.840.10008.5.1.4.1.1.2",
                                 "/tmp/instance-1.dcm", 1024, "", 1);
        REQUIRE(result.is_ok());
        CHECK(result.value() > 0);
    }

    SECTION("find by uid and pk") {
        auto inserted =
            repo.upsert_instance(series_pk, "1.2.840.instance.2",
                                 "1.2.840.10008.5.1.4.1.1.2",
                                 "/tmp/instance-2.dcm", 2048, "", 2);
        REQUIRE(inserted.is_ok());

        auto found = repo.find_instance("1.2.840.instance.2");
        REQUIRE(found.has_value());
        CHECK(found->file_path == "/tmp/instance-2.dcm");
        CHECK(found->series_pk == series_pk);

        auto by_pk = repo.find_instance_by_pk(inserted.value());
        REQUIRE(by_pk.has_value());
        CHECK(by_pk->sop_uid == "1.2.840.instance.2");
    }

    SECTION("upsert updates existing row") {
        auto first =
            repo.upsert_instance(series_pk, "1.2.840.instance.3",
                                 "1.2.840.10008.5.1.4.1.1.2",
                                 "/tmp/instance-3.dcm", 4096, "", 3);
        REQUIRE(first.is_ok());

        auto second =
            repo.upsert_instance(series_pk, "1.2.840.instance.3",
                                 "1.2.840.10008.5.1.4.1.1.2",
                                 "/tmp/instance-3-updated.dcm", 8192, "", 3);
        REQUIRE(second.is_ok());
        CHECK(first.value() == second.value());

        auto found = repo.find_instance("1.2.840.instance.3");
        REQUIRE(found.has_value());
        CHECK(found->file_path == "/tmp/instance-3-updated.dcm");
        CHECK(found->file_size == 8192);
    }
}

TEST_CASE("instance_repository search and file lookups", "[storage][instance]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported");
        return;
    }

    test_database db;
    patient_repository patient_repo(db.get());
    study_repository study_repo(db.get());
    series_repository series_repo(db.get());
    instance_repository repo(db.get());

    auto patient_pk =
        patient_repo.upsert_patient("INS-PAT-002", "Instance^Patient", "19800101", "M");
    REQUIRE(patient_pk.is_ok());
    auto study_pk =
        study_repo.upsert_study(patient_pk.value(), "1.2.840.instance.study.2", "STUDY-2");
    REQUIRE(study_pk.is_ok());
    auto series_a = series_repo.upsert_series(study_pk.value(), "1.2.840.instance.series.2", "CT", 1);
    REQUIRE(series_a.is_ok());
    auto series_b = series_repo.upsert_series(study_pk.value(), "1.2.840.instance.series.3", "MR", 2);
    REQUIRE(series_b.is_ok());

    REQUIRE(repo.upsert_instance(series_a.value(), "1.2.840.instance.10",
                                 "1.2.840.10008.5.1.4.1.1.2",
                                 "/tmp/study-file-1.dcm", 100, "", 1)
                .is_ok());
    REQUIRE(repo.upsert_instance(series_a.value(), "1.2.840.instance.11",
                                 "1.2.840.10008.5.1.4.1.1.2",
                                 "/tmp/study-file-2.dcm", 200, "", 2)
                .is_ok());
    REQUIRE(repo.upsert_instance(series_b.value(), "1.2.840.instance.12",
                                 "1.2.840.10008.5.1.4.1.1.4",
                                 "/tmp/study-file-3.dcm", 300, "", 1)
                .is_ok());

    instance_query query;
    query.series_uid = "1.2.840.instance.series.2";
    auto results = repo.search_instances(query);
    REQUIRE(results.is_ok());
    CHECK(results.value().size() == 2);

    CHECK(repo.list_instances("1.2.840.instance.series.2").value().size() == 2);
    CHECK(repo.instance_count().value() == 3);
    CHECK(repo.instance_count("1.2.840.instance.series.2").value() == 2);

    auto file_path = repo.get_file_path("1.2.840.instance.10");
    REQUIRE(file_path.is_ok());
    REQUIRE(file_path.value().has_value());
    CHECK(file_path.value().value() == "/tmp/study-file-1.dcm");

    auto study_files = repo.get_study_files("1.2.840.instance.study.2");
    REQUIRE(study_files.is_ok());
    CHECK(study_files.value().size() == 3);

    auto series_files = repo.get_series_files("1.2.840.instance.series.2");
    REQUIRE(series_files.is_ok());
    CHECK(series_files.value().size() == 2);
}

TEST_CASE("instance_repository delete", "[storage][instance]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported");
        return;
    }

    test_database db;
    patient_repository patient_repo(db.get());
    study_repository study_repo(db.get());
    series_repository series_repo(db.get());
    instance_repository repo(db.get());
    auto series_pk =
        insert_test_series(patient_repo, study_repo, series_repo, "INS-PAT-003");

    REQUIRE(repo.upsert_instance(series_pk, "1.2.840.instance.del",
                                 "1.2.840.10008.5.1.4.1.1.2",
                                 "/tmp/delete.dcm", 100)
                .is_ok());
    CHECK(repo.instance_count().value() == 1);

    REQUIRE(repo.delete_instance("1.2.840.instance.del").is_ok());
    CHECK(repo.instance_count().value() == 0);
    CHECK_FALSE(repo.find_instance("1.2.840.instance.del").has_value());
}

TEST_CASE("repository cascade delete across boundaries",
          "[storage][instance][cascade]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported");
        return;
    }

    test_database db;
    patient_repository patient_repo(db.get());
    study_repository study_repo(db.get());
    series_repository series_repo(db.get());
    instance_repository repo(db.get());

    auto patient_pk =
        patient_repo.upsert_patient("CASCADE-PAT", "Cascade^Patient",
                                    "19800101", "M");
    REQUIRE(patient_pk.is_ok());
    auto study_pk =
        study_repo.upsert_study(patient_pk.value(), "1.2.840.cascade.study",
                                "CASCADE-STUDY");
    REQUIRE(study_pk.is_ok());
    auto series_pk =
        series_repo.upsert_series(study_pk.value(), "1.2.840.cascade.series",
                                  "CT", 1);
    REQUIRE(series_pk.is_ok());
    REQUIRE(repo.upsert_instance(series_pk.value(), "1.2.840.cascade.instance",
                                 "1.2.840.10008.5.1.4.1.1.2",
                                 "/tmp/cascade-instance.dcm", 1000)
                .is_ok());

    CHECK(repo.instance_count().value() == 1);
    REQUIRE(patient_repo.delete_patient("CASCADE-PAT").is_ok());
    CHECK(repo.instance_count().value() == 0);
    CHECK_FALSE(repo.find_instance("1.2.840.cascade.instance").has_value());
}

#else

TEST_CASE("instance_repository construction", "[storage][instance]") {
    test_database db;
    instance_repository repo(db.get());
    SUCCEED("Construction succeeded");
}

TEST_CASE("instance_repository upsert and find", "[storage][instance]") {
    test_database db;
    patient_repository patient_repo(db.get());
    study_repository study_repo(db.get());
    series_repository series_repo(db.get());
    instance_repository repo(db.get());
    auto series_pk =
        insert_test_series(patient_repo, study_repo, series_repo);

    auto inserted =
        repo.upsert_instance(series_pk, "1.2.840.instance.2",
                             "1.2.840.10008.5.1.4.1.1.2",
                             "/tmp/instance-2.dcm", 2048, "", 2);
    REQUIRE(inserted.is_ok());

    auto found = repo.find_instance("1.2.840.instance.2");
    REQUIRE(found.has_value());
    CHECK(found->file_path == "/tmp/instance-2.dcm");
}

TEST_CASE("instance_repository file lookups and delete", "[storage][instance]") {
    test_database db;
    patient_repository patient_repo(db.get());
    study_repository study_repo(db.get());
    series_repository series_repo(db.get());
    instance_repository repo(db.get());
    auto series_pk =
        insert_test_series(patient_repo, study_repo, series_repo, "INS-PAT-004");

    REQUIRE(repo.upsert_instance(series_pk, "1.2.840.instance.del",
                                 "1.2.840.10008.5.1.4.1.1.2",
                                 "/tmp/delete.dcm", 100)
                .is_ok());

    auto file_path = repo.get_file_path("1.2.840.instance.del");
    REQUIRE(file_path.is_ok());
    REQUIRE(file_path.value().has_value());
    CHECK(file_path.value().value() == "/tmp/delete.dcm");

    REQUIRE(repo.delete_instance("1.2.840.instance.del").is_ok());
    CHECK(repo.instance_count().value() == 0);
}

TEST_CASE("repository cascade delete across boundaries",
          "[storage][instance][cascade]") {
    test_database db;
    patient_repository patient_repo(db.get());
    study_repository study_repo(db.get());
    series_repository series_repo(db.get());
    instance_repository repo(db.get());

    auto patient_pk =
        patient_repo.upsert_patient("CASCADE-PAT", "Cascade^Patient",
                                    "19800101", "M");
    REQUIRE(patient_pk.is_ok());
    auto study_pk =
        study_repo.upsert_study(patient_pk.value(), "1.2.840.cascade.study",
                                "CASCADE-STUDY");
    REQUIRE(study_pk.is_ok());
    auto series_pk =
        series_repo.upsert_series(study_pk.value(), "1.2.840.cascade.series",
                                  "CT", 1);
    REQUIRE(series_pk.is_ok());
    REQUIRE(repo.upsert_instance(series_pk.value(), "1.2.840.cascade.instance",
                                 "1.2.840.10008.5.1.4.1.1.2",
                                 "/tmp/cascade-instance.dcm", 1000)
                .is_ok());

    REQUIRE(patient_repo.delete_patient("CASCADE-PAT").is_ok());
    CHECK(repo.instance_count().value() == 0);
}

#endif
