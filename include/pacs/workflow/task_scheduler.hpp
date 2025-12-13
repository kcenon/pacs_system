/**
 * @file task_scheduler.hpp
 * @brief Task scheduler service for automated PACS operations
 *
 * This file provides the task_scheduler class which schedules and executes
 * recurring maintenance tasks like cleanup, archiving, and verification.
 *
 * @see SRS-WKF-002 - Task Scheduler Service Specification
 * @see FR-4.7 - Automated Maintenance Tasks
 */

#pragma once

#include "task_scheduler_config.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <vector>

// Forward declarations for kcenon ecosystem
namespace kcenon::thread {
class thread_pool;
}  // namespace kcenon::thread

// Forward declarations for PACS modules
namespace pacs::storage {
class index_database;
class file_storage;
}  // namespace pacs::storage

namespace pacs::workflow {

// Forward declare Result type (defined in integration or common)
template <typename T>
class Result;

/**
 * @brief Task scheduler service for automated PACS operations
 *
 * The task_scheduler manages recurring maintenance tasks such as:
 * - **Cleanup**: Automatic removal of old studies based on retention policies
 * - **Archive**: Moving studies to secondary storage (HSM, cloud, tape)
 * - **Verification**: Periodic integrity checks and consistency validation
 *
 * ## Key Features
 *
 * - **Cron-like Scheduling**: Support for complex schedule expressions
 * - **Interval Scheduling**: Simple fixed-interval execution
 * - **One-time Tasks**: Single execution at specific time
 * - **Persistence**: Tasks survive service restarts
 * - **Parallel Execution**: Concurrent task execution with thread pool
 * - **Task Management**: List, cancel, pause, resume tasks
 * - **Execution History**: Track task execution records
 *
 * ## Integration with kcenon Ecosystem
 *
 * - **thread_system**: Task execution thread pool
 * - **logger_system**: Task execution logging and audit trails
 * - **monitoring_system**: Task execution metrics and health checks
 * - **common_system**: Result<T> for error handling
 *
 * ## Task Execution Flow
 *
 * ```
 * Scheduler Thread
 *      │
 *      ▼
 * ┌────────────────────────┐
 * │ Check Due Tasks        │ (every check_interval)
 * └───────────┬────────────┘
 *             │
 *             ▼
 * ┌────────────────────────┐
 * │ Sort by Priority       │
 * └───────────┬────────────┘
 *             │
 *     ┌───────┴───────┐
 *     ▼               ▼
 * ┌────────┐     ┌────────┐
 * │Task 1  │     │Task N  │  (Parallel via thread_pool)
 * └───┬────┘     └───┬────┘
 *     │              │
 *     ▼              ▼
 * ┌─────────────────────────┐
 * │ Execute Callback        │
 * └───────────┬─────────────┘
 *             │
 *             ▼
 * ┌─────────────────────────┐
 * │ Record Result           │
 * │ (success/failure)       │
 * └───────────┬─────────────┘
 *             │
 *             ▼
 * ┌─────────────────────────┐
 * │ Calculate Next Run      │
 * └───────────┬─────────────┘
 *             │
 *             ▼
 * ┌─────────────────────────┐
 * │ Notify Callbacks        │
 * └─────────────────────────┘
 * ```
 *
 * @example Basic Usage
 * @code
 * // Configure scheduler
 * task_scheduler_config config;
 * config.max_concurrent_tasks = 4;
 * config.check_interval = std::chrono::seconds{60};
 *
 * // Configure cleanup
 * config.cleanup = cleanup_config{};
 * config.cleanup->default_retention = std::chrono::days{365};
 *
 * // Create scheduler
 * task_scheduler scheduler{database, storage, config};
 *
 * // Start scheduler
 * scheduler.start();
 *
 * // Schedule custom task
 * auto task_id = scheduler.schedule(
 *     "my_task",
 *     "Custom maintenance",
 *     cron_schedule::daily_at(1, 30),  // 1:30 AM
 *     []() -> std::optional<std::string> {
 *         // Do work...
 *         return std::nullopt;  // Success
 *     }
 * );
 *
 * // List scheduled tasks
 * auto tasks = scheduler.list_tasks();
 *
 * // Stop scheduler
 * scheduler.stop();
 * @endcode
 *
 * @see task_scheduler_config
 * @see scheduled_task
 */
class task_scheduler {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct task scheduler
     *
     * @param database Reference to the PACS index database
     * @param config Service configuration
     */
    explicit task_scheduler(
        storage::index_database& database,
        const task_scheduler_config& config = {});

    /**
     * @brief Construct task scheduler with storage and thread pool
     *
     * @param database Reference to the PACS index database
     * @param file_storage Reference to file storage for cleanup/archive
     * @param thread_pool Thread pool for parallel task execution
     * @param config Service configuration
     */
    task_scheduler(
        storage::index_database& database,
        storage::file_storage& file_storage,
        std::shared_ptr<kcenon::thread::thread_pool> thread_pool,
        const task_scheduler_config& config = {});

    /**
     * @brief Destructor - ensures graceful shutdown
     */
    ~task_scheduler();

    /// Non-copyable
    task_scheduler(const task_scheduler&) = delete;
    task_scheduler& operator=(const task_scheduler&) = delete;

    /// Non-movable
    task_scheduler(task_scheduler&&) = delete;
    task_scheduler& operator=(task_scheduler&&) = delete;

    // =========================================================================
    // Lifecycle Management
    // =========================================================================

    /**
     * @brief Start the scheduler service
     *
     * Starts the background scheduler thread and begins monitoring
     * for tasks to execute.
     */
    void start();

    /**
     * @brief Stop the scheduler service
     *
     * Gracefully stops the service, waiting for any in-progress
     * tasks to complete.
     *
     * @param wait_for_completion If true, waits for running tasks
     */
    void stop(bool wait_for_completion = true);

    /**
     * @brief Check if the scheduler is running
     *
     * @return true if the scheduler is actively running
     */
    [[nodiscard]] auto is_running() const noexcept -> bool;

    // =========================================================================
    // Task Scheduling - Cleanup
    // =========================================================================

    /**
     * @brief Schedule cleanup task
     *
     * Creates a cleanup task with the specified configuration.
     * If a cleanup task already exists, updates its configuration.
     *
     * @param config Cleanup configuration
     * @return Task ID for the cleanup task
     */
    auto schedule_cleanup(const cleanup_config& config) -> task_id;

    // =========================================================================
    // Task Scheduling - Archive
    // =========================================================================

    /**
     * @brief Schedule archive task
     *
     * Creates an archive task with the specified configuration.
     * If an archive task already exists, updates its configuration.
     *
     * @param config Archive configuration
     * @return Task ID for the archive task
     */
    auto schedule_archive(const archive_config& config) -> task_id;

    // =========================================================================
    // Task Scheduling - Verification
    // =========================================================================

    /**
     * @brief Schedule verification task
     *
     * Creates a verification task with the specified configuration.
     * If a verification task already exists, updates its configuration.
     *
     * @param config Verification configuration
     * @return Task ID for the verification task
     */
    auto schedule_verification(const verification_config& config) -> task_id;

    // =========================================================================
    // Task Scheduling - Custom
    // =========================================================================

    /**
     * @brief Schedule a custom task with interval
     *
     * @param name Task name
     * @param description Task description
     * @param interval Interval between executions
     * @param callback Task callback function
     * @return Task ID
     */
    auto schedule(
        const std::string& name,
        const std::string& description,
        std::chrono::seconds interval,
        task_callback_with_result callback) -> task_id;

    /**
     * @brief Schedule a custom task with cron schedule
     *
     * @param name Task name
     * @param description Task description
     * @param cron_expr Cron schedule expression
     * @param callback Task callback function
     * @return Task ID
     */
    auto schedule(
        const std::string& name,
        const std::string& description,
        const cron_schedule& cron_expr,
        task_callback_with_result callback) -> task_id;

    /**
     * @brief Schedule a one-time task
     *
     * @param name Task name
     * @param description Task description
     * @param execute_at Execution time
     * @param callback Task callback function
     * @return Task ID
     */
    auto schedule_once(
        const std::string& name,
        const std::string& description,
        std::chrono::system_clock::time_point execute_at,
        task_callback_with_result callback) -> task_id;

    /**
     * @brief Schedule a task with full definition
     *
     * @param task Complete task definition
     * @return Task ID
     */
    auto schedule(scheduled_task task) -> task_id;

    // =========================================================================
    // Task Management
    // =========================================================================

    /**
     * @brief List all scheduled tasks
     *
     * @return Vector of all scheduled tasks
     */
    [[nodiscard]] auto list_tasks() const -> std::vector<scheduled_task>;

    /**
     * @brief List tasks by type
     *
     * @param type Task type to filter by
     * @return Vector of matching tasks
     */
    [[nodiscard]] auto list_tasks(task_type type) const
        -> std::vector<scheduled_task>;

    /**
     * @brief List tasks by state
     *
     * @param state Task state to filter by
     * @return Vector of matching tasks
     */
    [[nodiscard]] auto list_tasks(task_state state) const
        -> std::vector<scheduled_task>;

    /**
     * @brief Get a specific task by ID
     *
     * @param id Task ID
     * @return Task if found, nullopt otherwise
     */
    [[nodiscard]] auto get_task(const task_id& id) const
        -> std::optional<scheduled_task>;

    /**
     * @brief Cancel a scheduled task
     *
     * Removes the task from the schedule. If the task is currently
     * running, it will complete but not be rescheduled.
     *
     * @param id Task ID to cancel
     * @return true if task was found and cancelled
     */
    auto cancel_task(const task_id& id) -> bool;

    /**
     * @brief Pause a scheduled task
     *
     * Temporarily suspends task execution. The task remains scheduled
     * but will not execute until resumed.
     *
     * @param id Task ID to pause
     * @return true if task was found and paused
     */
    auto pause_task(const task_id& id) -> bool;

    /**
     * @brief Resume a paused task
     *
     * @param id Task ID to resume
     * @return true if task was found and resumed
     */
    auto resume_task(const task_id& id) -> bool;

    /**
     * @brief Trigger immediate execution of a task
     *
     * Executes the task immediately, regardless of its schedule.
     *
     * @param id Task ID to execute
     * @return true if task was found and triggered
     */
    auto trigger_task(const task_id& id) -> bool;

    /**
     * @brief Update task schedule
     *
     * @param id Task ID
     * @param new_schedule New schedule
     * @return true if task was found and updated
     */
    auto update_schedule(const task_id& id, const pacs::workflow::schedule& new_schedule) -> bool;

    // =========================================================================
    // Execution History
    // =========================================================================

    /**
     * @brief Get execution history for a task
     *
     * @param id Task ID
     * @param limit Maximum number of records to return
     * @return Vector of execution records (most recent first)
     */
    [[nodiscard]] auto get_execution_history(
        const task_id& id,
        std::size_t limit = 100) const -> std::vector<task_execution_record>;

    /**
     * @brief Get all recent executions
     *
     * @param limit Maximum number of records to return
     * @return Vector of execution records (most recent first)
     */
    [[nodiscard]] auto get_recent_executions(std::size_t limit = 100) const
        -> std::vector<task_execution_record>;

    /**
     * @brief Clear execution history for a task
     *
     * @param id Task ID
     * @param keep_last Number of records to keep
     */
    void clear_history(const task_id& id, std::size_t keep_last = 0);

    // =========================================================================
    // Statistics and Monitoring
    // =========================================================================

    /**
     * @brief Get scheduler statistics
     *
     * @return Current scheduler statistics
     */
    [[nodiscard]] auto get_stats() const -> scheduler_stats;

    /**
     * @brief Get number of pending tasks
     *
     * @return Number of tasks waiting to execute
     */
    [[nodiscard]] auto pending_count() const noexcept -> std::size_t;

    /**
     * @brief Get number of running tasks
     *
     * @return Number of tasks currently executing
     */
    [[nodiscard]] auto running_count() const noexcept -> std::size_t;

    // =========================================================================
    // Persistence
    // =========================================================================

    /**
     * @brief Save all tasks to persistence storage
     *
     * @return true if saved successfully
     */
    auto save_tasks() const -> bool;

    /**
     * @brief Load tasks from persistence storage
     *
     * @return Number of tasks loaded
     */
    auto load_tasks() -> std::size_t;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Set the task complete callback
     *
     * @param callback Function called after each task execution
     */
    void set_task_complete_callback(
        task_scheduler_config::task_complete_callback callback);

    /**
     * @brief Set the error callback
     *
     * @param callback Function called when a task fails
     */
    void set_error_callback(
        task_scheduler_config::task_error_callback callback);

private:
    // =========================================================================
    // Internal Methods
    // =========================================================================

    /**
     * @brief Background thread main loop
     */
    void run_loop();

    /**
     * @brief Execute a single scheduler cycle
     *
     * Checks for due tasks and executes them.
     */
    void execute_cycle();

    /**
     * @brief Execute a single task
     *
     * @param task Task to execute
     * @return Execution record
     */
    auto execute_task(scheduled_task& task) -> task_execution_record;

    /**
     * @brief Calculate next run time for a schedule
     *
     * @param sched Schedule definition
     * @param from Starting time point
     * @return Next run time
     */
    [[nodiscard]] auto calculate_next_run(
        const pacs::workflow::schedule& sched,
        std::chrono::system_clock::time_point from =
            std::chrono::system_clock::now()) const
        -> std::optional<std::chrono::system_clock::time_point>;

    /**
     * @brief Calculate next run time for cron schedule
     *
     * @param cron Cron schedule
     * @param from Starting time point
     * @return Next run time
     */
    [[nodiscard]] auto calculate_next_cron_run(
        const cron_schedule& cron,
        std::chrono::system_clock::time_point from) const
        -> std::optional<std::chrono::system_clock::time_point>;

    /**
     * @brief Generate unique task ID
     *
     * @return New unique task ID
     */
    [[nodiscard]] auto generate_task_id() const -> task_id;

    /**
     * @brief Generate unique execution ID
     *
     * @return New unique execution ID
     */
    [[nodiscard]] auto generate_execution_id() const -> std::string;

    /**
     * @brief Create cleanup task callback
     *
     * @param config Cleanup configuration
     * @return Task callback
     */
    [[nodiscard]] auto create_cleanup_callback(const cleanup_config& config)
        -> task_callback_with_result;

    /**
     * @brief Create archive task callback
     *
     * @param config Archive configuration
     * @return Task callback
     */
    [[nodiscard]] auto create_archive_callback(const archive_config& config)
        -> task_callback_with_result;

    /**
     * @brief Create verification task callback
     *
     * @param config Verification configuration
     * @return Task callback
     */
    [[nodiscard]] auto create_verification_callback(const verification_config& config)
        -> task_callback_with_result;

    /**
     * @brief Record task execution
     *
     * @param task_id Task ID
     * @param record Execution record
     */
    void record_execution(const task_id& task_id,
                         const task_execution_record& record);

    /**
     * @brief Update statistics after execution
     *
     * @param record Execution record
     */
    void update_stats(const task_execution_record& record);

    /**
     * @brief Serialize tasks to JSON
     *
     * @return JSON string representation
     */
    [[nodiscard]] auto serialize_tasks() const -> std::string;

    /**
     * @brief Deserialize tasks from JSON
     *
     * @param json JSON string
     * @return Number of tasks loaded
     */
    auto deserialize_tasks(const std::string& json) -> std::size_t;

    // =========================================================================
    // Member Variables
    // =========================================================================

    /// Reference to PACS index database
    storage::index_database& database_;

    /// Optional reference to file storage
    storage::file_storage* file_storage_{nullptr};

    /// Thread pool for parallel task execution
    std::shared_ptr<kcenon::thread::thread_pool> thread_pool_;

    /// Service configuration
    task_scheduler_config config_;

    /// Background scheduler thread
    std::thread scheduler_thread_;

    /// Mutex for thread synchronization
    mutable std::mutex mutex_;

    /// Condition variable for sleep/wake
    std::condition_variable cv_;

    /// Flag to signal shutdown
    std::atomic<bool> stop_requested_{false};

    /// Flag indicating scheduler is running
    std::atomic<bool> running_{false};

    /// All scheduled tasks (id -> task)
    std::map<task_id, scheduled_task> tasks_;

    /// Mutex for tasks map
    mutable std::mutex tasks_mutex_;

    /// Execution history (task_id -> records)
    std::map<task_id, std::vector<task_execution_record>> execution_history_;

    /// Mutex for execution history
    mutable std::mutex history_mutex_;

    /// Running task count
    std::atomic<std::size_t> running_count_{0};

    /// Scheduler statistics
    scheduler_stats stats_;

    /// Mutex for statistics
    mutable std::mutex stats_mutex_;

    /// Start time for uptime calculation
    std::chrono::steady_clock::time_point start_time_;

    /// Next task ID counter
    mutable std::atomic<uint64_t> next_task_id_{1};

    /// Next execution ID counter
    mutable std::atomic<uint64_t> next_execution_id_{1};
};

}  // namespace pacs::workflow
