/**
 * @file sync_types.hpp
 * @brief Types and structures for bidirectional DICOM synchronization
 *
 * This file provides data structures for representing synchronization
 * configurations, conflicts, and results for the sync_manager.
 *
 * @see Issue #542 - Implement Sync Manager for Bidirectional Synchronization
 * @see Issue #530 - PACS Client System Support (Parent Epic)
 */

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pacs::client {

// =============================================================================
// Sync Direction
// =============================================================================

/**
 * @brief Direction of synchronization
 */
enum class sync_direction {
    pull,          ///< Pull from remote to local
    push,          ///< Push from local to remote
    bidirectional  ///< Both directions
};

/**
 * @brief Convert sync_direction to string representation
 * @param direction The direction to convert
 * @return String representation of the direction
 */
[[nodiscard]] constexpr const char* to_string(sync_direction direction) noexcept {
    switch (direction) {
        case sync_direction::pull: return "pull";
        case sync_direction::push: return "push";
        case sync_direction::bidirectional: return "bidirectional";
        default: return "unknown";
    }
}

/**
 * @brief Parse sync_direction from string
 * @param str The string to parse
 * @return Parsed direction, or pull if invalid
 */
[[nodiscard]] inline sync_direction sync_direction_from_string(
    std::string_view str) noexcept {
    if (str == "pull") return sync_direction::pull;
    if (str == "push") return sync_direction::push;
    if (str == "bidirectional") return sync_direction::bidirectional;
    return sync_direction::pull;
}

// =============================================================================
// Conflict Type
// =============================================================================

/**
 * @brief Type of synchronization conflict
 */
enum class sync_conflict_type {
    missing_local,   ///< Study exists on remote but not locally
    missing_remote,  ///< Study exists locally but not on remote
    modified,        ///< Study modified on both sides
    count_mismatch   ///< Instance counts differ
};

/**
 * @brief Convert sync_conflict_type to string representation
 * @param type The type to convert
 * @return String representation of the type
 */
[[nodiscard]] constexpr const char* to_string(sync_conflict_type type) noexcept {
    switch (type) {
        case sync_conflict_type::missing_local: return "missing_local";
        case sync_conflict_type::missing_remote: return "missing_remote";
        case sync_conflict_type::modified: return "modified";
        case sync_conflict_type::count_mismatch: return "count_mismatch";
        default: return "unknown";
    }
}

/**
 * @brief Parse sync_conflict_type from string
 * @param str The string to parse
 * @return Parsed type, or missing_local if invalid
 */
[[nodiscard]] inline sync_conflict_type sync_conflict_type_from_string(
    std::string_view str) noexcept {
    if (str == "missing_local") return sync_conflict_type::missing_local;
    if (str == "missing_remote") return sync_conflict_type::missing_remote;
    if (str == "modified") return sync_conflict_type::modified;
    if (str == "count_mismatch") return sync_conflict_type::count_mismatch;
    return sync_conflict_type::missing_local;
}

// =============================================================================
// Conflict Resolution
// =============================================================================

/**
 * @brief Strategy for resolving synchronization conflicts
 */
enum class conflict_resolution {
    prefer_local,   ///< Keep local version
    prefer_remote,  ///< Use remote version
    prefer_newer    ///< Use the newer version based on timestamp
};

/**
 * @brief Convert conflict_resolution to string representation
 * @param resolution The resolution to convert
 * @return String representation of the resolution
 */
[[nodiscard]] constexpr const char* to_string(
    conflict_resolution resolution) noexcept {
    switch (resolution) {
        case conflict_resolution::prefer_local: return "prefer_local";
        case conflict_resolution::prefer_remote: return "prefer_remote";
        case conflict_resolution::prefer_newer: return "prefer_newer";
        default: return "unknown";
    }
}

/**
 * @brief Parse conflict_resolution from string
 * @param str The string to parse
 * @return Parsed resolution, or prefer_remote if invalid
 */
[[nodiscard]] inline conflict_resolution conflict_resolution_from_string(
    std::string_view str) noexcept {
    if (str == "prefer_local") return conflict_resolution::prefer_local;
    if (str == "prefer_remote") return conflict_resolution::prefer_remote;
    if (str == "prefer_newer") return conflict_resolution::prefer_newer;
    return conflict_resolution::prefer_remote;
}

// =============================================================================
// Sync Config
// =============================================================================

/**
 * @brief Configuration for a synchronization task
 *
 * Defines the scope, behavior, and schedule for syncing with a remote node.
 */
struct sync_config {
    // =========================================================================
    // Identification
    // =========================================================================

    std::string config_id;       ///< Unique configuration identifier
    std::string source_node_id;  ///< Remote node to sync with
    std::string name;            ///< Human-readable name
    bool enabled{true};          ///< Whether this config is active

    // =========================================================================
    // Sync Scope
    // =========================================================================

    std::chrono::hours lookback{24};            ///< How far back to sync
    std::vector<std::string> modalities;        ///< Modality filter (empty = all)
    std::vector<std::string> patient_id_patterns;  ///< Patient ID patterns (empty = all)

    // =========================================================================
    // Sync Behavior
    // =========================================================================

    sync_direction direction{sync_direction::pull};  ///< Direction of sync
    bool delete_missing{false};       ///< Delete local if not on remote
    bool overwrite_existing{false};   ///< Overwrite if different
    bool sync_metadata_only{false};   ///< Only sync metadata, not images

    // =========================================================================
    // Schedule
    // =========================================================================

    std::string schedule_cron;  ///< Cron expression for scheduling

    // =========================================================================
    // Statistics
    // =========================================================================

    std::chrono::system_clock::time_point last_sync;
    std::chrono::system_clock::time_point last_successful_sync;
    size_t total_syncs{0};
    size_t studies_synced{0};

    // =========================================================================
    // Database Fields
    // =========================================================================

    int64_t pk{0};  ///< Primary key (0 if not persisted)
};

// =============================================================================
// Sync Conflict
// =============================================================================

/**
 * @brief Represents a conflict detected during synchronization
 */
struct sync_conflict {
    std::string config_id;   ///< Config that detected this conflict
    std::string study_uid;   ///< Study Instance UID
    std::string patient_id;  ///< Patient ID for reference

    sync_conflict_type conflict_type;  ///< Type of conflict

    std::chrono::system_clock::time_point local_modified;
    std::chrono::system_clock::time_point remote_modified;

    size_t local_instance_count{0};
    size_t remote_instance_count{0};

    bool resolved{false};                  ///< Whether this conflict was resolved
    conflict_resolution resolution_used;   ///< Resolution strategy used

    std::chrono::system_clock::time_point detected_at;
    std::optional<std::chrono::system_clock::time_point> resolved_at;

    // =========================================================================
    // Database Fields
    // =========================================================================

    int64_t pk{0};  ///< Primary key (0 if not persisted)
};

// =============================================================================
// Sync Result
// =============================================================================

/**
 * @brief Result of a synchronization operation
 */
struct sync_result {
    std::string config_id;   ///< Configuration used
    std::string job_id;      ///< Job ID if async
    bool success{false};     ///< Overall success

    // =========================================================================
    // Counts
    // =========================================================================

    size_t studies_checked{0};       ///< Total studies compared
    size_t studies_synced{0};        ///< Studies actually synced
    size_t studies_skipped{0};       ///< Studies skipped
    size_t instances_transferred{0}; ///< Individual instances transferred
    size_t bytes_transferred{0};     ///< Total bytes transferred

    // =========================================================================
    // Issues
    // =========================================================================

    std::vector<sync_conflict> conflicts;  ///< Conflicts detected
    std::vector<std::string> errors;       ///< Error messages

    // =========================================================================
    // Timing
    // =========================================================================

    std::chrono::system_clock::time_point started_at;
    std::chrono::system_clock::time_point completed_at;
    std::chrono::milliseconds elapsed{0};
};

// =============================================================================
// Sync History
// =============================================================================

/**
 * @brief Historical record of a sync operation
 */
struct sync_history {
    std::string config_id;
    std::string job_id;
    bool success{false};

    size_t studies_checked{0};
    size_t studies_synced{0};
    size_t conflicts_found{0};

    std::vector<std::string> errors;

    std::chrono::system_clock::time_point started_at;
    std::chrono::system_clock::time_point completed_at;

    int64_t pk{0};
};

// =============================================================================
// Sync Manager Configuration
// =============================================================================

/**
 * @brief Configuration for the sync manager
 */
struct sync_manager_config {
    size_t max_concurrent_syncs{2};                        ///< Max parallel syncs
    std::chrono::seconds comparison_timeout{300};          ///< Timeout for compare
    bool auto_resolve_conflicts{false};                    ///< Auto-resolve conflicts
    conflict_resolution default_resolution{conflict_resolution::prefer_remote};
};

// =============================================================================
// Sync Statistics
// =============================================================================

/**
 * @brief Aggregate statistics for synchronization operations
 */
struct sync_statistics {
    size_t total_syncs{0};
    size_t successful_syncs{0};
    size_t failed_syncs{0};
    size_t total_studies_synced{0};
    size_t total_bytes_transferred{0};
    size_t total_conflicts_detected{0};
    size_t total_conflicts_resolved{0};
};

// =============================================================================
// Callbacks
// =============================================================================

/**
 * @brief Callback for sync progress updates
 *
 * @param config_id The ID of the sync config
 * @param studies_synced Number of studies synced so far
 * @param studies_total Total studies to sync
 */
using sync_progress_callback = std::function<void(
    const std::string& config_id,
    size_t studies_synced,
    size_t studies_total)>;

/**
 * @brief Callback for sync completion
 *
 * @param config_id The ID of the sync config
 * @param result Final sync result
 */
using sync_completion_callback = std::function<void(
    const std::string& config_id,
    const sync_result& result)>;

/**
 * @brief Callback for conflict detection
 *
 * @param conflict The detected conflict
 */
using sync_conflict_callback = std::function<void(const sync_conflict& conflict)>;

}  // namespace pacs::client
