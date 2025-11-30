/**
 * @file verification_scp_test.cpp
 * @brief Unit tests for Verification SCP service
 */

#include <pacs/services/verification_scp.hpp>
#include <pacs/network/dimse/command_field.hpp>
#include <pacs/network/dimse/dimse_message.hpp>
#include <pacs/network/dimse/status_codes.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace pacs::services;
using namespace pacs::network;
using namespace pacs::network::dimse;

// ============================================================================
// verification_scp Construction Tests
// ============================================================================

TEST_CASE("verification_scp construction", "[services][verification]") {
    verification_scp scp;

    SECTION("service name is correct") {
        CHECK(scp.service_name() == "Verification SCP");
    }

    SECTION("supports exactly one SOP class") {
        auto classes = scp.supported_sop_classes();
        CHECK(classes.size() == 1);
    }

    SECTION("supports Verification SOP Class") {
        auto classes = scp.supported_sop_classes();
        REQUIRE(classes.size() == 1);
        CHECK(classes[0] == "1.2.840.10008.1.1");
    }
}

// ============================================================================
// SOP Class Support Tests
// ============================================================================

TEST_CASE("verification_scp SOP class support", "[services][verification]") {
    verification_scp scp;

    SECTION("supports Verification SOP Class UID") {
        CHECK(scp.supports_sop_class("1.2.840.10008.1.1"));
        CHECK(scp.supports_sop_class(verification_sop_class_uid));
    }

    SECTION("does not support other SOP classes") {
        // CT Image Storage
        CHECK_FALSE(scp.supports_sop_class("1.2.840.10008.5.1.4.1.1.2"));
        // MR Image Storage
        CHECK_FALSE(scp.supports_sop_class("1.2.840.10008.5.1.4.1.1.4"));
        // Patient Root Query/Retrieve
        CHECK_FALSE(scp.supports_sop_class("1.2.840.10008.5.1.4.1.2.1.1"));
        // Empty string
        CHECK_FALSE(scp.supports_sop_class(""));
        // Random UID
        CHECK_FALSE(scp.supports_sop_class("1.2.3.4.5.6.7.8.9"));
    }
}

// ============================================================================
// Verification SOP Class UID Constant Test
// ============================================================================

TEST_CASE("verification_sop_class_uid constant", "[services][verification]") {
    CHECK(verification_sop_class_uid == "1.2.840.10008.1.1");
}

// ============================================================================
// C-ECHO Message Factory Tests
// ============================================================================

TEST_CASE("make_c_echo_rq creates valid request", "[services][verification]") {
    auto request = make_c_echo_rq(42);

    CHECK(request.command() == command_field::c_echo_rq);
    CHECK(request.message_id() == 42);
    CHECK(request.affected_sop_class_uid() == "1.2.840.10008.1.1");
    CHECK(request.is_request());
    CHECK_FALSE(request.is_response());
    CHECK_FALSE(request.has_dataset());
}

TEST_CASE("make_c_echo_rsp creates valid response", "[services][verification]") {
    auto response = make_c_echo_rsp(42, status_success);

    CHECK(response.command() == command_field::c_echo_rsp);
    CHECK(response.message_id_responded_to() == 42);
    CHECK(response.affected_sop_class_uid() == "1.2.840.10008.1.1");
    CHECK(response.status() == status_success);
    CHECK(response.is_response());
    CHECK_FALSE(response.is_request());
    CHECK_FALSE(response.has_dataset());
}

// ============================================================================
// scp_service Base Class Tests
// ============================================================================

TEST_CASE("verification_scp is a scp_service", "[services][verification]") {
    // Verify verification_scp properly inherits from scp_service
    std::unique_ptr<scp_service> base_ptr = std::make_unique<verification_scp>();

    CHECK(base_ptr->service_name() == "Verification SCP");
    CHECK(base_ptr->supported_sop_classes().size() == 1);
    CHECK(base_ptr->supports_sop_class("1.2.840.10008.1.1"));
}

// ============================================================================
// Multiple Instance Tests
// ============================================================================

TEST_CASE("multiple verification_scp instances are independent", "[services][verification]") {
    verification_scp scp1;
    verification_scp scp2;

    // Both should have identical behavior
    CHECK(scp1.service_name() == scp2.service_name());
    CHECK(scp1.supported_sop_classes() == scp2.supported_sop_classes());
    CHECK(scp1.supports_sop_class("1.2.840.10008.1.1") ==
          scp2.supports_sop_class("1.2.840.10008.1.1"));
}
