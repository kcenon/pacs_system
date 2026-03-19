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
 * @file ups_workitem.hpp
 * @brief Unified Procedure Step (UPS) workitem data structures
 *
 * This file provides the ups_workitem, ups_subscription, and related query
 * structures for UPS data manipulation in the PACS index database.
 * UPS is the modern DICOM workflow service (PS3.4 Annex CC) that provides
 * unified task management replacing the separate MWL+MPPS workflow.
 *
 * @see DICOM PS3.4 Annex CC - Unified Procedure Step Service
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace kcenon::pacs::storage {

/**
 * @brief UPS workitem state values
 *
 * Defines the valid states for a Unified Procedure Step workitem.
 * State transitions are unidirectional as defined in PS3.4 Table CC.1.1-2.
 */
enum class ups_state {
    scheduled,     ///< Workitem is scheduled (initial state)
    in_progress,   ///< Workitem is being performed
    completed,     ///< Workitem completed successfully (final)
    canceled       ///< Workitem was canceled (final)
};

/**
 * @brief Convert ups_state enum to string representation
 *
 * @param state The state enum value
 * @return String representation matching DICOM standard
 */
[[nodiscard]] inline auto to_string(ups_state state) -> std::string {
    switch (state) {
        case ups_state::scheduled:
            return "SCHEDULED";
        case ups_state::in_progress:
            return "IN PROGRESS";
        case ups_state::completed:
            return "COMPLETED";
        case ups_state::canceled:
            return "CANCELED";
        default:
            return "SCHEDULED";
    }
}

/**
 * @brief Parse string to ups_state enum
 *
 * @param str The string representation
 * @return Optional containing the state if valid, nullopt otherwise
 */
[[nodiscard]] inline auto parse_ups_state(std::string_view str)
    -> std::optional<ups_state> {
    if (str == "SCHEDULED") {
        return ups_state::scheduled;
    }
    if (str == "IN PROGRESS") {
        return ups_state::in_progress;
    }
    if (str == "COMPLETED") {
        return ups_state::completed;
    }
    if (str == "CANCELED") {
        return ups_state::canceled;
    }
    return std::nullopt;
}

/**
 * @brief UPS workitem priority values
 *
 * Defines the priority levels for UPS workitems (PS3.4 CC.1.1).
 */
enum class ups_priority {
    low,      ///< Low priority
    medium,   ///< Medium priority (default)
    high      ///< High priority
};

/**
 * @brief Convert ups_priority enum to string representation
 *
 * @param priority The priority enum value
 * @return String representation matching DICOM standard
 */
[[nodiscard]] inline auto to_string(ups_priority priority) -> std::string {
    switch (priority) {
        case ups_priority::low:
            return "LOW";
        case ups_priority::medium:
            return "MEDIUM";
        case ups_priority::high:
            return "HIGH";
        default:
            return "MEDIUM";
    }
}

/**
 * @brief Parse string to ups_priority enum
 *
 * @param str The string representation
 * @return Optional containing the priority if valid, nullopt otherwise
 */
[[nodiscard]] inline auto parse_ups_priority(std::string_view str)
    -> std::optional<ups_priority> {
    if (str == "LOW") {
        return ups_priority::low;
    }
    if (str == "MEDIUM") {
        return ups_priority::medium;
    }
    if (str == "HIGH") {
        return ups_priority::high;
    }
    return std::nullopt;
}

/**
 * @brief UPS workitem record from the database
 *
 * Represents a single Unified Procedure Step workitem record.
 * Maps directly to the ups_workitems table in the database.
 *
 * UPS State Machine (PS3.4 Table CC.1.1-2):
 * @code
 *     N-CREATE (state = "SCHEDULED")
 *                   │
 *                   ▼
 *          ┌─────────────────┐
 *          │    SCHEDULED    │
 *          └────────┬────────┘
 *                   │
 *       ┌───────────┼───────────┐
 *       │ N-ACTION  │  N-ACTION │
 *       │ (claim)   │ (cancel)  │
 *       ▼           │           ▼
 *  ┌───────────┐    │    ┌──────────┐
 *  │IN PROGRESS│    │    │ CANCELED │
 *  └─────┬─────┘    │    └──────────┘
 *        │          │
 *    ┌───┼──────────┘
 *    │   │ N-ACTION
 *    │   │ (cancel)
 *    │   ▼
 *    │ ┌──────────┐
 *    │ │ CANCELED │
 *    │ └──────────┘
 *    │
 *    │ N-SET (final state)
 *    ▼
 *  ┌───────────┐
 *  │ COMPLETED │
 *  └───────────┘
 *
 *  Note: COMPLETED and CANCELED are final states
 * @endcode
 */
struct ups_workitem {
    /// Primary key (auto-generated)
    int64_t pk{0};

    /// UPS SOP Instance UID - unique identifier for this workitem
    std::string workitem_uid;

    /// Current state of the workitem
    std::string state;

    /// Procedure Step Label (human-readable description)
    std::string procedure_step_label;

    /// Worklist Label (for grouping workitems)
    std::string worklist_label;

    /// Priority (LOW, MEDIUM, HIGH)
    std::string priority;

    /// Scheduled Procedure Step Start DateTime (DICOM DT format)
    std::string scheduled_start_datetime;

    /// Expected Completion DateTime (DICOM DT format)
    std::string expected_completion_datetime;

    /// Scheduled Station Name Code Sequence (JSON serialized)
    std::string scheduled_station_name;

    /// Scheduled Station Class Code Sequence (JSON serialized)
    std::string scheduled_station_class;

    /// Scheduled Station Geographic Location Code Sequence (JSON serialized)
    std::string scheduled_station_geographic;

    /// Scheduled Human Performers Sequence (JSON serialized)
    std::string scheduled_human_performers;

    /// Input Information Sequence (JSON serialized references to input data)
    std::string input_information;

    /// Performing AE Title (set when claimed)
    std::string performing_ae;

    /// Procedure Step State (progress description text)
    std::string progress_description;

    /// Procedure Step Progress (0-100 percentage)
    int progress_percent{0};

    /// Output Information Sequence (JSON serialized references to output data)
    std::string output_information;

    /// Transaction UID (set when state changes to IN PROGRESS)
    std::string transaction_uid;

    /// Record creation timestamp
    std::chrono::system_clock::time_point created_at;

    /// Record last update timestamp
    std::chrono::system_clock::time_point updated_at;

    /**
     * @brief Check if this record has valid data
     *
     * @return true if workitem_uid is not empty
     */
    [[nodiscard]] auto is_valid() const noexcept -> bool {
        return !workitem_uid.empty();
    }

    /**
     * @brief Check if this workitem is in a final state
     *
     * @return true if state is COMPLETED or CANCELED
     */
    [[nodiscard]] auto is_final() const noexcept -> bool {
        return state == "COMPLETED" || state == "CANCELED";
    }

    /**
     * @brief Get the state as enum
     *
     * @return Optional containing the state enum if valid
     */
    [[nodiscard]] auto get_state() const -> std::optional<ups_state> {
        return parse_ups_state(state);
    }

    /**
     * @brief Get the priority as enum
     *
     * @return Optional containing the priority enum if valid
     */
    [[nodiscard]] auto get_priority() const -> std::optional<ups_priority> {
        return parse_ups_priority(priority);
    }
};

/**
 * @brief UPS subscription record from the database
 *
 * Represents a subscription to UPS workitem state change events.
 * Subscribers receive N-EVENT-REPORT notifications when workitems
 * matching their criteria change state.
 *
 * Subscription types (PS3.4 CC.2.4):
 * - Specific workitem: Subscribe to a single workitem by UID
 * - Filtered worklist: Subscribe to workitems matching a filter
 * - Global: Subscribe to all workitem state changes
 */
struct ups_subscription {
    /// Primary key (auto-generated)
    int64_t pk{0};

    /// Subscriber AE Title (required)
    std::string subscriber_ae;

    /// Specific workitem UID (empty for worklist/global subscriptions)
    std::string workitem_uid;

    /// Whether deletion is locked for this subscriber
    bool deletion_lock{false};

    /// Filter criteria for worklist subscriptions (JSON serialized)
    std::string filter_criteria;

    /// Record creation timestamp
    std::chrono::system_clock::time_point created_at;

    /**
     * @brief Check if this is a global subscription
     *
     * A global subscription matches all workitems.
     *
     * @return true if no workitem_uid and no filter_criteria
     */
    [[nodiscard]] auto is_global() const noexcept -> bool {
        return workitem_uid.empty() && filter_criteria.empty();
    }

    /**
     * @brief Check if this is a workitem-specific subscription
     *
     * @return true if subscribed to a specific workitem UID
     */
    [[nodiscard]] auto is_workitem_specific() const noexcept -> bool {
        return !workitem_uid.empty();
    }
};

/**
 * @brief Query parameters for UPS workitem search
 *
 * Empty fields are not included in the query filter.
 * Used for UPS C-FIND operations.
 *
 * @example
 * @code
 * ups_workitem_query query;
 * query.state = "SCHEDULED";
 * query.scheduled_station_name = "CT_SCANNER_1";
 * auto results = db.search_ups_workitems(query);
 * @endcode
 */
struct ups_workitem_query {
    /// Workitem SOP Instance UID (exact match)
    std::optional<std::string> workitem_uid;

    /// State filter (exact match)
    std::optional<std::string> state;

    /// Priority filter (exact match)
    std::optional<std::string> priority;

    /// Procedure Step Label filter (supports wildcards with '*')
    std::optional<std::string> procedure_step_label;

    /// Worklist Label filter (supports wildcards with '*')
    std::optional<std::string> worklist_label;

    /// Performing AE filter (exact match)
    std::optional<std::string> performing_ae;

    /// Scheduled start date range begin (inclusive, format: YYYYMMDD)
    std::optional<std::string> scheduled_date_from;

    /// Scheduled start date range end (inclusive, format: YYYYMMDD)
    std::optional<std::string> scheduled_date_to;

    /// Maximum number of results to return (0 = unlimited)
    size_t limit{0};

    /// Offset for pagination
    size_t offset{0};

    /**
     * @brief Check if any filter criteria is set
     *
     * @return true if at least one filter field is set
     */
    [[nodiscard]] auto has_criteria() const noexcept -> bool {
        return workitem_uid.has_value() || state.has_value() ||
               priority.has_value() || procedure_step_label.has_value() ||
               worklist_label.has_value() || performing_ae.has_value() ||
               scheduled_date_from.has_value() || scheduled_date_to.has_value();
    }
};

}  // namespace kcenon::pacs::storage
