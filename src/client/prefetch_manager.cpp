/**
 * @file prefetch_manager.cpp
 * @brief Implementation of the Prefetch Manager
 *
 * @see Issue #541 - Implement Prefetch Manager for Proactive Data Loading
 * @see Issue #530 - PACS Client System Support (Parent Epic)
 */

#include "pacs/client/prefetch_manager.hpp"
#include "pacs/client/job_manager.hpp"
#include "pacs/client/remote_node_manager.hpp"
#include "pacs/storage/prefetch_repository.hpp"
#include "pacs/services/worklist_scu.hpp"
#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_tag_constants.hpp"

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
#include <unordered_set>

namespace pacs::client {

// =============================================================================
// UUID Generation
// =============================================================================

namespace {

/**
 * @brief Generate a UUID v4 string
 */
std::string generate_uuid() {
    static thread_local std::random_device rd;
    static thread_local std::mt19937_64 gen(rd());
    static thread_local std::uniform_int_distribution<uint64_t> dis;

    uint64_t ab = dis(gen);
    uint64_t cd = dis(gen);

    // Set version (4) and variant (8, 9, A, or B)
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

/**
 * @brief Check if a string matches a comma-separated filter
 */
bool matches_filter(const std::string& value, const std::string& filter) {
    if (filter.empty()) return true;
    if (value.empty()) return false;

    size_t start = 0;
    while (start < filter.size()) {
        auto end = filter.find(',', start);
        if (end == std::string::npos) {
            end = filter.size();
        }

        auto pattern = filter.substr(start, end - start);
        // Trim whitespace
        while (!pattern.empty() && pattern.front() == ' ') pattern.erase(0, 1);
        while (!pattern.empty() && pattern.back() == ' ') pattern.pop_back();

        if (pattern == value) {
            return true;
        }

        start = end + 1;
    }

    return false;
}

/**
 * @brief Get DICOM tag value from dataset using the dataset's get_string method
 */
std::string get_dataset_value(const core::dicom_dataset& ds, core::dicom_tag tag) {
    return ds.get_string(tag);
}

// Local tag definitions for tags not in constants file
constexpr core::dicom_tag body_part_examined_tag{0x0018, 0x0015};

}  // namespace

// =============================================================================
// Implementation Structure
// =============================================================================

struct prefetch_manager::impl {
    // Configuration
    prefetch_manager_config config;

    // Dependencies
    std::shared_ptr<storage::prefetch_repository> repo;
    std::shared_ptr<remote_node_manager> node_manager;
    std::shared_ptr<job_manager> job_mgr;
    std::shared_ptr<services::worklist_scu> worklist_scu;
    std::shared_ptr<di::ILogger> logger;

    // Rules cache
    std::vector<prefetch_rule> rules_cache;
    mutable std::shared_mutex rules_mutex;

    // Scheduler thread
    std::thread scheduler_thread;
    std::atomic<bool> scheduler_running{false};
    std::condition_variable scheduler_cv;
    std::mutex scheduler_mutex;

    // Worklist monitor thread
    std::thread worklist_monitor_thread;
    std::atomic<bool> worklist_monitor_running{false};
    std::string worklist_node_id;
    std::condition_variable worklist_cv;
    std::mutex worklist_mutex;

    // Deduplication
    std::unordered_set<std::string> pending_study_uids;
    mutable std::mutex pending_mutex;

    // Statistics
    std::atomic<size_t> pending_count{0};

    // =========================================================================
    // Helper Methods
    // =========================================================================

    void load_rules_from_repo() {
        if (!repo) return;

        std::unique_lock lock(rules_mutex);
        rules_cache = repo->find_enabled_rules();
    }

    void save_rule_to_repo(const prefetch_rule& rule) {
        if (repo) {
            [[maybe_unused]] auto result = repo->save_rule(rule);
        }
    }

    bool is_study_pending(const std::string& study_uid) {
        std::lock_guard lock(pending_mutex);
        return pending_study_uids.count(study_uid) > 0;
    }

    void mark_study_pending(const std::string& study_uid) {
        std::lock_guard lock(pending_mutex);
        pending_study_uids.insert(study_uid);
        pending_count.fetch_add(1);
    }

    void mark_study_complete(const std::string& study_uid) {
        std::lock_guard lock(pending_mutex);
        pending_study_uids.erase(study_uid);
        auto count = pending_count.load();
        if (count > 0) {
            pending_count.fetch_sub(1);
        }
    }

    bool is_study_local(std::string_view study_uid) const {
        // Check history for completed prefetch
        if (repo) {
            return repo->is_study_prefetched(study_uid);
        }
        return false;
    }

    std::vector<prefetch_rule> get_matching_rules(
        prefetch_trigger trigger,
        const std::string& modality,
        const std::string& body_part,
        const std::string& station_ae) {
        std::vector<prefetch_rule> matches;

        std::shared_lock lock(rules_mutex);
        for (const auto& rule : rules_cache) {
            if (!rule.enabled) continue;
            if (rule.trigger != trigger) continue;

            if (!matches_filter(modality, rule.modality_filter)) continue;
            if (!matches_filter(body_part, rule.body_part_filter)) continue;
            if (!matches_filter(station_ae, rule.station_ae_filter)) continue;

            matches.push_back(rule);
        }

        return matches;
    }

    void record_prefetch_history(
        const std::string& patient_id,
        const std::string& study_uid,
        const std::string& rule_id,
        const std::string& source_node_id,
        const std::string& job_id,
        const std::string& status) {
        if (!repo) return;

        prefetch_history history;
        history.patient_id = patient_id;
        history.study_uid = study_uid;
        history.rule_id = rule_id;
        history.source_node_id = source_node_id;
        history.job_id = job_id;
        history.status = status;
        history.prefetched_at = std::chrono::system_clock::now();

        [[maybe_unused]] auto result = repo->save_history(history);
    }

    void increment_rule_stats(const std::string& rule_id, size_t studies) {
        if (!repo) return;

        [[maybe_unused]] auto result1 = repo->increment_triggered(rule_id);
        if (studies > 0) {
            [[maybe_unused]] auto result2 = repo->increment_studies_prefetched(rule_id, studies);
        }
    }

    // =========================================================================
    // Scheduler Loop
    // =========================================================================

    void scheduler_loop() {
        while (scheduler_running.load()) {
            std::unique_lock lock(scheduler_mutex);
            scheduler_cv.wait_for(lock, std::chrono::minutes(1), [this] {
                return !scheduler_running.load();
            });

            if (!scheduler_running.load()) break;

            // Check scheduled rules
            check_scheduled_rules();
        }
    }

    void check_scheduled_rules() {
        std::shared_lock lock(rules_mutex);
        for (const auto& rule : rules_cache) {
            if (!rule.enabled) continue;
            if (rule.trigger != prefetch_trigger::scheduled_exam) continue;
            if (rule.schedule_cron.empty()) continue;

            // Simple cron check - in production would use proper cron parsing
            // For now, just check if rule should run
            // This is a placeholder for actual cron implementation
        }
    }

    // =========================================================================
    // Worklist Monitor Loop
    // =========================================================================

    void worklist_monitor_loop() {
        while (worklist_monitor_running.load()) {
            std::unique_lock lock(worklist_mutex);
            worklist_cv.wait_for(lock, config.worklist_check_interval, [this] {
                return !worklist_monitor_running.load();
            });

            if (!worklist_monitor_running.load()) break;

            // Query worklist and process items
            query_and_process_worklist();
        }
    }

    void query_and_process_worklist() {
        if (!worklist_scu || !node_manager || worklist_node_id.empty()) {
            return;
        }

        // Get node configuration
        auto node = node_manager->get_node(worklist_node_id);
        if (!node) {
            logger->warn_fmt("Worklist node {} not found", worklist_node_id);
            return;
        }

        // Query today's worklist
        // Note: This would establish association and query in production
        // For now, this is a placeholder for the actual worklist query
        logger->debug_fmt("Checking worklist from node {}", worklist_node_id);
    }
};

// =============================================================================
// Construction / Destruction
// =============================================================================

prefetch_manager::prefetch_manager(
    std::shared_ptr<storage::prefetch_repository> repo,
    std::shared_ptr<remote_node_manager> node_manager,
    std::shared_ptr<job_manager> job_manager,
    std::shared_ptr<services::worklist_scu> worklist_scu,
    std::shared_ptr<di::ILogger> logger)
    : prefetch_manager(
          prefetch_manager_config{},
          std::move(repo),
          std::move(node_manager),
          std::move(job_manager),
          std::move(worklist_scu),
          std::move(logger)) {}

prefetch_manager::prefetch_manager(
    const prefetch_manager_config& config,
    std::shared_ptr<storage::prefetch_repository> repo,
    std::shared_ptr<remote_node_manager> node_manager,
    std::shared_ptr<job_manager> job_manager,
    std::shared_ptr<services::worklist_scu> worklist_scu,
    std::shared_ptr<di::ILogger> logger)
    : impl_(std::make_unique<impl>()) {
    impl_->config = config;
    impl_->repo = std::move(repo);
    impl_->node_manager = std::move(node_manager);
    impl_->job_mgr = std::move(job_manager);
    impl_->worklist_scu = std::move(worklist_scu);
    impl_->logger = logger ? std::move(logger) : std::make_shared<di::NullLogger>();

    // Initialize tables if repository exists
    if (impl_->repo) {
        [[maybe_unused]] auto result = impl_->repo->initialize_tables();
    }

    // Load rules from repository
    impl_->load_rules_from_repo();
}

prefetch_manager::~prefetch_manager() {
    stop_scheduler();
    stop_worklist_monitor();
}

// =============================================================================
// Rule Management
// =============================================================================

pacs::VoidResult prefetch_manager::add_rule(const prefetch_rule& rule) {
    prefetch_rule new_rule = rule;
    if (new_rule.rule_id.empty()) {
        new_rule.rule_id = generate_uuid();
    }

    // Save to repository
    if (impl_->repo) {
        auto result = impl_->repo->save_rule(new_rule);
        if (result.is_err()) {
            return result;
        }
    }

    // Update cache
    {
        std::unique_lock lock(impl_->rules_mutex);
        impl_->rules_cache.push_back(new_rule);
    }

    impl_->logger->info_fmt("Added prefetch rule: {} ({})", new_rule.name, new_rule.rule_id);
    return kcenon::common::ok();
}

pacs::VoidResult prefetch_manager::update_rule(const prefetch_rule& rule) {
    if (rule.rule_id.empty()) {
        return pacs::pacs_void_error(-1, "Rule ID is required for update");
    }

    // Save to repository
    if (impl_->repo) {
        auto result = impl_->repo->save_rule(rule);
        if (result.is_err()) {
            return result;
        }
    }

    // Update cache
    {
        std::unique_lock lock(impl_->rules_mutex);
        for (auto& cached_rule : impl_->rules_cache) {
            if (cached_rule.rule_id == rule.rule_id) {
                cached_rule = rule;
                break;
            }
        }
    }

    impl_->logger->info_fmt("Updated prefetch rule: {}", rule.rule_id);
    return kcenon::common::ok();
}

pacs::VoidResult prefetch_manager::remove_rule(std::string_view rule_id) {
    // Remove from repository
    if (impl_->repo) {
        auto result = impl_->repo->remove_rule(rule_id);
        if (result.is_err()) {
            return result;
        }
    }

    // Remove from cache
    {
        std::unique_lock lock(impl_->rules_mutex);
        impl_->rules_cache.erase(
            std::remove_if(impl_->rules_cache.begin(), impl_->rules_cache.end(),
                [&rule_id](const prefetch_rule& r) { return r.rule_id == rule_id; }),
            impl_->rules_cache.end());
    }

    impl_->logger->info_fmt("Removed prefetch rule: {}", std::string(rule_id));
    return kcenon::common::ok();
}

std::optional<prefetch_rule> prefetch_manager::get_rule(std::string_view rule_id) const {
    std::shared_lock lock(impl_->rules_mutex);
    for (const auto& rule : impl_->rules_cache) {
        if (rule.rule_id == rule_id) {
            return rule;
        }
    }
    return std::nullopt;
}

std::vector<prefetch_rule> prefetch_manager::list_rules() const {
    std::shared_lock lock(impl_->rules_mutex);
    return impl_->rules_cache;
}

// =============================================================================
// Worklist-Driven Prefetch
// =============================================================================

void prefetch_manager::process_worklist(
    const std::vector<core::dicom_dataset>& worklist_items) {
    for (const auto& item : worklist_items) {
        // Extract relevant fields from worklist item
        auto patient_id = get_dataset_value(item, core::tags::patient_id);
        auto modality_val = get_dataset_value(item, core::tags::modality);
        auto body_part = get_dataset_value(item, body_part_examined_tag);
        auto station_ae = get_dataset_value(item, core::tags::scheduled_station_ae_title);

        if (patient_id.empty()) {
            continue;
        }

        // Find matching rules
        auto rules = impl_->get_matching_rules(
            prefetch_trigger::worklist_match, modality_val, body_part, station_ae);

        for (const auto& rule : rules) {
            // Trigger prefetch for this patient
            auto result = prefetch_priors(patient_id, modality_val, body_part);

            // Update rule statistics
            impl_->increment_rule_stats(rule.rule_id, result.studies_prefetched);

            impl_->logger->debug_fmt("Worklist prefetch for patient {}: {} studies prefetched",
                patient_id, result.studies_prefetched);
        }
    }
}

std::future<void> prefetch_manager::process_worklist_async(
    const std::vector<core::dicom_dataset>& worklist_items) {
    return std::async(std::launch::async, [this, worklist_items]() {
        process_worklist(worklist_items);
    });
}

// =============================================================================
// Prior Study Prefetch
// =============================================================================

prefetch_result prefetch_manager::prefetch_priors(
    std::string_view patient_id,
    std::string_view current_modality,
    std::optional<std::string_view> body_part) {
    auto start_time = std::chrono::steady_clock::now();
    prefetch_result result;
    result.patient_id = std::string(patient_id);

    // Find matching prior study rules
    auto rules = impl_->get_matching_rules(
        prefetch_trigger::prior_studies,
        std::string(current_modality),
        body_part ? std::string(*body_part) : "",
        "");

    if (rules.empty()) {
        impl_->logger->debug_fmt("No prior study rules match for patient {} modality {}",
            std::string(patient_id), std::string(current_modality));
        return result;
    }

    // Use the first matching rule (or could combine multiple)
    const auto& rule = rules.front();

    // Query prior studies from each source node
    for (const auto& source_node_id : rule.source_node_ids) {
        auto node = impl_->node_manager->get_node(source_node_id);
        if (!node) {
            impl_->logger->warn_fmt("Source node {} not found", source_node_id);
            continue;
        }

        // Query for prior studies
        // In production, this would use query_scu to search for studies
        // For now, create prefetch jobs based on rule configuration

        // Record that we attempted prefetch
        impl_->logger->info_fmt("Prefetching priors for patient {} from node {}",
            std::string(patient_id), source_node_id);
    }

    auto end_time = std::chrono::steady_clock::now();
    result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    return result;
}

std::future<prefetch_result> prefetch_manager::prefetch_priors_async(
    std::string_view patient_id,
    std::string_view current_modality,
    std::optional<std::string_view> body_part) {
    std::string pid(patient_id);
    std::string mod(current_modality);
    std::optional<std::string> bp = body_part ?
        std::optional<std::string>(std::string(*body_part)) : std::nullopt;

    return std::async(std::launch::async, [this, pid, mod, bp]() {
        return prefetch_priors(pid, mod, bp ? std::optional<std::string_view>(*bp) : std::nullopt);
    });
}

// =============================================================================
// Manual Prefetch
// =============================================================================

std::string prefetch_manager::prefetch_study(
    std::string_view source_node_id,
    std::string_view study_uid) {
    // Check deduplication
    if (impl_->config.deduplicate_requests) {
        if (impl_->is_study_pending(std::string(study_uid))) {
            impl_->logger->debug_fmt("Study {} already pending", std::string(study_uid));
            return "";
        }
        if (impl_->is_study_local(study_uid)) {
            impl_->logger->debug_fmt("Study {} already local", std::string(study_uid));
            return "";
        }
    }

    // Mark as pending
    impl_->mark_study_pending(std::string(study_uid));

    // Create retrieve job
    if (impl_->job_mgr) {
        auto job_id = impl_->job_mgr->create_retrieve_job(
            source_node_id, study_uid, std::nullopt, job_priority::low);

        // Record history
        impl_->record_prefetch_history(
            "", // patient_id unknown
            std::string(study_uid),
            "", // no rule
            std::string(source_node_id),
            job_id,
            "pending");

        impl_->logger->info_fmt("Created prefetch job {} for study {}", job_id, std::string(study_uid));
        return job_id;
    }

    return "";
}

std::string prefetch_manager::prefetch_patient(
    std::string_view source_node_id,
    std::string_view patient_id,
    std::chrono::hours lookback) {
    // Create prefetch job for the patient
    if (impl_->job_mgr) {
        auto job_id = impl_->job_mgr->create_prefetch_job(
            source_node_id, patient_id, job_priority::low);

        impl_->logger->info_fmt("Created prefetch job {} for patient {}",
            job_id, std::string(patient_id));
        return job_id;
    }

    return "";
}

// =============================================================================
// Scheduler Control
// =============================================================================

void prefetch_manager::start_scheduler() {
    if (impl_->scheduler_running.load()) {
        return;
    }

    impl_->scheduler_running.store(true);
    impl_->scheduler_thread = std::thread([this]() {
        impl_->scheduler_loop();
    });

    impl_->logger->info("Started prefetch scheduler");
}

void prefetch_manager::stop_scheduler() {
    if (!impl_->scheduler_running.load()) {
        return;
    }

    impl_->scheduler_running.store(false);
    impl_->scheduler_cv.notify_all();

    if (impl_->scheduler_thread.joinable()) {
        impl_->scheduler_thread.join();
    }

    impl_->logger->info("Stopped prefetch scheduler");
}

bool prefetch_manager::is_scheduler_running() const noexcept {
    return impl_->scheduler_running.load();
}

// =============================================================================
// Worklist Monitor Control
// =============================================================================

void prefetch_manager::start_worklist_monitor(std::string_view worklist_node_id) {
    if (impl_->worklist_monitor_running.load()) {
        return;
    }

    impl_->worklist_node_id = std::string(worklist_node_id);
    impl_->worklist_monitor_running.store(true);
    impl_->worklist_monitor_thread = std::thread([this]() {
        impl_->worklist_monitor_loop();
    });

    impl_->logger->info_fmt("Started worklist monitor for node {}", std::string(worklist_node_id));
}

void prefetch_manager::stop_worklist_monitor() {
    if (!impl_->worklist_monitor_running.load()) {
        return;
    }

    impl_->worklist_monitor_running.store(false);
    impl_->worklist_cv.notify_all();

    if (impl_->worklist_monitor_thread.joinable()) {
        impl_->worklist_monitor_thread.join();
    }

    impl_->logger->info("Stopped worklist monitor");
}

bool prefetch_manager::is_worklist_monitor_running() const noexcept {
    return impl_->worklist_monitor_running.load();
}

// =============================================================================
// Status and Statistics
// =============================================================================

size_t prefetch_manager::pending_prefetches() const {
    return impl_->pending_count.load();
}

size_t prefetch_manager::completed_today() const {
    if (impl_->repo) {
        return impl_->repo->count_completed_today();
    }
    return 0;
}

size_t prefetch_manager::failed_today() const {
    if (impl_->repo) {
        return impl_->repo->count_failed_today();
    }
    return 0;
}

prefetch_rule_statistics prefetch_manager::get_rule_statistics(
    std::string_view rule_id) const {
    prefetch_rule_statistics stats;

    auto rule = get_rule(rule_id);
    if (rule) {
        stats.triggered_count = rule->triggered_count;
        stats.studies_prefetched = rule->studies_prefetched;
    }

    return stats;
}

// =============================================================================
// Configuration
// =============================================================================

const prefetch_manager_config& prefetch_manager::config() const noexcept {
    return impl_->config;
}

void prefetch_manager::set_config(prefetch_manager_config new_config) {
    impl_->config = new_config;
}

}  // namespace pacs::client
