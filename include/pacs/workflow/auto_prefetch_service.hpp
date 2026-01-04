/**
 * @file auto_prefetch_service.hpp
 * @brief Automatic prefetch service for prior studies
 *
 * This file provides the auto_prefetch_service class which automatically
 * prefetches prior patient studies from remote PACS when patients appear
 * in the modality worklist.
 *
 * @see SRS-WKF-001 - Auto Prefetch Service Specification
 * @see FR-4.6 - Automatic Prior Study Prefetch
 */

#pragma once

#include "prefetch_config.hpp"
#include "pacs/storage/worklist_record.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <set>
#include <string>
#include <thread>
#include <vector>

// Forward declarations for kcenon ecosystem
namespace kcenon::thread {
class thread_pool;
}  // namespace kcenon::thread

namespace kcenon::logger {
class logger;
}  // namespace kcenon::logger

namespace kcenon::common::interfaces {
class IExecutor;
}  // namespace kcenon::common::interfaces

// Forward declarations for PACS modules
namespace pacs::network {
class association;
}  // namespace pacs::network

namespace pacs::storage {
class index_database;
}  // namespace pacs::storage

namespace pacs::core {
class dicom_dataset;
}  // namespace pacs::core

namespace pacs::workflow {

/**
 * @brief Prefetch request for a single patient
 *
 * Represents a request to prefetch prior studies for a patient
 * based on worklist information.
 */
struct prefetch_request {
    /// Patient ID
    std::string patient_id;

    /// Patient Name
    std::string patient_name;

    /// Scheduled modality (for preference matching)
    std::string scheduled_modality;

    /// Scheduled body part (for preference matching)
    std::string scheduled_body_part;

    /// Study Instance UID of scheduled study (to avoid prefetching)
    std::string scheduled_study_uid;

    /// Request timestamp
    std::chrono::system_clock::time_point request_time;

    /// Number of retry attempts
    std::size_t retry_count{0};
};

/**
 * @brief Automatic prefetch service for prior patient studies
 *
 * The auto_prefetch_service monitors worklist queries and automatically
 * prefetches prior patient studies from remote PACS servers. This reduces
 * image retrieval time during radiologist reading sessions.
 *
 * ## Key Features
 *
 * - **Worklist-Triggered Prefetch**: Automatically prefetches priors when
 *   a patient appears in the modality worklist
 * - **Configurable Selection**: Filter priors by modality, body part,
 *   lookback period, and other criteria
 * - **Multi-Source Support**: Can prefetch from multiple remote PACS
 * - **Parallel Processing**: Uses thread_system for concurrent prefetches
 * - **Rate Limiting**: Prevents overloading remote PACS with requests
 * - **Retry Logic**: Automatically retries failed prefetches
 *
 * ## Integration with kcenon Ecosystem
 *
 * - **thread_system**: Background prefetch workers and parallel C-MOVE
 * - **logger_system**: Audit logging for prefetch operations
 * - **monitoring_system**: Metrics for prefetch success/failure rates
 *
 * ## Prefetch Workflow
 *
 * ```
 * Worklist Query
 *      │
 *      ▼
 * ┌────────────────────────┐
 * │ Extract Patient IDs    │
 * └───────────┬────────────┘
 *             │
 *             ▼
 * ┌────────────────────────┐
 * │ Queue Prefetch Request │
 * └───────────┬────────────┘
 *             │
 *     ┌───────┴───────┐
 *     ▼               ▼
 * ┌────────┐     ┌────────┐
 * │Worker 1│     │Worker N│  (Parallel)
 * └───┬────┘     └───┬────┘
 *     │              │
 *     ▼              ▼
 * ┌─────────────────────────┐
 * │ Query Remote PACS       │
 * │ (C-FIND for priors)     │
 * └───────────┬─────────────┘
 *             │
 *             ▼
 * ┌─────────────────────────┐
 * │ Filter by Criteria      │
 * │ (modality, date, etc.)  │
 * └───────────┬─────────────┘
 *             │
 *             ▼
 * ┌─────────────────────────┐
 * │ Check Local Storage     │
 * │ (skip if present)       │
 * └───────────┬─────────────┘
 *             │
 *             ▼
 * ┌─────────────────────────┐
 * │ C-MOVE from Remote      │
 * │ (prefetch images)       │
 * └───────────┬─────────────┘
 *             │
 *             ▼
 * ┌─────────────────────────┐
 * │ Update Statistics       │
 * └─────────────────────────┘
 * ```
 *
 * @example Basic Usage
 * @code
 * // Configure prefetch service
 * prefetch_service_config config;
 * config.prefetch_interval = std::chrono::minutes{5};
 * config.max_concurrent_prefetches = 4;
 * config.criteria.lookback_period = std::chrono::days{365};
 *
 * // Add remote PACS
 * remote_pacs_config remote;
 * remote.ae_title = "ARCHIVE_PACS";
 * remote.host = "192.168.1.100";
 * remote.port = 11112;
 * config.remote_pacs.push_back(remote);
 *
 * // Set callbacks
 * config.on_cycle_complete = [](const prefetch_result& r) {
 *     std::cout << "Prefetched " << r.studies_prefetched << " studies\n";
 * };
 *
 * // Create and start service
 * auto_prefetch_service service{database, config};
 * service.start();
 *
 * // Trigger prefetch for scheduled patient
 * service.prefetch_priors("PATIENT123", std::chrono::days{365});
 *
 * // Later: stop service
 * service.stop();
 * @endcode
 *
 * @see prefetch_service_config
 * @see prefetch_result
 */
class auto_prefetch_service {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct auto prefetch service
     *
     * @param database Reference to the PACS index database for checking
     *                 existing studies and receiving worklist events
     * @param config Service configuration
     */
    explicit auto_prefetch_service(
        storage::index_database& database,
        const prefetch_service_config& config = {});

    /**
     * @brief Construct auto prefetch service with thread pool
     *
     * @param database Reference to the PACS index database
     * @param thread_pool Thread pool for parallel prefetch operations
     * @param config Service configuration
     */
    auto_prefetch_service(
        storage::index_database& database,
        std::shared_ptr<kcenon::thread::thread_pool> thread_pool,
        const prefetch_service_config& config = {});

    /**
     * @brief Construct auto prefetch service with IExecutor (recommended)
     *
     * This constructor accepts the standardized IExecutor interface from
     * common_system, enabling better testability and decoupling from
     * specific thread pool implementations.
     *
     * @param database Reference to the PACS index database
     * @param executor IExecutor for task execution (from common_system)
     * @param config Service configuration
     *
     * @see Issue #487 - IExecutor integration
     */
    auto_prefetch_service(
        storage::index_database& database,
        std::shared_ptr<kcenon::common::interfaces::IExecutor> executor,
        const prefetch_service_config& config = {});

    /**
     * @brief Destructor - ensures graceful shutdown
     */
    ~auto_prefetch_service();

    /// Non-copyable
    auto_prefetch_service(const auto_prefetch_service&) = delete;
    auto_prefetch_service& operator=(const auto_prefetch_service&) = delete;

    /// Non-movable
    auto_prefetch_service(auto_prefetch_service&&) = delete;
    auto_prefetch_service& operator=(auto_prefetch_service&&) = delete;

    // =========================================================================
    // Lifecycle Management
    // =========================================================================

    /**
     * @brief Enable the prefetch service
     *
     * Starts the background prefetch thread and begins monitoring
     * worklist queries for prefetch candidates.
     */
    void enable();

    /**
     * @brief Start the prefetch service (alias for enable)
     */
    void start();

    /**
     * @brief Disable/stop the prefetch service
     *
     * Gracefully stops the service, waiting for any in-progress
     * prefetch operations to complete.
     *
     * @param wait_for_completion If true, waits for current operations
     */
    void disable(bool wait_for_completion = true);

    /**
     * @brief Stop the prefetch service (alias for disable)
     *
     * @param wait_for_completion If true, waits for current operations
     */
    void stop(bool wait_for_completion = true);

    /**
     * @brief Check if the service is enabled/running
     *
     * @return true if the service is actively prefetching
     */
    [[nodiscard]] auto is_enabled() const noexcept -> bool;

    /**
     * @brief Check if the service is running (alias for is_enabled)
     *
     * @return true if the service is actively prefetching
     */
    [[nodiscard]] auto is_running() const noexcept -> bool;

    // =========================================================================
    // Manual Operations
    // =========================================================================

    /**
     * @brief Manually prefetch prior studies for a patient
     *
     * Immediately triggers a prefetch operation for the specified patient,
     * regardless of whether the service is enabled or the patient is in
     * the worklist.
     *
     * @param patient_id The patient ID to prefetch priors for
     * @param lookback Lookback period (default: use config value)
     * @return Result with count of prefetched studies, or error
     */
    [[nodiscard]] auto prefetch_priors(
        const std::string& patient_id,
        std::chrono::days lookback = std::chrono::days{365})
        -> prefetch_result;

    /**
     * @brief Trigger prefetch for worklist items
     *
     * Queues prefetch requests for all patients in the provided
     * worklist items. Useful for batch prefetching.
     *
     * @param worklist_items Vector of worklist items to prefetch for
     */
    void trigger_for_worklist(
        const std::vector<storage::worklist_item>& worklist_items);

    /**
     * @brief Trigger next cycle immediately
     *
     * Wakes up the background thread to run a prefetch cycle immediately,
     * without waiting for the scheduled interval.
     */
    void trigger_cycle();

    /**
     * @brief Run a prefetch cycle manually
     *
     * Executes a complete prefetch cycle for all queued patients.
     * Can be called whether the service is enabled or not.
     *
     * @return Prefetch result with statistics
     */
    [[nodiscard]] auto run_prefetch_cycle() -> prefetch_result;

    // =========================================================================
    // Worklist Event Handler
    // =========================================================================

    /**
     * @brief Handle worklist query event
     *
     * Called when a worklist query is processed. Extracts patient
     * information and queues prefetch requests for each unique patient.
     *
     * @param worklist_items The worklist items returned by the query
     */
    void on_worklist_query(
        const std::vector<storage::worklist_item>& worklist_items);

    // =========================================================================
    // Statistics and Monitoring
    // =========================================================================

    /**
     * @brief Get the result of the last prefetch cycle
     *
     * @return Last prefetch result, or nullopt if no cycle has run
     */
    [[nodiscard]] auto get_last_result() const
        -> std::optional<prefetch_result>;

    /**
     * @brief Get cumulative statistics since service started
     *
     * @return Cumulative prefetch statistics
     */
    [[nodiscard]] auto get_cumulative_stats() const -> prefetch_result;

    /**
     * @brief Get the time until the next scheduled prefetch cycle
     *
     * @return Duration until next cycle, or nullopt if not scheduled
     */
    [[nodiscard]] auto time_until_next_cycle() const
        -> std::optional<std::chrono::seconds>;

    /**
     * @brief Get the number of cycles completed
     *
     * @return Total number of completed prefetch cycles
     */
    [[nodiscard]] auto cycles_completed() const noexcept -> std::size_t;

    /**
     * @brief Get the number of pending prefetch requests
     *
     * @return Number of patients waiting to be prefetched
     */
    [[nodiscard]] auto pending_requests() const noexcept -> std::size_t;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Update the prefetch interval
     *
     * @param interval New interval between prefetch cycles
     *
     * @note Takes effect at the next cycle
     */
    void set_prefetch_interval(std::chrono::seconds interval);

    /**
     * @brief Get the current prefetch interval
     *
     * @return Current interval between prefetch cycles
     */
    [[nodiscard]] auto get_prefetch_interval() const noexcept
        -> std::chrono::seconds;

    /**
     * @brief Update the prefetch criteria
     *
     * @param criteria New selection criteria for prior studies
     */
    void set_prefetch_criteria(const prefetch_criteria& criteria);

    /**
     * @brief Get the current prefetch criteria
     *
     * @return Current selection criteria
     */
    [[nodiscard]] auto get_prefetch_criteria() const noexcept
        -> const prefetch_criteria&;

    /**
     * @brief Set the cycle complete callback
     *
     * @param callback Function called after each prefetch cycle
     */
    void set_cycle_complete_callback(
        prefetch_service_config::cycle_complete_callback callback);

    /**
     * @brief Set the error callback
     *
     * @param callback Function called when a prefetch fails
     */
    void set_error_callback(
        prefetch_service_config::error_callback callback);

private:
    // =========================================================================
    // Internal Methods
    // =========================================================================

    /**
     * @brief Background thread main loop
     */
    void run_loop();

    /**
     * @brief Execute a single prefetch cycle
     * @return Prefetch result
     */
    [[nodiscard]] auto execute_cycle() -> prefetch_result;

    /**
     * @brief Process a single prefetch request
     *
     * @param request The prefetch request to process
     * @return Prefetch result for this request
     */
    [[nodiscard]] auto process_request(const prefetch_request& request)
        -> prefetch_result;

    /**
     * @brief Query remote PACS for prior studies
     *
     * @param pacs_config Remote PACS configuration
     * @param patient_id Patient ID to query
     * @param lookback Lookback period
     * @return Vector of prior study information
     */
    [[nodiscard]] auto query_prior_studies(
        const remote_pacs_config& pacs_config,
        const std::string& patient_id,
        std::chrono::days lookback) -> std::vector<prior_study_info>;

    /**
     * @brief Filter prior studies based on criteria
     *
     * @param studies List of candidate studies
     * @param request Original prefetch request
     * @return Filtered list of studies to prefetch
     */
    [[nodiscard]] auto filter_studies(
        const std::vector<prior_study_info>& studies,
        const prefetch_request& request) -> std::vector<prior_study_info>;

    /**
     * @brief Check if study already exists locally
     *
     * @param study_uid Study Instance UID
     * @return true if study is already in local storage
     */
    [[nodiscard]] auto study_exists_locally(const std::string& study_uid)
        -> bool;

    /**
     * @brief Prefetch a single study via C-MOVE
     *
     * @param pacs_config Remote PACS configuration
     * @param study Study information
     * @return true if prefetch was successful
     */
    [[nodiscard]] auto prefetch_study(
        const remote_pacs_config& pacs_config,
        const prior_study_info& study) -> bool;

    /**
     * @brief Update cumulative statistics
     *
     * @param result Result from latest operation
     */
    void update_stats(const prefetch_result& result);

    /**
     * @brief Add request to queue (deduplicated)
     *
     * @param request The prefetch request to queue
     */
    void queue_request(const prefetch_request& request);

    /**
     * @brief Get next request from queue
     *
     * @return Optional containing next request, or nullopt if empty
     */
    [[nodiscard]] auto dequeue_request()
        -> std::optional<prefetch_request>;

    // =========================================================================
    // Member Variables
    // =========================================================================

    /// Reference to PACS index database
    storage::index_database& database_;

    /// Thread pool for parallel prefetches (optional, legacy)
    std::shared_ptr<kcenon::thread::thread_pool> thread_pool_;

    /// IExecutor for task execution (recommended, Issue #487)
    std::shared_ptr<kcenon::common::interfaces::IExecutor> executor_;

    /// Service configuration
    prefetch_service_config config_;

    /// Background worker thread
    std::thread worker_thread_;

    /// Mutex for thread synchronization
    mutable std::mutex mutex_;

    /// Condition variable for sleep/wake
    std::condition_variable cv_;

    /// Flag to signal shutdown
    std::atomic<bool> stop_requested_{false};

    /// Flag indicating service is enabled
    std::atomic<bool> enabled_{false};

    /// Flag indicating a cycle is in progress
    std::atomic<bool> cycle_in_progress_{false};

    /// Queue of pending prefetch requests
    std::queue<prefetch_request> request_queue_;

    /// Set of patient IDs currently in queue (for deduplication)
    std::set<std::string> queued_patients_;

    /// Mutex for request queue
    mutable std::mutex queue_mutex_;

    /// Last prefetch result
    std::optional<prefetch_result> last_result_;

    /// Cumulative statistics
    prefetch_result cumulative_stats_;

    /// Number of completed cycles
    std::atomic<std::size_t> cycles_count_{0};

    /// Time of next scheduled cycle
    std::chrono::steady_clock::time_point next_cycle_time_;
};

}  // namespace pacs::workflow
