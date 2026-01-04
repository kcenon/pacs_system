/**
 * @file executor_adapter.hpp
 * @brief Adapter for integrating common_system's IExecutor interface
 *
 * This file provides adapters that bridge common_system's IExecutor interface
 * with pacs_system's thread pool infrastructure. It enables workflow modules
 * to use the standardized IExecutor interface while leveraging existing
 * thread pool implementations.
 *
 * @see Issue #487 - Integrate IExecutor interface for workflow pipeline execution
 * @see common_system's executor_interface.h
 */

#pragma once

#include <kcenon/common/interfaces/executor_interface.h>
#include <kcenon/common/patterns/result.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

namespace kcenon::thread {
class thread_pool;
}  // namespace kcenon::thread

namespace pacs::integration {

// Forward declaration
class thread_pool_interface;

/**
 * @class lambda_job
 * @brief IJob implementation that wraps a callable
 *
 * This class adapts a std::function to the IJob interface, allowing
 * lambda expressions and other callables to be used with IExecutor.
 *
 * @example
 * @code
 * auto job = std::make_unique<lambda_job>(
 *     []() -> kcenon::common::VoidResult {
 *         // Do work...
 *         return kcenon::common::VoidResult::ok();
 *     },
 *     "my_job",
 *     5  // priority
 * );
 * executor->execute(std::move(job));
 * @endcode
 */
class lambda_job : public kcenon::common::interfaces::IJob {
public:
    using job_function = std::function<kcenon::common::VoidResult()>;

    /**
     * @brief Construct a lambda job
     *
     * @param func The function to execute
     * @param name Job name for logging (default: "lambda_job")
     * @param priority Job priority (default: 0)
     */
    explicit lambda_job(
        job_function func,
        std::string name = "lambda_job",
        int priority = 0)
        : func_(std::move(func))
        , name_(std::move(name))
        , priority_(priority) {}

    /**
     * @brief Construct from void-returning callable
     *
     * @param func Void-returning function
     * @param name Job name for logging
     * @param priority Job priority
     */
    template <typename F>
        requires std::is_void_v<std::invoke_result_t<F>>
    explicit lambda_job(F&& func, std::string name = "lambda_job", int priority = 0)
        : func_([f = std::forward<F>(func)]() -> kcenon::common::VoidResult {
              f();
              return kcenon::common::VoidResult(std::monostate{});
          })
        , name_(std::move(name))
        , priority_(priority) {}

    kcenon::common::VoidResult execute() override {
        if (!func_) {
            return kcenon::common::VoidResult(
                kcenon::common::error_info{-1, "No function provided", "executor"});
        }
        return func_();
    }

    [[nodiscard]] std::string get_name() const override { return name_; }
    [[nodiscard]] int get_priority() const override { return priority_; }

private:
    job_function func_;
    std::string name_;
    int priority_;
};

/**
 * @class thread_pool_executor_adapter
 * @brief IExecutor implementation using kcenon::thread::thread_pool
 *
 * This class adapts kcenon::thread::thread_pool to the IExecutor interface,
 * enabling standardized task execution across the pacs_system workflow modules.
 *
 * Thread Safety: All public methods are thread-safe.
 *
 * @example
 * @code
 * // Create executor with thread pool
 * auto pool = std::make_shared<kcenon::thread::thread_pool>(4);
 * auto executor = std::make_shared<thread_pool_executor_adapter>(pool);
 *
 * // Submit job
 * auto job = std::make_unique<lambda_job>([]() {
 *     // Work...
 *     return kcenon::common::VoidResult::ok();
 * });
 * auto result = executor->execute(std::move(job));
 * if (result.is_ok()) {
 *     result.value().wait();
 * }
 * @endcode
 */
class thread_pool_executor_adapter : public kcenon::common::interfaces::IExecutor {
public:
    /**
     * @brief Construct adapter with thread pool
     *
     * @param pool Existing thread pool to use
     */
    explicit thread_pool_executor_adapter(
        std::shared_ptr<kcenon::thread::thread_pool> pool);

    /**
     * @brief Construct adapter with worker count
     *
     * Creates a new thread pool with the specified number of workers.
     *
     * @param worker_count Number of worker threads
     */
    explicit thread_pool_executor_adapter(std::size_t worker_count);

    /**
     * @brief Destructor - ensures graceful shutdown
     */
    ~thread_pool_executor_adapter() override;

    // Non-copyable, non-movable
    thread_pool_executor_adapter(const thread_pool_executor_adapter&) = delete;
    thread_pool_executor_adapter& operator=(const thread_pool_executor_adapter&) = delete;
    thread_pool_executor_adapter(thread_pool_executor_adapter&&) = delete;
    thread_pool_executor_adapter& operator=(thread_pool_executor_adapter&&) = delete;

    // =========================================================================
    // IExecutor Implementation
    // =========================================================================

    /**
     * @brief Execute a job
     *
     * Submits the job to the thread pool for asynchronous execution.
     *
     * @param job The job to execute
     * @return Result containing future or error
     */
    [[nodiscard]] kcenon::common::Result<std::future<void>> execute(
        std::unique_ptr<kcenon::common::interfaces::IJob>&& job) override;

    /**
     * @brief Execute a job with delay
     *
     * @param job The job to execute
     * @param delay The delay before execution
     * @return Result containing future or error
     */
    [[nodiscard]] kcenon::common::Result<std::future<void>> execute_delayed(
        std::unique_ptr<kcenon::common::interfaces::IJob>&& job,
        std::chrono::milliseconds delay) override;

    /**
     * @brief Get the number of worker threads
     */
    [[nodiscard]] std::size_t worker_count() const override;

    /**
     * @brief Check if the executor is running
     */
    [[nodiscard]] bool is_running() const override;

    /**
     * @brief Get the number of pending tasks
     */
    [[nodiscard]] std::size_t pending_tasks() const override;

    /**
     * @brief Shutdown the executor
     *
     * @param wait_for_completion Wait for all pending tasks to complete
     */
    void shutdown(bool wait_for_completion = true) override;

    // =========================================================================
    // Convenience Methods
    // =========================================================================

    /**
     * @brief Submit a void-returning callable directly
     *
     * This is a convenience method that wraps the callable in a lambda_job.
     *
     * @tparam F Callable type
     * @param func The function to execute
     * @param name Job name for logging
     * @return Result containing future or error
     */
    template <typename F>
        requires std::invocable<F>
    [[nodiscard]] auto submit(F&& func, std::string name = "submitted_job")
        -> kcenon::common::Result<std::future<void>> {
        auto job = std::make_unique<lambda_job>(std::forward<F>(func), std::move(name));
        return execute(std::move(job));
    }

    /**
     * @brief Get the underlying thread pool
     */
    [[nodiscard]] auto get_underlying_pool() const
        -> std::shared_ptr<kcenon::thread::thread_pool>;

private:
    std::shared_ptr<kcenon::thread::thread_pool> pool_;
    std::atomic<bool> running_{true};
    std::atomic<std::size_t> pending_count_{0};

    // For delayed execution
    std::thread delay_thread_;
    std::mutex delay_mutex_;
    std::condition_variable delay_cv_;
    std::atomic<bool> shutdown_requested_{false};

    struct delayed_task {
        std::chrono::steady_clock::time_point execute_at;
        std::function<void()> task;

        bool operator>(const delayed_task& other) const {
            return execute_at > other.execute_at;
        }
    };
    std::priority_queue<delayed_task, std::vector<delayed_task>,
                        std::greater<delayed_task>> delayed_tasks_;
};

/**
 * @brief Create an IExecutor from a thread_pool_interface
 *
 * Factory function that creates an appropriate executor adapter
 * from a thread_pool_interface implementation.
 *
 * @param pool_interface The thread pool interface to wrap
 * @return Shared pointer to IExecutor implementation
 */
[[nodiscard]] std::shared_ptr<kcenon::common::interfaces::IExecutor>
make_executor(std::shared_ptr<thread_pool_interface> pool_interface);

}  // namespace pacs::integration
