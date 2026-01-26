/**
 * @file viewer_state_repository.hpp
 * @brief Repository for viewer state persistence
 *
 * This file provides the viewer_state_repository class for persisting viewer state
 * and recent study records in the SQLite database.
 *
 * @see Issue #545 - Implement Annotation & Measurement APIs
 * @see Issue #581 - Part 1: Data Models and Repositories
 */

#pragma once

#ifdef PACS_WITH_DATABASE_SYSTEM


#include "pacs/storage/viewer_state_record.hpp"

#include <kcenon/common/patterns/result.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct sqlite3;

namespace pacs::storage {

template <typename T>
using Result = kcenon::common::Result<T>;

using VoidResult = kcenon::common::VoidResult;

/**
 * @brief Repository for viewer state and recent studies persistence
 *
 * Provides database operations for storing and retrieving viewer state records
 * and recent study access history. Uses SQLite for persistence.
 *
 * Thread Safety:
 * - This class is NOT thread-safe. External synchronization is required
 *   for concurrent access.
 *
 * @example
 * @code
 * auto repo = viewer_state_repository(db_handle);
 *
 * viewer_state_record state;
 * state.state_id = generate_uuid();
 * state.study_uid = "1.2.3.4.5";
 * state.user_id = "user123";
 * state.state_json = R"({"layout":{"rows":2,"cols":2}})";
 * repo.save_state(state);
 *
 * auto states = repo.find_states_by_study("1.2.3.4.5");
 * @endcode
 */
class viewer_state_repository {
public:
    explicit viewer_state_repository(sqlite3* db);
    ~viewer_state_repository();

    viewer_state_repository(const viewer_state_repository&) = delete;
    auto operator=(const viewer_state_repository&) -> viewer_state_repository& = delete;
    viewer_state_repository(viewer_state_repository&&) noexcept;
    auto operator=(viewer_state_repository&&) noexcept -> viewer_state_repository&;

    // =========================================================================
    // Viewer State Operations
    // =========================================================================

    /**
     * @brief Save a viewer state record
     *
     * If the state already exists (by state_id), updates it.
     * Otherwise, inserts a new record.
     *
     * @param record The viewer state record to save
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto save_state(const viewer_state_record& record) -> VoidResult;

    /**
     * @brief Find a viewer state by its unique ID
     *
     * @param state_id The state ID to search for
     * @return Optional containing the state if found
     */
    [[nodiscard]] auto find_state_by_id(std::string_view state_id) const
        -> std::optional<viewer_state_record>;

    /**
     * @brief Find viewer states by Study UID
     *
     * @param study_uid The Study UID to filter by
     * @return Vector of states for the specified study
     */
    [[nodiscard]] auto find_states_by_study(std::string_view study_uid) const
        -> std::vector<viewer_state_record>;

    /**
     * @brief Search viewer states with query options
     *
     * @param query Query options for filtering and pagination
     * @return Vector of matching states
     */
    [[nodiscard]] auto search_states(const viewer_state_query& query) const
        -> std::vector<viewer_state_record>;

    /**
     * @brief Delete a viewer state by ID
     *
     * @param state_id The state ID to delete
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto remove_state(std::string_view state_id) -> VoidResult;

    /**
     * @brief Get viewer state count
     *
     * @return Number of states in the repository
     */
    [[nodiscard]] auto count_states() const -> size_t;

    // =========================================================================
    // Recent Studies Operations
    // =========================================================================

    /**
     * @brief Record a study access
     *
     * Updates the access timestamp if the study was already accessed,
     * otherwise inserts a new record.
     *
     * @param user_id The user who accessed the study
     * @param study_uid The Study UID that was accessed
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto record_study_access(
        std::string_view user_id,
        std::string_view study_uid) -> VoidResult;

    /**
     * @brief Get recent studies for a user
     *
     * @param user_id The user to get recent studies for
     * @param limit Maximum number of studies to return (default: 20)
     * @return Vector of recent study records ordered by access time (most recent first)
     */
    [[nodiscard]] auto get_recent_studies(
        std::string_view user_id,
        size_t limit = 20) const -> std::vector<recent_study_record>;

    /**
     * @brief Clear recent studies history for a user
     *
     * @param user_id The user to clear history for
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto clear_recent_studies(std::string_view user_id) -> VoidResult;

    /**
     * @brief Get recent studies count for a user
     *
     * @param user_id The user to count recent studies for
     * @return Number of recent studies
     */
    [[nodiscard]] auto count_recent_studies(std::string_view user_id) const -> size_t;

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
    [[nodiscard]] auto parse_state_row(void* stmt) const -> viewer_state_record;
    [[nodiscard]] auto parse_recent_study_row(void* stmt) const -> recent_study_record;

    sqlite3* db_{nullptr};
};

}  // namespace pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
