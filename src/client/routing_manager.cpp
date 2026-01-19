/**
 * @file routing_manager.cpp
 * @brief Implementation of the routing manager for auto-forwarding
 *
 * @see Issue #539 - Implement Routing Manager for Auto-Forwarding
 * @see Issue #530 - PACS Client System Support (Parent Epic)
 */

#include "pacs/client/routing_manager.hpp"
#include "pacs/client/job_manager.hpp"
#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_tag.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/services/storage_scp.hpp"
#include "pacs/storage/routing_repository.hpp"

#include <algorithm>
#include <cctype>

namespace pacs::client {

// =============================================================================
// Additional DICOM Tags (not in constants)
// =============================================================================

namespace {

// Institutional Department Name (0008,1040)
inline constexpr core::dicom_tag institutional_department_name{0x0008, 0x1040};

// Body Part Examined (0018,0015)
inline constexpr core::dicom_tag body_part_examined{0x0018, 0x0015};

// =============================================================================
// Helper Functions
// =============================================================================

/// Convert string to lowercase
[[nodiscard]] std::string to_lower(std::string_view str) {
    std::string result;
    result.reserve(str.size());
    for (char c : str) {
        result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return result;
}

}  // namespace

// =============================================================================
// Construction / Destruction
// =============================================================================

routing_manager::routing_manager(
    std::shared_ptr<storage::routing_repository> repo,
    std::shared_ptr<job_manager> job_mgr,
    std::shared_ptr<di::ILogger> logger)
    : config_{}
    , repo_(std::move(repo))
    , job_manager_(std::move(job_mgr))
    , logger_(logger ? std::move(logger) : di::null_logger()) {

    load_rules();

    if (logger_) {
        logger_->info_fmt("routing_manager: Initialized with {} rules", rules_.size());
    }
}

routing_manager::routing_manager(
    const routing_manager_config& config,
    std::shared_ptr<storage::routing_repository> repo,
    std::shared_ptr<job_manager> job_mgr,
    std::shared_ptr<di::ILogger> logger)
    : config_(config)
    , repo_(std::move(repo))
    , job_manager_(std::move(job_mgr))
    , logger_(logger ? std::move(logger) : di::null_logger()) {

    enabled_.store(config_.enabled);
    load_rules();

    if (logger_) {
        logger_->info_fmt("routing_manager: Initialized with {} rules (enabled={})",
                         rules_.size(), enabled_.load());
    }
}

routing_manager::~routing_manager() {
    detach_from_storage_scp();
}

// =============================================================================
// Rule CRUD
// =============================================================================

pacs::VoidResult routing_manager::add_rule(const routing_rule& rule) {
    // Validate
    if (rule.rule_id.empty()) {
        return pacs::pacs_void_error(-1, "Rule ID cannot be empty");
    }

    {
        std::shared_lock lock(rules_mutex_);
        if (rules_.size() >= config_.max_rules) {
            return pacs::pacs_void_error(-1, "Maximum number of rules reached");
        }
    }

    // Save to repository
    auto result = repo_->save(rule);
    if (!result.is_ok()) {
        return result;
    }

    // Update cache
    {
        std::unique_lock lock(rules_mutex_);
        rules_.push_back(rule);

        // Sort by priority (descending)
        std::sort(rules_.begin(), rules_.end(),
                  [](const routing_rule& a, const routing_rule& b) {
                      return a.priority > b.priority;
                  });
    }

    if (logger_) {
        logger_->info_fmt("routing_manager: Added rule: {} ({})", rule.rule_id, rule.name);
    }

    return pacs::ok();
}

pacs::VoidResult routing_manager::update_rule(const routing_rule& rule) {
    // Save to repository
    auto result = repo_->save(rule);
    if (!result.is_ok()) {
        return result;
    }

    // Update cache
    {
        std::unique_lock lock(rules_mutex_);
        auto it = std::find_if(rules_.begin(), rules_.end(),
                               [&](const routing_rule& r) {
                                   return r.rule_id == rule.rule_id;
                               });
        if (it != rules_.end()) {
            *it = rule;
        } else {
            rules_.push_back(rule);
        }

        // Re-sort by priority
        std::sort(rules_.begin(), rules_.end(),
                  [](const routing_rule& a, const routing_rule& b) {
                      return a.priority > b.priority;
                  });
    }

    if (logger_) {
        logger_->info_fmt("routing_manager: Updated rule: {} ({})", rule.rule_id, rule.name);
    }

    return pacs::ok();
}

pacs::VoidResult routing_manager::remove_rule(std::string_view rule_id) {
    // Remove from repository
    auto result = repo_->remove(rule_id);
    if (!result.is_ok()) {
        return result;
    }

    // Remove from cache
    {
        std::unique_lock lock(rules_mutex_);
        rules_.erase(
            std::remove_if(rules_.begin(), rules_.end(),
                           [&](const routing_rule& r) {
                               return r.rule_id == rule_id;
                           }),
            rules_.end());
    }

    if (logger_) {
        logger_->info_fmt("routing_manager: Removed rule: {}", rule_id);
    }

    return pacs::ok();
}

std::optional<routing_rule> routing_manager::get_rule(std::string_view rule_id) const {
    std::shared_lock lock(rules_mutex_);
    auto it = std::find_if(rules_.begin(), rules_.end(),
                           [&](const routing_rule& r) {
                               return r.rule_id == rule_id;
                           });
    if (it != rules_.end()) {
        return *it;
    }
    return std::nullopt;
}

std::vector<routing_rule> routing_manager::list_rules() const {
    std::shared_lock lock(rules_mutex_);
    return rules_;
}

std::vector<routing_rule> routing_manager::list_enabled_rules() const {
    std::shared_lock lock(rules_mutex_);
    std::vector<routing_rule> result;
    std::copy_if(rules_.begin(), rules_.end(), std::back_inserter(result),
                 [](const routing_rule& r) { return r.enabled; });
    return result;
}

// =============================================================================
// Rule Ordering
// =============================================================================

pacs::VoidResult routing_manager::set_rule_priority(std::string_view rule_id, int priority) {
    auto result = repo_->update_priority(rule_id, priority);
    if (!result.is_ok()) {
        return result;
    }

    {
        std::unique_lock lock(rules_mutex_);
        auto it = std::find_if(rules_.begin(), rules_.end(),
                               [&](const routing_rule& r) {
                                   return r.rule_id == rule_id;
                               });
        if (it != rules_.end()) {
            it->priority = priority;
        }

        // Re-sort
        std::sort(rules_.begin(), rules_.end(),
                  [](const routing_rule& a, const routing_rule& b) {
                      return a.priority > b.priority;
                  });
    }

    return pacs::ok();
}

pacs::VoidResult routing_manager::reorder_rules(const std::vector<std::string>& rule_ids) {
    std::unique_lock lock(rules_mutex_);

    // Assign priorities based on order (first = highest)
    int priority = static_cast<int>(rule_ids.size());
    for (const auto& rule_id : rule_ids) {
        auto repo_result = repo_->update_priority(rule_id, priority);
        if (!repo_result.is_ok()) {
            return repo_result;
        }

        auto it = std::find_if(rules_.begin(), rules_.end(),
                               [&](const routing_rule& r) {
                                   return r.rule_id == rule_id;
                               });
        if (it != rules_.end()) {
            it->priority = priority;
        }
        --priority;
    }

    // Re-sort
    std::sort(rules_.begin(), rules_.end(),
              [](const routing_rule& a, const routing_rule& b) {
                  return a.priority > b.priority;
              });

    return pacs::ok();
}

// =============================================================================
// Rule Evaluation
// =============================================================================

std::vector<routing_action> routing_manager::evaluate(const core::dicom_dataset& dataset) {
    std::vector<routing_action> result;

    if (!enabled_.load()) {
        return result;
    }

    ++total_evaluated_;

    std::shared_lock lock(rules_mutex_);

    for (const auto& rule : rules_) {
        if (!rule.is_effective_now()) {
            continue;
        }

        // Check all conditions (AND logic)
        bool all_match = !rule.conditions.empty();
        for (const auto& condition : rule.conditions) {
            if (!match_condition(condition, dataset)) {
                all_match = false;
                break;
            }
        }

        if (all_match) {
            ++total_matched_;

            // Add all actions from this rule
            for (const auto& action : rule.actions) {
                result.push_back(action);
            }

            // Only first matching rule is executed (stop after first match)
            // Uncomment the break below if you want first-match-only behavior
            // break;
        }
    }

    return result;
}

std::vector<std::pair<std::string, std::vector<routing_action>>>
routing_manager::evaluate_with_rule_ids(const core::dicom_dataset& dataset) {
    std::vector<std::pair<std::string, std::vector<routing_action>>> result;

    if (!enabled_.load()) {
        return result;
    }

    ++total_evaluated_;

    std::shared_lock lock(rules_mutex_);

    for (const auto& rule : rules_) {
        if (!rule.is_effective_now()) {
            continue;
        }

        bool all_match = !rule.conditions.empty();
        for (const auto& condition : rule.conditions) {
            if (!match_condition(condition, dataset)) {
                all_match = false;
                break;
            }
        }

        if (all_match) {
            ++total_matched_;
            result.emplace_back(rule.rule_id, rule.actions);
        }
    }

    return result;
}

// =============================================================================
// Routing Execution
// =============================================================================

void routing_manager::route(const core::dicom_dataset& dataset) {
    if (!enabled_.load()) {
        return;
    }

    auto sop_instance_uid = dataset.get_string(core::tags::sop_instance_uid);
    if (sop_instance_uid.empty()) {
        if (logger_) {
            logger_->warn("routing_manager: Cannot route dataset without SOP Instance UID");
        }
        return;
    }

    auto matches = evaluate_with_rule_ids(dataset);

    for (const auto& [rule_id, actions] : matches) {
        // Update statistics
        auto stat_result = repo_->increment_triggered(rule_id);
        if (!stat_result.is_ok() && logger_) {
            logger_->warn_fmt("routing_manager: Failed to update statistics for rule: {}",
                             rule_id);
        }

        // Notify callback
        if (routing_callback_) {
            routing_callback_(rule_id, sop_instance_uid, actions);
        }

        // Execute actions
        execute_actions(sop_instance_uid, actions);
    }
}

void routing_manager::route(std::string_view sop_instance_uid) {
    if (!enabled_.load()) {
        return;
    }

    // Note: This would require loading the dataset from storage
    // For now, log a warning as this requires additional infrastructure
    if (logger_) {
        logger_->warn_fmt("routing_manager: route(sop_instance_uid) not fully implemented - "
                         "use route(dataset) instead. UID: {}", sop_instance_uid);
    }
}

// =============================================================================
// Enable/Disable
// =============================================================================

void routing_manager::enable() {
    enabled_.store(true);
    if (logger_) {
        logger_->info("routing_manager: Routing enabled");
    }
}

void routing_manager::disable() {
    enabled_.store(false);
    if (logger_) {
        logger_->info("routing_manager: Routing disabled");
    }
}

bool routing_manager::is_enabled() const noexcept {
    return enabled_.load();
}

// =============================================================================
// Storage SCP Integration
// =============================================================================

void routing_manager::attach_to_storage_scp(services::storage_scp& scp) {
    detach_from_storage_scp();

    attached_scp_ = &scp;

    // Register post-store handler
    scp.set_post_store_handler(
        [this](const core::dicom_dataset& dataset,
               const std::string& /*patient_id*/,
               const std::string& /*study_uid*/,
               const std::string& /*series_uid*/,
               const std::string& /*sop_instance_uid*/) {
            this->route(dataset);
        });

    if (logger_) {
        logger_->info("routing_manager: Attached to Storage SCP");
    }
}

void routing_manager::detach_from_storage_scp() {
    if (attached_scp_) {
        attached_scp_->set_post_store_handler(nullptr);
        attached_scp_ = nullptr;

        if (logger_) {
            logger_->info("routing_manager: Detached from Storage SCP");
        }
    }
}

// =============================================================================
// Event Callbacks
// =============================================================================

void routing_manager::set_routing_callback(routing_event_callback callback) {
    routing_callback_ = std::move(callback);
}

// =============================================================================
// Testing (Dry Run)
// =============================================================================

routing_test_result routing_manager::test_rules(const core::dicom_dataset& dataset) const {
    routing_test_result result;

    std::shared_lock lock(rules_mutex_);

    for (const auto& rule : rules_) {
        if (!rule.is_effective_now()) {
            continue;
        }

        bool all_match = !rule.conditions.empty();
        for (const auto& condition : rule.conditions) {
            if (!match_condition(condition, dataset)) {
                all_match = false;
                break;
            }
        }

        if (all_match) {
            result.matched = true;
            result.matched_rule_id = rule.rule_id;
            result.actions = rule.actions;
            break;  // Return first match for test
        }
    }

    return result;
}

// =============================================================================
// Statistics
// =============================================================================

routing_statistics routing_manager::get_statistics() const {
    routing_statistics stats;
    stats.total_evaluated = total_evaluated_.load();
    stats.total_matched = total_matched_.load();
    stats.total_forwarded = total_forwarded_.load();
    stats.total_failed = total_failed_.load();
    return stats;
}

routing_statistics routing_manager::get_rule_statistics(std::string_view rule_id) const {
    routing_statistics stats;

    auto rule = repo_->find_by_id(rule_id);
    if (rule) {
        stats.total_evaluated = 0;  // Not tracked per-rule
        stats.total_matched = rule->triggered_count;
        stats.total_forwarded = rule->success_count;
        stats.total_failed = rule->failure_count;
    }

    return stats;
}

void routing_manager::reset_statistics() {
    total_evaluated_.store(0);
    total_matched_.store(0);
    total_forwarded_.store(0);
    total_failed_.store(0);

    // Reset per-rule statistics
    std::shared_lock lock(rules_mutex_);
    for (const auto& rule : rules_) {
        auto result = repo_->reset_statistics(rule.rule_id);
        if (!result.is_ok() && logger_) {
            logger_->warn_fmt("routing_manager: Failed to reset statistics for rule: {}",
                             rule.rule_id);
        }
    }
}

// =============================================================================
// Configuration
// =============================================================================

const routing_manager_config& routing_manager::config() const noexcept {
    return config_;
}

// =============================================================================
// Private Implementation
// =============================================================================

bool routing_manager::match_condition(const routing_condition& condition,
                                      const core::dicom_dataset& dataset) const {
    auto value = get_field_value(condition.match_field, dataset);
    bool matched = match_pattern(condition.pattern, value, condition.case_sensitive);

    // Apply negation if needed
    if (condition.negate) {
        matched = !matched;
    }

    return matched;
}

bool routing_manager::match_pattern(std::string_view pattern,
                                    std::string_view value,
                                    bool case_sensitive) const {
    // Convert to lowercase if case-insensitive
    std::string pat_str = case_sensitive ? std::string(pattern) : to_lower(pattern);
    std::string val_str = case_sensitive ? std::string(value) : to_lower(value);

    // Simple wildcard matching (* and ?)
    // * matches any sequence of characters
    // ? matches any single character

    const char* p = pat_str.c_str();
    const char* v = val_str.c_str();

    const char* star_p = nullptr;
    const char* star_v = nullptr;

    while (*v) {
        if (*p == '*') {
            star_p = p++;
            star_v = v;
        } else if (*p == '?' || *p == *v) {
            ++p;
            ++v;
        } else if (star_p) {
            p = star_p + 1;
            v = ++star_v;
        } else {
            return false;
        }
    }

    while (*p == '*') {
        ++p;
    }

    return *p == '\0';
}

std::string routing_manager::get_field_value(routing_field field,
                                             const core::dicom_dataset& dataset) const {
    switch (field) {
        case routing_field::modality:
            return dataset.get_string(core::tags::modality);

        case routing_field::station_ae:
            return dataset.get_string(core::tags::station_name);

        case routing_field::institution:
            return dataset.get_string(core::tags::institution_name);

        case routing_field::department:
            return dataset.get_string(institutional_department_name);

        case routing_field::referring_physician:
            return dataset.get_string(core::tags::referring_physician_name);

        case routing_field::study_description:
            return dataset.get_string(core::tags::study_description);

        case routing_field::series_description:
            return dataset.get_string(core::tags::series_description);

        case routing_field::body_part:
            return dataset.get_string(body_part_examined);

        case routing_field::patient_id_pattern:
            return dataset.get_string(core::tags::patient_id);

        case routing_field::sop_class_uid:
            return dataset.get_string(core::tags::sop_class_uid);

        default:
            return "";
    }
}

void routing_manager::execute_actions(const std::string& sop_instance_uid,
                                      const std::vector<routing_action>& actions) {
    for (const auto& action : actions) {
        if (action.destination_node_id.empty()) {
            if (logger_) {
                logger_->warn_fmt("routing_manager: Skipping action with empty destination for UID: {}",
                                 sop_instance_uid);
            }
            continue;
        }

        // Create a store job via job_manager
        std::vector<std::string> instance_uids{sop_instance_uid};

        auto job_id = job_manager_->create_store_job(
            action.destination_node_id,
            instance_uids,
            action.priority);

        ++total_forwarded_;

        if (logger_) {
            logger_->info_fmt("routing_manager: Created forward job {} for UID {} -> {}",
                             job_id, sop_instance_uid, action.destination_node_id);
        }

        // Note: Delayed forwarding would require a scheduler/timer
        // For now, jobs are created immediately
        if (action.delay.count() > 0 && logger_) {
            logger_->debug_fmt("routing_manager: Delayed forwarding ({} min) not yet implemented",
                              action.delay.count());
        }
    }
}

void routing_manager::load_rules() {
    auto loaded = repo_->find_enabled_rules();

    std::unique_lock lock(rules_mutex_);
    rules_ = std::move(loaded);

    // Sort by priority (descending)
    std::sort(rules_.begin(), rules_.end(),
              [](const routing_rule& a, const routing_rule& b) {
                  return a.priority > b.priority;
              });
}

}  // namespace pacs::client
