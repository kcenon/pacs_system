/**
 * @file hsm_storage.cpp
 * @brief Implementation of Hierarchical Storage Management for multi-tier storage
 */

#include <pacs/storage/hsm_storage.hpp>

#include <pacs/core/dicom_tag_constants.hpp>

#include <algorithm>
#include <chrono>
#include <mutex>
#include <set>
#include <stdexcept>

namespace pacs::storage {

using kcenon::common::make_error;
using kcenon::common::ok;

namespace {

/// Error codes for HSM storage operations
constexpr int kInvalidConfiguration = -100;
constexpr int kInstanceNotFound = -101;
constexpr int kMigrationFailed = -102;
constexpr int kTierNotAvailable = -103;
constexpr int kIntegrityError = -104;

}  // namespace

// ============================================================================
// Construction
// ============================================================================

hsm_storage::hsm_storage(std::unique_ptr<storage_interface> hot_tier,
                         std::unique_ptr<storage_interface> warm_tier,
                         std::unique_ptr<storage_interface> cold_tier,
                         const hsm_storage_config& config)
    : hot_tier_(std::move(hot_tier)),
      warm_tier_(std::move(warm_tier)),
      cold_tier_(std::move(cold_tier)),
      config_(config) {
    if (!hot_tier_) {
        throw std::invalid_argument("hot_tier cannot be nullptr");
    }
}

// ============================================================================
// storage_interface Implementation
// ============================================================================

auto hsm_storage::store(const core::dicom_dataset& dataset) -> VoidResult {
    // Extract SOP Instance UID for metadata tracking
    auto sop_uid = dataset.get_string(core::tags::sop_instance_uid);
    if (sop_uid.empty()) {
        return make_error<std::monostate>(
            kInvalidConfiguration, "Missing SOP Instance UID", "hsm_storage");
    }

    // Store in hot tier
    auto result = hot_tier_->store(dataset);
    if (!result.is_ok()) {
        return result;
    }

    // Update metadata
    std::unique_lock lock(mutex_);
    update_metadata(sop_uid, storage_tier::hot, dataset);

    return ok();
}

auto hsm_storage::retrieve(std::string_view sop_instance_uid)
    -> Result<core::dicom_dataset> {
    // Find which tier contains the instance
    auto tier = find_tier(sop_instance_uid);
    if (!tier.has_value()) {
        return make_error<core::dicom_dataset>(
            kInstanceNotFound,
            "Instance not found: " + std::string(sop_instance_uid),
            "hsm_storage");
    }

    // Retrieve from the tier
    auto* storage = get_storage(*tier);
    if (storage == nullptr) {
        return make_error<core::dicom_dataset>(
            kTierNotAvailable, "Tier storage not available", "hsm_storage");
    }

    auto result = storage->retrieve(sop_instance_uid);
    if (!result.is_ok()) {
        return result;
    }

    // Update access time if tracking is enabled
    if (config_.track_access_time) {
        std::unique_lock lock(mutex_);
        update_access_time(sop_instance_uid);
    }

    return result;
}

auto hsm_storage::remove(std::string_view sop_instance_uid) -> VoidResult {
    // Find which tier contains the instance
    auto tier = find_tier(sop_instance_uid);
    if (!tier.has_value()) {
        // Not found is not an error for remove
        return ok();
    }

    // Remove from the tier
    auto* storage = get_storage(*tier);
    if (storage == nullptr) {
        return make_error<std::monostate>(
            kTierNotAvailable, "Tier storage not available", "hsm_storage");
    }

    auto result = storage->remove(sop_instance_uid);
    if (!result.is_ok()) {
        return result;
    }

    // Remove metadata
    std::unique_lock lock(mutex_);
    remove_metadata(sop_instance_uid);

    return ok();
}

auto hsm_storage::exists(std::string_view sop_instance_uid) const -> bool {
    std::shared_lock lock(mutex_);
    return metadata_index_.contains(std::string(sop_instance_uid));
}

auto hsm_storage::find(const core::dicom_dataset& query)
    -> Result<std::vector<core::dicom_dataset>> {
    std::vector<core::dicom_dataset> combined_results;

    // Search all tiers and combine results
    std::vector<storage_interface*> tiers = {hot_tier_.get(), warm_tier_.get(),
                                              cold_tier_.get()};

    for (auto* tier : tiers) {
        if (tier == nullptr) {
            continue;
        }

        auto result = tier->find(query);
        if (result.is_ok()) {
            auto& datasets = result.value();
            combined_results.insert(combined_results.end(), datasets.begin(),
                                    datasets.end());
        }
    }

    return combined_results;
}

auto hsm_storage::get_statistics() const -> storage_statistics {
    storage_statistics stats;

    std::shared_lock lock(mutex_);

    // Aggregate from all tiers
    std::vector<storage_interface*> tiers = {hot_tier_.get(), warm_tier_.get(),
                                              cold_tier_.get()};

    std::set<std::string> studies;
    std::set<std::string> series;
    std::set<std::string> patients;

    for (auto* tier : tiers) {
        if (tier == nullptr) {
            continue;
        }

        auto tier_stats = tier->get_statistics();
        stats.total_instances += tier_stats.total_instances;
        stats.total_bytes += tier_stats.total_bytes;
    }

    // Count unique studies/series from metadata
    for (const auto& [uid, meta] : metadata_index_) {
        if (!meta.study_instance_uid.empty()) {
            studies.insert(meta.study_instance_uid);
        }
        if (!meta.series_instance_uid.empty()) {
            series.insert(meta.series_instance_uid);
        }
    }

    stats.studies_count = studies.size();
    stats.series_count = series.size();

    return stats;
}

auto hsm_storage::verify_integrity() -> VoidResult {
    // Verify each tier
    std::vector<std::pair<storage_tier, storage_interface*>> tiers = {
        {storage_tier::hot, hot_tier_.get()},
        {storage_tier::warm, warm_tier_.get()},
        {storage_tier::cold, cold_tier_.get()}};

    for (const auto& [tier, storage] : tiers) {
        if (storage == nullptr) {
            continue;
        }

        auto result = storage->verify_integrity();
        if (!result.is_ok()) {
            return make_error<std::monostate>(
                kIntegrityError,
                "Integrity check failed for " + std::string(to_string(tier)) +
                    " tier: " + std::string(result.error().message),
                "hsm_storage");
        }
    }

    // Verify metadata consistency
    std::shared_lock lock(mutex_);
    for (const auto& [uid, meta] : metadata_index_) {
        auto* storage = get_storage(meta.current_tier);
        if (storage == nullptr) {
            continue;
        }

        if (!storage->exists(uid)) {
            return make_error<std::monostate>(
                kIntegrityError,
                "Metadata references non-existent instance: " + uid,
                "hsm_storage");
        }
    }

    return ok();
}

// ============================================================================
// HSM-specific Operations
// ============================================================================

auto hsm_storage::get_tier(std::string_view sop_instance_uid) const
    -> std::optional<storage_tier> {
    std::shared_lock lock(mutex_);
    auto it = metadata_index_.find(std::string(sop_instance_uid));
    if (it == metadata_index_.end()) {
        return std::nullopt;
    }
    return it->second.current_tier;
}

auto hsm_storage::get_tier_metadata(std::string_view sop_instance_uid) const
    -> std::optional<tier_metadata> {
    std::shared_lock lock(mutex_);
    auto it = metadata_index_.find(std::string(sop_instance_uid));
    if (it == metadata_index_.end()) {
        return std::nullopt;
    }
    return it->second;
}

auto hsm_storage::migrate(std::string_view sop_instance_uid,
                          storage_tier target_tier) -> VoidResult {
    // Find current tier
    auto current_tier_opt = find_tier(sop_instance_uid);
    if (!current_tier_opt.has_value()) {
        return make_error<std::monostate>(
            kInstanceNotFound,
            "Instance not found: " + std::string(sop_instance_uid),
            "hsm_storage");
    }

    auto current_tier = *current_tier_opt;
    if (current_tier == target_tier) {
        // Already in target tier
        return ok();
    }

    return migrate_instance(sop_instance_uid, current_tier, target_tier);
}

auto hsm_storage::get_migration_candidates(storage_tier from_tier,
                                            storage_tier to_tier) const
    -> std::vector<tier_metadata> {
    std::vector<tier_metadata> candidates;

    std::shared_lock lock(mutex_);
    for (const auto& [uid, meta] : metadata_index_) {
        if (meta.current_tier == from_tier &&
            meta.should_migrate(config_.policy, to_tier)) {
            candidates.push_back(meta);
        }
    }

    // Sort by age (oldest first)
    std::sort(candidates.begin(), candidates.end(),
              [](const tier_metadata& a, const tier_metadata& b) {
                  return a.stored_at < b.stored_at;
              });

    return candidates;
}

auto hsm_storage::run_migration_cycle() -> migration_result {
    migration_result result;
    auto start_time = std::chrono::steady_clock::now();

    // Hot to warm migration
    if (warm_tier_) {
        auto candidates =
            get_migration_candidates(storage_tier::hot, storage_tier::warm);

        for (const auto& meta : candidates) {
            if (result.instances_migrated >=
                config_.policy.max_instances_per_cycle) {
                break;
            }
            if (result.bytes_migrated >= config_.policy.max_bytes_per_cycle) {
                break;
            }

            auto migrate_result = migrate_instance(
                meta.sop_instance_uid, storage_tier::hot, storage_tier::warm);

            if (migrate_result.is_ok()) {
                result.instances_migrated++;
                result.bytes_migrated += meta.size_bytes;
            } else {
                result.failed_uids.push_back(meta.sop_instance_uid);
            }
        }
    }

    // Warm to cold migration
    if (cold_tier_) {
        auto candidates =
            get_migration_candidates(storage_tier::warm, storage_tier::cold);

        for (const auto& meta : candidates) {
            if (result.instances_migrated >=
                config_.policy.max_instances_per_cycle) {
                break;
            }
            if (result.bytes_migrated >= config_.policy.max_bytes_per_cycle) {
                break;
            }

            auto migrate_result = migrate_instance(
                meta.sop_instance_uid, storage_tier::warm, storage_tier::cold);

            if (migrate_result.is_ok()) {
                result.instances_migrated++;
                result.bytes_migrated += meta.size_bytes;
            } else {
                result.failed_uids.push_back(meta.sop_instance_uid);
            }
        }
    }

    // Also check hot to cold (if warm tier is skipped)
    if (!warm_tier_ && cold_tier_) {
        auto candidates =
            get_migration_candidates(storage_tier::hot, storage_tier::cold);

        for (const auto& meta : candidates) {
            if (result.instances_migrated >=
                config_.policy.max_instances_per_cycle) {
                break;
            }
            if (result.bytes_migrated >= config_.policy.max_bytes_per_cycle) {
                break;
            }

            auto migrate_result = migrate_instance(
                meta.sop_instance_uid, storage_tier::hot, storage_tier::cold);

            if (migrate_result.is_ok()) {
                result.instances_migrated++;
                result.bytes_migrated += meta.size_bytes;
            } else {
                result.failed_uids.push_back(meta.sop_instance_uid);
            }
        }
    }

    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    return result;
}

auto hsm_storage::get_tier_policy() const -> tier_policy {
    std::shared_lock lock(mutex_);
    return config_.policy;
}

void hsm_storage::set_tier_policy(const tier_policy& policy) {
    std::unique_lock lock(mutex_);
    config_.policy = policy;
}

auto hsm_storage::get_hsm_statistics() const -> hsm_statistics {
    hsm_statistics stats;

    std::shared_lock lock(mutex_);

    std::set<std::string> hot_studies, hot_series;
    std::set<std::string> warm_studies, warm_series;
    std::set<std::string> cold_studies, cold_series;

    for (const auto& [uid, meta] : metadata_index_) {
        switch (meta.current_tier) {
            case storage_tier::hot:
                stats.hot.instance_count++;
                stats.hot.total_bytes += meta.size_bytes;
                if (!meta.study_instance_uid.empty()) {
                    hot_studies.insert(meta.study_instance_uid);
                }
                if (!meta.series_instance_uid.empty()) {
                    hot_series.insert(meta.series_instance_uid);
                }
                break;
            case storage_tier::warm:
                stats.warm.instance_count++;
                stats.warm.total_bytes += meta.size_bytes;
                if (!meta.study_instance_uid.empty()) {
                    warm_studies.insert(meta.study_instance_uid);
                }
                if (!meta.series_instance_uid.empty()) {
                    warm_series.insert(meta.series_instance_uid);
                }
                break;
            case storage_tier::cold:
                stats.cold.instance_count++;
                stats.cold.total_bytes += meta.size_bytes;
                if (!meta.study_instance_uid.empty()) {
                    cold_studies.insert(meta.study_instance_uid);
                }
                if (!meta.series_instance_uid.empty()) {
                    cold_series.insert(meta.series_instance_uid);
                }
                break;
        }
    }

    stats.hot.study_count = hot_studies.size();
    stats.hot.series_count = hot_series.size();
    stats.warm.study_count = warm_studies.size();
    stats.warm.series_count = warm_series.size();
    stats.cold.study_count = cold_studies.size();
    stats.cold.series_count = cold_series.size();

    return stats;
}

auto hsm_storage::get_tier_storage(storage_tier tier) const
    -> storage_interface* {
    return get_storage(tier);
}

// ============================================================================
// Internal Helper Methods
// ============================================================================

auto hsm_storage::find_tier(std::string_view sop_instance_uid) const
    -> std::optional<storage_tier> {
    std::shared_lock lock(mutex_);
    auto it = metadata_index_.find(std::string(sop_instance_uid));
    if (it != metadata_index_.end()) {
        return it->second.current_tier;
    }

    // Fallback: search each tier directly
    lock.unlock();

    if (hot_tier_ && hot_tier_->exists(sop_instance_uid)) {
        return storage_tier::hot;
    }
    if (warm_tier_ && warm_tier_->exists(sop_instance_uid)) {
        return storage_tier::warm;
    }
    if (cold_tier_ && cold_tier_->exists(sop_instance_uid)) {
        return storage_tier::cold;
    }

    return std::nullopt;
}

auto hsm_storage::get_storage(storage_tier tier) const -> storage_interface* {
    switch (tier) {
        case storage_tier::hot:
            return hot_tier_.get();
        case storage_tier::warm:
            return warm_tier_.get();
        case storage_tier::cold:
            return cold_tier_.get();
    }
    return nullptr;
}

void hsm_storage::update_metadata(std::string_view sop_instance_uid,
                                   storage_tier tier,
                                   const core::dicom_dataset& dataset) {
    std::string uid(sop_instance_uid);

    tier_metadata meta;
    meta.sop_instance_uid = uid;
    meta.current_tier = tier;
    meta.stored_at = std::chrono::system_clock::now();
    meta.study_instance_uid =
        dataset.get_string(core::tags::study_instance_uid);
    meta.series_instance_uid =
        dataset.get_string(core::tags::series_instance_uid);

    // Try to get size from dataset (if available)
    // For now, estimate based on pixel data if present
    meta.size_bytes = 0;  // Will be updated by storage backend

    metadata_index_[uid] = std::move(meta);
}

void hsm_storage::update_access_time(std::string_view sop_instance_uid) {
    auto it = metadata_index_.find(std::string(sop_instance_uid));
    if (it != metadata_index_.end()) {
        it->second.last_accessed = std::chrono::system_clock::now();
    }
}

void hsm_storage::remove_metadata(std::string_view sop_instance_uid) {
    metadata_index_.erase(std::string(sop_instance_uid));
}

auto hsm_storage::migrate_instance(std::string_view uid, storage_tier from_tier,
                                    storage_tier to_tier) -> VoidResult {
    auto* source = get_storage(from_tier);
    auto* target = get_storage(to_tier);

    if (source == nullptr) {
        return make_error<std::monostate>(
            kTierNotAvailable,
            "Source tier not available: " + std::string(to_string(from_tier)),
            "hsm_storage");
    }
    if (target == nullptr) {
        return make_error<std::monostate>(
            kTierNotAvailable,
            "Target tier not available: " + std::string(to_string(to_tier)),
            "hsm_storage");
    }

    // Retrieve from source
    auto retrieve_result = source->retrieve(uid);
    if (!retrieve_result.is_ok()) {
        return make_error<std::monostate>(
            kMigrationFailed,
            "Failed to retrieve from source: " +
                std::string(retrieve_result.error().message),
            "hsm_storage");
    }

    auto& dataset = retrieve_result.value();

    // Store to target
    auto store_result = target->store(dataset);
    if (!store_result.is_ok()) {
        return make_error<std::monostate>(
            kMigrationFailed,
            "Failed to store to target: " +
                std::string(store_result.error().message),
            "hsm_storage");
    }

    // Verify if configured
    if (config_.verify_after_migration) {
        if (!target->exists(uid)) {
            return make_error<std::monostate>(
                kMigrationFailed,
                "Verification failed: instance not found in target tier",
                "hsm_storage");
        }
    }

    // Remove from source if configured
    if (config_.delete_after_migration) {
        auto remove_result = source->remove(uid);
        // Ignore remove failure - the instance is already in target tier
        (void)remove_result;
    }

    // Update metadata
    {
        std::unique_lock lock(mutex_);
        auto it = metadata_index_.find(std::string(uid));
        if (it != metadata_index_.end()) {
            it->second.current_tier = to_tier;
        }
    }

    return ok();
}

}  // namespace pacs::storage
