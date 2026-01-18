/**
 * @file thread_pool_adapter.cpp
 * @brief Implementation of thread_pool_adapter
 *
 * @see Issue #405 - Replace Singleton Pattern with Dependency Injection
 */

#include <pacs/integration/thread_pool_adapter.hpp>
#include <pacs/integration/thread_adapter.hpp>  // for thread_pool_config, job_priority

#include <kcenon/thread/core/thread_pool.h>
#include <kcenon/thread/core/thread_worker.h>
#include <kcenon/thread/interfaces/thread_context.h>

#include <stdexcept>
#include <vector>

namespace pacs::integration {

// =============================================================================
// Constructors & Destructor
// =============================================================================

thread_pool_adapter::thread_pool_adapter(const thread_pool_config& config)
    : config_(config) {
    // Validate configuration
    if (config_.min_threads == 0) {
        config_.min_threads = 1;
    }
    if (config_.max_threads < config_.min_threads) {
        config_.max_threads = config_.min_threads;
    }

    // Create the pool
    kcenon::thread::thread_context context;
    pool_ = std::make_shared<kcenon::thread::thread_pool>(config_.pool_name, context);
}

thread_pool_adapter::thread_pool_adapter(
    std::shared_ptr<kcenon::thread::thread_pool> pool)
    : pool_(std::move(pool)), initialized_(true) {
    if (!pool_) {
        throw std::invalid_argument("Thread pool cannot be null");
    }
}

thread_pool_adapter::~thread_pool_adapter() {
    shutdown(true);
}

// =============================================================================
// Lifecycle Management
// =============================================================================

auto thread_pool_adapter::start() -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_ && pool_ && pool_->is_running()) {
        return true;  // Already running
    }

    // Create pool if not exists
    if (!pool_) {
        kcenon::thread::thread_context context;
        pool_ = std::make_shared<kcenon::thread::thread_pool>(config_.pool_name, context);
    }

    // Create worker threads
    std::vector<std::unique_ptr<kcenon::thread::thread_worker>> workers;
    workers.reserve(config_.min_threads);

    kcenon::thread::thread_context context;
    for (std::size_t i = 0; i < config_.min_threads; ++i) {
        workers.push_back(std::make_unique<kcenon::thread::thread_worker>(false, context));
    }

    // Enqueue workers
    auto enqueue_result = pool_->enqueue_batch(std::move(workers));
    if (enqueue_result.is_err()) {
        return false;
    }

    // Start the pool
    auto start_result = pool_->start();
    if (start_result.is_err()) {
        return false;
    }

    initialized_ = true;
    return true;
}

auto thread_pool_adapter::is_running() const noexcept -> bool {
    std::lock_guard<std::mutex> lock(mutex_);
    return pool_ && pool_->is_running();
}

void thread_pool_adapter::shutdown(bool wait_for_completion) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (pool_) {
        pool_->stop(!wait_for_completion);
    }

    initialized_ = false;
}

// =============================================================================
// Task Submission
// =============================================================================

auto thread_pool_adapter::submit(std::function<void()> task)
    -> std::future<void> {
    auto promise = std::make_shared<std::promise<void>>();
    auto future = promise->get_future();

    submit_internal(
        [task = std::move(task), promise]() mutable {
            try {
                task();
                promise->set_value();
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        },
        job_priority::normal);

    return future;
}

auto thread_pool_adapter::submit_with_priority(
    job_priority priority,
    std::function<void()> task) -> std::future<void> {
    auto promise = std::make_shared<std::promise<void>>();
    auto future = promise->get_future();

    submit_internal(
        [task = std::move(task), promise]() mutable {
            try {
                task();
                promise->set_value();
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        },
        priority);

    return future;
}

void thread_pool_adapter::submit_fire_and_forget(std::function<void()> task) {
    submit_internal(std::move(task), job_priority::low);
}

void thread_pool_adapter::submit_internal(
    std::function<void()> task,
    job_priority /*priority*/) {
    // Ensure pool is started
    if (!is_running()) {
        if (!start()) {
            throw std::runtime_error("Failed to start thread pool");
        }
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (pool_) {
        // Note: Priority is not currently used as the base thread_pool
        // doesn't support it. For future implementation, consider using
        // typed_thread_pool or a priority queue wrapper.
        try {
            (void)pool_->submit(std::move(task));
        } catch (const std::exception&) {
            throw std::runtime_error("Failed to submit task to thread pool");
        }
    }
}

// =============================================================================
// Statistics
// =============================================================================

auto thread_pool_adapter::get_thread_count() const -> std::size_t {
    std::lock_guard<std::mutex> lock(mutex_);
    // Return configured thread count if pool is initialized
    // This reflects the expected worker count rather than transient active state
    return initialized_ ? config_.min_threads : 0;
}

auto thread_pool_adapter::get_pending_task_count() const -> std::size_t {
    std::lock_guard<std::mutex> lock(mutex_);
    return pool_ ? pool_->get_pending_task_count() : 0;
}

auto thread_pool_adapter::get_idle_worker_count() const -> std::size_t {
    std::lock_guard<std::mutex> lock(mutex_);
    return pool_ ? pool_->get_idle_worker_count() : 0;
}

// =============================================================================
// Adapter-specific Methods
// =============================================================================

auto thread_pool_adapter::get_underlying_pool() const
    -> std::shared_ptr<kcenon::thread::thread_pool> {
    std::lock_guard<std::mutex> lock(mutex_);
    return pool_;
}

auto thread_pool_adapter::get_config() const noexcept -> const thread_pool_config& {
    return config_;
}

}  // namespace pacs::integration
