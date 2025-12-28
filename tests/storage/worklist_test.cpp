/**
 * @file worklist_test.cpp
 * @brief Unit tests for worklist operations
 *
 * Tests CRUD operations for the worklist table as specified in DES-DB-006.
 */

#include <pacs/storage/index_database.hpp>
#include <pacs/storage/worklist_record.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
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
 * @brief Create a basic worklist item for testing
 */
auto create_test_item() -> worklist_item {
    worklist_item item;
    item.step_id = "SPS001";
    item.patient_id = "PAT001";
    item.patient_name = "Doe^John";
    item.birth_date = "19800115";
    item.sex = "M";
    item.accession_no = "ACC001";
    item.requested_proc_id = "RP001";
    item.study_uid = "1.2.3.4.5.6.7.8.9";
    item.scheduled_datetime = "20231115093000";
    item.station_ae = "CT_SCANNER_1";
    item.station_name = "CT Scanner Room 1";
    item.modality = "CT";
    item.procedure_desc = "CT Chest with Contrast";
    item.referring_phys = "Smith^Jane^Dr";
    item.referring_phys_id = "DR001";
    return item;
}

}  // namespace

// ============================================================================
// Worklist Insert Tests
// ============================================================================

TEST_CASE("worklist: add basic item", "[storage][worklist]") {
    auto db = create_test_database();

    auto item = create_test_item();
    auto result = db->add_worklist_item(item);

    REQUIRE(result.is_ok());
    CHECK(result.value() > 0);

    // Verify worklist item was inserted
    auto found = db->find_worklist_item("SPS001", "ACC001");
    REQUIRE(found.has_value());
    CHECK(found->step_id == "SPS001");
    CHECK(found->patient_id == "PAT001");
    CHECK(found->patient_name == "Doe^John");
    CHECK(found->modality == "CT");
    CHECK(found->step_status == "SCHEDULED");
}

TEST_CASE("worklist: add item requires step_id", "[storage][worklist]") {
    auto db = create_test_database();

    auto item = create_test_item();
    item.step_id = "";
    auto result = db->add_worklist_item(item);

    REQUIRE(result.is_err());
    CHECK(result.error().message.find("Step ID is required") !=
          std::string::npos);
}

TEST_CASE("worklist: add item requires patient_id", "[storage][worklist]") {
    auto db = create_test_database();

    auto item = create_test_item();
    item.patient_id = "";
    auto result = db->add_worklist_item(item);

    REQUIRE(result.is_err());
    CHECK(result.error().message.find("Patient ID is required") !=
          std::string::npos);
}

TEST_CASE("worklist: add item requires modality", "[storage][worklist]") {
    auto db = create_test_database();

    auto item = create_test_item();
    item.modality = "";
    auto result = db->add_worklist_item(item);

    REQUIRE(result.is_err());
    CHECK(result.error().message.find("Modality is required") !=
          std::string::npos);
}

TEST_CASE("worklist: add item requires scheduled_datetime",
          "[storage][worklist]") {
    auto db = create_test_database();

    auto item = create_test_item();
    item.scheduled_datetime = "";
    auto result = db->add_worklist_item(item);

    REQUIRE(result.is_err());
    CHECK(result.error().message.find("Scheduled datetime is required") !=
          std::string::npos);
}

TEST_CASE("worklist: multiple items with unique constraint",
          "[storage][worklist]") {
    auto db = create_test_database();

    auto item1 = create_test_item();
    auto item2 = create_test_item();
    item2.step_id = "SPS002";

    REQUIRE(db->add_worklist_item(item1).is_ok());
    REQUIRE(db->add_worklist_item(item2).is_ok());

    // Same step_id + accession_no should fail
    auto item3 = create_test_item();
    auto result = db->add_worklist_item(item3);
    REQUIRE(result.is_err());
}

// ============================================================================
// Worklist Query Tests
// ============================================================================

TEST_CASE("worklist: query all scheduled items", "[storage][worklist]") {
    auto db = create_test_database();

    // Add multiple items
    for (int i = 1; i <= 5; ++i) {
        auto item = create_test_item();
        item.step_id = "SPS" + std::to_string(i);
        item.accession_no = "ACC" + std::to_string(i);
        REQUIRE(db->add_worklist_item(item).is_ok());
    }

    worklist_query query;
    auto results = db->query_worklist(query).value();

    CHECK(results.size() == 5);
}

TEST_CASE("worklist: query by station_ae", "[storage][worklist]") {
    auto db = create_test_database();

    // Add items for different stations
    auto item1 = create_test_item();
    item1.station_ae = "CT_SCANNER_1";
    REQUIRE(db->add_worklist_item(item1).is_ok());

    auto item2 = create_test_item();
    item2.step_id = "SPS002";
    item2.accession_no = "ACC002";
    item2.station_ae = "MR_SCANNER_1";
    REQUIRE(db->add_worklist_item(item2).is_ok());

    worklist_query query;
    query.station_ae = "CT_SCANNER_1";
    auto results = db->query_worklist(query).value();

    CHECK(results.size() == 1);
    CHECK(results[0].station_ae == "CT_SCANNER_1");
}

TEST_CASE("worklist: query by modality", "[storage][worklist]") {
    auto db = create_test_database();

    // Add items for different modalities
    auto item1 = create_test_item();
    item1.modality = "CT";
    REQUIRE(db->add_worklist_item(item1).is_ok());

    auto item2 = create_test_item();
    item2.step_id = "SPS002";
    item2.accession_no = "ACC002";
    item2.modality = "MR";
    REQUIRE(db->add_worklist_item(item2).is_ok());

    worklist_query query;
    query.modality = "MR";
    auto results = db->query_worklist(query).value();

    CHECK(results.size() == 1);
    CHECK(results[0].modality == "MR");
}

TEST_CASE("worklist: query by scheduled date range", "[storage][worklist]") {
    auto db = create_test_database();

    // Add items for different dates
    auto item1 = create_test_item();
    item1.scheduled_datetime = "20231115093000";
    REQUIRE(db->add_worklist_item(item1).is_ok());

    auto item2 = create_test_item();
    item2.step_id = "SPS002";
    item2.accession_no = "ACC002";
    item2.scheduled_datetime = "20231116100000";
    REQUIRE(db->add_worklist_item(item2).is_ok());

    auto item3 = create_test_item();
    item3.step_id = "SPS003";
    item3.accession_no = "ACC003";
    item3.scheduled_datetime = "20231117140000";
    REQUIRE(db->add_worklist_item(item3).is_ok());

    worklist_query query;
    query.scheduled_date_from = "20231116";
    query.scheduled_date_to = "20231116";
    auto results = db->query_worklist(query).value();

    CHECK(results.size() == 1);
    CHECK(results[0].step_id == "SPS002");
}

TEST_CASE("worklist: query by patient_id wildcard", "[storage][worklist]") {
    auto db = create_test_database();

    auto item1 = create_test_item();
    item1.patient_id = "PAT001";
    REQUIRE(db->add_worklist_item(item1).is_ok());

    auto item2 = create_test_item();
    item2.step_id = "SPS002";
    item2.accession_no = "ACC002";
    item2.patient_id = "PAT002";
    REQUIRE(db->add_worklist_item(item2).is_ok());

    auto item3 = create_test_item();
    item3.step_id = "SPS003";
    item3.accession_no = "ACC003";
    item3.patient_id = "TEST001";
    REQUIRE(db->add_worklist_item(item3).is_ok());

    worklist_query query;
    query.patient_id = "PAT*";
    auto results = db->query_worklist(query).value();

    CHECK(results.size() == 2);
}

TEST_CASE("worklist: query only returns scheduled by default",
          "[storage][worklist]") {
    auto db = create_test_database();

    auto item = create_test_item();
    REQUIRE(db->add_worklist_item(item).is_ok());

    // Update status to STARTED
    REQUIRE(db->update_worklist_status("SPS001", "ACC001", "STARTED").is_ok());

    // Default query should not return it
    worklist_query query;
    auto results = db->query_worklist(query).value();
    CHECK(results.empty());

    // With include_all_status, it should return
    query.include_all_status = true;
    results = db->query_worklist(query).value();
    CHECK(results.size() == 1);
    CHECK(results[0].step_status == "STARTED");
}

TEST_CASE("worklist: query with limit and offset", "[storage][worklist]") {
    auto db = create_test_database();

    // Add 10 items
    for (int i = 1; i <= 10; ++i) {
        auto item = create_test_item();
        item.step_id = "SPS" + std::to_string(i);
        item.accession_no = "ACC" + std::to_string(i);
        item.scheduled_datetime =
            "2023111" + std::to_string(i % 10) + "093000";
        REQUIRE(db->add_worklist_item(item).is_ok());
    }

    worklist_query query;
    query.limit = 3;
    auto results = db->query_worklist(query).value();
    CHECK(results.size() == 3);

    query.offset = 5;
    results = db->query_worklist(query).value();
    CHECK(results.size() == 3);
}

// ============================================================================
// Worklist Status Update Tests
// ============================================================================

TEST_CASE("worklist: update status to STARTED", "[storage][worklist]") {
    auto db = create_test_database();

    auto item = create_test_item();
    REQUIRE(db->add_worklist_item(item).is_ok());

    auto result = db->update_worklist_status("SPS001", "ACC001", "STARTED");
    REQUIRE(result.is_ok());

    auto found = db->find_worklist_item("SPS001", "ACC001");
    REQUIRE(found.has_value());
    CHECK(found->step_status == "STARTED");
}

TEST_CASE("worklist: update status to COMPLETED", "[storage][worklist]") {
    auto db = create_test_database();

    auto item = create_test_item();
    REQUIRE(db->add_worklist_item(item).is_ok());

    auto result = db->update_worklist_status("SPS001", "ACC001", "COMPLETED");
    REQUIRE(result.is_ok());

    auto found = db->find_worklist_item("SPS001", "ACC001");
    REQUIRE(found.has_value());
    CHECK(found->step_status == "COMPLETED");
}

TEST_CASE("worklist: invalid status update fails", "[storage][worklist]") {
    auto db = create_test_database();

    auto item = create_test_item();
    REQUIRE(db->add_worklist_item(item).is_ok());

    auto result = db->update_worklist_status("SPS001", "ACC001", "INVALID");
    REQUIRE(result.is_err());
    CHECK(result.error().message.find("Invalid status") != std::string::npos);
}

// ============================================================================
// Worklist Find Tests
// ============================================================================

TEST_CASE("worklist: find by step_id and accession_no", "[storage][worklist]") {
    auto db = create_test_database();

    auto item = create_test_item();
    REQUIRE(db->add_worklist_item(item).is_ok());

    auto found = db->find_worklist_item("SPS001", "ACC001");
    REQUIRE(found.has_value());
    CHECK(found->step_id == "SPS001");
    CHECK(found->accession_no == "ACC001");
}

TEST_CASE("worklist: find non-existent item returns nullopt",
          "[storage][worklist]") {
    auto db = create_test_database();

    auto found = db->find_worklist_item("NONEXISTENT", "ACCXXX");
    CHECK(!found.has_value());
}

TEST_CASE("worklist: find by primary key", "[storage][worklist]") {
    auto db = create_test_database();

    auto item = create_test_item();
    auto result = db->add_worklist_item(item);
    REQUIRE(result.is_ok());

    auto pk = result.value();
    auto found = db->find_worklist_by_pk(pk);
    REQUIRE(found.has_value());
    CHECK(found->step_id == "SPS001");
}

// ============================================================================
// Worklist Delete Tests
// ============================================================================

TEST_CASE("worklist: delete item", "[storage][worklist]") {
    auto db = create_test_database();

    auto item = create_test_item();
    REQUIRE(db->add_worklist_item(item).is_ok());

    auto result = db->delete_worklist_item("SPS001", "ACC001");
    REQUIRE(result.is_ok());

    auto found = db->find_worklist_item("SPS001", "ACC001");
    CHECK(!found.has_value());
}

TEST_CASE("worklist: delete non-existent item succeeds",
          "[storage][worklist]") {
    auto db = create_test_database();

    // Delete without adding - should succeed (no error, just no rows affected)
    auto result = db->delete_worklist_item("NONEXISTENT", "ACCXXX");
    CHECK(result.is_ok());
}

// ============================================================================
// Worklist Count Tests
// ============================================================================

TEST_CASE("worklist: count all items", "[storage][worklist]") {
    auto db = create_test_database();

    CHECK(db->worklist_count().value() == 0);

    for (int i = 1; i <= 5; ++i) {
        auto item = create_test_item();
        item.step_id = "SPS" + std::to_string(i);
        item.accession_no = "ACC" + std::to_string(i);
        REQUIRE(db->add_worklist_item(item).is_ok());
    }

    CHECK(db->worklist_count().value() == 5);
}

TEST_CASE("worklist: count by status", "[storage][worklist]") {
    auto db = create_test_database();

    // Add 3 scheduled items
    for (int i = 1; i <= 3; ++i) {
        auto item = create_test_item();
        item.step_id = "SPS" + std::to_string(i);
        item.accession_no = "ACC" + std::to_string(i);
        REQUIRE(db->add_worklist_item(item).is_ok());
    }

    // Update one to STARTED
    REQUIRE(db->update_worklist_status("SPS1", "ACC1", "STARTED").is_ok());

    // Update one to COMPLETED
    REQUIRE(db->update_worklist_status("SPS2", "ACC2", "COMPLETED").is_ok());

    CHECK(db->worklist_count("SCHEDULED").value() == 1);
    CHECK(db->worklist_count("STARTED").value() == 1);
    CHECK(db->worklist_count("COMPLETED").value() == 1);
}

// ============================================================================
// Worklist Record Helper Tests
// ============================================================================

TEST_CASE("worklist_status: to_string conversion", "[storage][worklist]") {
    CHECK(to_string(worklist_status::scheduled) == "SCHEDULED");
    CHECK(to_string(worklist_status::started) == "STARTED");
    CHECK(to_string(worklist_status::completed) == "COMPLETED");
}

TEST_CASE("worklist_status: parse_worklist_status", "[storage][worklist]") {
    auto scheduled = parse_worklist_status("SCHEDULED");
    REQUIRE(scheduled.has_value());
    CHECK(*scheduled == worklist_status::scheduled);

    auto started = parse_worklist_status("STARTED");
    REQUIRE(started.has_value());
    CHECK(*started == worklist_status::started);

    auto completed = parse_worklist_status("COMPLETED");
    REQUIRE(completed.has_value());
    CHECK(*completed == worklist_status::completed);

    auto invalid = parse_worklist_status("INVALID");
    CHECK(!invalid.has_value());
}

TEST_CASE("worklist_item: is_valid check", "[storage][worklist]") {
    worklist_item item;
    CHECK(!item.is_valid());

    item.step_id = "SPS001";
    CHECK(!item.is_valid());

    item.patient_id = "PAT001";
    CHECK(!item.is_valid());

    item.modality = "CT";
    CHECK(!item.is_valid());

    item.scheduled_datetime = "20231115093000";
    CHECK(item.is_valid());
}

TEST_CASE("worklist_item: is_scheduled check", "[storage][worklist]") {
    worklist_item item;
    CHECK(item.is_scheduled());  // Empty status is treated as SCHEDULED

    item.step_status = "SCHEDULED";
    CHECK(item.is_scheduled());

    item.step_status = "STARTED";
    CHECK(!item.is_scheduled());

    item.step_status = "COMPLETED";
    CHECK(!item.is_scheduled());
}

TEST_CASE("worklist_query: has_criteria check", "[storage][worklist]") {
    worklist_query query;
    CHECK(!query.has_criteria());

    query.station_ae = "CT_SCANNER_1";
    CHECK(query.has_criteria());

    worklist_query query2;
    query2.modality = "CT";
    CHECK(query2.has_criteria());

    worklist_query query3;
    query3.patient_id = "PAT*";
    CHECK(query3.has_criteria());
}

// ============================================================================
// Worklist Cleanup Tests
// ============================================================================

TEST_CASE("worklist: cleanup_old_worklist_items removes old non-scheduled items",
          "[storage][worklist][cleanup]") {
    auto db = create_test_database();

    // Add items
    for (int i = 1; i <= 3; ++i) {
        auto item = create_test_item();
        item.step_id = "SPS" + std::to_string(i);
        item.accession_no = "ACC" + std::to_string(i);
        REQUIRE(db->add_worklist_item(item).is_ok());
    }

    // Mark one as COMPLETED (eligible for cleanup)
    REQUIRE(db->update_worklist_status("SPS1", "ACC1", "COMPLETED").is_ok());

    // Cleanup with 0 hours should remove the completed item
    auto result = db->cleanup_old_worklist_items(std::chrono::hours(0));
    REQUIRE(result.is_ok());
    CHECK(result.value() == 1);

    // SCHEDULED items should remain
    CHECK(db->worklist_count("SCHEDULED").value() == 2);
}

TEST_CASE("worklist: cleanup_old_worklist_items preserves scheduled items",
          "[storage][worklist][cleanup]") {
    auto db = create_test_database();

    // Add a scheduled item
    auto item = create_test_item();
    REQUIRE(db->add_worklist_item(item).is_ok());

    // Try cleanup with 0 hours - should not delete scheduled items
    auto result = db->cleanup_old_worklist_items(std::chrono::hours(0));
    REQUIRE(result.is_ok());
    CHECK(result.value() == 0);

    // Item should still exist
    auto found = db->find_worklist_item("SPS001", "ACC001");
    REQUIRE(found.has_value());
    CHECK(found->step_status == "SCHEDULED");
}

TEST_CASE("worklist: cleanup_worklist_items_before removes items by date",
          "[storage][worklist][cleanup]") {
    auto db = create_test_database();

    // Add items with different scheduled dates
    auto item1 = create_test_item();
    item1.scheduled_datetime = "2023-10-15 09:30:00";  // Old
    REQUIRE(db->add_worklist_item(item1).is_ok());

    auto item2 = create_test_item();
    item2.step_id = "SPS002";
    item2.accession_no = "ACC002";
    item2.scheduled_datetime = "2024-12-15 09:30:00";  // Future
    REQUIRE(db->add_worklist_item(item2).is_ok());

    // Mark both as COMPLETED (eligible for cleanup)
    REQUIRE(db->update_worklist_status("SPS001", "ACC001", "COMPLETED").is_ok());
    REQUIRE(db->update_worklist_status("SPS002", "ACC002", "COMPLETED").is_ok());

    // Cleanup items scheduled before 2024-01-01
    std::tm tm = {};
    tm.tm_year = 2024 - 1900;  // Year since 1900
    tm.tm_mon = 0;             // January (0-based)
    tm.tm_mday = 1;
    auto cutoff_time = std::mktime(&tm);
    auto cutoff = std::chrono::system_clock::from_time_t(cutoff_time);

    auto result = db->cleanup_worklist_items_before(cutoff);
    REQUIRE(result.is_ok());
    CHECK(result.value() == 1);

    // Only the future item should remain
    CHECK(db->worklist_count().value() == 1);
    auto found = db->find_worklist_item("SPS002", "ACC002");
    REQUIRE(found.has_value());
}

TEST_CASE("worklist: cleanup_worklist_items_before preserves scheduled items",
          "[storage][worklist][cleanup]") {
    auto db = create_test_database();

    // Add an old scheduled item
    auto item = create_test_item();
    item.scheduled_datetime = "2023-10-15 09:30:00";  // Old but SCHEDULED
    REQUIRE(db->add_worklist_item(item).is_ok());

    // Cleanup items before now - should preserve SCHEDULED
    auto now = std::chrono::system_clock::now();
    auto result = db->cleanup_worklist_items_before(now);
    REQUIRE(result.is_ok());
    CHECK(result.value() == 0);

    // Scheduled item should still exist
    auto found = db->find_worklist_item("SPS001", "ACC001");
    REQUIRE(found.has_value());
    CHECK(found->step_status == "SCHEDULED");
}

TEST_CASE("worklist: cleanup_worklist_items_before exact boundary",
          "[storage][worklist][cleanup]") {
    auto db = create_test_database();

    // Add item at exact boundary time
    auto item = create_test_item();
    item.scheduled_datetime = "2024-06-15 12:00:00";  // Exactly at boundary
    REQUIRE(db->add_worklist_item(item).is_ok());
    REQUIRE(db->update_worklist_status("SPS001", "ACC001", "COMPLETED").is_ok());

    // Cleanup before exact same time - should NOT delete (strictly less than)
    std::tm tm = {};
    tm.tm_year = 2024 - 1900;
    tm.tm_mon = 5;   // June (0-based)
    tm.tm_mday = 15;
    tm.tm_hour = 12;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    auto boundary_time = std::mktime(&tm);
    auto boundary = std::chrono::system_clock::from_time_t(boundary_time);

    auto result = db->cleanup_worklist_items_before(boundary);
    REQUIRE(result.is_ok());
    CHECK(result.value() == 0);

    // Cleanup 1 second later - should delete
    auto after_boundary = boundary + std::chrono::seconds(1);
    result = db->cleanup_worklist_items_before(after_boundary);
    REQUIRE(result.is_ok());
    CHECK(result.value() == 1);
}
