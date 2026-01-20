/**
 * @file sync_manager.hpp
 * @brief Sync manager for bidirectional DICOM data synchronization
 *
 * This file provides the sync_manager class for managing bidirectional
 * synchronization between local PACS and remote nodes.
 *
 * @see Issue #542 - Implement Sync Manager for Bidirectional Synchronization
 * @see Issue #530 - PACS Client System Support (Parent Epic)
 */

#pragma once

#include "pacs/client/sync_types.hpp"
#include "pacs/core/result.hpp"
#include "pacs/di/ilogger.hpp"

#include <atomic>
#include <future>
#include <memory>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>

// Forward declarations
namespace pacs::storage {
class sync_repository;
}

namespace pacs::services {
class query_scu;
}

namespace pacs::client {

// Forward declarations
class remote_node_manager;
class job_manager;

// =============================================================================
// Sync Manager
// =============================================================================

/**
 * @brief Manager for bidirectional DICOM data synchronization
 *
 * Provides synchronization capabilities with:
 * - Incremental sync based on timestamps
 * - Full sync for initial setup
 * - Conflict detection and resolution
 * - Scheduled sync jobs via cron expressions
 * - Statistics tracking per config
 *
 * Thread Safety:
 * - All public methods are thread-safe
 * - Uses shared_mutex for config access
 *
 * @example
 * @code
 * // Create manager with dependencies
 * auto sync_repo = std::make_shared<sync_repository>(db);
 * auto node_mgr = std::make_shared<remote_node_manager>(node_repo);
 * auto job_mgr = std::make_shared<job_manager>(job_repo, node_mgr);
 * auto query_scu = std::make_shared<query_scu>();
 * auto manager = std::make_unique<sync_manager>(sync_repo, node_mgr, job_mgr, query_scu);
 *
 * // Add a sync configuration
 * sync_config config;
 * config.config_id = "daily-sync";
 * config.name = "Daily Sync with Archive";
 * config.source_node_id = "archive-server";
 * config.direction = sync_direction::bidirectional;
 * config.schedule_cron = "0 2 * * *";  // 2 AM daily
 * manager->add_config(config);
 *
 * // Start scheduler
 * manager->start_scheduler();
 *
 * // Manual sync
 * auto job_id = manager->sync_now("daily-sync");
 *
 * // Compare without syncing (dry run)
 * auto result = manager->compare("daily-sync");
 * @endcode
 */
class sync_manager {
public:
    // =========================================================================
    // Construction / Destruction
    // =========================================================================

    /**
     * @brief Construct a sync manager with default configuration
     *
     * @param repo Sync repository for persistence (required)
     * @param node_manager Remote node manager (required)
     * @param job_manager Job manager for async operations (required)
     * @param query_scu Query SCU for study comparisons (required)
     * @param logger Logger instance (optional)
     */
    explicit sync_manager(
        std::shared_ptr<storage::sync_repository> repo,
        std::shared_ptr<remote_node_manager> node_manager,
        std::shared_ptr<job_manager> job_manager,
        std::shared_ptr<services::query_scu> query_scu,
        std::shared_ptr<di::ILogger> logger = nullptr);

    /**
     * @brief Construct a sync manager with custom configuration
     *
     * @param config Manager configuration
     * @param repo Sync repository for persistence (required)
     * @param node_manager Remote node manager (required)
     * @param job_manager Job manager for async operations (required)
     * @param query_scu Query SCU for study comparisons (required)
     * @param logger Logger instance (optional)
     */
    explicit sync_manager(
        const sync_manager_config& config,
        std::shared_ptr<storage::sync_repository> repo,
        std::shared_ptr<remote_node_manager> node_manager,
        std::shared_ptr<job_manager> job_manager,
        std::shared_ptr<services::query_scu> query_scu,
        std::shared_ptr<di::ILogger> logger = nullptr);

    /**
     * @brief Destructor - stops scheduler if running
     */
    ~sync_manager();

    // Non-copyable, non-movable
    sync_manager(const sync_manager&) = delete;
    auto operator=(const sync_manager&) -> sync_manager& = delete;
    sync_manager(sync_manager&&) = delete;
    auto operator=(sync_manager&&) -> sync_manager& = delete;

    // =========================================================================
    // Config CRUD
    // =========================================================================

    /**
     * @brief Add a new sync configuration
     *
     * @param config The configuration to add
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto add_config(const sync_config& config) -> pacs::VoidResult;

    /**
     * @brief Update an existing sync configuration
     *
     * @param config The configuration to update (identified by config_id)
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto update_config(const sync_config& config) -> pacs::VoidResult;

    /**
     * @brief Remove a sync configuration
     *
     * @param config_id The ID of the configuration to remove
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto remove_config(std::string_view config_id) -> pacs::VoidResult;

    /**
     * @brief Get a sync configuration by ID
     *
     * @param config_id The configuration ID to retrieve
     * @return Optional containing the config if found
     */
    [[nodiscard]] auto get_config(std::string_view config_id) const
        -> std::optional<sync_config>;

    /**
     * @brief List all sync configurations
     *
     * @return Vector of all configurations
     */
    [[nodiscard]] auto list_configs() const -> std::vector<sync_config>;

    // =========================================================================
    // Manual Sync Operations
    // =========================================================================

    /**
     * @brief Start sync immediately for a configuration
     *
     * Uses the config's existing settings (incremental or full based on last_sync).
     *
     * @param config_id The configuration ID to sync
     * @return Job ID for tracking progress
     */
    [[nodiscard]] auto sync_now(std::string_view config_id) -> std::string;

    /**
     * @brief Perform a full sync for a configuration
     *
     * Syncs all data within the lookback period, ignoring last sync time.
     *
     * @param config_id The configuration ID to sync
     * @return Job ID for tracking progress
     */
    [[nodiscard]] auto full_sync(std::string_view config_id) -> std::string;

    /**
     * @brief Perform an incremental sync for a configuration
     *
     * Only syncs data modified since the last successful sync.
     *
     * @param config_id The configuration ID to sync
     * @return Job ID for tracking progress
     */
    [[nodiscard]] auto incremental_sync(std::string_view config_id) -> std::string;

    /**
     * @brief Wait for a sync operation to complete
     *
     * @param job_id The job ID returned from sync operations
     * @return Future containing the sync result
     */
    [[nodiscard]] auto wait_for_sync(std::string_view job_id)
        -> std::future<sync_result>;

    // =========================================================================
    // Comparison (Dry Run)
    // =========================================================================

    /**
     * @brief Compare local and remote data without syncing
     *
     * Returns a sync_result showing what would be synced.
     *
     * @param config_id The configuration ID to compare
     * @return Sync result with comparison data
     */
    [[nodiscard]] auto compare(std::string_view config_id) -> sync_result;

    /**
     * @brief Compare local and remote data asynchronously
     *
     * @param config_id The configuration ID to compare
     * @return Future containing the comparison result
     */
    [[nodiscard]] auto compare_async(std::string_view config_id)
        -> std::future<sync_result>;

    // =========================================================================
    // Conflict Management
    // =========================================================================

    /**
     * @brief Get all unresolved conflicts
     *
     * @return Vector of all unresolved conflicts
     */
    [[nodiscard]] auto get_conflicts() const -> std::vector<sync_conflict>;

    /**
     * @brief Get unresolved conflicts for a specific configuration
     *
     * @param config_id The configuration ID
     * @return Vector of unresolved conflicts for that config
     */
    [[nodiscard]] auto get_conflicts(std::string_view config_id) const
        -> std::vector<sync_conflict>;

    /**
     * @brief Resolve a specific conflict
     *
     * @param study_uid The Study Instance UID of the conflict
     * @param resolution How to resolve the conflict
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto resolve_conflict(
        std::string_view study_uid,
        conflict_resolution resolution) -> pacs::VoidResult;

    /**
     * @brief Resolve all conflicts for a configuration
     *
     * @param config_id The configuration ID
     * @param resolution How to resolve all conflicts
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto resolve_all_conflicts(
        std::string_view config_id,
        conflict_resolution resolution) -> pacs::VoidResult;

    // =========================================================================
    // Scheduler
    // =========================================================================

    /**
     * @brief Start the sync scheduler
     *
     * Schedules syncs based on each config's schedule_cron setting.
     */
    void start_scheduler();

    /**
     * @brief Stop the sync scheduler
     */
    void stop_scheduler();

    /**
     * @brief Check if scheduler is running
     *
     * @return true if scheduler is active
     */
    [[nodiscard]] auto is_scheduler_running() const noexcept -> bool;

    // =========================================================================
    // Status
    // =========================================================================

    /**
     * @brief Check if a sync is currently running for a configuration
     *
     * @param config_id The configuration ID to check
     * @return true if sync is in progress
     */
    [[nodiscard]] auto is_syncing(std::string_view config_id) const -> bool;

    /**
     * @brief Get the last sync result for a configuration
     *
     * @param config_id The configuration ID
     * @return Last sync result
     */
    [[nodiscard]] auto get_last_result(std::string_view config_id) const
        -> sync_result;

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get overall sync statistics
     *
     * @return Aggregate statistics for all sync operations
     */
    [[nodiscard]] auto get_statistics() const -> sync_statistics;

    /**
     * @brief Get statistics for a specific configuration
     *
     * @param config_id The configuration ID
     * @return Statistics for that configuration
     */
    [[nodiscard]] auto get_statistics(std::string_view config_id) const
        -> sync_statistics;

    // =========================================================================
    // Callbacks
    // =========================================================================

    /**
     * @brief Set callback for sync progress updates
     *
     * @param callback The callback function
     */
    void set_progress_callback(sync_progress_callback callback);

    /**
     * @brief Set callback for sync completion
     *
     * @param callback The callback function
     */
    void set_completion_callback(sync_completion_callback callback);

    /**
     * @brief Set callback for conflict detection
     *
     * @param callback The callback function
     */
    void set_conflict_callback(sync_conflict_callback callback);

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Get current manager configuration
     *
     * @return Current configuration
     */
    [[nodiscard]] auto config() const noexcept -> const sync_manager_config&;

private:
    // =========================================================================
    // Private Implementation (Pimpl)
    // =========================================================================

    struct impl;
    std::unique_ptr<impl> impl_;
};

}  // namespace pacs::client
