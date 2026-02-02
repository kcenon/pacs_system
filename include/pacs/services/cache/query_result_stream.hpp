/**
 * @file query_result_stream.hpp
 * @brief Streaming query results with pagination support
 *
 * This file provides the query_result_stream class for paginated query
 * results. It wraps the database cursor and converts database records
 * to DICOM datasets suitable for C-FIND responses.
 *
 * When compiled with PACS_WITH_DATABASE_SYSTEM, uses pacs_database_adapter
 * for unified database access.
 *
 * @see SRS-SVC-006, FR-5.3
 * @see Issue #188 - Streaming query results with pagination
 * @see DICOM PS3.4 Section C - Query/Retrieve Service Class
 */

#pragma once

#include <pacs/core/dicom_dataset.hpp>
#include <pacs/services/query_scp.hpp>
#include <pacs/storage/instance_record.hpp>
#include <pacs/storage/patient_record.hpp>
#include <pacs/storage/series_record.hpp>
#include <pacs/storage/study_record.hpp>

#include <kcenon/common/patterns/result.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

// Include database_cursor only when PACS_WITH_DATABASE_SYSTEM is defined
// IMPORTANT: Must be outside namespace to avoid polluting pacs::services with std
#ifdef PACS_WITH_DATABASE_SYSTEM
#include "database_cursor.hpp"
#endif

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
 * @brief Configuration for query result streaming
 */
struct stream_config {
    /// Default page size for batched fetching
    size_t page_size = 100;

    /// Whether to include total count (may be expensive for large datasets)
    bool include_total_count = false;
};

#ifdef PACS_WITH_DATABASE_SYSTEM

/**
 * @brief Streaming query results with pagination support
 *
 * Provides paginated access to query results from the database.
 * Instead of loading all results into memory, results are fetched
 * in batches and converted to DICOM datasets on demand.
 *
 * Thread Safety: This class is NOT thread-safe. The stream should
 * be used from a single thread.
 *
 * @example
 * @code
 * // Create stream from query
 * stream_config config;
 * config.page_size = 50;
 *
 * auto stream_result = query_result_stream::create(
 *     db, query_level::study, query_keys, config);
 *
 * if (stream_result.is_err()) {
 *     // Handle error
 *     return;
 * }
 *
 * auto stream = std::move(stream_result.value());
 *
 * // Fetch results in batches
 * while (stream->has_more()) {
 *     auto batch = stream->next_batch();
 *     for (const auto& dataset : batch) {
 *         // Send C-FIND-RSP with dataset
 *     }
 * }
 * @endcode
 */
class query_result_stream {
public:
    /**
     * @brief Destructor
     */
    ~query_result_stream() = default;

    // Non-copyable
    query_result_stream(const query_result_stream&) = delete;
    auto operator=(const query_result_stream&) -> query_result_stream& = delete;

    // Movable
    query_result_stream(query_result_stream&&) noexcept = default;
    auto operator=(query_result_stream&&) noexcept -> query_result_stream& = default;

    // =========================================================================
    // Factory Methods
    // =========================================================================

    /**
     * @brief Create a query result stream from a database and query parameters
     *
     * @param db Pointer to the index database
     * @param level Query level (patient, study, series, image)
     * @param query_keys DICOM dataset containing query criteria
     * @param config Stream configuration
     * @return Result containing the stream or error
     */
    [[nodiscard]] static auto create(storage::index_database* db, query_level level,
                                     const core::dicom_dataset& query_keys,
                                     const stream_config& config = {})
        -> Result<std::unique_ptr<query_result_stream>>;

    /**
     * @brief Create a query result stream from a cursor state
     *
     * Resumes a previously serialized cursor state.
     *
     * @param db Pointer to the index database
     * @param cursor_state Serialized cursor state
     * @param level Query level
     * @param query_keys Original query criteria
     * @param config Stream configuration
     * @return Result containing the stream or error
     */
    [[nodiscard]] static auto from_cursor(storage::index_database* db,
                                          const std::string& cursor_state,
                                          query_level level,
                                          const core::dicom_dataset& query_keys,
                                          const stream_config& config = {})
        -> Result<std::unique_ptr<query_result_stream>>;

    // =========================================================================
    // Stream Operations
    // =========================================================================

    /**
     * @brief Check if there are more results available
     *
     * @return true if more results can be fetched
     */
    [[nodiscard]] auto has_more() const noexcept -> bool;

    /**
     * @brief Fetch the next batch of results
     *
     * Fetches up to `page_size` results from the stream.
     *
     * @return Vector of DICOM datasets (may be smaller than page_size)
     */
    [[nodiscard]] auto next_batch() -> std::optional<std::vector<core::dicom_dataset>>;

    /**
     * @brief Get total count of results (if available)
     *
     * May return nullopt if total count was not requested in config
     * or if the count is not yet computed.
     *
     * @return Optional total count
     */
    [[nodiscard]] auto total_count() const -> std::optional<size_t>;

    /**
     * @brief Get the current cursor position
     *
     * @return Number of records that have been fetched so far
     */
    [[nodiscard]] auto position() const noexcept -> size_t;

    /**
     * @brief Get the query level
     *
     * @return The query level for this stream
     */
    [[nodiscard]] auto level() const noexcept -> query_level;

    // =========================================================================
    // Cursor State
    // =========================================================================

    /**
     * @brief Get the current cursor state for resumption
     *
     * Creates a serialized representation of the current position
     * that can be used to resume the stream later.
     *
     * @return Serialized cursor state
     */
    [[nodiscard]] auto cursor() const -> std::string;

private:
    /**
     * @brief Private constructor - use factory methods
     */
    query_result_stream(std::unique_ptr<database_cursor> cursor, query_level level,
                        const core::dicom_dataset& query_keys, stream_config config);

    /**
     * @brief Convert a patient record to DICOM dataset
     */
    [[nodiscard]] auto patient_to_dataset(const storage::patient_record& record) const
        -> core::dicom_dataset;

    /**
     * @brief Convert a study record to DICOM dataset
     */
    [[nodiscard]] auto study_to_dataset(const storage::study_record& record) const
        -> core::dicom_dataset;

    /**
     * @brief Convert a series record to DICOM dataset
     */
    [[nodiscard]] auto series_to_dataset(const storage::series_record& record) const
        -> core::dicom_dataset;

    /**
     * @brief Convert an instance record to DICOM dataset
     */
    [[nodiscard]] auto instance_to_dataset(const storage::instance_record& record) const
        -> core::dicom_dataset;

    /**
     * @brief Convert a query record to DICOM dataset
     */
    [[nodiscard]] auto record_to_dataset(const query_record& record) const
        -> core::dicom_dataset;

    /**
     * @brief Extract patient query from DICOM dataset
     */
    [[nodiscard]] static auto extract_patient_query(const core::dicom_dataset& keys)
        -> storage::patient_query;

    /**
     * @brief Extract study query from DICOM dataset
     */
    [[nodiscard]] static auto extract_study_query(const core::dicom_dataset& keys)
        -> storage::study_query;

    /**
     * @brief Extract series query from DICOM dataset
     */
    [[nodiscard]] static auto extract_series_query(const core::dicom_dataset& keys)
        -> storage::series_query;

    /**
     * @brief Extract instance query from DICOM dataset
     */
    [[nodiscard]] static auto extract_instance_query(const core::dicom_dataset& keys)
        -> storage::instance_query;

    /// Database cursor for streaming results
    std::unique_ptr<database_cursor> cursor_;

    /// Query level for this stream
    query_level level_;

    /// Original query keys (for filtering response fields)
    core::dicom_dataset query_keys_;

    /// Stream configuration
    stream_config config_;

    /// Cached total count (if computed)
    mutable std::optional<size_t> total_count_;
};

#endif  // PACS_WITH_DATABASE_SYSTEM

}  // namespace pacs::services
