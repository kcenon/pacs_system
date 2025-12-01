/**
 * @file mpps_record.hpp
 * @brief MPPS (Modality Performed Procedure Step) record data structures
 *
 * This file provides the mpps_record and mpps_query structures for MPPS data
 * manipulation in the PACS index database. MPPS tracks exam progress from
 * modalities and enables workflow integration with RIS/HIS systems.
 *
 * @see SRS-SVC-007, FR-3.4
 */

#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace pacs::storage {

/**
 * @brief MPPS status values
 *
 * Defines the valid states for a Modality Performed Procedure Step.
 * COMPLETED and DISCONTINUED are final states.
 */
enum class mpps_status {
    in_progress,   ///< Procedure is currently being performed
    completed,     ///< Procedure completed successfully
    discontinued   ///< Procedure was stopped/cancelled
};

/**
 * @brief Convert mpps_status enum to string representation
 *
 * @param status The status enum value
 * @return String representation matching DICOM standard
 */
[[nodiscard]] inline auto to_string(mpps_status status) -> std::string {
    switch (status) {
        case mpps_status::in_progress:
            return "IN PROGRESS";
        case mpps_status::completed:
            return "COMPLETED";
        case mpps_status::discontinued:
            return "DISCONTINUED";
        default:
            return "IN PROGRESS";
    }
}

/**
 * @brief Parse string to mpps_status enum
 *
 * @param str The string representation
 * @return Optional containing the status if valid, nullopt otherwise
 */
[[nodiscard]] inline auto parse_mpps_status(std::string_view str)
    -> std::optional<mpps_status> {
    if (str == "IN PROGRESS") {
        return mpps_status::in_progress;
    }
    if (str == "COMPLETED") {
        return mpps_status::completed;
    }
    if (str == "DISCONTINUED") {
        return mpps_status::discontinued;
    }
    return std::nullopt;
}

/**
 * @brief Information about a performed series
 *
 * Used to track which series were created during the procedure.
 */
struct performed_series_info {
    std::string series_uid;     ///< Series Instance UID
    std::string protocol_name;  ///< Protocol name used
    int num_instances{0};       ///< Number of instances in this series
};

/**
 * @brief MPPS record from the database
 *
 * Represents a single Modality Performed Procedure Step record.
 * Maps directly to the mpps table in the database.
 *
 * MPPS State Machine:
 * @code
 *     N-CREATE (status = "IN PROGRESS")
 *                   │
 *                   ▼
 *          ┌─────────────────┐
 *          │   IN PROGRESS   │
 *          └────────┬────────┘
 *                   │
 *       ┌───────────┼───────────┐
 *       │ N-SET     │     N-SET │
 *       │ COMPLETED │  DISCONTINUED
 *       ▼           ▼           ▼
 *  ┌───────────┐  ┌──────────────┐
 *  │ COMPLETED │  │ DISCONTINUED │
 *  └───────────┘  └──────────────┘
 *
 *  Note: COMPLETED and DISCONTINUED are final states
 * @endcode
 */
struct mpps_record {
    /// Primary key (auto-generated)
    int64_t pk{0};

    /// SOP Instance UID - unique identifier for this MPPS
    std::string mpps_uid;

    /// Current status of the procedure step
    std::string status;

    /// Start date/time of the procedure (DICOM DT format: YYYYMMDDHHMMSS)
    std::string start_datetime;

    /// End date/time of the procedure (set when completed/discontinued)
    std::string end_datetime;

    /// Performing station AE Title
    std::string station_ae;

    /// Performing station name
    std::string station_name;

    /// Modality type (CT, MR, etc.)
    std::string modality;

    /// Related Study Instance UID
    std::string study_uid;

    /// Accession number
    std::string accession_no;

    /// Scheduled Procedure Step ID (from worklist)
    std::string scheduled_step_id;

    /// Requested Procedure ID
    std::string requested_proc_id;

    /// Performed series information (JSON serialized)
    std::string performed_series;

    /// Record creation timestamp
    std::chrono::system_clock::time_point created_at;

    /// Record last update timestamp
    std::chrono::system_clock::time_point updated_at;

    /**
     * @brief Check if this record has valid data
     *
     * @return true if mpps_uid is not empty
     */
    [[nodiscard]] auto is_valid() const noexcept -> bool {
        return !mpps_uid.empty();
    }

    /**
     * @brief Check if this MPPS is in a final state
     *
     * @return true if status is COMPLETED or DISCONTINUED
     */
    [[nodiscard]] auto is_final() const noexcept -> bool {
        return status == "COMPLETED" || status == "DISCONTINUED";
    }

    /**
     * @brief Get the status as enum
     *
     * @return Optional containing the status enum if valid
     */
    [[nodiscard]] auto get_status() const -> std::optional<mpps_status> {
        return parse_mpps_status(status);
    }
};

/**
 * @brief Query parameters for MPPS search
 *
 * Empty fields are not included in the query filter.
 *
 * @example
 * @code
 * mpps_query query;
 * query.station_ae = "CT_SCANNER_1";
 * query.status = "IN PROGRESS";
 * auto results = db.search_mpps(query);
 * @endcode
 */
struct mpps_query {
    /// MPPS SOP Instance UID (exact match)
    std::optional<std::string> mpps_uid;

    /// Status filter (exact match)
    std::optional<std::string> status;

    /// Station AE Title filter (exact match)
    std::optional<std::string> station_ae;

    /// Modality filter (exact match)
    std::optional<std::string> modality;

    /// Study Instance UID filter (exact match)
    std::optional<std::string> study_uid;

    /// Accession number filter (exact match)
    std::optional<std::string> accession_no;

    /// Start date range begin (inclusive, format: YYYYMMDD)
    std::optional<std::string> start_date_from;

    /// Start date range end (inclusive, format: YYYYMMDD)
    std::optional<std::string> start_date_to;

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
        return mpps_uid.has_value() || status.has_value() ||
               station_ae.has_value() || modality.has_value() ||
               study_uid.has_value() || accession_no.has_value() ||
               start_date_from.has_value() || start_date_to.has_value();
    }
};

}  // namespace pacs::storage
