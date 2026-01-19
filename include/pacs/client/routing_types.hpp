/**
 * @file routing_types.hpp
 * @brief Routing types and structures for auto-forwarding DICOM images
 *
 * This file provides data structures for representing routing rules,
 * conditions, actions, and related types for automatic DICOM image forwarding.
 *
 * @see Issue #539 - Implement Routing Manager for Auto-Forwarding
 * @see Issue #530 - PACS Client System Support (Parent Epic)
 */

#pragma once

#include "pacs/client/job_types.hpp"

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pacs::client {

// =============================================================================
// Routing Condition Field
// =============================================================================

/**
 * @brief DICOM field to match in routing conditions
 *
 * These fields map to standard DICOM attributes that can be used
 * for routing decisions.
 */
enum class routing_field {
    modality,              ///< (0008,0060) Modality - CT, MR, US, etc.
    station_ae,            ///< (0008,1010) Station Name or calling AE
    institution,           ///< (0008,0080) Institution Name
    department,            ///< (0008,1040) Institutional Department Name
    referring_physician,   ///< (0008,0090) Referring Physician's Name
    study_description,     ///< (0008,1030) Study Description
    series_description,    ///< (0008,103E) Series Description
    body_part,             ///< (0018,0015) Body Part Examined
    patient_id_pattern,    ///< (0010,0020) Patient ID (pattern matching)
    sop_class_uid          ///< (0008,0016) SOP Class UID
};

/**
 * @brief Convert routing_field to string representation
 * @param field The field to convert
 * @return String representation of the field
 */
[[nodiscard]] constexpr const char* to_string(routing_field field) noexcept {
    switch (field) {
        case routing_field::modality: return "modality";
        case routing_field::station_ae: return "station_ae";
        case routing_field::institution: return "institution";
        case routing_field::department: return "department";
        case routing_field::referring_physician: return "referring_physician";
        case routing_field::study_description: return "study_description";
        case routing_field::series_description: return "series_description";
        case routing_field::body_part: return "body_part";
        case routing_field::patient_id_pattern: return "patient_id_pattern";
        case routing_field::sop_class_uid: return "sop_class_uid";
        default: return "unknown";
    }
}

/**
 * @brief Parse routing_field from string
 * @param str The string to parse
 * @return Parsed field, or modality if invalid
 */
[[nodiscard]] inline routing_field routing_field_from_string(
    std::string_view str) noexcept {
    if (str == "modality") return routing_field::modality;
    if (str == "station_ae") return routing_field::station_ae;
    if (str == "institution") return routing_field::institution;
    if (str == "department") return routing_field::department;
    if (str == "referring_physician") return routing_field::referring_physician;
    if (str == "study_description") return routing_field::study_description;
    if (str == "series_description") return routing_field::series_description;
    if (str == "body_part") return routing_field::body_part;
    if (str == "patient_id_pattern") return routing_field::patient_id_pattern;
    if (str == "sop_class_uid") return routing_field::sop_class_uid;
    return routing_field::modality;
}

// =============================================================================
// Routing Condition
// =============================================================================

/**
 * @brief A single condition for routing rule evaluation
 *
 * Conditions are combined using AND logic within a rule.
 * Supports wildcard patterns (* for any characters, ? for single character).
 */
struct routing_condition {
    routing_field match_field;     ///< The DICOM field to match
    std::string pattern;           ///< Pattern to match (supports wildcards: *, ?)
    bool case_sensitive{false};    ///< Whether matching is case-sensitive
    bool negate{false};            ///< Invert the match result

    /**
     * @brief Default constructor
     */
    routing_condition() = default;

    /**
     * @brief Construct with field and pattern
     * @param field The DICOM field to match
     * @param pat The pattern to match against
     * @param case_sens Case sensitivity flag
     * @param neg Negation flag
     */
    routing_condition(routing_field field, std::string pat,
                     bool case_sens = false, bool neg = false)
        : match_field(field)
        , pattern(std::move(pat))
        , case_sensitive(case_sens)
        , negate(neg) {}
};

// =============================================================================
// Routing Action
// =============================================================================

/**
 * @brief Action to perform when a routing rule matches
 *
 * Defines where to forward the DICOM instance and with what settings.
 */
struct routing_action {
    std::string destination_node_id;      ///< Target remote node ID
    job_priority priority{job_priority::normal};  ///< Job priority for forwarding
    std::chrono::minutes delay{0};        ///< Delay before forwarding
    bool delete_after_send{false};        ///< Delete local copy after successful send
    bool notify_on_failure{true};         ///< Generate notification on failure

    /**
     * @brief Default constructor
     */
    routing_action() = default;

    /**
     * @brief Construct with destination
     * @param dest_node_id The destination node ID
     * @param prio Job priority
     * @param del Delay before sending
     */
    routing_action(std::string dest_node_id,
                  job_priority prio = job_priority::normal,
                  std::chrono::minutes del = std::chrono::minutes{0})
        : destination_node_id(std::move(dest_node_id))
        , priority(prio)
        , delay(del) {}
};

// =============================================================================
// Routing Rule
// =============================================================================

/**
 * @brief A complete routing rule with conditions and actions
 *
 * Rules are evaluated in priority order (higher priority first).
 * All conditions must match (AND logic) for actions to be triggered.
 */
struct routing_rule {
    // =========================================================================
    // Identification
    // =========================================================================

    std::string rule_id;          ///< Unique rule identifier
    std::string name;             ///< Human-readable name
    std::string description;      ///< Detailed description

    // =========================================================================
    // Rule State
    // =========================================================================

    bool enabled{true};           ///< Whether the rule is active
    int priority{0};              ///< Evaluation priority (higher = first)

    // =========================================================================
    // Matching Configuration
    // =========================================================================

    std::vector<routing_condition> conditions;  ///< Conditions (AND logic)
    std::vector<routing_action> actions;        ///< Actions to execute on match

    // =========================================================================
    // Schedule (Optional)
    // =========================================================================

    std::optional<std::string> schedule_cron;   ///< Cron expression for scheduling
    std::optional<std::chrono::system_clock::time_point> effective_from;
    std::optional<std::chrono::system_clock::time_point> effective_until;

    // =========================================================================
    // Statistics
    // =========================================================================

    size_t triggered_count{0};    ///< Number of times the rule was triggered
    size_t success_count{0};      ///< Successful forwarding count
    size_t failure_count{0};      ///< Failed forwarding count
    std::chrono::system_clock::time_point last_triggered;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;

    // =========================================================================
    // Database Fields
    // =========================================================================

    int64_t pk{0};                ///< Primary key (0 if not persisted)

    // =========================================================================
    // Convenience Methods
    // =========================================================================

    /**
     * @brief Check if the rule is currently effective based on schedule
     * @return true if the rule can be applied now
     */
    [[nodiscard]] bool is_effective_now() const noexcept {
        if (!enabled) return false;

        auto now = std::chrono::system_clock::now();

        if (effective_from.has_value() && now < effective_from.value()) {
            return false;
        }

        if (effective_until.has_value() && now > effective_until.value()) {
            return false;
        }

        return true;
    }
};

// =============================================================================
// Routing Event Callback
// =============================================================================

/**
 * @brief Callback type for routing events
 *
 * @param rule_id The ID of the rule that triggered
 * @param instance_uid The SOP Instance UID being routed
 * @param triggered_actions The actions that will be executed
 */
using routing_event_callback = std::function<void(
    const std::string& rule_id,
    const std::string& instance_uid,
    const std::vector<routing_action>& triggered_actions)>;

// =============================================================================
// Routing Manager Configuration
// =============================================================================

/**
 * @brief Configuration for the routing manager
 */
struct routing_manager_config {
    bool enabled{true};                            ///< Enable routing globally
    size_t max_rules{100};                         ///< Maximum number of rules
    std::chrono::seconds evaluation_timeout{5};    ///< Timeout for rule evaluation
};

// =============================================================================
// Routing Statistics
// =============================================================================

/**
 * @brief Statistics for routing operations
 */
struct routing_statistics {
    size_t total_evaluated{0};    ///< Total instances evaluated
    size_t total_matched{0};      ///< Total instances that matched a rule
    size_t total_forwarded{0};    ///< Total successful forwards
    size_t total_failed{0};       ///< Total failed forwards
};

// =============================================================================
// Test Result
// =============================================================================

/**
 * @brief Result of testing rules against a dataset (dry run)
 */
struct routing_test_result {
    bool matched{false};                          ///< Whether any rule matched
    std::string matched_rule_id;                  ///< ID of the matched rule
    std::vector<routing_action> actions;          ///< Actions that would execute
};

}  // namespace pacs::client
