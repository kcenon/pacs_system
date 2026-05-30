/**
 * @file ups_push_scp_test.cpp
 * @brief Unit tests for UPS (Unified Procedure Step) Push SCP service
 */

#include <kcenon/pacs/services/ups/ups_push_scp.h>
#include <kcenon/pacs/network/dimse/command_field.h>
#include <kcenon/pacs/network/dimse/dimse_message.h>
#include <kcenon/pacs/network/dimse/status_codes.h>
#include <kcenon/pacs/storage/ups_workitem.h>

#include <catch2/catch_test_macros.hpp>

using namespace kcenon::pacs::services;
using namespace kcenon::pacs::network;
using namespace kcenon::pacs::network::dimse;
using namespace kcenon::pacs::core;
using namespace kcenon::pacs::storage;

// ============================================================================
// ups_push_scp Construction Tests
// ============================================================================

TEST_CASE("ups_push_scp construction", "[services][ups]") {
    ups_push_scp scp;

    SECTION("service name is correct") {
        CHECK(scp.service_name() == "UPS Push SCP");
    }

    SECTION("supports exactly one SOP class") {
        auto classes = scp.supported_sop_classes();
        CHECK(classes.size() == 1);
    }

    SECTION("supports UPS Push SOP Class") {
        auto classes = scp.supported_sop_classes();
        REQUIRE(classes.size() == 1);
        CHECK(classes[0] == "1.2.840.10008.5.1.4.34.6.1");
    }
}

// ============================================================================
// SOP Class Support Tests
// ============================================================================

TEST_CASE("ups_push_scp SOP class support", "[services][ups]") {
    ups_push_scp scp;

    SECTION("supports UPS Push SOP Class UID") {
        CHECK(scp.supports_sop_class("1.2.840.10008.5.1.4.34.6.1"));
        CHECK(scp.supports_sop_class(ups_push_sop_class_uid));
    }

    SECTION("does not support other SOP classes") {
        // Verification SOP Class
        CHECK_FALSE(scp.supports_sop_class("1.2.840.10008.1.1"));
        // MPPS SOP Class
        CHECK_FALSE(scp.supports_sop_class("1.2.840.10008.3.1.2.3.3"));
        // UPS Watch SOP Class
        CHECK_FALSE(scp.supports_sop_class("1.2.840.10008.5.1.4.34.6.2"));
        // UPS Pull SOP Class
        CHECK_FALSE(scp.supports_sop_class("1.2.840.10008.5.1.4.34.6.3"));
        // Empty string
        CHECK_FALSE(scp.supports_sop_class(""));
        // Random UID
        CHECK_FALSE(scp.supports_sop_class("1.2.3.4.5.6.7.8.9"));
    }
}

// ============================================================================
// UPS Push SOP Class UID Constant Test
// ============================================================================

TEST_CASE("ups_push_sop_class_uid constant", "[services][ups]") {
    CHECK(ups_push_sop_class_uid == "1.2.840.10008.5.1.4.34.6.1");
}

// ============================================================================
// N-ACTION Type Constants Tests
// ============================================================================

TEST_CASE("UPS N-ACTION type constants", "[services][ups]") {
    CHECK(ups_action_change_state == 1);
    CHECK(ups_action_request_cancel == 3);
}

// ============================================================================
// UPS State Tests (from ups_workitem.hpp)
// ============================================================================

TEST_CASE("ups_state to_string conversion", "[services][ups]") {
    CHECK(to_string(ups_state::scheduled) == "SCHEDULED");
    CHECK(to_string(ups_state::in_progress) == "IN PROGRESS");
    CHECK(to_string(ups_state::completed) == "COMPLETED");
    CHECK(to_string(ups_state::canceled) == "CANCELED");
}

TEST_CASE("parse_ups_state conversion", "[services][ups]") {
    SECTION("valid state strings") {
        auto scheduled = parse_ups_state("SCHEDULED");
        REQUIRE(scheduled.has_value());
        CHECK(scheduled.value() == ups_state::scheduled);

        auto in_progress = parse_ups_state("IN PROGRESS");
        REQUIRE(in_progress.has_value());
        CHECK(in_progress.value() == ups_state::in_progress);

        auto completed = parse_ups_state("COMPLETED");
        REQUIRE(completed.has_value());
        CHECK(completed.value() == ups_state::completed);

        auto canceled = parse_ups_state("CANCELED");
        REQUIRE(canceled.has_value());
        CHECK(canceled.value() == ups_state::canceled);
    }

    SECTION("invalid state strings return nullopt") {
        CHECK_FALSE(parse_ups_state("").has_value());
        CHECK_FALSE(parse_ups_state("INVALID").has_value());
        CHECK_FALSE(parse_ups_state("scheduled").has_value());  // case sensitive
        CHECK_FALSE(parse_ups_state("IN_PROGRESS").has_value());  // wrong format
    }
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_CASE("ups_push_scp statistics", "[services][ups]") {
    ups_push_scp scp;

    SECTION("initial statistics are zero") {
        CHECK(scp.creates_processed() == 0);
        CHECK(scp.sets_processed() == 0);
        CHECK(scp.gets_processed() == 0);
        CHECK(scp.actions_processed() == 0);
        CHECK(scp.state_changes() == 0);
        CHECK(scp.cancel_requests() == 0);
    }

    SECTION("reset_statistics clears all counters") {
        scp.reset_statistics();
        CHECK(scp.creates_processed() == 0);
        CHECK(scp.sets_processed() == 0);
        CHECK(scp.gets_processed() == 0);
        CHECK(scp.actions_processed() == 0);
        CHECK(scp.state_changes() == 0);
        CHECK(scp.cancel_requests() == 0);
    }
}

// ============================================================================
// Handler Configuration Tests
// ============================================================================

TEST_CASE("ups_push_scp handler configuration", "[services][ups]") {
    ups_push_scp scp;

    SECTION("can set create handler") {
        bool handler_called = false;
        scp.set_create_handler([&handler_called](const ups_workitem& /*wi*/) {
            handler_called = true;
            return Result<std::monostate>{std::monostate{}};
        });
        CHECK_FALSE(handler_called);
    }

    SECTION("can set set handler") {
        bool handler_called = false;
        scp.set_set_handler([&handler_called](
            const std::string& /*uid*/,
            const dicom_dataset& /*mods*/) {
            handler_called = true;
            return Result<std::monostate>{std::monostate{}};
        });
        CHECK_FALSE(handler_called);
    }

    SECTION("can set get handler") {
        bool handler_called = false;
        scp.set_get_handler([&handler_called](const std::string& /*uid*/) {
            handler_called = true;
            return Result<ups_workitem>{ups_workitem{}};
        });
        CHECK_FALSE(handler_called);
    }

    SECTION("can set change state handler") {
        bool handler_called = false;
        scp.set_change_state_handler([&handler_called](
            const std::string& /*uid*/,
            const std::string& /*state*/,
            const std::string& /*txn_uid*/) {
            handler_called = true;
            return Result<std::monostate>{std::monostate{}};
        });
        CHECK_FALSE(handler_called);
    }

    SECTION("can set request cancel handler") {
        bool handler_called = false;
        scp.set_request_cancel_handler([&handler_called](
            const std::string& /*uid*/,
            const std::string& /*reason*/) {
            handler_called = true;
            return Result<std::monostate>{std::monostate{}};
        });
        CHECK_FALSE(handler_called);
    }
}

// ============================================================================
// ups_workitem Structure Tests
// ============================================================================

TEST_CASE("ups_workitem structure", "[services][ups]") {
    ups_workitem workitem;

    SECTION("default construction") {
        CHECK(workitem.workitem_uid.empty());
        CHECK(workitem.state.empty());
        CHECK(workitem.pk == 0);
        CHECK(workitem.progress_percent == 0);
        CHECK_FALSE(workitem.is_valid());
    }

    SECTION("can be initialized") {
        workitem.workitem_uid = "1.2.3.4.5.6";
        workitem.state = "SCHEDULED";
        workitem.procedure_step_label = "CT Scan";
        workitem.priority = "MEDIUM";

        CHECK(workitem.is_valid());
        CHECK_FALSE(workitem.is_final());
        CHECK(workitem.get_state().has_value());
        CHECK(workitem.get_state().value() == ups_state::scheduled);
    }

    SECTION("is_final for terminal states") {
        workitem.state = "COMPLETED";
        CHECK(workitem.is_final());

        workitem.state = "CANCELED";
        CHECK(workitem.is_final());

        workitem.state = "SCHEDULED";
        CHECK_FALSE(workitem.is_final());

        workitem.state = "IN PROGRESS";
        CHECK_FALSE(workitem.is_final());
    }
}

// ============================================================================
// UPS Tags Tests
// ============================================================================

TEST_CASE("ups_tags namespace contains UPS-specific tags", "[services][ups]") {
    using namespace ups_tags;

    SECTION("procedure step tags") {
        CHECK(procedure_step_state == dicom_tag{0x0074, 0x1000});
        CHECK(procedure_step_progress == dicom_tag{0x0074, 0x1004});
        CHECK(procedure_step_label == dicom_tag{0x0074, 0x1204});
    }

    SECTION("scheduling tags") {
        CHECK(scheduled_procedure_step_priority == dicom_tag{0x0074, 0x1200});
        CHECK(worklist_label == dicom_tag{0x0074, 0x1202});
    }

    SECTION("transaction and input tags") {
        CHECK(transaction_uid == dicom_tag{0x0008, 0x1195});
        CHECK(input_information_sequence == dicom_tag{0x0040, 0x4021});
    }

    SECTION("cancellation tags") {
        CHECK(reason_for_cancellation == dicom_tag{0x0074, 0x1238});
    }
}

// ============================================================================
// scp_service Base Class Tests
// ============================================================================

TEST_CASE("ups_push_scp is a scp_service", "[services][ups]") {
    std::unique_ptr<scp_service> base_ptr = std::make_unique<ups_push_scp>();

    CHECK(base_ptr->service_name() == "UPS Push SCP");
    CHECK(base_ptr->supported_sop_classes().size() == 1);
    CHECK(base_ptr->supports_sop_class("1.2.840.10008.5.1.4.34.6.1"));
}

// ============================================================================
// Multiple Instance Tests
// ============================================================================

TEST_CASE("multiple ups_push_scp instances are independent", "[services][ups]") {
    ups_push_scp scp1;
    ups_push_scp scp2;

    CHECK(scp1.service_name() == scp2.service_name());
    CHECK(scp1.supported_sop_classes() == scp2.supported_sop_classes());
    CHECK(scp1.supports_sop_class("1.2.840.10008.5.1.4.34.6.1") ==
          scp2.supports_sop_class("1.2.840.10008.5.1.4.34.6.1"));

    // Statistics should be independent
    scp1.reset_statistics();
    CHECK(scp1.creates_processed() == scp2.creates_processed());
    CHECK(scp1.actions_processed() == scp2.actions_processed());
}

// ============================================================================
// Command Field Tests
// ============================================================================

TEST_CASE("UPS-relevant DIMSE-N command fields", "[services][ups]") {
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

    SECTION("N-GET command fields") {
        CHECK(static_cast<uint16_t>(command_field::n_get_rq) == 0x0110);
        CHECK(static_cast<uint16_t>(command_field::n_get_rsp) == 0x8110);
        CHECK(is_request(command_field::n_get_rq));
        CHECK(is_response(command_field::n_get_rsp));
    }

    SECTION("N-ACTION command fields") {
        CHECK(static_cast<uint16_t>(command_field::n_action_rq) == 0x0130);
        CHECK(static_cast<uint16_t>(command_field::n_action_rsp) == 0x8130);
        CHECK(is_request(command_field::n_action_rq));
        CHECK(is_response(command_field::n_action_rsp));
    }

    SECTION("all are DIMSE-N commands") {
        CHECK(is_dimse_n(command_field::n_create_rq));
        CHECK(is_dimse_n(command_field::n_set_rq));
        CHECK(is_dimse_n(command_field::n_get_rq));
        CHECK(is_dimse_n(command_field::n_action_rq));
    }
}

// ============================================================================
// UPS Priority Tests
// ============================================================================

TEST_CASE("ups_priority to_string conversion", "[services][ups]") {
    CHECK(to_string(ups_priority::low) == "LOW");
    CHECK(to_string(ups_priority::medium) == "MEDIUM");
    CHECK(to_string(ups_priority::high) == "HIGH");
}

TEST_CASE("parse_ups_priority conversion", "[services][ups]") {
    SECTION("valid priority strings") {
        auto low = parse_ups_priority("LOW");
        REQUIRE(low.has_value());
        CHECK(low.value() == ups_priority::low);

        auto medium = parse_ups_priority("MEDIUM");
        REQUIRE(medium.has_value());
        CHECK(medium.value() == ups_priority::medium);

        auto high = parse_ups_priority("HIGH");
        REQUIRE(high.has_value());
        CHECK(high.value() == ups_priority::high);
    }

    SECTION("invalid priority strings return nullopt") {
        CHECK_FALSE(parse_ups_priority("").has_value());
        CHECK_FALSE(parse_ups_priority("INVALID").has_value());
        CHECK_FALSE(parse_ups_priority("low").has_value());  // case sensitive
    }
}

// ============================================================================
// DIMSE Status Code Tests (UPS-relevant)
// ============================================================================

TEST_CASE("DIMSE status codes used by UPS Push SCP", "[services][ups]") {
    CHECK(status_success == 0x0000);
    CHECK(status_refused_sop_class_not_supported == 0x0122);
    CHECK(status_error_missing_attribute == 0x0120);
    CHECK(status_error_cannot_understand == 0xC000);
    CHECK(status_error_unable_to_process == 0xC001);
    CHECK(status_error_invalid_object_instance == 0x0117);
    CHECK(status_error_no_such_action_type == 0x0123);
}
