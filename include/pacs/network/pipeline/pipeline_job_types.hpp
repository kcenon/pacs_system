/**
 * @file pipeline_job_types.hpp
 * @brief Job type definitions for the 6-stage DICOM I/O pipeline
 *
 * This file defines the job types and priorities for the pipeline stages
 * that process DICOM operations with improved throughput.
 *
 * Pipeline Architecture:
 * @code
 * Stage 1: Network I/O (Receive)     <- net_io_workers
 *          | [Queue 1: PDU Bytes]
 * Stage 2: PDU Decode                <- protocol_workers
 *          | [Queue 2: Decoded PDU]
 * Stage 3: DIMSE Processing          <- protocol_workers
 *          | [Queue 3: Service Request]
 * Stage 4: Storage/Query Execution   <- execution_workers (blocking allowed)
 *          | [Queue 4: Service Result]
 * Stage 5: Response Encoding         <- encode_workers
 *          | [Queue 5: Encoded PDU]
 * Stage 6: Network I/O (Send)        <- net_io_workers
 * @endcode
 *
 * @see Issue #517 - Implement typed_thread_pool-based I/O Pipeline
 * @see DICOM PS3.8 Network Communication Support
 */

#pragma once

#include <cstdint>
#include <string_view>

namespace pacs::network::pipeline {

/**
 * @enum pipeline_stage
 * @brief Identifies the 6 stages of the DICOM I/O pipeline
 *
 * Each stage has dedicated worker threads optimized for its workload:
 * - Network I/O stages: Low latency, non-blocking
 * - Protocol stages: CPU-bound parsing/encoding
 * - Execution stage: Blocking I/O allowed (database, file system)
 */
enum class pipeline_stage : uint8_t {
    /// Stage 1: Receive raw PDU bytes from network
    network_receive = 0,

    /// Stage 2: Decode PDU bytes into structured data
    pdu_decode = 1,

    /// Stage 3: Process DIMSE messages and route requests
    dimse_process = 2,

    /// Stage 4: Execute storage/query operations (blocking allowed)
    storage_query_exec = 3,

    /// Stage 5: Encode response into PDU bytes
    response_encode = 4,

    /// Stage 6: Send PDU bytes to network
    network_send = 5,

    /// Total number of stages
    stage_count = 6
};

/**
 * @brief Get the human-readable name of a pipeline stage
 * @param stage The pipeline stage
 * @return String view of the stage name
 */
[[nodiscard]] constexpr auto get_stage_name(pipeline_stage stage) noexcept
    -> std::string_view {
    switch (stage) {
        case pipeline_stage::network_receive:
            return "network_receive";
        case pipeline_stage::pdu_decode:
            return "pdu_decode";
        case pipeline_stage::dimse_process:
            return "dimse_process";
        case pipeline_stage::storage_query_exec:
            return "storage_query_exec";
        case pipeline_stage::response_encode:
            return "response_encode";
        case pipeline_stage::network_send:
            return "network_send";
        default:
            return "unknown";
    }
}

/**
 * @brief Check if a stage allows blocking operations
 * @param stage The pipeline stage
 * @return true if blocking I/O is allowed in this stage
 */
[[nodiscard]] constexpr auto is_blocking_stage(pipeline_stage stage) noexcept
    -> bool {
    return stage == pipeline_stage::storage_query_exec;
}

/**
 * @brief Check if a stage handles network I/O
 * @param stage The pipeline stage
 * @return true if this is a network I/O stage
 */
[[nodiscard]] constexpr auto is_network_io_stage(pipeline_stage stage) noexcept
    -> bool {
    return stage == pipeline_stage::network_receive ||
           stage == pipeline_stage::network_send;
}

/**
 * @enum job_category
 * @brief Categories for pipeline jobs used in metrics and monitoring
 */
enum class job_category : uint8_t {
    /// C-ECHO verification request/response
    echo = 0,

    /// C-STORE storage request/response
    store = 1,

    /// C-FIND query request/response
    find = 2,

    /// C-GET retrieve request/response
    get = 3,

    /// C-MOVE move request/response
    move = 4,

    /// Association management (A-ASSOCIATE, A-RELEASE, A-ABORT)
    association = 5,

    /// Internal pipeline control messages
    control = 6,

    /// Unknown or other category
    other = 7
};

/**
 * @brief Get the human-readable name of a job category
 * @param category The job category
 * @return String view of the category name
 */
[[nodiscard]] constexpr auto get_category_name(job_category category) noexcept
    -> std::string_view {
    switch (category) {
        case job_category::echo:
            return "echo";
        case job_category::store:
            return "store";
        case job_category::find:
            return "find";
        case job_category::get:
            return "get";
        case job_category::move:
            return "move";
        case job_category::association:
            return "association";
        case job_category::control:
            return "control";
        default:
            return "other";
    }
}

/**
 * @struct job_context
 * @brief Context information attached to pipeline jobs for tracking
 *
 * This context is passed through all pipeline stages for a single
 * DICOM operation, enabling end-to-end tracing and metrics.
 */
struct job_context {
    /// Unique identifier for this job (monotonically increasing)
    uint64_t job_id{0};

    /// Session/association identifier
    uint64_t session_id{0};

    /// Message ID from DIMSE command (if applicable)
    uint16_t message_id{0};

    /// Current pipeline stage
    pipeline_stage stage{pipeline_stage::network_receive};

    /// Job category for metrics
    job_category category{job_category::other};

    /// Timestamp when job entered the pipeline (nanoseconds since epoch)
    uint64_t enqueue_time_ns{0};

    /// Sequence number for ordering within a session
    uint32_t sequence_number{0};

    /// Priority (lower = higher priority, 0 = highest)
    uint8_t priority{128};
};

}  // namespace pacs::network::pipeline
