/**
 * @file routing_manager.hpp
 * @brief Routing manager for automatic DICOM image forwarding
 *
 * This file provides the routing_manager class for managing rule-based
 * automatic forwarding of DICOM images to configured destinations.
 *
 * @see Issue #539 - Implement Routing Manager for Auto-Forwarding
 * @see Issue #530 - PACS Client System Support (Parent Epic)
 */

#pragma once

#include "pacs/client/routing_types.hpp"
#include "pacs/core/result.hpp"
#include "pacs/di/ilogger.hpp"

#include <atomic>
#include <memory>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>

// Forward declarations
namespace pacs::core {
class dicom_dataset;
}

namespace pacs::storage {
class routing_repository;
}

namespace pacs::services {
class storage_scp;
}

namespace pacs::client {

// Forward declaration
class job_manager;

// =============================================================================
// Routing Manager
// =============================================================================

/**
 * @brief Manager for automatic DICOM image forwarding based on rules
 *
 * Provides rule-based automatic forwarding with:
 * - Multiple conditions per rule (AND logic)
 * - Wildcard pattern matching (*, ?)
 * - Multiple destinations per rule
 * - Delayed forwarding support
 * - Integration with Storage SCP
 * - Statistics tracking
 *
 * Thread Safety:
 * - All public methods are thread-safe
 * - Uses shared_mutex for rule access
 *
 * @example
 * @code
 * // Create manager with dependencies
 * auto routing_repo = std::make_shared<routing_repository>(db);
 * auto job_mgr = std::make_shared<job_manager>(job_repo, node_mgr);
 * auto manager = std::make_unique<routing_manager>(routing_repo, job_mgr);
 *
 * // Add a routing rule
 * routing_rule rule;
 * rule.rule_id = "ct-to-archive";
 * rule.name = "Forward CT to Archive";
 * rule.conditions.push_back({routing_field::modality, "CT"});
 * rule.actions.push_back({"archive-server-1"});
 * manager->add_rule(rule);
 *
 * // Enable routing
 * manager->enable();
 *
 * // Attach to Storage SCP for automatic forwarding
 * manager->attach_to_storage_scp(storage_scp);
 *
 * // Manual routing
 * manager->route(received_dataset);
 * @endcode
 */
class routing_manager {
public:
    // =========================================================================
    // Construction / Destruction
    // =========================================================================

    /**
     * @brief Construct a routing manager with default configuration
     *
     * @param repo Routing repository for persistence (required)
     * @param job_manager Job manager for executing forward jobs (required)
     * @param logger Logger instance (optional, defaults to NullLogger)
     */
    explicit routing_manager(
        std::shared_ptr<storage::routing_repository> repo,
        std::shared_ptr<job_manager> job_manager,
        std::shared_ptr<di::ILogger> logger = nullptr);

    /**
     * @brief Construct a routing manager with custom configuration
     *
     * @param config Manager configuration
     * @param repo Routing repository for persistence (required)
     * @param job_manager Job manager for executing forward jobs (required)
     * @param logger Logger instance (optional, defaults to NullLogger)
     */
    explicit routing_manager(
        const routing_manager_config& config,
        std::shared_ptr<storage::routing_repository> repo,
        std::shared_ptr<job_manager> job_manager,
        std::shared_ptr<di::ILogger> logger = nullptr);

    /**
     * @brief Destructor
     */
    ~routing_manager();

    // Non-copyable, non-movable
    routing_manager(const routing_manager&) = delete;
    auto operator=(const routing_manager&) -> routing_manager& = delete;
    routing_manager(routing_manager&&) = delete;
    auto operator=(routing_manager&&) -> routing_manager& = delete;

    // =========================================================================
    // Rule CRUD
    // =========================================================================

    /**
     * @brief Add a new routing rule
     *
     * @param rule The rule to add
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto add_rule(const routing_rule& rule) -> pacs::VoidResult;

    /**
     * @brief Update an existing routing rule
     *
     * @param rule The rule to update (identified by rule_id)
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto update_rule(const routing_rule& rule) -> pacs::VoidResult;

    /**
     * @brief Remove a routing rule
     *
     * @param rule_id The ID of the rule to remove
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto remove_rule(std::string_view rule_id) -> pacs::VoidResult;

    /**
     * @brief Get a routing rule by ID
     *
     * @param rule_id The rule ID to retrieve
     * @return Optional containing the rule if found
     */
    [[nodiscard]] auto get_rule(std::string_view rule_id) const
        -> std::optional<routing_rule>;

    /**
     * @brief List all routing rules
     *
     * @return Vector of all rules
     */
    [[nodiscard]] auto list_rules() const -> std::vector<routing_rule>;

    /**
     * @brief List only enabled routing rules
     *
     * @return Vector of enabled rules
     */
    [[nodiscard]] auto list_enabled_rules() const -> std::vector<routing_rule>;

    // =========================================================================
    // Rule Ordering
    // =========================================================================

    /**
     * @brief Set the priority of a rule
     *
     * @param rule_id The rule ID to update
     * @param priority The new priority (higher = evaluated first)
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto set_rule_priority(std::string_view rule_id, int priority)
        -> pacs::VoidResult;

    /**
     * @brief Reorder rules by specifying the desired order
     *
     * @param rule_ids Vector of rule IDs in desired order (first = highest priority)
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto reorder_rules(const std::vector<std::string>& rule_ids)
        -> pacs::VoidResult;

    // =========================================================================
    // Rule Evaluation
    // =========================================================================

    /**
     * @brief Evaluate rules against a dataset
     *
     * Returns all actions that should be executed based on matching rules.
     *
     * @param dataset The DICOM dataset to evaluate
     * @return Vector of actions to execute
     */
    [[nodiscard]] auto evaluate(const core::dicom_dataset& dataset)
        -> std::vector<routing_action>;

    /**
     * @brief Evaluate rules and return with matched rule IDs
     *
     * @param dataset The DICOM dataset to evaluate
     * @return Vector of pairs (rule_id, actions)
     */
    [[nodiscard]] auto evaluate_with_rule_ids(const core::dicom_dataset& dataset)
        -> std::vector<std::pair<std::string, std::vector<routing_action>>>;

    // =========================================================================
    // Routing Execution
    // =========================================================================

    /**
     * @brief Route a DICOM dataset based on matching rules
     *
     * Evaluates rules and creates forward jobs for matching actions.
     *
     * @param dataset The DICOM dataset to route
     */
    void route(const core::dicom_dataset& dataset);

    /**
     * @brief Route a stored instance by SOP Instance UID
     *
     * @param sop_instance_uid The SOP Instance UID to route
     */
    void route(std::string_view sop_instance_uid);

    // =========================================================================
    // Enable/Disable
    // =========================================================================

    /**
     * @brief Enable routing globally
     */
    void enable();

    /**
     * @brief Disable routing globally
     */
    void disable();

    /**
     * @brief Check if routing is enabled
     *
     * @return true if routing is enabled
     */
    [[nodiscard]] auto is_enabled() const noexcept -> bool;

    // =========================================================================
    // Storage SCP Integration
    // =========================================================================

    /**
     * @brief Attach to a Storage SCP for automatic routing
     *
     * Registers a post-store handler to automatically route received images.
     *
     * @param scp The Storage SCP to attach to
     */
    void attach_to_storage_scp(services::storage_scp& scp);

    /**
     * @brief Detach from the currently attached Storage SCP
     */
    void detach_from_storage_scp();

    // =========================================================================
    // Event Callbacks
    // =========================================================================

    /**
     * @brief Set callback for routing events
     *
     * Called when a rule matches and actions are triggered.
     *
     * @param callback The callback function
     */
    void set_routing_callback(routing_event_callback callback);

    // =========================================================================
    // Testing (Dry Run)
    // =========================================================================

    /**
     * @brief Test rules against a dataset without executing actions
     *
     * @param dataset The DICOM dataset to test
     * @return Test result showing matches
     */
    [[nodiscard]] auto test_rules(const core::dicom_dataset& dataset) const
        -> routing_test_result;

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get overall routing statistics
     *
     * @return Aggregate statistics for all routing operations
     */
    [[nodiscard]] auto get_statistics() const -> routing_statistics;

    /**
     * @brief Get statistics for a specific rule
     *
     * @param rule_id The rule ID
     * @return Statistics for the specified rule
     */
    [[nodiscard]] auto get_rule_statistics(std::string_view rule_id) const
        -> routing_statistics;

    /**
     * @brief Reset all statistics
     */
    void reset_statistics();

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Get current configuration
     *
     * @return Current manager configuration
     */
    [[nodiscard]] auto config() const noexcept -> const routing_manager_config&;

private:
    // =========================================================================
    // Private Implementation
    // =========================================================================

    /**
     * @brief Check if a condition matches a dataset
     */
    [[nodiscard]] auto match_condition(const routing_condition& condition,
                                       const core::dicom_dataset& dataset) const
        -> bool;

    /**
     * @brief Match a wildcard pattern against a value
     */
    [[nodiscard]] auto match_pattern(std::string_view pattern,
                                     std::string_view value,
                                     bool case_sensitive) const -> bool;

    /**
     * @brief Get DICOM field value from dataset
     */
    [[nodiscard]] auto get_field_value(routing_field field,
                                       const core::dicom_dataset& dataset) const
        -> std::string;

    /**
     * @brief Execute routing actions
     */
    void execute_actions(const std::string& sop_instance_uid,
                        const std::vector<routing_action>& actions);

    /**
     * @brief Load rules from repository into cache
     */
    void load_rules();

    // =========================================================================
    // Member Variables
    // =========================================================================

    routing_manager_config config_;
    std::shared_ptr<storage::routing_repository> repo_;
    std::shared_ptr<job_manager> job_manager_;
    std::shared_ptr<di::ILogger> logger_;

    std::vector<routing_rule> rules_;
    mutable std::shared_mutex rules_mutex_;

    std::atomic<bool> enabled_{true};
    routing_event_callback routing_callback_;

    // Statistics
    std::atomic<size_t> total_evaluated_{0};
    std::atomic<size_t> total_matched_{0};
    std::atomic<size_t> total_forwarded_{0};
    std::atomic<size_t> total_failed_{0};

    // Storage SCP attachment state
    services::storage_scp* attached_scp_{nullptr};
};

}  // namespace pacs::client
