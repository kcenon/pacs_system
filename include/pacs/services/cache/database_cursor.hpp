/**
 * @file database_cursor.hpp
 * @brief Database cursor for streaming query results
 *
 * This file provides the database_cursor class for lazy evaluation of
 * database query results. Instead of loading all results into memory,
 * the cursor allows fetching results one batch at a time.
 *
 * Uses database_system's query builder for type-safe query construction.
 *
 * @see SRS-SVC-006, FR-5.3
 * @see Issue #642 - Unified implementation using database_system
 * @see Issue #420 - Migrate database_cursor to database_system
 */

#pragma once

#include <pacs/storage/instance_record.hpp>
#include <pacs/storage/patient_record.hpp>
#include <pacs/storage/series_record.hpp>
#include <pacs/storage/study_record.hpp>

#include <kcenon/common/patterns/result.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

// Suppress deprecated warnings from database_system headers
// database_base is deprecated in favor of database_backend
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include <database/database_manager.h>
#include <database/query_builder.h>

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace pacs::services {

/// Result type alias for operations returning a value
template <typename T>
using Result = kcenon::common::Result<T>;

/// Result type alias for void operations
using VoidResult = kcenon::common::VoidResult;

/**
 * @brief Query record type variant for multi-level queries
 *
 * Represents any of the DICOM hierarchical record types that can be
 * returned from a query operation.
 */
using query_record = std::variant<storage::patient_record, storage::study_record,
                                  storage::series_record, storage::instance_record>;

/**
 * @brief Database cursor for streaming query results
 *
 * Provides lazy evaluation of database query results using SQLite's
 * prepared statement API. Results are fetched one at a time, reducing
 * memory usage for large result sets.
 *
 * Thread Safety: This class is NOT thread-safe. The cursor should be
 * used from a single thread, and the underlying database connection
 * must remain valid for the cursor's lifetime.
 *
 * @example
 * @code
 * // Create cursor from a query
 * auto cursor_result = database_cursor::create_study_cursor(db, query);
 * if (cursor_result.is_err()) {
 *     // Handle error
 *     return;
 * }
 *
 * auto cursor = std::move(cursor_result.value());
 *
 * // Fetch results in batches
 * while (cursor->has_more()) {
 *     auto batch = cursor->fetch_batch(100);
 *     for (const auto& record : batch) {
 *         // Process record
 *     }
 * }
 * @endcode
 */
class database_cursor {
public:
    /**
     * @brief Record type enumeration for cursor type identification
     */
    enum class record_type { patient, study, series, instance };

    /**
     * @brief Destructor - finalizes the SQLite statement
     */
    ~database_cursor();

    // Non-copyable
    database_cursor(const database_cursor&) = delete;
    auto operator=(const database_cursor&) -> database_cursor& = delete;

    // Movable
    database_cursor(database_cursor&& other) noexcept;
    auto operator=(database_cursor&& other) noexcept -> database_cursor&;

    // =========================================================================
    // Factory Methods
    // =========================================================================

    /**
     * @brief Create a cursor for patient queries using database_system
     *
     * @param db Database manager instance
     * @param query Query parameters for patient search
     * @return Result containing the cursor or error
     */
    [[nodiscard]] static auto create_patient_cursor(
        std::shared_ptr<database::database_manager> db,
        const storage::patient_query& query) -> Result<std::unique_ptr<database_cursor>>;

    /**
     * @brief Create a cursor for study queries using database_system
     *
     * @param db Database manager instance
     * @param query Query parameters for study search
     * @return Result containing the cursor or error
     */
    [[nodiscard]] static auto create_study_cursor(
        std::shared_ptr<database::database_manager> db,
        const storage::study_query& query) -> Result<std::unique_ptr<database_cursor>>;

    /**
     * @brief Create a cursor for series queries using database_system
     *
     * @param db Database manager instance
     * @param query Query parameters for series search
     * @return Result containing the cursor or error
     */
    [[nodiscard]] static auto create_series_cursor(
        std::shared_ptr<database::database_manager> db,
        const storage::series_query& query) -> Result<std::unique_ptr<database_cursor>>;

    /**
     * @brief Create a cursor for instance queries using database_system
     *
     * @param db Database manager instance
     * @param query Query parameters for instance search
     * @return Result containing the cursor or error
     */
    [[nodiscard]] static auto create_instance_cursor(
        std::shared_ptr<database::database_manager> db,
        const storage::instance_query& query) -> Result<std::unique_ptr<database_cursor>>;

    // =========================================================================
    // Cursor Operations
    // =========================================================================

    /**
     * @brief Check if there are more results available
     *
     * @return true if more results can be fetched
     */
    [[nodiscard]] auto has_more() const noexcept -> bool;

    /**
     * @brief Fetch the next result
     *
     * Advances the cursor and returns the next record.
     *
     * @return Optional containing the next record, or nullopt if exhausted
     */
    [[nodiscard]] auto fetch_next() -> std::optional<query_record>;

    /**
     * @brief Fetch the next batch of results
     *
     * Fetches up to `batch_size` results from the cursor.
     *
     * @param batch_size Maximum number of results to fetch
     * @return Vector of records (may be smaller than batch_size if exhausted)
     */
    [[nodiscard]] auto fetch_batch(size_t batch_size) -> std::vector<query_record>;

    /**
     * @brief Get the current position in the result set
     *
     * @return Number of records that have been fetched so far
     */
    [[nodiscard]] auto position() const noexcept -> size_t;

    /**
     * @brief Get the record type for this cursor
     *
     * @return The type of records this cursor returns
     */
    [[nodiscard]] auto type() const noexcept -> record_type;

    /**
     * @brief Reset the cursor to the beginning
     *
     * Allows re-iterating through the results from the start.
     *
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto reset() -> VoidResult;

    // =========================================================================
    // Cursor State
    // =========================================================================

    /**
     * @brief Serialize the cursor state for resumption
     *
     * Creates a string representation of the current cursor position
     * that can be used to resume from this point later.
     *
     * @return Serialized cursor state
     */
    [[nodiscard]] auto serialize() const -> std::string;

private:
    /**
     * @brief Private constructor - use factory methods
     */
    database_cursor(std::vector<query_record> results, record_type type);

    /**
     * @brief Parse a patient record from database row
     */
    [[nodiscard]] static auto parse_patient_row(
        const database::core::database_row& row) -> storage::patient_record;

    /**
     * @brief Parse a study record from database row
     */
    [[nodiscard]] static auto parse_study_row(
        const database::core::database_row& row) -> storage::study_record;

    /**
     * @brief Parse a series record from database row
     */
    [[nodiscard]] static auto parse_series_row(
        const database::core::database_row& row) -> storage::series_record;

    /**
     * @brief Parse an instance record from database row
     */
    [[nodiscard]] static auto parse_instance_row(
        const database::core::database_row& row) -> storage::instance_record;

    /**
     * @brief Apply DICOM wildcard conditions to query builder
     */
    static void apply_dicom_condition(database::query_builder& builder,
                                      const std::string& field,
                                      const std::string& value);

    /// Cached results from database query
    std::vector<query_record> results_;

    /**
     * @brief Convert DICOM wildcard pattern to SQL LIKE pattern
     *
     * DICOM wildcards: '*' matches any sequence, '?' matches single character
     * SQL wildcards: '%' matches any sequence, '_' matches single character
     */
    [[nodiscard]] static auto to_like_pattern(std::string_view pattern) -> std::string;

    /**
     * @brief Check if pattern contains DICOM wildcards
     */
    [[nodiscard]] static auto contains_dicom_wildcards(std::string_view pattern) -> bool;

    /// Record type for this cursor
    record_type type_;

    /// Current position in result set
    size_t position_{0};

    /// Whether more results are available
    bool has_more_{true};

    /// Whether the cursor has been stepped at least once
    bool stepped_{false};
};

}  // namespace pacs::services
