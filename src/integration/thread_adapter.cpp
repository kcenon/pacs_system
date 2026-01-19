/**
 * @file thread_adapter.cpp
 * @brief Implementation of thread_adapter for thread_system integration
 */

#include <pacs/integration/thread_adapter.hpp>

#include <kcenon/thread/core/thread_pool.h>
#include <kcenon/thread/core/thread_worker.h>
#include <kcenon/thread/interfaces/thread_context.h>

#include <stdexcept>
#include <vector>

namespace pacs::integration {

// ─────────────────────────────────────────────────────
// Static Member Definitions
// ─────────────────────────────────────────────────────

std::shared_ptr<kcenon::thread::thread_pool> thread_adapter::pool_ = nullptr;
thread_pool_config thread_adapter::config_;
std::mutex thread_adapter::mutex_;
bool thread_adapter::initialized_ = false;

// ─────────────────────────────────────────────────────
// Thread Pool Management
// ─────────────────────────────────────────────────────

auto thread_adapter::get_pool() -> std::shared_ptr<kcenon::thread::thread_pool> {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!pool_) {
        kcenon::thread::thread_context context;
        pool_ = std::make_shared<kcenon::thread::thread_pool>(config_.pool_name, context);
    }

    return pool_;
}

void thread_adapter::configure(const thread_pool_config& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;

    // Apply validation
    if (config_.min_threads == 0) {
        config_.min_threads = 1;
    }
    if (config_.max_threads < config_.min_threads) {
        config_.max_threads = config_.min_threads;
    }
}

auto thread_adapter::get_config() noexcept -> const thread_pool_config& {
    return config_;
}

auto thread_adapter::start() -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_ && pool_ && pool_->is_running()) {
        return true;  // Already running
    }

    // Create pool if not exists with proper context
    kcenon::thread::thread_context context;
    if (!pool_) {
        pool_ = std::make_shared<kcenon::thread::thread_pool>(config_.pool_name, context);
    }

    // Create worker threads and add them in batch
    // Note: thread_pool::enqueue() automatically sets job_queue, context, and metrics
    std::vector<std::unique_ptr<kcenon::thread::thread_worker>> workers;
    workers.reserve(config_.min_threads);

    for (std::size_t i = 0; i < config_.min_threads; ++i) {
        // Pass false for use_time_tag and context for consistency
        workers.push_back(std::make_unique<kcenon::thread::thread_worker>(false, context));
    }

    // Use batch enqueue for efficiency
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

auto thread_adapter::is_running() noexcept -> bool {
    std::lock_guard<std::mutex> lock(mutex_);
    return pool_ && pool_->is_running();
}

void thread_adapter::shutdown(bool wait_for_completion) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (pool_) {
        pool_->stop(!wait_for_completion);
        pool_.reset();
    }

    initialized_ = false;
}

// ─────────────────────────────────────────────────────
// Job Submission Internal
// ─────────────────────────────────────────────────────

void thread_adapter::submit_job_internal(std::function<void()> task,
                                         job_priority /*priority*/) {
    // Ensure pool is initialized (but don't call is_running to avoid deadlock)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!pool_) {
            kcenon::thread::thread_context context;
            pool_ = std::make_shared<kcenon::thread::thread_pool>(config_.pool_name, context);
        }
    }

    // Try to start if not running (start() handles its own locking)
    if (!is_running()) {
        if (!start()) {
            throw std::runtime_error("Failed to start thread pool");
        }
    }

    // Use the unified submit API (fire-and-forget for void tasks)
    // Note: Priority is not currently used as the base thread_pool doesn't support it.
    // For future implementation, consider using typed_thread_pool or a priority queue wrapper.
    std::lock_guard<std::mutex> lock(mutex_);
    if (pool_) {
        try {
            (void)pool_->submit(std::move(task));
        } catch (const std::exception&) {
            throw std::runtime_error("Failed to submit task to thread pool");
        }
    }
}

// ─────────────────────────────────────────────────────
// Statistics
// ─────────────────────────────────────────────────────

auto thread_adapter::get_thread_count() -> std::size_t {
    std::lock_guard<std::mutex> lock(mutex_);
    if (pool_) {
        return pool_->get_active_worker_count();
    }
    return 0;
}

auto thread_adapter::get_pending_job_count() -> std::size_t {
    std::lock_guard<std::mutex> lock(mutex_);
    if (pool_) {
        return pool_->get_pending_task_count();
    }
    return 0;
}

auto thread_adapter::get_idle_worker_count() -> std::size_t {
    std::lock_guard<std::mutex> lock(mutex_);
    if (pool_) {
        return pool_->get_idle_worker_count();
    }
    return 0;
}

}  // namespace pacs::integration
