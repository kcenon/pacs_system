/**
 * @file series_record.hpp
 * @brief Series record data structures for database operations
 *
 * This file provides the series_record and series_query structures
 * for series data manipulation in the PACS index database.
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
 * @brief Series record from the database
 *
 * Represents a single series record with all series-level information.
 * Maps directly to the series table in the database.
 */
struct series_record {
    /// Primary key (auto-generated)
    int64_t pk{0};

    /// Foreign key to studies table
    int64_t study_pk{0};

    /// Series Instance UID - DICOM tag (0020,000E)
    std::string series_uid;

    /// Modality - DICOM tag (0008,0060)
    std::string modality;

    /// Series Number - DICOM tag (0020,0011)
    std::optional<int> series_number;

    /// Series Description - DICOM tag (0008,103E)
    std::string series_description;

    /// Body Part Examined - DICOM tag (0018,0015)
    std::string body_part_examined;

    /// Station Name - DICOM tag (0008,1010)
    std::string station_name;

    /// Number of instances in this series (denormalized)
    int num_instances{0};

    /// Record creation timestamp
    std::chrono::system_clock::time_point created_at;

    /// Record last update timestamp
    std::chrono::system_clock::time_point updated_at;

    /**
     * @brief Check if this record has valid data
     *
     * @return true if series_uid is not empty
     */
    [[nodiscard]] auto is_valid() const noexcept -> bool {
        return !series_uid.empty();
    }
};

/**
 * @brief Query parameters for series search
 *
 * Supports wildcard matching using '*' for prefix/suffix matching.
 * Empty fields are not included in the query filter.
 *
 * @example
 * @code
 * series_query query;
 * query.study_uid = "1.2.840.123456";  // Exact study match
 * query.modality = "CT";               // Exact match
 * auto results = db.search_series(query);
 * @endcode
 */
struct series_query {
    /// Study Instance UID for filtering by study (exact match)
    std::optional<std::string> study_uid;

    /// Series Instance UID (exact match)
    std::optional<std::string> series_uid;

    /// Modality filter (exact match, e.g., "CT", "MR")
    std::optional<std::string> modality;

    /// Series number filter
    std::optional<int> series_number;

    /// Series description pattern (supports * wildcard)
    std::optional<std::string> series_description;

    /// Body part examined (exact match)
    std::optional<std::string> body_part_examined;

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
        return study_uid.has_value() || series_uid.has_value() ||
               modality.has_value() || series_number.has_value() ||
               series_description.has_value() || body_part_examined.has_value();
    }
};

}  // namespace pacs::storage
