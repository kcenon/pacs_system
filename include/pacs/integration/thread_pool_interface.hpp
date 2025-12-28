/**
 * @file thread_pool_interface.hpp
 * @brief Abstract interface for thread pool operations
 *
 * This file defines the abstract thread_pool_interface class which provides
 * a unified API for thread pool operations. This interface enables dependency
 * injection and allows mock implementations for testing.
 *
 * @see Issue #405 - Replace Singleton Pattern with Dependency Injection
 * @see IR-3 (thread_system Integration)
 */

#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <future>
#include <type_traits>

namespace pacs::integration {

// Forward declaration
enum class job_priority;

/**
 * @brief Abstract interface for thread pool operations
 *
 * Provides a unified API for submitting tasks to a thread pool and managing
 * its lifecycle. This interface enables dependency injection, allowing
 * components to receive a thread pool instance rather than depending on
 * global state.
 *
 * Benefits:
 * - **Testability**: Mock implementations can be injected for unit testing
 * - **Flexibility**: Different thread pool configurations per component
 * - **Clean Architecture**: Follows Dependency Inversion Principle (SOLID)
 *
 * Thread Safety:
 * - All methods must be thread-safe in concrete implementations
 * - Concurrent task submission is allowed
 *
 * @example
 * @code
 * // Using dependency injection
 * class my_service {
 * public:
 *     explicit my_service(std::shared_ptr<thread_pool_interface> pool)
 *         : pool_(std::move(pool)) {}
 *
 *     void process_async() {
 *         pool_->submit([this] { do_work(); });
 *     }
 *
 * private:
 *     std::shared_ptr<thread_pool_interface> pool_;
 * };
 *
 * // In production
 * auto pool = std::make_shared<thread_pool_adapter>(config);
 * auto service = my_service(pool);
 *
 * // In tests
 * auto mock_pool = std::make_shared<mock_thread_pool>();
 * auto service = my_service(mock_pool);
 * @endcode
 */
class thread_pool_interface {
public:
    /// Virtual destructor for proper polymorphic destruction
    virtual ~thread_pool_interface() = default;

    // =========================================================================
    // Lifecycle Management
    // =========================================================================

    /**
     * @brief Start the thread pool
     *
     * Initializes worker threads and begins accepting tasks.
     * Safe to call multiple times; subsequent calls are no-ops if running.
     *
     * @return true if started successfully or already running, false otherwise
     */
    [[nodiscard]] virtual auto start() -> bool = 0;

    /**
     * @brief Check if the thread pool is running
     *
     * @return true if the pool is active and accepting tasks
     */
    [[nodiscard]] virtual auto is_running() const noexcept -> bool = 0;

    /**
     * @brief Shutdown the thread pool
     *
     * Stops accepting new tasks and optionally waits for pending tasks.
     *
     * @param wait_for_completion If true, waits for all pending tasks to complete
     */
    virtual void shutdown(bool wait_for_completion = true) = 0;

    // =========================================================================
    // Task Submission
    // =========================================================================

    /**
     * @brief Submit a task for execution
     *
     * Submits a void-returning task to the thread pool for asynchronous
     * execution with normal priority.
     *
     * @param task The task to execute (must be callable with no arguments)
     * @return Future that completes when the task finishes
     *
     * @throws std::runtime_error if pool is not running
     */
    [[nodiscard]] virtual auto submit(std::function<void()> task)
        -> std::future<void> = 0;

    /**
     * @brief Submit a task with a specific priority level
     *
     * Jobs with higher priority (lower enum value) are processed first.
     *
     * @param priority The priority level for this task
     * @param task The task to execute
     * @return Future that completes when the task finishes
     *
     * @throws std::runtime_error if pool is not running
     */
    [[nodiscard]] virtual auto submit_with_priority(
        job_priority priority,
        std::function<void()> task) -> std::future<void> = 0;

    /**
     * @brief Submit a task without waiting for completion
     *
     * Fire-and-forget submission for tasks where the result is not needed.
     * Errors are logged but not propagated.
     *
     * @param task The task to execute
     */
    virtual void submit_fire_and_forget(std::function<void()> task) = 0;

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get the current number of worker threads
     *
     * @return Number of active worker threads
     */
    [[nodiscard]] virtual auto get_thread_count() const -> std::size_t = 0;

    /**
     * @brief Get the number of pending tasks in the queue
     *
     * @return Number of tasks waiting to be processed
     */
    [[nodiscard]] virtual auto get_pending_task_count() const -> std::size_t = 0;

    /**
     * @brief Get the number of idle workers
     *
     * @return Number of workers not currently processing tasks
     */
    [[nodiscard]] virtual auto get_idle_worker_count() const -> std::size_t = 0;

protected:
    /// Protected default constructor for derived classes
    thread_pool_interface() = default;

    /// Non-copyable
    thread_pool_interface(const thread_pool_interface&) = delete;
    thread_pool_interface& operator=(const thread_pool_interface&) = delete;

    /// Movable
    thread_pool_interface(thread_pool_interface&&) = default;
    thread_pool_interface& operator=(thread_pool_interface&&) = default;
};

}  // namespace pacs::integration
