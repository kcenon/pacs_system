/**
 * @file parallel_query_executor.hpp
 * @brief Parallel query executor for concurrent query processing
 *
 * This file provides the parallel_query_executor class for executing multiple
 * queries concurrently using the thread pool. Supports batch execution,
 * timeout handling, and query prioritization.
 *
 * @see SRS-SVC-006, FR-5.3
 * @see Issue #190 - Parallel query executor
 * @see DICOM PS3.4 Section C - Query/Retrieve Service Class
 */

#pragma once

#include "query_result_stream.hpp"

#include <pacs/core/dicom_dataset.hpp>
#include <pacs/services/query_scp.hpp>

#include <kcenon/common/patterns/result.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace pacs::storage {
class index_database;
}  // namespace pacs::storage

namespace pacs::services {

/// Result type alias for operations returning a value
template <typename T>
using Result = kcenon::common::Result<T>;

/// Result type alias for void operations
using VoidResult = kcenon::common::VoidResult;

/**
 * @brief Query request for parallel execution
 *
 * Encapsulates all parameters needed to execute a single query.
 */
struct query_request {
    /// Query level (patient, study, series, image)
    query_level level = query_level::study;

    /// DICOM dataset containing query criteria
    core::dicom_dataset query_keys;

    /// Calling AE title (for logging/access control)
    std::string calling_ae;

    /// Optional query ID for tracking
    std::string query_id;

    /// Priority (lower value = higher priority)
    int priority = 0;
};

/**
 * @brief Result of a parallel query execution
 */
struct query_execution_result {
    /// Query ID (from request)
    std::string query_id;

    /// Whether the query succeeded
    bool success = false;

    /// Error message (if failed)
    std::string error_message;

    /// Result stream (if successful)
    std::unique_ptr<query_result_stream> stream;

    /// Execution time in milliseconds
    std::chrono::milliseconds execution_time{0};

    /// Whether the query was cancelled
    bool cancelled = false;

    /// Whether the query timed out
    bool timed_out = false;
};

/**
 * @brief Configuration for parallel query executor
 */
struct parallel_executor_config {
    /// Maximum number of concurrent queries (default: 4)
    size_t max_concurrent = 4;

    /// Default timeout for queries (0 = no timeout)
    std::chrono::milliseconds default_timeout{0};

    /// Page size for result streams
    size_t page_size = 100;

    /// Enable query prioritization
    bool enable_priority = true;
};

/**
 * @brief Parallel query executor for concurrent query processing
 *
 * Provides parallel execution of multiple queries using the thread pool.
 * Supports batch execution, timeout handling, and query prioritization.
 *
 * Thread Safety: This class is thread-safe. All public methods can be
 * called concurrently from multiple threads.
 *
 * @example
 * @code
 * // Create executor
 * parallel_executor_config config;
 * config.max_concurrent = 4;
 * config.default_timeout = std::chrono::seconds(30);
 *
 * parallel_query_executor executor(db, config);
 *
 * // Create multiple queries
 * std::vector<query_request> queries;
 * queries.push_back({query_level::study, study_keys, "CALLING_AE", "query1"});
 * queries.push_back({query_level::series, series_keys, "CALLING_AE", "query2"});
 *
 * // Execute all queries in parallel
 * auto results = executor.execute_all(std::move(queries));
 *
 * for (auto& result : results) {
 *     if (result.success) {
 *         while (result.stream->has_more()) {
 *             auto batch = result.stream->next_batch();
 *             // Process batch
 *         }
 *     }
 * }
 *
 * // Single query with timeout
 * query_request req;
 * req.level = query_level::patient;
 * req.query_keys = patient_keys;
 * req.calling_ae = "CALLING_AE";
 *
 * auto stream_result = executor.execute_with_timeout(
 *     req, std::chrono::seconds(10));
 *
 * if (stream_result.is_ok()) {
 *     auto& stream = stream_result.value();
 *     // Process stream
 * }
 * @endcode
 */
class parallel_query_executor {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct a parallel query executor
     *
     * @param db Pointer to the index database (must outlive this executor)
     * @param config Configuration options (optional)
     */
    explicit parallel_query_executor(storage::index_database* db,
                                     const parallel_executor_config& config = {});

    /**
     * @brief Destructor
     *
     * Cancels any pending queries and waits for completion.
     */
    ~parallel_query_executor();

    // Non-copyable
    parallel_query_executor(const parallel_query_executor&) = delete;
    auto operator=(const parallel_query_executor&) -> parallel_query_executor& = delete;

    // Movable
    parallel_query_executor(parallel_query_executor&&) noexcept;
    auto operator=(parallel_query_executor&&) noexcept -> parallel_query_executor&;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Set maximum concurrent queries
     *
     * @param max Maximum number of concurrent queries
     */
    void set_max_concurrent(size_t max) noexcept;

    /**
     * @brief Get maximum concurrent queries
     *
     * @return Maximum number of concurrent queries
     */
    [[nodiscard]] auto max_concurrent() const noexcept -> size_t;

    /**
     * @brief Set default timeout for queries
     *
     * @param timeout Default timeout (0 = no timeout)
     */
    void set_default_timeout(std::chrono::milliseconds timeout) noexcept;

    /**
     * @brief Get default timeout
     *
     * @return Default timeout
     */
    [[nodiscard]] auto default_timeout() const noexcept -> std::chrono::milliseconds;

    // =========================================================================
    // Batch Execution
    // =========================================================================

    /**
     * @brief Execute multiple queries in parallel
     *
     * Executes all queries concurrently (up to max_concurrent at a time).
     * Results are returned in the same order as the input queries.
     *
     * If priority is enabled, queries are sorted by priority before execution.
     *
     * @param queries Vector of query requests to execute
     * @return Vector of execution results (same order as input)
     */
    [[nodiscard]] auto execute_all(std::vector<query_request> queries)
        -> std::vector<query_execution_result>;

    // =========================================================================
    // Single Query with Timeout
    // =========================================================================

    /**
     * @brief Execute a single query with timeout
     *
     * Executes the query and returns a result stream if successful.
     * The query is cancelled if it exceeds the specified timeout.
     *
     * @param query Query request to execute
     * @param timeout Timeout duration (overrides default)
     * @return Result containing the stream or error
     */
    [[nodiscard]] auto execute_with_timeout(const query_request& query,
                                            std::chrono::milliseconds timeout)
        -> Result<std::unique_ptr<query_result_stream>>;

    /**
     * @brief Execute a single query with default timeout
     *
     * Uses the default timeout configured for this executor.
     *
     * @param query Query request to execute
     * @return Result containing the stream or error
     */
    [[nodiscard]] auto execute(const query_request& query)
        -> Result<std::unique_ptr<query_result_stream>>;

    // =========================================================================
    // Cancellation
    // =========================================================================

    /**
     * @brief Cancel all pending queries
     *
     * Sets the cancellation flag for all pending queries.
     * Already executing queries will check this flag periodically.
     */
    void cancel_all() noexcept;

    /**
     * @brief Check if cancellation was requested
     *
     * @return true if cancel_all() was called
     */
    [[nodiscard]] auto is_cancelled() const noexcept -> bool;

    /**
     * @brief Reset cancellation flag
     *
     * Clears the cancellation flag for new batch execution.
     */
    void reset_cancellation() noexcept;

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get total queries executed
     *
     * @return Number of queries executed since creation
     */
    [[nodiscard]] auto queries_executed() const noexcept -> size_t;

    /**
     * @brief Get total queries succeeded
     *
     * @return Number of queries that succeeded
     */
    [[nodiscard]] auto queries_succeeded() const noexcept -> size_t;

    /**
     * @brief Get total queries failed
     *
     * @return Number of queries that failed
     */
    [[nodiscard]] auto queries_failed() const noexcept -> size_t;

    /**
     * @brief Get total queries timed out
     *
     * @return Number of queries that timed out
     */
    [[nodiscard]] auto queries_timed_out() const noexcept -> size_t;

    /**
     * @brief Get number of currently executing queries
     *
     * @return Number of queries currently in progress
     */
    [[nodiscard]] auto queries_in_progress() const noexcept -> size_t;

    /**
     * @brief Reset statistics counters
     */
    void reset_statistics() noexcept;

private:
    // =========================================================================
    // Private Implementation
    // =========================================================================

    /**
     * @brief Execute a single query (internal)
     *
     * @param query Query request
     * @param timeout Timeout duration
     * @return Execution result
     */
    [[nodiscard]] auto execute_query_internal(const query_request& query,
                                              std::chrono::milliseconds timeout)
        -> query_execution_result;

    /**
     * @brief Create a stream from query parameters
     *
     * @param query Query request
     * @return Result containing the stream or error
     */
    [[nodiscard]] auto create_stream(const query_request& query)
        -> Result<std::unique_ptr<query_result_stream>>;

    // =========================================================================
    // Member Variables
    // =========================================================================

    /// Database pointer
    storage::index_database* db_;

    /// Configuration
    parallel_executor_config config_;

    /// Cancellation flag
    std::atomic<bool> cancelled_{false};

    /// Statistics
    std::atomic<size_t> queries_executed_{0};
    std::atomic<size_t> queries_succeeded_{0};
    std::atomic<size_t> queries_failed_{0};
    std::atomic<size_t> queries_timed_out_{0};
    std::atomic<size_t> queries_in_progress_{0};

    /// Mutex for thread-safe operations
    mutable std::mutex mutex_;
};

}  // namespace pacs::services
