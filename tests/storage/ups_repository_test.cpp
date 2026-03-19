/**
 * @file ups_repository_test.cpp
 * @brief Unit tests for ups_repository class
 *
 * Verifies UPS lifecycle, search, and subscription behavior
 * for the extracted ups_repository.
 *
 * @see Issue #915 - Extract UPS lifecycle repository
 */

#include <catch2/catch_test_macros.hpp>

#include <pacs/storage/migration_runner.hpp>
#include <pacs/storage/ups_repository.hpp>

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

auto create_workitem(const std::string& uid,
                     const std::string& label = "Test Procedure",
                     const std::string& priority = "MEDIUM")
    -> ups_workitem {
    ups_workitem item;
    item.workitem_uid = uid;
    item.procedure_step_label = label;
    item.worklist_label = "Radiology";
    item.priority = priority;
    item.scheduled_start_datetime = "20240115090000";
    return item;
}

}  // namespace

#ifdef PACS_WITH_DATABASE_SYSTEM

TEST_CASE("ups_repository construction", "[storage][ups_repository]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported");
        return;
    }

    test_database db;
    ups_repository repo(db.get());
    SUCCEED("Construction succeeded");
}

TEST_CASE("ups_repository lifecycle and search", "[storage][ups_repository]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported");
        return;
    }

    test_database db;
    ups_repository repo(db.get());

    auto create = repo.create_ups_workitem(
        create_workitem("1.2.840.ups.1", "CT Head", "HIGH"));
    REQUIRE(create.is_ok());
    CHECK(create.value() > 0);

    auto found = repo.find_ups_workitem("1.2.840.ups.1");
    REQUIRE(found.has_value());
    CHECK(found->state == "SCHEDULED");
    CHECK(found->priority == "HIGH");

    ups_workitem updated = *found;
    updated.procedure_step_label = "Updated Label";
    updated.priority = "HIGH";
    updated.progress_description = "Preparing";
    updated.progress_percent = 10;
    REQUIRE(repo.update_ups_workitem(updated).is_ok());

    auto claim = repo.change_ups_state("1.2.840.ups.1", "IN PROGRESS",
                                       "1.2.840.txn.1");
    REQUIRE(claim.is_ok());
    auto complete = repo.change_ups_state("1.2.840.ups.1", "COMPLETED");
    REQUIRE(complete.is_ok());

    auto completed = repo.find_ups_workitem("1.2.840.ups.1");
    REQUIRE(completed.has_value());
    CHECK(completed->state == "COMPLETED");

    auto invalid_update = repo.update_ups_workitem(updated);
    REQUIRE(invalid_update.is_err());

    REQUIRE(repo.create_ups_workitem(
                create_workitem("1.2.840.ups.2", "MR Brain", "MEDIUM"))
                .is_ok());

    ups_workitem_query query;
    query.state = "SCHEDULED";
    auto scheduled = repo.search_ups_workitems(query);
    REQUIRE(scheduled.is_ok());
    CHECK(scheduled.value().size() == 1);

    CHECK(repo.ups_workitem_count().value() == 2);
    CHECK(repo.ups_workitem_count("COMPLETED").value() == 1);
}

TEST_CASE("ups_repository subscriptions and delete",
          "[storage][ups_repository]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported");
        return;
    }

    test_database db;
    ups_repository repo(db.get());
    REQUIRE(repo.create_ups_workitem(create_workitem("1.2.840.ups.sub"))
                .is_ok());

    ups_subscription specific;
    specific.subscriber_ae = "SPECIFIC_AE";
    specific.workitem_uid = "1.2.840.ups.sub";
    specific.deletion_lock = true;
    REQUIRE(repo.subscribe_ups(specific).is_ok());

    ups_subscription global;
    global.subscriber_ae = "GLOBAL_AE";
    REQUIRE(repo.subscribe_ups(global).is_ok());

    auto subs = repo.get_ups_subscriptions("SPECIFIC_AE");
    REQUIRE(subs.is_ok());
    REQUIRE(subs.value().size() == 1);
    CHECK(subs.value()[0].workitem_uid == "1.2.840.ups.sub");

    auto subscribers = repo.get_ups_subscribers("1.2.840.ups.sub");
    REQUIRE(subscribers.is_ok());
    CHECK(subscribers.value().size() == 2);

    REQUIRE(repo.unsubscribe_ups("SPECIFIC_AE", "1.2.840.ups.sub").is_ok());
    CHECK(repo.get_ups_subscriptions("SPECIFIC_AE").value().empty());

    REQUIRE(repo.delete_ups_workitem("1.2.840.ups.sub").is_ok());
    CHECK_FALSE(repo.find_ups_workitem("1.2.840.ups.sub").has_value());
}

#else

TEST_CASE("ups_repository construction", "[storage][ups_repository]") {
    test_database db;
    ups_repository repo(db.get());
    SUCCEED("Construction succeeded");
}

TEST_CASE("ups_repository lifecycle", "[storage][ups_repository]") {
    test_database db;
    ups_repository repo(db.get());

    REQUIRE(repo.create_ups_workitem(create_workitem("1.2.840.ups.1")).is_ok());
    REQUIRE(repo.change_ups_state("1.2.840.ups.1", "IN PROGRESS",
                                  "1.2.840.txn.1")
                .is_ok());
    CHECK(repo.ups_workitem_count("IN PROGRESS").value() == 1);
}

#endif
