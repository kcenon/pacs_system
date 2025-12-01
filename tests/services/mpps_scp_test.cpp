/**
 * @file mpps_scp_test.cpp
 * @brief Unit tests for MPPS (Modality Performed Procedure Step) SCP service
 */

#include <pacs/services/mpps_scp.hpp>
#include <pacs/network/dimse/command_field.hpp>
#include <pacs/network/dimse/dimse_message.hpp>
#include <pacs/network/dimse/status_codes.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace pacs::services;
using namespace pacs::network;
using namespace pacs::network::dimse;
using namespace pacs::core;

// ============================================================================
// mpps_scp Construction Tests
// ============================================================================

TEST_CASE("mpps_scp construction", "[services][mpps]") {
    mpps_scp scp;

    SECTION("service name is correct") {
        CHECK(scp.service_name() == "MPPS SCP");
    }

    SECTION("supports exactly one SOP class") {
        auto classes = scp.supported_sop_classes();
        CHECK(classes.size() == 1);
    }

    SECTION("supports MPPS SOP Class") {
        auto classes = scp.supported_sop_classes();
        REQUIRE(classes.size() == 1);
        CHECK(classes[0] == "1.2.840.10008.3.1.2.3.3");
    }
}

// ============================================================================
// SOP Class Support Tests
// ============================================================================

TEST_CASE("mpps_scp SOP class support", "[services][mpps]") {
    mpps_scp scp;

    SECTION("supports MPPS SOP Class UID") {
        CHECK(scp.supports_sop_class("1.2.840.10008.3.1.2.3.3"));
        CHECK(scp.supports_sop_class(mpps_sop_class_uid));
    }

    SECTION("does not support other SOP classes") {
        // Verification SOP Class
        CHECK_FALSE(scp.supports_sop_class("1.2.840.10008.1.1"));
        // CT Image Storage
        CHECK_FALSE(scp.supports_sop_class("1.2.840.10008.5.1.4.1.1.2"));
        // Modality Worklist
        CHECK_FALSE(scp.supports_sop_class("1.2.840.10008.5.1.4.31"));
        // Empty string
        CHECK_FALSE(scp.supports_sop_class(""));
        // Random UID
        CHECK_FALSE(scp.supports_sop_class("1.2.3.4.5.6.7.8.9"));
    }
}

// ============================================================================
// MPPS SOP Class UID Constant Test
// ============================================================================

TEST_CASE("mpps_sop_class_uid constant", "[services][mpps]") {
    CHECK(mpps_sop_class_uid == "1.2.840.10008.3.1.2.3.3");
}

// ============================================================================
// mpps_status Enumeration Tests
// ============================================================================

TEST_CASE("mpps_status to_string conversion", "[services][mpps]") {
    CHECK(to_string(mpps_status::in_progress) == "IN PROGRESS");
    CHECK(to_string(mpps_status::completed) == "COMPLETED");
    CHECK(to_string(mpps_status::discontinued) == "DISCONTINUED");
}

TEST_CASE("parse_mpps_status conversion", "[services][mpps]") {
    SECTION("valid status strings") {
        auto in_progress = parse_mpps_status("IN PROGRESS");
        REQUIRE(in_progress.has_value());
        CHECK(in_progress.value() == mpps_status::in_progress);

        auto completed = parse_mpps_status("COMPLETED");
        REQUIRE(completed.has_value());
        CHECK(completed.value() == mpps_status::completed);

        auto discontinued = parse_mpps_status("DISCONTINUED");
        REQUIRE(discontinued.has_value());
        CHECK(discontinued.value() == mpps_status::discontinued);
    }

    SECTION("invalid status strings return nullopt") {
        CHECK_FALSE(parse_mpps_status("").has_value());
        CHECK_FALSE(parse_mpps_status("INVALID").has_value());
        CHECK_FALSE(parse_mpps_status("in progress").has_value());  // case sensitive
        CHECK_FALSE(parse_mpps_status("completed").has_value());    // case sensitive
        CHECK_FALSE(parse_mpps_status("IN_PROGRESS").has_value());  // wrong format
    }
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_CASE("mpps_scp statistics", "[services][mpps]") {
    mpps_scp scp;

    SECTION("initial statistics are zero") {
        CHECK(scp.creates_processed() == 0);
        CHECK(scp.sets_processed() == 0);
        CHECK(scp.mpps_completed() == 0);
        CHECK(scp.mpps_discontinued() == 0);
    }

    SECTION("reset_statistics clears all counters") {
        // Note: We can't easily increment these without a full integration test,
        // but we can verify reset works from zero state
        scp.reset_statistics();
        CHECK(scp.creates_processed() == 0);
        CHECK(scp.sets_processed() == 0);
        CHECK(scp.mpps_completed() == 0);
        CHECK(scp.mpps_discontinued() == 0);
    }
}

// ============================================================================
// Handler Configuration Tests
// ============================================================================

TEST_CASE("mpps_scp handler configuration", "[services][mpps]") {
    mpps_scp scp;

    SECTION("can set create handler") {
        bool handler_called = false;
        scp.set_create_handler([&handler_called](const mpps_instance& /*instance*/) {
            handler_called = true;
            return Result<std::monostate>{std::monostate{}};
        });

        // Handler is stored but not called without a real association
        CHECK_FALSE(handler_called);
    }

    SECTION("can set set handler") {
        bool handler_called = false;
        scp.set_set_handler([&handler_called](
            const std::string& /*uid*/,
            const dicom_dataset& /*mods*/,
            mpps_status /*status*/) {
            handler_called = true;
            return Result<std::monostate>{std::monostate{}};
        });

        // Handler is stored but not called without a real association
        CHECK_FALSE(handler_called);
    }
}

// ============================================================================
// mpps_instance Structure Tests
// ============================================================================

TEST_CASE("mpps_instance structure", "[services][mpps]") {
    mpps_instance instance;

    SECTION("default construction") {
        CHECK(instance.sop_instance_uid.empty());
        CHECK(instance.status == mpps_status::in_progress);
        CHECK(instance.station_ae.empty());
    }

    SECTION("can be initialized") {
        instance.sop_instance_uid = "1.2.3.4.5.6";
        instance.status = mpps_status::in_progress;
        instance.station_ae = "CT_SCANNER_01";

        CHECK(instance.sop_instance_uid == "1.2.3.4.5.6");
        CHECK(instance.status == mpps_status::in_progress);
        CHECK(instance.station_ae == "CT_SCANNER_01");
    }
}

// ============================================================================
// MPPS Tags Tests
// ============================================================================

TEST_CASE("mpps_tags namespace contains MPPS-specific tags", "[services][mpps]") {
    using namespace mpps_tags;

    SECTION("performed station tags") {
        CHECK(performed_station_ae_title == dicom_tag{0x0040, 0x0241});
        CHECK(performed_station_name == dicom_tag{0x0040, 0x0242});
        CHECK(performed_location == dicom_tag{0x0040, 0x0243});
    }

    SECTION("performed procedure step tags") {
        CHECK(performed_procedure_step_end_date == dicom_tag{0x0040, 0x0250});
        CHECK(performed_procedure_step_end_time == dicom_tag{0x0040, 0x0251});
        CHECK(performed_procedure_step_status == dicom_tag{0x0040, 0x0252});
        CHECK(performed_procedure_step_id == dicom_tag{0x0040, 0x0253});
    }

    SECTION("sequence tags") {
        CHECK(performed_series_sequence == dicom_tag{0x0040, 0x0340});
        CHECK(scheduled_step_attributes_sequence == dicom_tag{0x0040, 0x0270});
    }
}

// ============================================================================
// scp_service Base Class Tests
// ============================================================================

TEST_CASE("mpps_scp is a scp_service", "[services][mpps]") {
    // Verify mpps_scp properly inherits from scp_service
    std::unique_ptr<scp_service> base_ptr = std::make_unique<mpps_scp>();

    CHECK(base_ptr->service_name() == "MPPS SCP");
    CHECK(base_ptr->supported_sop_classes().size() == 1);
    CHECK(base_ptr->supports_sop_class("1.2.840.10008.3.1.2.3.3"));
}

// ============================================================================
// Multiple Instance Tests
// ============================================================================

TEST_CASE("multiple mpps_scp instances are independent", "[services][mpps]") {
    mpps_scp scp1;
    mpps_scp scp2;

    // Both should have identical behavior
    CHECK(scp1.service_name() == scp2.service_name());
    CHECK(scp1.supported_sop_classes() == scp2.supported_sop_classes());
    CHECK(scp1.supports_sop_class("1.2.840.10008.3.1.2.3.3") ==
          scp2.supports_sop_class("1.2.840.10008.3.1.2.3.3"));

    // Statistics should be independent
    scp1.reset_statistics();
    CHECK(scp1.creates_processed() == scp2.creates_processed());
}

// ============================================================================
// Command Field Tests for N-CREATE and N-SET
// ============================================================================

TEST_CASE("N-CREATE and N-SET command fields", "[services][mpps]") {
    SECTION("N-CREATE command fields") {
        CHECK(static_cast<uint16_t>(command_field::n_create_rq) == 0x0140);
        CHECK(static_cast<uint16_t>(command_field::n_create_rsp) == 0x8140);
        CHECK(is_request(command_field::n_create_rq));
        CHECK(is_response(command_field::n_create_rsp));
    }

    SECTION("N-SET command fields") {
        CHECK(static_cast<uint16_t>(command_field::n_set_rq) == 0x0120);
        CHECK(static_cast<uint16_t>(command_field::n_set_rsp) == 0x8120);
        CHECK(is_request(command_field::n_set_rq));
        CHECK(is_response(command_field::n_set_rsp));
    }

    SECTION("N-CREATE and N-SET are DIMSE-N commands") {
        CHECK(is_dimse_n(command_field::n_create_rq));
        CHECK(is_dimse_n(command_field::n_create_rsp));
        CHECK(is_dimse_n(command_field::n_set_rq));
        CHECK(is_dimse_n(command_field::n_set_rsp));
    }
}
