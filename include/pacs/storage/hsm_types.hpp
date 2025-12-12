/**
 * @file hsm_types.hpp
 * @brief Types for Hierarchical Storage Management (HSM)
 *
 * This file defines the core types used by the HSM storage system including
 * storage tiers, migration policies, and tier metadata tracking.
 *
 * @see SRS-STOR-010, FR-4.5 (Hierarchical Storage Management)
 */

#pragma once

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pacs::storage {

/**
 * @brief Storage tier classification
 *
 * Represents the different tiers in the hierarchical storage system.
 * Each tier has different performance characteristics and cost implications:
 * - Hot: Fast access, high cost (SSD/NVMe)
 * - Warm: Medium access, medium cost (HDD)
 * - Cold: Slow access, low cost (S3/Glacier/Archive)
 */
enum class storage_tier {
    /// Hot tier - Recent, frequently accessed data (SSD/NVMe)
    hot,

    /// Warm tier - Older, occasionally accessed data (HDD)
    warm,

    /// Cold tier - Archive, rarely accessed data (S3/Glacier)
    cold
};

/**
 * @brief Convert storage_tier to string representation
 * @param tier The storage tier
 * @return String representation ("hot", "warm", "cold")
 */
[[nodiscard]] constexpr auto to_string(storage_tier tier) noexcept
    -> std::string_view {
    switch (tier) {
        case storage_tier::hot:
            return "hot";
        case storage_tier::warm:
            return "warm";
        case storage_tier::cold:
            return "cold";
    }
    return "unknown";
}

/**
 * @brief Parse storage_tier from string
 * @param str String representation ("hot", "warm", "cold")
 * @return Parsed storage tier or std::nullopt if invalid
 */
[[nodiscard]] constexpr auto storage_tier_from_string(std::string_view str)
    -> std::optional<storage_tier> {
    if (str == "hot") {
        return storage_tier::hot;
    }
    if (str == "warm") {
        return storage_tier::warm;
    }
    if (str == "cold") {
        return storage_tier::cold;
    }
    return std::nullopt;
}

/**
 * @brief Tier migration policy configuration
 *
 * Defines the rules for automatic migration between storage tiers.
 * Instances are migrated based on their age (time since last access or storage).
 *
 * @example
 * @code
 * tier_policy policy;
 * policy.hot_to_warm = std::chrono::days{30};   // Move to warm after 30 days
 * policy.warm_to_cold = std::chrono::days{365}; // Move to cold after 1 year
 * policy.auto_migrate = true;                    // Enable automatic migration
 *
 * hsm_storage storage{hot, warm, cold};
 * storage.set_tier_policy(policy);
 * @endcode
 */
struct tier_policy {
    /// Time threshold for migrating from hot to warm tier
    /// Default: 30 days
    std::chrono::days hot_to_warm{30};

    /// Time threshold for migrating from warm to cold tier
    /// Default: 365 days (1 year)
    std::chrono::days warm_to_cold{365};

    /// Enable automatic background migration
    /// When false, migration must be triggered manually
    bool auto_migrate{true};

    /// Minimum size in bytes for an instance to be considered for migration
    /// Smaller instances may not be worth the overhead of migration
    /// Default: 0 (no minimum)
    std::size_t min_migration_size{0};

    /// Maximum number of instances to migrate per cycle
    /// Prevents overwhelming the storage system
    /// Default: 100
    std::size_t max_instances_per_cycle{100};

    /// Maximum bytes to migrate per cycle
    /// Default: 10 GB
    std::size_t max_bytes_per_cycle{10ULL * 1024 * 1024 * 1024};

    /**
     * @brief Check if two policies are equal
     */
    [[nodiscard]] auto operator==(const tier_policy& other) const noexcept
        -> bool = default;
};

/**
 * @brief Metadata for tracking instance tier location
 *
 * Stores information about where an instance is stored and when it was
 * last accessed, used for making migration decisions.
 */
struct tier_metadata {
    /// SOP Instance UID of the DICOM instance
    std::string sop_instance_uid;

    /// Current storage tier
    storage_tier current_tier{storage_tier::hot};

    /// Timestamp when instance was stored
    std::chrono::system_clock::time_point stored_at{
        std::chrono::system_clock::now()};

    /// Timestamp of last access (retrieve operation)
    /// nullopt if never accessed after initial storage
    std::optional<std::chrono::system_clock::time_point> last_accessed;

    /// Size of the instance in bytes
    std::size_t size_bytes{0};

    /// Study Instance UID (for grouping migrations)
    std::string study_instance_uid;

    /// Series Instance UID (for grouping migrations)
    std::string series_instance_uid;

    /**
     * @brief Get the age of the instance (time since storage)
     * @return Duration since the instance was stored
     */
    [[nodiscard]] auto age() const -> std::chrono::system_clock::duration {
        return std::chrono::system_clock::now() - stored_at;
    }

    /**
     * @brief Get the time since last access
     * @return Duration since last access, or age if never accessed
     */
    [[nodiscard]] auto time_since_access() const
        -> std::chrono::system_clock::duration {
        if (last_accessed.has_value()) {
            return std::chrono::system_clock::now() - *last_accessed;
        }
        return age();
    }

    /**
     * @brief Check if instance is eligible for migration to a target tier
     * @param policy The tier policy to check against
     * @param target_tier The target tier for migration
     * @return true if instance should be migrated
     */
    [[nodiscard]] auto should_migrate(const tier_policy& policy,
                                       storage_tier target_tier) const -> bool {
        // Can only migrate to a "colder" tier
        if (target_tier <= current_tier) {
            return false;
        }

        // Check size threshold
        if (size_bytes < policy.min_migration_size) {
            return false;
        }

        auto time_inactive = time_since_access();

        // Check if instance has been inactive long enough
        if (current_tier == storage_tier::hot &&
            target_tier == storage_tier::warm) {
            return time_inactive >= policy.hot_to_warm;
        }

        if (current_tier == storage_tier::warm &&
            target_tier == storage_tier::cold) {
            return time_inactive >= policy.warm_to_cold;
        }

        // Hot directly to cold (must meet both thresholds)
        if (current_tier == storage_tier::hot &&
            target_tier == storage_tier::cold) {
            return time_inactive >= (policy.hot_to_warm + policy.warm_to_cold);
        }

        return false;
    }
};

/**
 * @brief Result of a migration operation
 */
struct migration_result {
    /// Number of instances successfully migrated
    std::size_t instances_migrated{0};

    /// Total bytes migrated
    std::size_t bytes_migrated{0};

    /// Duration of the migration operation
    std::chrono::milliseconds duration{0};

    /// SOP Instance UIDs that failed to migrate
    std::vector<std::string> failed_uids;

    /// Number of instances that were skipped (not eligible)
    std::size_t instances_skipped{0};

    /**
     * @brief Check if the migration was completely successful
     * @return true if no failures occurred
     */
    [[nodiscard]] auto is_success() const noexcept -> bool {
        return failed_uids.empty();
    }

    /**
     * @brief Get the total number of instances processed
     */
    [[nodiscard]] auto total_processed() const noexcept -> std::size_t {
        return instances_migrated + failed_uids.size() + instances_skipped;
    }
};

/**
 * @brief Statistics for a single storage tier
 */
struct tier_statistics {
    /// Number of instances in this tier
    std::size_t instance_count{0};

    /// Total bytes stored in this tier
    std::size_t total_bytes{0};

    /// Number of unique studies in this tier
    std::size_t study_count{0};

    /// Number of unique series in this tier
    std::size_t series_count{0};
};

/**
 * @brief Combined statistics for all HSM tiers
 */
struct hsm_statistics {
    /// Statistics for hot tier
    tier_statistics hot;

    /// Statistics for warm tier
    tier_statistics warm;

    /// Statistics for cold tier
    tier_statistics cold;

    /**
     * @brief Get total instance count across all tiers
     */
    [[nodiscard]] auto total_instances() const noexcept -> std::size_t {
        return hot.instance_count + warm.instance_count + cold.instance_count;
    }

    /**
     * @brief Get total bytes across all tiers
     */
    [[nodiscard]] auto total_bytes() const noexcept -> std::size_t {
        return hot.total_bytes + warm.total_bytes + cold.total_bytes;
    }
};

}  // namespace pacs::storage
