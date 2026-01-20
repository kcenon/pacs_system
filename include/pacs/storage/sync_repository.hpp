/**
 * @file sync_repository.hpp
 * @brief Repository for sync persistence
 *
 * This file provides the sync_repository class for persisting sync
 * configurations, conflicts, and history in the SQLite database.
 *
 * @see Issue #542 - Implement Sync Manager for Bidirectional Synchronization
 * @see Issue #530 - PACS Client System Support (Parent Epic)
 */

#pragma once

#include "pacs/client/sync_types.hpp"

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
 * @brief Repository for sync persistence
 *
 * Provides database operations for storing and retrieving sync configurations,
 * conflicts, and history. Uses SQLite for persistence.
 *
 * Thread Safety:
 * - This class is NOT thread-safe. External synchronization is required
 *   for concurrent access.
 *
 * @example
 * @code
 * auto repo = std::make_shared<sync_repository>(db_handle);
 *
 * // Save a config
 * client::sync_config config;
 * config.config_id = "daily-sync";
 * config.source_node_id = "archive-server";
 * repo->save_config(config);
 *
 * // List configs
 * auto configs = repo->list_configs();
 *
 * // Save a conflict
 * client::sync_conflict conflict;
 * conflict.study_uid = "1.2.3.4.5";
 * conflict.conflict_type = client::sync_conflict_type::missing_local;
 * repo->save_conflict(conflict);
 * @endcode
 */
class sync_repository {
public:
    // =========================================================================
    // Construction / Destruction
    // =========================================================================

    /**
     * @brief Construct repository with SQLite handle
     *
     * @param db SQLite database handle (must remain valid for repository lifetime)
     */
    explicit sync_repository(sqlite3* db);

    /**
     * @brief Destructor
     */
    ~sync_repository();

    // Non-copyable, movable
    sync_repository(const sync_repository&) = delete;
    auto operator=(const sync_repository&) -> sync_repository& = delete;
    sync_repository(sync_repository&&) noexcept;
    auto operator=(sync_repository&&) noexcept -> sync_repository&;

    // =========================================================================
    // Config Operations
    // =========================================================================

    /**
     * @brief Save a sync configuration
     *
     * If the config already exists (by config_id), updates it.
     * Otherwise, inserts a new record.
     *
     * @param config The configuration to save
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto save_config(const client::sync_config& config) -> VoidResult;

    /**
     * @brief Find a config by its unique ID
     *
     * @param config_id The config ID to search for
     * @return Optional containing the config if found
     */
    [[nodiscard]] auto find_config(std::string_view config_id) const
        -> std::optional<client::sync_config>;

    /**
     * @brief List all sync configurations
     *
     * @return Vector of all configurations
     */
    [[nodiscard]] auto list_configs() const -> std::vector<client::sync_config>;

    /**
     * @brief List enabled sync configurations
     *
     * @return Vector of enabled configurations
     */
    [[nodiscard]] auto list_enabled_configs() const -> std::vector<client::sync_config>;

    /**
     * @brief Delete a config by ID
     *
     * @param config_id The config ID to delete
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto remove_config(std::string_view config_id) -> VoidResult;

    /**
     * @brief Update config statistics
     *
     * @param config_id The config ID to update
     * @param success Whether the sync was successful
     * @param studies_synced Number of studies synced
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto update_config_stats(
        std::string_view config_id,
        bool success,
        size_t studies_synced) -> VoidResult;

    // =========================================================================
    // Conflict Operations
    // =========================================================================

    /**
     * @brief Save a sync conflict
     *
     * If a conflict for the same study_uid exists, updates it.
     *
     * @param conflict The conflict to save
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto save_conflict(const client::sync_conflict& conflict) -> VoidResult;

    /**
     * @brief Find a conflict by study UID
     *
     * @param study_uid The Study Instance UID
     * @return Optional containing the conflict if found
     */
    [[nodiscard]] auto find_conflict(std::string_view study_uid) const
        -> std::optional<client::sync_conflict>;

    /**
     * @brief List all conflicts for a config
     *
     * @param config_id The config ID to filter by
     * @return Vector of conflicts for the config
     */
    [[nodiscard]] auto list_conflicts(std::string_view config_id) const
        -> std::vector<client::sync_conflict>;

    /**
     * @brief List all unresolved conflicts
     *
     * @return Vector of unresolved conflicts
     */
    [[nodiscard]] auto list_unresolved_conflicts() const
        -> std::vector<client::sync_conflict>;

    /**
     * @brief Resolve a conflict
     *
     * @param study_uid The Study Instance UID of the conflict
     * @param resolution How the conflict was resolved
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto resolve_conflict(
        std::string_view study_uid,
        client::conflict_resolution resolution) -> VoidResult;

    /**
     * @brief Delete resolved conflicts older than specified age
     *
     * @param max_age Maximum age of resolved conflicts to keep
     * @return Number of deleted conflicts or error
     */
    [[nodiscard]] auto cleanup_old_conflicts(std::chrono::hours max_age)
        -> Result<size_t>;

    // =========================================================================
    // History Operations
    // =========================================================================

    /**
     * @brief Save a sync history record
     *
     * @param history The history record to save
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto save_history(const client::sync_history& history) -> VoidResult;

    /**
     * @brief List sync history for a config
     *
     * @param config_id The config ID to filter by
     * @param limit Maximum records to return
     * @return Vector of history records
     */
    [[nodiscard]] auto list_history(std::string_view config_id, size_t limit = 100) const
        -> std::vector<client::sync_history>;

    /**
     * @brief Get last sync history for a config
     *
     * @param config_id The config ID
     * @return Optional containing the last history record
     */
    [[nodiscard]] auto get_last_history(std::string_view config_id) const
        -> std::optional<client::sync_history>;

    /**
     * @brief Delete history older than specified age
     *
     * @param max_age Maximum age of history to keep
     * @return Number of deleted records or error
     */
    [[nodiscard]] auto cleanup_old_history(std::chrono::hours max_age)
        -> Result<size_t>;

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get total config count
     *
     * @return Number of configs in the repository
     */
    [[nodiscard]] auto count_configs() const -> size_t;

    /**
     * @brief Get total unresolved conflict count
     *
     * @return Number of unresolved conflicts
     */
    [[nodiscard]] auto count_unresolved_conflicts() const -> size_t;

    /**
     * @brief Get syncs completed today
     *
     * @return Number of syncs completed today
     */
    [[nodiscard]] auto count_syncs_today() const -> size_t;

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
     * @brief Parse a config from a prepared statement
     */
    [[nodiscard]] auto parse_config_row(void* stmt) const -> client::sync_config;

    /**
     * @brief Parse a conflict from a prepared statement
     */
    [[nodiscard]] auto parse_conflict_row(void* stmt) const -> client::sync_conflict;

    /**
     * @brief Parse a history record from a prepared statement
     */
    [[nodiscard]] auto parse_history_row(void* stmt) const -> client::sync_history;

    /**
     * @brief Serialize vector to JSON
     */
    [[nodiscard]] static auto serialize_vector(
        const std::vector<std::string>& vec) -> std::string;

    /**
     * @brief Deserialize vector from JSON
     */
    [[nodiscard]] static auto deserialize_vector(
        std::string_view json) -> std::vector<std::string>;

    /// SQLite database handle
    sqlite3* db_{nullptr};
};

}  // namespace pacs::storage
