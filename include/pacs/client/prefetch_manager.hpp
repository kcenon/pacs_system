// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @file prefetch_manager.hpp
 * @brief Prefetch manager for proactive DICOM data loading
 *
 * This file provides the prefetch_manager class for proactively loading
 * DICOM data based on worklist entries, scheduled exams, or prior study rules.
 *
 * @see Issue #541 - Implement Prefetch Manager for Proactive Data Loading
 * @see Issue #530 - PACS Client System Support (Parent Epic)
 * @see IHE Scheduled Workflow Profile
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include "pacs/client/prefetch_types.hpp"
#include "pacs/core/result.hpp"
#include "pacs/di/ilogger.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <vector>

// Forward declarations
namespace kcenon::pacs::storage {
class prefetch_repository;
class prefetch_rule_repository;
class prefetch_history_repository;
}

namespace kcenon::pacs::services {
class worklist_scu;
}

namespace kcenon::pacs::core {
class dicom_dataset;
}

namespace kcenon::pacs::client {

// Forward declarations
class remote_node_manager;
class job_manager;

struct prefetch_repositories {
    std::shared_ptr<storage::prefetch_rule_repository> rules;
    std::shared_ptr<storage::prefetch_history_repository> history;
};

// =============================================================================
// Prefetch Manager
// =============================================================================

/**
 * @brief Manager for proactive DICOM data prefetching
 *
 * Provides proactive data loading capabilities including:
 * - Rule-based prefetch configuration
 * - Worklist-driven automatic prefetch
 * - Prior study retrieval for comparison
 * - Scheduled prefetch jobs
 * - Request deduplication
 *
 * Thread Safety:
 * - All public methods are thread-safe
 * - Background threads handle monitoring and scheduling
 *
 * @example
 * @code
 * // Create manager with dependencies
 * auto prefetch_repo = std::make_shared<prefetch_repository>(db);
 * auto node_mgr = std::make_shared<remote_node_manager>(node_repo);
 * auto job_mgr = std::make_shared<job_manager>(job_repo, node_mgr);
 * auto worklist = std::make_shared<worklist_scu>();
 *
 * auto manager = std::make_unique<prefetch_manager>(
 *     prefetch_repo, node_mgr, job_mgr, worklist);
 *
 * // Add a prefetch rule
 * prefetch_rule rule;
 * rule.rule_id = generate_uuid();
 * rule.name = "CT Prior Studies";
 * rule.trigger = prefetch_trigger::prior_studies;
 * rule.modality_filter = "CT";
 * rule.max_prior_studies = 3;
 * rule.source_node_ids = {"archive-pacs"};
 * manager->add_rule(rule);
 *
 * // Start worklist monitor
 * manager->start_worklist_monitor("ris-worklist");
 *
 * // Manual prefetch of prior studies
 * auto result = manager->prefetch_priors("PATIENT123", "CT");
 * @endcode
 */
class prefetch_manager {
public:
    // =========================================================================
    // Construction / Destruction
    // =========================================================================

    /**
     * @brief Construct a prefetch manager from split repositories
     *
     * @param repositories Split prefetch repositories for rules and history
     * @param node_manager Remote node manager for DICOM operations (required)
     * @param job_manager Job manager for async operations (required)
     * @param worklist_scu Worklist SCU for MWL queries (optional)
     * @param logger Logger instance (optional, defaults to NullLogger)
     */
    explicit prefetch_manager(
        prefetch_repositories repositories,
        std::shared_ptr<remote_node_manager> node_manager,
        std::shared_ptr<job_manager> job_manager,
        std::shared_ptr<services::worklist_scu> worklist_scu = nullptr,
        std::shared_ptr<di::ILogger> logger = nullptr);

    /**
     * @brief Construct with custom configuration and split repositories
     *
     * @param config Manager configuration
     * @param repositories Split prefetch repositories for rules and history
     * @param node_manager Remote node manager for DICOM operations (required)
     * @param job_manager Job manager for async operations (required)
     * @param worklist_scu Worklist SCU for MWL queries (optional)
     * @param logger Logger instance (optional, defaults to NullLogger)
     */
    explicit prefetch_manager(
        const prefetch_manager_config& config,
        prefetch_repositories repositories,
        std::shared_ptr<remote_node_manager> node_manager,
        std::shared_ptr<job_manager> job_manager,
        std::shared_ptr<services::worklist_scu> worklist_scu = nullptr,
        std::shared_ptr<di::ILogger> logger = nullptr);

    /**
     * @brief Construct a prefetch manager with dependencies
     *
     * Compatibility-only overload. Prefer split repositories for new wiring.
     *
     * @param repo Prefetch repository for persistence (required)
     * @param node_manager Remote node manager for DICOM operations (required)
     * @param job_manager Job manager for async operations (required)
     * @param worklist_scu Worklist SCU for MWL queries (optional)
     * @param logger Logger instance (optional, defaults to NullLogger)
     */
    explicit prefetch_manager(
        std::shared_ptr<storage::prefetch_repository> repo,
        std::shared_ptr<remote_node_manager> node_manager,
        std::shared_ptr<job_manager> job_manager,
        std::shared_ptr<services::worklist_scu> worklist_scu = nullptr,
        std::shared_ptr<di::ILogger> logger = nullptr);

    /**
     * @brief Construct with custom configuration
     *
     * Compatibility-only overload. Prefer split repositories for new wiring.
     *
     * @param config Manager configuration
     * @param repo Prefetch repository for persistence (required)
     * @param node_manager Remote node manager for DICOM operations (required)
     * @param job_manager Job manager for async operations (required)
     * @param worklist_scu Worklist SCU for MWL queries (optional)
     * @param logger Logger instance (optional, defaults to NullLogger)
     */
    explicit prefetch_manager(
        const prefetch_manager_config& config,
        std::shared_ptr<storage::prefetch_repository> repo,
        std::shared_ptr<remote_node_manager> node_manager,
        std::shared_ptr<job_manager> job_manager,
        std::shared_ptr<services::worklist_scu> worklist_scu = nullptr,
        std::shared_ptr<di::ILogger> logger = nullptr);

    /**
     * @brief Destructor - stops scheduler and monitor if running
     */
    ~prefetch_manager();

    // Non-copyable, non-movable (due to internal threading)
    prefetch_manager(const prefetch_manager&) = delete;
    auto operator=(const prefetch_manager&) -> prefetch_manager& = delete;
    prefetch_manager(prefetch_manager&&) = delete;
    auto operator=(prefetch_manager&&) -> prefetch_manager& = delete;

    // =========================================================================
    // Rule Management
    // =========================================================================

    /**
     * @brief Add a new prefetch rule
     *
     * @param rule The rule to add
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto add_rule(const prefetch_rule& rule) -> kcenon::pacs::VoidResult;

    /**
     * @brief Update an existing prefetch rule
     *
     * @param rule The rule to update (rule_id must match existing)
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto update_rule(const prefetch_rule& rule) -> kcenon::pacs::VoidResult;

    /**
     * @brief Remove a prefetch rule
     *
     * @param rule_id ID of the rule to remove
     * @return VoidResult indicating success or error
     */
    [[nodiscard]] auto remove_rule(std::string_view rule_id) -> kcenon::pacs::VoidResult;

    /**
     * @brief Get a rule by ID
     *
     * @param rule_id ID of the rule to retrieve
     * @return Optional containing the rule if found
     */
    [[nodiscard]] auto get_rule(std::string_view rule_id) const
        -> std::optional<prefetch_rule>;

    /**
     * @brief List all prefetch rules
     *
     * @return Vector of all rules
     */
    [[nodiscard]] auto list_rules() const -> std::vector<prefetch_rule>;

    // =========================================================================
    // Worklist-Driven Prefetch
    // =========================================================================

    /**
     * @brief Process worklist items and trigger prefetch
     *
     * Evaluates each worklist item against enabled rules and
     * triggers appropriate prefetch operations.
     *
     * @param worklist_items Worklist items from MWL query
     */
    void process_worklist(const std::vector<core::dicom_dataset>& worklist_items);

    /**
     * @brief Process worklist items asynchronously
     *
     * @param worklist_items Worklist items from MWL query
     * @return Future that completes when processing is done
     */
    [[nodiscard]] auto process_worklist_async(
        const std::vector<core::dicom_dataset>& worklist_items) -> std::future<void>;

    // =========================================================================
    // Prior Study Prefetch
    // =========================================================================

    /**
     * @brief Prefetch prior studies for a patient
     *
     * Searches configured source nodes for prior studies matching
     * the patient and modality criteria.
     *
     * @param patient_id Patient ID to search for
     * @param current_modality Current exam modality
     * @param body_part Optional body part filter
     * @return Prefetch result with statistics
     */
    [[nodiscard]] auto prefetch_priors(
        std::string_view patient_id,
        std::string_view current_modality,
        std::optional<std::string_view> body_part = std::nullopt) -> prefetch_result;

    /**
     * @brief Prefetch prior studies asynchronously
     *
     * @param patient_id Patient ID to search for
     * @param current_modality Current exam modality
     * @param body_part Optional body part filter
     * @return Future containing prefetch result
     */
    [[nodiscard]] auto prefetch_priors_async(
        std::string_view patient_id,
        std::string_view current_modality,
        std::optional<std::string_view> body_part = std::nullopt)
        -> std::future<prefetch_result>;

    // =========================================================================
    // Manual Prefetch
    // =========================================================================

    /**
     * @brief Prefetch a specific study
     *
     * @param source_node_id Source node to retrieve from
     * @param study_uid Study Instance UID to prefetch
     * @return Job ID for tracking the prefetch operation
     */
    [[nodiscard]] auto prefetch_study(
        std::string_view source_node_id,
        std::string_view study_uid) -> std::string;

    /**
     * @brief Prefetch all studies for a patient
     *
     * @param source_node_id Source node to retrieve from
     * @param patient_id Patient ID to prefetch studies for
     * @param lookback Lookback period (default: 1 year)
     * @return Job ID for tracking the prefetch operation
     */
    [[nodiscard]] auto prefetch_patient(
        std::string_view source_node_id,
        std::string_view patient_id,
        std::chrono::hours lookback = std::chrono::hours{8760}) -> std::string;

    // =========================================================================
    // Scheduler Control
    // =========================================================================

    /**
     * @brief Start the scheduler for cron-based rules
     *
     * Begins checking and executing scheduled prefetch rules.
     * Does nothing if already running.
     */
    void start_scheduler();

    /**
     * @brief Stop the scheduler
     *
     * Stops the scheduler thread. Does nothing if not running.
     */
    void stop_scheduler();

    /**
     * @brief Check if scheduler is running
     *
     * @return true if the scheduler is active
     */
    [[nodiscard]] auto is_scheduler_running() const noexcept -> bool;

    // =========================================================================
    // Worklist Monitor Control
    // =========================================================================

    /**
     * @brief Start the worklist monitor
     *
     * Begins periodic polling of the worklist for new items.
     *
     * @param worklist_node_id Node ID for worklist queries
     */
    void start_worklist_monitor(std::string_view worklist_node_id);

    /**
     * @brief Stop the worklist monitor
     *
     * Stops periodic worklist polling. Does nothing if not running.
     */
    void stop_worklist_monitor();

    /**
     * @brief Check if worklist monitor is running
     *
     * @return true if the worklist monitor is active
     */
    [[nodiscard]] auto is_worklist_monitor_running() const noexcept -> bool;

    // =========================================================================
    // Status and Statistics
    // =========================================================================

    /**
     * @brief Get number of pending prefetch operations
     *
     * @return Number of prefetches currently queued
     */
    [[nodiscard]] auto pending_prefetches() const -> size_t;

    /**
     * @brief Get number of prefetches completed today
     *
     * @return Number of successful prefetches today
     */
    [[nodiscard]] auto completed_today() const -> size_t;

    /**
     * @brief Get number of prefetches failed today
     *
     * @return Number of failed prefetches today
     */
    [[nodiscard]] auto failed_today() const -> size_t;

    /**
     * @brief Get statistics for a specific rule
     *
     * @param rule_id Rule ID to get statistics for
     * @return Statistics for the rule
     */
    [[nodiscard]] auto get_rule_statistics(std::string_view rule_id) const
        -> prefetch_rule_statistics;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Get current configuration
     *
     * @return Current manager configuration
     */
    [[nodiscard]] auto config() const noexcept -> const prefetch_manager_config&;

    /**
     * @brief Update configuration
     *
     * @param new_config New configuration to apply
     */
    void set_config(prefetch_manager_config new_config);

private:
    // =========================================================================
    // Private Implementation
    // =========================================================================

    struct impl;
    std::unique_ptr<impl> impl_;
};

}  // namespace kcenon::pacs::client
