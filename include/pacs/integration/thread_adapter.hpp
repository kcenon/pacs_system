/**
 * @file thread_adapter.hpp
 * @brief Adapter for integrating thread_system job queue and thread pool
 *
 * This file provides the thread_adapter class for integrating thread_system's
 * high-performance thread pool with PACS operations. It supports job submission,
 * priority scheduling, and DIMSE-specific job wrappers.
 *
 * @see IR-3 (thread_system Integration)
 */

#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>

namespace kcenon::thread {
class thread_pool;
}  // namespace kcenon::thread

namespace pacs::integration {

// ─────────────────────────────────────────────────────
// Forward Declarations
// ─────────────────────────────────────────────────────

/**
 * @enum job_priority
 * @brief Priority levels for job scheduling
 *
 * Jobs with higher priority (lower numeric value) are processed first.
 * This enables critical DICOM operations to be handled with urgency.
 */
enum class job_priority {
    critical = 0,  ///< C-ECHO, association handling - highest priority
    high = 1,      ///< C-STORE responses
    normal = 2,    ///< C-FIND queries
    low = 3        ///< Background tasks (cleanup, maintenance)
};

/**
 * @struct thread_pool_config
 * @brief Configuration options for the thread pool
 */
struct thread_pool_config {
    /// Minimum number of worker threads
    std::size_t min_threads = 2;

    /// Maximum number of worker threads
    std::size_t max_threads = std::thread::hardware_concurrency();

    /// Time before idle threads are terminated (for dynamic scaling)
    std::chrono::milliseconds idle_timeout{30000};

    /// Enable lock-free queue for higher throughput
    bool use_lock_free_queue = true;

    /// Thread pool name for logging
    std::string pool_name = "pacs_thread_pool";
};

// ─────────────────────────────────────────────────────
// Thread Adapter Class
// ─────────────────────────────────────────────────────

/**
 * @class thread_adapter
 * @brief Adapter for integrating thread_system job queue and thread pool
 *
 * This class provides a PACS-specific interface to thread_system's
 * high-performance thread pool. Key features include:
 *
 * - **Lock-free Job Queue**: Uses thread_system's lock-free queue for
 *   high throughput in concurrent scenarios
 * - **Priority Scheduling**: Critical operations (C-ECHO) get priority
 *   over normal operations (C-FIND)
 * - **Automatic Scaling**: Thread pool grows/shrinks based on workload
 * - **DIMSE Job Wrappers**: Convenient submission methods for DICOM operations
 *
 * Thread Safety: All public methods are thread-safe and can be called
 * from any thread.
 *
 * @example
 * @code
 * // Configure and start the thread pool
 * thread_pool_config config;
 * config.min_threads = 4;
 * config.max_threads = 16;
 * thread_adapter::configure(config);
 * thread_adapter::start();
 *
 * // Submit a task with result
 * auto future = thread_adapter::submit([]() {
 *     return 42;
 * });
 * int result = future.get();
 *
 * // Submit with priority
 * thread_adapter::submit_with_priority(job_priority::high, []() {
 *     // High priority work
 * });
 *
 * // Fire and forget task
 * thread_adapter::submit_fire_and_forget([]() {
 *     // Background work
 * });
 *
 * // Graceful shutdown
 * thread_adapter::shutdown();
 * @endcode
 */
class thread_adapter {
public:
    // ─────────────────────────────────────────────────────
    // Thread Pool Management
    // ─────────────────────────────────────────────────────

    /**
     * @brief Get the singleton thread pool instance
     *
     * Returns a reference to the shared thread pool. If the pool
     * has not been configured, a default configuration is used.
     *
     * @return Reference to the thread pool
     */
    [[nodiscard]] static auto get_pool() -> std::shared_ptr<kcenon::thread::thread_pool>;

    /**
     * @brief Configure the thread pool
     *
     * Must be called before start() for the configuration to take effect.
     * If called after start(), the new configuration will be applied
     * on the next restart.
     *
     * @param config Configuration options for the thread pool
     */
    static void configure(const thread_pool_config& config);

    /**
     * @brief Get the current configuration
     *
     * @return Current thread pool configuration
     */
    [[nodiscard]] static auto get_config() noexcept -> const thread_pool_config&;

    /**
     * @brief Start the thread pool
     *
     * Initializes and starts worker threads according to the
     * current configuration. Safe to call multiple times;
     * subsequent calls are no-ops if already running.
     *
     * @return true if started successfully, false otherwise
     */
    [[nodiscard]] static auto start() -> bool;

    /**
     * @brief Check if the thread pool is running
     *
     * @return true if the pool is active, false otherwise
     */
    [[nodiscard]] static auto is_running() noexcept -> bool;

    /**
     * @brief Shutdown the thread pool
     *
     * Stops accepting new jobs and waits for pending jobs to complete.
     * If wait_for_completion is false, attempts to cancel pending jobs.
     *
     * @param wait_for_completion If true, waits for all pending jobs to complete
     */
    static void shutdown(bool wait_for_completion = true);

    // ─────────────────────────────────────────────────────
    // Job Submission
    // ─────────────────────────────────────────────────────

    /**
     * @brief Submit a task for execution and get a future for the result
     *
     * This is the primary method for submitting tasks that return a value.
     * The task will be executed asynchronously by a worker thread.
     *
     * @tparam F Callable type
     * @param task The task to execute
     * @return Future for the task result
     *
     * @example
     * @code
     * auto future = thread_adapter::submit([]() {
     *     return expensive_computation();
     * });
     * auto result = future.get(); // Blocks until complete
     * @endcode
     */
    template <typename F>
    [[nodiscard]] static auto submit(F&& task)
        -> std::future<std::invoke_result_t<std::decay_t<F>>>;

    /**
     * @brief Submit a task without waiting for the result
     *
     * Use this for tasks where you don't need the result or
     * don't want to block. Errors are logged but not propagated.
     *
     * @tparam F Callable type
     * @param task The task to execute
     *
     * @example
     * @code
     * thread_adapter::submit_fire_and_forget([]() {
     *     write_audit_log("operation completed");
     * });
     * @endcode
     */
    template <typename F>
    static void submit_fire_and_forget(F&& task);

    // ─────────────────────────────────────────────────────
    // Priority Queue
    // ─────────────────────────────────────────────────────

    /**
     * @brief Submit a task with a specific priority level
     *
     * Jobs with higher priority (lower numeric value) are processed first.
     * This is useful for ensuring critical operations are handled promptly.
     *
     * @tparam F Callable type
     * @param priority The priority level for this task
     * @param task The task to execute
     * @return Future for the task result
     *
     * @example
     * @code
     * // Critical C-ECHO response
     * auto future = thread_adapter::submit_with_priority(
     *     job_priority::critical,
     *     []() { return handle_echo_request(); }
     * );
     * @endcode
     */
    template <typename F>
    [[nodiscard]] static auto submit_with_priority(job_priority priority, F&& task)
        -> std::future<std::invoke_result_t<std::decay_t<F>>>;

    // ─────────────────────────────────────────────────────
    // Statistics
    // ─────────────────────────────────────────────────────

    /**
     * @brief Get the current number of worker threads
     *
     * @return Number of active worker threads
     */
    [[nodiscard]] static auto get_thread_count() -> std::size_t;

    /**
     * @brief Get the number of pending jobs in the queue
     *
     * @return Number of jobs waiting to be processed
     */
    [[nodiscard]] static auto get_pending_job_count() -> std::size_t;

    /**
     * @brief Get the number of idle workers
     *
     * @return Number of workers not currently processing jobs
     */
    [[nodiscard]] static auto get_idle_worker_count() -> std::size_t;

private:
    // Singleton instance management
    static std::shared_ptr<kcenon::thread::thread_pool> pool_;
    static thread_pool_config config_;
    static std::mutex mutex_;
    static bool initialized_;

    // Priority management (for future priority queue support)
    static void submit_job_internal(std::function<void()> task, job_priority priority);

    // Prevent instantiation
    thread_adapter() = delete;
    ~thread_adapter() = delete;
    thread_adapter(const thread_adapter&) = delete;
    thread_adapter& operator=(const thread_adapter&) = delete;
};

// ─────────────────────────────────────────────────────
// Template Implementation
// ─────────────────────────────────────────────────────

template <typename F>
auto thread_adapter::submit(F&& task)
    -> std::future<std::invoke_result_t<std::decay_t<F>>> {
    using return_type = std::invoke_result_t<std::decay_t<F>>;

    auto packaged_task =
        std::make_shared<std::packaged_task<return_type()>>(std::forward<F>(task));
    auto future = packaged_task->get_future();

    submit_job_internal([packaged_task]() { (*packaged_task)(); }, job_priority::normal);

    return future;
}

template <typename F>
void thread_adapter::submit_fire_and_forget(F&& task) {
    submit_job_internal(std::forward<F>(task), job_priority::low);
}

template <typename F>
auto thread_adapter::submit_with_priority(job_priority priority, F&& task)
    -> std::future<std::invoke_result_t<std::decay_t<F>>> {
    using return_type = std::invoke_result_t<std::decay_t<F>>;

    auto packaged_task =
        std::make_shared<std::packaged_task<return_type()>>(std::forward<F>(task));
    auto future = packaged_task->get_future();

    submit_job_internal([packaged_task]() { (*packaged_task)(); }, priority);

    return future;
}

}  // namespace pacs::integration
