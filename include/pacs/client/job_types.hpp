/**
 * @file job_types.hpp
 * @brief Job types and structures for asynchronous DICOM operations
 *
 * This file provides data structures for representing background jobs,
 * including job types, status, priority, progress tracking, and job records.
 *
 * @see Issue #537 - Implement Job Manager for Async DICOM Operations
 * @see Issue #552 - Part 1: Job Types and Repository Implementation
 */

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace pacs::client {

// =============================================================================
// Job Type
// =============================================================================

/**
 * @brief Type of job operation
 */
enum class job_type {
    query,      ///< C-FIND operation
    retrieve,   ///< C-MOVE/C-GET operation
    store,      ///< C-STORE operation
    export_,    ///< Export to external system
    import_,    ///< Import from external source
    prefetch,   ///< Prefetch prior studies
    sync        ///< Synchronization
};

/**
 * @brief Convert job_type to string representation
 * @param type The type to convert
 * @return String representation of the type
 */
[[nodiscard]] constexpr const char* to_string(job_type type) noexcept {
    switch (type) {
        case job_type::query: return "query";
        case job_type::retrieve: return "retrieve";
        case job_type::store: return "store";
        case job_type::export_: return "export";
        case job_type::import_: return "import";
        case job_type::prefetch: return "prefetch";
        case job_type::sync: return "sync";
        default: return "unknown";
    }
}

/**
 * @brief Parse job_type from string
 * @param str The string to parse
 * @return Parsed type, or query if invalid
 */
[[nodiscard]] inline job_type job_type_from_string(std::string_view str) noexcept {
    if (str == "query") return job_type::query;
    if (str == "retrieve") return job_type::retrieve;
    if (str == "store") return job_type::store;
    if (str == "export") return job_type::export_;
    if (str == "import") return job_type::import_;
    if (str == "prefetch") return job_type::prefetch;
    if (str == "sync") return job_type::sync;
    return job_type::query;
}

// =============================================================================
// Job Status
// =============================================================================

/**
 * @brief Current status of a job
 */
enum class job_status {
    pending,    ///< Job created but not yet queued
    queued,     ///< Job is in the execution queue
    running,    ///< Job is currently executing
    completed,  ///< Job completed successfully
    failed,     ///< Job failed with error
    cancelled,  ///< Job was cancelled by user
    paused      ///< Job is paused
};

/**
 * @brief Convert job_status to string representation
 * @param status The status to convert
 * @return String representation of the status
 */
[[nodiscard]] constexpr const char* to_string(job_status status) noexcept {
    switch (status) {
        case job_status::pending: return "pending";
        case job_status::queued: return "queued";
        case job_status::running: return "running";
        case job_status::completed: return "completed";
        case job_status::failed: return "failed";
        case job_status::cancelled: return "cancelled";
        case job_status::paused: return "paused";
        default: return "unknown";
    }
}

/**
 * @brief Parse job_status from string
 * @param str The string to parse
 * @return Parsed status, or pending if invalid
 */
[[nodiscard]] inline job_status job_status_from_string(std::string_view str) noexcept {
    if (str == "pending") return job_status::pending;
    if (str == "queued") return job_status::queued;
    if (str == "running") return job_status::running;
    if (str == "completed") return job_status::completed;
    if (str == "failed") return job_status::failed;
    if (str == "cancelled") return job_status::cancelled;
    if (str == "paused") return job_status::paused;
    return job_status::pending;
}

/**
 * @brief Check if job status is a terminal state
 * @param status The status to check
 * @return true if the job has finished (completed, failed, or cancelled)
 */
[[nodiscard]] constexpr bool is_terminal_status(job_status status) noexcept {
    return status == job_status::completed ||
           status == job_status::failed ||
           status == job_status::cancelled;
}

// =============================================================================
// Job Priority
// =============================================================================

/**
 * @brief Priority level for job execution
 *
 * Higher priority jobs are executed before lower priority ones.
 */
enum class job_priority {
    low = 0,     ///< Background operations
    normal = 1,  ///< Standard priority
    high = 2,    ///< User-requested operations
    urgent = 3   ///< Critical operations
};

/**
 * @brief Convert job_priority to string representation
 * @param priority The priority to convert
 * @return String representation of the priority
 */
[[nodiscard]] constexpr const char* to_string(job_priority priority) noexcept {
    switch (priority) {
        case job_priority::low: return "low";
        case job_priority::normal: return "normal";
        case job_priority::high: return "high";
        case job_priority::urgent: return "urgent";
        default: return "unknown";
    }
}

/**
 * @brief Parse job_priority from string
 * @param str The string to parse
 * @return Parsed priority, or normal if invalid
 */
[[nodiscard]] inline job_priority job_priority_from_string(std::string_view str) noexcept {
    if (str == "low") return job_priority::low;
    if (str == "normal") return job_priority::normal;
    if (str == "high") return job_priority::high;
    if (str == "urgent") return job_priority::urgent;
    return job_priority::normal;
}

/**
 * @brief Parse job_priority from integer
 * @param value The integer value (0-3)
 * @return Parsed priority, or normal if out of range
 */
[[nodiscard]] inline job_priority job_priority_from_int(int value) noexcept {
    if (value <= 0) return job_priority::low;
    if (value == 1) return job_priority::normal;
    if (value == 2) return job_priority::high;
    if (value >= 3) return job_priority::urgent;
    return job_priority::normal;
}

// =============================================================================
// Job Progress
// =============================================================================

/**
 * @brief Progress tracking for a job
 *
 * Contains detailed progress information for monitoring job execution.
 */
struct job_progress {
    size_t total_items{0};                ///< Total number of items to process
    size_t completed_items{0};            ///< Successfully completed items
    size_t failed_items{0};               ///< Failed items
    size_t skipped_items{0};              ///< Skipped items
    size_t bytes_transferred{0};          ///< Total bytes transferred

    float percent_complete{0.0f};         ///< Completion percentage (0-100)

    std::string current_item;             ///< Current SOP Instance UID being processed
    std::string current_item_description; ///< Human-readable description

    std::chrono::milliseconds elapsed{0};             ///< Time elapsed since start
    std::chrono::milliseconds estimated_remaining{0}; ///< Estimated time remaining

    /**
     * @brief Calculate completion percentage from item counts
     */
    void calculate_percent() noexcept {
        if (total_items > 0) {
            percent_complete = static_cast<float>(completed_items + failed_items + skipped_items)
                             / static_cast<float>(total_items) * 100.0f;
        }
    }

    /**
     * @brief Check if all items have been processed
     */
    [[nodiscard]] bool is_complete() const noexcept {
        return total_items > 0 &&
               (completed_items + failed_items + skipped_items) >= total_items;
    }
};

// =============================================================================
// Job Record
// =============================================================================

/**
 * @brief Complete job record with all metadata
 *
 * Contains all information about a job including its configuration,
 * progress, timing, and error information.
 */
struct job_record {
    // =========================================================================
    // Identification
    // =========================================================================

    std::string job_id;         ///< Unique job identifier (UUID)
    job_type type;              ///< Type of operation
    job_status status{job_status::pending};  ///< Current status
    job_priority priority{job_priority::normal};  ///< Execution priority

    // =========================================================================
    // Source/Destination
    // =========================================================================

    std::string source_node_id;       ///< Source remote node ID
    std::string destination_node_id;  ///< Destination remote node ID

    // =========================================================================
    // Scope (what to process)
    // =========================================================================

    std::optional<std::string> patient_id;        ///< Patient ID filter
    std::optional<std::string> study_uid;         ///< Study Instance UID
    std::optional<std::string> series_uid;        ///< Series Instance UID
    std::optional<std::string> sop_instance_uid;  ///< Single SOP Instance UID
    std::vector<std::string> instance_uids;       ///< Batch operation UIDs

    // =========================================================================
    // Progress
    // =========================================================================

    job_progress progress;  ///< Current progress

    // =========================================================================
    // Timing
    // =========================================================================

    std::chrono::system_clock::time_point created_at;     ///< Job creation time
    std::optional<std::chrono::system_clock::time_point> queued_at;    ///< Time added to queue
    std::optional<std::chrono::system_clock::time_point> started_at;   ///< Execution start time
    std::optional<std::chrono::system_clock::time_point> completed_at; ///< Completion time

    // =========================================================================
    // Error Handling
    // =========================================================================

    std::string error_message;   ///< Error message if failed
    std::string error_details;   ///< Detailed error information
    int retry_count{0};          ///< Number of retry attempts
    int max_retries{3};          ///< Maximum retry attempts

    // =========================================================================
    // Metadata
    // =========================================================================

    std::string created_by;      ///< User ID who created the job
    std::unordered_map<std::string, std::string> metadata;  ///< Custom key-value pairs

    // =========================================================================
    // Database Fields
    // =========================================================================

    int64_t pk{0};  ///< Primary key (0 if not persisted)

    // =========================================================================
    // Convenience Methods
    // =========================================================================

    /**
     * @brief Check if job is in a terminal state
     */
    [[nodiscard]] bool is_finished() const noexcept {
        return is_terminal_status(status);
    }

    /**
     * @brief Check if job can be started
     */
    [[nodiscard]] bool can_start() const noexcept {
        return status == job_status::pending ||
               status == job_status::queued ||
               status == job_status::paused;
    }

    /**
     * @brief Check if job can be cancelled
     */
    [[nodiscard]] bool can_cancel() const noexcept {
        return !is_terminal_status(status);
    }

    /**
     * @brief Check if job can be paused
     */
    [[nodiscard]] bool can_pause() const noexcept {
        return status == job_status::running ||
               status == job_status::queued;
    }

    /**
     * @brief Check if job can be retried
     */
    [[nodiscard]] bool can_retry() const noexcept {
        return status == job_status::failed && retry_count < max_retries;
    }

    /**
     * @brief Get job duration
     * @return Duration from start to completion (or now if still running)
     */
    [[nodiscard]] std::chrono::milliseconds duration() const noexcept {
        if (!started_at.has_value()) {
            return std::chrono::milliseconds{0};
        }
        auto end_time = completed_at.value_or(std::chrono::system_clock::now());
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - started_at.value());
    }
};

// =============================================================================
// Callbacks
// =============================================================================

/**
 * @brief Callback for job progress updates
 *
 * @param job_id The ID of the job
 * @param progress Current progress information
 */
using job_progress_callback = std::function<void(
    const std::string& job_id,
    const job_progress& progress)>;

/**
 * @brief Callback for job completion
 *
 * @param job_id The ID of the job
 * @param record Final job record
 */
using job_completion_callback = std::function<void(
    const std::string& job_id,
    const job_record& record)>;

// =============================================================================
// Job Manager Configuration
// =============================================================================

/**
 * @brief Configuration for the job manager
 */
struct job_manager_config {
    size_t worker_count{4};                          ///< Number of worker threads
    size_t max_queue_size{1000};                     ///< Maximum jobs in queue
    std::chrono::seconds job_timeout{3600};          ///< Job timeout (1 hour default)
    bool persist_jobs{true};                         ///< Persist jobs to database
    bool auto_retry_failed{true};                    ///< Auto-retry failed jobs
    std::chrono::seconds retry_delay{60};            ///< Delay between retries
    std::string local_ae_title{"PACS_CLIENT"};       ///< Local AE title for operations
};

}  // namespace pacs::client
