/**
 * @file worklist_repository_test.cpp
 * @brief Unit tests for worklist_repository class
 *
 * Verifies CRUD operations, query behavior, and cleanup semantics
 * for the extracted worklist_repository.
 *
 * @see Issue #914 - Extract worklist lifecycle repository
 */

#include <catch2/catch_test_macros.hpp>

#include <pacs/storage/migration_runner.hpp>
#include <pacs/storage/worklist_repository.hpp>

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

auto create_item(const std::string& step_id = "SPS001",
                 const std::string& accession = "ACC001")
    -> worklist_item {
    worklist_item item;
    item.step_id = step_id;
    item.patient_id = "PAT001";
    item.patient_name = "Doe^John";
    item.birth_date = "19800115";
    item.sex = "M";
    item.accession_no = accession;
    item.requested_proc_id = "RP001";
    item.study_uid = "1.2.840.worklist.study";
    item.scheduled_datetime = "20240115093000";
    item.station_ae = "CT_SCANNER_1";
    item.station_name = "CT Scanner Room 1";
    item.modality = "CT";
    item.procedure_desc = "CT Chest";
    item.referring_phys = "Smith^Jane^Dr";
    item.referring_phys_id = "DR001";
    return item;
}

}  // namespace

#ifdef PACS_WITH_DATABASE_SYSTEM

TEST_CASE("worklist_repository construction",
          "[storage][worklist_repository]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported");
        return;
    }

    test_database db;
    worklist_repository repo(db.get());
    SUCCEED("Construction succeeded");
}

TEST_CASE("worklist_repository add, query, and update",
          "[storage][worklist_repository]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported");
        return;
    }

    test_database db;
    worklist_repository repo(db.get());

    auto pk = repo.add_worklist_item(create_item());
    REQUIRE(pk.is_ok());
    CHECK(pk.value() > 0);

    auto found = repo.find_worklist_item("SPS001", "ACC001");
    REQUIRE(found.has_value());
    CHECK(found->step_status == "SCHEDULED");

    auto by_pk = repo.find_worklist_by_pk(pk.value());
    REQUIRE(by_pk.has_value());
    CHECK(by_pk->step_id == "SPS001");

    worklist_query query;
    auto scheduled = repo.query_worklist(query);
    REQUIRE(scheduled.is_ok());
    CHECK(scheduled.value().size() == 1);

    REQUIRE(repo.update_worklist_status("SPS001", "ACC001", "STARTED").is_ok());

    auto default_query = repo.query_worklist(query);
    REQUIRE(default_query.is_ok());
    CHECK(default_query.value().empty());

    query.include_all_status = true;
    auto all_items = repo.query_worklist(query);
    REQUIRE(all_items.is_ok());
    CHECK(all_items.value().size() == 1);
    CHECK(all_items.value()[0].step_status == "STARTED");

    CHECK(repo.worklist_count().value() == 1);
    CHECK(repo.worklist_count("STARTED").value() == 1);
}

TEST_CASE("worklist_repository cleanup and delete",
          "[storage][worklist_repository]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported");
        return;
    }

    test_database db;
    worklist_repository repo(db.get());

    auto old_item = create_item("SPS-OLD", "ACC-OLD");
    old_item.scheduled_datetime = "2023-10-15 09:30:00";
    auto future_item = create_item("SPS-FUTURE", "ACC-FUTURE");
    future_item.scheduled_datetime = "2024-12-15 09:30:00";

    REQUIRE(repo.add_worklist_item(old_item).is_ok());
    REQUIRE(repo.add_worklist_item(future_item).is_ok());
    REQUIRE(repo.update_worklist_status("SPS-OLD", "ACC-OLD", "COMPLETED")
                .is_ok());
    REQUIRE(repo.update_worklist_status("SPS-FUTURE", "ACC-FUTURE",
                                        "COMPLETED")
                .is_ok());

    std::tm tm{};
    tm.tm_year = 2024 - 1900;
    tm.tm_mon = 0;
    tm.tm_mday = 1;
    auto cutoff =
        std::chrono::system_clock::from_time_t(std::mktime(&tm));

    auto cleanup_before = repo.cleanup_worklist_items_before(cutoff);
    REQUIRE(cleanup_before.is_ok());
    CHECK(cleanup_before.value() == 1);

    auto future_found = repo.find_worklist_item("SPS-FUTURE", "ACC-FUTURE");
    REQUIRE(future_found.has_value());

    auto cleanup_old = repo.cleanup_old_worklist_items(std::chrono::hours(-1));
    REQUIRE(cleanup_old.is_ok());
    CHECK(cleanup_old.value() == 1);
    CHECK(repo.worklist_count().value() == 0);

    REQUIRE(repo.add_worklist_item(create_item("SPS-DEL", "ACC-DEL")).is_ok());
    REQUIRE(repo.delete_worklist_item("SPS-DEL", "ACC-DEL").is_ok());
    CHECK_FALSE(repo.find_worklist_item("SPS-DEL", "ACC-DEL").has_value());
}

#else

TEST_CASE("worklist_repository construction",
          "[storage][worklist_repository]") {
    test_database db;
    worklist_repository repo(db.get());
    SUCCEED("Construction succeeded");
}

TEST_CASE("worklist_repository add and cleanup",
          "[storage][worklist_repository]") {
    test_database db;
    worklist_repository repo(db.get());

    REQUIRE(repo.add_worklist_item(create_item()).is_ok());
    REQUIRE(repo.update_worklist_status("SPS001", "ACC001", "COMPLETED").is_ok());
    CHECK(repo.worklist_count("COMPLETED").value() == 1);
    CHECK(repo.cleanup_old_worklist_items(std::chrono::hours(-1)).value() == 1);
}

#endif
