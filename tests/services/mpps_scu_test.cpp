/**
 * @file mpps_scu_test.cpp
 * @brief Unit tests for MPPS (Modality Performed Procedure Step) SCU service
 */

#include <pacs/services/mpps_scu.hpp>
#include <pacs/network/dimse/command_field.hpp>
#include <pacs/network/dimse/dimse_message.hpp>
#include <pacs/network/dimse/status_codes.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace pacs::services;
using namespace pacs::network;
using namespace pacs::network::dimse;
using namespace pacs::core;

// ============================================================================
// mpps_scu Construction Tests
// ============================================================================

TEST_CASE("mpps_scu construction", "[services][mpps][scu]") {
    SECTION("default construction succeeds") {
        mpps_scu scu;
        CHECK(scu.creates_performed() == 0);
        CHECK(scu.sets_performed() == 0);
    }

    SECTION("construction with config succeeds") {
        mpps_scu_config config;
        config.timeout = std::chrono::milliseconds{60000};
        config.auto_generate_uid = false;

        mpps_scu scu(config);
        CHECK(scu.creates_performed() == 0);
        CHECK(scu.sets_performed() == 0);
    }

    SECTION("construction with nullptr logger succeeds") {
        mpps_scu scu(nullptr);
        CHECK(scu.creates_performed() == 0);
    }

    SECTION("construction with config and nullptr logger succeeds") {
        mpps_scu_config config;
        mpps_scu scu(config, nullptr);
        CHECK(scu.creates_performed() == 0);
    }
}

// ============================================================================
// mpps_scu_config Tests
// ============================================================================

TEST_CASE("mpps_scu_config defaults", "[services][mpps][scu]") {
    mpps_scu_config config;

    CHECK(config.timeout == std::chrono::milliseconds{30000});
    CHECK(config.auto_generate_uid == true);
}

TEST_CASE("mpps_scu_config customization", "[services][mpps][scu]") {
    mpps_scu_config config;
    config.timeout = std::chrono::milliseconds{60000};
    config.auto_generate_uid = false;

    CHECK(config.timeout == std::chrono::milliseconds{60000});
    CHECK(config.auto_generate_uid == false);
}

// ============================================================================
// mpps_create_data Structure Tests
// ============================================================================

TEST_CASE("mpps_create_data structure", "[services][mpps][scu]") {
    mpps_create_data data;

    SECTION("default construction has empty fields") {
        CHECK(data.scheduled_procedure_step_id.empty());
        CHECK(data.study_instance_uid.empty());
        CHECK(data.accession_number.empty());
        CHECK(data.patient_name.empty());
        CHECK(data.patient_id.empty());
        CHECK(data.patient_birth_date.empty());
        CHECK(data.patient_sex.empty());
        CHECK(data.mpps_sop_instance_uid.empty());
        CHECK(data.procedure_step_start_date.empty());
        CHECK(data.procedure_step_start_time.empty());
        CHECK(data.modality.empty());
        CHECK(data.station_ae_title.empty());
        CHECK(data.station_name.empty());
        CHECK(data.procedure_description.empty());
        CHECK(data.performing_physician.empty());
        CHECK(data.operator_name.empty());
    }

    SECTION("can be initialized with values") {
        data.patient_id = "12345";
        data.patient_name = "Doe^John";
        data.modality = "CT";
        data.station_ae_title = "CT_SCANNER_01";
        data.study_instance_uid = "1.2.3.4.5.6.7";

        CHECK(data.patient_id == "12345");
        CHECK(data.patient_name == "Doe^John");
        CHECK(data.modality == "CT");
        CHECK(data.station_ae_title == "CT_SCANNER_01");
        CHECK(data.study_instance_uid == "1.2.3.4.5.6.7");
    }
}

// ============================================================================
// mpps_set_data Structure Tests
// ============================================================================

TEST_CASE("mpps_set_data structure", "[services][mpps][scu]") {
    mpps_set_data data;

    SECTION("default construction") {
        CHECK(data.mpps_sop_instance_uid.empty());
        CHECK(data.status == mpps_status::completed);
        CHECK(data.procedure_step_end_date.empty());
        CHECK(data.procedure_step_end_time.empty());
        CHECK(data.performed_series.empty());
        CHECK(data.discontinuation_reason.empty());
    }

    SECTION("can be initialized for completion") {
        data.mpps_sop_instance_uid = "1.2.3.4.5.6.7.8";
        data.status = mpps_status::completed;
        data.procedure_step_end_date = "20241215";
        data.procedure_step_end_time = "143000";

        performed_series_info series;
        series.series_uid = "1.2.3.4.5.6.7.8.9";
        series.modality = "CT";
        series.num_instances = 150;
        data.performed_series.push_back(series);

        CHECK(data.mpps_sop_instance_uid == "1.2.3.4.5.6.7.8");
        CHECK(data.status == mpps_status::completed);
        CHECK(data.procedure_step_end_date == "20241215");
        CHECK(data.procedure_step_end_time == "143000");
        REQUIRE(data.performed_series.size() == 1);
        CHECK(data.performed_series[0].series_uid == "1.2.3.4.5.6.7.8.9");
        CHECK(data.performed_series[0].modality == "CT");
        CHECK(data.performed_series[0].num_instances == 150);
    }

    SECTION("can be initialized for discontinuation") {
        data.mpps_sop_instance_uid = "1.2.3.4.5.6.7.8";
        data.status = mpps_status::discontinued;
        data.discontinuation_reason = "Patient refused";

        CHECK(data.mpps_sop_instance_uid == "1.2.3.4.5.6.7.8");
        CHECK(data.status == mpps_status::discontinued);
        CHECK(data.discontinuation_reason == "Patient refused");
    }
}

// ============================================================================
// performed_series_info Structure Tests
// ============================================================================

TEST_CASE("performed_series_info structure", "[services][mpps][scu]") {
    performed_series_info series;

    SECTION("default construction") {
        CHECK(series.series_uid.empty());
        CHECK(series.series_description.empty());
        CHECK(series.modality.empty());
        CHECK(series.performing_physician.empty());
        CHECK(series.operator_name.empty());
        CHECK(series.sop_instance_uids.empty());
        CHECK(series.num_instances == 0);
    }

    SECTION("can be fully initialized") {
        series.series_uid = "1.2.3.4.5.6.7.8.9";
        series.series_description = "CT Chest";
        series.modality = "CT";
        series.performing_physician = "Dr. Smith";
        series.operator_name = "Tech Johnson";
        series.sop_instance_uids = {"1.2.3.4.5.6.7.8.9.1", "1.2.3.4.5.6.7.8.9.2"};
        series.num_instances = 150;

        CHECK(series.series_uid == "1.2.3.4.5.6.7.8.9");
        CHECK(series.series_description == "CT Chest");
        CHECK(series.modality == "CT");
        CHECK(series.performing_physician == "Dr. Smith");
        CHECK(series.operator_name == "Tech Johnson");
        REQUIRE(series.sop_instance_uids.size() == 2);
        CHECK(series.num_instances == 150);
    }
}

// ============================================================================
// mpps_result Structure Tests
// ============================================================================

TEST_CASE("mpps_result structure", "[services][mpps][scu]") {
    mpps_result result;

    SECTION("default construction") {
        CHECK(result.mpps_sop_instance_uid.empty());
        CHECK(result.status == 0);
        CHECK(result.error_comment.empty());
        CHECK(result.elapsed == std::chrono::milliseconds{0});
    }

    SECTION("is_success returns true for status 0x0000") {
        result.status = 0x0000;
        CHECK(result.is_success());
        CHECK_FALSE(result.is_warning());
        CHECK_FALSE(result.is_error());
    }

    SECTION("is_warning returns true for 0xBxxx status") {
        result.status = 0xB000;
        CHECK_FALSE(result.is_success());
        CHECK(result.is_warning());
        CHECK_FALSE(result.is_error());

        result.status = 0xB123;
        CHECK(result.is_warning());

        result.status = 0xBFFF;
        CHECK(result.is_warning());
    }

    SECTION("is_error returns true for error status codes") {
        result.status = 0xC310;  // Common MPPS error
        CHECK_FALSE(result.is_success());
        CHECK_FALSE(result.is_warning());
        CHECK(result.is_error());

        result.status = 0xA700;  // Out of resources
        CHECK(result.is_error());

        result.status = 0x0110;  // Processing failure
        CHECK(result.is_error());
    }

    SECTION("can store elapsed time") {
        result.elapsed = std::chrono::milliseconds{150};
        CHECK(result.elapsed.count() == 150);
    }
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_CASE("mpps_scu statistics", "[services][mpps][scu]") {
    mpps_scu scu;

    SECTION("initial statistics are zero") {
        CHECK(scu.creates_performed() == 0);
        CHECK(scu.sets_performed() == 0);
    }

    SECTION("reset_statistics clears all counters") {
        scu.reset_statistics();
        CHECK(scu.creates_performed() == 0);
        CHECK(scu.sets_performed() == 0);
    }
}

// ============================================================================
// Multiple Instance Tests
// ============================================================================

TEST_CASE("multiple mpps_scu instances are independent", "[services][mpps][scu]") {
    mpps_scu scu1;
    mpps_scu scu2;

    // Both should have zero statistics
    CHECK(scu1.creates_performed() == scu2.creates_performed());
    CHECK(scu1.sets_performed() == scu2.sets_performed());

    // Statistics should be independent
    scu1.reset_statistics();
    CHECK(scu1.creates_performed() == 0);
    CHECK(scu2.creates_performed() == 0);
}

// ============================================================================
// MPPS Tags Tests
// ============================================================================

TEST_CASE("mpps_tags namespace contains MPPS-specific tags for SCU", "[services][mpps][scu]") {
    using namespace mpps_tags;

    SECTION("performed procedure step timing tags") {
        CHECK(performed_procedure_step_start_date == dicom_tag{0x0040, 0x0244});
        CHECK(performed_procedure_step_start_time == dicom_tag{0x0040, 0x0245});
    }

    SECTION("performed procedure step description tag") {
        CHECK(performed_procedure_step_description == dicom_tag{0x0040, 0x0254});
    }

    SECTION("performing information tags") {
        CHECK(performing_physicians_name == dicom_tag{0x0008, 0x1050});
        CHECK(operators_name == dicom_tag{0x0008, 0x1070});
    }

    SECTION("series description tag") {
        CHECK(series_description == dicom_tag{0x0008, 0x103E});
    }

    SECTION("discontinuation reason code sequence tag") {
        CHECK(discontinuation_reason_code_sequence == dicom_tag{0x0040, 0x0281});
    }
}

// ============================================================================
// SOP Class UID Constant Test
// ============================================================================

TEST_CASE("mpps_sop_class_uid is accessible from mpps_scu", "[services][mpps][scu]") {
    // Verify the constant is accessible and correct
    CHECK(mpps_sop_class_uid == "1.2.840.10008.3.1.2.3.3");
}

// ============================================================================
// mpps_status Enumeration Tests (from mpps_scp.hpp, used by SCU)
// ============================================================================

TEST_CASE("mpps_status to_string conversion (SCU)", "[services][mpps][scu]") {
    CHECK(to_string(mpps_status::in_progress) == "IN PROGRESS");
    CHECK(to_string(mpps_status::completed) == "COMPLETED");
    CHECK(to_string(mpps_status::discontinued) == "DISCONTINUED");
}

// ============================================================================
// Data Structure Copy and Move Tests
// ============================================================================

TEST_CASE("mpps_create_data is copyable and movable", "[services][mpps][scu]") {
    mpps_create_data data;
    data.patient_id = "12345";
    data.patient_name = "Doe^John";
    data.modality = "CT";

    SECTION("copy construction") {
        mpps_create_data copy = data;
        CHECK(copy.patient_id == "12345");
        CHECK(copy.patient_name == "Doe^John");
        CHECK(copy.modality == "CT");
    }

    SECTION("move construction") {
        mpps_create_data moved = std::move(data);
        CHECK(moved.patient_id == "12345");
        CHECK(moved.patient_name == "Doe^John");
        CHECK(moved.modality == "CT");
    }
}

TEST_CASE("mpps_set_data is copyable and movable", "[services][mpps][scu]") {
    mpps_set_data data;
    data.mpps_sop_instance_uid = "1.2.3.4.5.6.7.8";
    data.status = mpps_status::completed;

    performed_series_info series;
    series.series_uid = "1.2.3.4.5.6.7.8.9";
    data.performed_series.push_back(series);

    SECTION("copy construction") {
        mpps_set_data copy = data;
        CHECK(copy.mpps_sop_instance_uid == "1.2.3.4.5.6.7.8");
        CHECK(copy.status == mpps_status::completed);
        REQUIRE(copy.performed_series.size() == 1);
        CHECK(copy.performed_series[0].series_uid == "1.2.3.4.5.6.7.8.9");
    }

    SECTION("move construction") {
        mpps_set_data moved = std::move(data);
        CHECK(moved.mpps_sop_instance_uid == "1.2.3.4.5.6.7.8");
        CHECK(moved.status == mpps_status::completed);
        REQUIRE(moved.performed_series.size() == 1);
        CHECK(moved.performed_series[0].series_uid == "1.2.3.4.5.6.7.8.9");
    }
}

TEST_CASE("mpps_result is copyable and movable", "[services][mpps][scu]") {
    mpps_result result;
    result.mpps_sop_instance_uid = "1.2.3.4.5.6.7.8";
    result.status = 0x0000;
    result.elapsed = std::chrono::milliseconds{100};

    SECTION("copy construction") {
        mpps_result copy = result;
        CHECK(copy.mpps_sop_instance_uid == "1.2.3.4.5.6.7.8");
        CHECK(copy.status == 0x0000);
        CHECK(copy.elapsed.count() == 100);
    }

    SECTION("move construction") {
        mpps_result moved = std::move(result);
        CHECK(moved.mpps_sop_instance_uid == "1.2.3.4.5.6.7.8");
        CHECK(moved.status == 0x0000);
        CHECK(moved.elapsed.count() == 100);
    }
}
