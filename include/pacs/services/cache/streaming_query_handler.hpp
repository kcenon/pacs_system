/**
 * @file streaming_query_handler.hpp
 * @brief Streaming query handler for memory-efficient C-FIND processing
 *
 * This file provides the streaming_query_handler class that integrates
 * query_result_stream with the query_scp service for paginated C-FIND
 * responses without loading all results into memory.
 *
 * When compiled with PACS_WITH_DATABASE_SYSTEM, uses query_result_stream
 * for unified database access.
 *
 * @see SRS-SVC-006, FR-5.3
 * @see Issue #188 - Streaming query results with pagination
 */

#pragma once

#include "query_result_stream.hpp"

#include <pacs/core/dicom_dataset.hpp>
#include <pacs/services/query_scp.hpp>

#include <functional>
#include <memory>
#include <string>

namespace pacs::storage {
class index_database;
}  // namespace pacs::storage

namespace pacs::services {

#ifdef PACS_WITH_DATABASE_SYSTEM

/**
 * @brief Streaming query handler for memory-efficient C-FIND responses
 *
 * Provides a streaming interface for query results that can be used
 * with query_scp. Instead of returning all results in a vector, it
 * allows fetching results in batches.
 *
 * @example
 * @code
 * // Create streaming handler
 * streaming_query_handler handler(db);
 * handler.set_page_size(100);
 *
 * // Use with query_scp (via adapter)
 * scp.set_handler(handler.as_query_handler());
 *
 * // Or use streaming interface directly
 * auto stream_result = handler.create_stream(level, query_keys, ae);
 * if (stream_result.is_ok()) {
 *     auto& stream = stream_result.value();
 *     while (stream->has_more()) {
 *         auto batch = stream->next_batch();
 *         // Process batch
 *     }
 * }
 * @endcode
 */
class streaming_query_handler {
public:
    /// Result type for stream creation
    using StreamResult = Result<std::unique_ptr<query_result_stream>>;

    /**
     * @brief Construct a streaming query handler
     *
     * @param db Pointer to the index database
     */
    explicit streaming_query_handler(storage::index_database* db);

    /**
     * @brief Destructor
     */
    ~streaming_query_handler() = default;

    // Non-copyable
    streaming_query_handler(const streaming_query_handler&) = delete;
    auto operator=(const streaming_query_handler&) -> streaming_query_handler& = delete;

    // Movable
    streaming_query_handler(streaming_query_handler&&) noexcept = default;
    auto operator=(streaming_query_handler&&) noexcept -> streaming_query_handler& = default;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Set the page size for batch fetching
     *
     * @param size Number of results per batch (default: 100)
     */
    void set_page_size(size_t size) noexcept;

    /**
     * @brief Get the current page size
     *
     * @return Current page size
     */
    [[nodiscard]] auto page_size() const noexcept -> size_t;

    /**
     * @brief Set maximum total results
     *
     * @param max Maximum results to return (0 = unlimited)
     */
    void set_max_results(size_t max) noexcept;

    /**
     * @brief Get maximum results limit
     *
     * @return Maximum results (0 = unlimited)
     */
    [[nodiscard]] auto max_results() const noexcept -> size_t;

    // =========================================================================
    // Stream Operations
    // =========================================================================

    /**
     * @brief Create a query result stream
     *
     * Creates a new stream for the given query parameters.
     *
     * @param level Query level (patient, study, series, image)
     * @param query_keys DICOM dataset with query criteria
     * @param calling_ae Calling AE title (for logging/access control)
     * @return Result containing the stream or error
     */
    [[nodiscard]] auto create_stream(query_level level,
                                     const core::dicom_dataset& query_keys,
                                     const std::string& calling_ae) -> StreamResult;

    /**
     * @brief Resume a stream from cursor state
     *
     * @param cursor_state Serialized cursor state
     * @param level Original query level
     * @param query_keys Original query criteria
     * @return Result containing the resumed stream or error
     */
    [[nodiscard]] auto resume_stream(const std::string& cursor_state, query_level level,
                                     const core::dicom_dataset& query_keys) -> StreamResult;

    // =========================================================================
    // Compatibility Adapter
    // =========================================================================

    /**
     * @brief Get a query_handler compatible adapter
     *
     * Returns a query_handler function that can be used with query_scp.
     * This adapter loads all results into memory for compatibility with
     * the existing interface. For true streaming, use create_stream().
     *
     * @return query_handler function
     */
    [[nodiscard]] auto as_query_handler() -> query_handler;

private:
    /// Database pointer
    storage::index_database* db_;

    /// Page size for batch fetching
    size_t page_size_{100};

    /// Maximum results (0 = unlimited)
    size_t max_results_{0};
};

#endif  // PACS_WITH_DATABASE_SYSTEM

}  // namespace pacs::services
