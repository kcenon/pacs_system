/**
 * @file ups_watch_scu_test.cpp
 * @brief Unit tests for UPS Watch SCU service
 */

#include <kcenon/pacs/services/ups/ups_watch_scu.h>
#include <kcenon/pacs/network/dimse/command_field.h>
#include <kcenon/pacs/network/dimse/dimse_message.h>
#include <kcenon/pacs/network/dimse/status_codes.h>

#include <catch2/catch_test_macros.hpp>

using namespace kcenon::pacs::services;
using namespace kcenon::pacs::network;
using namespace kcenon::pacs::network::dimse;
using namespace kcenon::pacs::core;

// ============================================================================
// ups_watch_scu Construction Tests
// ============================================================================

TEST_CASE("ups_watch_scu construction", "[services][ups][watch-scu]") {
    SECTION("default construction succeeds") {
        ups_watch_scu scu;
        CHECK(scu.subscribes_performed() == 0);
        CHECK(scu.unsubscribes_performed() == 0);
        CHECK(scu.events_received() == 0);
    }

    SECTION("construction with config succeeds") {
        ups_watch_scu_config config;
        config.timeout = std::chrono::milliseconds{60000};

        ups_watch_scu scu(config);
        CHECK(scu.subscribes_performed() == 0);
        CHECK(scu.unsubscribes_performed() == 0);
        CHECK(scu.events_received() == 0);
    }

    SECTION("construction with nullptr logger succeeds") {
        ups_watch_scu scu(nullptr);
        CHECK(scu.subscribes_performed() == 0);
    }

    SECTION("construction with config and nullptr logger succeeds") {
        ups_watch_scu_config config;
        ups_watch_scu scu(config, nullptr);
        CHECK(scu.subscribes_performed() == 0);
    }
}

// ============================================================================
// ups_watch_scu_config Tests
// ============================================================================

TEST_CASE("ups_watch_scu_config defaults", "[services][ups][watch-scu]") {
    ups_watch_scu_config config;
    CHECK(config.timeout == std::chrono::milliseconds{30000});
}

TEST_CASE("ups_watch_scu_config customization", "[services][ups][watch-scu]") {
    ups_watch_scu_config config;
    config.timeout = std::chrono::milliseconds{60000};
    CHECK(config.timeout == std::chrono::milliseconds{60000});
}

// ============================================================================
// ups_subscribe_data Structure Tests
// ============================================================================

TEST_CASE("ups_subscribe_data structure", "[services][ups][watch-scu]") {
    ups_subscribe_data data;

    SECTION("default construction has correct defaults") {
        CHECK(data.workitem_uid.empty());
        CHECK(data.deletion_lock == false);
    }

    SECTION("can be initialized for workitem subscription") {
        data.workitem_uid = "1.2.3.4.5.6.7.8";
        data.deletion_lock = true;

        CHECK(data.workitem_uid == "1.2.3.4.5.6.7.8");
        CHECK(data.deletion_lock == true);
    }

    SECTION("empty workitem_uid means global subscription") {
        CHECK(data.workitem_uid.empty());
    }
}

// ============================================================================
// ups_unsubscribe_data Structure Tests
// ============================================================================

TEST_CASE("ups_unsubscribe_data structure", "[services][ups][watch-scu]") {
    ups_unsubscribe_data data;

    SECTION("default construction") {
        CHECK(data.workitem_uid.empty());
    }

    SECTION("can be initialized for workitem unsubscription") {
        data.workitem_uid = "1.2.3.4.5.6.7.8";
        CHECK(data.workitem_uid == "1.2.3.4.5.6.7.8");
    }
}

// ============================================================================
// ups_watch_event Structure Tests
// ============================================================================

TEST_CASE("ups_watch_event structure", "[services][ups][watch-scu]") {
    ups_watch_event event;

    SECTION("default construction") {
        CHECK(event.event_type_id == 0);
        CHECK(event.workitem_uid.empty());
        CHECK(event.procedure_step_state.empty());
        CHECK(event.progress.empty());
        CHECK(event.cancellation_reason.empty());
    }

    SECTION("can be initialized for state report") {
        event.event_type_id = ups_event_state_report;
        event.workitem_uid = "1.2.3.4.5.6.7.8";
        event.procedure_step_state = "COMPLETED";
        event.timestamp = std::chrono::system_clock::now();

        CHECK(event.event_type_id == 1);
        CHECK(event.workitem_uid == "1.2.3.4.5.6.7.8");
        CHECK(event.procedure_step_state == "COMPLETED");
    }

    SECTION("can be initialized for cancel requested") {
        event.event_type_id = ups_event_cancel_requested;
        event.workitem_uid = "1.2.3.4.5.6.7.8";
        event.cancellation_reason = "Patient refused";

        CHECK(event.event_type_id == 2);
        CHECK(event.cancellation_reason == "Patient refused");
    }

    SECTION("can be initialized for progress report") {
        event.event_type_id = ups_event_progress_report;
        event.workitem_uid = "1.2.3.4.5.6.7.8";
        event.progress = "75";

        CHECK(event.event_type_id == 3);
        CHECK(event.progress == "75");
    }
}

// ============================================================================
// SOP Class UID and Constant Tests
// ============================================================================

TEST_CASE("UPS Watch SOP Class UID constant", "[services][ups][watch-scu]") {
    CHECK(ups_watch_sop_class_uid == "1.2.840.10008.5.1.4.34.6.2");
}

TEST_CASE("UPS Global Subscription Instance UID constant",
          "[services][ups][watch-scu]") {
    CHECK(ups_global_subscription_instance_uid == "1.2.840.10008.5.1.4.34.5");
}

TEST_CASE("UPS Watch N-ACTION type constants", "[services][ups][watch-scu]") {
    CHECK(ups_watch_action_subscribe == 3);
    CHECK(ups_watch_action_unsubscribe == 4);
    CHECK(ups_watch_action_suspend_global == 5);
}

TEST_CASE("UPS N-EVENT-REPORT type constants", "[services][ups][watch-scu]") {
    CHECK(ups_event_state_report == 1);
    CHECK(ups_event_cancel_requested == 2);
    CHECK(ups_event_progress_report == 3);
    CHECK(ups_event_scp_status_change == 4);
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_CASE("ups_watch_scu statistics", "[services][ups][watch-scu]") {
    ups_watch_scu scu;

    SECTION("initial statistics are zero") {
        CHECK(scu.subscribes_performed() == 0);
        CHECK(scu.unsubscribes_performed() == 0);
        CHECK(scu.events_received() == 0);
    }

    SECTION("reset_statistics clears all counters") {
        scu.reset_statistics();
        CHECK(scu.subscribes_performed() == 0);
        CHECK(scu.unsubscribes_performed() == 0);
        CHECK(scu.events_received() == 0);
    }
}

// ============================================================================
// Multiple Instance Tests
// ============================================================================

TEST_CASE("multiple ups_watch_scu instances are independent",
          "[services][ups][watch-scu]") {
    ups_watch_scu scu1;
    ups_watch_scu scu2;

    CHECK(scu1.subscribes_performed() == scu2.subscribes_performed());
    CHECK(scu1.unsubscribes_performed() == scu2.unsubscribes_performed());
    CHECK(scu1.events_received() == scu2.events_received());

    scu1.reset_statistics();
    CHECK(scu1.subscribes_performed() == 0);
    CHECK(scu2.subscribes_performed() == 0);
}

// ============================================================================
// Callback Configuration Tests
// ============================================================================

TEST_CASE("ups_watch_scu callback configuration", "[services][ups][watch-scu]") {
    ups_watch_scu scu;

    SECTION("can set event received callback") {
        bool called = false;
        scu.set_event_received_callback([&](const ups_watch_event&) {
            called = true;
        });
        // Callback is stored but not yet invoked
        CHECK_FALSE(called);
    }

    SECTION("can replace callback") {
        int count = 0;
        scu.set_event_received_callback([&](const ups_watch_event&) {
            count = 1;
        });
        scu.set_event_received_callback([&](const ups_watch_event&) {
            count = 2;
        });
        // Replacement is valid
        CHECK(count == 0);
    }
}

// ============================================================================
// N-EVENT-REPORT Handler Tests
// ============================================================================

TEST_CASE("ups_watch_scu handle_event_report", "[services][ups][watch-scu]") {
    ups_watch_scu scu;

    SECTION("rejects non-N-EVENT-REPORT commands") {
        dimse_message msg{command_field::n_action_rq, 1};
        auto result = scu.handle_event_report(msg);
        CHECK(result.is_err());
    }

    SECTION("processes state report event") {
        dimse_message msg{command_field::n_event_report_rq, 1};
        msg.set_affected_sop_class_uid(ups_watch_sop_class_uid);
        msg.set_affected_sop_instance_uid("1.2.3.4.5");
        msg.set_event_type_id(ups_event_state_report);

        dicom_dataset ds;
        ds.set_string(ups_tags::procedure_step_state,
                      kcenon::pacs::encoding::vr_type::CS, "COMPLETED");
        msg.set_dataset(std::move(ds));

        auto result = scu.handle_event_report(msg);
        REQUIRE(result.is_ok());
        CHECK(result.value().event_type_id == ups_event_state_report);
        CHECK(result.value().workitem_uid == "1.2.3.4.5");
        CHECK(result.value().procedure_step_state == "COMPLETED");
        CHECK(scu.events_received() == 1);
    }

    SECTION("processes cancel requested event") {
        dimse_message msg{command_field::n_event_report_rq, 1};
        msg.set_affected_sop_class_uid(ups_watch_sop_class_uid);
        msg.set_affected_sop_instance_uid("1.2.3.4.5");
        msg.set_event_type_id(ups_event_cancel_requested);

        dicom_dataset ds;
        ds.set_string(ups_tags::reason_for_cancellation,
                      kcenon::pacs::encoding::vr_type::LT, "Patient refused");
        msg.set_dataset(std::move(ds));

        auto result = scu.handle_event_report(msg);
        REQUIRE(result.is_ok());
        CHECK(result.value().event_type_id == ups_event_cancel_requested);
        CHECK(result.value().cancellation_reason == "Patient refused");
        CHECK(scu.events_received() == 1);
    }

    SECTION("processes progress report event") {
        dimse_message msg{command_field::n_event_report_rq, 1};
        msg.set_affected_sop_class_uid(ups_watch_sop_class_uid);
        msg.set_affected_sop_instance_uid("1.2.3.4.5");
        msg.set_event_type_id(ups_event_progress_report);

        dicom_dataset ds;
        ds.set_string(ups_tags::procedure_step_progress,
                      kcenon::pacs::encoding::vr_type::DS, "75");
        msg.set_dataset(std::move(ds));

        auto result = scu.handle_event_report(msg);
        REQUIRE(result.is_ok());
        CHECK(result.value().event_type_id == ups_event_progress_report);
        CHECK(result.value().progress == "75");
        CHECK(scu.events_received() == 1);
    }

    SECTION("handles event without dataset") {
        dimse_message msg{command_field::n_event_report_rq, 1};
        msg.set_affected_sop_class_uid(ups_watch_sop_class_uid);
        msg.set_affected_sop_instance_uid("1.2.3.4.5");
        msg.set_event_type_id(ups_event_scp_status_change);

        auto result = scu.handle_event_report(msg);
        REQUIRE(result.is_ok());
        CHECK(result.value().event_type_id == ups_event_scp_status_change);
        CHECK(result.value().workitem_uid == "1.2.3.4.5");
        CHECK(scu.events_received() == 1);
    }

    SECTION("invokes callback on event") {
        ups_watch_event received_event;
        scu.set_event_received_callback([&](const ups_watch_event& e) {
            received_event = e;
        });

        dimse_message msg{command_field::n_event_report_rq, 1};
        msg.set_affected_sop_class_uid(ups_watch_sop_class_uid);
        msg.set_affected_sop_instance_uid("1.2.3.4.5");
        msg.set_event_type_id(ups_event_state_report);

        dicom_dataset ds;
        ds.set_string(ups_tags::procedure_step_state,
                      kcenon::pacs::encoding::vr_type::CS, "IN PROGRESS");
        msg.set_dataset(std::move(ds));

        auto result = scu.handle_event_report(msg);
        REQUIRE(result.is_ok());
        CHECK(received_event.event_type_id == ups_event_state_report);
        CHECK(received_event.procedure_step_state == "IN PROGRESS");
        CHECK(received_event.workitem_uid == "1.2.3.4.5");
    }

    SECTION("increments events_received for multiple events") {
        for (int i = 0; i < 3; ++i) {
            dimse_message msg{command_field::n_event_report_rq, 1};
            msg.set_affected_sop_class_uid(ups_watch_sop_class_uid);
            msg.set_affected_sop_instance_uid("1.2.3.4." + std::to_string(i));
            msg.set_event_type_id(ups_event_state_report);

            dicom_dataset ds;
            ds.set_string(ups_tags::procedure_step_state,
                          kcenon::pacs::encoding::vr_type::CS, "COMPLETED");
            msg.set_dataset(std::move(ds));

            auto result = scu.handle_event_report(msg);
            REQUIRE(result.is_ok());
        }
        CHECK(scu.events_received() == 3);
    }
}

// ============================================================================
// Data Structure Copy and Move Tests
// ============================================================================

TEST_CASE("ups_subscribe_data is copyable and movable",
          "[services][ups][watch-scu]") {
    ups_subscribe_data data;
    data.workitem_uid = "1.2.3.4.5.6.7.8";
    data.deletion_lock = true;

    SECTION("copy construction") {
        ups_subscribe_data copy = data;
        CHECK(copy.workitem_uid == "1.2.3.4.5.6.7.8");
        CHECK(copy.deletion_lock == true);
    }

    SECTION("move construction") {
        ups_subscribe_data moved = std::move(data);
        CHECK(moved.workitem_uid == "1.2.3.4.5.6.7.8");
        CHECK(moved.deletion_lock == true);
    }
}

TEST_CASE("ups_unsubscribe_data is copyable and movable",
          "[services][ups][watch-scu]") {
    ups_unsubscribe_data data;
    data.workitem_uid = "1.2.3.4.5.6.7.8";

    SECTION("copy construction") {
        ups_unsubscribe_data copy = data;
        CHECK(copy.workitem_uid == "1.2.3.4.5.6.7.8");
    }

    SECTION("move construction") {
        ups_unsubscribe_data moved = std::move(data);
        CHECK(moved.workitem_uid == "1.2.3.4.5.6.7.8");
    }
}

TEST_CASE("ups_watch_event is copyable and movable",
          "[services][ups][watch-scu]") {
    ups_watch_event event;
    event.event_type_id = ups_event_state_report;
    event.workitem_uid = "1.2.3.4.5.6.7.8";
    event.procedure_step_state = "COMPLETED";
    event.timestamp = std::chrono::system_clock::now();

    SECTION("copy construction") {
        ups_watch_event copy = event;
        CHECK(copy.event_type_id == ups_event_state_report);
        CHECK(copy.workitem_uid == "1.2.3.4.5.6.7.8");
        CHECK(copy.procedure_step_state == "COMPLETED");
    }

    SECTION("move construction") {
        ups_watch_event moved = std::move(event);
        CHECK(moved.event_type_id == ups_event_state_report);
        CHECK(moved.workitem_uid == "1.2.3.4.5.6.7.8");
        CHECK(moved.procedure_step_state == "COMPLETED");
    }
}

// ============================================================================
// Event Dataset Content Tests
// ============================================================================

TEST_CASE("ups_watch_event preserves full event dataset",
          "[services][ups][watch-scu]") {
    ups_watch_scu scu;

    dimse_message msg{command_field::n_event_report_rq, 1};
    msg.set_affected_sop_class_uid(ups_watch_sop_class_uid);
    msg.set_affected_sop_instance_uid("1.2.3.4.5");
    msg.set_event_type_id(ups_event_state_report);

    dicom_dataset ds;
    ds.set_string(ups_tags::procedure_step_state,
                  kcenon::pacs::encoding::vr_type::CS, "COMPLETED");
    msg.set_dataset(std::move(ds));

    auto result = scu.handle_event_report(msg);
    REQUIRE(result.is_ok());

    // The event_info dataset should contain the original data
    const auto& event = result.value();
    auto state = event.event_info.get_string(ups_tags::procedure_step_state);
    CHECK(state == "COMPLETED");
}
