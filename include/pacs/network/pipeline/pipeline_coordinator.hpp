/**
 * @file pipeline_coordinator.hpp
 * @brief Main coordinator for the 6-stage DICOM I/O pipeline
 *
 * The pipeline coordinator manages multiple thread pools, one per stage,
 * to achieve high-throughput DICOM operations with optimal parallelism.
 *
 * @see Issue #517 - Implement typed_thread_pool-based I/O Pipeline
 * @see DICOM PS3.8 Network Communication Support
 */

#pragma once

#include <pacs/network/pipeline/pipeline_job_types.hpp>
#include <pacs/network/pipeline/metrics/pipeline_metrics.hpp>
#include <pacs/integration/thread_pool_interface.hpp>
#include <pacs/core/result.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace pacs::network::pipeline {

/**
 * @struct pipeline_config
 * @brief Configuration options for the pipeline coordinator
 *
 * Default values are tuned for a typical PACS workload with
 * balanced throughput and latency requirements.
 */
struct pipeline_config {
    /// Number of workers for network I/O stages (1 & 6)
    /// Low latency, non-blocking operations
    size_t net_io_workers = 4;

    /// Number of workers for protocol stages (2 & 3)
    /// PDU decoding and DIMSE processing
    size_t protocol_workers = 2;

    /// Number of workers for execution stage (4)
    /// Blocking I/O allowed (database, file system)
    size_t execution_workers = 8;

    /// Number of workers for encoding stage (5)
    /// Response PDU encoding
    size_t encode_workers = 2;

    /// Maximum queue depth per stage (backpressure threshold)
    size_t max_queue_depth = 10000;

    /// Graceful shutdown timeout
    std::chrono::milliseconds shutdown_timeout{500};

    /// Enable metrics collection
    bool enable_metrics = true;

    /// Name prefix for thread pools (for logging)
    std::string name_prefix = "pipeline";

    /**
     * @brief Get the number of workers for a specific stage
     * @param stage The pipeline stage
     * @return Number of workers configured for that stage
     */
    [[nodiscard]] auto get_workers_for_stage(pipeline_stage stage) const noexcept
        -> size_t {
        switch (stage) {
            case pipeline_stage::network_receive:
            case pipeline_stage::network_send:
                return net_io_workers;

            case pipeline_stage::pdu_decode:
            case pipeline_stage::dimse_process:
                return protocol_workers;

            case pipeline_stage::storage_query_exec:
                return execution_workers;

            case pipeline_stage::response_encode:
                return encode_workers;

            default:
                return 1;
        }
    }
};

// Forward declarations for job types
class pipeline_job_base;

/**
 * @class pipeline_coordinator
 * @brief Coordinates the 6-stage DICOM I/O pipeline
 *
 * The coordinator manages:
 * - Per-stage thread pools with dedicated workers
 * - Job submission and routing between stages
 * - Backpressure handling when queues are full
 * - Graceful shutdown with timeout
 * - Metrics collection for monitoring
 *
 * Thread Safety: All public methods are thread-safe.
 *
 * @example
 * @code
 * // Create coordinator with custom configuration
 * pipeline_config config;
 * config.execution_workers = 16;  // More DB workers
 * config.enable_metrics = true;
 *
 * auto coordinator = std::make_shared<pipeline_coordinator>(config);
 * auto result = coordinator->start();
 * if (!result) {
 *     // Handle error
 * }
 *
 * // Submit a job to the first stage
 * auto job = std::make_unique<receive_network_io_job>(...);
 * coordinator->submit_to_stage(pipeline_stage::network_receive, std::move(job));
 *
 * // Get metrics
 * auto& metrics = coordinator->get_metrics();
 * auto throughput = metrics.get_throughput_per_second(job_category::store);
 *
 * // Graceful shutdown
 * coordinator->stop();
 * @endcode
 */
class pipeline_coordinator {
public:
    /// Callback type for job completion notification
    using job_completion_callback = std::function<void(const job_context&, bool success)>;

    /// Callback type for backpressure notification
    using backpressure_callback = std::function<void(pipeline_stage, size_t queue_depth)>;

    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct coordinator with default configuration
     */
    pipeline_coordinator();

    /**
     * @brief Construct coordinator with custom configuration
     * @param config Pipeline configuration options
     */
    explicit pipeline_coordinator(const pipeline_config& config);

    /**
     * @brief Destructor - ensures graceful shutdown
     */
    ~pipeline_coordinator();

    // Non-copyable, non-movable
    pipeline_coordinator(const pipeline_coordinator&) = delete;
    pipeline_coordinator& operator=(const pipeline_coordinator&) = delete;
    pipeline_coordinator(pipeline_coordinator&&) = delete;
    pipeline_coordinator& operator=(pipeline_coordinator&&) = delete;

    // =========================================================================
    // Lifecycle Management
    // =========================================================================

    /**
     * @brief Start the pipeline
     *
     * Initializes and starts all stage thread pools.
     *
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto start() -> VoidResult;

    /**
     * @brief Stop the pipeline gracefully
     *
     * Stops accepting new jobs and waits for pending jobs to complete
     * up to the configured shutdown timeout.
     *
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto stop() -> VoidResult;

    /**
     * @brief Check if the pipeline is running
     * @return true if the pipeline is active
     */
    [[nodiscard]] auto is_running() const noexcept -> bool;

    // =========================================================================
    // Job Submission
    // =========================================================================

    /**
     * @brief Submit a job to a specific stage
     *
     * Jobs are executed asynchronously by the stage's worker pool.
     * If the queue is full, backpressure callback is invoked.
     *
     * @param stage Target pipeline stage
     * @param job Job to execute (ownership transferred)
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto submit_to_stage(pipeline_stage stage,
                                       std::unique_ptr<pipeline_job_base> job)
        -> VoidResult;

    /**
     * @brief Submit a job with specific context
     *
     * @param stage Target pipeline stage
     * @param job Job to execute
     * @param ctx Job context for tracking
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto submit_to_stage(pipeline_stage stage,
                                       std::unique_ptr<pipeline_job_base> job,
                                       const job_context& ctx) -> VoidResult;

    /**
     * @brief Submit a raw function to a stage (for simple tasks)
     *
     * @param stage Target pipeline stage
     * @param task Function to execute
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto submit_task(pipeline_stage stage,
                                   std::function<void()> task) -> VoidResult;

    // =========================================================================
    // Stage Management
    // =========================================================================

    /**
     * @brief Get the thread pool for a specific stage
     *
     * @param stage The pipeline stage
     * @return Shared pointer to the stage's thread pool, or nullptr if not running
     */
    [[nodiscard]] auto get_stage_pool(pipeline_stage stage)
        -> std::shared_ptr<pacs::integration::thread_pool_interface>;

    /**
     * @brief Get queue depth for a specific stage
     *
     * @param stage The pipeline stage
     * @return Number of pending jobs in the stage's queue
     */
    [[nodiscard]] auto get_queue_depth(pipeline_stage stage) const noexcept
        -> size_t;

    /**
     * @brief Check if backpressure is active for a stage
     *
     * @param stage The pipeline stage
     * @return true if queue depth exceeds threshold
     */
    [[nodiscard]] auto is_backpressure_active(pipeline_stage stage) const noexcept
        -> bool;

    // =========================================================================
    // Configuration & Callbacks
    // =========================================================================

    /**
     * @brief Get the current configuration
     * @return Reference to the pipeline configuration
     */
    [[nodiscard]] auto get_config() const noexcept -> const pipeline_config&;

    /**
     * @brief Set the job completion callback
     *
     * Called when any job completes (success or failure).
     *
     * @param callback Callback function
     */
    void set_job_completion_callback(job_completion_callback callback);

    /**
     * @brief Set the backpressure callback
     *
     * Called when a stage's queue depth exceeds threshold.
     *
     * @param callback Callback function
     */
    void set_backpressure_callback(backpressure_callback callback);

    // =========================================================================
    // Metrics
    // =========================================================================

    /**
     * @brief Get the metrics collector
     * @return Reference to the pipeline metrics
     */
    [[nodiscard]] auto get_metrics() noexcept -> pipeline_metrics&;

    /**
     * @brief Get the metrics collector (const)
     */
    [[nodiscard]] auto get_metrics() const noexcept -> const pipeline_metrics&;

    /**
     * @brief Reset all metrics
     */
    void reset_metrics() noexcept;

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get total number of workers across all stages
     * @return Total worker count
     */
    [[nodiscard]] auto get_total_worker_count() const noexcept -> size_t;

    /**
     * @brief Get total pending jobs across all stages
     * @return Total pending job count
     */
    [[nodiscard]] auto get_total_pending_jobs() const noexcept -> size_t;

    /**
     * @brief Generate a unique job ID
     * @return Monotonically increasing job ID
     */
    [[nodiscard]] auto generate_job_id() noexcept -> uint64_t;

private:
    // Stage thread pool array
    static constexpr size_t stage_count =
        static_cast<size_t>(pipeline_stage::stage_count);

    using pool_ptr = std::shared_ptr<pacs::integration::thread_pool_interface>;
    std::array<pool_ptr, stage_count> stage_pools_;

    // Configuration
    pipeline_config config_;

    // State
    std::atomic<bool> running_{false};
    mutable std::mutex mutex_;

    // Metrics
    pipeline_metrics metrics_;

    // Callbacks
    job_completion_callback job_completion_callback_;
    backpressure_callback backpressure_callback_;

    // Job ID generator
    std::atomic<uint64_t> next_job_id_{1};

    // Internal methods
    [[nodiscard]] auto create_stage_pool(pipeline_stage stage) -> pool_ptr;
    void notify_job_completion(const job_context& ctx, bool success);
    void check_backpressure(pipeline_stage stage);
};

/**
 * @class pipeline_job_base
 * @brief Base class for all pipeline jobs
 *
 * Pipeline jobs encapsulate work to be executed by a stage's worker pool.
 * Derived classes implement the execute() method for stage-specific logic.
 */
class pipeline_job_base {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~pipeline_job_base() = default;

    /**
     * @brief Execute the job
     *
     * Called by the worker thread. Implementations should perform the
     * stage-specific work and optionally submit follow-up jobs to the
     * next stage.
     *
     * @param coordinator Reference to the pipeline coordinator
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] virtual auto execute(pipeline_coordinator& coordinator)
        -> VoidResult = 0;

    /**
     * @brief Get the job context
     * @return Reference to the job context
     */
    [[nodiscard]] virtual auto get_context() const noexcept
        -> const job_context& = 0;

    /**
     * @brief Get the job context (mutable)
     * @return Reference to the job context
     */
    [[nodiscard]] virtual auto get_context() noexcept -> job_context& = 0;

    /**
     * @brief Get the job name for logging
     * @return Job name string
     */
    [[nodiscard]] virtual auto get_name() const -> std::string = 0;

protected:
    pipeline_job_base() = default;

    // Non-copyable but movable
    pipeline_job_base(const pipeline_job_base&) = delete;
    pipeline_job_base& operator=(const pipeline_job_base&) = delete;
    pipeline_job_base(pipeline_job_base&&) = default;
    pipeline_job_base& operator=(pipeline_job_base&&) = default;
};

}  // namespace pacs::network::pipeline
