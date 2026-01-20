/**
 * @file sync_manager.cpp
 * @brief Implementation of the Sync Manager
 *
 * @see Issue #542 - Implement Sync Manager for Bidirectional Synchronization
 * @see Issue #530 - PACS Client System Support (Parent Epic)
 */

#include "pacs/client/sync_manager.hpp"
#include "pacs/client/job_manager.hpp"
#include "pacs/client/remote_node_manager.hpp"
#include "pacs/storage/sync_repository.hpp"
#include "pacs/services/query_scu.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/encoding/vr_type.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iomanip>
#include <mutex>
#include <random>
#include <shared_mutex>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <unordered_set>

namespace pacs::client {

// =============================================================================
// UUID Generation
// =============================================================================

namespace {

std::string generate_uuid() {
    static thread_local std::random_device rd;
    static thread_local std::mt19937_64 gen(rd());
    static thread_local std::uniform_int_distribution<uint64_t> dis;

    uint64_t ab = dis(gen);
    uint64_t cd = dis(gen);

    ab = (ab & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
    cd = (cd & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    oss << std::setw(8) << (ab >> 32);
    oss << '-';
    oss << std::setw(4) << ((ab >> 16) & 0xFFFF);
    oss << '-';
    oss << std::setw(4) << (ab & 0xFFFF);
    oss << '-';
    oss << std::setw(4) << (cd >> 48);
    oss << '-';
    oss << std::setw(12) << (cd & 0xFFFFFFFFFFFFULL);

    return oss.str();
}

}  // namespace

// =============================================================================
// Implementation Structure
// =============================================================================

struct sync_manager::impl {
    // Configuration
    sync_manager_config config;

    // Dependencies
    std::shared_ptr<storage::sync_repository> repo;
    std::shared_ptr<remote_node_manager> node_manager;
    std::shared_ptr<job_manager> job_mgr;
    std::shared_ptr<services::query_scu> query_scu;
    std::shared_ptr<di::ILogger> logger;

    // Config cache
    std::vector<sync_config> configs;
    mutable std::shared_mutex configs_mutex;

    // Active syncs tracking
    std::unordered_set<std::string> active_sync_config_ids;
    mutable std::mutex active_mutex;

    // Last results cache
    std::unordered_map<std::string, sync_result> last_results;
    mutable std::shared_mutex results_mutex;

    // Conflicts storage
    std::vector<sync_conflict> conflicts;
    mutable std::mutex conflicts_mutex;

    // Scheduler
    std::thread scheduler_thread;
    std::atomic<bool> scheduler_running{false};
    std::condition_variable scheduler_cv;
    std::mutex scheduler_mutex;

    // Callbacks
    sync_progress_callback progress_callback;
    sync_completion_callback completion_callback;
    sync_conflict_callback conflict_callback;
    mutable std::shared_mutex callbacks_mutex;

    // Completion promises
    std::unordered_map<std::string, std::shared_ptr<std::promise<sync_result>>>
        completion_promises;
    mutable std::mutex promises_mutex;

    // Statistics
    std::atomic<size_t> total_syncs{0};
    std::atomic<size_t> successful_syncs{0};
    std::atomic<size_t> failed_syncs{0};
    std::atomic<size_t> total_studies_synced{0};
    std::atomic<size_t> total_bytes_transferred{0};
    std::atomic<size_t> total_conflicts_detected{0};
    std::atomic<size_t> total_conflicts_resolved{0};

    // =========================================================================
    // Helper Methods
    // =========================================================================

    void save_config(const sync_config& cfg) {
        {
            std::unique_lock lock(configs_mutex);
            auto it = std::find_if(configs.begin(), configs.end(),
                [&cfg](const sync_config& c) {
                    return c.config_id == cfg.config_id;
                });
            if (it != configs.end()) {
                *it = cfg;
            } else {
                configs.push_back(cfg);
            }
        }

        if (repo) {
            [[maybe_unused]] auto result = repo->save_config(cfg);
        }
    }

    std::optional<sync_config> get_config_from_cache(std::string_view config_id) const {
        std::shared_lock lock(configs_mutex);
        auto it = std::find_if(configs.begin(), configs.end(),
            [&config_id](const sync_config& c) {
                return c.config_id == config_id;
            });
        if (it != configs.end()) {
            return *it;
        }
        return std::nullopt;
    }

    void update_config_stats(std::string_view config_id, bool success,
                             size_t studies_synced) {
        std::unique_lock lock(configs_mutex);
        auto it = std::find_if(configs.begin(), configs.end(),
            [&config_id](const sync_config& c) {
                return c.config_id == config_id;
            });
        if (it != configs.end()) {
            it->total_syncs++;
            it->studies_synced += studies_synced;
            it->last_sync = std::chrono::system_clock::now();
            if (success) {
                it->last_successful_sync = std::chrono::system_clock::now();
            }
        }
    }

    void mark_sync_active(std::string_view config_id) {
        std::lock_guard lock(active_mutex);
        active_sync_config_ids.insert(std::string(config_id));
    }

    void mark_sync_inactive(std::string_view config_id) {
        std::lock_guard lock(active_mutex);
        active_sync_config_ids.erase(std::string(config_id));
    }

    bool is_sync_active(std::string_view config_id) const {
        std::lock_guard lock(active_mutex);
        return active_sync_config_ids.count(std::string(config_id)) > 0;
    }

    void store_last_result(const std::string& config_id, const sync_result& result) {
        std::unique_lock lock(results_mutex);
        last_results[config_id] = result;
    }

    void notify_progress(const std::string& config_id, size_t synced, size_t total) {
        std::shared_lock lock(callbacks_mutex);
        if (progress_callback) {
            progress_callback(config_id, synced, total);
        }
    }

    void notify_completion(const std::string& config_id, const sync_result& result) {
        {
            std::shared_lock lock(callbacks_mutex);
            if (completion_callback) {
                completion_callback(config_id, result);
            }
        }

        // Fulfill promise if exists
        {
            std::lock_guard lock(promises_mutex);
            auto it = completion_promises.find(result.job_id);
            if (it != completion_promises.end()) {
                it->second->set_value(result);
                completion_promises.erase(it);
            }
        }
    }

    void notify_conflict(const sync_conflict& conflict) {
        std::shared_lock lock(callbacks_mutex);
        if (conflict_callback) {
            conflict_callback(conflict);
        }
    }

    void add_conflict(const sync_conflict& conflict) {
        {
            std::lock_guard lock(conflicts_mutex);
            // Check for existing conflict for same study
            auto it = std::find_if(conflicts.begin(), conflicts.end(),
                [&conflict](const sync_conflict& c) {
                    return c.study_uid == conflict.study_uid &&
                           c.config_id == conflict.config_id;
                });
            if (it != conflicts.end()) {
                *it = conflict;  // Update existing
            } else {
                conflicts.push_back(conflict);
            }
        }

        if (repo) {
            [[maybe_unused]] auto result = repo->save_conflict(conflict);
        }

        total_conflicts_detected.fetch_add(1, std::memory_order_relaxed);
        notify_conflict(conflict);
    }

    // =========================================================================
    // Sync Execution
    // =========================================================================

    sync_result perform_sync(const sync_config& cfg, bool full_sync) {
        sync_result result;
        result.config_id = cfg.config_id;
        result.job_id = generate_uuid();
        result.started_at = std::chrono::system_clock::now();

        if (logger) {
            logger->info_fmt("Starting {} sync for config '{}' from node '{}'",
                full_sync ? "full" : "incremental", cfg.config_id, cfg.source_node_id);
        }

        // Get the source node
        auto node_opt = node_manager->get_node(cfg.source_node_id);
        if (!node_opt) {
            result.success = false;
            result.errors.push_back("Source node not found: " + cfg.source_node_id);
            result.completed_at = std::chrono::system_clock::now();
            result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                result.completed_at - result.started_at);
            return result;
        }

        const auto& node = *node_opt;
        if (!node.is_online()) {
            result.success = false;
            result.errors.push_back("Source node is offline: " + cfg.source_node_id);
            result.completed_at = std::chrono::system_clock::now();
            result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                result.completed_at - result.started_at);
            return result;
        }

        if (!node.supports_find) {
            result.success = false;
            result.errors.push_back("Node does not support C-FIND");
            result.completed_at = std::chrono::system_clock::now();
            result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                result.completed_at - result.started_at);
            return result;
        }

        // Determine sync start time based on full_sync or incremental
        auto sync_start_time = full_sync
            ? std::chrono::system_clock::now() - cfg.lookback
            : cfg.last_successful_sync;

        // If no successful sync and not full sync, use lookback
        if (!full_sync && sync_start_time == std::chrono::system_clock::time_point{}) {
            sync_start_time = std::chrono::system_clock::now() - cfg.lookback;
        }

        // Query remote studies
        auto remote_studies = query_remote_studies(cfg, sync_start_time, result);
        if (!result.errors.empty()) {
            result.success = false;
            result.completed_at = std::chrono::system_clock::now();
            result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                result.completed_at - result.started_at);
            return result;
        }

        result.studies_checked = remote_studies.size();

        // Compare with local studies
        auto comparison = compare_with_local(cfg, remote_studies, result);

        // Process differences based on sync direction
        if (cfg.direction == sync_direction::pull ||
            cfg.direction == sync_direction::bidirectional) {
            // Pull missing studies from remote
            for (const auto& conflict : comparison) {
                if (conflict.conflict_type == sync_conflict_type::missing_local) {
                    // Create retrieve job
                    if (job_mgr && node.supports_query_retrieve()) {
                        auto job_id = job_mgr->create_retrieve_job(
                            cfg.source_node_id, conflict.study_uid, std::nullopt,
                            job_priority::normal);
                        if (!job_id.empty()) {
                            result.studies_synced++;
                            notify_progress(cfg.config_id, result.studies_synced,
                                          result.studies_checked);
                        }
                    }
                } else if (conflict.conflict_type == sync_conflict_type::count_mismatch ||
                          conflict.conflict_type == sync_conflict_type::modified) {
                    // Handle conflicts
                    if (config.auto_resolve_conflicts) {
                        resolve_conflict_internal(conflict, config.default_resolution, result);
                    } else {
                        add_conflict(conflict);
                        result.conflicts.push_back(conflict);
                    }
                }
            }
        }

        if (cfg.direction == sync_direction::push ||
            cfg.direction == sync_direction::bidirectional) {
            // Push studies missing on remote
            for (const auto& conflict : comparison) {
                if (conflict.conflict_type == sync_conflict_type::missing_remote) {
                    // Store job would be created here for push
                    if (logger) {
                        logger->info_fmt("Study {} is missing on remote, would create store job",
                            conflict.study_uid);
                    }
                    result.studies_synced++;
                }
            }
        }

        // Update statistics
        total_syncs.fetch_add(1, std::memory_order_relaxed);
        total_studies_synced.fetch_add(result.studies_synced, std::memory_order_relaxed);

        result.success = result.errors.empty();
        result.completed_at = std::chrono::system_clock::now();
        result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            result.completed_at - result.started_at);

        if (result.success) {
            successful_syncs.fetch_add(1, std::memory_order_relaxed);
        } else {
            failed_syncs.fetch_add(1, std::memory_order_relaxed);
        }

        // Update config statistics
        update_config_stats(cfg.config_id, result.success, result.studies_synced);

        // Store result
        store_last_result(cfg.config_id, result);

        // Save history
        if (repo) {
            sync_history history;
            history.config_id = cfg.config_id;
            history.job_id = result.job_id;
            history.success = result.success;
            history.studies_checked = result.studies_checked;
            history.studies_synced = result.studies_synced;
            history.conflicts_found = result.conflicts.size();
            history.errors = result.errors;
            history.started_at = result.started_at;
            history.completed_at = result.completed_at;
            [[maybe_unused]] auto save_result = repo->save_history(history);
        }

        if (logger) {
            logger->info_fmt("Sync completed for config '{}': {} checked, {} synced, {} conflicts",
                cfg.config_id, result.studies_checked, result.studies_synced,
                result.conflicts.size());
        }

        return result;
    }

    std::vector<std::string> query_remote_studies(
        const sync_config& cfg,
        std::chrono::system_clock::time_point since,
        sync_result& result) {

        std::vector<std::string> study_uids;

        // Get association for query
        std::vector<std::string> sop_classes = {
            std::string(services::study_root_find_sop_class_uid)
        };

        auto assoc_result = node_manager->acquire_association(
            cfg.source_node_id, sop_classes);
        if (assoc_result.is_err()) {
            result.errors.push_back("Failed to acquire association: " +
                                    assoc_result.error().message);
            return study_uids;
        }

        auto assoc = std::move(assoc_result.value());

        // Build query
        core::dicom_dataset query_keys;
        query_keys.set_string(core::tags::query_retrieve_level, encoding::vr_type::CS, "STUDY");
        query_keys.set_string(core::tags::study_instance_uid, encoding::vr_type::UI, "");

        // Apply filters
        if (!cfg.modalities.empty()) {
            std::string modality_filter;
            for (size_t i = 0; i < cfg.modalities.size(); ++i) {
                if (i > 0) modality_filter += "\\";
                modality_filter += cfg.modalities[i];
            }
            query_keys.set_string(core::tags::modality, encoding::vr_type::CS, modality_filter);
        }

        // Date range filter
        if (since != std::chrono::system_clock::time_point{}) {
            auto since_time_t = std::chrono::system_clock::to_time_t(since);
            std::tm tm_buf;
#ifdef _WIN32
            localtime_s(&tm_buf, &since_time_t);
#else
            localtime_r(&since_time_t, &tm_buf);
#endif
            std::ostringstream date_oss;
            date_oss << std::put_time(&tm_buf, "%Y%m%d") << "-";
            query_keys.set_string(core::tags::study_date, encoding::vr_type::DA, date_oss.str());
        }

        // Execute query
        services::query_scu_config query_config;
        query_config.model = services::query_model::study_root;
        query_config.level = services::query_level::study;

        services::query_scu scu(query_config, logger);
        auto query_result = scu.find(*assoc, query_keys);

        // Release association
        node_manager->release_association(cfg.source_node_id, std::move(assoc));

        if (query_result.is_err()) {
            result.errors.push_back("Query failed: " + query_result.error().message);
            return study_uids;
        }

        const auto& qr = query_result.value();
        for (const auto& match : qr.matches) {
            auto uid = match.get_string(core::tags::study_instance_uid);
            if (!uid.empty()) {
                study_uids.push_back(uid);
            }
        }

        return study_uids;
    }

    std::vector<sync_conflict> compare_with_local(
        const sync_config& cfg,
        const std::vector<std::string>& remote_study_uids,
        sync_result& result) {

        std::vector<sync_conflict> comparison_conflicts;

        // For each remote study, check if it exists locally
        for (const auto& study_uid : remote_study_uids) {
            // In real implementation, query local database
            // For now, we'll simulate that all remote studies are missing locally
            bool exists_locally = false;  // Would check storage index here

            if (!exists_locally) {
                sync_conflict conflict;
                conflict.config_id = cfg.config_id;
                conflict.study_uid = study_uid;
                conflict.conflict_type = sync_conflict_type::missing_local;
                conflict.remote_modified = std::chrono::system_clock::now();
                conflict.detected_at = std::chrono::system_clock::now();
                comparison_conflicts.push_back(conflict);
            }
        }

        return comparison_conflicts;
    }

    void resolve_conflict_internal(const sync_conflict& conflict,
                                   conflict_resolution resolution,
                                   sync_result& result) {
        switch (resolution) {
            case conflict_resolution::prefer_remote:
                // Create retrieve job
                if (job_mgr) {
                    auto job_id = job_mgr->create_retrieve_job(
                        conflict.config_id, conflict.study_uid, std::nullopt,
                        job_priority::normal);
                    if (!job_id.empty()) {
                        result.studies_synced++;
                    }
                }
                break;

            case conflict_resolution::prefer_local:
                // Keep local, skip
                result.studies_skipped++;
                break;

            case conflict_resolution::prefer_newer:
                // Compare timestamps and choose
                if (conflict.remote_modified > conflict.local_modified) {
                    if (job_mgr) {
                        auto job_id = job_mgr->create_retrieve_job(
                            conflict.config_id, conflict.study_uid, std::nullopt,
                            job_priority::normal);
                        if (!job_id.empty()) {
                            result.studies_synced++;
                        }
                    }
                } else {
                    result.studies_skipped++;
                }
                break;
        }

        total_conflicts_resolved.fetch_add(1, std::memory_order_relaxed);
    }

    // =========================================================================
    // Scheduler
    // =========================================================================

    void scheduler_loop() {
        while (scheduler_running.load()) {
            std::unique_lock lock(scheduler_mutex);
            scheduler_cv.wait_for(lock, std::chrono::minutes(1), [this] {
                return !scheduler_running.load();
            });

            if (!scheduler_running.load()) {
                break;
            }

            // Check each config for scheduled syncs
            std::vector<sync_config> configs_copy;
            {
                std::shared_lock cfg_lock(configs_mutex);
                configs_copy = configs;
            }

            for (const auto& cfg : configs_copy) {
                if (!cfg.enabled || cfg.schedule_cron.empty()) {
                    continue;
                }

                // Simple cron check - in production would parse cron expression
                // For now, check if it's time based on last sync
                auto now = std::chrono::system_clock::now();
                auto since_last = now - cfg.last_sync;

                // Default to hourly if cron not parsed
                if (since_last >= std::chrono::hours(1)) {
                    if (!is_sync_active(cfg.config_id)) {
                        if (logger) {
                            logger->info_fmt("Scheduler triggering sync for config '{}'",
                                cfg.config_id);
                        }

                        mark_sync_active(cfg.config_id);
                        auto result = perform_sync(cfg, false);
                        mark_sync_inactive(cfg.config_id);
                        notify_completion(cfg.config_id, result);
                    }
                }
            }
        }
    }

    void load_configs_from_repo() {
        if (!repo) return;

        auto loaded = repo->list_configs();
        {
            std::unique_lock lock(configs_mutex);
            configs = std::move(loaded);
        }

        if (logger && !configs.empty()) {
            logger->info_fmt("Loaded {} sync configs from repository", configs.size());
        }
    }

    void load_conflicts_from_repo() {
        if (!repo) return;

        auto loaded = repo->list_unresolved_conflicts();
        {
            std::lock_guard lock(conflicts_mutex);
            conflicts = std::move(loaded);
        }

        if (logger && !conflicts.empty()) {
            logger->info_fmt("Loaded {} unresolved conflicts from repository", conflicts.size());
        }
    }
};

// =============================================================================
// Construction / Destruction
// =============================================================================

sync_manager::sync_manager(
    std::shared_ptr<storage::sync_repository> repo,
    std::shared_ptr<remote_node_manager> node_manager,
    std::shared_ptr<job_manager> job_manager,
    std::shared_ptr<services::query_scu> query_scu,
    std::shared_ptr<di::ILogger> logger)
    : sync_manager(sync_manager_config{}, std::move(repo), std::move(node_manager),
                   std::move(job_manager), std::move(query_scu), std::move(logger)) {
}

sync_manager::sync_manager(
    const sync_manager_config& config,
    std::shared_ptr<storage::sync_repository> repo,
    std::shared_ptr<remote_node_manager> node_manager,
    std::shared_ptr<job_manager> job_manager,
    std::shared_ptr<services::query_scu> query_scu,
    std::shared_ptr<di::ILogger> logger)
    : impl_(std::make_unique<impl>()) {

    impl_->config = config;
    impl_->repo = std::move(repo);
    impl_->node_manager = std::move(node_manager);
    impl_->job_mgr = std::move(job_manager);
    impl_->query_scu = std::move(query_scu);
    impl_->logger = logger ? std::move(logger) : di::null_logger();

    impl_->load_configs_from_repo();
    impl_->load_conflicts_from_repo();
}

sync_manager::~sync_manager() {
    stop_scheduler();
}

// =============================================================================
// Config CRUD
// =============================================================================

pacs::VoidResult sync_manager::add_config(const sync_config& config) {
    if (config.config_id.empty()) {
        return pacs::pacs_void_error(
            pacs::error_codes::invalid_argument,
            "Config ID cannot be empty");
    }

    if (config.source_node_id.empty()) {
        return pacs::pacs_void_error(
            pacs::error_codes::invalid_argument,
            "Source node ID cannot be empty");
    }

    // Check for duplicate
    if (impl_->get_config_from_cache(config.config_id).has_value()) {
        return pacs::pacs_void_error(
            pacs::error_codes::already_exists,
            "Config already exists: " + config.config_id);
    }

    impl_->save_config(config);

    if (impl_->logger) {
        impl_->logger->info_fmt("Added sync config '{}' for node '{}'",
            config.config_id, config.source_node_id);
    }

    return pacs::ok();
}

pacs::VoidResult sync_manager::update_config(const sync_config& config) {
    if (!impl_->get_config_from_cache(config.config_id).has_value()) {
        return pacs::pacs_void_error(
            pacs::error_codes::not_found,
            "Config not found: " + config.config_id);
    }

    impl_->save_config(config);

    if (impl_->logger) {
        impl_->logger->info_fmt("Updated sync config '{}'", config.config_id);
    }

    return pacs::ok();
}

pacs::VoidResult sync_manager::remove_config(std::string_view config_id) {
    {
        std::unique_lock lock(impl_->configs_mutex);
        auto it = std::find_if(impl_->configs.begin(), impl_->configs.end(),
            [&config_id](const sync_config& c) {
                return c.config_id == config_id;
            });
        if (it == impl_->configs.end()) {
            return pacs::pacs_void_error(
                pacs::error_codes::not_found,
                "Config not found: " + std::string(config_id));
        }
        impl_->configs.erase(it);
    }

    if (impl_->repo) {
        [[maybe_unused]] auto result = impl_->repo->remove_config(config_id);
    }

    if (impl_->logger) {
        impl_->logger->info_fmt("Removed sync config '{}'", config_id);
    }

    return pacs::ok();
}

std::optional<sync_config> sync_manager::get_config(std::string_view config_id) const {
    return impl_->get_config_from_cache(config_id);
}

std::vector<sync_config> sync_manager::list_configs() const {
    std::shared_lock lock(impl_->configs_mutex);
    return impl_->configs;
}

// =============================================================================
// Manual Sync Operations
// =============================================================================

std::string sync_manager::sync_now(std::string_view config_id) {
    auto config_opt = impl_->get_config_from_cache(config_id);
    if (!config_opt) {
        return "";
    }

    impl_->mark_sync_active(config_id);

    // Determine if incremental or full based on last_successful_sync
    bool full = config_opt->last_successful_sync == std::chrono::system_clock::time_point{};
    auto result = impl_->perform_sync(*config_opt, full);

    impl_->mark_sync_inactive(config_id);
    impl_->notify_completion(std::string(config_id), result);

    return result.job_id;
}

std::string sync_manager::full_sync(std::string_view config_id) {
    auto config_opt = impl_->get_config_from_cache(config_id);
    if (!config_opt) {
        return "";
    }

    impl_->mark_sync_active(config_id);
    auto result = impl_->perform_sync(*config_opt, true);
    impl_->mark_sync_inactive(config_id);
    impl_->notify_completion(std::string(config_id), result);

    return result.job_id;
}

std::string sync_manager::incremental_sync(std::string_view config_id) {
    auto config_opt = impl_->get_config_from_cache(config_id);
    if (!config_opt) {
        return "";
    }

    impl_->mark_sync_active(config_id);
    auto result = impl_->perform_sync(*config_opt, false);
    impl_->mark_sync_inactive(config_id);
    impl_->notify_completion(std::string(config_id), result);

    return result.job_id;
}

std::future<sync_result> sync_manager::wait_for_sync(std::string_view job_id) {
    auto promise = std::make_shared<std::promise<sync_result>>();
    auto future = promise->get_future();

    {
        std::lock_guard lock(impl_->promises_mutex);
        impl_->completion_promises[std::string(job_id)] = promise;
    }

    return future;
}

// =============================================================================
// Comparison (Dry Run)
// =============================================================================

sync_result sync_manager::compare(std::string_view config_id) {
    sync_result result;
    result.config_id = std::string(config_id);
    result.started_at = std::chrono::system_clock::now();

    auto config_opt = impl_->get_config_from_cache(config_id);
    if (!config_opt) {
        result.errors.push_back("Config not found: " + std::string(config_id));
        result.completed_at = std::chrono::system_clock::now();
        return result;
    }

    const auto& cfg = *config_opt;

    // Query remote studies
    auto sync_start_time = std::chrono::system_clock::now() - cfg.lookback;
    auto remote_studies = impl_->query_remote_studies(cfg, sync_start_time, result);

    if (!result.errors.empty()) {
        result.completed_at = std::chrono::system_clock::now();
        return result;
    }

    result.studies_checked = remote_studies.size();

    // Compare with local
    auto conflicts = impl_->compare_with_local(cfg, remote_studies, result);
    result.conflicts = conflicts;

    result.success = true;
    result.completed_at = std::chrono::system_clock::now();
    result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        result.completed_at - result.started_at);

    return result;
}

std::future<sync_result> sync_manager::compare_async(std::string_view config_id) {
    return std::async(std::launch::async, [this, id = std::string(config_id)]() {
        return compare(id);
    });
}

// =============================================================================
// Conflict Management
// =============================================================================

std::vector<sync_conflict> sync_manager::get_conflicts() const {
    std::lock_guard lock(impl_->conflicts_mutex);
    std::vector<sync_conflict> unresolved;
    std::copy_if(impl_->conflicts.begin(), impl_->conflicts.end(),
                 std::back_inserter(unresolved),
                 [](const sync_conflict& c) { return !c.resolved; });
    return unresolved;
}

std::vector<sync_conflict> sync_manager::get_conflicts(std::string_view config_id) const {
    std::lock_guard lock(impl_->conflicts_mutex);
    std::vector<sync_conflict> filtered;
    std::copy_if(impl_->conflicts.begin(), impl_->conflicts.end(),
                 std::back_inserter(filtered),
                 [&config_id](const sync_conflict& c) {
                     return !c.resolved && c.config_id == config_id;
                 });
    return filtered;
}

pacs::VoidResult sync_manager::resolve_conflict(
    std::string_view study_uid,
    conflict_resolution resolution) {

    std::lock_guard lock(impl_->conflicts_mutex);

    auto it = std::find_if(impl_->conflicts.begin(), impl_->conflicts.end(),
        [&study_uid](const sync_conflict& c) {
            return c.study_uid == study_uid && !c.resolved;
        });

    if (it == impl_->conflicts.end()) {
        return pacs::pacs_void_error(
            pacs::error_codes::not_found,
            "Conflict not found: " + std::string(study_uid));
    }

    // Apply resolution
    sync_result dummy_result;
    impl_->resolve_conflict_internal(*it, resolution, dummy_result);

    it->resolved = true;
    it->resolution_used = resolution;
    it->resolved_at = std::chrono::system_clock::now();

    if (impl_->repo) {
        [[maybe_unused]] auto result = impl_->repo->resolve_conflict(study_uid, resolution);
    }

    if (impl_->logger) {
        impl_->logger->info_fmt("Resolved conflict for study '{}' using {}",
            study_uid, to_string(resolution));
    }

    return pacs::ok();
}

pacs::VoidResult sync_manager::resolve_all_conflicts(
    std::string_view config_id,
    conflict_resolution resolution) {

    std::vector<std::string> study_uids;

    {
        std::lock_guard lock(impl_->conflicts_mutex);
        for (const auto& c : impl_->conflicts) {
            if (c.config_id == config_id && !c.resolved) {
                study_uids.push_back(c.study_uid);
            }
        }
    }

    for (const auto& uid : study_uids) {
        auto result = resolve_conflict(uid, resolution);
        if (result.is_err()) {
            return result;
        }
    }

    if (impl_->logger) {
        impl_->logger->info_fmt("Resolved {} conflicts for config '{}'",
            study_uids.size(), config_id);
    }

    return pacs::ok();
}

// =============================================================================
// Scheduler
// =============================================================================

void sync_manager::start_scheduler() {
    if (impl_->scheduler_running.load()) {
        return;
    }

    impl_->scheduler_running.store(true);
    impl_->scheduler_thread = std::thread([this]() {
        impl_->scheduler_loop();
    });

    if (impl_->logger) {
        impl_->logger->info("Sync scheduler started");
    }
}

void sync_manager::stop_scheduler() {
    if (!impl_->scheduler_running.load()) {
        return;
    }

    impl_->scheduler_running.store(false);
    impl_->scheduler_cv.notify_all();

    if (impl_->scheduler_thread.joinable()) {
        impl_->scheduler_thread.join();
    }

    if (impl_->logger) {
        impl_->logger->info("Sync scheduler stopped");
    }
}

bool sync_manager::is_scheduler_running() const noexcept {
    return impl_->scheduler_running.load();
}

// =============================================================================
// Status
// =============================================================================

bool sync_manager::is_syncing(std::string_view config_id) const {
    return impl_->is_sync_active(config_id);
}

sync_result sync_manager::get_last_result(std::string_view config_id) const {
    std::shared_lock lock(impl_->results_mutex);
    auto it = impl_->last_results.find(std::string(config_id));
    if (it != impl_->last_results.end()) {
        return it->second;
    }
    return sync_result{};
}

// =============================================================================
// Statistics
// =============================================================================

sync_statistics sync_manager::get_statistics() const {
    sync_statistics stats;
    stats.total_syncs = impl_->total_syncs.load(std::memory_order_relaxed);
    stats.successful_syncs = impl_->successful_syncs.load(std::memory_order_relaxed);
    stats.failed_syncs = impl_->failed_syncs.load(std::memory_order_relaxed);
    stats.total_studies_synced = impl_->total_studies_synced.load(std::memory_order_relaxed);
    stats.total_bytes_transferred = impl_->total_bytes_transferred.load(std::memory_order_relaxed);
    stats.total_conflicts_detected = impl_->total_conflicts_detected.load(std::memory_order_relaxed);
    stats.total_conflicts_resolved = impl_->total_conflicts_resolved.load(std::memory_order_relaxed);
    return stats;
}

sync_statistics sync_manager::get_statistics(std::string_view config_id) const {
    sync_statistics stats;

    auto config_opt = impl_->get_config_from_cache(config_id);
    if (config_opt) {
        stats.total_syncs = config_opt->total_syncs;
        stats.total_studies_synced = config_opt->studies_synced;
    }

    // Count conflicts for this config
    {
        std::lock_guard lock(impl_->conflicts_mutex);
        for (const auto& c : impl_->conflicts) {
            if (c.config_id == config_id) {
                stats.total_conflicts_detected++;
                if (c.resolved) {
                    stats.total_conflicts_resolved++;
                }
            }
        }
    }

    return stats;
}

// =============================================================================
// Callbacks
// =============================================================================

void sync_manager::set_progress_callback(sync_progress_callback callback) {
    std::unique_lock lock(impl_->callbacks_mutex);
    impl_->progress_callback = std::move(callback);
}

void sync_manager::set_completion_callback(sync_completion_callback callback) {
    std::unique_lock lock(impl_->callbacks_mutex);
    impl_->completion_callback = std::move(callback);
}

void sync_manager::set_conflict_callback(sync_conflict_callback callback) {
    std::unique_lock lock(impl_->callbacks_mutex);
    impl_->conflict_callback = std::move(callback);
}

// =============================================================================
// Configuration
// =============================================================================

const sync_manager_config& sync_manager::config() const noexcept {
    return impl_->config;
}

}  // namespace pacs::client
