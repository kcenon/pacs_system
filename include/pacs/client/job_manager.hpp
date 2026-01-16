/**
 * @file job_manager.hpp
 * @brief Job manager for asynchronous DICOM operations
 *
 * This file provides the job_manager class for managing background jobs
 * with worker thread pool, priority queue, and progress tracking.
 *
 * @see Issue #537 - Implement Job Manager for Async DICOM Operations
 * @see Issue #553 - Part 2: Job Manager Core Implementation
 */

#pragma once

#include "pacs/client/job_types.hpp"
#include "pacs/core/result.hpp"
#include "pacs/di/ilogger.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

// Forward declarations
namespace pacs::storage {
class job_repository;
}

namespace pacs::client {

// Forward declaration
class remote_node_manager;

// =============================================================================
// Job Manager
// =============================================================================

/**
 * @brief Manager for asynchronous DICOM job execution
 *
 * Provides background job execution with:
 * - Configurable worker thread pool
 * - Priority-based job queue
 * - Progress tracking with callbacks
 * - Job persistence for recovery
 * - Pause/resume/cancel capabilities
 *
 * Thread Safety:
 * - All public methods are thread-safe
 * - Callbacks are invoked from worker threads
 *
 * @example
 * @code
 * // Create manager with dependencies
 * auto job_repo = std::make_shared<job_repository>(db);
 * auto node_mgr = std::make_shared<remote_node_manager>(node_repo);
 * auto manager = std::make_unique<job_manager>(job_repo, node_mgr);
 *
 * // Start workers
 * manager->start_workers();
 *
 * // Create a retrieve job
 * auto job_id = manager->create_retrieve_job(
 *     "external-pacs",
 *     "1.2.3.4.5",  // Study UID
 *     std::nullopt,  // Series UID (all series)
 *     job_priority::high
 * );
 *
 * // Set progress callback
 * manager->set_progress_callback([](const std::string& id, const job_progress& p) {
 *     std::cout << "Job " << id << ": " << p.percent_complete << "%" << std::endl;
 * });
 *
 * // Wait for completion
 * auto future = manager->wait_for_completion(job_id);
 * auto result = future.get();
 * @endcode
 */
class job_manager {
public:
    // =========================================================================
    // Construction / Destruction
    // =========================================================================

    /**
     * @brief Construct a job manager with default configuration
     *
     * @param repo Job repository for persistence (required)
     * @param node_manager Remote node manager for DICOM operations (required)
     * @param logger Logger instance (optional, defaults to NullLogger)
     */
    explicit job_manager(
        std::shared_ptr<storage::job_repository> repo,
        std::shared_ptr<remote_node_manager> node_manager,
        std::shared_ptr<di::ILogger> logger = nullptr);

    /**
     * @brief Construct a job manager with custom configuration
     *
     * @param config Manager configuration
     * @param repo Job repository for persistence (required)
     * @param node_manager Remote node manager for DICOM operations (required)
     * @param logger Logger instance (optional, defaults to NullLogger)
     */
    explicit job_manager(
        const job_manager_config& config,
        std::shared_ptr<storage::job_repository> repo,
        std::shared_ptr<remote_node_manager> node_manager,
        std::shared_ptr<di::ILogger> logger = nullptr);

    /**
     * @brief Destructor - stops workers if running
     */
    ~job_manager();

    // Non-copyable, non-movable (due to internal threading)
    job_manager(const job_manager&) = delete;
    auto operator=(const job_manager&) -> job_manager& = delete;
    job_manager(job_manager&&) = delete;
    auto operator=(job_manager&&) -> job_manager& = delete;

    // =========================================================================
    // Job Creation
    // =========================================================================

    /**
     * @brief Create a retrieve job (C-MOVE/C-GET)
     *
     * Creates a job to retrieve DICOM objects from a remote node.
     *
     * @param source_node_id ID of the source remote node
     * @param study_uid Study Instance UID to retrieve
     * @param series_uid Optional Series Instance UID (retrieve all if not specified)
     * @param priority Job priority (default: normal)
     * @return Unique job ID
     */
    [[nodiscard]] auto create_retrieve_job(
        std::string_view source_node_id,
        std::string_view study_uid,
        std::optional<std::string_view> series_uid = std::nullopt,
        job_priority priority = job_priority::normal) -> std::string;

    /**
     * @brief Create a store job (C-STORE)
     *
     * Creates a job to store DICOM objects to a remote node.
     *
     * @param destination_node_id ID of the destination remote node
     * @param instance_uids SOP Instance UIDs to store
     * @param priority Job priority (default: normal)
     * @return Unique job ID
     */
    [[nodiscard]] auto create_store_job(
        std::string_view destination_node_id,
        const std::vector<std::string>& instance_uids,
        job_priority priority = job_priority::normal) -> std::string;

    /**
     * @brief Create a query job (C-FIND)
     *
     * Creates a job to query a remote node.
     *
     * @param node_id ID of the remote node to query
     * @param query_level Query retrieve level (PATIENT, STUDY, SERIES, IMAGE)
     * @param query_keys Query keys as key-value pairs
     * @param priority Job priority (default: normal)
     * @return Unique job ID
     */
    [[nodiscard]] auto create_query_job(
        std::string_view node_id,
        std::string_view query_level,
        const std::unordered_map<std::string, std::string>& query_keys,
        job_priority priority = job_priority::normal) -> std::string;

    /**
     * @brief Create a sync job
     *
     * Creates a job to synchronize data with a remote node.
     *
     * @param source_node_id ID of the source remote node
     * @param patient_id Optional patient ID filter
     * @param priority Job priority (default: low for background sync)
     * @return Unique job ID
     */
    [[nodiscard]] auto create_sync_job(
        std::string_view source_node_id,
        std::optional<std::string_view> patient_id = std::nullopt,
        job_priority priority = job_priority::low) -> std::string;

    /**
     * @brief Create a prefetch job
     *
     * Creates a job to prefetch prior studies for a patient.
     *
     * @param source_node_id ID of the source remote node
     * @param patient_id Patient ID to prefetch studies for
     * @param priority Job priority (default: low for background prefetch)
     * @return Unique job ID
     */
    [[nodiscard]] auto create_prefetch_job(
        std::string_view source_node_id,
        std::string_view patient_id,
        job_priority priority = job_priority::low) -> std::string;

    // =========================================================================
    // Job Control
    // =========================================================================

    /**
     * @brief Start a pending job
     *
     * Moves a pending job to the execution queue.
     *
     * @param job_id The job ID to start
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto start_job(std::string_view job_id) -> pacs::VoidResult;

    /**
     * @brief Pause a running or queued job
     *
     * @param job_id The job ID to pause
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto pause_job(std::string_view job_id) -> pacs::VoidResult;

    /**
     * @brief Resume a paused job
     *
     * @param job_id The job ID to resume
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto resume_job(std::string_view job_id) -> pacs::VoidResult;

    /**
     * @brief Cancel a job
     *
     * Cancels a running, queued, or pending job.
     *
     * @param job_id The job ID to cancel
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto cancel_job(std::string_view job_id) -> pacs::VoidResult;

    /**
     * @brief Retry a failed job
     *
     * Resets a failed job and queues it for re-execution.
     *
     * @param job_id The job ID to retry
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto retry_job(std::string_view job_id) -> pacs::VoidResult;

    /**
     * @brief Delete a job
     *
     * Removes a job from the system. Running jobs are cancelled first.
     *
     * @param job_id The job ID to delete
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto delete_job(std::string_view job_id) -> pacs::VoidResult;

    // =========================================================================
    // Job Queries
    // =========================================================================

    /**
     * @brief Get a job by ID
     *
     * @param job_id The job ID to retrieve
     * @return Optional containing the job if found
     */
    [[nodiscard]] auto get_job(std::string_view job_id) const
        -> std::optional<job_record>;

    /**
     * @brief List jobs with optional filters
     *
     * @param status Optional status filter
     * @param type Optional type filter
     * @param limit Maximum results (default: 100)
     * @param offset Result offset for pagination
     * @return Vector of matching jobs
     */
    [[nodiscard]] auto list_jobs(
        std::optional<job_status> status = std::nullopt,
        std::optional<job_type> type = std::nullopt,
        size_t limit = 100,
        size_t offset = 0) const -> std::vector<job_record>;

    /**
     * @brief List jobs by node ID
     *
     * Returns jobs that involve the specified node as source or destination.
     *
     * @param node_id The node ID to filter by
     * @return Vector of jobs involving the node
     */
    [[nodiscard]] auto list_jobs_by_node(std::string_view node_id) const
        -> std::vector<job_record>;

    // =========================================================================
    // Progress Monitoring
    // =========================================================================

    /**
     * @brief Get current progress for a job
     *
     * @param job_id The job ID
     * @return Current progress information
     */
    [[nodiscard]] auto get_progress(std::string_view job_id) const -> job_progress;

    /**
     * @brief Set the progress callback
     *
     * The callback is invoked whenever job progress is updated.
     * Only one callback can be set; setting a new one replaces the old.
     *
     * @param callback Function to call on progress updates
     */
    void set_progress_callback(job_progress_callback callback);

    /**
     * @brief Set the completion callback
     *
     * The callback is invoked when a job completes (success, failure, or cancel).
     * Only one callback can be set; setting a new one replaces the old.
     *
     * @param callback Function to call on job completion
     */
    void set_completion_callback(job_completion_callback callback);

    // =========================================================================
    // Wait for Completion
    // =========================================================================

    /**
     * @brief Wait for a job to complete
     *
     * Returns a future that completes when the job finishes.
     *
     * @param job_id The job ID to wait for
     * @return Future containing the final job record
     */
    [[nodiscard]] auto wait_for_completion(std::string_view job_id)
        -> std::future<job_record>;

    // =========================================================================
    // Worker Management
    // =========================================================================

    /**
     * @brief Start the worker threads
     *
     * Starts the configured number of worker threads.
     * Does nothing if already running.
     */
    void start_workers();

    /**
     * @brief Stop the worker threads
     *
     * Gracefully stops all worker threads.
     * Running jobs are allowed to complete or timeout.
     */
    void stop_workers();

    /**
     * @brief Check if workers are running
     *
     * @return true if worker threads are active
     */
    [[nodiscard]] auto is_running() const noexcept -> bool;

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get number of active (running) jobs
     *
     * @return Number of jobs currently executing
     */
    [[nodiscard]] auto active_jobs() const -> size_t;

    /**
     * @brief Get number of pending jobs
     *
     * @return Number of jobs waiting in queue
     */
    [[nodiscard]] auto pending_jobs() const -> size_t;

    /**
     * @brief Get number of jobs completed today
     *
     * @return Number of successful completions today
     */
    [[nodiscard]] auto completed_jobs_today() const -> size_t;

    /**
     * @brief Get number of jobs failed today
     *
     * @return Number of failures today
     */
    [[nodiscard]] auto failed_jobs_today() const -> size_t;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Get current configuration
     *
     * @return Current manager configuration
     */
    [[nodiscard]] auto config() const noexcept -> const job_manager_config&;

private:
    // =========================================================================
    // Private Implementation
    // =========================================================================

    struct impl;
    std::unique_ptr<impl> impl_;
};

}  // namespace pacs::client
