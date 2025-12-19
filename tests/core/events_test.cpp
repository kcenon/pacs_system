/**
 * @file events_test.cpp
 * @brief Unit tests for DICOM event types and event bus integration
 */

#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <chrono>
#include <thread>

#include "pacs/core/events.hpp"
#include <kcenon/common/patterns/event_bus.h>

using namespace pacs::events;

// ============================================================================
// Event Type Construction Tests
// ============================================================================

TEST_CASE("association_established_event construction", "[events][association]") {
    association_established_event event{
        "CALLING_AE",
        "CALLED_AE",
        "192.168.1.100",
        11112,
        16384
    };

    CHECK(event.calling_ae == "CALLING_AE");
    CHECK(event.called_ae == "CALLED_AE");
    CHECK(event.remote_host == "192.168.1.100");
    CHECK(event.remote_port == 11112);
    CHECK(event.max_pdu_size == 16384);
    CHECK(event.timestamp.time_since_epoch().count() > 0);
}

TEST_CASE("association_released_event construction", "[events][association]") {
    association_released_event event{
        "CALLING_AE",
        "CALLED_AE",
        std::chrono::milliseconds{5000},
        10
    };

    CHECK(event.calling_ae == "CALLING_AE");
    CHECK(event.called_ae == "CALLED_AE");
    CHECK(event.duration.count() == 5000);
    CHECK(event.operations_count == 10);
}

TEST_CASE("association_aborted_event construction", "[events][association]") {
    association_aborted_event event{
        "CALLING_AE",
        "CALLED_AE",
        "Connection timeout",
        2,  // service-provider
        1   // not-specified
    };

    CHECK(event.calling_ae == "CALLING_AE");
    CHECK(event.reason == "Connection timeout");
    CHECK(event.source == 2);
    CHECK(event.reason_code == 1);
}

TEST_CASE("image_received_event construction", "[events][storage]") {
    image_received_event event{
        "PATIENT123",
        "1.2.3.4.5.6.7",
        "1.2.3.4.5.6.8",
        "1.2.3.4.5.6.9",
        "1.2.840.10008.5.1.4.1.1.2",
        "MODALITY_AE",
        1048576
    };

    CHECK(event.patient_id == "PATIENT123");
    CHECK(event.study_instance_uid == "1.2.3.4.5.6.7");
    CHECK(event.series_instance_uid == "1.2.3.4.5.6.8");
    CHECK(event.sop_instance_uid == "1.2.3.4.5.6.9");
    CHECK(event.sop_class_uid == "1.2.840.10008.5.1.4.1.1.2");
    CHECK(event.calling_ae == "MODALITY_AE");
    CHECK(event.bytes_received == 1048576);
}

TEST_CASE("query_executed_event construction", "[events][query]") {
    query_executed_event event{
        query_level::study,
        "WORKSTATION_AE",
        42,
        150
    };

    CHECK(event.level == query_level::study);
    CHECK(event.calling_ae == "WORKSTATION_AE");
    CHECK(event.result_count == 42);
    CHECK(event.execution_time_ms == 150);
}

TEST_CASE("retrieve_events construction", "[events][retrieve]") {
    SECTION("retrieve_started_event") {
        retrieve_started_event event{
            retrieve_operation::c_move,
            "WORKSTATION_AE",
            "ARCHIVE_AE",
            "1.2.3.4.5.6.7",
            100
        };

        CHECK(event.operation == retrieve_operation::c_move);
        CHECK(event.calling_ae == "WORKSTATION_AE");
        CHECK(event.destination_ae == "ARCHIVE_AE");
        CHECK(event.study_instance_uid == "1.2.3.4.5.6.7");
        CHECK(event.total_instances == 100);
    }

    SECTION("retrieve_completed_event") {
        retrieve_completed_event event{
            retrieve_operation::c_get,
            "WORKSTATION_AE",
            "",
            95,
            3,
            2,
            30000
        };

        CHECK(event.operation == retrieve_operation::c_get);
        CHECK(event.instances_sent == 95);
        CHECK(event.instances_failed == 3);
        CHECK(event.instances_warning == 2);
        CHECK(event.duration_ms == 30000);
    }
}

// ============================================================================
// Event Bus Integration Tests
// ============================================================================

TEST_CASE("Event bus publish and subscribe", "[events][integration]") {
    auto& bus = kcenon::common::get_event_bus();

    std::atomic<int> event_count{0};
    std::string received_patient_id;

    auto sub_id = bus.subscribe<image_received_event>(
        [&](const image_received_event& evt) {
            event_count++;
            received_patient_id = evt.patient_id;
        }
    );

    // Publish event
    bus.publish(image_received_event{
        "TEST_PATIENT",
        "1.2.3.4",
        "1.2.3.5",
        "1.2.3.6",
        "1.2.840.10008.5.1.4.1.1.2",
        "TEST_AE",
        1024
    });

    // Give time for async processing if any
    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    CHECK(event_count == 1);
    CHECK(received_patient_id == "TEST_PATIENT");

    // Cleanup
    bus.unsubscribe(sub_id);
}

TEST_CASE("Multiple event types subscription", "[events][integration]") {
    auto& bus = kcenon::common::get_event_bus();

    std::atomic<int> association_events{0};
    std::atomic<int> storage_events{0};

    auto assoc_sub = bus.subscribe<association_established_event>(
        [&](const association_established_event&) {
            association_events++;
        }
    );

    auto storage_sub = bus.subscribe<image_received_event>(
        [&](const image_received_event&) {
            storage_events++;
        }
    );

    // Publish different events
    bus.publish(association_established_event{
        "AE1", "AE2", "host", 11112, 16384
    });
    bus.publish(image_received_event{
        "P1", "S1", "SE1", "I1", "C1", "AE", 100
    });
    bus.publish(association_established_event{
        "AE3", "AE4", "host", 11112, 16384
    });

    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    CHECK(association_events == 2);
    CHECK(storage_events == 1);

    // Cleanup
    bus.unsubscribe(assoc_sub);
    bus.unsubscribe(storage_sub);
}

// ============================================================================
// Helper Function Tests
// ============================================================================

TEST_CASE("query_level_to_string conversion", "[events][helpers]") {
    CHECK(query_level_to_string(query_level::patient) == "PATIENT");
    CHECK(query_level_to_string(query_level::study) == "STUDY");
    CHECK(query_level_to_string(query_level::series) == "SERIES");
    CHECK(query_level_to_string(query_level::image) == "IMAGE");
}

TEST_CASE("retrieve_operation_to_string conversion", "[events][helpers]") {
    CHECK(retrieve_operation_to_string(retrieve_operation::c_move) == "C-MOVE");
    CHECK(retrieve_operation_to_string(retrieve_operation::c_get) == "C-GET");
}
