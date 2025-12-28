/**
 * @file mock_thread_pool.hpp
 * @brief Mock implementation of thread_pool_interface for testing
 *
 * This file provides a mock thread pool implementation that can be used
 * in unit tests to isolate components from real thread pool behavior.
 *
 * @see Issue #405 - Replace Singleton Pattern with Dependency Injection
 */

#pragma once

#include <pacs/integration/thread_pool_interface.hpp>
#include <pacs/integration/thread_adapter.hpp>  // for job_priority

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace pacs::integration::testing {

/**
 * @brief Record of a submitted task for verification
 */
struct submitted_task_record {
    job_priority priority;
    std::chrono::steady_clock::time_point submit_time;
};

/**
 * @brief Mock implementation of thread_pool_interface
 *
 * This mock provides several modes of operation:
 * - **Synchronous mode**: Tasks are executed immediately on the calling thread
 * - **Recording mode**: Tasks are recorded but not executed
 * - **Failure mode**: All submissions fail
 *
 * Thread Safety: All public methods are thread-safe.
 *
 * @example
 * @code
 * // Create mock and inject into service
 * auto mock = std::make_shared<mock_thread_pool>();
 * my_service service(mock);
 *
 * // Execute method that submits tasks
 * service.process_data();
 *
 * // Verify tasks were submitted
 * REQUIRE(mock->get_submitted_task_count() == 1);
 *
 * // Test error handling
 * mock->set_should_fail(true);
 * REQUIRE_THROWS(service.process_data());
 * @endcode
 */
class mock_thread_pool final : public thread_pool_interface {
public:
    /**
     * @brief Execution mode for the mock
     */
    enum class execution_mode {
        synchronous,  ///< Execute tasks immediately on calling thread
        recording,    ///< Record tasks but don't execute them
        async         ///< Execute tasks on a background thread
    };

    // =========================================================================
    // Constructors
    // =========================================================================

    /**
     * @brief Construct mock with default settings
     *
     * Default: synchronous mode, not running, no failures
     */
    mock_thread_pool() = default;

    /**
     * @brief Destructor
     */
    ~mock_thread_pool() override {
        shutdown(true);
    }

    // Non-copyable, non-movable
    mock_thread_pool(const mock_thread_pool&) = delete;
    mock_thread_pool& operator=(const mock_thread_pool&) = delete;
    mock_thread_pool(mock_thread_pool&&) = delete;
    mock_thread_pool& operator=(mock_thread_pool&&) = delete;

    // =========================================================================
    // thread_pool_interface Implementation
    // =========================================================================

    [[nodiscard]] auto start() -> bool override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (should_fail_start_) {
            return false;
        }
        running_ = true;
        return true;
    }

    [[nodiscard]] auto is_running() const noexcept -> bool override {
        return running_;
    }

    void shutdown(bool /*wait_for_completion*/) override {
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;

        // Process remaining tasks if in async mode
        if (mode_ == execution_mode::async && worker_thread_.joinable()) {
            shutdown_requested_ = true;
            cv_.notify_all();
        }

        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }

    [[nodiscard]] auto submit(std::function<void()> task)
        -> std::future<void> override {
        return submit_with_priority(job_priority::normal, std::move(task));
    }

    [[nodiscard]] auto submit_with_priority(
        job_priority priority,
        std::function<void()> task) -> std::future<void> override {
        auto promise = std::make_shared<std::promise<void>>();
        auto future = promise->get_future();

        {
            std::lock_guard<std::mutex> lock(mutex_);

            if (should_fail_submit_) {
                promise->set_exception(
                    std::make_exception_ptr(
                        std::runtime_error("Mock configured to fail")));
                return future;
            }

            // Record the submission
            submitted_tasks_.push_back({
                priority,
                std::chrono::steady_clock::now()
            });
            ++submitted_task_count_;

            // Execute based on mode
            switch (mode_) {
                case execution_mode::synchronous:
                    try {
                        task();
                        promise->set_value();
                    } catch (...) {
                        promise->set_exception(std::current_exception());
                    }
                    break;

                case execution_mode::recording:
                    // Don't execute, just set value
                    promise->set_value();
                    break;

                case execution_mode::async:
                    pending_tasks_.push([task = std::move(task), promise]() mutable {
                        try {
                            task();
                            promise->set_value();
                        } catch (...) {
                            promise->set_exception(std::current_exception());
                        }
                    });
                    cv_.notify_one();
                    break;
            }
        }

        return future;
    }

    void submit_fire_and_forget(std::function<void()> task) override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (should_fail_submit_) {
            return;  // Silently fail for fire-and-forget
        }

        submitted_tasks_.push_back({
            job_priority::low,
            std::chrono::steady_clock::now()
        });
        ++submitted_task_count_;

        if (mode_ == execution_mode::synchronous) {
            try {
                task();
            } catch (...) {
                // Ignore exceptions in fire-and-forget
            }
        }
    }

    [[nodiscard]] auto get_thread_count() const -> std::size_t override {
        return thread_count_;
    }

    [[nodiscard]] auto get_pending_task_count() const -> std::size_t override {
        std::lock_guard<std::mutex> lock(mutex_);
        return pending_tasks_.size();
    }

    [[nodiscard]] auto get_idle_worker_count() const -> std::size_t override {
        return idle_worker_count_;
    }

    // =========================================================================
    // Mock Configuration
    // =========================================================================

    /**
     * @brief Set the execution mode
     *
     * @param mode The execution mode to use
     */
    void set_mode(execution_mode mode) {
        std::lock_guard<std::mutex> lock(mutex_);
        mode_ = mode;

        if (mode == execution_mode::async && !worker_thread_.joinable()) {
            shutdown_requested_ = false;
            worker_thread_ = std::thread([this] { async_worker_loop(); });
        }
    }

    /**
     * @brief Configure whether task submissions should fail
     *
     * @param should_fail If true, all submissions will fail
     */
    void set_should_fail(bool should_fail) {
        std::lock_guard<std::mutex> lock(mutex_);
        should_fail_submit_ = should_fail;
    }

    /**
     * @brief Configure whether start() should fail
     *
     * @param should_fail If true, start() will return false
     */
    void set_should_fail_start(bool should_fail) {
        std::lock_guard<std::mutex> lock(mutex_);
        should_fail_start_ = should_fail;
    }

    /**
     * @brief Set the reported thread count
     *
     * @param count The count to report from get_thread_count()
     */
    void set_thread_count(std::size_t count) {
        thread_count_ = count;
    }

    /**
     * @brief Set the reported idle worker count
     *
     * @param count The count to report from get_idle_worker_count()
     */
    void set_idle_worker_count(std::size_t count) {
        idle_worker_count_ = count;
    }

    // =========================================================================
    // Verification Methods
    // =========================================================================

    /**
     * @brief Get the total number of submitted tasks
     *
     * @return Number of tasks submitted since creation or last reset
     */
    [[nodiscard]] auto get_submitted_task_count() const -> std::size_t {
        return submitted_task_count_;
    }

    /**
     * @brief Get records of all submitted tasks
     *
     * @return Vector of task submission records
     */
    [[nodiscard]] auto get_submitted_tasks() const
        -> std::vector<submitted_task_record> {
        std::lock_guard<std::mutex> lock(mutex_);
        return submitted_tasks_;
    }

    /**
     * @brief Reset all recorded state
     *
     * Clears submitted task records and resets counters.
     */
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        submitted_tasks_.clear();
        submitted_task_count_ = 0;
        should_fail_submit_ = false;
        should_fail_start_ = false;
    }

    /**
     * @brief Wait for all async tasks to complete
     *
     * Only applicable in async mode.
     *
     * @param timeout Maximum time to wait
     * @return true if all tasks completed, false on timeout
     */
    [[nodiscard]] auto wait_for_completion(
        std::chrono::milliseconds timeout = std::chrono::seconds{5}) -> bool {
        auto start = std::chrono::steady_clock::now();
        while (true) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (pending_tasks_.empty()) {
                    return true;
                }
            }

            if (std::chrono::steady_clock::now() - start > timeout) {
                return false;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds{10});
        }
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;

    // State
    std::atomic<bool> running_{false};
    execution_mode mode_{execution_mode::synchronous};

    // Failure simulation
    bool should_fail_submit_{false};
    bool should_fail_start_{false};

    // Statistics
    std::atomic<std::size_t> thread_count_{4};
    std::atomic<std::size_t> idle_worker_count_{2};
    std::atomic<std::size_t> submitted_task_count_{0};

    // Recording
    std::vector<submitted_task_record> submitted_tasks_;

    // Async execution
    std::queue<std::function<void()>> pending_tasks_;
    std::thread worker_thread_;
    std::atomic<bool> shutdown_requested_{false};

    void async_worker_loop() {
        while (!shutdown_requested_) {
            std::function<void()> task;

            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this] {
                    return shutdown_requested_ || !pending_tasks_.empty();
                });

                if (shutdown_requested_ && pending_tasks_.empty()) {
                    break;
                }

                if (!pending_tasks_.empty()) {
                    task = std::move(pending_tasks_.front());
                    pending_tasks_.pop();
                }
            }

            if (task) {
                task();
            }
        }
    }
};

}  // namespace pacs::integration::testing
