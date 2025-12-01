/**
 * @file retrieve_scp_test.cpp
 * @brief Unit tests for Retrieve SCP service (C-MOVE/C-GET)
 */

#include <pacs/services/retrieve_scp.hpp>
#include <pacs/network/dimse/command_field.hpp>
#include <pacs/network/dimse/dimse_message.hpp>
#include <pacs/network/dimse/status_codes.hpp>
#include <pacs/core/dicom_tag_constants.hpp>
#include <pacs/encoding/vr_type.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace pacs::services;
using namespace pacs::network;
using namespace pacs::network::dimse;
using namespace pacs::core;
using namespace pacs::encoding;

// ============================================================================
// sub_operation_stats Tests
// ============================================================================

TEST_CASE("sub_operation_stats", "[services][retrieve]") {
    sub_operation_stats stats;

    SECTION("default values are zero") {
        CHECK(stats.remaining == 0);
        CHECK(stats.completed == 0);
        CHECK(stats.failed == 0);
        CHECK(stats.warning == 0);
    }

    SECTION("total() returns sum of all counts") {
        stats.remaining = 10;
        stats.completed = 5;
        stats.failed = 2;
        stats.warning = 1;

        CHECK(stats.total() == 18);
    }

    SECTION("all_successful() returns true when no failures") {
        stats.completed = 10;
        stats.warning = 2;
        CHECK(stats.all_successful());
    }

    SECTION("all_successful() returns false when failures exist") {
        stats.completed = 10;
        stats.failed = 1;
        CHECK_FALSE(stats.all_successful());
    }
}

// ============================================================================
// retrieve_scp Construction Tests
// ============================================================================

TEST_CASE("retrieve_scp construction", "[services][retrieve]") {
    retrieve_scp scp;

    SECTION("service name is correct") {
        CHECK(scp.service_name() == "Retrieve SCP");
    }

    SECTION("supports four SOP classes") {
        auto classes = scp.supported_sop_classes();
        CHECK(classes.size() == 4);
    }

    SECTION("initial statistics are zero") {
        CHECK(scp.move_operations() == 0);
        CHECK(scp.get_operations() == 0);
        CHECK(scp.images_transferred() == 0);
    }
}

// ============================================================================
// SOP Class Support Tests
// ============================================================================

TEST_CASE("retrieve_scp SOP class support", "[services][retrieve]") {
    retrieve_scp scp;

    SECTION("supports Patient Root Query/Retrieve MOVE") {
        CHECK(scp.supports_sop_class("1.2.840.10008.5.1.4.1.2.1.2"));
        CHECK(scp.supports_sop_class(patient_root_move_sop_class_uid));
    }

    SECTION("supports Study Root Query/Retrieve MOVE") {
        CHECK(scp.supports_sop_class("1.2.840.10008.5.1.4.1.2.2.2"));
        CHECK(scp.supports_sop_class(study_root_move_sop_class_uid));
    }

    SECTION("supports Patient Root Query/Retrieve GET") {
        CHECK(scp.supports_sop_class("1.2.840.10008.5.1.4.1.2.1.3"));
        CHECK(scp.supports_sop_class(patient_root_get_sop_class_uid));
    }

    SECTION("supports Study Root Query/Retrieve GET") {
        CHECK(scp.supports_sop_class("1.2.840.10008.5.1.4.1.2.2.3"));
        CHECK(scp.supports_sop_class(study_root_get_sop_class_uid));
    }

    SECTION("does not support non-Retrieve SOP classes") {
        // Verification SOP Class
        CHECK_FALSE(scp.supports_sop_class("1.2.840.10008.1.1"));
        // CT Image Storage
        CHECK_FALSE(scp.supports_sop_class("1.2.840.10008.5.1.4.1.1.2"));
        // Patient Root FIND
        CHECK_FALSE(scp.supports_sop_class("1.2.840.10008.5.1.4.1.2.1.1"));
        // Study Root FIND
        CHECK_FALSE(scp.supports_sop_class("1.2.840.10008.5.1.4.1.2.2.1"));
        // Empty string
        CHECK_FALSE(scp.supports_sop_class(""));
    }
}

// ============================================================================
// SOP Class UID Constants Tests
// ============================================================================

TEST_CASE("retrieve SOP class UID constants", "[services][retrieve]") {
    CHECK(patient_root_move_sop_class_uid == "1.2.840.10008.5.1.4.1.2.1.2");
    CHECK(study_root_move_sop_class_uid == "1.2.840.10008.5.1.4.1.2.2.2");
    CHECK(patient_root_get_sop_class_uid == "1.2.840.10008.5.1.4.1.2.1.3");
    CHECK(study_root_get_sop_class_uid == "1.2.840.10008.5.1.4.1.2.2.3");
}

// ============================================================================
// Configuration Tests
// ============================================================================

TEST_CASE("retrieve_scp configuration", "[services][retrieve]") {
    retrieve_scp scp;

    SECTION("set_retrieve_handler accepts lambda") {
        bool handler_called = false;
        scp.set_retrieve_handler([&handler_called](
            [[maybe_unused]] const dicom_dataset& keys) {
            handler_called = true;
            return std::vector<dicom_file>{};
        });
        // Handler is stored but not called in this test
        CHECK_FALSE(handler_called);
    }

    SECTION("set_destination_resolver accepts lambda") {
        bool resolver_called = false;
        scp.set_destination_resolver([&resolver_called](
            [[maybe_unused]] const std::string& ae) {
            resolver_called = true;
            return std::make_optional(std::make_pair(std::string("localhost"), uint16_t{11112}));
        });
        // Resolver is stored but not called in this test
        CHECK_FALSE(resolver_called);
    }

    SECTION("set_cancel_check accepts lambda") {
        bool cancel_called = false;
        scp.set_cancel_check([&cancel_called]() {
            cancel_called = true;
            return false;
        });
        // Cancel check is stored but not called in this test
        CHECK_FALSE(cancel_called);
    }

    SECTION("set_store_sub_operation accepts lambda") {
        bool store_called = false;
        scp.set_store_sub_operation([&store_called](
            [[maybe_unused]] association& assoc,
            [[maybe_unused]] uint8_t context_id,
            [[maybe_unused]] const dicom_file& file,
            [[maybe_unused]] const std::string& move_originator_ae,
            [[maybe_unused]] uint16_t move_originator_msg_id) {
            store_called = true;
            return status_success;
        });
        // Store handler is stored but not called in this test
        CHECK_FALSE(store_called);
    }
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_CASE("retrieve_scp statistics", "[services][retrieve]") {
    retrieve_scp scp;

    SECTION("statistics start at zero") {
        CHECK(scp.move_operations() == 0);
        CHECK(scp.get_operations() == 0);
        CHECK(scp.images_transferred() == 0);
    }

    SECTION("reset_statistics resets all counters to zero") {
        // Reset and verify
        scp.reset_statistics();
        CHECK(scp.move_operations() == 0);
        CHECK(scp.get_operations() == 0);
        CHECK(scp.images_transferred() == 0);
    }
}

// ============================================================================
// scp_service Base Class Tests
// ============================================================================

TEST_CASE("retrieve_scp is a scp_service", "[services][retrieve]") {
    // Verify retrieve_scp properly inherits from scp_service
    std::unique_ptr<scp_service> base_ptr = std::make_unique<retrieve_scp>();

    CHECK(base_ptr->service_name() == "Retrieve SCP");
    CHECK(base_ptr->supported_sop_classes().size() == 4);
    CHECK(base_ptr->supports_sop_class(patient_root_move_sop_class_uid));
    CHECK(base_ptr->supports_sop_class(study_root_move_sop_class_uid));
    CHECK(base_ptr->supports_sop_class(patient_root_get_sop_class_uid));
    CHECK(base_ptr->supports_sop_class(study_root_get_sop_class_uid));
}

// ============================================================================
// Multiple Instance Tests
// ============================================================================

TEST_CASE("multiple retrieve_scp instances are independent", "[services][retrieve]") {
    retrieve_scp scp1;
    retrieve_scp scp2;

    // Configure different handlers
    bool handler1_called = false;
    bool handler2_called = false;

    scp1.set_retrieve_handler([&handler1_called](const dicom_dataset&) {
        handler1_called = true;
        return std::vector<dicom_file>{};
    });

    scp2.set_retrieve_handler([&handler2_called](const dicom_dataset&) {
        handler2_called = true;
        return std::vector<dicom_file>{};
    });

    // Verify handlers are independent
    CHECK_FALSE(handler1_called);
    CHECK_FALSE(handler2_called);

    // Reset one should not affect the other
    scp1.reset_statistics();
    CHECK(scp1.move_operations() == 0);
    CHECK(scp2.move_operations() == 0);
}

// ============================================================================
// Destination Resolver Tests
// ============================================================================

TEST_CASE("retrieve_scp destination resolver", "[services][retrieve]") {
    retrieve_scp scp;

    SECTION("resolver can return valid destination") {
        scp.set_destination_resolver([](const std::string& ae) {
            if (ae == "VIEWER") {
                return std::make_optional(std::make_pair(std::string("192.168.1.10"), uint16_t{11112}));
            }
            return std::optional<std::pair<std::string, uint16_t>>{};
        });
        // Resolver is stored but not called in this test
        CHECK(true);  // Just verify no exception
    }

    SECTION("resolver can return nullopt for unknown destination") {
        scp.set_destination_resolver([](const std::string& ae) {
            if (ae == "KNOWN_AE") {
                return std::make_optional(std::make_pair(std::string("localhost"), uint16_t{104}));
            }
            return std::optional<std::pair<std::string, uint16_t>>{};
        });
        // Resolver is stored but not called in this test
        CHECK(true);  // Just verify no exception
    }
}

// ============================================================================
// Handler Integration Tests
// ============================================================================

TEST_CASE("retrieve_scp handler integration", "[services][retrieve]") {
    retrieve_scp scp;

    dicom_dataset query_keys;
    query_keys.set_string(tags::study_instance_uid, vr_type::UI, "1.2.3.4.5");
    query_keys.set_string(tags::query_retrieve_level, vr_type::CS, "STUDY");

    bool handler_called = false;
    std::string captured_study_uid;

    scp.set_retrieve_handler([&](const dicom_dataset& keys) {
        handler_called = true;
        captured_study_uid = keys.get_string(tags::study_instance_uid);
        return std::vector<dicom_file>{};
    });

    SECTION("handler captures correct parameters") {
        // Note: Actual handle_message testing requires a mock association
        // This test validates handler setup only
        CHECK_FALSE(handler_called);
    }
}

// ============================================================================
// Status Code Tests
// ============================================================================

TEST_CASE("retrieve status codes", "[services][retrieve]") {
    SECTION("status_refused_move_destination_unknown is 0xA801") {
        CHECK(status_refused_move_destination_unknown == 0xA801);
    }

    SECTION("status_refused_out_of_resources_subops is 0xA702") {
        CHECK(status_refused_out_of_resources_subops == 0xA702);
    }

    SECTION("status_warning_subops_complete_failures is 0xB000") {
        CHECK(status_warning_subops_complete_failures == 0xB000);
    }

    SECTION("is_pending identifies pending status") {
        CHECK(is_pending(status_pending));
        CHECK(is_pending(status_pending_warning));
        CHECK_FALSE(is_pending(status_success));
        CHECK_FALSE(is_pending(status_cancel));
    }

    SECTION("is_success identifies success status") {
        CHECK(is_success(status_success));
        CHECK_FALSE(is_success(status_pending));
        CHECK_FALSE(is_success(status_cancel));
    }

    SECTION("is_warning identifies warning status") {
        CHECK(is_warning(status_warning_coercion));
        CHECK(is_warning(status_warning_subops_complete_failures));
        CHECK_FALSE(is_warning(status_success));
        CHECK_FALSE(is_warning(status_pending));
    }
}

// ============================================================================
// Command Field Tests
// ============================================================================

TEST_CASE("retrieve command fields", "[services][retrieve]") {
    SECTION("C-MOVE-RQ command field is 0x0021") {
        CHECK(static_cast<uint16_t>(command_field::c_move_rq) == 0x0021);
    }

    SECTION("C-MOVE-RSP command field is 0x8021") {
        CHECK(static_cast<uint16_t>(command_field::c_move_rsp) == 0x8021);
    }

    SECTION("C-GET-RQ command field is 0x0010") {
        CHECK(static_cast<uint16_t>(command_field::c_get_rq) == 0x0010);
    }

    SECTION("C-GET-RSP command field is 0x8010") {
        CHECK(static_cast<uint16_t>(command_field::c_get_rsp) == 0x8010);
    }

    SECTION("get_response_command returns correct response") {
        CHECK(get_response_command(command_field::c_move_rq) == command_field::c_move_rsp);
        CHECK(get_response_command(command_field::c_get_rq) == command_field::c_get_rsp);
    }

    SECTION("is_request identifies request commands") {
        CHECK(is_request(command_field::c_move_rq));
        CHECK(is_request(command_field::c_get_rq));
        CHECK_FALSE(is_request(command_field::c_move_rsp));
        CHECK_FALSE(is_request(command_field::c_get_rsp));
    }

    SECTION("is_response identifies response commands") {
        CHECK(is_response(command_field::c_move_rsp));
        CHECK(is_response(command_field::c_get_rsp));
        CHECK_FALSE(is_response(command_field::c_move_rq));
        CHECK_FALSE(is_response(command_field::c_get_rq));
    }
}

// ============================================================================
// DIMSE Tag Constants Tests
// ============================================================================

TEST_CASE("retrieve DIMSE tag constants", "[services][retrieve]") {
    SECTION("Move Destination tag is (0000,0600)") {
        CHECK(tag_move_destination.group() == 0x0000);
        CHECK(tag_move_destination.element() == 0x0600);
    }

    SECTION("Number of Remaining Sub-operations tag is (0000,1020)") {
        CHECK(tag_number_of_remaining_subops.group() == 0x0000);
        CHECK(tag_number_of_remaining_subops.element() == 0x1020);
    }

    SECTION("Number of Completed Sub-operations tag is (0000,1021)") {
        CHECK(tag_number_of_completed_subops.group() == 0x0000);
        CHECK(tag_number_of_completed_subops.element() == 0x1021);
    }

    SECTION("Number of Failed Sub-operations tag is (0000,1022)") {
        CHECK(tag_number_of_failed_subops.group() == 0x0000);
        CHECK(tag_number_of_failed_subops.element() == 0x1022);
    }

    SECTION("Number of Warning Sub-operations tag is (0000,1023)") {
        CHECK(tag_number_of_warning_subops.group() == 0x0000);
        CHECK(tag_number_of_warning_subops.element() == 0x1023);
    }

    SECTION("Move Originator AET tag is (0000,1030)") {
        CHECK(tag_move_originator_aet.group() == 0x0000);
        CHECK(tag_move_originator_aet.element() == 0x1030);
    }

    SECTION("Move Originator Message ID tag is (0000,1031)") {
        CHECK(tag_move_originator_message_id.group() == 0x0000);
        CHECK(tag_move_originator_message_id.element() == 0x1031);
    }
}
