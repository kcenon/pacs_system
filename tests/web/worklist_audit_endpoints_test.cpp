/**
 * @file worklist_audit_endpoints_test.cpp
 * @brief Unit tests for worklist and audit API endpoints
 *
 * @copyright Copyright (c) 2025
 * @license MIT
 */

#include <catch2/catch_test_macros.hpp>

#include "pacs/storage/audit_record.hpp"
#include "pacs/storage/index_database.hpp"
#include "pacs/storage/worklist_record.hpp"
#include "pacs/web/rest_config.hpp"
#include "pacs/web/rest_types.hpp"

using namespace pacs::storage;
using namespace pacs::web;

// ============================================================================
// Worklist Query Tests
// ============================================================================

TEST_CASE("Worklist query structure", "[web][worklist]") {
  worklist_query query;

  SECTION("default values") {
    REQUIRE(query.limit == 0);
    REQUIRE(query.offset == 0);
    REQUIRE_FALSE(query.include_all_status);
    REQUIRE_FALSE(query.station_ae.has_value());
    REQUIRE_FALSE(query.modality.has_value());
  }

  SECTION("with station_ae") {
    query.station_ae = "CT_SCANNER_1";
    REQUIRE(query.station_ae.has_value());
    REQUIRE(query.station_ae.value() == "CT_SCANNER_1");
  }

  SECTION("with modality") {
    query.modality = "CT";
    REQUIRE(query.modality.has_value());
    REQUIRE(query.modality.value() == "CT");
  }

  SECTION("with date range") {
    query.scheduled_date_from = "20231001";
    query.scheduled_date_to = "20231031";
    REQUIRE(query.scheduled_date_from.has_value());
    REQUIRE(query.scheduled_date_to.has_value());
  }

  SECTION("with patient_id") {
    query.patient_id = "12345";
    REQUIRE(query.patient_id.has_value());
    REQUIRE(query.patient_id.value() == "12345");
  }

  SECTION("with include_all_status") {
    query.include_all_status = true;
    REQUIRE(query.include_all_status);
  }
}

TEST_CASE("Worklist item validation", "[web][worklist]") {
  worklist_item item;

  SECTION("invalid when required fields are empty") {
    REQUIRE_FALSE(item.is_valid());
  }

  SECTION("valid when required fields are set") {
    item.step_id = "STEP001";
    item.patient_id = "P001";
    item.modality = "CT";
    item.scheduled_datetime = "20231015120000";
    REQUIRE(item.is_valid());
  }

  SECTION("invalid with missing step_id") {
    item.patient_id = "P001";
    item.modality = "CT";
    item.scheduled_datetime = "20231015120000";
    REQUIRE_FALSE(item.is_valid());
  }

  SECTION("invalid with missing modality") {
    item.step_id = "STEP001";
    item.patient_id = "P001";
    item.scheduled_datetime = "20231015120000";
    REQUIRE_FALSE(item.is_valid());
  }
}

TEST_CASE("Index database worklist operations", "[web][worklist][database]") {
  auto db_result = index_database::open(":memory:");
  REQUIRE(db_result.is_ok());
  auto &db = db_result.value();

  SECTION("insert and find worklist item") {
    worklist_item item;
    item.step_id = "STEP001";
    item.patient_id = "P001";
    item.patient_name = "Doe^John";
    item.modality = "CT";
    item.scheduled_datetime = "20231015120000";
    item.station_ae = "CT_SCANNER";
    item.accession_no = "ACC001";

    auto pk_result = db->add_worklist_item(item);
    REQUIRE(pk_result.is_ok());
    REQUIRE(pk_result.value() > 0);

    auto found = db->find_worklist_by_pk(pk_result.value());
    REQUIRE(found.has_value());
    REQUIRE(found->step_id == "STEP001");
    REQUIRE(found->patient_id == "P001");
    REQUIRE(found->modality == "CT");
  }

  SECTION("query worklist with filters") {
    // Insert multiple items
    worklist_item item1;
    item1.step_id = "STEP001";
    item1.patient_id = "P001";
    item1.modality = "CT";
    item1.scheduled_datetime = "20231015120000";
    item1.station_ae = "CT_SCANNER";
    (void)db->add_worklist_item(item1);

    worklist_item item2;
    item2.step_id = "STEP002";
    item2.patient_id = "P002";
    item2.modality = "MR";
    item2.scheduled_datetime = "20231015130000";
    item2.station_ae = "MR_SCANNER";
    (void)db->add_worklist_item(item2);

    // Query by modality
    worklist_query query;
    query.modality = "CT";
    auto results = db->query_worklist(query);
    REQUIRE(results.size() == 1);
    REQUIRE(results[0].modality == "CT");

    // Query by station_ae
    query.modality = std::nullopt;
    query.station_ae = "MR_SCANNER";
    results = db->query_worklist(query);
    REQUIRE(results.size() == 1);
    REQUIRE(results[0].station_ae == "MR_SCANNER");
  }

  SECTION("update worklist status") {
    worklist_item item;
    item.step_id = "STEP001";
    item.patient_id = "P001";
    item.modality = "CT";
    item.scheduled_datetime = "20231015120000";
    item.accession_no = "ACC001";

    auto pk_result = db->add_worklist_item(item);
    REQUIRE(pk_result.is_ok());

    auto update_result =
        db->update_worklist_status("STEP001", "ACC001", "STARTED");
    REQUIRE(update_result.is_ok());

    auto found = db->find_worklist_item("STEP001", "ACC001");
    REQUIRE(found.has_value());
    REQUIRE(found->step_status == "STARTED");
  }

  SECTION("delete worklist item") {
    worklist_item item;
    item.step_id = "STEP001";
    item.patient_id = "P001";
    item.modality = "CT";
    item.scheduled_datetime = "20231015120000";
    item.accession_no = "ACC001";

    (void)db->add_worklist_item(item);

    auto result = db->delete_worklist_item("STEP001", "ACC001");
    REQUIRE(result.is_ok());

    auto found = db->find_worklist_item("STEP001", "ACC001");
    REQUIRE_FALSE(found.has_value());
  }

  SECTION("worklist count") {
    worklist_item item;
    item.step_id = "STEP001";
    item.patient_id = "P001";
    item.modality = "CT";
    item.scheduled_datetime = "20231015120000";
    (void)db->add_worklist_item(item);

    item.step_id = "STEP002";
    (void)db->add_worklist_item(item);

    REQUIRE(db->worklist_count() == 2);
    REQUIRE(db->worklist_count("SCHEDULED") == 2);
  }
}

// ============================================================================
// Audit Log Tests
// ============================================================================

TEST_CASE("Audit query structure", "[web][audit]") {
  audit_query query;

  SECTION("default values") {
    REQUIRE(query.limit == 0);
    REQUIRE(query.offset == 0);
    REQUIRE_FALSE(query.has_criteria());
  }

  SECTION("has_criteria returns true when filters are set") {
    query.event_type = "C_STORE";
    REQUIRE(query.has_criteria());
  }

  SECTION("with date range") {
    query.date_from = "2023-10-01";
    query.date_to = "2023-10-31";
    REQUIRE(query.has_criteria());
  }

  SECTION("with user_id") {
    query.user_id = "admin";
    REQUIRE(query.has_criteria());
  }

  SECTION("with source_ae") {
    query.source_ae = "MODALITY1";
    REQUIRE(query.has_criteria());
  }
}

TEST_CASE("Audit record validation", "[web][audit]") {
  audit_record record;

  SECTION("invalid when event_type is empty") {
    REQUIRE_FALSE(record.is_valid());
  }

  SECTION("valid when event_type is set") {
    record.event_type = "C_STORE";
    REQUIRE(record.is_valid());
  }
}

TEST_CASE("Audit event type conversion", "[web][audit]") {
  SECTION("to_string") {
    REQUIRE(to_string(audit_event_type::c_store) == "C_STORE");
    REQUIRE(to_string(audit_event_type::c_find) == "C_FIND");
    REQUIRE(to_string(audit_event_type::c_move) == "C_MOVE");
    REQUIRE(to_string(audit_event_type::association_established) ==
            "ASSOCIATION_ESTABLISHED");
    REQUIRE(to_string(audit_event_type::security_event) == "SECURITY_EVENT");
  }

  SECTION("parse_audit_event_type") {
    auto result = parse_audit_event_type("C_STORE");
    REQUIRE(result.has_value());
    REQUIRE(result.value() == audit_event_type::c_store);

    result = parse_audit_event_type("UNKNOWN_EVENT");
    REQUIRE_FALSE(result.has_value());
  }
}

TEST_CASE("Index database audit operations", "[web][audit][database]") {
  auto db_result = index_database::open(":memory:");
  REQUIRE(db_result.is_ok());
  auto &db = db_result.value();

  SECTION("insert and find audit log") {
    audit_record record;
    record.event_type = "C_STORE";
    record.outcome = "SUCCESS";
    record.user_id = "MODALITY1";
    record.source_ae = "MODALITY1";
    record.target_ae = "PACS_SCP";
    record.patient_id = "P001";
    record.study_uid = "1.2.840.123456";
    record.message = "Image stored successfully";

    auto pk_result = db->add_audit_log(record);
    REQUIRE(pk_result.is_ok());
    REQUIRE(pk_result.value() > 0);

    auto found = db->find_audit_by_pk(pk_result.value());
    REQUIRE(found.has_value());
    REQUIRE(found->event_type == "C_STORE");
    REQUIRE(found->outcome == "SUCCESS");
    REQUIRE(found->patient_id == "P001");
  }

  SECTION("query audit logs with filters") {
    // Insert multiple records
    audit_record record1;
    record1.event_type = "C_STORE";
    record1.outcome = "SUCCESS";
    record1.source_ae = "MODALITY1";
    (void)db->add_audit_log(record1);

    audit_record record2;
    record2.event_type = "C_FIND";
    record2.outcome = "SUCCESS";
    record2.source_ae = "MODALITY2";
    (void)db->add_audit_log(record2);

    audit_record record3;
    record3.event_type = "C_STORE";
    record3.outcome = "FAILURE";
    record3.source_ae = "MODALITY1";
    (void)db->add_audit_log(record3);

    // Query by event_type
    audit_query query;
    query.event_type = "C_STORE";
    auto results = db->query_audit_log(query);
    REQUIRE(results.size() == 2);

    // Query by outcome
    query.event_type = std::nullopt;
    query.outcome = "FAILURE";
    results = db->query_audit_log(query);
    REQUIRE(results.size() == 1);
    REQUIRE(results[0].outcome == "FAILURE");

    // Query by source_ae
    query.outcome = std::nullopt;
    query.source_ae = "MODALITY2";
    results = db->query_audit_log(query);
    REQUIRE(results.size() == 1);
    REQUIRE(results[0].event_type == "C_FIND");
  }

  SECTION("audit count") {
    audit_record record;
    record.event_type = "C_STORE";
    record.outcome = "SUCCESS";
    (void)db->add_audit_log(record);
    (void)db->add_audit_log(record);
    (void)db->add_audit_log(record);

    REQUIRE(db->audit_count() == 3);
  }

  SECTION("query with pagination") {
    // Insert 10 records
    for (int i = 0; i < 10; ++i) {
      audit_record record;
      record.event_type = "C_STORE";
      record.outcome = "SUCCESS";
      record.message = "Record " + std::to_string(i);
      (void)db->add_audit_log(record);
    }

    audit_query query;
    query.limit = 5;
    query.offset = 0;
    auto page1 = db->query_audit_log(query);
    REQUIRE(page1.size() == 5);

    query.offset = 5;
    auto page2 = db->query_audit_log(query);
    REQUIRE(page2.size() == 5);
  }
}

// ============================================================================
// REST Types Tests
// ============================================================================

TEST_CASE("HTTP status codes", "[web][types]") {
  REQUIRE(static_cast<uint16_t>(http_status::ok) == 200);
  REQUIRE(static_cast<uint16_t>(http_status::created) == 201);
  REQUIRE(static_cast<uint16_t>(http_status::bad_request) == 400);
  REQUIRE(static_cast<uint16_t>(http_status::unauthorized) == 401);
  REQUIRE(static_cast<uint16_t>(http_status::forbidden) == 403);
  REQUIRE(static_cast<uint16_t>(http_status::not_found) == 404);
  REQUIRE(static_cast<uint16_t>(http_status::internal_server_error) == 500);
  REQUIRE(static_cast<uint16_t>(http_status::service_unavailable) == 503);
}

TEST_CASE("JSON escape function", "[web][types]") {
  SECTION("escapes double quotes") {
    REQUIRE(json_escape("Hello \"World\"") == "Hello \\\"World\\\"");
  }

  SECTION("escapes backslash") {
    REQUIRE(json_escape("path\\to\\file") == "path\\\\to\\\\file");
  }

  SECTION("escapes newlines") {
    REQUIRE(json_escape("line1\nline2") == "line1\\nline2");
  }

  SECTION("escapes tabs") {
    REQUIRE(json_escape("col1\tcol2") == "col1\\tcol2");
  }

  SECTION("handles empty string") {
    REQUIRE(json_escape("") == "");
  }

  SECTION("handles plain text") {
    REQUIRE(json_escape("Hello World") == "Hello World");
  }
}

TEST_CASE("Error JSON generation", "[web][types]") {
  auto json = make_error_json("NOT_FOUND", "Resource not found");
  REQUIRE(json.find("NOT_FOUND") != std::string::npos);
  REQUIRE(json.find("Resource not found") != std::string::npos);
}

TEST_CASE("Success JSON generation", "[web][types]") {
  auto json = make_success_json("Operation completed");
  REQUIRE(json.find("success") != std::string::npos);
  REQUIRE(json.find("Operation completed") != std::string::npos);
}
