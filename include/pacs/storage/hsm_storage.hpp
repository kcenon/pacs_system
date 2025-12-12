/**
 * @file hsm_storage.hpp
 * @brief Hierarchical Storage Management (HSM) for multi-tier DICOM storage
 *
 * This file provides the hsm_storage class which implements storage_interface
 * by combining multiple storage backends into a tiered hierarchy. Data is
 * automatically migrated between tiers based on configurable age policies.
 *
 * @see SRS-STOR-010, FR-4.5 (Hierarchical Storage Management)
 */

#pragma once

#include "hsm_types.hpp"
#include "storage_interface.hpp"

#include <memory>
#include <shared_mutex>
#include <unordered_map>

namespace pacs::storage {

/**
 * @brief Configuration for HSM storage
 */
struct hsm_storage_config {
    /// Tier migration policy
    tier_policy policy;

    /// Whether to track access times for migration decisions
    /// When true, retrieves update the last_accessed timestamp
    bool track_access_time{true};

    /// Whether to verify data integrity after migration
    bool verify_after_migration{true};

    /// Whether to remove source after successful migration
    /// When false, data is copied (not moved) between tiers
    bool delete_after_migration{true};
};

/**
 * @brief Hierarchical Storage Management for multi-tier DICOM storage
 *
 * Combines multiple storage backends (hot, warm, cold) into a unified
 * hierarchical storage system. New data is stored in the hot tier by default,
 * and automatically migrates to cooler tiers based on age and access patterns.
 *
 * The retrieval process is transparent - the caller doesn't need to know
 * which tier contains the data.
 *
 * Thread Safety:
 * - All methods are thread-safe
 * - Concurrent reads are allowed (shared lock)
 * - Writes and migrations require exclusive lock
 *
 * @example
 * @code
 * // Create tier backends
 * auto hot = std::make_unique<file_storage>(hot_config);
 * auto warm = std::make_unique<file_storage>(warm_config);
 * auto cold = std::make_unique<s3_storage>(s3_config);
 *
 * // Create HSM storage
 * hsm_storage_config config;
 * config.policy.hot_to_warm = std::chrono::days{30};
 * config.policy.warm_to_cold = std::chrono::days{365};
 *
 * hsm_storage storage{std::move(hot), std::move(warm), std::move(cold), config};
 *
 * // Store (goes to hot tier by default)
 * storage.store(dataset);
 *
 * // Retrieve (transparently from any tier)
 * auto result = storage.retrieve("1.2.3.4.5");
 *
 * // Manual migration
 * storage.migrate("1.2.3.4.5", storage_tier::cold);
 * @endcode
 */
class hsm_storage : public storage_interface {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct HSM storage with three tier backends
     *
     * @param hot_tier Storage backend for hot tier (required)
     * @param warm_tier Storage backend for warm tier (optional, can be nullptr)
     * @param cold_tier Storage backend for cold tier (optional, can be nullptr)
     * @param config HSM configuration
     *
     * @throws std::invalid_argument if hot_tier is nullptr
     *
     * @note At least hot_tier must be provided. If warm_tier is nullptr,
     *       migration will skip directly to cold_tier.
     */
    hsm_storage(std::unique_ptr<storage_interface> hot_tier,
                std::unique_ptr<storage_interface> warm_tier,
                std::unique_ptr<storage_interface> cold_tier,
                const hsm_storage_config& config = {});

    /**
     * @brief Destructor
     */
    ~hsm_storage() override = default;

    /// Non-copyable (contains mutex and unique_ptr)
    hsm_storage(const hsm_storage&) = delete;
    hsm_storage& operator=(const hsm_storage&) = delete;

    /// Non-movable (contains mutex)
    hsm_storage(hsm_storage&&) = delete;
    hsm_storage& operator=(hsm_storage&&) = delete;

    // =========================================================================
    // storage_interface Implementation
    // =========================================================================

    /**
     * @brief Store a DICOM dataset to the hot tier
     *
     * New data is always stored in the hot tier. Migration to cooler tiers
     * happens based on the configured tier policy.
     *
     * @param dataset The DICOM dataset to store
     * @return VoidResult Success or error information
     */
    [[nodiscard]] auto store(const core::dicom_dataset& dataset)
        -> VoidResult override;

    /**
     * @brief Retrieve a DICOM dataset by SOP Instance UID
     *
     * Searches all tiers for the instance, starting from hot tier.
     * If track_access_time is enabled, updates the last access timestamp.
     *
     * @param sop_instance_uid The unique identifier for the instance
     * @return Result containing the dataset or error information
     */
    [[nodiscard]] auto retrieve(std::string_view sop_instance_uid)
        -> Result<core::dicom_dataset> override;

    /**
     * @brief Remove a DICOM dataset from all tiers
     *
     * Removes the instance from whichever tier contains it.
     *
     * @param sop_instance_uid The unique identifier for the instance to remove
     * @return VoidResult Success or error information
     */
    [[nodiscard]] auto remove(std::string_view sop_instance_uid)
        -> VoidResult override;

    /**
     * @brief Check if a DICOM instance exists in any tier
     *
     * @param sop_instance_uid The unique identifier to check
     * @return true if the instance exists in any tier
     */
    [[nodiscard]] auto exists(std::string_view sop_instance_uid) const
        -> bool override;

    /**
     * @brief Find DICOM datasets matching query criteria across all tiers
     *
     * Searches all tiers and combines results.
     *
     * @param query The query dataset containing search criteria
     * @return Result containing matching datasets or error information
     */
    [[nodiscard]] auto find(const core::dicom_dataset& query)
        -> Result<std::vector<core::dicom_dataset>> override;

    /**
     * @brief Get combined storage statistics from all tiers
     *
     * @return Storage statistics aggregated from all tiers
     */
    [[nodiscard]] auto get_statistics() const -> storage_statistics override;

    /**
     * @brief Verify storage integrity across all tiers
     *
     * Verifies each tier's integrity and checks tier metadata consistency.
     *
     * @return VoidResult Success if integrity is verified, error otherwise
     */
    [[nodiscard]] auto verify_integrity() -> VoidResult override;

    // =========================================================================
    // HSM-specific Operations
    // =========================================================================

    /**
     * @brief Get the current tier of an instance
     *
     * @param sop_instance_uid The SOP Instance UID
     * @return The storage tier, or nullopt if not found
     */
    [[nodiscard]] auto get_tier(std::string_view sop_instance_uid) const
        -> std::optional<storage_tier>;

    /**
     * @brief Get tier metadata for an instance
     *
     * @param sop_instance_uid The SOP Instance UID
     * @return Tier metadata, or nullopt if not found
     */
    [[nodiscard]] auto get_tier_metadata(std::string_view sop_instance_uid) const
        -> std::optional<tier_metadata>;

    /**
     * @brief Manually migrate an instance to a different tier
     *
     * @param sop_instance_uid The SOP Instance UID to migrate
     * @param target_tier The target tier
     * @return VoidResult Success or error information
     *
     * @note Migration to a "hotter" tier (e.g., cold to hot) is allowed
     *       for restoring frequently accessed archived data.
     */
    [[nodiscard]] auto migrate(std::string_view sop_instance_uid,
                                storage_tier target_tier) -> VoidResult;

    /**
     * @brief Get instances eligible for migration
     *
     * Returns instances that should be migrated based on the tier policy.
     *
     * @param from_tier Source tier to check
     * @param to_tier Target tier
     * @return Vector of tier metadata for eligible instances
     */
    [[nodiscard]] auto get_migration_candidates(storage_tier from_tier,
                                                 storage_tier to_tier) const
        -> std::vector<tier_metadata>;

    /**
     * @brief Run a single migration cycle
     *
     * Migrates eligible instances according to the tier policy.
     *
     * @return Migration result with statistics
     */
    [[nodiscard]] auto run_migration_cycle() -> migration_result;

    /**
     * @brief Get the current tier policy
     *
     * @return The current tier policy
     */
    [[nodiscard]] auto get_tier_policy() const -> tier_policy;

    /**
     * @brief Set the tier policy
     *
     * @param policy The new tier policy
     */
    void set_tier_policy(const tier_policy& policy);

    /**
     * @brief Get HSM-specific statistics
     *
     * @return Statistics broken down by tier
     */
    [[nodiscard]] auto get_hsm_statistics() const -> hsm_statistics;

    /**
     * @brief Get the storage backend for a specific tier
     *
     * @param tier The tier to get
     * @return Pointer to the storage backend, or nullptr if tier not configured
     */
    [[nodiscard]] auto get_tier_storage(storage_tier tier) const
        -> storage_interface*;

private:
    // =========================================================================
    // Internal Helper Methods
    // =========================================================================

    /**
     * @brief Find which tier contains an instance
     * @param sop_instance_uid The SOP Instance UID
     * @return The tier containing the instance, or nullopt if not found
     */
    [[nodiscard]] auto find_tier(std::string_view sop_instance_uid) const
        -> std::optional<storage_tier>;

    /**
     * @brief Get the storage backend for a tier
     * @param tier The tier
     * @return Pointer to the storage backend
     */
    [[nodiscard]] auto get_storage(storage_tier tier) const
        -> storage_interface*;

    /**
     * @brief Update tier metadata after store/retrieve
     * @param sop_instance_uid The SOP Instance UID
     * @param tier The tier where the instance is stored
     * @param dataset The dataset (for extracting metadata)
     */
    void update_metadata(std::string_view sop_instance_uid, storage_tier tier,
                         const core::dicom_dataset& dataset);

    /**
     * @brief Update last access time for an instance
     * @param sop_instance_uid The SOP Instance UID
     */
    void update_access_time(std::string_view sop_instance_uid);

    /**
     * @brief Remove tier metadata
     * @param sop_instance_uid The SOP Instance UID
     */
    void remove_metadata(std::string_view sop_instance_uid);

    /**
     * @brief Migrate a single instance between tiers
     * @param uid The SOP Instance UID
     * @param from_tier Source tier
     * @param to_tier Target tier
     * @return VoidResult Success or error information
     */
    [[nodiscard]] auto migrate_instance(std::string_view uid,
                                         storage_tier from_tier,
                                         storage_tier to_tier) -> VoidResult;

    // =========================================================================
    // Member Variables
    // =========================================================================

    /// Hot tier storage backend
    std::unique_ptr<storage_interface> hot_tier_;

    /// Warm tier storage backend (may be nullptr)
    std::unique_ptr<storage_interface> warm_tier_;

    /// Cold tier storage backend (may be nullptr)
    std::unique_ptr<storage_interface> cold_tier_;

    /// HSM configuration
    hsm_storage_config config_;

    /// Tier metadata index (SOP Instance UID -> metadata)
    std::unordered_map<std::string, tier_metadata> metadata_index_;

    /// Mutex for thread-safe access
    mutable std::shared_mutex mutex_;
};

}  // namespace pacs::storage
