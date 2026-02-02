/**
 * @file parallel_query_executor.cpp
 * @brief Implementation of parallel query executor
 *
 * @see Issue #190 - Parallel query executor
 */

#include <pacs/services/cache/parallel_query_executor.hpp>

#ifdef PACS_WITH_DATABASE_SYSTEM

#include <pacs/storage/index_database.hpp>

#include <kcenon/common/patterns/result.h>

#include <algorithm>
#include <future>
#include <thread>
#include <utility>

namespace pacs::services {

namespace {

/// Helper to create error Result with error_info
template<typename T>
Result<T> make_error(const std::string& message, const std::string& module = "parallel_query_executor") {
    return Result<T>::err(kcenon::common::error_info(0, message, module));
}

}  // namespace

// ============================================================================
// Construction / Destruction
// ============================================================================

parallel_query_executor::parallel_query_executor(storage::index_database* db,
                                                 const parallel_executor_config& config)
    : db_(db), config_(config) {}

parallel_query_executor::~parallel_query_executor() {
    cancel_all();
    // Wait for in-progress queries to complete
    while (queries_in_progress_.load() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

parallel_query_executor::parallel_query_executor(parallel_query_executor&& other) noexcept
    : db_(other.db_),
      config_(other.config_),
      cancelled_(other.cancelled_.load()),
      queries_executed_(other.queries_executed_.load()),
      queries_succeeded_(other.queries_succeeded_.load()),
      queries_failed_(other.queries_failed_.load()),
      queries_timed_out_(other.queries_timed_out_.load()),
      queries_in_progress_(other.queries_in_progress_.load()) {
    other.db_ = nullptr;
}

auto parallel_query_executor::operator=(parallel_query_executor&& other) noexcept
    -> parallel_query_executor& {
    if (this != &other) {
        cancel_all();
        while (queries_in_progress_.load() > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        db_ = other.db_;
        config_ = other.config_;
        cancelled_.store(other.cancelled_.load());
        queries_executed_.store(other.queries_executed_.load());
        queries_succeeded_.store(other.queries_succeeded_.load());
        queries_failed_.store(other.queries_failed_.load());
        queries_timed_out_.store(other.queries_timed_out_.load());
        queries_in_progress_.store(other.queries_in_progress_.load());

        other.db_ = nullptr;
    }
    return *this;
}

// ============================================================================
// Configuration
// ============================================================================

void parallel_query_executor::set_max_concurrent(size_t max) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.max_concurrent = max > 0 ? max : 1;
}

auto parallel_query_executor::max_concurrent() const noexcept -> size_t {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.max_concurrent;
}

void parallel_query_executor::set_default_timeout(std::chrono::milliseconds timeout) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.default_timeout = timeout;
}

auto parallel_query_executor::default_timeout() const noexcept -> std::chrono::milliseconds {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.default_timeout;
}

// ============================================================================
// Batch Execution
// ============================================================================

auto parallel_query_executor::execute_all(std::vector<query_request> queries)
    -> std::vector<query_execution_result> {
    // Directly implement here instead of delegating to avoid ambiguity
    if (queries.empty()) {
        return {};
    }

    // Reset cancellation for new batch
    reset_cancellation();

    // Get configuration snapshot
    parallel_executor_config config;
    std::chrono::milliseconds timeout;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        config = config_;
        timeout = config_.default_timeout;
    }

    // Sort by priority if enabled
    if (config.enable_priority) {
        std::stable_sort(queries.begin(), queries.end(),
                         [](const query_request& a, const query_request& b) {
                             return a.priority < b.priority;
                         });
    }

    // Store original indices for result ordering
    std::vector<size_t> original_indices(queries.size());
    for (size_t i = 0; i < queries.size(); ++i) {
        original_indices[i] = i;
    }

    // If priority sorting was done, we need to track original positions
    // Create index mapping after sort
    std::vector<std::pair<size_t, query_request>> indexed_queries;
    indexed_queries.reserve(queries.size());
    for (size_t i = 0; i < queries.size(); ++i) {
        indexed_queries.emplace_back(i, std::move(queries[i]));
    }

    // Execute queries in batches
    std::vector<query_execution_result> results(indexed_queries.size());
    std::vector<std::future<query_execution_result>> futures;
    futures.reserve(config.max_concurrent);

    size_t submitted = 0;

    while (submitted < indexed_queries.size() || !futures.empty()) {
        // Check cancellation
        if (is_cancelled()) {
            // Mark remaining queries as cancelled
            for (size_t i = submitted; i < indexed_queries.size(); ++i) {
                auto& [idx, query] = indexed_queries[i];
                query_execution_result result;
                result.query_id = query.query_id;
                result.success = false;
                result.cancelled = true;
                result.error_message = "Query cancelled";
                results[idx] = std::move(result);
            }
            break;
        }

        // Submit new queries up to max_concurrent
        while (submitted < indexed_queries.size() && futures.size() < config.max_concurrent) {
            auto& [idx, query] = indexed_queries[submitted];
            (void)idx;  // Used later for result mapping

            // Capture by value for thread safety
            auto captured_query = query;
            auto captured_timeout = timeout;

            // Use std::async for simpler and more portable parallel execution
            futures.push_back(std::async(std::launch::async,
                [this, captured_query = std::move(captured_query), captured_timeout]() {
                    return execute_query_internal(captured_query, captured_timeout);
                }));

            ++submitted;
        }

        // Wait for at least one future to complete
        if (!futures.empty()) {
            // Find completed futures
            for (auto it = futures.begin(); it != futures.end();) {
                if (it->wait_for(std::chrono::milliseconds(1)) == std::future_status::ready) {
                    auto result = it->get();

                    // Find the corresponding original index
                    size_t result_idx = 0;
                    for (size_t i = 0; i < indexed_queries.size(); ++i) {
                        if (indexed_queries[i].second.query_id == result.query_id) {
                            result_idx = indexed_queries[i].first;
                            break;
                        }
                    }

                    results[result_idx] = std::move(result);
                    it = futures.erase(it);
                } else {
                    ++it;
                }
            }

            // Small sleep to avoid busy waiting
            if (futures.size() >= config.max_concurrent) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }

    // Wait for remaining futures
    for (auto& future : futures) {
        auto result = future.get();

        // Find the corresponding original index
        for (size_t i = 0; i < indexed_queries.size(); ++i) {
            if (indexed_queries[i].second.query_id == result.query_id) {
                results[indexed_queries[i].first] = std::move(result);
                break;
            }
        }
    }

    return results;
}

// ============================================================================
// Single Query Execution
// ============================================================================

auto parallel_query_executor::execute_with_timeout(const query_request& query,
                                                   std::chrono::milliseconds timeout)
    -> Result<std::unique_ptr<query_result_stream>> {
    auto result = execute_query_internal(query, timeout);

    if (result.success) {
        return Result<std::unique_ptr<query_result_stream>>::ok(std::move(result.stream));
    }

    if (result.timed_out) {
        return make_error<std::unique_ptr<query_result_stream>>(
            "Query timed out after " + std::to_string(timeout.count()) + "ms");
    }

    if (result.cancelled) {
        return make_error<std::unique_ptr<query_result_stream>>("Query was cancelled");
    }

    return make_error<std::unique_ptr<query_result_stream>>(result.error_message);
}

auto parallel_query_executor::execute(const query_request& query)
    -> Result<std::unique_ptr<query_result_stream>> {
    std::chrono::milliseconds timeout;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        timeout = config_.default_timeout;
    }
    return execute_with_timeout(query, timeout);
}

// ============================================================================
// Cancellation
// ============================================================================

void parallel_query_executor::cancel_all() noexcept {
    cancelled_.store(true);
}

auto parallel_query_executor::is_cancelled() const noexcept -> bool {
    return cancelled_.load();
}

void parallel_query_executor::reset_cancellation() noexcept {
    cancelled_.store(false);
}

// ============================================================================
// Statistics
// ============================================================================

auto parallel_query_executor::queries_executed() const noexcept -> size_t {
    return queries_executed_.load();
}

auto parallel_query_executor::queries_succeeded() const noexcept -> size_t {
    return queries_succeeded_.load();
}

auto parallel_query_executor::queries_failed() const noexcept -> size_t {
    return queries_failed_.load();
}

auto parallel_query_executor::queries_timed_out() const noexcept -> size_t {
    return queries_timed_out_.load();
}

auto parallel_query_executor::queries_in_progress() const noexcept -> size_t {
    return queries_in_progress_.load();
}

void parallel_query_executor::reset_statistics() noexcept {
    queries_executed_.store(0);
    queries_succeeded_.store(0);
    queries_failed_.store(0);
    queries_timed_out_.store(0);
}

// ============================================================================
// Private Implementation
// ============================================================================

auto parallel_query_executor::execute_query_internal(const query_request& query,
                                                     std::chrono::milliseconds timeout)
    -> query_execution_result {
    query_execution_result result;
    result.query_id = query.query_id;

    auto start_time = std::chrono::steady_clock::now();

    // Track in-progress count
    ++queries_in_progress_;
    ++queries_executed_;

    // Check cancellation before starting
    if (is_cancelled()) {
        result.success = false;
        result.cancelled = true;
        result.error_message = "Query cancelled before execution";
        --queries_in_progress_;
        ++queries_failed_;
        return result;
    }

    // Execute with timeout if specified
    if (timeout.count() > 0) {
        // Use async with timeout
        auto future = std::async(std::launch::async, [this, &query]() {
            return create_stream(query);
        });

        auto status = future.wait_for(timeout);

        if (status == std::future_status::timeout) {
            result.success = false;
            result.timed_out = true;
            result.error_message = "Query timed out";
            result.execution_time = timeout;
            --queries_in_progress_;
            ++queries_failed_;
            ++queries_timed_out_;
            return result;
        }

        auto stream_result = future.get();
        auto end_time = std::chrono::steady_clock::now();
        result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);

        if (stream_result.is_ok()) {
            result.success = true;
            result.stream = std::move(stream_result.value());
            --queries_in_progress_;
            ++queries_succeeded_;
        } else {
            result.success = false;
            result.error_message = stream_result.error().message;
            --queries_in_progress_;
            ++queries_failed_;
        }
    } else {
        // No timeout - direct execution
        auto stream_result = create_stream(query);
        auto end_time = std::chrono::steady_clock::now();
        result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);

        if (stream_result.is_ok()) {
            result.success = true;
            result.stream = std::move(stream_result.value());
            --queries_in_progress_;
            ++queries_succeeded_;
        } else {
            result.success = false;
            result.error_message = stream_result.error().message;
            --queries_in_progress_;
            ++queries_failed_;
        }
    }

    return result;
}

auto parallel_query_executor::create_stream(const query_request& query)
    -> Result<std::unique_ptr<query_result_stream>> {
    if (db_ == nullptr) {
        return make_error<std::unique_ptr<query_result_stream>>("Database not initialized");
    }

    stream_config config;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        config.page_size = config_.page_size;
    }

    return query_result_stream::create(db_, query.level, query.query_keys, config);
}

}  // namespace pacs::services

#endif  // PACS_WITH_DATABASE_SYSTEM
