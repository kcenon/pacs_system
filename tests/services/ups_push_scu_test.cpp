/**
 * @file ups_push_scu_test.cpp
 * @brief Unit tests for UPS Push SCU service
 */

#include <pacs/services/ups/ups_push_scu.hpp>
#include <pacs/network/dimse/command_field.hpp>
#include <pacs/network/dimse/dimse_message.hpp>
#include <pacs/network/dimse/status_codes.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace pacs::services;
using namespace pacs::network;
using namespace pacs::network::dimse;
using namespace pacs::core;

// ============================================================================
// ups_push_scu Construction Tests
// ============================================================================

TEST_CASE("ups_push_scu construction", "[services][ups][scu]") {
    SECTION("default construction succeeds") {
        ups_push_scu scu;
        CHECK(scu.creates_performed() == 0);
        CHECK(scu.sets_performed() == 0);
        CHECK(scu.gets_performed() == 0);
        CHECK(scu.actions_performed() == 0);
    }

    SECTION("construction with config succeeds") {
        ups_push_scu_config config;
        config.timeout = std::chrono::milliseconds{60000};
        config.auto_generate_uid = false;

        ups_push_scu scu(config);
        CHECK(scu.creates_performed() == 0);
        CHECK(scu.sets_performed() == 0);
        CHECK(scu.gets_performed() == 0);
        CHECK(scu.actions_performed() == 0);
    }

    SECTION("construction with nullptr logger succeeds") {
        ups_push_scu scu(nullptr);
        CHECK(scu.creates_performed() == 0);
    }

    SECTION("construction with config and nullptr logger succeeds") {
        ups_push_scu_config config;
        ups_push_scu scu(config, nullptr);
        CHECK(scu.creates_performed() == 0);
    }
}

// ============================================================================
// ups_push_scu_config Tests
// ============================================================================

TEST_CASE("ups_push_scu_config defaults", "[services][ups][scu]") {
    ups_push_scu_config config;

    CHECK(config.timeout == std::chrono::milliseconds{30000});
    CHECK(config.auto_generate_uid == true);
}

TEST_CASE("ups_push_scu_config customization", "[services][ups][scu]") {
    ups_push_scu_config config;
    config.timeout = std::chrono::milliseconds{60000};
    config.auto_generate_uid = false;

    CHECK(config.timeout == std::chrono::milliseconds{60000});
    CHECK(config.auto_generate_uid == false);
}

// ============================================================================
// ups_create_data Structure Tests
// ============================================================================

TEST_CASE("ups_create_data structure", "[services][ups][scu]") {
    ups_create_data data;

    SECTION("default construction has correct defaults") {
        CHECK(data.workitem_uid.empty());
        CHECK(data.procedure_step_label.empty());
        CHECK(data.worklist_label.empty());
        CHECK(data.priority == "MEDIUM");
        CHECK(data.scheduled_start_datetime.empty());
        CHECK(data.expected_completion_datetime.empty());
        CHECK(data.scheduled_station_name.empty());
        CHECK(data.input_information.empty());
    }

    SECTION("can be initialized with values") {
        data.workitem_uid = "1.2.3.4.5.6.7.8";
        data.procedure_step_label = "AI Analysis";
        data.priority = "HIGH";
        data.scheduled_station_name = "AI_ENGINE_01";

        CHECK(data.workitem_uid == "1.2.3.4.5.6.7.8");
        CHECK(data.procedure_step_label == "AI Analysis");
        CHECK(data.priority == "HIGH");
        CHECK(data.scheduled_station_name == "AI_ENGINE_01");
    }
}

// ============================================================================
// ups_set_data Structure Tests
// ============================================================================

TEST_CASE("ups_set_data structure", "[services][ups][scu]") {
    ups_set_data data;

    SECTION("default construction") {
        CHECK(data.workitem_uid.empty());
    }

    SECTION("can be initialized with modifications") {
        data.workitem_uid = "1.2.3.4.5.6.7.8";

        CHECK(data.workitem_uid == "1.2.3.4.5.6.7.8");
    }
}

// ============================================================================
// ups_get_data Structure Tests
// ============================================================================

TEST_CASE("ups_get_data structure", "[services][ups][scu]") {
    ups_get_data data;

    SECTION("default construction") {
        CHECK(data.workitem_uid.empty());
        CHECK(data.attribute_tags.empty());
    }

    SECTION("can specify attribute tags") {
        data.workitem_uid = "1.2.3.4.5.6.7.8";
        data.attribute_tags = {
            ups_tags::procedure_step_state,
            ups_tags::procedure_step_label,
            ups_tags::procedure_step_progress
        };

        CHECK(data.workitem_uid == "1.2.3.4.5.6.7.8");
        CHECK(data.attribute_tags.size() == 3);
    }
}

// ============================================================================
// ups_change_state_data Structure Tests
// ============================================================================

TEST_CASE("ups_change_state_data structure", "[services][ups][scu]") {
    ups_change_state_data data;

    SECTION("default construction") {
        CHECK(data.workitem_uid.empty());
        CHECK(data.requested_state.empty());
        CHECK(data.transaction_uid.empty());
    }

    SECTION("can be initialized for claiming") {
        data.workitem_uid = "1.2.3.4.5.6.7.8";
        data.requested_state = "IN PROGRESS";
        data.transaction_uid = "1.2.3.4.5.6.7.8.9";

        CHECK(data.workitem_uid == "1.2.3.4.5.6.7.8");
        CHECK(data.requested_state == "IN PROGRESS");
        CHECK(data.transaction_uid == "1.2.3.4.5.6.7.8.9");
    }

    SECTION("can be initialized for completion") {
        data.workitem_uid = "1.2.3.4.5.6.7.8";
        data.requested_state = "COMPLETED";
        data.transaction_uid = "1.2.3.4.5.6.7.8.9";

        CHECK(data.requested_state == "COMPLETED");
    }

    SECTION("can be initialized for cancellation") {
        data.workitem_uid = "1.2.3.4.5.6.7.8";
        data.requested_state = "CANCELED";
        data.transaction_uid = "1.2.3.4.5.6.7.8.9";

        CHECK(data.requested_state == "CANCELED");
    }
}

// ============================================================================
// ups_request_cancel_data Structure Tests
// ============================================================================

TEST_CASE("ups_request_cancel_data structure", "[services][ups][scu]") {
    ups_request_cancel_data data;

    SECTION("default construction") {
        CHECK(data.workitem_uid.empty());
        CHECK(data.reason.empty());
    }

    SECTION("can be initialized with reason") {
        data.workitem_uid = "1.2.3.4.5.6.7.8";
        data.reason = "Patient refused procedure";

        CHECK(data.workitem_uid == "1.2.3.4.5.6.7.8");
        CHECK(data.reason == "Patient refused procedure");
    }

    SECTION("can be initialized without reason") {
        data.workitem_uid = "1.2.3.4.5.6.7.8";

        CHECK(data.workitem_uid == "1.2.3.4.5.6.7.8");
        CHECK(data.reason.empty());
    }
}

// ============================================================================
// ups_result Structure Tests
// ============================================================================

TEST_CASE("ups_result structure", "[services][ups][scu]") {
    ups_result result;

    SECTION("default construction") {
        CHECK(result.workitem_uid.empty());
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
        result.status = 0xC310;
        CHECK_FALSE(result.is_success());
        CHECK_FALSE(result.is_warning());
        CHECK(result.is_error());

        result.status = 0xA700;
        CHECK(result.is_error());

        result.status = 0x0110;
        CHECK(result.is_error());
    }

    SECTION("can store elapsed time") {
        result.elapsed = std::chrono::milliseconds{250};
        CHECK(result.elapsed.count() == 250);
    }
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_CASE("ups_push_scu statistics", "[services][ups][scu]") {
    ups_push_scu scu;

    SECTION("initial statistics are zero") {
        CHECK(scu.creates_performed() == 0);
        CHECK(scu.sets_performed() == 0);
        CHECK(scu.gets_performed() == 0);
        CHECK(scu.actions_performed() == 0);
    }

    SECTION("reset_statistics clears all counters") {
        scu.reset_statistics();
        CHECK(scu.creates_performed() == 0);
        CHECK(scu.sets_performed() == 0);
        CHECK(scu.gets_performed() == 0);
        CHECK(scu.actions_performed() == 0);
    }
}

// ============================================================================
// Multiple Instance Tests
// ============================================================================

TEST_CASE("multiple ups_push_scu instances are independent", "[services][ups][scu]") {
    ups_push_scu scu1;
    ups_push_scu scu2;

    CHECK(scu1.creates_performed() == scu2.creates_performed());
    CHECK(scu1.sets_performed() == scu2.sets_performed());
    CHECK(scu1.gets_performed() == scu2.gets_performed());
    CHECK(scu1.actions_performed() == scu2.actions_performed());

    scu1.reset_statistics();
    CHECK(scu1.creates_performed() == 0);
    CHECK(scu2.creates_performed() == 0);
}

// ============================================================================
// SOP Class UID Constant Test
// ============================================================================

TEST_CASE("ups_push_sop_class_uid is correct", "[services][ups][scu]") {
    CHECK(ups_push_sop_class_uid == "1.2.840.10008.5.1.4.34.6.1");
}

// ============================================================================
// N-ACTION Type Constants Test
// ============================================================================

TEST_CASE("UPS N-ACTION type constants", "[services][ups][scu]") {
    CHECK(ups_action_change_state == 1);
    CHECK(ups_action_request_cancel == 3);
}

// ============================================================================
// UPS Tags Tests
// ============================================================================

TEST_CASE("ups_tags namespace contains UPS-specific tags", "[services][ups][scu]") {
    using namespace ups_tags;

    SECTION("procedure step state tag") {
        CHECK(procedure_step_state == dicom_tag{0x0074, 0x1000});
    }

    SECTION("procedure step progress tag") {
        CHECK(procedure_step_progress == dicom_tag{0x0074, 0x1004});
    }

    SECTION("scheduled procedure step priority tag") {
        CHECK(scheduled_procedure_step_priority == dicom_tag{0x0074, 0x1200});
    }

    SECTION("worklist label tag") {
        CHECK(worklist_label == dicom_tag{0x0074, 0x1202});
    }

    SECTION("procedure step label tag") {
        CHECK(procedure_step_label == dicom_tag{0x0074, 0x1204});
    }

    SECTION("transaction uid tag") {
        CHECK(transaction_uid == dicom_tag{0x0008, 0x1195});
    }

    SECTION("input information sequence tag") {
        CHECK(input_information_sequence == dicom_tag{0x0040, 0x4021});
    }

    SECTION("reason for cancellation tag") {
        CHECK(reason_for_cancellation == dicom_tag{0x0074, 0x1238});
    }
}

// ============================================================================
// Data Structure Copy and Move Tests
// ============================================================================

TEST_CASE("ups_create_data is copyable and movable", "[services][ups][scu]") {
    ups_create_data data;
    data.workitem_uid = "1.2.3.4.5.6.7.8";
    data.procedure_step_label = "AI Analysis";
    data.priority = "HIGH";

    SECTION("copy construction") {
        ups_create_data copy = data;
        CHECK(copy.workitem_uid == "1.2.3.4.5.6.7.8");
        CHECK(copy.procedure_step_label == "AI Analysis");
        CHECK(copy.priority == "HIGH");
    }

    SECTION("move construction") {
        ups_create_data moved = std::move(data);
        CHECK(moved.workitem_uid == "1.2.3.4.5.6.7.8");
        CHECK(moved.procedure_step_label == "AI Analysis");
        CHECK(moved.priority == "HIGH");
    }
}

TEST_CASE("ups_change_state_data is copyable and movable", "[services][ups][scu]") {
    ups_change_state_data data;
    data.workitem_uid = "1.2.3.4.5.6.7.8";
    data.requested_state = "IN PROGRESS";
    data.transaction_uid = "1.2.3.4.5.6.7.8.9";

    SECTION("copy construction") {
        ups_change_state_data copy = data;
        CHECK(copy.workitem_uid == "1.2.3.4.5.6.7.8");
        CHECK(copy.requested_state == "IN PROGRESS");
        CHECK(copy.transaction_uid == "1.2.3.4.5.6.7.8.9");
    }

    SECTION("move construction") {
        ups_change_state_data moved = std::move(data);
        CHECK(moved.workitem_uid == "1.2.3.4.5.6.7.8");
        CHECK(moved.requested_state == "IN PROGRESS");
        CHECK(moved.transaction_uid == "1.2.3.4.5.6.7.8.9");
    }
}

TEST_CASE("ups_result is copyable and movable", "[services][ups][scu]") {
    ups_result result;
    result.workitem_uid = "1.2.3.4.5.6.7.8";
    result.status = 0x0000;
    result.elapsed = std::chrono::milliseconds{100};

    SECTION("copy construction") {
        ups_result copy = result;
        CHECK(copy.workitem_uid == "1.2.3.4.5.6.7.8");
        CHECK(copy.status == 0x0000);
        CHECK(copy.elapsed.count() == 100);
    }

    SECTION("move construction") {
        ups_result moved = std::move(result);
        CHECK(moved.workitem_uid == "1.2.3.4.5.6.7.8");
        CHECK(moved.status == 0x0000);
        CHECK(moved.elapsed.count() == 100);
    }
}
