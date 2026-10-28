// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file pipeline_coordinator.cpp
 * @brief Implementation of the pipeline coordinator
 *
 * @see Issue #517 - Implement typed_thread_pool-based I/O Pipeline
 */

#include <kcenon/pacs/network/pipeline/pipeline_coordinator.h>
#include <kcenon/pacs/integration/thread_pool_adapter.h>

#include <chrono>

namespace kcenon::pacs::network::pipeline {

// =============================================================================
// Construction & Destruction
// =============================================================================

pipeline_coordinator::pipeline_coordinator()
    : pipeline_coordinator(pipeline_config{}) {
}

pipeline_coordinator::pipeline_coordinator(const pipeline_config& config)
    : config_(config) {
    if (config_.enable_metrics) {
        metrics_.mark_start_time();
    }
}

pipeline_coordinator::~pipeline_coordinator() {
    if (running_.load(std::memory_order_acquire)) {
        static_cast<void>(stop());
    }
}

// =============================================================================
// Lifecycle Management
// =============================================================================

auto pipeline_coordinator::start() -> VoidResult {
    std::lock_guard<std::mutex> start_stop_lock(start_stop_mutex_);

    if (running_.load(std::memory_order_acquire)) {
        return pacs_void_error(error_codes::already_exists,
                               "Pipeline coordinator is already running");
    }

    // Create and start thread pools for each stage without holding mutex_;
    // they are published to stage_pools_ once all of them have started
    std::array<pool_ptr, stage_count> pools;
    for (size_t i = 0; i < stage_count; ++i) {
        auto stage = static_cast<pipeline_stage>(i);
        try {
            pools[i] = create_stage_pool(stage);
            if (!pools[i]->start()) {
                // Cleanup already started pools
                shutdown_pools(pools, false);
                return pacs_void_error(error_codes::internal_error,
                    "Failed to start thread pool for stage: " +
                    std::string(get_stage_name(stage)));
            }

            // Update metrics
            if (config_.enable_metrics) {
                auto& stage_metrics = metrics_.get_stage_metrics(stage);
                stage_metrics.active_workers.store(
                    static_cast<uint32_t>(config_.get_workers_for_stage(stage)),
                    std::memory_order_relaxed);
            }
        } catch (const std::exception& e) {
            // Cleanup already started pools
            shutdown_pools(pools, false);
            return pacs_void_error(error_codes::internal_error,
                "Exception creating thread pool for stage " +
                std::string(get_stage_name(stage)) + ": " + e.what());
        }
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        stage_pools_.swap(pools);
        running_.store(true, std::memory_order_release);
    }
    return ok();
}

auto pipeline_coordinator::stop() -> VoidResult {
    std::lock_guard<std::mutex> start_stop_lock(start_stop_mutex_);

    // Change the state and take the pools out under mutex_, then stop them
    // after releasing it. A pool worker holds its lifecycle lock while it
    // runs a job, and the job's completion path takes mutex_
    // (notify_job_completion()), so stopping a pool under mutex_ inverts
    // the lock order and can deadlock (#1253)
    std::array<pool_ptr, stage_count> pools;
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!running_.load(std::memory_order_acquire)) {
            return ok();  // Already stopped
        }

        running_.store(false, std::memory_order_release);
        stage_pools_.swap(pools);
    }

    // Use wait_for_completion=true to drain queues gracefully
    shutdown_pools(pools, true);

    return ok();
}

auto pipeline_coordinator::is_running() const noexcept -> bool {
    return running_.load(std::memory_order_acquire);
}

// =============================================================================
// Job Submission
// =============================================================================

auto pipeline_coordinator::submit_to_stage(pipeline_stage stage,
                                           std::unique_ptr<pipeline_job_base> job)
    -> VoidResult {
    if (!job) {
        return pacs_void_error(error_codes::invalid_argument,
                               "Cannot submit null job");
    }
    // Extract context before moving job to avoid undefined behavior
    // (evaluation order of function arguments is unspecified in C++)
    auto ctx = job->get_context();
    return submit_to_stage(stage, std::move(job), ctx);
}

auto pipeline_coordinator::submit_to_stage(pipeline_stage stage,
                                           std::unique_ptr<pipeline_job_base> job,
                                           const job_context& ctx) -> VoidResult {
    if (!running_.load(std::memory_order_acquire)) {
        return pacs_void_error(error_codes::not_initialized,
                               "Pipeline coordinator is not running");
    }

    size_t stage_idx = static_cast<size_t>(stage);
    if (stage_idx >= stage_count) {
        return pacs_void_error(error_codes::invalid_argument,
                               "Invalid pipeline stage");
    }

    // Hold a reference so a concurrent stop() cannot release the pool while
    // the job is handed over; the pool is empty once stop() has begun
    auto pool = get_pool(stage_idx);
    if (!pool) {
        return pacs_void_error(error_codes::not_initialized,
                               "Pipeline coordinator is not running");
    }

    // Check backpressure
    check_backpressure(stage);

    // Update metrics
    if (config_.enable_metrics) {
        metrics_.increment_stage_queue(stage);
    }

    // The lambda shares ownership of the job, so a job the pool discards
    // without running it (a stopping pool cancels its queued jobs) is still
    // destroyed
    std::shared_ptr<pipeline_job_base> shared_job(std::move(job));
    auto start_time = std::chrono::steady_clock::now();

    // Submit to stage pool
    try {
        pool->submit_fire_and_forget(
            [this, shared_job, stage, ctx_copy = ctx, start_time]() mutable {
                auto owned_job = std::move(shared_job);

                // Execute the job
                auto result = owned_job->execute(*this);

                // Record completion
                auto end_time = std::chrono::steady_clock::now();
                auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    end_time - start_time).count();

                bool success = result.is_ok();

                if (config_.enable_metrics) {
                    metrics_.decrement_stage_queue(stage);
                    metrics_.record_stage_completion(stage,
                        static_cast<uint64_t>(duration_ns), success);
                }

                notify_job_completion(ctx_copy, success);
            });
    } catch (const std::exception& e) {
        // The pool did not take the job, for example because a concurrent
        // stop() stopped it after the checks above; the job never ran
        if (config_.enable_metrics) {
            metrics_.decrement_stage_queue(stage);
        }
        return submit_failure(e);
    }

    return ok();
}

auto pipeline_coordinator::submit_task(pipeline_stage stage,
                                       std::function<void()> task) -> VoidResult {
    if (!running_.load(std::memory_order_acquire)) {
        return pacs_void_error(error_codes::not_initialized,
                               "Pipeline coordinator is not running");
    }

    size_t stage_idx = static_cast<size_t>(stage);
    if (stage_idx >= stage_count) {
        return pacs_void_error(error_codes::invalid_argument,
                               "Invalid pipeline stage");
    }

    auto pool = get_pool(stage_idx);
    if (!pool) {
        return pacs_void_error(error_codes::not_initialized,
                               "Pipeline coordinator is not running");
    }

    if (config_.enable_metrics) {
        metrics_.increment_stage_queue(stage);
    }

    try {
        pool->submit_fire_and_forget(
            [this, stage, task = std::move(task)]() {
                task();
                if (config_.enable_metrics) {
                    metrics_.decrement_stage_queue(stage);
                }
            });
    } catch (const std::exception& e) {
        // The pool did not take the task (see submit_to_stage())
        if (config_.enable_metrics) {
            metrics_.decrement_stage_queue(stage);
        }
        return submit_failure(e);
    }

    return ok();
}

// =============================================================================
// Stage Management
// =============================================================================

auto pipeline_coordinator::get_stage_pool(pipeline_stage stage)
    -> std::shared_ptr<kcenon::pacs::integration::thread_pool_interface> {
    return get_pool(static_cast<size_t>(stage));
}

auto pipeline_coordinator::get_queue_depth(pipeline_stage stage) const noexcept
    -> size_t {
    auto pool = get_pool(static_cast<size_t>(stage));
    return pool ? pool->get_pending_task_count() : 0;
}

auto pipeline_coordinator::is_backpressure_active(pipeline_stage stage) const noexcept
    -> bool {
    return get_queue_depth(stage) >= config_.max_queue_depth;
}

// =============================================================================
// Configuration & Callbacks
// =============================================================================

auto pipeline_coordinator::get_config() const noexcept -> const pipeline_config& {
    return config_;
}

void pipeline_coordinator::set_job_completion_callback(job_completion_callback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    job_completion_callback_ = std::move(callback);
}

void pipeline_coordinator::set_backpressure_callback(backpressure_callback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    backpressure_callback_ = std::move(callback);
}

// =============================================================================
// Metrics
// =============================================================================

auto pipeline_coordinator::get_metrics() noexcept -> pipeline_metrics& {
    return metrics_;
}

auto pipeline_coordinator::get_metrics() const noexcept -> const pipeline_metrics& {
    return metrics_;
}

void pipeline_coordinator::reset_metrics() noexcept {
    metrics_.reset();
    metrics_.mark_start_time();
}

// =============================================================================
// Statistics
// =============================================================================

auto pipeline_coordinator::get_total_worker_count() const noexcept -> size_t {
    size_t total = 0;
    for (const auto& pool : get_pools()) {
        if (pool) {
            total += pool->get_thread_count();
        }
    }
    return total;
}

auto pipeline_coordinator::get_total_pending_jobs() const noexcept -> size_t {
    size_t total = 0;
    for (const auto& pool : get_pools()) {
        if (pool) {
            total += pool->get_pending_task_count();
        }
    }
    return total;
}

auto pipeline_coordinator::generate_job_id() noexcept -> uint64_t {
    return next_job_id_.fetch_add(1, std::memory_order_relaxed);
}

// =============================================================================
// Private Methods
// =============================================================================

auto pipeline_coordinator::create_stage_pool(pipeline_stage stage)
    -> pool_ptr {
    kcenon::pacs::integration::thread_pool_config pool_config;
    pool_config.min_threads = config_.get_workers_for_stage(stage);
    pool_config.max_threads = config_.get_workers_for_stage(stage);
    pool_config.pool_name = config_.name_prefix + "_" + std::string(get_stage_name(stage));

    return std::make_shared<kcenon::pacs::integration::thread_pool_adapter>(pool_config);
}

auto pipeline_coordinator::get_pool(size_t stage_idx) const -> pool_ptr {
    if (stage_idx >= stage_count) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    return stage_pools_[stage_idx];
}

auto pipeline_coordinator::get_pools() const -> std::array<pool_ptr, stage_count> {
    std::lock_guard<std::mutex> lock(mutex_);
    return stage_pools_;
}

auto pipeline_coordinator::submit_failure(const std::exception& e) const -> VoidResult {
    if (!running_.load(std::memory_order_acquire)) {
        return pacs_void_error(error_codes::not_initialized,
                               "Pipeline coordinator is not running");
    }
    return pacs_void_error(error_codes::internal_error,
                           std::string("Failed to submit to stage pool: ") + e.what());
}

void pipeline_coordinator::shutdown_pools(std::array<pool_ptr, stage_count>& pools,
                                          bool wait_for_completion) {
    // Callers must not hold mutex_ (see stop())
    for (auto& pool : pools) {
        if (pool) {
            pool->shutdown(wait_for_completion);
            pool.reset();
        }
    }
}

void pipeline_coordinator::notify_job_completion(const job_context& ctx, bool success) {
    job_completion_callback callback;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callback = job_completion_callback_;
    }

    if (callback) {
        try {
            callback(ctx, success);
        } catch (...) {
            // Swallow exceptions from callback to prevent pipeline disruption
        }
    }
}

void pipeline_coordinator::check_backpressure(pipeline_stage stage) {
    size_t depth = get_queue_depth(stage);
    if (depth >= config_.max_queue_depth) {
        backpressure_callback callback;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            callback = backpressure_callback_;
        }

        if (callback) {
            try {
                callback(stage, depth);
            } catch (...) {
                // Swallow exceptions from callback
            }
        }
    }
}

}  // namespace kcenon::pacs::network::pipeline
