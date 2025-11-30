/**
 * @file study_record.hpp
 * @brief Study record data structures for database operations
 *
 * This file provides the study_record and study_query structures
 * for study data manipulation in the PACS index database.
 *
 * @see SRS-STOR-003, FR-4.2
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace pacs::storage {

/**
 * @brief Study record from the database
 *
 * Represents a single study record with all study-level information.
 * Maps directly to the studies table in the database.
 */
struct study_record {
    /// Primary key (auto-generated)
    int64_t pk{0};

    /// Foreign key to patients table
    int64_t patient_pk{0};

    /// Study Instance UID - DICOM tag (0020,000D)
    std::string study_uid;

    /// Study ID - DICOM tag (0020,0010)
    std::string study_id;

    /// Study Date - DICOM tag (0008,0020) format: YYYYMMDD
    std::string study_date;

    /// Study Time - DICOM tag (0008,0030) format: HHMMSS
    std::string study_time;

    /// Accession Number - DICOM tag (0008,0050)
    std::string accession_number;

    /// Referring Physician's Name - DICOM tag (0008,0090)
    std::string referring_physician;

    /// Study Description - DICOM tag (0008,1030)
    std::string study_description;

    /// Modalities in Study - DICOM tag (0008,0061) e.g., "CT\\MR"
    std::string modalities_in_study;

    /// Number of series in this study (denormalized)
    int num_series{0};

    /// Number of instances in this study (denormalized)
    int num_instances{0};

    /// Record creation timestamp
    std::chrono::system_clock::time_point created_at;

    /// Record last update timestamp
    std::chrono::system_clock::time_point updated_at;

    /**
     * @brief Check if this record has valid data
     *
     * @return true if study_uid is not empty
     */
    [[nodiscard]] auto is_valid() const noexcept -> bool {
        return !study_uid.empty();
    }
};

/**
 * @brief Query parameters for study search
 *
 * Supports wildcard matching using '*' for prefix/suffix matching.
 * Empty fields are not included in the query filter.
 *
 * @example
 * @code
 * study_query query;
 * query.patient_id = "12345";     // Exact patient match
 * query.study_date_from = "20230101";
 * query.study_date_to = "20231231";
 * query.modality = "CT";
 * auto results = db.search_studies(query);
 * @endcode
 */
struct study_query {
    /// Patient ID for filtering by patient (exact match or wildcard)
    std::optional<std::string> patient_id;

    /// Patient name pattern (supports * wildcard)
    std::optional<std::string> patient_name;

    /// Study Instance UID (exact match)
    std::optional<std::string> study_uid;

    /// Study ID pattern (supports * wildcard)
    std::optional<std::string> study_id;

    /// Study date (exact match, format: YYYYMMDD)
    std::optional<std::string> study_date;

    /// Study date range start (inclusive)
    std::optional<std::string> study_date_from;

    /// Study date range end (inclusive)
    std::optional<std::string> study_date_to;

    /// Accession number pattern (supports * wildcard)
    std::optional<std::string> accession_number;

    /// Modality filter (exact match, e.g., "CT", "MR")
    std::optional<std::string> modality;

    /// Referring physician pattern (supports * wildcard)
    std::optional<std::string> referring_physician;

    /// Study description pattern (supports * wildcard)
    std::optional<std::string> study_description;

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
        return patient_id.has_value() || patient_name.has_value() ||
               study_uid.has_value() || study_id.has_value() ||
               study_date.has_value() || study_date_from.has_value() ||
               study_date_to.has_value() || accession_number.has_value() ||
               modality.has_value() || referring_physician.has_value() ||
               study_description.has_value();
    }
};

}  // namespace pacs::storage
