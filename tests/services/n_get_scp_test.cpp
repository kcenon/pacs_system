/**
 * @file n_get_scp_test.cpp
 * @brief Unit tests for N-GET SCP service
 *
 * @see DICOM PS3.7 Section 10.1.2 - N-GET Service
 * @see Issue #720 - Implement N-GET SCP/SCU service
 */

#include <pacs/services/n_get_scp.hpp>
#include <pacs/services/mpps_scp.hpp>
#include <pacs/network/dimse/command_field.hpp>
#include <pacs/network/dimse/dimse_message.hpp>
#include <pacs/network/dimse/status_codes.hpp>
#include <pacs/core/dicom_tag_constants.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace kcenon::pacs::services;
using namespace kcenon::pacs::network;
using namespace kcenon::pacs::network::dimse;
using namespace kcenon::pacs::core;

// ============================================================================
// n_get_scp Construction Tests
// ============================================================================

TEST_CASE("n_get_scp construction", "[services][n_get]") {
    n_get_scp scp;

    SECTION("service name is correct") {
        CHECK(scp.service_name() == "N-GET SCP");
    }

    SECTION("no SOP classes supported by default") {
        auto classes = scp.supported_sop_classes();
        CHECK(classes.empty());
    }

    SECTION("initial statistics are zero") {
        CHECK(scp.gets_processed() == 0);
    }
}

// ============================================================================
// SOP Class Registration Tests
// ============================================================================

TEST_CASE("n_get_scp SOP class registration", "[services][n_get]") {
    n_get_scp scp;

    SECTION("can register MPPS SOP Class") {
        scp.add_supported_sop_class(std::string(mpps_sop_class_uid));

        auto classes = scp.supported_sop_classes();
        REQUIRE(classes.size() == 1);
        CHECK(classes[0] == mpps_sop_class_uid);
    }

    SECTION("can register multiple SOP Classes") {
        scp.add_supported_sop_class(std::string(mpps_sop_class_uid));
        scp.add_supported_sop_class("1.2.840.10008.5.1.1.1");  // Basic Film Session

        auto classes = scp.supported_sop_classes();
        CHECK(classes.size() == 2);
    }

    SECTION("supports registered SOP Class") {
        scp.add_supported_sop_class(std::string(mpps_sop_class_uid));
        CHECK(scp.supports_sop_class(mpps_sop_class_uid));
    }

    SECTION("does not support unregistered SOP Classes") {
        scp.add_supported_sop_class(std::string(mpps_sop_class_uid));

        CHECK_FALSE(scp.supports_sop_class("1.2.840.10008.1.1"));
        CHECK_FALSE(scp.supports_sop_class(""));
    }
}

// ============================================================================
// Handler Configuration Tests
// ============================================================================

TEST_CASE("n_get_scp handler configuration", "[services][n_get]") {
    n_get_scp scp;

    SECTION("can set handler") {
        bool handler_called = false;
        scp.set_handler([&handler_called](
            const std::string& /*sop_class*/,
            const std::string& /*sop_instance*/,
            const std::vector<dicom_tag>& /*tags*/) {
            handler_called = true;
            return Result<dicom_dataset>{dicom_dataset{}};
        });

        // Handler is stored but not called without a real association
        CHECK_FALSE(handler_called);
    }

    SECTION("can set handler with attribute filtering") {
        scp.set_handler([](const std::string& /*sop_class*/,
                           const std::string& /*sop_instance*/,
                           const std::vector<dicom_tag>& tags) {
            dicom_dataset ds;
            if (tags.empty()) {
                // Return all attributes
                ds.set_string(tags::patient_name, kcenon::pacs::encoding::vr_type::PN,
                              "Test^Patient");
                ds.set_string(tags::patient_id, kcenon::pacs::encoding::vr_type::LO,
                              "12345");
            }
            return Result<dicom_dataset>{std::move(ds)};
        });
    }
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_CASE("n_get_scp statistics", "[services][n_get]") {
    n_get_scp scp;

    SECTION("initial statistics are zero") {
        CHECK(scp.gets_processed() == 0);
    }

    SECTION("reset_statistics clears all counters") {
        scp.reset_statistics();
        CHECK(scp.gets_processed() == 0);
    }
}

// ============================================================================
// scp_service Base Class Tests
// ============================================================================

TEST_CASE("n_get_scp is a scp_service", "[services][n_get]") {
    std::unique_ptr<scp_service> base_ptr = std::make_unique<n_get_scp>();

    CHECK(base_ptr->service_name() == "N-GET SCP");
    CHECK(base_ptr->supported_sop_classes().empty());
}

// ============================================================================
// Multiple Instance Tests
// ============================================================================

TEST_CASE("multiple n_get_scp instances are independent", "[services][n_get]") {
    n_get_scp scp1;
    n_get_scp scp2;

    scp1.add_supported_sop_class(std::string(mpps_sop_class_uid));

    CHECK(scp1.supported_sop_classes().size() == 1);
    CHECK(scp2.supported_sop_classes().empty());

    CHECK(scp1.service_name() == scp2.service_name());
    CHECK(scp1.gets_processed() == scp2.gets_processed());
}

// ============================================================================
// N-GET Command Field Tests
// ============================================================================

TEST_CASE("N-GET command fields", "[services][n_get]") {
    SECTION("N-GET command field values") {
        CHECK(static_cast<uint16_t>(command_field::n_get_rq) == 0x0110);
        CHECK(static_cast<uint16_t>(command_field::n_get_rsp) == 0x8110);
    }

    SECTION("N-GET request is a request") {
        CHECK(is_request(command_field::n_get_rq));
        CHECK(is_response(command_field::n_get_rsp));
    }

    SECTION("N-GET is a DIMSE-N command") {
        CHECK(is_dimse_n(command_field::n_get_rq));
        CHECK(is_dimse_n(command_field::n_get_rsp));
    }
}

// ============================================================================
// N-GET Factory Function Tests
// ============================================================================

TEST_CASE("make_n_get_rq factory function", "[services][n_get]") {
    SECTION("creates request with correct fields") {
        auto msg = make_n_get_rq(
            42, mpps_sop_class_uid, "1.2.3.4.5.6.7.8", {});

        CHECK(msg.command() == command_field::n_get_rq);
        CHECK(msg.message_id() == 42);
        CHECK(msg.requested_sop_class_uid() == mpps_sop_class_uid);
        CHECK(msg.requested_sop_instance_uid() == "1.2.3.4.5.6.7.8");
    }

    SECTION("creates request with attribute tags") {
        std::vector<dicom_tag> tags = {
            tags::patient_name,
            tags::patient_id
        };
        auto msg = make_n_get_rq(
            1, mpps_sop_class_uid, "1.2.3.4", tags);

        auto retrieved_tags = msg.attribute_identifier_list();
        REQUIRE(retrieved_tags.size() == 2);
        CHECK(retrieved_tags[0] == tags::patient_name);
        CHECK(retrieved_tags[1] == tags::patient_id);
    }

    SECTION("creates request without attribute tags") {
        auto msg = make_n_get_rq(
            1, mpps_sop_class_uid, "1.2.3.4", {});

        auto retrieved_tags = msg.attribute_identifier_list();
        CHECK(retrieved_tags.empty());
    }
}

TEST_CASE("make_n_get_rsp factory function", "[services][n_get]") {
    SECTION("creates success response") {
        auto msg = make_n_get_rsp(
            42, mpps_sop_class_uid, "1.2.3.4.5.6.7.8", status_success);

        CHECK(msg.command() == command_field::n_get_rsp);
        CHECK(msg.status() == status_success);
        CHECK(msg.affected_sop_class_uid() == mpps_sop_class_uid);
        CHECK(msg.affected_sop_instance_uid() == "1.2.3.4.5.6.7.8");
    }

    SECTION("creates error response") {
        auto msg = make_n_get_rsp(
            42, mpps_sop_class_uid, "1.2.3.4",
            status_error_invalid_object_instance);

        CHECK(msg.command() == command_field::n_get_rsp);
        CHECK(msg.status() == status_error_invalid_object_instance);
    }
}
