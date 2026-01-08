/**
 * @file pipeline_metrics.hpp
 * @brief Metrics collection for the DICOM I/O pipeline
 *
 * This file provides comprehensive metrics collection for monitoring
 * pipeline performance, throughput, and latency.
 *
 * @see Issue #517 - Implement typed_thread_pool-based I/O Pipeline
 * @see Issue #490 - DICOM metrics collector
 */

#pragma once

#include <pacs/network/pipeline/pipeline_job_types.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>

namespace pacs::network::pipeline {

/**
 * @struct stage_metrics
 * @brief Metrics for a single pipeline stage
 *
 * All counters are atomic for thread-safe updates without locks.
 */
struct stage_metrics {
    /// Total jobs processed by this stage
    std::atomic<uint64_t> jobs_processed{0};

    /// Total jobs currently in queue for this stage
    std::atomic<uint64_t> jobs_queued{0};

    /// Jobs that failed in this stage
    std::atomic<uint64_t> jobs_failed{0};

    /// Cumulative processing time in nanoseconds
    std::atomic<uint64_t> total_processing_time_ns{0};

    /// Maximum processing time observed (nanoseconds)
    std::atomic<uint64_t> max_processing_time_ns{0};

    /// Number of active workers in this stage
    std::atomic<uint32_t> active_workers{0};

    /// Number of idle workers in this stage
    std::atomic<uint32_t> idle_workers{0};

    /**
     * @brief Record a job completion
     * @param processing_time_ns Time taken to process the job in nanoseconds
     * @param success Whether the job completed successfully
     */
    void record_job_completion(uint64_t processing_time_ns, bool success) noexcept {
        jobs_processed.fetch_add(1, std::memory_order_relaxed);
        total_processing_time_ns.fetch_add(processing_time_ns, std::memory_order_relaxed);

        if (!success) {
            jobs_failed.fetch_add(1, std::memory_order_relaxed);
        }

        // Update max processing time (compare-exchange loop)
        uint64_t current_max = max_processing_time_ns.load(std::memory_order_relaxed);
        while (processing_time_ns > current_max &&
               !max_processing_time_ns.compare_exchange_weak(
                   current_max, processing_time_ns,
                   std::memory_order_relaxed, std::memory_order_relaxed)) {
            // Loop until successful or processing_time_ns <= current_max
        }
    }

    /**
     * @brief Get average processing time in nanoseconds
     * @return Average processing time, or 0 if no jobs processed
     */
    [[nodiscard]] auto get_avg_processing_time_ns() const noexcept -> uint64_t {
        uint64_t processed = jobs_processed.load(std::memory_order_relaxed);
        if (processed == 0) {
            return 0;
        }
        return total_processing_time_ns.load(std::memory_order_relaxed) / processed;
    }

    /**
     * @brief Reset all metrics to zero
     */
    void reset() noexcept {
        jobs_processed.store(0, std::memory_order_relaxed);
        jobs_queued.store(0, std::memory_order_relaxed);
        jobs_failed.store(0, std::memory_order_relaxed);
        total_processing_time_ns.store(0, std::memory_order_relaxed);
        max_processing_time_ns.store(0, std::memory_order_relaxed);
    }
};

/**
 * @struct category_metrics
 * @brief Metrics for a job category (e.g., C-STORE, C-FIND)
 */
struct category_metrics {
    /// Total operations of this category
    std::atomic<uint64_t> total_operations{0};

    /// Successful operations
    std::atomic<uint64_t> successful_operations{0};

    /// Failed operations
    std::atomic<uint64_t> failed_operations{0};

    /// Total end-to-end latency in nanoseconds
    std::atomic<uint64_t> total_latency_ns{0};

    /// Maximum end-to-end latency observed
    std::atomic<uint64_t> max_latency_ns{0};

    /// Minimum end-to-end latency observed (initialized to max)
    std::atomic<uint64_t> min_latency_ns{UINT64_MAX};

    /**
     * @brief Record an operation completion
     * @param latency_ns End-to-end latency in nanoseconds
     * @param success Whether the operation succeeded
     */
    void record_operation(uint64_t latency_ns, bool success) noexcept {
        total_operations.fetch_add(1, std::memory_order_relaxed);
        total_latency_ns.fetch_add(latency_ns, std::memory_order_relaxed);

        if (success) {
            successful_operations.fetch_add(1, std::memory_order_relaxed);
        } else {
            failed_operations.fetch_add(1, std::memory_order_relaxed);
        }

        // Update max latency
        uint64_t current_max = max_latency_ns.load(std::memory_order_relaxed);
        while (latency_ns > current_max &&
               !max_latency_ns.compare_exchange_weak(
                   current_max, latency_ns,
                   std::memory_order_relaxed, std::memory_order_relaxed)) {
        }

        // Update min latency
        uint64_t current_min = min_latency_ns.load(std::memory_order_relaxed);
        while (latency_ns < current_min &&
               !min_latency_ns.compare_exchange_weak(
                   current_min, latency_ns,
                   std::memory_order_relaxed, std::memory_order_relaxed)) {
        }
    }

    /**
     * @brief Get average latency in nanoseconds
     */
    [[nodiscard]] auto get_avg_latency_ns() const noexcept -> uint64_t {
        uint64_t total = total_operations.load(std::memory_order_relaxed);
        if (total == 0) {
            return 0;
        }
        return total_latency_ns.load(std::memory_order_relaxed) / total;
    }

    /**
     * @brief Reset all metrics
     */
    void reset() noexcept {
        total_operations.store(0, std::memory_order_relaxed);
        successful_operations.store(0, std::memory_order_relaxed);
        failed_operations.store(0, std::memory_order_relaxed);
        total_latency_ns.store(0, std::memory_order_relaxed);
        max_latency_ns.store(0, std::memory_order_relaxed);
        min_latency_ns.store(UINT64_MAX, std::memory_order_relaxed);
    }
};

/**
 * @class pipeline_metrics
 * @brief Centralized metrics collection for the entire pipeline
 *
 * Provides thread-safe metrics collection with minimal overhead using
 * atomic operations and relaxed memory ordering where safe.
 *
 * @example
 * @code
 * pipeline_metrics metrics;
 *
 * // Record stage processing
 * metrics.record_stage_completion(pipeline_stage::pdu_decode, 1500, true);
 *
 * // Record end-to-end operation
 * metrics.record_operation_completion(job_category::store, 50000, true);
 *
 * // Get throughput
 * auto throughput = metrics.get_throughput_per_second(job_category::store);
 * @endcode
 */
class pipeline_metrics {
public:
    /// Number of pipeline stages
    static constexpr size_t stage_count =
        static_cast<size_t>(pipeline_stage::stage_count);

    /// Number of job categories
    static constexpr size_t category_count = 8;

    /**
     * @brief Default constructor
     */
    pipeline_metrics() = default;

    // Non-copyable, non-movable (contains atomics)
    pipeline_metrics(const pipeline_metrics&) = delete;
    pipeline_metrics& operator=(const pipeline_metrics&) = delete;
    pipeline_metrics(pipeline_metrics&&) = delete;
    pipeline_metrics& operator=(pipeline_metrics&&) = delete;

    // =========================================================================
    // Stage Metrics
    // =========================================================================

    /**
     * @brief Get metrics for a specific stage
     * @param stage The pipeline stage
     * @return Reference to the stage metrics
     */
    [[nodiscard]] auto get_stage_metrics(pipeline_stage stage) noexcept
        -> stage_metrics& {
        return stage_metrics_[static_cast<size_t>(stage)];
    }

    /**
     * @brief Get metrics for a specific stage (const)
     */
    [[nodiscard]] auto get_stage_metrics(pipeline_stage stage) const noexcept
        -> const stage_metrics& {
        return stage_metrics_[static_cast<size_t>(stage)];
    }

    /**
     * @brief Record a stage job completion
     * @param stage The pipeline stage
     * @param processing_time_ns Processing time in nanoseconds
     * @param success Whether the job succeeded
     */
    void record_stage_completion(pipeline_stage stage, uint64_t processing_time_ns,
                                 bool success) noexcept {
        stage_metrics_[static_cast<size_t>(stage)]
            .record_job_completion(processing_time_ns, success);
    }

    /**
     * @brief Increment queued job count for a stage
     */
    void increment_stage_queue(pipeline_stage stage) noexcept {
        stage_metrics_[static_cast<size_t>(stage)].jobs_queued.fetch_add(
            1, std::memory_order_relaxed);
    }

    /**
     * @brief Decrement queued job count for a stage
     */
    void decrement_stage_queue(pipeline_stage stage) noexcept {
        stage_metrics_[static_cast<size_t>(stage)].jobs_queued.fetch_sub(
            1, std::memory_order_relaxed);
    }

    // =========================================================================
    // Category Metrics
    // =========================================================================

    /**
     * @brief Get metrics for a specific category
     * @param category The job category
     * @return Reference to the category metrics
     */
    [[nodiscard]] auto get_category_metrics(job_category category) noexcept
        -> category_metrics& {
        return category_metrics_[static_cast<size_t>(category)];
    }

    /**
     * @brief Get metrics for a specific category (const)
     */
    [[nodiscard]] auto get_category_metrics(job_category category) const noexcept
        -> const category_metrics& {
        return category_metrics_[static_cast<size_t>(category)];
    }

    /**
     * @brief Record an operation completion
     * @param category The job category
     * @param latency_ns End-to-end latency in nanoseconds
     * @param success Whether the operation succeeded
     */
    void record_operation_completion(job_category category, uint64_t latency_ns,
                                     bool success) noexcept {
        category_metrics_[static_cast<size_t>(category)]
            .record_operation(latency_ns, success);
        total_operations_.fetch_add(1, std::memory_order_relaxed);
    }

    // =========================================================================
    // Global Metrics
    // =========================================================================

    /**
     * @brief Get total operations processed
     */
    [[nodiscard]] auto get_total_operations() const noexcept -> uint64_t {
        return total_operations_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Get current number of active associations
     */
    [[nodiscard]] auto get_active_associations() const noexcept -> uint32_t {
        return active_associations_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Increment active association count
     */
    void increment_active_associations() noexcept {
        uint32_t current = active_associations_.fetch_add(1, std::memory_order_relaxed);
        // Update peak if necessary
        uint32_t peak = peak_associations_.load(std::memory_order_relaxed);
        while (current + 1 > peak &&
               !peak_associations_.compare_exchange_weak(
                   peak, current + 1,
                   std::memory_order_relaxed, std::memory_order_relaxed)) {
        }
    }

    /**
     * @brief Decrement active association count
     */
    void decrement_active_associations() noexcept {
        active_associations_.fetch_sub(1, std::memory_order_relaxed);
    }

    /**
     * @brief Get peak concurrent associations
     */
    [[nodiscard]] auto get_peak_associations() const noexcept -> uint32_t {
        return peak_associations_.load(std::memory_order_relaxed);
    }

    // =========================================================================
    // Utility Methods
    // =========================================================================

    /**
     * @brief Reset all metrics
     */
    void reset() noexcept {
        for (auto& stage : stage_metrics_) {
            stage.reset();
        }
        for (auto& category : category_metrics_) {
            category.reset();
        }
        total_operations_.store(0, std::memory_order_relaxed);
        peak_associations_.store(0, std::memory_order_relaxed);
    }

    /**
     * @brief Mark the start time for throughput calculation
     */
    void mark_start_time() noexcept {
        start_time_ = std::chrono::steady_clock::now();
    }

    /**
     * @brief Calculate throughput for a category (operations per second)
     * @param category The job category
     * @return Operations per second since start_time
     */
    [[nodiscard]] auto get_throughput_per_second(job_category category) const noexcept
        -> double {
        auto elapsed = std::chrono::steady_clock::now() - start_time_;
        auto seconds = std::chrono::duration<double>(elapsed).count();
        if (seconds <= 0.0) {
            return 0.0;
        }
        return static_cast<double>(
            category_metrics_[static_cast<size_t>(category)]
                .total_operations.load(std::memory_order_relaxed)) / seconds;
    }

private:
    std::array<stage_metrics, stage_count> stage_metrics_;
    std::array<category_metrics, category_count> category_metrics_;
    std::atomic<uint64_t> total_operations_{0};
    std::atomic<uint32_t> active_associations_{0};
    std::atomic<uint32_t> peak_associations_{0};
    std::chrono::steady_clock::time_point start_time_{std::chrono::steady_clock::now()};
};

}  // namespace pacs::network::pipeline
