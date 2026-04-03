/**
 * @file ups_watch_scp_test.cpp
 * @brief Unit tests for UPS Watch SCP service (subscription and event notification)
 */

#include <kcenon/pacs/services/ups/ups_watch_scp.h>
#include <kcenon/pacs/services/ups/ups_push_scp.h>
#include <kcenon/pacs/network/dimse/command_field.h>
#include <kcenon/pacs/network/dimse/dimse_message.h>
#include <kcenon/pacs/network/dimse/status_codes.h>
#include <kcenon/pacs/core/dicom_tag_constants.h>
#include <kcenon/pacs/encoding/vr_type.h>

#include <catch2/catch_test_macros.hpp>

using namespace kcenon::pacs::services;
using namespace kcenon::pacs::network;
using namespace kcenon::pacs::network::dimse;
using namespace kcenon::pacs::core;
using namespace kcenon::pacs::encoding;

// ============================================================================
// ups_watch_scp Construction Tests
// ============================================================================

TEST_CASE("ups_watch_scp construction", "[services][ups][watch]") {
    ups_watch_scp scp;

    SECTION("service name is correct") {
        CHECK(scp.service_name() == "UPS Watch SCP");
    }

    SECTION("supports one SOP class") {
        auto classes = scp.supported_sop_classes();
        CHECK(classes.size() == 1);
    }

    SECTION("initial subscriptions_created is zero") {
        CHECK(scp.subscriptions_created() == 0);
    }

    SECTION("initial subscriptions_removed is zero") {
        CHECK(scp.subscriptions_removed() == 0);
    }

    SECTION("initial events_sent is zero") {
        CHECK(scp.events_sent() == 0);
    }
}

// ============================================================================
// SOP Class Support Tests
// ============================================================================

TEST_CASE("ups_watch_scp SOP class support", "[services][ups][watch]") {
    ups_watch_scp scp;

    SECTION("supports UPS Watch SOP Class") {
        CHECK(scp.supports_sop_class("1.2.840.10008.5.1.4.34.6.2"));
        CHECK(scp.supports_sop_class(ups_watch_sop_class_uid));
    }

    SECTION("does not support UPS Push SOP Class") {
        CHECK_FALSE(scp.supports_sop_class("1.2.840.10008.5.1.4.34.6.1"));
    }

    SECTION("does not support UPS Query SOP Class") {
        CHECK_FALSE(scp.supports_sop_class("1.2.840.10008.5.1.4.34.6.4"));
    }

    SECTION("does not support non-UPS SOP classes") {
        CHECK_FALSE(scp.supports_sop_class("1.2.840.10008.1.1"));
        CHECK_FALSE(scp.supports_sop_class(""));
    }
}

// ============================================================================
// SOP Class UID Constant Tests
// ============================================================================

TEST_CASE("ups_watch_sop_class_uid constant", "[services][ups][watch]") {
    CHECK(ups_watch_sop_class_uid == "1.2.840.10008.5.1.4.34.6.2");
}

TEST_CASE("ups_global_subscription_instance_uid constant",
          "[services][ups][watch]") {
    CHECK(ups_global_subscription_instance_uid ==
          "1.2.840.10008.5.1.4.34.5");
}

// ============================================================================
// N-ACTION Type Constant Tests
// ============================================================================

TEST_CASE("UPS Watch N-ACTION type constants", "[services][ups][watch]") {
    CHECK(ups_watch_action_subscribe == 3);
    CHECK(ups_watch_action_unsubscribe == 4);
    CHECK(ups_watch_action_suspend_global == 5);
}

// ============================================================================
// N-EVENT-REPORT Event Type Constant Tests
// ============================================================================

TEST_CASE("UPS N-EVENT-REPORT event type constants",
          "[services][ups][watch]") {
    CHECK(ups_event_state_report == 1);
    CHECK(ups_event_cancel_requested == 2);
    CHECK(ups_event_progress_report == 3);
    CHECK(ups_event_scp_status_change == 4);
}

// ============================================================================
// Configuration Tests
// ============================================================================

TEST_CASE("ups_watch_scp configuration", "[services][ups][watch]") {
    ups_watch_scp scp;

    SECTION("set_subscribe_handler accepts lambda") {
        bool handler_called = false;
        scp.set_subscribe_handler([&handler_called](
            [[maybe_unused]] const std::string& ae,
            [[maybe_unused]] const std::string& uid,
            [[maybe_unused]] bool deletion_lock) {
            handler_called = true;
            return Result<std::monostate>(std::monostate{});
        });
        CHECK_FALSE(handler_called);
    }

    SECTION("set_unsubscribe_handler accepts lambda") {
        bool handler_called = false;
        scp.set_unsubscribe_handler([&handler_called](
            [[maybe_unused]] const std::string& ae,
            [[maybe_unused]] const std::string& uid) {
            handler_called = true;
            return Result<std::monostate>(std::monostate{});
        });
        CHECK_FALSE(handler_called);
    }

    SECTION("set_get_subscribers_handler accepts lambda") {
        bool handler_called = false;
        scp.set_get_subscribers_handler([&handler_called](
            [[maybe_unused]] const std::string& uid) {
            handler_called = true;
            return Result<std::vector<std::string>>(
                std::vector<std::string>{});
        });
        CHECK_FALSE(handler_called);
    }

    SECTION("set_event_callback accepts lambda") {
        bool callback_called = false;
        scp.set_event_callback([&callback_called](
            [[maybe_unused]] const std::string& ae,
            [[maybe_unused]] uint16_t event_type,
            [[maybe_unused]] const std::string& uid,
            [[maybe_unused]] const dicom_dataset& info) {
            callback_called = true;
        });
        CHECK_FALSE(callback_called);
    }
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_CASE("ups_watch_scp statistics", "[services][ups][watch]") {
    ups_watch_scp scp;

    SECTION("statistics start at zero") {
        CHECK(scp.subscriptions_created() == 0);
        CHECK(scp.subscriptions_removed() == 0);
        CHECK(scp.events_sent() == 0);
    }

    SECTION("reset_statistics resets all counters") {
        scp.reset_statistics();
        CHECK(scp.subscriptions_created() == 0);
        CHECK(scp.subscriptions_removed() == 0);
        CHECK(scp.events_sent() == 0);
    }
}

// ============================================================================
// Handler Function Type Tests
// ============================================================================

TEST_CASE("ups_subscribe_handler function type", "[services][ups][watch]") {
    ups_subscribe_handler handler = [](
        const std::string& subscriber_ae,
        const std::string& workitem_uid,
        bool deletion_lock) {
        CHECK(subscriber_ae == "AI_ENGINE");
        CHECK(workitem_uid == "1.2.3.4.5");
        CHECK_FALSE(deletion_lock);
        return Result<std::monostate>(std::monostate{});
    };

    auto result = handler("AI_ENGINE", "1.2.3.4.5", false);
    CHECK(result.is_ok());
}

TEST_CASE("ups_unsubscribe_handler function type",
          "[services][ups][watch]") {
    ups_unsubscribe_handler handler = [](
        const std::string& subscriber_ae,
        const std::string& workitem_uid) {
        CHECK(subscriber_ae == "AI_ENGINE");
        CHECK(workitem_uid == "1.2.3.4.5");
        return Result<std::monostate>(std::monostate{});
    };

    auto result = handler("AI_ENGINE", "1.2.3.4.5");
    CHECK(result.is_ok());
}

TEST_CASE("ups_get_subscribers_handler function type",
          "[services][ups][watch]") {
    ups_get_subscribers_handler handler = [](
        [[maybe_unused]] const std::string& workitem_uid) {
        return Result<std::vector<std::string>>(
            std::vector<std::string>{"AI_ENGINE", "PACS_SERVER"});
    };

    auto result = handler("1.2.3.4.5");
    CHECK(result.is_ok());
    CHECK(result.value().size() == 2);
    CHECK(result.value()[0] == "AI_ENGINE");
    CHECK(result.value()[1] == "PACS_SERVER");
}

TEST_CASE("ups_event_callback function type", "[services][ups][watch]") {
    std::string captured_ae;
    uint16_t captured_event_type = 0;
    std::string captured_uid;

    ups_event_callback callback = [&](
        const std::string& ae,
        uint16_t event_type,
        const std::string& uid,
        [[maybe_unused]] const dicom_dataset& info) {
        captured_ae = ae;
        captured_event_type = event_type;
        captured_uid = uid;
    };

    dicom_dataset info;
    callback("AI_ENGINE", ups_event_state_report, "1.2.3.4.5", info);

    CHECK(captured_ae == "AI_ENGINE");
    CHECK(captured_event_type == ups_event_state_report);
    CHECK(captured_uid == "1.2.3.4.5");
}

// ============================================================================
// Event Notification Tests
// ============================================================================

TEST_CASE("ups_watch_scp notify_state_change dispatches events",
          "[services][ups][watch]") {
    ups_watch_scp scp;

    std::vector<std::string> notified_aes;
    std::vector<uint16_t> notified_events;
    std::vector<std::string> notified_states;

    scp.set_get_subscribers_handler([](
        [[maybe_unused]] const std::string& uid) {
        return Result<std::vector<std::string>>(
            std::vector<std::string>{"AI_ENGINE", "PACS_SERVER"});
    });

    scp.set_event_callback([&](
        const std::string& ae,
        uint16_t event_type,
        [[maybe_unused]] const std::string& uid,
        const dicom_dataset& info) {
        notified_aes.push_back(ae);
        notified_events.push_back(event_type);
        notified_states.push_back(
            info.get_string(ups_tags::procedure_step_state));
    });

    scp.notify_state_change("1.2.3.4.5", "IN PROGRESS");

    REQUIRE(notified_aes.size() == 2);
    CHECK(notified_aes[0] == "AI_ENGINE");
    CHECK(notified_aes[1] == "PACS_SERVER");
    CHECK(notified_events[0] == ups_event_state_report);
    CHECK(notified_events[1] == ups_event_state_report);
    CHECK(notified_states[0] == "IN PROGRESS");
    CHECK(notified_states[1] == "IN PROGRESS");
    CHECK(scp.events_sent() == 2);
}

TEST_CASE("ups_watch_scp notify_cancel_requested dispatches events",
          "[services][ups][watch]") {
    ups_watch_scp scp;

    std::string captured_reason;

    scp.set_get_subscribers_handler([](
        [[maybe_unused]] const std::string& uid) {
        return Result<std::vector<std::string>>(
            std::vector<std::string>{"PERFORMER_AE"});
    });

    scp.set_event_callback([&](
        [[maybe_unused]] const std::string& ae,
        uint16_t event_type,
        [[maybe_unused]] const std::string& uid,
        const dicom_dataset& info) {
        CHECK(event_type == ups_event_cancel_requested);
        captured_reason =
            info.get_string(ups_tags::reason_for_cancellation);
    });

    scp.notify_cancel_requested("1.2.3.4.5", "Patient left");

    CHECK(captured_reason == "Patient left");
    CHECK(scp.events_sent() == 1);
}

TEST_CASE("ups_watch_scp notify_progress dispatches events",
          "[services][ups][watch]") {
    ups_watch_scp scp;

    std::string captured_progress;

    scp.set_get_subscribers_handler([](
        [[maybe_unused]] const std::string& uid) {
        return Result<std::vector<std::string>>(
            std::vector<std::string>{"MONITOR_AE"});
    });

    scp.set_event_callback([&](
        [[maybe_unused]] const std::string& ae,
        uint16_t event_type,
        [[maybe_unused]] const std::string& uid,
        const dicom_dataset& info) {
        CHECK(event_type == ups_event_progress_report);
        captured_progress =
            info.get_string(ups_tags::procedure_step_progress);
    });

    scp.notify_progress("1.2.3.4.5", 75);

    CHECK(captured_progress == "75");
    CHECK(scp.events_sent() == 1);
}

TEST_CASE("ups_watch_scp notify with no subscribers does nothing",
          "[services][ups][watch]") {
    ups_watch_scp scp;

    scp.set_get_subscribers_handler([](
        [[maybe_unused]] const std::string& uid) {
        return Result<std::vector<std::string>>(
            std::vector<std::string>{});
    });

    bool callback_called = false;
    scp.set_event_callback([&](
        [[maybe_unused]] const std::string& ae,
        [[maybe_unused]] uint16_t event_type,
        [[maybe_unused]] const std::string& uid,
        [[maybe_unused]] const dicom_dataset& info) {
        callback_called = true;
    });

    scp.notify_state_change("1.2.3.4.5", "COMPLETED");

    CHECK_FALSE(callback_called);
    CHECK(scp.events_sent() == 0);
}

TEST_CASE("ups_watch_scp notify without handlers configured",
          "[services][ups][watch]") {
    ups_watch_scp scp;

    // Should not crash when no handlers are set
    scp.notify_state_change("1.2.3.4.5", "COMPLETED");
    scp.notify_cancel_requested("1.2.3.4.5", "reason");
    scp.notify_progress("1.2.3.4.5", 50);

    CHECK(scp.events_sent() == 0);
}

// ============================================================================
// N-ACTION Message Factory Tests
// ============================================================================

TEST_CASE("make_n_action_rq for UPS Watch subscribe",
          "[services][ups][watch]") {
    auto request = make_n_action_rq(
        42, ups_watch_sop_class_uid,
        "1.2.3.4.5.6.7.8",
        ups_watch_action_subscribe);

    CHECK(request.command() == command_field::n_action_rq);
    CHECK(request.message_id() == 42);
    CHECK(request.is_request());
}

TEST_CASE("make_n_action_rsp for UPS Watch subscribe",
          "[services][ups][watch]") {
    auto response = make_n_action_rsp(
        42, ups_watch_sop_class_uid,
        "1.2.3.4.5.6.7.8",
        ups_watch_action_subscribe,
        status_success);

    CHECK(response.command() == command_field::n_action_rsp);
    CHECK(response.message_id_responded_to() == 42);
    CHECK(response.status() == status_success);
    CHECK(response.is_response());
}

TEST_CASE("make_n_action_rq for UPS Watch unsubscribe",
          "[services][ups][watch]") {
    auto request = make_n_action_rq(
        99, ups_watch_sop_class_uid,
        std::string(ups_global_subscription_instance_uid),
        ups_watch_action_unsubscribe);

    CHECK(request.command() == command_field::n_action_rq);
    CHECK(request.message_id() == 99);
}

TEST_CASE("make_n_action_rq for UPS Watch suspend global",
          "[services][ups][watch]") {
    auto request = make_n_action_rq(
        100, ups_watch_sop_class_uid,
        std::string(ups_global_subscription_instance_uid),
        ups_watch_action_suspend_global);

    CHECK(request.command() == command_field::n_action_rq);
    CHECK(request.message_id() == 100);
}

// ============================================================================
// N-EVENT-REPORT Message Factory Tests
// ============================================================================

TEST_CASE("make_n_event_report_rq for UPS state change",
          "[services][ups][watch]") {
    auto report = make_n_event_report_rq(
        1, ups_watch_sop_class_uid,
        "1.2.3.4.5.6.7.8",
        ups_event_state_report);

    CHECK(report.command() == command_field::n_event_report_rq);
    CHECK(report.message_id() == 1);
    CHECK(report.is_request());
}

TEST_CASE("make_n_event_report_rsp for UPS state change",
          "[services][ups][watch]") {
    auto response = make_n_event_report_rsp(
        1, ups_watch_sop_class_uid,
        "1.2.3.4.5.6.7.8",
        ups_event_state_report,
        status_success);

    CHECK(response.command() == command_field::n_event_report_rsp);
    CHECK(response.message_id_responded_to() == 1);
    CHECK(response.status() == status_success);
    CHECK(response.is_response());
}

TEST_CASE("make_n_event_report_rq for UPS cancel requested",
          "[services][ups][watch]") {
    auto report = make_n_event_report_rq(
        2, ups_watch_sop_class_uid,
        "1.2.3.4.5.6.7.8",
        ups_event_cancel_requested);

    CHECK(report.command() == command_field::n_event_report_rq);
    CHECK(report.message_id() == 2);
}

TEST_CASE("make_n_event_report_rq for UPS progress report",
          "[services][ups][watch]") {
    auto report = make_n_event_report_rq(
        3, ups_watch_sop_class_uid,
        "1.2.3.4.5.6.7.8",
        ups_event_progress_report);

    CHECK(report.command() == command_field::n_event_report_rq);
    CHECK(report.message_id() == 3);
}

// ============================================================================
// scp_service Base Class Tests
// ============================================================================

TEST_CASE("ups_watch_scp is a scp_service", "[services][ups][watch]") {
    std::unique_ptr<scp_service> base_ptr =
        std::make_unique<ups_watch_scp>();

    CHECK(base_ptr->service_name() == "UPS Watch SCP");
    CHECK(base_ptr->supported_sop_classes().size() == 1);
    CHECK(base_ptr->supports_sop_class(ups_watch_sop_class_uid));
}

// ============================================================================
// Multiple Instance Tests
// ============================================================================

TEST_CASE("multiple ups_watch_scp instances are independent",
          "[services][ups][watch]") {
    ups_watch_scp scp1;
    ups_watch_scp scp2;

    scp1.set_get_subscribers_handler([](
        [[maybe_unused]] const std::string& uid) {
        return Result<std::vector<std::string>>(
            std::vector<std::string>{"AE1"});
    });

    bool scp1_callback = false;
    scp1.set_event_callback([&scp1_callback](
        [[maybe_unused]] const std::string& ae,
        [[maybe_unused]] uint16_t event_type,
        [[maybe_unused]] const std::string& uid,
        [[maybe_unused]] const dicom_dataset& info) {
        scp1_callback = true;
    });

    scp1.notify_state_change("1.2.3", "COMPLETED");

    CHECK(scp1_callback);
    CHECK(scp1.events_sent() == 1);
    CHECK(scp2.events_sent() == 0);
}

// ============================================================================
// Event Dataset Content Tests
// ============================================================================

TEST_CASE("notify_state_change event dataset contains state",
          "[services][ups][watch]") {
    ups_watch_scp scp;

    dicom_dataset captured_info;

    scp.set_get_subscribers_handler([](
        [[maybe_unused]] const std::string& uid) {
        return Result<std::vector<std::string>>(
            std::vector<std::string>{"AE1"});
    });

    scp.set_event_callback([&captured_info](
        [[maybe_unused]] const std::string& ae,
        [[maybe_unused]] uint16_t event_type,
        [[maybe_unused]] const std::string& uid,
        const dicom_dataset& info) {
        captured_info = info;
    });

    SECTION("SCHEDULED state") {
        scp.notify_state_change("1.2.3", "SCHEDULED");
        CHECK(captured_info.get_string(ups_tags::procedure_step_state) ==
              "SCHEDULED");
    }

    SECTION("IN PROGRESS state") {
        scp.notify_state_change("1.2.3", "IN PROGRESS");
        CHECK(captured_info.get_string(ups_tags::procedure_step_state) ==
              "IN PROGRESS");
    }

    SECTION("COMPLETED state") {
        scp.notify_state_change("1.2.3", "COMPLETED");
        CHECK(captured_info.get_string(ups_tags::procedure_step_state) ==
              "COMPLETED");
    }

    SECTION("CANCELED state") {
        scp.notify_state_change("1.2.3", "CANCELED");
        CHECK(captured_info.get_string(ups_tags::procedure_step_state) ==
              "CANCELED");
    }
}

TEST_CASE("notify_cancel_requested with empty reason",
          "[services][ups][watch]") {
    ups_watch_scp scp;

    bool has_reason_tag = true;

    scp.set_get_subscribers_handler([](
        [[maybe_unused]] const std::string& uid) {
        return Result<std::vector<std::string>>(
            std::vector<std::string>{"AE1"});
    });

    scp.set_event_callback([&has_reason_tag](
        [[maybe_unused]] const std::string& ae,
        [[maybe_unused]] uint16_t event_type,
        [[maybe_unused]] const std::string& uid,
        const dicom_dataset& info) {
        has_reason_tag =
            info.contains(ups_tags::reason_for_cancellation);
    });

    scp.notify_cancel_requested("1.2.3", "");

    CHECK_FALSE(has_reason_tag);
}
