/**
 * @file patient_record.hpp
 * @brief Patient record data structures for database operations
 *
 * This file provides the patient_record and patient_query structures
 * for patient data manipulation in the PACS index database.
 *
 * @see SRS-STOR-003, FR-4.2
 */

#pragma once

#include <chrono>
#include <optional>
#include <string>

namespace pacs::storage {

/**
 * @brief Patient record from the database
 *
 * Represents a single patient record with all demographic information.
 * Maps directly to the patients table in the database.
 */
struct patient_record {
    /// Primary key (auto-generated)
    int64_t pk{0};

    /// Patient ID - DICOM tag (0010,0020)
    std::string patient_id;

    /// Patient's Name - DICOM tag (0010,0010)
    std::string patient_name;

    /// Patient's Birth Date - DICOM tag (0010,0030) format: YYYYMMDD
    std::string birth_date;

    /// Patient's Sex - DICOM tag (0010,0040) values: M, F, O
    std::string sex;

    /// Other Patient IDs - DICOM tag (0010,1000)
    std::string other_ids;

    /// Ethnic Group - DICOM tag (0010,2160)
    std::string ethnic_group;

    /// Patient Comments - DICOM tag (0010,4000)
    std::string comments;

    /// Record creation timestamp
    std::chrono::system_clock::time_point created_at;

    /// Record last update timestamp
    std::chrono::system_clock::time_point updated_at;

    /**
     * @brief Check if this record has valid data
     *
     * @return true if patient_id is not empty
     */
    [[nodiscard]] auto is_valid() const noexcept -> bool {
        return !patient_id.empty();
    }
};

/**
 * @brief Query parameters for patient search
 *
 * Supports wildcard matching using '*' for prefix/suffix matching.
 * Empty fields are not included in the query filter.
 *
 * @example
 * @code
 * patient_query query;
 * query.patient_name = "Doe*";  // Match names starting with "Doe"
 * query.sex = "M";              // Exact match
 * auto results = db.search_patients(query);
 * @endcode
 */
struct patient_query {
    /// Patient ID pattern (supports * wildcard)
    std::optional<std::string> patient_id;

    /// Patient name pattern (supports * wildcard)
    std::optional<std::string> patient_name;

    /// Birth date (exact match, format: YYYYMMDD)
    std::optional<std::string> birth_date;

    /// Birth date range start (inclusive)
    std::optional<std::string> birth_date_from;

    /// Birth date range end (inclusive)
    std::optional<std::string> birth_date_to;

    /// Sex (exact match: M, F, O)
    std::optional<std::string> sex;

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
               birth_date.has_value() || birth_date_from.has_value() ||
               birth_date_to.has_value() || sex.has_value();
    }
};

}  // namespace pacs::storage
