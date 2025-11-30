/**
 * @file instance_record.hpp
 * @brief Instance record data structures for database operations
 *
 * This file provides the instance_record and instance_query structures
 * for instance (SOP Instance) data manipulation in the PACS index database.
 *
 * @see SRS-STOR-003, FR-4.3
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace pacs::storage {

/**
 * @brief Instance record from the database
 *
 * Represents a single DICOM instance (SOP Instance) record with all
 * instance-level information. Maps directly to the instances table.
 */
struct instance_record {
    /// Primary key (auto-generated)
    int64_t pk{0};

    /// Foreign key to series table
    int64_t series_pk{0};

    /// SOP Instance UID - DICOM tag (0008,0018)
    std::string sop_uid;

    /// SOP Class UID - DICOM tag (0008,0016)
    std::string sop_class_uid;

    /// Instance Number - DICOM tag (0020,0013)
    std::optional<int> instance_number;

    /// Transfer Syntax UID - DICOM tag (0002,0010)
    std::string transfer_syntax;

    /// Content Date - DICOM tag (0008,0023) format: YYYYMMDD
    std::string content_date;

    /// Content Time - DICOM tag (0008,0033) format: HHMMSS
    std::string content_time;

    /// Image Rows - DICOM tag (0028,0010)
    std::optional<int> rows;

    /// Image Columns - DICOM tag (0028,0011)
    std::optional<int> columns;

    /// Bits Allocated - DICOM tag (0028,0100)
    std::optional<int> bits_allocated;

    /// Number of Frames - DICOM tag (0028,0008)
    std::optional<int> number_of_frames;

    /// File path where the instance is stored
    std::string file_path;

    /// File size in bytes
    int64_t file_size{0};

    /// File hash (e.g., MD5 or SHA-256) for integrity verification
    std::string file_hash;

    /// Record creation timestamp
    std::chrono::system_clock::time_point created_at;

    /**
     * @brief Check if this record has valid data
     *
     * @return true if sop_uid and file_path are not empty
     */
    [[nodiscard]] auto is_valid() const noexcept -> bool {
        return !sop_uid.empty() && !file_path.empty();
    }
};

/**
 * @brief Query parameters for instance search
 *
 * Supports wildcard matching using '*' for prefix/suffix matching.
 * Empty fields are not included in the query filter.
 *
 * @example
 * @code
 * instance_query query;
 * query.series_uid = "1.2.840.123456.1";  // Exact series match
 * query.sop_class_uid = "1.2.840.10008.5.1.4.1.1.2";  // CT Image Storage
 * auto results = db.search_instances(query);
 * @endcode
 */
struct instance_query {
    /// Series Instance UID for filtering by series (exact match)
    std::optional<std::string> series_uid;

    /// SOP Instance UID (exact match)
    std::optional<std::string> sop_uid;

    /// SOP Class UID filter (exact match)
    std::optional<std::string> sop_class_uid;

    /// Instance number filter
    std::optional<int> instance_number;

    /// Content date (exact match, format: YYYYMMDD)
    std::optional<std::string> content_date;

    /// Content date range start (inclusive)
    std::optional<std::string> content_date_from;

    /// Content date range end (inclusive)
    std::optional<std::string> content_date_to;

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
        return series_uid.has_value() || sop_uid.has_value() ||
               sop_class_uid.has_value() || instance_number.has_value() ||
               content_date.has_value() || content_date_from.has_value() ||
               content_date_to.has_value();
    }
};

}  // namespace pacs::storage
