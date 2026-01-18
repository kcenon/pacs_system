/**
 * @file executor_adapter.cpp
 * @brief Implementation of IExecutor adapters for pacs_system
 *
 * @see Issue #487 - Integrate IExecutor interface for workflow pipeline execution
 */

#include <pacs/integration/executor_adapter.hpp>
#include <pacs/integration/thread_pool_interface.hpp>
#include <pacs/integration/logger_adapter.hpp>

#include <kcenon/thread/core/thread_pool.h>
#include <kcenon/thread/core/thread_worker.h>
#include <kcenon/thread/interfaces/thread_context.h>

#include <algorithm>
#include <stdexcept>

namespace pacs::integration {

// =============================================================================
// thread_pool_executor_adapter Implementation
// =============================================================================

thread_pool_executor_adapter::thread_pool_executor_adapter(
    std::shared_ptr<kcenon::thread::thread_pool> pool)
    : pool_(std::move(pool)) {
    if (!pool_) {
        throw std::invalid_argument("Thread pool cannot be null");
    }
}

thread_pool_executor_adapter::thread_pool_executor_adapter(std::size_t worker_count) {
    kcenon::thread::thread_context context;
    pool_ = std::make_shared<kcenon::thread::thread_pool>("executor_pool", context);

    // Create and enqueue workers
    std::vector<std::unique_ptr<kcenon::thread::thread_worker>> workers;
    workers.reserve(worker_count);

    for (std::size_t i = 0; i < worker_count; ++i) {
        workers.push_back(std::make_unique<kcenon::thread::thread_worker>(false, context));
    }

    auto enqueue_result = pool_->enqueue_batch(std::move(workers));
    if (enqueue_result.is_err()) {
        throw std::runtime_error("Failed to enqueue workers to thread pool");
    }

    auto start_result = pool_->start();
    if (start_result.is_err()) {
        throw std::runtime_error("Failed to start thread pool");
    }
}

thread_pool_executor_adapter::~thread_pool_executor_adapter() {
    shutdown(true);
}

kcenon::common::Result<std::future<void>> thread_pool_executor_adapter::execute(
    std::unique_ptr<kcenon::common::interfaces::IJob>&& job) {

    if (!running_.load()) {
        return kcenon::common::Result<std::future<void>>(
            kcenon::common::error_info{-1, "Executor is not running", "executor"});
    }

    if (!job) {
        return kcenon::common::Result<std::future<void>>(
            kcenon::common::error_info{-2, "Job cannot be null", "executor"});
    }

    auto promise = std::make_shared<std::promise<void>>();
    auto future = promise->get_future();

    // Capture job by shared_ptr for thread safety
    auto shared_job = std::shared_ptr<kcenon::common::interfaces::IJob>(std::move(job));

    ++pending_count_;

    try {
        (void)pool_->submit(
            [this, shared_job, promise]() mutable {
                try {
                    auto result = shared_job->execute();
                    if (result.is_ok()) {
                        promise->set_value();
                    } else {
                        promise->set_exception(std::make_exception_ptr(
                            std::runtime_error(result.error().message)));
                    }
                } catch (...) {
                    promise->set_exception(std::current_exception());
                }
                --pending_count_;
            });
    } catch (const std::exception&) {
        --pending_count_;
        return kcenon::common::Result<std::future<void>>(
            kcenon::common::error_info{-3, "Failed to submit task to thread pool", "executor"});
    }

    return kcenon::common::Result<std::future<void>>::ok(std::move(future));
}

kcenon::common::Result<std::future<void>> thread_pool_executor_adapter::execute_delayed(
    std::unique_ptr<kcenon::common::interfaces::IJob>&& job,
    std::chrono::milliseconds delay) {

    if (!running_.load()) {
        return kcenon::common::Result<std::future<void>>(
            kcenon::common::error_info{-1, "Executor is not running", "executor"});
    }

    if (!job) {
        return kcenon::common::Result<std::future<void>>(
            kcenon::common::error_info{-2, "Job cannot be null", "executor"});
    }

    auto promise = std::make_shared<std::promise<void>>();
    auto future = promise->get_future();

    // Capture job and schedule for later execution
    auto shared_job = std::shared_ptr<kcenon::common::interfaces::IJob>(std::move(job));
    auto execute_at = std::chrono::steady_clock::now() + delay;

    ++pending_count_;

    {
        std::lock_guard<std::mutex> lock(delay_mutex_);
        delayed_tasks_.push({execute_at,
            [this, shared_job, promise]() mutable {
                try {
                    auto result = shared_job->execute();
                    if (result.is_ok()) {
                        promise->set_value();
                    } else {
                        promise->set_exception(std::make_exception_ptr(
                            std::runtime_error(result.error().message)));
                    }
                } catch (...) {
                    promise->set_exception(std::current_exception());
                }
                --pending_count_;
            }});
    }

    // Start delay thread if not already running
    if (!delay_thread_.joinable()) {
        delay_thread_ = std::thread([this]() {
            while (!shutdown_requested_.load()) {
                std::function<void()> task;

                {
                    std::unique_lock<std::mutex> lock(delay_mutex_);

                    if (delayed_tasks_.empty()) {
                        delay_cv_.wait_for(lock, std::chrono::milliseconds(100));
                        continue;
                    }

                    auto now = std::chrono::steady_clock::now();
                    const auto& top_task = delayed_tasks_.top();

                    if (top_task.execute_at > now) {
                        delay_cv_.wait_until(lock, top_task.execute_at);
                        continue;
                    }

                    task = top_task.task;
                    delayed_tasks_.pop();
                }

                if (task && pool_ && pool_->is_running()) {
                    try {
                        (void)pool_->submit(std::move(task));
                    } catch (...) {
                        // Ignore submit failures in delayed task scheduler
                    }
                }
            }
        });
    }

    delay_cv_.notify_one();

    return kcenon::common::Result<std::future<void>>::ok(std::move(future));
}

std::size_t thread_pool_executor_adapter::worker_count() const {
    return pool_ ? pool_->get_active_worker_count() : 0;
}

bool thread_pool_executor_adapter::is_running() const {
    return running_.load() && pool_ && pool_->is_running();
}

std::size_t thread_pool_executor_adapter::pending_tasks() const {
    std::size_t pool_pending = pool_ ? pool_->get_pending_task_count() : 0;
    return pool_pending + pending_count_.load();
}

void thread_pool_executor_adapter::shutdown(bool wait_for_completion) {
    running_.store(false);
    shutdown_requested_.store(true);

    delay_cv_.notify_all();

    if (delay_thread_.joinable()) {
        delay_thread_.join();
    }

    if (pool_) {
        pool_->stop(!wait_for_completion);
    }
}

std::shared_ptr<kcenon::thread::thread_pool>
thread_pool_executor_adapter::get_underlying_pool() const {
    return pool_;
}

// =============================================================================
// Factory Function
// =============================================================================

std::shared_ptr<kcenon::common::interfaces::IExecutor>
make_executor(std::shared_ptr<thread_pool_interface> pool_interface) {
    if (!pool_interface) {
        throw std::invalid_argument("Pool interface cannot be null");
    }

    // Create an executor that uses the pool interface
    // This requires creating a custom adapter that works with the interface

    class interface_executor_adapter : public kcenon::common::interfaces::IExecutor {
    public:
        explicit interface_executor_adapter(std::shared_ptr<thread_pool_interface> pool)
            : pool_(std::move(pool)) {}

        kcenon::common::Result<std::future<void>> execute(
            std::unique_ptr<kcenon::common::interfaces::IJob>&& job) override {

            if (!pool_ || !pool_->is_running()) {
                return kcenon::common::Result<std::future<void>>(
                    kcenon::common::error_info{-1, "Pool is not running", "executor"});
            }

            auto shared_job = std::shared_ptr<kcenon::common::interfaces::IJob>(std::move(job));

            auto future = pool_->submit([shared_job]() {
                auto result = shared_job->execute();
                if (result.is_err()) {
                    throw std::runtime_error(result.error().message);
                }
            });

            return kcenon::common::Result<std::future<void>>::ok(std::move(future));
        }

        kcenon::common::Result<std::future<void>> execute_delayed(
            std::unique_ptr<kcenon::common::interfaces::IJob>&& /*job*/,
            std::chrono::milliseconds /*delay*/) override {
            // Not supported through interface
            return kcenon::common::Result<std::future<void>>(
                kcenon::common::error_info{-4, "Delayed execution not supported through pool interface", "executor"});
        }

        std::size_t worker_count() const override {
            return pool_ ? pool_->get_active_worker_count() : 0;
        }

        bool is_running() const override {
            return pool_ && pool_->is_running();
        }

        std::size_t pending_tasks() const override {
            return pool_ ? pool_->get_pending_task_count() : 0;
        }

        void shutdown(bool wait_for_completion) override {
            if (pool_) {
                pool_->shutdown(wait_for_completion);
            }
        }

    private:
        std::shared_ptr<thread_pool_interface> pool_;
    };

    return std::make_shared<interface_executor_adapter>(std::move(pool_interface));
}

}  // namespace pacs::integration
