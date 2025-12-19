/**
 * @file events.hpp
 * @brief DICOM event definitions for event-based communication
 *
 * This file defines event types for inter-module communication in the
 * PACS system using the common_system Event Bus pattern.
 *
 * @see common_system/include/kcenon/common/patterns/event_bus.h
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace pacs::events {

// ============================================================================
// Association Events
// ============================================================================

/**
 * @brief Event published when a DICOM association is successfully established
 *
 * This event is triggered after successful A-ASSOCIATE-AC exchange.
 */
struct association_established_event {
    std::string calling_ae;
    std::string called_ae;
    std::string remote_host;
    uint16_t remote_port;
    uint32_t max_pdu_size;
    std::chrono::steady_clock::time_point timestamp;

    association_established_event(std::string calling,
                                  std::string called,
                                  std::string host,
                                  uint16_t port,
                                  uint32_t pdu_size)
        : calling_ae(std::move(calling)),
          called_ae(std::move(called)),
          remote_host(std::move(host)),
          remote_port(port),
          max_pdu_size(pdu_size),
          timestamp(std::chrono::steady_clock::now()) {}
};

/**
 * @brief Event published when a DICOM association is gracefully released
 */
struct association_released_event {
    std::string calling_ae;
    std::string called_ae;
    std::chrono::milliseconds duration;
    uint32_t operations_count;
    std::chrono::steady_clock::time_point timestamp;

    association_released_event(std::string calling,
                               std::string called,
                               std::chrono::milliseconds dur,
                               uint32_t ops)
        : calling_ae(std::move(calling)),
          called_ae(std::move(called)),
          duration(dur),
          operations_count(ops),
          timestamp(std::chrono::steady_clock::now()) {}
};

/**
 * @brief Event published when a DICOM association is aborted
 */
struct association_aborted_event {
    std::string calling_ae;
    std::string called_ae;
    std::string reason;
    uint8_t source;   // 0 = unknown, 1 = service-user, 2 = service-provider
    uint8_t reason_code;
    std::chrono::steady_clock::time_point timestamp;

    association_aborted_event(std::string calling,
                              std::string called,
                              std::string abort_reason,
                              uint8_t src = 0,
                              uint8_t code = 0)
        : calling_ae(std::move(calling)),
          called_ae(std::move(called)),
          reason(std::move(abort_reason)),
          source(src),
          reason_code(code),
          timestamp(std::chrono::steady_clock::now()) {}
};

// ============================================================================
// Storage Events (C-STORE)
// ============================================================================

/**
 * @brief Event published when an image is successfully received via C-STORE
 */
struct image_received_event {
    std::string patient_id;
    std::string study_instance_uid;
    std::string series_instance_uid;
    std::string sop_instance_uid;
    std::string sop_class_uid;
    std::string calling_ae;
    size_t bytes_received;
    std::chrono::steady_clock::time_point timestamp;

    image_received_event(std::string patient,
                         std::string study_uid,
                         std::string series_uid,
                         std::string sop_uid,
                         std::string sop_class,
                         std::string calling,
                         size_t bytes)
        : patient_id(std::move(patient)),
          study_instance_uid(std::move(study_uid)),
          series_instance_uid(std::move(series_uid)),
          sop_instance_uid(std::move(sop_uid)),
          sop_class_uid(std::move(sop_class)),
          calling_ae(std::move(calling)),
          bytes_received(bytes),
          timestamp(std::chrono::steady_clock::now()) {}
};

/**
 * @brief Event published when a C-STORE operation fails
 */
struct storage_failed_event {
    std::string patient_id;
    std::string sop_instance_uid;
    std::string calling_ae;
    int error_code;
    std::string error_message;
    std::chrono::steady_clock::time_point timestamp;

    storage_failed_event(std::string patient,
                         std::string sop_uid,
                         std::string calling,
                         int code,
                         std::string message)
        : patient_id(std::move(patient)),
          sop_instance_uid(std::move(sop_uid)),
          calling_ae(std::move(calling)),
          error_code(code),
          error_message(std::move(message)),
          timestamp(std::chrono::steady_clock::now()) {}
};

// ============================================================================
// Query Events (C-FIND)
// ============================================================================

/**
 * @brief Query level enumeration
 */
enum class query_level {
    patient,
    study,
    series,
    image
};

/**
 * @brief Convert query level to string
 */
[[nodiscard]] inline auto query_level_to_string(query_level level) -> std::string {
    switch (level) {
        case query_level::patient: return "PATIENT";
        case query_level::study: return "STUDY";
        case query_level::series: return "SERIES";
        case query_level::image: return "IMAGE";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Event published when a C-FIND query is executed
 */
struct query_executed_event {
    query_level level;
    std::string calling_ae;
    size_t result_count;
    uint64_t execution_time_ms;
    std::chrono::steady_clock::time_point timestamp;

    query_executed_event(query_level lvl,
                         std::string calling,
                         size_t results,
                         uint64_t exec_time)
        : level(lvl),
          calling_ae(std::move(calling)),
          result_count(results),
          execution_time_ms(exec_time),
          timestamp(std::chrono::steady_clock::now()) {}
};

/**
 * @brief Event published when a C-FIND query fails
 */
struct query_failed_event {
    std::string calling_ae;
    int error_code;
    std::string error_message;
    std::chrono::steady_clock::time_point timestamp;

    query_failed_event(std::string calling,
                       int code,
                       std::string message)
        : calling_ae(std::move(calling)),
          error_code(code),
          error_message(std::move(message)),
          timestamp(std::chrono::steady_clock::now()) {}
};

// ============================================================================
// Retrieve Events (C-MOVE / C-GET)
// ============================================================================

/**
 * @brief Retrieve operation type
 */
enum class retrieve_operation {
    c_move,
    c_get
};

/**
 * @brief Convert retrieve operation to string
 */
[[nodiscard]] inline auto retrieve_operation_to_string(retrieve_operation op) -> std::string {
    return op == retrieve_operation::c_move ? "C-MOVE" : "C-GET";
}

/**
 * @brief Event published when a retrieve operation (C-MOVE/C-GET) starts
 */
struct retrieve_started_event {
    retrieve_operation operation;
    std::string calling_ae;
    std::string destination_ae;  // Only for C-MOVE
    std::string study_instance_uid;
    uint16_t total_instances;
    std::chrono::steady_clock::time_point timestamp;

    retrieve_started_event(retrieve_operation op,
                           std::string calling,
                           std::string destination,
                           std::string study_uid,
                           uint16_t total)
        : operation(op),
          calling_ae(std::move(calling)),
          destination_ae(std::move(destination)),
          study_instance_uid(std::move(study_uid)),
          total_instances(total),
          timestamp(std::chrono::steady_clock::now()) {}
};

/**
 * @brief Event published when a retrieve operation completes
 */
struct retrieve_completed_event {
    retrieve_operation operation;
    std::string calling_ae;
    std::string destination_ae;
    uint16_t instances_sent;
    uint16_t instances_failed;
    uint16_t instances_warning;
    uint64_t duration_ms;
    std::chrono::steady_clock::time_point timestamp;

    retrieve_completed_event(retrieve_operation op,
                             std::string calling,
                             std::string destination,
                             uint16_t sent,
                             uint16_t failed,
                             uint16_t warning,
                             uint64_t duration)
        : operation(op),
          calling_ae(std::move(calling)),
          destination_ae(std::move(destination)),
          instances_sent(sent),
          instances_failed(failed),
          instances_warning(warning),
          duration_ms(duration),
          timestamp(std::chrono::steady_clock::now()) {}
};

}  // namespace pacs::events
