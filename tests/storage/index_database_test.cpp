/**
 * @file index_database_test.cpp
 * @brief Unit tests for index_database patient operations
 *
 * Tests CRUD operations for the patients table as specified in DES-DB-001.
 */

#include <pacs/storage/index_database.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <memory>

using namespace pacs::storage;

namespace {

/**
 * @brief Helper to create a test database
 */
auto create_test_database() -> std::unique_ptr<index_database> {
    auto result = index_database::open(":memory:");
    REQUIRE(result.is_ok());
    return std::move(result.value());
}

}  // namespace

// ============================================================================
// Database Creation Tests
// ============================================================================

TEST_CASE("index_database: create in-memory database", "[storage][database]") {
    auto result = index_database::open(":memory:");

    REQUIRE(result.is_ok());
    auto db = std::move(result.value());

    CHECK(db->is_open());
    CHECK(db->schema_version() == 1);
    CHECK(db->path() == ":memory:");
}

TEST_CASE("index_database: create file-based database",
          "[storage][database]") {
    const std::string test_path = "/tmp/pacs_test_db.sqlite";

    // Clean up any existing test file
    std::filesystem::remove(test_path);

    {
        auto result = index_database::open(test_path);
        REQUIRE(result.is_ok());
        auto db = std::move(result.value());

        CHECK(db->is_open());
        CHECK(db->schema_version() == 1);
    }

    // Verify file was created
    CHECK(std::filesystem::exists(test_path));

    // Clean up
    std::filesystem::remove(test_path);
}

// ============================================================================
// Patient Insert Tests
// ============================================================================

TEST_CASE("index_database: insert patient with basic info",
          "[storage][patient]") {
    auto db = create_test_database();

    auto result = db->upsert_patient("12345", "Doe^John", "19800115", "M");

    REQUIRE(result.is_ok());
    CHECK(result.value() > 0);

    // Verify patient was inserted
    auto patient = db->find_patient("12345");
    REQUIRE(patient.has_value());
    CHECK(patient->patient_id == "12345");
    CHECK(patient->patient_name == "Doe^John");
    CHECK(patient->birth_date == "19800115");
    CHECK(patient->sex == "M");
}

TEST_CASE("index_database: insert patient with full record",
          "[storage][patient]") {
    auto db = create_test_database();

    patient_record record;
    record.patient_id = "67890";
    record.patient_name = "Smith^Jane";
    record.birth_date = "19900220";
    record.sex = "F";
    record.other_ids = "ALT001";
    record.ethnic_group = "2106-3";
    record.comments = "Test patient";

    auto result = db->upsert_patient(record);

    REQUIRE(result.is_ok());

    auto patient = db->find_patient("67890");
    REQUIRE(patient.has_value());
    CHECK(patient->patient_id == "67890");
    CHECK(patient->patient_name == "Smith^Jane");
    CHECK(patient->other_ids == "ALT001");
    CHECK(patient->ethnic_group == "2106-3");
    CHECK(patient->comments == "Test patient");
}

TEST_CASE("index_database: insert patient requires patient_id",
          "[storage][patient]") {
    auto db = create_test_database();

    auto result = db->upsert_patient("", "Doe^John", "19800115", "M");

    REQUIRE(result.is_err());
    CHECK(result.error().message.find("Patient ID is required") !=
          std::string::npos);
}

TEST_CASE("index_database: patient_id max length validation",
          "[storage][patient]") {
    auto db = create_test_database();

    // 65 characters - should fail
    std::string long_id(65, 'X');
    auto result = db->upsert_patient(long_id, "Test", "", "");

    REQUIRE(result.is_err());
    CHECK(result.error().message.find("maximum length") != std::string::npos);

    // 64 characters - should succeed
    std::string max_id(64, 'X');
    result = db->upsert_patient(max_id, "Test", "", "");
    REQUIRE(result.is_ok());
}

TEST_CASE("index_database: sex value validation", "[storage][patient]") {
    auto db = create_test_database();

    // Valid values
    REQUIRE(db->upsert_patient("P1", "Test", "", "M").is_ok());
    REQUIRE(db->upsert_patient("P2", "Test", "", "F").is_ok());
    REQUIRE(db->upsert_patient("P3", "Test", "", "O").is_ok());
    REQUIRE(db->upsert_patient("P4", "Test", "", "").is_ok());  // Empty is OK

    // Invalid value
    auto result = db->upsert_patient("P5", "Test", "", "X");
    REQUIRE(result.is_err());
    CHECK(result.error().message.find("Invalid sex value") !=
          std::string::npos);
}

// ============================================================================
// Patient Update Tests
// ============================================================================

TEST_CASE("index_database: update existing patient", "[storage][patient]") {
    auto db = create_test_database();

    // Insert initial patient
    REQUIRE(db->upsert_patient("12345", "Doe^John", "19800115", "M").is_ok());

    // Update with new name
    auto result = db->upsert_patient("12345", "Doe^Jane", "19800115", "F");
    REQUIRE(result.is_ok());

    // Verify only one patient exists
    CHECK(db->patient_count() == 1);

    // Verify update was applied
    auto patient = db->find_patient("12345");
    REQUIRE(patient.has_value());
    CHECK(patient->patient_name == "Doe^Jane");
    CHECK(patient->sex == "F");
}

TEST_CASE("index_database: upsert preserves primary key",
          "[storage][patient]") {
    auto db = create_test_database();

    // Insert patient
    auto pk1 = db->upsert_patient("12345", "Doe^John", "19800115", "M");
    REQUIRE(pk1.is_ok());

    // Update same patient
    auto pk2 = db->upsert_patient("12345", "Doe^Jane", "19800115", "F");
    REQUIRE(pk2.is_ok());

    // Primary key should be the same
    CHECK(pk1.value() == pk2.value());
}

// ============================================================================
// Patient Search Tests
// ============================================================================

TEST_CASE("index_database: find patient by id", "[storage][patient]") {
    auto db = create_test_database();

    REQUIRE(db->upsert_patient("12345", "Doe^John", "19800115", "M").is_ok());

    auto patient = db->find_patient("12345");
    REQUIRE(patient.has_value());
    CHECK(patient->patient_id == "12345");

    // Non-existent patient
    auto not_found = db->find_patient("99999");
    CHECK_FALSE(not_found.has_value());
}

TEST_CASE("index_database: find patient by pk", "[storage][patient]") {
    auto db = create_test_database();

    auto result = db->upsert_patient("12345", "Doe^John", "19800115", "M");
    REQUIRE(result.is_ok());
    auto pk = result.value();

    auto patient = db->find_patient_by_pk(pk);
    REQUIRE(patient.has_value());
    CHECK(patient->patient_id == "12345");

    // Non-existent PK
    auto not_found = db->find_patient_by_pk(99999);
    CHECK_FALSE(not_found.has_value());
}

TEST_CASE("index_database: search patients by name wildcard",
          "[storage][patient]") {
    auto db = create_test_database();

    // Insert test data
    REQUIRE(db->upsert_patient("001", "Doe^John", "19800115", "M").is_ok());
    REQUIRE(db->upsert_patient("002", "Doe^Jane", "19850220", "F").is_ok());
    REQUIRE(db->upsert_patient("003", "Smith^Bob", "19900310", "M").is_ok());
    REQUIRE(db->upsert_patient("004", "Johnson^Mary", "19751205", "F").is_ok());

    SECTION("prefix wildcard") {
        patient_query query;
        query.patient_name = "Doe*";

        auto results = db->search_patients(query);

        CHECK(results.size() == 2);
        CHECK(results[0].patient_name == "Doe^Jane");  // Ordered by name
        CHECK(results[1].patient_name == "Doe^John");
    }

    SECTION("suffix wildcard") {
        patient_query query;
        query.patient_name = "*John";

        auto results = db->search_patients(query);

        CHECK(results.size() == 1);
        CHECK(results[0].patient_name == "Doe^John");
    }

    SECTION("contains wildcard") {
        patient_query query;
        query.patient_name = "*o*";

        auto results = db->search_patients(query);

        // Matches: Doe^John, Doe^Jane, Johnson^Mary, Smith^Bob
        CHECK(results.size() == 4);
    }
}

TEST_CASE("index_database: search patients by patient_id wildcard",
          "[storage][patient]") {
    auto db = create_test_database();

    REQUIRE(db->upsert_patient("ABC001", "Test1", "", "").is_ok());
    REQUIRE(db->upsert_patient("ABC002", "Test2", "", "").is_ok());
    REQUIRE(db->upsert_patient("XYZ001", "Test3", "", "").is_ok());

    patient_query query;
    query.patient_id = "ABC*";

    auto results = db->search_patients(query);

    CHECK(results.size() == 2);
}

TEST_CASE("index_database: search patients by sex", "[storage][patient]") {
    auto db = create_test_database();

    REQUIRE(db->upsert_patient("001", "Doe^John", "", "M").is_ok());
    REQUIRE(db->upsert_patient("002", "Doe^Jane", "", "F").is_ok());
    REQUIRE(db->upsert_patient("003", "Smith^Bob", "", "M").is_ok());

    patient_query query;
    query.sex = "M";

    auto results = db->search_patients(query);

    CHECK(results.size() == 2);
}

TEST_CASE("index_database: search patients by birth date range",
          "[storage][patient]") {
    auto db = create_test_database();

    REQUIRE(db->upsert_patient("001", "Test1", "19800101", "").is_ok());
    REQUIRE(db->upsert_patient("002", "Test2", "19850615", "").is_ok());
    REQUIRE(db->upsert_patient("003", "Test3", "19901231", "").is_ok());

    SECTION("exact date") {
        patient_query query;
        query.birth_date = "19850615";

        auto results = db->search_patients(query);
        CHECK(results.size() == 1);
        CHECK(results[0].patient_id == "002");
    }

    SECTION("date range") {
        patient_query query;
        query.birth_date_from = "19800101";
        query.birth_date_to = "19851231";

        auto results = db->search_patients(query);
        CHECK(results.size() == 2);
    }
}

TEST_CASE("index_database: search patients with pagination",
          "[storage][patient]") {
    auto db = create_test_database();

    // Insert 10 patients
    for (int i = 1; i <= 10; ++i) {
        auto id = std::format("{:03d}", i);
        REQUIRE(db->upsert_patient(id, "Test^Patient" + std::to_string(i), "", "").is_ok());
    }

    patient_query query;
    query.limit = 3;
    query.offset = 0;

    auto page1 = db->search_patients(query);
    CHECK(page1.size() == 3);

    query.offset = 3;
    auto page2 = db->search_patients(query);
    CHECK(page2.size() == 3);

    // Last page
    query.offset = 9;
    auto page4 = db->search_patients(query);
    CHECK(page4.size() == 1);
}

TEST_CASE("index_database: search with multiple criteria",
          "[storage][patient]") {
    auto db = create_test_database();

    REQUIRE(db->upsert_patient("001", "Doe^John", "19800115", "M").is_ok());
    REQUIRE(db->upsert_patient("002", "Doe^Jane", "19850220", "F").is_ok());
    REQUIRE(db->upsert_patient("003", "Smith^John", "19800115", "M").is_ok());

    patient_query query;
    query.patient_name = "Doe*";
    query.sex = "M";

    auto results = db->search_patients(query);

    CHECK(results.size() == 1);
    CHECK(results[0].patient_id == "001");
}

// ============================================================================
// Patient Delete Tests
// ============================================================================

TEST_CASE("index_database: delete patient", "[storage][patient]") {
    auto db = create_test_database();

    REQUIRE(db->upsert_patient("12345", "Doe^John", "19800115", "M").is_ok());
    CHECK(db->patient_count() == 1);

    auto result = db->delete_patient("12345");
    REQUIRE(result.is_ok());

    CHECK(db->patient_count() == 0);
    CHECK_FALSE(db->find_patient("12345").has_value());
}

TEST_CASE("index_database: delete non-existent patient",
          "[storage][patient]") {
    auto db = create_test_database();

    // Should not error
    auto result = db->delete_patient("nonexistent");
    CHECK(result.is_ok());
}

// ============================================================================
// Patient Count Tests
// ============================================================================

TEST_CASE("index_database: patient count", "[storage][patient]") {
    auto db = create_test_database();

    CHECK(db->patient_count() == 0);

    REQUIRE(db->upsert_patient("001", "Test1", "", "").is_ok());
    CHECK(db->patient_count() == 1);

    REQUIRE(db->upsert_patient("002", "Test2", "", "").is_ok());
    CHECK(db->patient_count() == 2);

    REQUIRE(db->delete_patient("001").is_ok());
    CHECK(db->patient_count() == 1);
}

// ============================================================================
// Patient Record Tests
// ============================================================================

TEST_CASE("patient_record: is_valid", "[storage][patient]") {
    patient_record record;

    CHECK_FALSE(record.is_valid());

    record.patient_id = "12345";
    CHECK(record.is_valid());
}

TEST_CASE("patient_query: has_criteria", "[storage][patient]") {
    patient_query query;

    CHECK_FALSE(query.has_criteria());

    query.patient_name = "Test";
    CHECK(query.has_criteria());
}
