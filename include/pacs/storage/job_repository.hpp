/**
 * @file job_repository.hpp
 * @brief Repository for job persistence
 *
 * This file provides the job_repository class for persisting job records
 * in the SQLite database. Supports CRUD operations, status updates, and
 * progress tracking.
 *
 * @see Issue #537 - Implement Job Manager for Async DICOM Operations
 * @see Issue #552 - Part 1: Job Types and Repository Implementation
 */

#pragma once

#ifdef PACS_WITH_DATABASE_SYSTEM


#include "pacs/client/job_types.hpp"

#include <kcenon/common/patterns/result.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Forward declaration
struct sqlite3;

namespace pacs::storage {

/// Result type alias
template <typename T>
using Result = kcenon::common::Result<T>;

/// Void result type alias
using VoidResult = kcenon::common::VoidResult;

/**
 * @brief Query options for listing jobs
 */
struct job_query_options {
    std::optional<client::job_status> status;  ///< Filter by status
    std::optional<client::job_type> type;      ///< Filter by type
    std::optional<std::string> node_id;        ///< Filter by source or destination node
    std::optional<std::string> created_by;     ///< Filter by creator
    size_t limit{100};                         ///< Maximum results
    size_t offset{0};                          ///< Result offset for pagination
    bool order_by_priority{true};              ///< Order by priority (desc) then created_at
};

/**
 * @brief Repository for job persistence
 *
 * Provides database operations for storing and retrieving job records.
 * Uses SQLite for persistence.
 *
 * Thread Safety:
 * - This class is NOT thread-safe. External synchronization is required
 *   for concurrent access.
 *
 * @example
 * @code
 * auto repo = std::make_shared<job_repository>(db_handle);
 *
 * // Save a job
 * client::job_record job;
 * job.job_id = generate_uuid();
 * job.type = client::job_type::retrieve;
 * job.source_node_id = "external-pacs";
 * repo->save(job);
 *
 * // Find a job
 * auto found = repo->find_by_id(job.job_id);
 * if (found) {
 *     std::cout << "Status: " << to_string(found->status) << std::endl;
 * }
 *
 * // Update progress
 * client::job_progress progress;
 * progress.total_items = 100;
 * progress.completed_items = 50;
 * repo->update_progress(job.job_id, progress);
 * @endcode
 */
class job_repository {
public:
    // =========================================================================
    // Construction / Destruction
    // =========================================================================

    /**
     * @brief Construct repository with SQLite handle
     *
     * @param db SQLite database handle (must remain valid for repository lifetime)
     */
    explicit job_repository(sqlite3* db);

    /**
     * @brief Destructor
     */
    ~job_repository();

    // Non-copyable, movable
    job_repository(const job_repository&) = delete;
    auto operator=(const job_repository&) -> job_repository& = delete;
    job_repository(job_repository&&) noexcept;
    auto operator=(job_repository&&) noexcept -> job_repository&;

    // =========================================================================
    // CRUD Operations
    // =========================================================================

    /**
     * @brief Save a job record
     *
     * If the job already exists (by job_id), updates it.
     * Otherwise, inserts a new record.
     *
     * @param job The job record to save
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto save(const client::job_record& job) -> VoidResult;

    /**
     * @brief Find a job by its unique ID
     *
     * @param job_id The job ID to search for
     * @return Optional containing the job if found
     */
    [[nodiscard]] auto find_by_id(std::string_view job_id) const
        -> std::optional<client::job_record>;

    /**
     * @brief Find a job by primary key
     *
     * @param pk The primary key
     * @return Optional containing the job if found
     */
    [[nodiscard]] auto find_by_pk(int64_t pk) const
        -> std::optional<client::job_record>;

    /**
     * @brief List jobs with query options
     *
     * @param options Query options for filtering and pagination
     * @return Vector of matching jobs
     */
    [[nodiscard]] auto find_jobs(const job_query_options& options = {}) const
        -> std::vector<client::job_record>;

    /**
     * @brief Find jobs by status
     *
     * @param status The status to filter by
     * @param limit Maximum results
     * @return Vector of jobs with the specified status
     */
    [[nodiscard]] auto find_by_status(client::job_status status, size_t limit = 100) const
        -> std::vector<client::job_record>;

    /**
     * @brief Find pending jobs ordered by priority
     *
     * Returns jobs in pending or queued status, ordered by priority (desc)
     * and created_at (asc) for FIFO within same priority.
     *
     * @param limit Maximum results
     * @return Vector of pending jobs
     */
    [[nodiscard]] auto find_pending_jobs(size_t limit = 10) const
        -> std::vector<client::job_record>;

    /**
     * @brief Find jobs by node ID (source or destination)
     *
     * @param node_id The node ID to search for
     * @return Vector of jobs involving the specified node
     */
    [[nodiscard]] auto find_by_node(std::string_view node_id) const
        -> std::vector<client::job_record>;

    /**
     * @brief Delete a job by ID
     *
     * @param job_id The job ID to delete
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto remove(std::string_view job_id) -> VoidResult;

    /**
     * @brief Delete completed jobs older than specified age
     *
     * @param max_age Maximum age of completed jobs to keep
     * @return Number of deleted jobs or error
     */
    [[nodiscard]] auto cleanup_old_jobs(std::chrono::hours max_age)
        -> Result<size_t>;

    /**
     * @brief Check if a job exists
     *
     * @param job_id The job ID to check
     * @return true if the job exists
     */
    [[nodiscard]] auto exists(std::string_view job_id) const -> bool;

    // =========================================================================
    // Status Updates
    // =========================================================================

    /**
     * @brief Update job status
     *
     * @param job_id The job ID to update
     * @param status New status
     * @param error_message Error message (for failed status)
     * @param error_details Detailed error information
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto update_status(
        std::string_view job_id,
        client::job_status status,
        std::string_view error_message = "",
        std::string_view error_details = "") -> VoidResult;

    /**
     * @brief Update job progress
     *
     * @param job_id The job ID to update
     * @param progress New progress information
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto update_progress(
        std::string_view job_id,
        const client::job_progress& progress) -> VoidResult;

    /**
     * @brief Mark job as started
     *
     * Updates status to running and sets started_at timestamp.
     *
     * @param job_id The job ID to update
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto mark_started(std::string_view job_id) -> VoidResult;

    /**
     * @brief Mark job as completed
     *
     * Updates status to completed and sets completed_at timestamp.
     *
     * @param job_id The job ID to update
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto mark_completed(std::string_view job_id) -> VoidResult;

    /**
     * @brief Mark job as failed
     *
     * Updates status to failed, sets error message, and increments retry count.
     *
     * @param job_id The job ID to update
     * @param error_message Error message
     * @param error_details Detailed error information
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto mark_failed(
        std::string_view job_id,
        std::string_view error_message,
        std::string_view error_details = "") -> VoidResult;

    /**
     * @brief Increment retry count
     *
     * @param job_id The job ID to update
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto increment_retry(std::string_view job_id) -> VoidResult;

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get total job count
     *
     * @return Number of jobs in the repository
     */
    [[nodiscard]] auto count() const -> size_t;

    /**
     * @brief Get job count by status
     *
     * @param status The status to count
     * @return Number of jobs with the specified status
     */
    [[nodiscard]] auto count_by_status(client::job_status status) const -> size_t;

    /**
     * @brief Get jobs completed today
     *
     * @return Number of jobs completed today
     */
    [[nodiscard]] auto count_completed_today() const -> size_t;

    /**
     * @brief Get jobs failed today
     *
     * @return Number of jobs failed today
     */
    [[nodiscard]] auto count_failed_today() const -> size_t;

    // =========================================================================
    // Database Information
    // =========================================================================

    /**
     * @brief Check if the database connection is valid
     *
     * @return true if the database handle is valid
     */
    [[nodiscard]] auto is_valid() const noexcept -> bool;

private:
    // =========================================================================
    // Private Implementation
    // =========================================================================

    /**
     * @brief Parse a job record from a prepared statement
     */
    [[nodiscard]] auto parse_row(void* stmt) const -> client::job_record;

    /**
     * @brief Serialize instance_uids vector to JSON
     */
    [[nodiscard]] static auto serialize_instance_uids(
        const std::vector<std::string>& uids) -> std::string;

    /**
     * @brief Deserialize instance_uids from JSON
     */
    [[nodiscard]] static auto deserialize_instance_uids(
        std::string_view json) -> std::vector<std::string>;

    /**
     * @brief Serialize metadata map to JSON
     */
    [[nodiscard]] static auto serialize_metadata(
        const std::unordered_map<std::string, std::string>& metadata) -> std::string;

    /**
     * @brief Deserialize metadata from JSON
     */
    [[nodiscard]] static auto deserialize_metadata(
        std::string_view json) -> std::unordered_map<std::string, std::string>;

    /// SQLite database handle
    sqlite3* db_{nullptr};
};

}  // namespace pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
