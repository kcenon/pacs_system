/**
 * @file prefetch_types.hpp
 * @brief Types and structures for prefetch manager
 *
 * This file provides data structures for representing prefetch rules,
 * triggers, results, and configuration for proactive DICOM data loading.
 *
 * @see Issue #541 - Implement Prefetch Manager for Proactive Data Loading
 * @see Issue #530 - PACS Client System Support (Parent Epic)
 */

#pragma once

#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

namespace pacs::client {

// =============================================================================
// Prefetch Trigger
// =============================================================================

/**
 * @brief Trigger type for prefetch operations
 */
enum class prefetch_trigger {
    worklist_match,  ///< Triggered by worklist entry
    prior_studies,   ///< Fetch prior studies for patient
    scheduled_exam,  ///< Based on scheduled procedure
    manual           ///< Manual request
};

/**
 * @brief Convert prefetch_trigger to string representation
 * @param trigger The trigger to convert
 * @return String representation of the trigger
 */
[[nodiscard]] constexpr const char* to_string(prefetch_trigger trigger) noexcept {
    switch (trigger) {
        case prefetch_trigger::worklist_match: return "worklist_match";
        case prefetch_trigger::prior_studies: return "prior_studies";
        case prefetch_trigger::scheduled_exam: return "scheduled_exam";
        case prefetch_trigger::manual: return "manual";
        default: return "unknown";
    }
}

/**
 * @brief Parse prefetch_trigger from string
 * @param str The string to parse
 * @return Parsed trigger, or manual if invalid
 */
[[nodiscard]] inline prefetch_trigger prefetch_trigger_from_string(
    std::string_view str) noexcept {
    if (str == "worklist_match") return prefetch_trigger::worklist_match;
    if (str == "prior_studies") return prefetch_trigger::prior_studies;
    if (str == "scheduled_exam") return prefetch_trigger::scheduled_exam;
    if (str == "manual") return prefetch_trigger::manual;
    return prefetch_trigger::manual;
}

// =============================================================================
// Prefetch Rule
// =============================================================================

/**
 * @brief Rule defining when and how to prefetch DICOM data
 *
 * Contains all configuration for a prefetch rule including filters,
 * prior study settings, and scheduling information.
 */
struct prefetch_rule {
    // =========================================================================
    // Identification
    // =========================================================================

    std::string rule_id;        ///< Unique rule identifier (UUID)
    std::string name;           ///< Human-readable name
    bool enabled{true};         ///< Whether the rule is active

    prefetch_trigger trigger;   ///< What triggers this rule

    // =========================================================================
    // Filters
    // =========================================================================

    std::string modality_filter;     ///< Modality filter (e.g., "CT,MR")
    std::string body_part_filter;    ///< Body part filter (e.g., "CHEST,ABDOMEN")
    std::string station_ae_filter;   ///< Station AE title filter

    // =========================================================================
    // Prior Study Settings
    // =========================================================================

    std::chrono::hours prior_lookback{8760};     ///< Lookback period (default: 1 year)
    size_t max_prior_studies{3};                 ///< Maximum prior studies to fetch
    std::vector<std::string> prior_modalities;   ///< Modalities to fetch (empty = same)

    // =========================================================================
    // Source Nodes
    // =========================================================================

    std::vector<std::string> source_node_ids;    ///< Nodes to search for data

    // =========================================================================
    // Schedule Settings
    // =========================================================================

    std::string schedule_cron;                   ///< Cron expression (e.g., "0 6 * * *")
    std::chrono::minutes advance_time{60};       ///< Prefetch N minutes before scheduled

    // =========================================================================
    // Statistics
    // =========================================================================

    size_t triggered_count{0};                   ///< Times rule was triggered
    size_t studies_prefetched{0};                ///< Total studies prefetched
    std::chrono::system_clock::time_point last_triggered;  ///< Last trigger time

    // =========================================================================
    // Database Fields
    // =========================================================================

    int64_t pk{0};  ///< Primary key (0 if not persisted)
};

// =============================================================================
// Prefetch Result
// =============================================================================

/**
 * @brief Result of a prefetch operation
 *
 * Contains statistics about what was prefetched for a patient.
 */
struct prefetch_result {
    std::string patient_id;           ///< Patient ID
    std::string patient_name;         ///< Patient name
    size_t studies_found{0};          ///< Studies found on remote
    size_t studies_prefetched{0};     ///< Studies actually prefetched
    size_t studies_already_local{0};  ///< Studies already local
    std::vector<std::string> job_ids; ///< Created job IDs
    std::chrono::milliseconds elapsed{0};  ///< Operation duration

    /**
     * @brief Check if prefetch was successful
     */
    [[nodiscard]] bool is_success() const noexcept {
        return studies_prefetched > 0 || studies_already_local > 0;
    }
};

// =============================================================================
// Prefetch History
// =============================================================================

/**
 * @brief History record for a single prefetch operation
 */
struct prefetch_history {
    std::string patient_id;           ///< Patient ID
    std::string study_uid;            ///< Study Instance UID
    std::string rule_id;              ///< Rule that triggered this (if any)
    std::string source_node_id;       ///< Source node ID
    std::string job_id;               ///< Associated job ID
    std::string status;               ///< Status (pending, completed, failed)
    std::chrono::system_clock::time_point prefetched_at;  ///< Timestamp

    int64_t pk{0};  ///< Primary key
};

// =============================================================================
// Prefetch Manager Configuration
// =============================================================================

/**
 * @brief Configuration for the prefetch manager
 */
struct prefetch_manager_config {
    bool enabled{true};                             ///< Enable prefetch functionality
    std::chrono::seconds worklist_check_interval{300};  ///< Worklist polling interval
    size_t max_concurrent_prefetch{4};              ///< Max concurrent prefetch jobs
    bool deduplicate_requests{true};                ///< Deduplicate pending requests
};

// =============================================================================
// Rule Statistics
// =============================================================================

/**
 * @brief Statistics for a prefetch rule
 */
struct prefetch_rule_statistics {
    size_t triggered_count{0};      ///< Times rule was triggered
    size_t studies_prefetched{0};   ///< Total studies prefetched
    size_t bytes_prefetched{0};     ///< Total bytes prefetched
};

}  // namespace pacs::client
