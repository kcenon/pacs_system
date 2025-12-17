/**
 * @file mpps_test.cpp
 * @brief Unit tests for index_database MPPS operations
 *
 * Tests CRUD operations for the mpps table as specified in DES-DB-005.
 * Validates MPPS state machine transitions and N-CREATE/N-SET behavior.
 */

#include <pacs/storage/index_database.hpp>

#include <catch2/catch_test_macros.hpp>

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

/**
 * @brief Helper to create a test MPPS and return its UID
 */
auto create_test_mpps(index_database& db, const std::string& uid,
                      const std::string& station = "CT_SCANNER_1",
                      const std::string& modality = "CT") -> int64_t {
    auto result = db.create_mpps(uid, station, modality, "1.2.3.4.5", "ACC001",
                                 "20231115120000");
    REQUIRE(result.is_ok());
    return result.value();
}

}  // namespace

// ============================================================================
// MPPS Creation Tests (N-CREATE)
// ============================================================================

TEST_CASE("MPPS: create with basic info", "[storage][mpps]") {
    auto db = create_test_database();

    auto result = db->create_mpps("1.2.3.4.5.100", "CT_SCANNER_1", "CT",
                                  "1.2.3.4.5", "ACC001", "20231115120000");

    REQUIRE(result.is_ok());
    CHECK(result.value() > 0);

    // Verify MPPS was created with IN PROGRESS status
    auto mpps = db->find_mpps("1.2.3.4.5.100");
    REQUIRE(mpps.has_value());
    CHECK(mpps->mpps_uid == "1.2.3.4.5.100");
    CHECK(mpps->status == "IN PROGRESS");
    CHECK(mpps->station_ae == "CT_SCANNER_1");
    CHECK(mpps->modality == "CT");
    CHECK(mpps->study_uid == "1.2.3.4.5");
    CHECK(mpps->accession_no == "ACC001");
    CHECK(mpps->start_datetime == "20231115120000");
}

TEST_CASE("MPPS: create with full record", "[storage][mpps]") {
    auto db = create_test_database();

    mpps_record record;
    record.mpps_uid = "1.2.3.4.5.200";
    record.station_ae = "MR_SCANNER_1";
    record.station_name = "MR Unit 1";
    record.modality = "MR";
    record.study_uid = "1.2.3.4.6";
    record.accession_no = "ACC002";
    record.scheduled_step_id = "STEP001";
    record.requested_proc_id = "PROC001";
    record.start_datetime = "20231115140000";

    auto result = db->create_mpps(record);

    REQUIRE(result.is_ok());

    auto mpps = db->find_mpps("1.2.3.4.5.200");
    REQUIRE(mpps.has_value());
    CHECK(mpps->status == "IN PROGRESS");
    CHECK(mpps->station_name == "MR Unit 1");
    CHECK(mpps->scheduled_step_id == "STEP001");
}

TEST_CASE("MPPS: create requires UID", "[storage][mpps]") {
    auto db = create_test_database();

    auto result = db->create_mpps("", "CT_SCANNER_1", "CT");

    REQUIRE(result.is_err());
    CHECK(result.error().message.find("required") != std::string::npos);
}

TEST_CASE("MPPS: create with invalid status fails", "[storage][mpps]") {
    auto db = create_test_database();

    mpps_record record;
    record.mpps_uid = "1.2.3.4.5.300";
    record.status = "COMPLETED";  // Invalid for N-CREATE

    auto result = db->create_mpps(record);

    REQUIRE(result.is_err());
    CHECK(result.error().message.find("IN PROGRESS") != std::string::npos);
}

TEST_CASE("MPPS: create duplicate UID fails", "[storage][mpps]") {
    auto db = create_test_database();

    // Create first MPPS
    auto result1 = db->create_mpps("1.2.3.4.5.100", "CT_SCANNER_1", "CT");
    REQUIRE(result1.is_ok());

    // Try to create duplicate
    auto result2 = db->create_mpps("1.2.3.4.5.100", "MR_SCANNER_1", "MR");
    REQUIRE(result2.is_err());
}

// ============================================================================
// MPPS Update Tests (N-SET)
// ============================================================================

TEST_CASE("MPPS: update to COMPLETED", "[storage][mpps]") {
    auto db = create_test_database();
    create_test_mpps(*db, "1.2.3.4.5.100");

    auto result = db->update_mpps("1.2.3.4.5.100", "COMPLETED", "20231115130000",
                                  R"([{"series_uid": "1.2.3.4.5.1", "num_instances": 50}])");

    REQUIRE(result.is_ok());

    auto mpps = db->find_mpps("1.2.3.4.5.100");
    REQUIRE(mpps.has_value());
    CHECK(mpps->status == "COMPLETED");
    CHECK(mpps->end_datetime == "20231115130000");
    CHECK(mpps->performed_series.find("series_uid") != std::string::npos);
}

TEST_CASE("MPPS: update to DISCONTINUED", "[storage][mpps]") {
    auto db = create_test_database();
    create_test_mpps(*db, "1.2.3.4.5.100");

    auto result =
        db->update_mpps("1.2.3.4.5.100", "DISCONTINUED", "20231115131500");

    REQUIRE(result.is_ok());

    auto mpps = db->find_mpps("1.2.3.4.5.100");
    REQUIRE(mpps.has_value());
    CHECK(mpps->status == "DISCONTINUED");
}

TEST_CASE("MPPS: cannot update COMPLETED to another status", "[storage][mpps]") {
    auto db = create_test_database();
    create_test_mpps(*db, "1.2.3.4.5.100");

    // Complete the MPPS
    auto complete_result =
        db->update_mpps("1.2.3.4.5.100", "COMPLETED", "20231115130000");
    REQUIRE(complete_result.is_ok());

    // Try to update to IN PROGRESS - should fail
    auto result = db->update_mpps("1.2.3.4.5.100", "IN PROGRESS");
    REQUIRE(result.is_err());
    CHECK(result.error().message.find("final state") != std::string::npos);
}

TEST_CASE("MPPS: cannot update DISCONTINUED to another status",
          "[storage][mpps]") {
    auto db = create_test_database();
    create_test_mpps(*db, "1.2.3.4.5.100");

    // Discontinue the MPPS
    auto disc_result =
        db->update_mpps("1.2.3.4.5.100", "DISCONTINUED", "20231115130000");
    REQUIRE(disc_result.is_ok());

    // Try to update to COMPLETED - should fail
    auto result = db->update_mpps("1.2.3.4.5.100", "COMPLETED");
    REQUIRE(result.is_err());
    CHECK(result.error().message.find("final state") != std::string::npos);
}

TEST_CASE("MPPS: update with invalid status fails", "[storage][mpps]") {
    auto db = create_test_database();
    create_test_mpps(*db, "1.2.3.4.5.100");

    auto result = db->update_mpps("1.2.3.4.5.100", "INVALID_STATUS");
    REQUIRE(result.is_err());
}

TEST_CASE("MPPS: update non-existent MPPS fails", "[storage][mpps]") {
    auto db = create_test_database();

    auto result = db->update_mpps("1.2.3.4.5.999", "COMPLETED");
    REQUIRE(result.is_err());
    CHECK(result.error().message.find("not found") != std::string::npos);
}

// ============================================================================
// MPPS Find Tests
// ============================================================================

TEST_CASE("MPPS: find by UID", "[storage][mpps]") {
    auto db = create_test_database();
    auto pk = create_test_mpps(*db, "1.2.3.4.5.100");

    auto mpps = db->find_mpps("1.2.3.4.5.100");

    REQUIRE(mpps.has_value());
    CHECK(mpps->pk == pk);
    CHECK(mpps->mpps_uid == "1.2.3.4.5.100");
}

TEST_CASE("MPPS: find by pk", "[storage][mpps]") {
    auto db = create_test_database();
    auto pk = create_test_mpps(*db, "1.2.3.4.5.100");

    auto mpps = db->find_mpps_by_pk(pk);

    REQUIRE(mpps.has_value());
    CHECK(mpps->mpps_uid == "1.2.3.4.5.100");
}

TEST_CASE("MPPS: find non-existent returns empty", "[storage][mpps]") {
    auto db = create_test_database();

    auto mpps = db->find_mpps("1.2.3.4.5.999");
    CHECK_FALSE(mpps.has_value());

    auto mpps_by_pk = db->find_mpps_by_pk(999);
    CHECK_FALSE(mpps_by_pk.has_value());
}

// ============================================================================
// MPPS List and Search Tests
// ============================================================================

TEST_CASE("MPPS: list active by station", "[storage][mpps]") {
    auto db = create_test_database();

    // Create multiple MPPS for different stations
    create_test_mpps(*db, "1.2.3.4.5.100", "CT_SCANNER_1", "CT");
    create_test_mpps(*db, "1.2.3.4.5.101", "CT_SCANNER_1", "CT");
    create_test_mpps(*db, "1.2.3.4.5.102", "MR_SCANNER_1", "MR");

    // Complete one CT MPPS
    REQUIRE(db->update_mpps("1.2.3.4.5.100", "COMPLETED", "20231115130000").is_ok());

    // List active MPPS for CT_SCANNER_1
    auto results = db->list_active_mpps("CT_SCANNER_1").value();

    CHECK(results.size() == 1);
    CHECK(results[0].mpps_uid == "1.2.3.4.5.101");
}

TEST_CASE("MPPS: find by study", "[storage][mpps]") {
    auto db = create_test_database();

    // Create MPPS for different studies
    REQUIRE(db->create_mpps("1.2.3.4.5.100", "CT_SCANNER_1", "CT", "1.2.3.4.5", "ACC001",
                    "20231115120000").is_ok());
    REQUIRE(db->create_mpps("1.2.3.4.5.101", "CT_SCANNER_1", "CT", "1.2.3.4.5", "ACC001",
                    "20231115120500").is_ok());  // Same study
    REQUIRE(db->create_mpps("1.2.3.4.5.102", "MR_SCANNER_1", "MR", "1.2.3.4.6", "ACC002",
                    "20231115130000").is_ok());  // Different study

    auto results = db->find_mpps_by_study("1.2.3.4.5").value();

    CHECK(results.size() == 2);
}

TEST_CASE("MPPS: search with query", "[storage][mpps]") {
    auto db = create_test_database();

    // Create test data
    REQUIRE(db->create_mpps("1.2.3.4.5.100", "CT_SCANNER_1", "CT", "1.2.3.4.5", "ACC001",
                    "20231115120000").is_ok());
    REQUIRE(db->create_mpps("1.2.3.4.5.101", "CT_SCANNER_1", "MR", "1.2.3.4.6", "ACC002",
                    "20231115130000").is_ok());
    REQUIRE(db->create_mpps("1.2.3.4.5.102", "MR_SCANNER_1", "MR", "1.2.3.4.7", "ACC003",
                    "20231116100000").is_ok());

    SECTION("search by modality") {
        mpps_query query;
        query.modality = "MR";

        auto results = db->search_mpps(query).value();
        CHECK(results.size() == 2);
    }

    SECTION("search by station") {
        mpps_query query;
        query.station_ae = "CT_SCANNER_1";

        auto results = db->search_mpps(query).value();
        CHECK(results.size() == 2);
    }

    SECTION("search by date range") {
        mpps_query query;
        query.start_date_from = "20231115";
        query.start_date_to = "20231115";

        auto results = db->search_mpps(query).value();
        CHECK(results.size() == 2);
    }

    SECTION("search with limit") {
        mpps_query query;
        query.limit = 1;

        auto results = db->search_mpps(query).value();
        CHECK(results.size() == 1);
    }

    SECTION("search by status") {
        // Complete one MPPS
        REQUIRE(db->update_mpps("1.2.3.4.5.100", "COMPLETED", "20231115130000").is_ok());

        mpps_query query;
        query.status = "IN PROGRESS";

        auto results = db->search_mpps(query).value();
        CHECK(results.size() == 2);
    }
}

// ============================================================================
// MPPS Delete Tests
// ============================================================================

TEST_CASE("MPPS: delete by UID", "[storage][mpps]") {
    auto db = create_test_database();
    create_test_mpps(*db, "1.2.3.4.5.100");

    auto result = db->delete_mpps("1.2.3.4.5.100");
    REQUIRE(result.is_ok());

    auto mpps = db->find_mpps("1.2.3.4.5.100");
    CHECK_FALSE(mpps.has_value());
}

// ============================================================================
// MPPS Count Tests
// ============================================================================

TEST_CASE("MPPS: count total", "[storage][mpps]") {
    auto db = create_test_database();

    CHECK(db->mpps_count().value() == 0);

    create_test_mpps(*db, "1.2.3.4.5.100");
    CHECK(db->mpps_count().value() == 1);

    create_test_mpps(*db, "1.2.3.4.5.101");
    CHECK(db->mpps_count().value() == 2);
}

TEST_CASE("MPPS: count by status", "[storage][mpps]") {
    auto db = create_test_database();

    create_test_mpps(*db, "1.2.3.4.5.100");
    create_test_mpps(*db, "1.2.3.4.5.101");
    create_test_mpps(*db, "1.2.3.4.5.102");

    CHECK(db->mpps_count("IN PROGRESS").value() == 3);
    CHECK(db->mpps_count("COMPLETED").value() == 0);

    // Complete one
    REQUIRE(db->update_mpps("1.2.3.4.5.100", "COMPLETED", "20231115130000").is_ok());

    CHECK(db->mpps_count("IN PROGRESS").value() == 2);
    CHECK(db->mpps_count("COMPLETED").value() == 1);

    // Discontinue one
    REQUIRE(db->update_mpps("1.2.3.4.5.101", "DISCONTINUED", "20231115131500").is_ok());

    CHECK(db->mpps_count("IN PROGRESS").value() == 1);
    CHECK(db->mpps_count("DISCONTINUED").value() == 1);
}

// ============================================================================
// MPPS Record Helper Tests
// ============================================================================

TEST_CASE("mpps_record: is_valid", "[storage][mpps]") {
    mpps_record record;
    CHECK_FALSE(record.is_valid());

    record.mpps_uid = "1.2.3.4.5.100";
    CHECK(record.is_valid());
}

TEST_CASE("mpps_record: is_final", "[storage][mpps]") {
    mpps_record record;

    record.status = "IN PROGRESS";
    CHECK_FALSE(record.is_final());

    record.status = "COMPLETED";
    CHECK(record.is_final());

    record.status = "DISCONTINUED";
    CHECK(record.is_final());
}

TEST_CASE("mpps_status: to_string and parse", "[storage][mpps]") {
    CHECK(to_string(mpps_status::in_progress) == "IN PROGRESS");
    CHECK(to_string(mpps_status::completed) == "COMPLETED");
    CHECK(to_string(mpps_status::discontinued) == "DISCONTINUED");

    auto in_progress = parse_mpps_status("IN PROGRESS");
    REQUIRE(in_progress.has_value());
    CHECK(*in_progress == mpps_status::in_progress);

    auto completed = parse_mpps_status("COMPLETED");
    REQUIRE(completed.has_value());
    CHECK(*completed == mpps_status::completed);

    auto invalid = parse_mpps_status("INVALID");
    CHECK_FALSE(invalid.has_value());
}
