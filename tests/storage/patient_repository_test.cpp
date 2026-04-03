/**
 * @file patient_repository_test.cpp
 * @brief Unit tests for patient_repository class
 *
 * Verifies CRUD operations, wildcard search, and upsert semantics
 * for the extracted patient_repository.
 *
 * @see Issue #912 - Extract patient and study metadata repositories
 */

#include <catch2/catch_test_macros.hpp>

#include <kcenon/pacs/storage/patient_repository.h>
#include <kcenon/pacs/storage/migration_runner.h>

#ifdef PACS_WITH_DATABASE_SYSTEM
#include <kcenon/pacs/storage/pacs_database_adapter.h>
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

#else  // !PACS_WITH_DATABASE_SYSTEM

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

#endif  // PACS_WITH_DATABASE_SYSTEM

}  // namespace

// ============================================================================
// Construction
// ============================================================================

#ifdef PACS_WITH_DATABASE_SYSTEM

TEST_CASE("patient_repository construction", "[storage][patient]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported");
        return;
    }

    test_database db;
    patient_repository repo(db.get());
    SUCCEED("Construction succeeded");
}

#else

TEST_CASE("patient_repository construction", "[storage][patient]") {
    test_database db;
    patient_repository repo(db.get());
    SUCCEED("Construction succeeded");
}

#endif

// ============================================================================
// Upsert and Find
// ============================================================================

#ifdef PACS_WITH_DATABASE_SYSTEM

TEST_CASE("patient_repository upsert and find", "[storage][patient]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported");
        return;
    }

    test_database db;
    patient_repository repo(db.get());

    SECTION("upsert new patient returns pk") {
        auto result = repo.upsert_patient("PAT001", "Doe^John", "19800101", "M");
        REQUIRE(result.is_ok());
        CHECK(result.value() > 0);
    }

    SECTION("find by patient_id") {
        auto pk = repo.upsert_patient("PAT002", "Smith^Jane", "19900215", "F");
        REQUIRE(pk.is_ok());

        auto found = repo.find_patient("PAT002");
        REQUIRE(found.has_value());
        CHECK(found->patient_id == "PAT002");
        CHECK(found->patient_name == "Smith^Jane");
        CHECK(found->birth_date == "19900215");
        CHECK(found->sex == "F");
    }

    SECTION("find by pk") {
        auto pk = repo.upsert_patient("PAT003", "Kim^Min", "19750610", "M");
        REQUIRE(pk.is_ok());

        auto found = repo.find_patient_by_pk(pk.value());
        REQUIRE(found.has_value());
        CHECK(found->patient_id == "PAT003");
        CHECK(found->pk == pk.value());
    }

    SECTION("find non-existent returns nullopt") {
        auto found = repo.find_patient("NON_EXISTENT");
        CHECK_FALSE(found.has_value());
    }

    SECTION("upsert updates existing patient") {
        auto pk1 = repo.upsert_patient("PAT004", "Lee^Old", "19850101", "M");
        REQUIRE(pk1.is_ok());

        auto pk2 = repo.upsert_patient("PAT004", "Lee^Updated", "19850101", "M");
        REQUIRE(pk2.is_ok());
        CHECK(pk1.value() == pk2.value());

        auto found = repo.find_patient("PAT004");
        REQUIRE(found.has_value());
        CHECK(found->patient_name == "Lee^Updated");
    }

    SECTION("upsert with record struct") {
        patient_record record;
        record.patient_id = "PAT005";
        record.patient_name = "Park^Sun";
        record.birth_date = "20000305";
        record.sex = "F";
        record.ethnic_group = "Asian";
        record.comments = "Test patient";

        auto result = repo.upsert_patient(record);
        REQUIRE(result.is_ok());

        auto found = repo.find_patient("PAT005");
        REQUIRE(found.has_value());
        CHECK(found->patient_name == "Park^Sun");
        CHECK(found->ethnic_group == "Asian");
        CHECK(found->comments == "Test patient");
    }
}

#else  // !PACS_WITH_DATABASE_SYSTEM

TEST_CASE("patient_repository upsert and find", "[storage][patient]") {
    test_database db;
    patient_repository repo(db.get());

    SECTION("upsert new patient returns pk") {
        auto result = repo.upsert_patient("PAT001", "Doe^John", "19800101", "M");
        REQUIRE(result.is_ok());
        CHECK(result.value() > 0);
    }

    SECTION("find by patient_id") {
        auto pk = repo.upsert_patient("PAT002", "Smith^Jane", "19900215", "F");
        REQUIRE(pk.is_ok());

        auto found = repo.find_patient("PAT002");
        REQUIRE(found.has_value());
        CHECK(found->patient_id == "PAT002");
        CHECK(found->patient_name == "Smith^Jane");
    }

    SECTION("find non-existent returns nullopt") {
        auto found = repo.find_patient("NON_EXISTENT");
        CHECK_FALSE(found.has_value());
    }

    SECTION("upsert updates existing patient") {
        auto pk1 = repo.upsert_patient("PAT004", "Lee^Old", "19850101", "M");
        REQUIRE(pk1.is_ok());

        auto pk2 = repo.upsert_patient("PAT004", "Lee^Updated", "19850101", "M");
        REQUIRE(pk2.is_ok());
        CHECK(pk1.value() == pk2.value());

        auto found = repo.find_patient("PAT004");
        REQUIRE(found.has_value());
        CHECK(found->patient_name == "Lee^Updated");
    }
}

#endif  // PACS_WITH_DATABASE_SYSTEM

// ============================================================================
// Search
// ============================================================================

#ifdef PACS_WITH_DATABASE_SYSTEM

TEST_CASE("patient_repository search", "[storage][patient]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported");
        return;
    }

    test_database db;
    patient_repository repo(db.get());

    (void)repo.upsert_patient("PAT-A1", "Doe^John", "19800101", "M");
    (void)repo.upsert_patient("PAT-A2", "Doe^Jane", "19820315", "F");
    (void)repo.upsert_patient("PAT-B1", "Smith^Bob", "19750620", "M");

    SECTION("search by patient_name wildcard") {
        patient_query query;
        query.patient_name = "Doe*";
        auto results = repo.search_patients(query);
        REQUIRE(results.is_ok());
        CHECK(results.value().size() == 2);
    }

    SECTION("search by sex filter") {
        patient_query query;
        query.sex = "M";
        auto results = repo.search_patients(query);
        REQUIRE(results.is_ok());
        CHECK(results.value().size() == 2);
    }

    SECTION("search by patient_id wildcard") {
        patient_query query;
        query.patient_id = "PAT-A*";
        auto results = repo.search_patients(query);
        REQUIRE(results.is_ok());
        CHECK(results.value().size() == 2);
    }

    SECTION("search with limit") {
        patient_query query;
        query.limit = 1;
        auto results = repo.search_patients(query);
        REQUIRE(results.is_ok());
        CHECK(results.value().size() == 1);
    }

    SECTION("search with no matches") {
        patient_query query;
        query.patient_name = "Nonexistent*";
        auto results = repo.search_patients(query);
        REQUIRE(results.is_ok());
        CHECK(results.value().empty());
    }
}

#else  // !PACS_WITH_DATABASE_SYSTEM

TEST_CASE("patient_repository search", "[storage][patient]") {
    test_database db;
    patient_repository repo(db.get());

    (void)repo.upsert_patient("PAT-A1", "Doe^John", "19800101", "M");
    (void)repo.upsert_patient("PAT-A2", "Doe^Jane", "19820315", "F");
    (void)repo.upsert_patient("PAT-B1", "Smith^Bob", "19750620", "M");

    SECTION("search by patient_name wildcard") {
        patient_query query;
        query.patient_name = "Doe*";
        auto results = repo.search_patients(query);
        REQUIRE(results.is_ok());
        CHECK(results.value().size() == 2);
    }

    SECTION("search by sex filter") {
        patient_query query;
        query.sex = "M";
        auto results = repo.search_patients(query);
        REQUIRE(results.is_ok());
        CHECK(results.value().size() == 2);
    }
}

#endif  // PACS_WITH_DATABASE_SYSTEM

// ============================================================================
// Delete and Count
// ============================================================================

#ifdef PACS_WITH_DATABASE_SYSTEM

TEST_CASE("patient_repository delete and count", "[storage][patient]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported");
        return;
    }

    test_database db;
    patient_repository repo(db.get());

    (void)repo.upsert_patient("DEL-PAT1", "Delete^Me", "19900101", "M");
    (void)repo.upsert_patient("DEL-PAT2", "Keep^Me", "19900202", "F");

    SECTION("count returns correct total") {
        auto count = repo.patient_count();
        REQUIRE(count.is_ok());
        CHECK(count.value() == 2);
    }

    SECTION("delete removes patient") {
        auto result = repo.delete_patient("DEL-PAT1");
        REQUIRE(result.is_ok());

        auto found = repo.find_patient("DEL-PAT1");
        CHECK_FALSE(found.has_value());

        auto count = repo.patient_count();
        REQUIRE(count.is_ok());
        CHECK(count.value() == 1);
    }
}

#else  // !PACS_WITH_DATABASE_SYSTEM

TEST_CASE("patient_repository delete and count", "[storage][patient]") {
    test_database db;
    patient_repository repo(db.get());

    (void)repo.upsert_patient("DEL-PAT1", "Delete^Me", "19900101", "M");
    (void)repo.upsert_patient("DEL-PAT2", "Keep^Me", "19900202", "F");

    SECTION("count returns correct total") {
        auto count = repo.patient_count();
        REQUIRE(count.is_ok());
        CHECK(count.value() == 2);
    }

    SECTION("delete removes patient") {
        auto result = repo.delete_patient("DEL-PAT1");
        REQUIRE(result.is_ok());

        auto found = repo.find_patient("DEL-PAT1");
        CHECK_FALSE(found.has_value());

        auto count = repo.patient_count();
        REQUIRE(count.is_ok());
        CHECK(count.value() == 1);
    }
}

#endif  // PACS_WITH_DATABASE_SYSTEM
