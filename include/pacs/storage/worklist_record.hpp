/**
 * @file worklist_record.hpp
 * @brief Modality Worklist (MWL) record data structures
 *
 * This file provides the worklist_item and worklist_query structures for
 * Modality Worklist data manipulation in the PACS index database. MWL provides
 * scheduled procedure information to modalities for patient/procedure selection.
 *
 * @see SRS-SVC-006, FR-3.3
 */

#pragma once

#include <chrono>
#include <optional>
#include <string>

namespace pacs::storage {

/**
 * @brief Worklist step status values
 *
 * Defines the valid states for a Scheduled Procedure Step.
 */
enum class worklist_status {
    scheduled,   ///< Procedure is scheduled (default)
    started,     ///< Procedure has been started (MPPS received)
    completed    ///< Procedure has been completed
};

/**
 * @brief Convert worklist_status enum to string representation
 *
 * @param status The status enum value
 * @return String representation
 */
[[nodiscard]] inline auto to_string(worklist_status status) -> std::string {
    switch (status) {
        case worklist_status::scheduled:
            return "SCHEDULED";
        case worklist_status::started:
            return "STARTED";
        case worklist_status::completed:
            return "COMPLETED";
        default:
            return "SCHEDULED";
    }
}

/**
 * @brief Parse string to worklist_status enum
 *
 * @param str The string representation
 * @return Optional containing the status if valid, nullopt otherwise
 */
[[nodiscard]] inline auto parse_worklist_status(std::string_view str)
    -> std::optional<worklist_status> {
    if (str == "SCHEDULED") {
        return worklist_status::scheduled;
    }
    if (str == "STARTED") {
        return worklist_status::started;
    }
    if (str == "COMPLETED") {
        return worklist_status::completed;
    }
    return std::nullopt;
}

/**
 * @brief Worklist item record from the database
 *
 * Represents a single Scheduled Procedure Step item for Modality Worklist.
 * Maps directly to the worklist table in the database.
 *
 * MWL Workflow:
 * @code
 *     RIS/HIS creates worklist item (status = SCHEDULED)
 *                   │
 *                   ▼
 *          ┌─────────────────┐
 *          │    SCHEDULED    │ ◄── MWL C-FIND returns this
 *          └────────┬────────┘
 *                   │
 *            MPPS N-CREATE
 *                   │
 *                   ▼
 *          ┌─────────────────┐
 *          │     STARTED     │
 *          └────────┬────────┘
 *                   │
 *            MPPS N-SET (COMPLETED)
 *                   │
 *                   ▼
 *          ┌─────────────────┐
 *          │    COMPLETED    │
 *          └─────────────────┘
 * @endcode
 */
struct worklist_item {
    /// Primary key (auto-generated)
    int64_t pk{0};

    /// Scheduled Procedure Step ID (required)
    std::string step_id;

    /// Current status of the procedure step
    std::string step_status;

    /// Patient ID (required)
    std::string patient_id;

    /// Patient's Name in DICOM PN format
    std::string patient_name;

    /// Patient's Birth Date (YYYYMMDD format)
    std::string birth_date;

    /// Patient's Sex (M, F, O)
    std::string sex;

    /// Accession Number
    std::string accession_no;

    /// Requested Procedure ID
    std::string requested_proc_id;

    /// Study Instance UID (pre-assigned for the procedure)
    std::string study_uid;

    /// Scheduled Procedure Step Start Date/Time (YYYYMMDDHHMMSS format)
    std::string scheduled_datetime;

    /// Scheduled Station AE Title
    std::string station_ae;

    /// Scheduled Station Name
    std::string station_name;

    /// Modality (CT, MR, etc.) (required)
    std::string modality;

    /// Scheduled Procedure Step Description
    std::string procedure_desc;

    /// Protocol Code Sequence (JSON serialized)
    std::string protocol_code;

    /// Referring Physician's Name
    std::string referring_phys;

    /// Referring Physician ID
    std::string referring_phys_id;

    /// Record creation timestamp
    std::chrono::system_clock::time_point created_at;

    /// Record last update timestamp
    std::chrono::system_clock::time_point updated_at;

    /**
     * @brief Check if this record has valid data
     *
     * @return true if required fields are not empty
     */
    [[nodiscard]] auto is_valid() const noexcept -> bool {
        return !step_id.empty() && !patient_id.empty() && !modality.empty() &&
               !scheduled_datetime.empty();
    }

    /**
     * @brief Check if this item is available for MWL query
     *
     * Only SCHEDULED items are returned in MWL C-FIND responses.
     *
     * @return true if status is SCHEDULED
     */
    [[nodiscard]] auto is_scheduled() const noexcept -> bool {
        return step_status == "SCHEDULED" || step_status.empty();
    }

    /**
     * @brief Get the status as enum
     *
     * @return Optional containing the status enum if valid
     */
    [[nodiscard]] auto get_status() const -> std::optional<worklist_status> {
        return parse_worklist_status(step_status);
    }
};

/**
 * @brief Query parameters for worklist search
 *
 * Used for MWL C-FIND operations. Empty fields are not included in the filter.
 * Only items with status 'SCHEDULED' are returned by default.
 *
 * @example
 * @code
 * worklist_query query;
 * query.station_ae = "CT_SCANNER_1";
 * query.modality = "CT";
 * query.scheduled_date_from = "20231115";
 * query.scheduled_date_to = "20231115";
 * auto results = db.query_worklist(query);
 * @endcode
 */
struct worklist_query {
    /// Scheduled Station AE Title filter (exact match)
    std::optional<std::string> station_ae;

    /// Modality filter (exact match)
    std::optional<std::string> modality;

    /// Scheduled date range begin (inclusive, format: YYYYMMDD)
    std::optional<std::string> scheduled_date_from;

    /// Scheduled date range end (inclusive, format: YYYYMMDD)
    std::optional<std::string> scheduled_date_to;

    /// Patient ID filter (supports wildcards with '*')
    std::optional<std::string> patient_id;

    /// Patient Name filter (supports wildcards with '*')
    std::optional<std::string> patient_name;

    /// Accession Number filter (exact match)
    std::optional<std::string> accession_no;

    /// Step ID filter (exact match)
    std::optional<std::string> step_id;

    /// Include non-SCHEDULED items (default: false, only SCHEDULED)
    bool include_all_status{false};

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
        return station_ae.has_value() || modality.has_value() ||
               scheduled_date_from.has_value() || scheduled_date_to.has_value() ||
               patient_id.has_value() || patient_name.has_value() ||
               accession_no.has_value() || step_id.has_value();
    }
};

}  // namespace pacs::storage
