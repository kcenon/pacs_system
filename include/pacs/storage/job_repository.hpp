/**
 * @file job_repository.hpp
 * @brief Repository for job persistence using base_repository pattern
 *
 * This file provides the job_repository class for persisting job records
 * using the base_repository pattern. Supports CRUD operations, status updates,
 * and progress tracking.
 *
 * @see Issue #537 - Implement Job Manager for Async DICOM Operations
 * @see Issue #552 - Part 1: Job Types and Repository Implementation
 * @see Issue #610 - Phase 4: Repository Migrations
 * @see Issue #649 - Part 1: Migrate job_repository
 */

#pragma once

#include "pacs/client/job_types.hpp"

#include <kcenon/common/patterns/result.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifdef PACS_WITH_DATABASE_SYSTEM

#include "pacs/storage/base_repository.hpp"

namespace pacs::storage {

/**
 * @brief Query options for listing jobs
 */
struct job_query_options {
    std::optional<client::job_status> status;  ///< Filter by status
    std::optional<client::job_type> type;      ///< Filter by type
    std::optional<std::string> node_id;  ///< Filter by source or destination node
    std::optional<std::string> created_by;  ///< Filter by creator
    size_t limit{100};                         ///< Maximum results
    size_t offset{0};                 ///< Result offset for pagination
    bool order_by_priority{true};     ///< Order by priority (desc) then created_at
};

/**
 * @brief Repository for job persistence using base_repository pattern
 *
 * Extends base_repository to inherit standard CRUD operations.
 * Provides database operations for storing and retrieving job records.
 *
 * Thread Safety:
 * - This class is NOT thread-safe. External synchronization is required
 *   for concurrent access.
 *
 * @example
 * @code
 * auto db = std::make_shared<pacs_database_adapter>("pacs.db");
 * db->connect();
 * auto repo = job_repository(db);
 *
 * client::job_record job;
 * job.job_id = generate_uuid();
 * job.type = client::job_type::retrieve;
 * job.source_node_id = "external-pacs";
 * repo.save(job);
 *
 * auto result = repo.find_by_id(job.job_id);
 * if (result.is_ok()) {
 *     auto& found = result.value();
 *     std::cout << "Status: " << to_string(found.status) << std::endl;
 * }
 * @endcode
 */
class job_repository
    : public base_repository<client::job_record, std::string> {
public:
    explicit job_repository(std::shared_ptr<pacs_database_adapter> db);
    ~job_repository() override = default;

    job_repository(const job_repository&) = delete;
    auto operator=(const job_repository&) -> job_repository& = delete;
    job_repository(job_repository&&) noexcept = default;
    auto operator=(job_repository&&) noexcept -> job_repository& = default;

    // =========================================================================
    // Domain-Specific Operations
    // =========================================================================

    /**
     * @brief Find a job by primary key (integer pk)
     *
     * @param pk The primary key (integer)
     * @return Result containing the job if found, or error
     */
    [[nodiscard]] auto find_by_pk(int64_t pk) -> result_type;

    /**
     * @brief List jobs with query options
     *
     * @param options Query options for filtering and pagination
     * @return Result containing vector of matching jobs or error
     */
    [[nodiscard]] auto find_jobs(const job_query_options& options = {})
        -> list_result_type;

    /**
     * @brief Find jobs by status
     *
     * @param status The status to filter by
     * @param limit Maximum results
     * @return Result containing vector of jobs with the specified status
     */
    [[nodiscard]] auto find_by_status(client::job_status status,
                                      size_t limit = 100) -> list_result_type;

    /**
     * @brief Find pending jobs ordered by priority
     *
     * Returns jobs in pending or queued status, ordered by priority (desc)
     * and created_at (asc) for FIFO within same priority.
     *
     * @param limit Maximum results
     * @return Result containing vector of pending jobs
     */
    [[nodiscard]] auto find_pending_jobs(size_t limit = 10) -> list_result_type;

    /**
     * @brief Find jobs by node ID (source or destination)
     *
     * @param node_id The node ID to search for
     * @return Result containing vector of jobs involving the specified node
     */
    [[nodiscard]] auto find_by_node(std::string_view node_id)
        -> list_result_type;

    /**
     * @brief Delete completed jobs older than specified age
     *
     * @param max_age Maximum age of completed jobs to keep
     * @return Result containing number of deleted jobs or error
     */
    [[nodiscard]] auto cleanup_old_jobs(std::chrono::hours max_age)
        -> Result<size_t>;

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
    [[nodiscard]] auto update_status(std::string_view job_id,
                                     client::job_status status,
                                     std::string_view error_message = "",
                                     std::string_view error_details = "")
        -> VoidResult;

    /**
     * @brief Update job progress
     *
     * @param job_id The job ID to update
     * @param progress New progress information
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto update_progress(std::string_view job_id,
                                       const client::job_progress& progress)
        -> VoidResult;

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
    [[nodiscard]] auto mark_failed(std::string_view job_id,
                                   std::string_view error_message,
                                   std::string_view error_details = "")
        -> VoidResult;

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
     * @brief Get job count by status
     *
     * @param status The status to count
     * @return Result containing number of jobs with the specified status
     */
    [[nodiscard]] auto count_by_status(client::job_status status)
        -> Result<size_t>;

    /**
     * @brief Get jobs completed today
     *
     * @return Result containing number of jobs completed today
     */
    [[nodiscard]] auto count_completed_today() -> Result<size_t>;

    /**
     * @brief Get jobs failed today
     *
     * @return Result containing number of jobs failed today
     */
    [[nodiscard]] auto count_failed_today() -> Result<size_t>;

protected:
    // =========================================================================
    // base_repository overrides
    // =========================================================================

    [[nodiscard]] auto map_row_to_entity(const database_row& row) const
        -> client::job_record override;

    [[nodiscard]] auto entity_to_row(const client::job_record& entity) const
        -> std::map<std::string, database_value> override;

    [[nodiscard]] auto get_pk(const client::job_record& entity) const
        -> std::string override;

    [[nodiscard]] auto has_pk(const client::job_record& entity) const
        -> bool override;

    [[nodiscard]] auto select_columns() const
        -> std::vector<std::string> override;

private:
    [[nodiscard]] auto parse_timestamp(const std::string& str) const
        -> std::chrono::system_clock::time_point;

    [[nodiscard]] auto format_timestamp(
        std::chrono::system_clock::time_point tp) const -> std::string;

    [[nodiscard]] auto format_optional_timestamp(
        const std::optional<std::chrono::system_clock::time_point>& tp) const
        -> std::string;

    [[nodiscard]] static auto serialize_instance_uids(
        const std::vector<std::string>& uids) -> std::string;

    [[nodiscard]] static auto deserialize_instance_uids(std::string_view json)
        -> std::vector<std::string>;

    [[nodiscard]] static auto serialize_metadata(
        const std::unordered_map<std::string, std::string>& metadata)
        -> std::string;

    [[nodiscard]] static auto deserialize_metadata(std::string_view json)
        -> std::unordered_map<std::string, std::string>;
};

}  // namespace pacs::storage

#else  // !PACS_WITH_DATABASE_SYSTEM

// Legacy interface for builds without database_system
struct sqlite3;

namespace pacs::storage {

template <typename T>
using Result = kcenon::common::Result<T>;

using VoidResult = kcenon::common::VoidResult;

/**
 * @brief Query options for listing jobs
 */
struct job_query_options {
    std::optional<client::job_status> status;  ///< Filter by status
    std::optional<client::job_type> type;      ///< Filter by type
    std::optional<std::string> node_id;  ///< Filter by source or destination node
    std::optional<std::string> created_by;  ///< Filter by creator
    size_t limit{100};                         ///< Maximum results
    size_t offset{0};                 ///< Result offset for pagination
    bool order_by_priority{true};     ///< Order by priority (desc) then created_at
};

/**
 * @brief Repository for job persistence (legacy SQLite interface)
 *
 * This is the legacy interface maintained for builds without database_system.
 * New code should use the base_repository version when PACS_WITH_DATABASE_SYSTEM
 * is defined.
 */
class job_repository {
public:
    explicit job_repository(sqlite3* db);
    ~job_repository();

    job_repository(const job_repository&) = delete;
    auto operator=(const job_repository&) -> job_repository& = delete;
    job_repository(job_repository&&) noexcept;
    auto operator=(job_repository&&) noexcept -> job_repository&;

    [[nodiscard]] auto save(const client::job_record& job) -> VoidResult;
    [[nodiscard]] auto find_by_id(std::string_view job_id) const
        -> std::optional<client::job_record>;
    [[nodiscard]] auto find_by_pk(int64_t pk) const
        -> std::optional<client::job_record>;
    [[nodiscard]] auto find_jobs(const job_query_options& options = {}) const
        -> std::vector<client::job_record>;
    [[nodiscard]] auto find_by_status(client::job_status status,
                                      size_t limit = 100) const
        -> std::vector<client::job_record>;
    [[nodiscard]] auto find_pending_jobs(size_t limit = 10) const
        -> std::vector<client::job_record>;
    [[nodiscard]] auto find_by_node(std::string_view node_id) const
        -> std::vector<client::job_record>;
    [[nodiscard]] auto remove(std::string_view job_id) -> VoidResult;
    [[nodiscard]] auto cleanup_old_jobs(std::chrono::hours max_age)
        -> Result<size_t>;
    [[nodiscard]] auto exists(std::string_view job_id) const -> bool;

    [[nodiscard]] auto update_status(std::string_view job_id,
                                     client::job_status status,
                                     std::string_view error_message = "",
                                     std::string_view error_details = "")
        -> VoidResult;
    [[nodiscard]] auto update_progress(std::string_view job_id,
                                       const client::job_progress& progress)
        -> VoidResult;
    [[nodiscard]] auto mark_started(std::string_view job_id) -> VoidResult;
    [[nodiscard]] auto mark_completed(std::string_view job_id) -> VoidResult;
    [[nodiscard]] auto mark_failed(std::string_view job_id,
                                   std::string_view error_message,
                                   std::string_view error_details = "")
        -> VoidResult;
    [[nodiscard]] auto increment_retry(std::string_view job_id) -> VoidResult;

    [[nodiscard]] auto count() const -> size_t;
    [[nodiscard]] auto count_by_status(client::job_status status) const
        -> size_t;
    [[nodiscard]] auto count_completed_today() const -> size_t;
    [[nodiscard]] auto count_failed_today() const -> size_t;

    [[nodiscard]] auto is_valid() const noexcept -> bool;

private:
    [[nodiscard]] auto parse_row(void* stmt) const -> client::job_record;

    [[nodiscard]] static auto serialize_instance_uids(
        const std::vector<std::string>& uids) -> std::string;

    [[nodiscard]] static auto deserialize_instance_uids(std::string_view json)
        -> std::vector<std::string>;

    [[nodiscard]] static auto serialize_metadata(
        const std::unordered_map<std::string, std::string>& metadata)
        -> std::string;

    [[nodiscard]] static auto deserialize_metadata(std::string_view json)
        -> std::unordered_map<std::string, std::string>;

    sqlite3* db_{nullptr};
};

}  // namespace pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
