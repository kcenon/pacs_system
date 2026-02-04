/**
 * @file viewer_state_record_repository.hpp
 * @brief Repository for viewer state records using base_repository pattern
 *
 * This file provides the viewer_state_record_repository class that extends
 * base_repository for standardized CRUD operations on viewer state records.
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
 * @brief Repository for viewer state records using base_repository pattern
 *
 * Provides CRUD operations for viewer_state_record entities using the
 * standardized base_repository interface. Domain-specific queries are
 * implemented as extensions to the base functionality.
 *
 * Thread Safety:
 * - This class is NOT thread-safe. External synchronization is required
 *   for concurrent access.
 *
 * @example
 * @code
 * auto db = std::make_shared<pacs_database_adapter>("pacs.db");
 * db->connect();
 * viewer_state_record_repository repo(db);
 *
 * // Save a state
 * viewer_state_record state;
 * state.state_id = "uuid-1234";
 * state.study_uid = "1.2.3.4.5";
 * state.user_id = "user123";
 * state.state_json = R"({"layout":{"rows":2}})";
 * auto result = repo.save(state);
 *
 * // Find by ID
 * auto found = repo.find_by_id("uuid-1234");
 * @endcode
 */
class viewer_state_record_repository
    : public base_repository<viewer_state_record, std::string> {
public:
    /**
     * @brief Construct repository with database adapter
     *
     * @param db Shared pointer to database adapter (must be connected)
     */
    explicit viewer_state_record_repository(
        std::shared_ptr<pacs_database_adapter> db);

    // =========================================================================
    // Domain-Specific Queries
    // =========================================================================

    /**
     * @brief Find viewer states by Study UID
     *
     * @param study_uid The Study UID to filter by
     * @return Result containing vector of states for the specified study
     */
    [[nodiscard]] auto find_by_study(std::string_view study_uid)
        -> list_result_type;

    /**
     * @brief Find viewer states by user ID
     *
     * @param user_id The user ID to filter by
     * @return Result containing vector of states for the specified user
     */
    [[nodiscard]] auto find_by_user(std::string_view user_id)
        -> list_result_type;

    /**
     * @brief Find viewer states by Study UID and user ID
     *
     * @param study_uid The Study UID to filter by
     * @param user_id The user ID to filter by
     * @return Result containing vector of matching states
     */
    [[nodiscard]] auto find_by_study_and_user(
        std::string_view study_uid,
        std::string_view user_id) -> list_result_type;

    /**
     * @brief Search viewer states with query options
     *
     * @param query Query options for filtering and pagination
     * @return Result containing vector of matching states
     */
    [[nodiscard]] auto search(const viewer_state_query& query)
        -> list_result_type;

protected:
    // =========================================================================
    // base_repository Overrides
    // =========================================================================

    /**
     * @brief Map database row to viewer_state_record entity
     */
    [[nodiscard]] auto map_row_to_entity(const database_row& row) const
        -> viewer_state_record override;

    /**
     * @brief Map viewer_state_record entity to column-value pairs
     */
    [[nodiscard]] auto entity_to_row(const viewer_state_record& entity) const
        -> std::map<std::string, database_value> override;

    /**
     * @brief Get primary key value from entity
     */
    [[nodiscard]] auto get_pk(const viewer_state_record& entity) const
        -> std::string override;

    /**
     * @brief Check if entity has a valid primary key
     */
    [[nodiscard]] auto has_pk(const viewer_state_record& entity) const
        -> bool override;

    /**
     * @brief Get columns for SELECT queries
     */
    [[nodiscard]] auto select_columns() const
        -> std::vector<std::string> override;

private:
    /**
     * @brief Parse timestamp string to time_point
     */
    [[nodiscard]] auto parse_timestamp(const std::string& str) const
        -> std::chrono::system_clock::time_point;

    /**
     * @brief Format time_point to timestamp string
     */
    [[nodiscard]] auto format_timestamp(
        std::chrono::system_clock::time_point tp) const -> std::string;
};

}  // namespace pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
