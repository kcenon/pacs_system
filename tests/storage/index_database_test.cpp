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

// ============================================================================
// Study Insert Tests
// ============================================================================

namespace {

/**
 * @brief Helper to create a test patient and return pk
 */
auto create_test_patient(index_database& db, const std::string& patient_id = "P001")
    -> int64_t {
    auto result = db.upsert_patient(patient_id, "Test^Patient", "19800115", "M");
    REQUIRE(result.is_ok());
    return result.value();
}

}  // namespace

TEST_CASE("index_database: insert study with basic info", "[storage][study]") {
    auto db = create_test_database();
    auto patient_pk = create_test_patient(*db);

    auto result = db->upsert_study(patient_pk, "1.2.3.4.5.6.7", "STUDY001",
                                   "20231115", "120000", "ACC001",
                                   "Dr. Smith", "CT Head");

    REQUIRE(result.is_ok());
    CHECK(result.value() > 0);

    // Verify study was inserted
    auto study = db->find_study("1.2.3.4.5.6.7");
    REQUIRE(study.has_value());
    CHECK(study->study_uid == "1.2.3.4.5.6.7");
    CHECK(study->study_id == "STUDY001");
    CHECK(study->study_date == "20231115");
    CHECK(study->study_time == "120000");
    CHECK(study->accession_number == "ACC001");
    CHECK(study->referring_physician == "Dr. Smith");
    CHECK(study->study_description == "CT Head");
}

TEST_CASE("index_database: insert study with full record", "[storage][study]") {
    auto db = create_test_database();
    auto patient_pk = create_test_patient(*db);

    study_record record;
    record.patient_pk = patient_pk;
    record.study_uid = "1.2.3.4.5.6.8";
    record.study_id = "STUDY002";
    record.study_date = "20231120";
    record.study_time = "143000";
    record.accession_number = "ACC002";
    record.referring_physician = "Dr. Jones";
    record.study_description = "MRI Brain";

    auto result = db->upsert_study(record);

    REQUIRE(result.is_ok());

    auto study = db->find_study("1.2.3.4.5.6.8");
    REQUIRE(study.has_value());
    CHECK(study->study_id == "STUDY002");
    CHECK(study->study_description == "MRI Brain");
}

TEST_CASE("index_database: insert study requires study_uid",
          "[storage][study]") {
    auto db = create_test_database();
    auto patient_pk = create_test_patient(*db);

    auto result = db->upsert_study(patient_pk, "", "STUDY001");

    REQUIRE(result.is_err());
    CHECK(result.error().message.find("Study Instance UID is required") !=
          std::string::npos);
}

TEST_CASE("index_database: insert study requires valid patient_pk",
          "[storage][study]") {
    auto db = create_test_database();

    auto result = db->upsert_study(0, "1.2.3.4.5.6.7", "STUDY001");

    REQUIRE(result.is_err());
    CHECK(result.error().message.find("patient_pk is required") !=
          std::string::npos);
}

TEST_CASE("index_database: study_uid max length validation",
          "[storage][study]") {
    auto db = create_test_database();
    auto patient_pk = create_test_patient(*db);

    // 65 characters - should fail
    std::string long_uid(65, '1');
    auto result = db->upsert_study(patient_pk, long_uid, "TEST");

    REQUIRE(result.is_err());
    CHECK(result.error().message.find("maximum length") != std::string::npos);

    // 64 characters - should succeed
    std::string max_uid(64, '1');
    result = db->upsert_study(patient_pk, max_uid, "TEST");
    REQUIRE(result.is_ok());
}

// ============================================================================
// Study Update Tests
// ============================================================================

TEST_CASE("index_database: update existing study", "[storage][study]") {
    auto db = create_test_database();
    auto patient_pk = create_test_patient(*db);

    // Insert initial study
    REQUIRE(db->upsert_study(patient_pk, "1.2.3.4.5.6.7", "STUDY001",
                             "20231115", "120000", "ACC001", "Dr. Smith",
                             "CT Head")
                .is_ok());

    // Update with new description
    auto result = db->upsert_study(patient_pk, "1.2.3.4.5.6.7", "STUDY001",
                                   "20231115", "120000", "ACC001", "Dr. Smith",
                                   "CT Head with Contrast");
    REQUIRE(result.is_ok());

    // Verify only one study exists
    CHECK(db->study_count() == 1);

    // Verify update was applied
    auto study = db->find_study("1.2.3.4.5.6.7");
    REQUIRE(study.has_value());
    CHECK(study->study_description == "CT Head with Contrast");
}

TEST_CASE("index_database: upsert study preserves primary key",
          "[storage][study]") {
    auto db = create_test_database();
    auto patient_pk = create_test_patient(*db);

    // Insert study
    auto pk1 = db->upsert_study(patient_pk, "1.2.3.4.5.6.7", "STUDY001");
    REQUIRE(pk1.is_ok());

    // Update same study
    auto pk2 = db->upsert_study(patient_pk, "1.2.3.4.5.6.7", "STUDY001_UPDATED");
    REQUIRE(pk2.is_ok());

    // Primary key should be the same
    CHECK(pk1.value() == pk2.value());
}

// ============================================================================
// Study Search Tests
// ============================================================================

TEST_CASE("index_database: find study by uid", "[storage][study]") {
    auto db = create_test_database();
    auto patient_pk = create_test_patient(*db);

    REQUIRE(db->upsert_study(patient_pk, "1.2.3.4.5.6.7", "STUDY001").is_ok());

    auto study = db->find_study("1.2.3.4.5.6.7");
    REQUIRE(study.has_value());
    CHECK(study->study_uid == "1.2.3.4.5.6.7");

    // Non-existent study
    auto not_found = db->find_study("9.9.9.9.9.9.9");
    CHECK_FALSE(not_found.has_value());
}

TEST_CASE("index_database: find study by pk", "[storage][study]") {
    auto db = create_test_database();
    auto patient_pk = create_test_patient(*db);

    auto result = db->upsert_study(patient_pk, "1.2.3.4.5.6.7", "STUDY001");
    REQUIRE(result.is_ok());
    auto pk = result.value();

    auto study = db->find_study_by_pk(pk);
    REQUIRE(study.has_value());
    CHECK(study->study_uid == "1.2.3.4.5.6.7");

    // Non-existent PK
    auto not_found = db->find_study_by_pk(99999);
    CHECK_FALSE(not_found.has_value());
}

TEST_CASE("index_database: list studies for patient", "[storage][study]") {
    auto db = create_test_database();

    // Create two patients
    auto patient1_pk = create_test_patient(*db, "P001");
    auto patient2_pk = create_test_patient(*db, "P002");

    // Add studies to patient 1
    REQUIRE(
        db->upsert_study(patient1_pk, "1.2.3.1", "STUDY01", "20231115").is_ok());
    REQUIRE(
        db->upsert_study(patient1_pk, "1.2.3.2", "STUDY02", "20231120").is_ok());

    // Add study to patient 2
    REQUIRE(
        db->upsert_study(patient2_pk, "1.2.3.3", "STUDY03", "20231125").is_ok());

    // List studies for patient 1
    auto studies = db->list_studies("P001");

    CHECK(studies.size() == 2);
    // Should be ordered by date DESC
    CHECK(studies[0].study_date == "20231120");
    CHECK(studies[1].study_date == "20231115");
}

TEST_CASE("index_database: search studies by patient id", "[storage][study]") {
    auto db = create_test_database();

    auto patient_pk = create_test_patient(*db, "PAT001");
    REQUIRE(db->upsert_study(patient_pk, "1.2.3.1", "STUDY01").is_ok());
    REQUIRE(db->upsert_study(patient_pk, "1.2.3.2", "STUDY02").is_ok());

    auto patient2_pk = create_test_patient(*db, "PAT002");
    REQUIRE(db->upsert_study(patient2_pk, "1.2.3.3", "STUDY03").is_ok());

    study_query query;
    query.patient_id = "PAT001";

    auto results = db->search_studies(query);

    CHECK(results.size() == 2);
}

TEST_CASE("index_database: search studies by date range", "[storage][study]") {
    auto db = create_test_database();
    auto patient_pk = create_test_patient(*db);

    REQUIRE(db->upsert_study(patient_pk, "1.2.3.1", "S1", "20231101").is_ok());
    REQUIRE(db->upsert_study(patient_pk, "1.2.3.2", "S2", "20231115").is_ok());
    REQUIRE(db->upsert_study(patient_pk, "1.2.3.3", "S3", "20231130").is_ok());

    SECTION("exact date") {
        study_query query;
        query.study_date = "20231115";

        auto results = db->search_studies(query);
        CHECK(results.size() == 1);
        CHECK(results[0].study_id == "S2");
    }

    SECTION("date range") {
        study_query query;
        query.study_date_from = "20231110";
        query.study_date_to = "20231125";

        auto results = db->search_studies(query);
        CHECK(results.size() == 1);
        CHECK(results[0].study_id == "S2");
    }
}

TEST_CASE("index_database: search studies by accession number",
          "[storage][study]") {
    auto db = create_test_database();
    auto patient_pk = create_test_patient(*db);

    REQUIRE(db->upsert_study(patient_pk, "1.2.3.1", "S1", "", "", "ACC001")
                .is_ok());
    REQUIRE(db->upsert_study(patient_pk, "1.2.3.2", "S2", "", "", "ACC002")
                .is_ok());
    REQUIRE(db->upsert_study(patient_pk, "1.2.3.3", "S3", "", "", "ACC003")
                .is_ok());

    study_query query;
    query.accession_number = "ACC00*";

    auto results = db->search_studies(query);

    CHECK(results.size() == 3);
}

TEST_CASE("index_database: search studies with pagination",
          "[storage][study]") {
    auto db = create_test_database();
    auto patient_pk = create_test_patient(*db);

    // Insert 10 studies
    for (int i = 1; i <= 10; ++i) {
        auto uid = std::format("1.2.3.{}", i);
        auto study_id = std::format("STUDY{:02d}", i);
        auto date = std::format("202311{:02d}", i);
        REQUIRE(db->upsert_study(patient_pk, uid, study_id, date).is_ok());
    }

    study_query query;
    query.limit = 3;
    query.offset = 0;

    auto page1 = db->search_studies(query);
    CHECK(page1.size() == 3);

    query.offset = 3;
    auto page2 = db->search_studies(query);
    CHECK(page2.size() == 3);

    // Last page
    query.offset = 9;
    auto page4 = db->search_studies(query);
    CHECK(page4.size() == 1);
}

TEST_CASE("index_database: search studies with multiple criteria",
          "[storage][study]") {
    auto db = create_test_database();
    auto patient_pk = create_test_patient(*db);

    REQUIRE(db->upsert_study(patient_pk, "1.2.3.1", "S1", "20231115", "", "",
                             "Dr. Smith", "CT Head")
                .is_ok());
    REQUIRE(db->upsert_study(patient_pk, "1.2.3.2", "S2", "20231115", "", "",
                             "Dr. Jones", "CT Chest")
                .is_ok());
    REQUIRE(db->upsert_study(patient_pk, "1.2.3.3", "S3", "20231120", "", "",
                             "Dr. Smith", "MRI Brain")
                .is_ok());

    study_query query;
    query.study_date = "20231115";
    query.referring_physician = "Dr. Smith";

    auto results = db->search_studies(query);

    CHECK(results.size() == 1);
    CHECK(results[0].study_id == "S1");
}

// ============================================================================
// Study Delete Tests
// ============================================================================

TEST_CASE("index_database: delete study", "[storage][study]") {
    auto db = create_test_database();
    auto patient_pk = create_test_patient(*db);

    REQUIRE(db->upsert_study(patient_pk, "1.2.3.4.5.6.7", "STUDY001").is_ok());
    CHECK(db->study_count() == 1);

    auto result = db->delete_study("1.2.3.4.5.6.7");
    REQUIRE(result.is_ok());

    CHECK(db->study_count() == 0);
    CHECK_FALSE(db->find_study("1.2.3.4.5.6.7").has_value());
}

TEST_CASE("index_database: delete non-existent study", "[storage][study]") {
    auto db = create_test_database();

    // Should not error
    auto result = db->delete_study("nonexistent");
    CHECK(result.is_ok());
}

// ============================================================================
// Study Count Tests
// ============================================================================

TEST_CASE("index_database: study count", "[storage][study]") {
    auto db = create_test_database();
    auto patient_pk = create_test_patient(*db);

    CHECK(db->study_count() == 0);

    REQUIRE(db->upsert_study(patient_pk, "1.2.3.1", "S1").is_ok());
    CHECK(db->study_count() == 1);

    REQUIRE(db->upsert_study(patient_pk, "1.2.3.2", "S2").is_ok());
    CHECK(db->study_count() == 2);

    REQUIRE(db->delete_study("1.2.3.1").is_ok());
    CHECK(db->study_count() == 1);
}

TEST_CASE("index_database: study count for patient", "[storage][study]") {
    auto db = create_test_database();

    auto patient1_pk = create_test_patient(*db, "P001");
    auto patient2_pk = create_test_patient(*db, "P002");

    REQUIRE(db->upsert_study(patient1_pk, "1.2.3.1", "S1").is_ok());
    REQUIRE(db->upsert_study(patient1_pk, "1.2.3.2", "S2").is_ok());
    REQUIRE(db->upsert_study(patient2_pk, "1.2.3.3", "S3").is_ok());

    CHECK(db->study_count("P001") == 2);
    CHECK(db->study_count("P002") == 1);
    CHECK(db->study_count("P999") == 0);
}

// ============================================================================
// Study Record Tests
// ============================================================================

TEST_CASE("study_record: is_valid", "[storage][study]") {
    study_record record;

    CHECK_FALSE(record.is_valid());

    record.study_uid = "1.2.3.4.5.6.7";
    CHECK(record.is_valid());
}

TEST_CASE("study_query: has_criteria", "[storage][study]") {
    study_query query;

    CHECK_FALSE(query.has_criteria());

    query.patient_id = "P001";
    CHECK(query.has_criteria());
}

// ============================================================================
// Patient-Study Cascade Tests
// ============================================================================

TEST_CASE("index_database: delete patient cascades to studies",
          "[storage][cascade]") {
    auto db = create_test_database();
    auto patient_pk = create_test_patient(*db, "P001");

    REQUIRE(db->upsert_study(patient_pk, "1.2.3.1", "S1").is_ok());
    REQUIRE(db->upsert_study(patient_pk, "1.2.3.2", "S2").is_ok());

    CHECK(db->study_count() == 2);

    // Delete patient should cascade to studies
    REQUIRE(db->delete_patient("P001").is_ok());

    CHECK(db->study_count() == 0);
    CHECK_FALSE(db->find_study("1.2.3.1").has_value());
    CHECK_FALSE(db->find_study("1.2.3.2").has_value());
}

// ============================================================================
// Series Insert Tests
// ============================================================================

namespace {

/**
 * @brief Helper to create a test study and return pk
 */
auto create_test_study(index_database& db, int64_t patient_pk,
                       const std::string& study_uid = "1.2.3.4.5.6.7")
    -> int64_t {
    auto result = db.upsert_study(patient_pk, study_uid, "STUDY001");
    REQUIRE(result.is_ok());
    return result.value();
}

}  // namespace

TEST_CASE("index_database: insert series with basic info", "[storage][series]") {
    auto db = create_test_database();
    auto patient_pk = create_test_patient(*db);
    auto study_pk = create_test_study(*db, patient_pk);

    auto result = db->upsert_series(study_pk, "1.2.3.4.5.6.7.1", "CT", 1,
                                    "CT Series 1", "HEAD", "SCANNER1");

    REQUIRE(result.is_ok());
    CHECK(result.value() > 0);

    // Verify series was inserted
    auto series = db->find_series("1.2.3.4.5.6.7.1");
    REQUIRE(series.has_value());
    CHECK(series->series_uid == "1.2.3.4.5.6.7.1");
    CHECK(series->modality == "CT");
    CHECK(series->series_number.has_value());
    CHECK(*series->series_number == 1);
    CHECK(series->series_description == "CT Series 1");
    CHECK(series->body_part_examined == "HEAD");
    CHECK(series->station_name == "SCANNER1");
}

TEST_CASE("index_database: insert series with full record", "[storage][series]") {
    auto db = create_test_database();
    auto patient_pk = create_test_patient(*db);
    auto study_pk = create_test_study(*db, patient_pk);

    series_record record;
    record.study_pk = study_pk;
    record.series_uid = "1.2.3.4.5.6.7.2";
    record.modality = "MR";
    record.series_number = 2;
    record.series_description = "MR Brain";
    record.body_part_examined = "BRAIN";
    record.station_name = "MRI-001";

    auto result = db->upsert_series(record);

    REQUIRE(result.is_ok());

    auto series = db->find_series("1.2.3.4.5.6.7.2");
    REQUIRE(series.has_value());
    CHECK(series->modality == "MR");
    CHECK(series->series_description == "MR Brain");
}

TEST_CASE("index_database: insert series requires series_uid",
          "[storage][series]") {
    auto db = create_test_database();
    auto patient_pk = create_test_patient(*db);
    auto study_pk = create_test_study(*db, patient_pk);

    auto result = db->upsert_series(study_pk, "", "CT");

    REQUIRE(result.is_err());
    CHECK(result.error().message.find("Series Instance UID is required") !=
          std::string::npos);
}

TEST_CASE("index_database: insert series requires valid study_pk",
          "[storage][series]") {
    auto db = create_test_database();

    auto result = db->upsert_series(0, "1.2.3.4.5.6.7.1", "CT");

    REQUIRE(result.is_err());
    CHECK(result.error().message.find("study_pk is required") !=
          std::string::npos);
}

TEST_CASE("index_database: series_uid max length validation",
          "[storage][series]") {
    auto db = create_test_database();
    auto patient_pk = create_test_patient(*db);
    auto study_pk = create_test_study(*db, patient_pk);

    // 65 characters - should fail
    std::string long_uid(65, '1');
    auto result = db->upsert_series(study_pk, long_uid, "CT");

    REQUIRE(result.is_err());
    CHECK(result.error().message.find("maximum length") != std::string::npos);

    // 64 characters - should succeed
    std::string max_uid(64, '1');
    result = db->upsert_series(study_pk, max_uid, "CT");
    REQUIRE(result.is_ok());
}

// ============================================================================
// Series Update Tests
// ============================================================================

TEST_CASE("index_database: update existing series", "[storage][series]") {
    auto db = create_test_database();
    auto patient_pk = create_test_patient(*db);
    auto study_pk = create_test_study(*db, patient_pk);

    // Insert initial series
    REQUIRE(db->upsert_series(study_pk, "1.2.3.4.5.6.7.1", "CT", 1,
                               "CT Series 1").is_ok());

    // Update with new description
    auto result = db->upsert_series(study_pk, "1.2.3.4.5.6.7.1", "CT", 1,
                                    "CT Series 1 Updated");
    REQUIRE(result.is_ok());

    // Verify only one series exists
    CHECK(db->series_count() == 1);

    // Verify update was applied
    auto series = db->find_series("1.2.3.4.5.6.7.1");
    REQUIRE(series.has_value());
    CHECK(series->series_description == "CT Series 1 Updated");
}

TEST_CASE("index_database: upsert series preserves primary key",
          "[storage][series]") {
    auto db = create_test_database();
    auto patient_pk = create_test_patient(*db);
    auto study_pk = create_test_study(*db, patient_pk);

    // Insert series
    auto pk1 = db->upsert_series(study_pk, "1.2.3.4.5.6.7.1", "CT");
    REQUIRE(pk1.is_ok());

    // Update same series
    auto pk2 = db->upsert_series(study_pk, "1.2.3.4.5.6.7.1", "MR");
    REQUIRE(pk2.is_ok());

    // Primary key should be the same
    CHECK(pk1.value() == pk2.value());
}

// ============================================================================
// Series Search Tests
// ============================================================================

TEST_CASE("index_database: find series by uid", "[storage][series]") {
    auto db = create_test_database();
    auto patient_pk = create_test_patient(*db);
    auto study_pk = create_test_study(*db, patient_pk);

    REQUIRE(db->upsert_series(study_pk, "1.2.3.4.5.6.7.1", "CT").is_ok());

    auto series = db->find_series("1.2.3.4.5.6.7.1");
    REQUIRE(series.has_value());
    CHECK(series->series_uid == "1.2.3.4.5.6.7.1");

    // Non-existent series
    auto not_found = db->find_series("9.9.9.9.9.9.9.9");
    CHECK_FALSE(not_found.has_value());
}

TEST_CASE("index_database: find series by pk", "[storage][series]") {
    auto db = create_test_database();
    auto patient_pk = create_test_patient(*db);
    auto study_pk = create_test_study(*db, patient_pk);

    auto result = db->upsert_series(study_pk, "1.2.3.4.5.6.7.1", "CT");
    REQUIRE(result.is_ok());
    auto pk = result.value();

    auto series = db->find_series_by_pk(pk);
    REQUIRE(series.has_value());
    CHECK(series->series_uid == "1.2.3.4.5.6.7.1");

    // Non-existent PK
    auto not_found = db->find_series_by_pk(99999);
    CHECK_FALSE(not_found.has_value());
}

TEST_CASE("index_database: list series for study", "[storage][series]") {
    auto db = create_test_database();
    auto patient_pk = create_test_patient(*db);
    auto study1_pk = create_test_study(*db, patient_pk, "1.2.3.4.5.6.7");
    auto study2_pk = create_test_study(*db, patient_pk, "1.2.3.4.5.6.8");

    // Add series to study 1
    REQUIRE(db->upsert_series(study1_pk, "1.2.3.1", "CT", 1).is_ok());
    REQUIRE(db->upsert_series(study1_pk, "1.2.3.2", "CT", 2).is_ok());

    // Add series to study 2
    REQUIRE(db->upsert_series(study2_pk, "1.2.3.3", "MR", 1).is_ok());

    // List series for study 1
    auto series_list = db->list_series("1.2.3.4.5.6.7");

    CHECK(series_list.size() == 2);
    // Should be ordered by series number
    CHECK(series_list[0].series_number.value_or(-1) == 1);
    CHECK(series_list[1].series_number.value_or(-1) == 2);
}

TEST_CASE("index_database: search series by modality", "[storage][series]") {
    auto db = create_test_database();
    auto patient_pk = create_test_patient(*db);
    auto study_pk = create_test_study(*db, patient_pk);

    REQUIRE(db->upsert_series(study_pk, "1.2.3.1", "CT").is_ok());
    REQUIRE(db->upsert_series(study_pk, "1.2.3.2", "CT").is_ok());
    REQUIRE(db->upsert_series(study_pk, "1.2.3.3", "MR").is_ok());

    series_query query;
    query.modality = "CT";

    auto results = db->search_series(query);

    CHECK(results.size() == 2);
}

TEST_CASE("index_database: search series by study_uid", "[storage][series]") {
    auto db = create_test_database();
    auto patient_pk = create_test_patient(*db);
    auto study1_pk = create_test_study(*db, patient_pk, "1.2.3.4.5.6.7");
    auto study2_pk = create_test_study(*db, patient_pk, "1.2.3.4.5.6.8");

    REQUIRE(db->upsert_series(study1_pk, "1.2.3.1", "CT").is_ok());
    REQUIRE(db->upsert_series(study1_pk, "1.2.3.2", "CT").is_ok());
    REQUIRE(db->upsert_series(study2_pk, "1.2.3.3", "MR").is_ok());

    series_query query;
    query.study_uid = "1.2.3.4.5.6.7";

    auto results = db->search_series(query);

    CHECK(results.size() == 2);
}

TEST_CASE("index_database: search series with pagination", "[storage][series]") {
    auto db = create_test_database();
    auto patient_pk = create_test_patient(*db);
    auto study_pk = create_test_study(*db, patient_pk);

    // Insert 10 series
    for (int i = 1; i <= 10; ++i) {
        auto uid = std::format("1.2.3.4.5.6.7.{}", i);
        REQUIRE(db->upsert_series(study_pk, uid, "CT", i).is_ok());
    }

    series_query query;
    query.limit = 3;
    query.offset = 0;

    auto page1 = db->search_series(query);
    CHECK(page1.size() == 3);

    query.offset = 3;
    auto page2 = db->search_series(query);
    CHECK(page2.size() == 3);

    // Last page
    query.offset = 9;
    auto page4 = db->search_series(query);
    CHECK(page4.size() == 1);
}

TEST_CASE("index_database: search series with multiple criteria",
          "[storage][series]") {
    auto db = create_test_database();
    auto patient_pk = create_test_patient(*db);
    auto study_pk = create_test_study(*db, patient_pk);

    REQUIRE(db->upsert_series(study_pk, "1.2.3.1", "CT", 1, "", "HEAD").is_ok());
    REQUIRE(db->upsert_series(study_pk, "1.2.3.2", "CT", 2, "", "CHEST").is_ok());
    REQUIRE(db->upsert_series(study_pk, "1.2.3.3", "MR", 3, "", "HEAD").is_ok());

    series_query query;
    query.modality = "CT";
    query.body_part_examined = "HEAD";

    auto results = db->search_series(query);

    CHECK(results.size() == 1);
    CHECK(results[0].series_uid == "1.2.3.1");
}

// ============================================================================
// Series Delete Tests
// ============================================================================

TEST_CASE("index_database: delete series", "[storage][series]") {
    auto db = create_test_database();
    auto patient_pk = create_test_patient(*db);
    auto study_pk = create_test_study(*db, patient_pk);

    REQUIRE(db->upsert_series(study_pk, "1.2.3.4.5.6.7.1", "CT").is_ok());
    CHECK(db->series_count() == 1);

    auto result = db->delete_series("1.2.3.4.5.6.7.1");
    REQUIRE(result.is_ok());

    CHECK(db->series_count() == 0);
    CHECK_FALSE(db->find_series("1.2.3.4.5.6.7.1").has_value());
}

TEST_CASE("index_database: delete non-existent series", "[storage][series]") {
    auto db = create_test_database();

    // Should not error
    auto result = db->delete_series("nonexistent");
    CHECK(result.is_ok());
}

// ============================================================================
// Series Count Tests
// ============================================================================

TEST_CASE("index_database: series count", "[storage][series]") {
    auto db = create_test_database();
    auto patient_pk = create_test_patient(*db);
    auto study_pk = create_test_study(*db, patient_pk);

    CHECK(db->series_count() == 0);

    REQUIRE(db->upsert_series(study_pk, "1.2.3.1", "CT").is_ok());
    CHECK(db->series_count() == 1);

    REQUIRE(db->upsert_series(study_pk, "1.2.3.2", "CT").is_ok());
    CHECK(db->series_count() == 2);

    REQUIRE(db->delete_series("1.2.3.1").is_ok());
    CHECK(db->series_count() == 1);
}

TEST_CASE("index_database: series count for study", "[storage][series]") {
    auto db = create_test_database();
    auto patient_pk = create_test_patient(*db);
    auto study1_pk = create_test_study(*db, patient_pk, "1.2.3.4.5.6.7");
    auto study2_pk = create_test_study(*db, patient_pk, "1.2.3.4.5.6.8");

    REQUIRE(db->upsert_series(study1_pk, "1.2.3.1", "CT").is_ok());
    REQUIRE(db->upsert_series(study1_pk, "1.2.3.2", "CT").is_ok());
    REQUIRE(db->upsert_series(study2_pk, "1.2.3.3", "MR").is_ok());

    CHECK(db->series_count("1.2.3.4.5.6.7") == 2);
    CHECK(db->series_count("1.2.3.4.5.6.8") == 1);
    CHECK(db->series_count("9.9.9.9.9.9.9") == 0);
}

// ============================================================================
// Series Record Tests
// ============================================================================

TEST_CASE("series_record: is_valid", "[storage][series]") {
    series_record record;

    CHECK_FALSE(record.is_valid());

    record.series_uid = "1.2.3.4.5.6.7.1";
    CHECK(record.is_valid());
}

TEST_CASE("series_query: has_criteria", "[storage][series]") {
    series_query query;

    CHECK_FALSE(query.has_criteria());

    query.modality = "CT";
    CHECK(query.has_criteria());
}

// ============================================================================
// Study-Series Cascade Tests
// ============================================================================

TEST_CASE("index_database: delete study cascades to series",
          "[storage][cascade]") {
    auto db = create_test_database();
    auto patient_pk = create_test_patient(*db);
    auto study_pk = create_test_study(*db, patient_pk, "1.2.3.4.5.6.7");

    REQUIRE(db->upsert_series(study_pk, "1.2.3.1", "CT").is_ok());
    REQUIRE(db->upsert_series(study_pk, "1.2.3.2", "CT").is_ok());

    CHECK(db->series_count() == 2);

    // Delete study should cascade to series
    REQUIRE(db->delete_study("1.2.3.4.5.6.7").is_ok());

    CHECK(db->series_count() == 0);
    CHECK_FALSE(db->find_series("1.2.3.1").has_value());
    CHECK_FALSE(db->find_series("1.2.3.2").has_value());
}

// ============================================================================
// Series Parent Count Update Tests (via trigger)
// ============================================================================

TEST_CASE("index_database: series insert updates study num_series",
          "[storage][series][trigger]") {
    auto db = create_test_database();
    auto patient_pk = create_test_patient(*db);
    auto study_pk = create_test_study(*db, patient_pk, "1.2.3.4.5.6.7");

    // Initially num_series should be 0
    auto study_before = db->find_study("1.2.3.4.5.6.7");
    REQUIRE(study_before.has_value());
    CHECK(study_before->num_series == 0);

    // Insert series
    REQUIRE(db->upsert_series(study_pk, "1.2.3.1", "CT").is_ok());

    // num_series should be updated
    auto study_after = db->find_study("1.2.3.4.5.6.7");
    REQUIRE(study_after.has_value());
    CHECK(study_after->num_series == 1);

    // Insert another series
    REQUIRE(db->upsert_series(study_pk, "1.2.3.2", "CT").is_ok());

    study_after = db->find_study("1.2.3.4.5.6.7");
    REQUIRE(study_after.has_value());
    CHECK(study_after->num_series == 2);
}

TEST_CASE("index_database: series delete updates study num_series",
          "[storage][series][trigger]") {
    auto db = create_test_database();
    auto patient_pk = create_test_patient(*db);
    auto study_pk = create_test_study(*db, patient_pk, "1.2.3.4.5.6.7");

    REQUIRE(db->upsert_series(study_pk, "1.2.3.1", "CT").is_ok());
    REQUIRE(db->upsert_series(study_pk, "1.2.3.2", "CT").is_ok());

    auto study = db->find_study("1.2.3.4.5.6.7");
    REQUIRE(study.has_value());
    CHECK(study->num_series == 2);

    // Delete one series
    REQUIRE(db->delete_series("1.2.3.1").is_ok());

    study = db->find_study("1.2.3.4.5.6.7");
    REQUIRE(study.has_value());
    CHECK(study->num_series == 1);
}

// ============================================================================
// Modalities In Study Update Tests
// ============================================================================

TEST_CASE("index_database: series insert updates modalities_in_study",
          "[storage][series]") {
    auto db = create_test_database();
    auto patient_pk = create_test_patient(*db);
    auto study_pk = create_test_study(*db, patient_pk, "1.2.3.4.5.6.7");

    // Insert CT series
    REQUIRE(db->upsert_series(study_pk, "1.2.3.1", "CT").is_ok());

    auto study = db->find_study("1.2.3.4.5.6.7");
    REQUIRE(study.has_value());
    CHECK(study->modalities_in_study == "CT");

    // Insert MR series
    REQUIRE(db->upsert_series(study_pk, "1.2.3.2", "MR").is_ok());

    study = db->find_study("1.2.3.4.5.6.7");
    REQUIRE(study.has_value());
    // modalities_in_study should contain both CT and MR
    CHECK(study->modalities_in_study.find("CT") != std::string::npos);
    CHECK(study->modalities_in_study.find("MR") != std::string::npos);
}

// ============================================================================
// Series Ordering Tests
// ============================================================================

TEST_CASE("index_database: series ordering by series_number",
          "[storage][series]") {
    auto db = create_test_database();
    auto patient_pk = create_test_patient(*db);
    auto study_pk = create_test_study(*db, patient_pk, "1.2.3.4.5.6.7");

    // Insert out of order
    REQUIRE(db->upsert_series(study_pk, "1.2.3.3", "CT", 3).is_ok());
    REQUIRE(db->upsert_series(study_pk, "1.2.3.1", "CT", 1).is_ok());
    REQUIRE(db->upsert_series(study_pk, "1.2.3.2", "CT", 2).is_ok());

    auto series_list = db->list_series("1.2.3.4.5.6.7");

    REQUIRE(series_list.size() == 3);
    CHECK(series_list[0].series_number.value_or(-1) == 1);
    CHECK(series_list[1].series_number.value_or(-1) == 2);
    CHECK(series_list[2].series_number.value_or(-1) == 3);
}
