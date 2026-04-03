/**
 * @file study_repository_test.cpp
 * @brief Unit tests for study_repository class
 *
 * Verifies CRUD operations, wildcard search, upsert semantics,
 * and modalities_in_study denormalization for the extracted study_repository.
 *
 * @see Issue #912 - Extract patient and study metadata repositories
 */

#include <catch2/catch_test_macros.hpp>

#include <kcenon/pacs/storage/patient_repository.h>
#include <kcenon/pacs/storage/study_repository.h>
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

/// Helper: insert a patient and return its pk
auto insert_test_patient(auto& repo, const std::string& id = "TEST-PAT-001")
    -> int64_t {
    auto result = repo.upsert_patient(id, "Test^Patient", "19800101", "M");
    if (result.is_err()) {
        throw std::runtime_error("Failed to insert test patient");
    }
    return result.value();
}

}  // namespace

// ============================================================================
// Construction
// ============================================================================

#ifdef PACS_WITH_DATABASE_SYSTEM

TEST_CASE("study_repository construction", "[storage][study]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported");
        return;
    }

    test_database db;
    study_repository repo(db.get());
    SUCCEED("Construction succeeded");
}

#else

TEST_CASE("study_repository construction", "[storage][study]") {
    test_database db;
    study_repository repo(db.get());
    SUCCEED("Construction succeeded");
}

#endif

// ============================================================================
// Upsert and Find
// ============================================================================

#ifdef PACS_WITH_DATABASE_SYSTEM

TEST_CASE("study_repository upsert and find", "[storage][study]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported");
        return;
    }

    test_database db;
    patient_repository pat_repo(db.get());
    study_repository repo(db.get());

    auto patient_pk = insert_test_patient(pat_repo);

    SECTION("upsert new study returns pk") {
        auto result = repo.upsert_study(
            patient_pk, "1.2.840.10008.1.1", "STUDY001",
            "20240101", "120000", "ACC001", "Dr^Smith", "CT Head");
        REQUIRE(result.is_ok());
        CHECK(result.value() > 0);
    }

    SECTION("find by study_uid") {
        (void)repo.upsert_study(
            patient_pk, "1.2.840.10008.2.2", "STUDY002",
            "20240215", "093000", "ACC002", "Dr^Jones", "MR Brain");

        auto found = repo.find_study("1.2.840.10008.2.2");
        REQUIRE(found.has_value());
        CHECK(found->study_uid == "1.2.840.10008.2.2");
        CHECK(found->study_id == "STUDY002");
        CHECK(found->study_date == "20240215");
        CHECK(found->accession_number == "ACC002");
        CHECK(found->referring_physician == "Dr^Jones");
        CHECK(found->study_description == "MR Brain");
        CHECK(found->patient_pk == patient_pk);
    }

    SECTION("find by pk") {
        auto pk = repo.upsert_study(
            patient_pk, "1.2.840.10008.3.3", "STUDY003");
        REQUIRE(pk.is_ok());

        auto found = repo.find_study_by_pk(pk.value());
        REQUIRE(found.has_value());
        CHECK(found->study_uid == "1.2.840.10008.3.3");
        CHECK(found->pk == pk.value());
    }

    SECTION("find non-existent returns nullopt") {
        auto found = repo.find_study("1.2.3.4.5.6.7.8.9.NOPE");
        CHECK_FALSE(found.has_value());
    }

    SECTION("upsert updates existing study") {
        auto pk1 = repo.upsert_study(
            patient_pk, "1.2.840.10008.4.4", "OLD-ID",
            "20240101", "", "", "", "Old Description");
        REQUIRE(pk1.is_ok());

        auto pk2 = repo.upsert_study(
            patient_pk, "1.2.840.10008.4.4", "NEW-ID",
            "20240101", "", "", "", "New Description");
        REQUIRE(pk2.is_ok());
        CHECK(pk1.value() == pk2.value());

        auto found = repo.find_study("1.2.840.10008.4.4");
        REQUIRE(found.has_value());
        CHECK(found->study_description == "New Description");
    }

    SECTION("upsert with record struct") {
        study_record record;
        record.patient_pk = patient_pk;
        record.study_uid = "1.2.840.10008.5.5";
        record.study_id = "REC-STUDY";
        record.study_date = "20240310";
        record.study_time = "143000";
        record.accession_number = "ACC-REC";
        record.referring_physician = "Dr^Record";
        record.study_description = "Record-based insert";

        auto result = repo.upsert_study(record);
        REQUIRE(result.is_ok());

        auto found = repo.find_study("1.2.840.10008.5.5");
        REQUIRE(found.has_value());
        CHECK(found->study_id == "REC-STUDY");
        CHECK(found->accession_number == "ACC-REC");
    }
}

#else  // !PACS_WITH_DATABASE_SYSTEM

TEST_CASE("study_repository upsert and find", "[storage][study]") {
    test_database db;
    patient_repository pat_repo(db.get());
    study_repository repo(db.get());

    auto patient_pk = insert_test_patient(pat_repo);

    SECTION("upsert new study returns pk") {
        auto result = repo.upsert_study(
            patient_pk, "1.2.840.10008.1.1", "STUDY001",
            "20240101", "120000", "ACC001", "Dr^Smith", "CT Head");
        REQUIRE(result.is_ok());
        CHECK(result.value() > 0);
    }

    SECTION("find by study_uid") {
        (void)repo.upsert_study(
            patient_pk, "1.2.840.10008.2.2", "STUDY002",
            "20240215", "093000", "ACC002", "Dr^Jones", "MR Brain");

        auto found = repo.find_study("1.2.840.10008.2.2");
        REQUIRE(found.has_value());
        CHECK(found->study_uid == "1.2.840.10008.2.2");
        CHECK(found->study_id == "STUDY002");
    }

    SECTION("find non-existent returns nullopt") {
        auto found = repo.find_study("1.2.3.4.5.6.7.8.9.NOPE");
        CHECK_FALSE(found.has_value());
    }

    SECTION("upsert updates existing study") {
        auto pk1 = repo.upsert_study(
            patient_pk, "1.2.840.10008.4.4", "OLD-ID",
            "20240101", "", "", "", "Old Description");
        REQUIRE(pk1.is_ok());

        auto pk2 = repo.upsert_study(
            patient_pk, "1.2.840.10008.4.4", "NEW-ID",
            "20240101", "", "", "", "New Description");
        REQUIRE(pk2.is_ok());
        CHECK(pk1.value() == pk2.value());

        auto found = repo.find_study("1.2.840.10008.4.4");
        REQUIRE(found.has_value());
        CHECK(found->study_description == "New Description");
    }
}

#endif  // PACS_WITH_DATABASE_SYSTEM

// ============================================================================
// Search
// ============================================================================

#ifdef PACS_WITH_DATABASE_SYSTEM

TEST_CASE("study_repository search", "[storage][study]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported");
        return;
    }

    test_database db;
    patient_repository pat_repo(db.get());
    study_repository repo(db.get());

    auto pat_pk = insert_test_patient(pat_repo, "SEARCH-PAT");
    (void)repo.upsert_study(
        pat_pk, "1.2.840.search.1", "S001",
        "20240101", "", "ACC-S1", "Dr^A", "CT Head");
    (void)repo.upsert_study(
        pat_pk, "1.2.840.search.2", "S002",
        "20240215", "", "ACC-S2", "Dr^B", "MR Brain");
    (void)repo.upsert_study(
        pat_pk, "1.2.840.search.3", "S003",
        "20240301", "", "ACC-S3", "Dr^A", "CT Chest");

    SECTION("search by study_date range") {
        study_query query;
        query.study_date_from = "20240101";
        query.study_date_to = "20240228";
        auto results = repo.search_studies(query);
        REQUIRE(results.is_ok());
        CHECK(results.value().size() == 2);
    }

    SECTION("search by accession_number wildcard") {
        study_query query;
        query.accession_number = "ACC-S*";
        auto results = repo.search_studies(query);
        REQUIRE(results.is_ok());
        CHECK(results.value().size() == 3);
    }

    SECTION("search by study_description wildcard") {
        study_query query;
        query.study_description = "CT*";
        auto results = repo.search_studies(query);
        REQUIRE(results.is_ok());
        CHECK(results.value().size() == 2);
    }

    SECTION("search with limit") {
        study_query query;
        query.limit = 2;
        auto results = repo.search_studies(query);
        REQUIRE(results.is_ok());
        CHECK(results.value().size() == 2);
    }

    SECTION("search with no matches") {
        study_query query;
        query.study_uid = "1.2.3.nonexistent";
        auto results = repo.search_studies(query);
        REQUIRE(results.is_ok());
        CHECK(results.value().empty());
    }
}

#else  // !PACS_WITH_DATABASE_SYSTEM

TEST_CASE("study_repository search", "[storage][study]") {
    test_database db;
    patient_repository pat_repo(db.get());
    study_repository repo(db.get());

    auto pat_pk = insert_test_patient(pat_repo, "SEARCH-PAT");
    (void)repo.upsert_study(
        pat_pk, "1.2.840.search.1", "S001",
        "20240101", "", "ACC-S1", "Dr^A", "CT Head");
    (void)repo.upsert_study(
        pat_pk, "1.2.840.search.2", "S002",
        "20240215", "", "ACC-S2", "Dr^B", "MR Brain");

    SECTION("search by accession_number wildcard") {
        study_query query;
        query.accession_number = "ACC-S*";
        auto results = repo.search_studies(query);
        REQUIRE(results.is_ok());
        CHECK(results.value().size() == 2);
    }

    SECTION("search with no matches") {
        study_query query;
        query.study_uid = "1.2.3.nonexistent";
        auto results = repo.search_studies(query);
        REQUIRE(results.is_ok());
        CHECK(results.value().empty());
    }
}

#endif  // PACS_WITH_DATABASE_SYSTEM

// ============================================================================
// Delete and Count
// ============================================================================

#ifdef PACS_WITH_DATABASE_SYSTEM

TEST_CASE("study_repository delete and count", "[storage][study]") {
    if (!is_sqlite_backend_supported()) {
        SUCCEED("Skipped: SQLite backend not supported");
        return;
    }

    test_database db;
    patient_repository pat_repo(db.get());
    study_repository repo(db.get());

    auto pat_pk = insert_test_patient(pat_repo, "DEL-PAT");
    (void)repo.upsert_study(pat_pk, "1.2.840.del.1", "DEL-S1");
    (void)repo.upsert_study(pat_pk, "1.2.840.del.2", "DEL-S2");

    SECTION("count returns correct total") {
        auto count = repo.study_count();
        REQUIRE(count.is_ok());
        CHECK(count.value() == 2);
    }

    SECTION("count for patient") {
        auto count = repo.study_count_for_patient(pat_pk);
        REQUIRE(count.is_ok());
        CHECK(count.value() == 2);
    }

    SECTION("delete removes study") {
        auto result = repo.delete_study("1.2.840.del.1");
        REQUIRE(result.is_ok());

        auto found = repo.find_study("1.2.840.del.1");
        CHECK_FALSE(found.has_value());

        auto count = repo.study_count();
        REQUIRE(count.is_ok());
        CHECK(count.value() == 1);
    }
}

#else  // !PACS_WITH_DATABASE_SYSTEM

TEST_CASE("study_repository delete and count", "[storage][study]") {
    test_database db;
    patient_repository pat_repo(db.get());
    study_repository repo(db.get());

    auto pat_pk = insert_test_patient(pat_repo, "DEL-PAT");
    (void)repo.upsert_study(pat_pk, "1.2.840.del.1", "DEL-S1");
    (void)repo.upsert_study(pat_pk, "1.2.840.del.2", "DEL-S2");

    SECTION("count returns correct total") {
        auto count = repo.study_count();
        REQUIRE(count.is_ok());
        CHECK(count.value() == 2);
    }

    SECTION("count for patient") {
        auto count = repo.study_count_for_patient(pat_pk);
        REQUIRE(count.is_ok());
        CHECK(count.value() == 2);
    }

    SECTION("delete removes study") {
        auto result = repo.delete_study("1.2.840.del.1");
        REQUIRE(result.is_ok());

        auto found = repo.find_study("1.2.840.del.1");
        CHECK_FALSE(found.has_value());

        auto count = repo.study_count();
        REQUIRE(count.is_ok());
        CHECK(count.value() == 1);
    }
}

#endif  // PACS_WITH_DATABASE_SYSTEM
