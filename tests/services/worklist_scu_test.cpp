/**
 * @file worklist_scu_test.cpp
 * @brief Unit tests for Worklist SCU service
 *
 * @see Issue #533 - Implement worklist_scu Library (MWL SCU)
 */

#include <kcenon/pacs/services/worklist_scu.h>
#include <kcenon/pacs/core/dicom_dataset.h>
#include <kcenon/pacs/core/dicom_tag_constants.h>
#include <kcenon/pacs/encoding/vr_type.h>

#include <catch2/catch_test_macros.hpp>
#include "../../tools/worklist_scu/worklist_result_formatter.h"

using namespace kcenon::pacs::services;
using namespace kcenon::pacs::core;
using namespace kcenon::pacs::encoding;

// =============================================================================
// worklist_query_keys Tests
// =============================================================================

TEST_CASE("worklist_query_keys default construction", "[services][worklist_scu]") {
    worklist_query_keys keys;

    CHECK(keys.scheduled_station_ae.empty());
    CHECK(keys.modality.empty());
    CHECK(keys.scheduled_date.empty());
    CHECK(keys.scheduled_time.empty());
    CHECK(keys.scheduled_physician.empty());
    CHECK(keys.patient_name.empty());
    CHECK(keys.patient_id.empty());
    CHECK(keys.accession_number.empty());
}

TEST_CASE("worklist_query_keys can be populated", "[services][worklist_scu]") {
    worklist_query_keys keys;
    keys.scheduled_station_ae = "CT_SCANNER_01";
    keys.modality = "CT";
    keys.scheduled_date = "20241215";
    keys.patient_name = "DOE^JOHN";
    keys.patient_id = "12345";
    keys.accession_number = "ACC001";

    CHECK(keys.scheduled_station_ae == "CT_SCANNER_01");
    CHECK(keys.modality == "CT");
    CHECK(keys.scheduled_date == "20241215");
    CHECK(keys.patient_name == "DOE^JOHN");
    CHECK(keys.patient_id == "12345");
    CHECK(keys.accession_number == "ACC001");
}

// =============================================================================
// worklist_item Tests
// =============================================================================

TEST_CASE("worklist_item default construction", "[services][worklist_scu]") {
    worklist_item item;

    CHECK(item.patient_name.empty());
    CHECK(item.patient_id.empty());
    CHECK(item.scheduled_station_ae.empty());
    CHECK(item.modality.empty());
    CHECK(item.scheduled_date.empty());
    CHECK(item.study_instance_uid.empty());
}

TEST_CASE("worklist_item can be populated", "[services][worklist_scu]") {
    worklist_item item;
    item.patient_name = "DOE^JOHN";
    item.patient_id = "12345";
    item.patient_birth_date = "19800115";
    item.patient_sex = "M";
    item.scheduled_station_ae = "CT_SCANNER_01";
    item.modality = "CT";
    item.scheduled_date = "20241215";
    item.scheduled_time = "100000";
    item.scheduled_procedure_step_id = "SPS001";
    item.accession_number = "ACC001";
    item.study_instance_uid = "1.2.3.4.5.6.7.8.9";

    CHECK(item.patient_name == "DOE^JOHN");
    CHECK(item.patient_id == "12345");
    CHECK(item.patient_birth_date == "19800115");
    CHECK(item.patient_sex == "M");
    CHECK(item.scheduled_station_ae == "CT_SCANNER_01");
    CHECK(item.modality == "CT");
    CHECK(item.scheduled_date == "20241215");
    CHECK(item.scheduled_time == "100000");
    CHECK(item.scheduled_procedure_step_id == "SPS001");
    CHECK(item.accession_number == "ACC001");
    CHECK(item.study_instance_uid == "1.2.3.4.5.6.7.8.9");
}

// =============================================================================
// worklist_result Tests
// =============================================================================

TEST_CASE("worklist_result status checks", "[services][worklist_scu]") {
    SECTION("success status") {
        worklist_result result;
        result.status = 0x0000;

        CHECK(result.is_success());
        CHECK_FALSE(result.is_cancelled());
    }

    SECTION("cancelled status") {
        worklist_result result;
        result.status = 0xFE00;

        CHECK_FALSE(result.is_success());
        CHECK(result.is_cancelled());
    }

    SECTION("error status") {
        worklist_result result;
        result.status = 0xA700;

        CHECK_FALSE(result.is_success());
        CHECK_FALSE(result.is_cancelled());
    }
}

TEST_CASE("worklist_result items and elapsed", "[services][worklist_scu]") {
    worklist_result result;
    result.status = 0x0000;
    result.elapsed = std::chrono::milliseconds{150};
    result.total_pending = 5;

    worklist_item item1;
    item1.patient_name = "DOE^JOHN";
    result.items.push_back(item1);

    worklist_item item2;
    item2.patient_name = "SMITH^JANE";
    result.items.push_back(item2);

    CHECK(result.is_success());
    CHECK(result.items.size() == 2);
    CHECK(result.elapsed.count() == 150);
    CHECK(result.total_pending == 5);
}

// =============================================================================
// worklist_scu_config Tests
// =============================================================================

TEST_CASE("worklist_scu_config defaults", "[services][worklist_scu]") {
    worklist_scu_config config;

    CHECK(config.timeout.count() == 30000);
    CHECK(config.max_results == 0);
    CHECK(config.cancel_on_max == true);
}

TEST_CASE("worklist_scu_config can be customized", "[services][worklist_scu]") {
    worklist_scu_config config;
    config.timeout = std::chrono::milliseconds{60000};
    config.max_results = 100;
    config.cancel_on_max = false;

    CHECK(config.timeout.count() == 60000);
    CHECK(config.max_results == 100);
    CHECK(config.cancel_on_max == false);
}

// =============================================================================
// worklist_scu Construction Tests
// =============================================================================

TEST_CASE("worklist_scu construction with default config", "[services][worklist_scu]") {
    worklist_scu scu;

    CHECK(scu.config().timeout.count() == 30000);
    CHECK(scu.config().max_results == 0);
    CHECK(scu.queries_performed() == 0);
    CHECK(scu.total_items() == 0);
}

TEST_CASE("worklist_scu construction with custom config", "[services][worklist_scu]") {
    worklist_scu_config config;
    config.timeout = std::chrono::milliseconds{45000};
    config.max_results = 50;

    worklist_scu scu(config);

    CHECK(scu.config().timeout.count() == 45000);
    CHECK(scu.config().max_results == 50);
}

TEST_CASE("worklist_scu config update", "[services][worklist_scu]") {
    worklist_scu scu;

    worklist_scu_config new_config;
    new_config.timeout = std::chrono::milliseconds{15000};
    new_config.max_results = 25;

    scu.set_config(new_config);

    CHECK(scu.config().timeout.count() == 15000);
    CHECK(scu.config().max_results == 25);
}

// =============================================================================
// worklist_scu Statistics Tests
// =============================================================================

TEST_CASE("worklist_scu statistics reset", "[services][worklist_scu]") {
    worklist_scu scu;

    // Initial state
    CHECK(scu.queries_performed() == 0);
    CHECK(scu.total_items() == 0);

    // Reset should keep at zero
    scu.reset_statistics();

    CHECK(scu.queries_performed() == 0);
    CHECK(scu.total_items() == 0);
}

// =============================================================================
// Integration-style Tests (would require mock association)
// =============================================================================

// Note: Full integration tests with actual DICOM associations would require
// either:
// 1. A mock association class
// 2. Integration tests with a real worklist SCP
//
// These tests focus on the public interface behavior that can be verified
// without network connections.

TEST_CASE("worklist_scu date format", "[services][worklist_scu]") {
    // The library uses YYYYMMDD format for dates
    worklist_query_keys keys;
    keys.scheduled_date = "20241215";

    CHECK(keys.scheduled_date.length() == 8);
    CHECK(keys.scheduled_date.substr(0, 4) == "2024");
    CHECK(keys.scheduled_date.substr(4, 2) == "12");
    CHECK(keys.scheduled_date.substr(6, 2) == "15");
}

TEST_CASE("worklist_scu date range format", "[services][worklist_scu]") {
    // Date ranges use YYYYMMDD-YYYYMMDD format
    worklist_query_keys keys;
    keys.scheduled_date = "20241201-20241231";

    CHECK(keys.scheduled_date.find('-') != std::string::npos);
}

TEST_CASE("MWL typed matching and return keys use the scheduled step item",
          "[services][worklist_scu][nested]") {
    worklist_scu scu;
    worklist_query_keys keys;
    keys.patient_id = "P003";
    keys.modality = "MR";
    keys.scheduled_date = "20261101-20261103";
    keys.scheduled_time = "090000";
    keys.scheduled_station_ae = "MR_STATION";
    keys.scheduled_physician = "DOCTOR^TEST";
    keys.scheduled_procedure_step_id = "SPS003";
    keys.requested_procedure_description = "Requested MRI";
    auto ds = scu.build_query_dataset(keys);
    REQUIRE(ds.get_sequence(tags::scheduled_procedure_step_sequence) != nullptr);
    const auto& steps = *ds.get_sequence(tags::scheduled_procedure_step_sequence);
    REQUIRE(steps.size() == 1);
    const auto& step = steps.front();
    CHECK(ds.get_string(tags::patient_id) == "P003");
    CHECK(ds.get_string(tags::requested_procedure_description) == "Requested MRI");
    CHECK_FALSE(ds.contains(tags::study_description));
    CHECK(step.get_string(tags::modality) == "MR");
    CHECK(step.get_string(tags::scheduled_procedure_step_start_date) == keys.scheduled_date);
    CHECK(step.get_string(tags::scheduled_procedure_step_start_time) == "090000");
    CHECK(step.get_string(tags::scheduled_station_ae_title) == "MR_STATION");
    CHECK(step.get_string(tags::scheduled_performing_physician_name) == "DOCTOR^TEST");
    CHECK(step.get_string(tags::scheduled_procedure_step_id) == "SPS003");
    for (auto tag : {tags::modality, tags::scheduled_procedure_step_start_date,
                     tags::scheduled_procedure_step_start_time, tags::scheduled_station_ae_title,
                     tags::scheduled_performing_physician_name, tags::scheduled_procedure_step_id,
                     tags::scheduled_procedure_step_description, tags::scheduled_station_name,
                     tags::scheduled_procedure_step_location}) {
        CHECK_FALSE(ds.contains(tag));
        CHECK(step.contains(tag));
    }
}

TEST_CASE("MWL formatter preserves nested scheduled step fields in every output format",
          "[services][worklist_scu][nested]") {
    dicom_dataset ds;
    ds.set_string(tags::patient_id, vr_type::LO, "P003");
    // Misplaced root values must never override the nested fields.
    ds.set_string(tags::modality, vr_type::CS, "ROOT_WRONG");
    dicom_dataset step;
    step.set_string(tags::modality, vr_type::CS, "MR");
    step.set_string(tags::scheduled_procedure_step_start_date, vr_type::DA, "20261101");
    step.set_string(tags::scheduled_procedure_step_start_time, vr_type::TM, "091530");
    step.set_string(tags::scheduled_station_ae_title, vr_type::AE, "MR_STATION");
    step.set_string(tags::scheduled_procedure_step_id, vr_type::SH, "SPS003");
    step.set_string(tags::scheduled_procedure_step_description, vr_type::LO, "MRI Brain");
    ds.get_or_create_sequence(tags::scheduled_procedure_step_sequence).push_back(std::move(step));
    for (auto format : {worklist_cli::output_format::table, worklist_cli::output_format::json,
                        worklist_cli::output_format::csv, worklist_cli::output_format::xml}) {
        auto text = worklist_cli::worklist_result_formatter(format).format({ds});
        CHECK(text.find("MR_STATION") != std::string::npos);
        CHECK(text.find("SPS003") != std::string::npos);
        CHECK(text.find("ROOT_WRONG") == std::string::npos);
        if (format == worklist_cli::output_format::table) {
            CHECK(text.find("2026-11-01") != std::string::npos);
            CHECK(text.find("09:15") != std::string::npos);
        } else {
            CHECK(text.find("20261101") != std::string::npos);
            CHECK(text.find("091530") != std::string::npos);
            CHECK(text.find("MRI Brain") != std::string::npos);
        }
    }
}
