/**
 * @file thread_pool_adapter.hpp
 * @brief Concrete implementation of thread_pool_interface using kcenon::thread
 *
 * This file provides the thread_pool_adapter class which implements the
 * thread_pool_interface using kcenon::thread::thread_pool as the backend.
 * It enables dependency injection for thread pool operations.
 *
 * @see Issue #405 - Replace Singleton Pattern with Dependency Injection
 * @see IR-3 (thread_system Integration)
 */

#pragma once

#include <pacs/integration/thread_pool_interface.hpp>
#include <pacs/integration/thread_adapter.hpp>  // for thread_pool_config, job_priority

#include <chrono>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace kcenon::thread {
class thread_pool;
}  // namespace kcenon::thread

namespace pacs::integration {

/**
 * @class thread_pool_adapter
 * @brief Concrete implementation of thread_pool_interface
 *
 * This class adapts kcenon::thread::thread_pool to the thread_pool_interface,
 * enabling dependency injection for thread pool operations in PACS components.
 *
 * Key features:
 * - **Lock-free Job Queue**: Uses thread_system's lock-free queue
 * - **Priority Scheduling**: Critical operations get priority
 * - **Automatic Scaling**: Thread pool grows/shrinks based on workload
 * - **Injectable**: Can be passed to components via constructor
 *
 * Thread Safety: All public methods are thread-safe.
 *
 * @example
 * @code
 * // Create adapter with configuration
 * thread_pool_config config;
 * config.min_threads = 4;
 * config.max_threads = 16;
 *
 * auto pool = std::make_shared<thread_pool_adapter>(config);
 * pool->start();
 *
 * // Use in a service
 * my_service service(pool);
 *
 * // Submit tasks directly
 * auto future = pool->submit([]() { return 42; });
 * @endcode
 */
class thread_pool_adapter final : public thread_pool_interface {
public:
    // =========================================================================
    // Constructors
    // =========================================================================

    /**
     * @brief Construct adapter with configuration
     *
     * Creates a new thread pool with the specified configuration.
     * The pool is not started until start() is called.
     *
     * @param config Configuration options for the thread pool
     */
    explicit thread_pool_adapter(const thread_pool_config& config);

    /**
     * @brief Construct adapter with an existing thread pool
     *
     * Wraps an existing thread pool instance. This is useful for
     * sharing a pool across multiple components or for testing.
     *
     * @param pool Existing thread pool to wrap (must not be null)
     */
    explicit thread_pool_adapter(std::shared_ptr<kcenon::thread::thread_pool> pool);

    /**
     * @brief Destructor
     *
     * Shuts down the thread pool if it's still running.
     */
    ~thread_pool_adapter() override;

    // =========================================================================
    // Non-copyable, Non-movable
    // =========================================================================

    thread_pool_adapter(const thread_pool_adapter&) = delete;
    thread_pool_adapter& operator=(const thread_pool_adapter&) = delete;
    thread_pool_adapter(thread_pool_adapter&&) = delete;
    thread_pool_adapter& operator=(thread_pool_adapter&&) = delete;

    // =========================================================================
    // Lifecycle Management (from thread_pool_interface)
    // =========================================================================

    [[nodiscard]] auto start() -> bool override;
    [[nodiscard]] auto is_running() const noexcept -> bool override;
    void shutdown(bool wait_for_completion = true) override;

    // =========================================================================
    // Task Submission (from thread_pool_interface)
    // =========================================================================

    [[nodiscard]] auto submit(std::function<void()> task)
        -> std::future<void> override;

    [[nodiscard]] auto submit_with_priority(
        job_priority priority,
        std::function<void()> task) -> std::future<void> override;

    void submit_fire_and_forget(std::function<void()> task) override;

    // =========================================================================
    // Statistics (from thread_pool_interface)
    // =========================================================================

    [[nodiscard]] auto get_thread_count() const -> std::size_t override;
    [[nodiscard]] auto get_pending_task_count() const -> std::size_t override;
    [[nodiscard]] auto get_idle_worker_count() const -> std::size_t override;

    // =========================================================================
    // Adapter-specific Methods
    // =========================================================================

    /**
     * @brief Get the underlying thread pool
     *
     * Provides access to the underlying kcenon::thread::thread_pool for
     * advanced use cases. Use with caution.
     *
     * @return Shared pointer to the underlying thread pool
     */
    [[nodiscard]] auto get_underlying_pool() const
        -> std::shared_ptr<kcenon::thread::thread_pool>;

    /**
     * @brief Get the current configuration
     *
     * @return Current thread pool configuration
     */
    [[nodiscard]] auto get_config() const noexcept -> const thread_pool_config&;

private:
    std::shared_ptr<kcenon::thread::thread_pool> pool_;
    thread_pool_config config_;
    mutable std::mutex mutex_;
    bool initialized_{false};

    /// Internal task submission with priority
    void submit_internal(std::function<void()> task, job_priority priority);
};

}  // namespace pacs::integration
