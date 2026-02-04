/**
 * @file recent_study_repository.hpp
 * @brief Repository for recent study records using base_repository pattern
 *
 * This file provides the recent_study_repository class that extends
 * base_repository for standardized CRUD operations on recent study records.
 *
 * @see Issue #610 - Phase 4: Repository Migrations
 * @see Issue #607 - Phase 2: Base Repository Pattern Implementation
 */

#pragma once

#include "pacs/storage/base_repository.hpp"
#include "pacs/storage/viewer_state_record.hpp"

#ifdef PACS_WITH_DATABASE_SYSTEM

namespace pacs::storage {

/**
 * @brief Repository for recent study records using base_repository pattern
 *
 * Provides CRUD operations for recent_study_record entities using the
 * standardized base_repository interface. Tracks which studies users have
 * recently accessed for quick navigation in the viewer.
 *
 * Thread Safety:
 * - This class is NOT thread-safe. External synchronization is required
 *   for concurrent access.
 *
 * @example
 * @code
 * auto db = std::make_shared<pacs_database_adapter>("pacs.db");
 * db->connect();
 * recent_study_repository repo(db);
 *
 * // Record a study access
 * auto result = repo.record_access("user123", "1.2.3.4.5");
 *
 * // Get recent studies for a user
 * auto recent = repo.find_by_user("user123", 20);
 * @endcode
 */
class recent_study_repository
    : public base_repository<recent_study_record, int64_t> {
public:
    /**
     * @brief Construct repository with database adapter
     *
     * @param db Shared pointer to database adapter (must be connected)
     */
    explicit recent_study_repository(std::shared_ptr<pacs_database_adapter> db);

    // =========================================================================
    // Domain-Specific Queries
    // =========================================================================

    /**
     * @brief Record a study access (insert or update)
     *
     * Updates the access timestamp if the study was already accessed,
     * otherwise inserts a new record.
     *
     * @param user_id The user who accessed the study
     * @param study_uid The Study UID that was accessed
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto record_access(
        std::string_view user_id,
        std::string_view study_uid) -> VoidResult;

    /**
     * @brief Find recent studies by user ID
     *
     * @param user_id The user ID to filter by
     * @param limit Maximum number of studies to return
     * @return Result containing vector of recent studies (most recent first)
     */
    [[nodiscard]] auto find_by_user(
        std::string_view user_id,
        size_t limit = 20) -> list_result_type;

    /**
     * @brief Clear recent studies history for a user
     *
     * @param user_id The user to clear history for
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto clear_for_user(std::string_view user_id) -> VoidResult;

    /**
     * @brief Count recent studies for a user
     *
     * @param user_id The user to count recent studies for
     * @return Result containing the count
     */
    [[nodiscard]] auto count_for_user(std::string_view user_id)
        -> Result<size_t>;

    /**
     * @brief Check if a study was recently accessed by a user
     *
     * @param user_id The user ID
     * @param study_uid The Study UID
     * @return Result containing true if recently accessed
     */
    [[nodiscard]] auto was_recently_accessed(
        std::string_view user_id,
        std::string_view study_uid) -> Result<bool>;

protected:
    // =========================================================================
    // base_repository Overrides
    // =========================================================================

    [[nodiscard]] auto map_row_to_entity(const database_row& row) const
        -> recent_study_record override;

    [[nodiscard]] auto entity_to_row(const recent_study_record& entity) const
        -> std::map<std::string, database_value> override;

    [[nodiscard]] auto get_pk(const recent_study_record& entity) const
        -> int64_t override;

    [[nodiscard]] auto has_pk(const recent_study_record& entity) const
        -> bool override;

    [[nodiscard]] auto select_columns() const
        -> std::vector<std::string> override;

private:
    [[nodiscard]] auto parse_timestamp(const std::string& str) const
        -> std::chrono::system_clock::time_point;

    [[nodiscard]] auto format_timestamp(
        std::chrono::system_clock::time_point tp) const -> std::string;
};

}  // namespace pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
