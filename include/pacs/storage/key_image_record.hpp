/**
 * @file key_image_record.hpp
 * @brief Key image record data structures for database operations
 *
 * This file provides the key_image_record and key_image_query structures
 * for storing and retrieving key image markers on DICOM studies.
 *
 * Key images are significant images marked by users for easy reference,
 * following DICOM PS3.3 Key Object Selection Document patterns.
 *
 * @see Issue #545 - Implement Annotation & Measurement APIs
 * @see Issue #581 - Part 1: Data Models and Repositories
 */

#pragma once

#include <chrono>
#include <optional>
#include <string>

namespace pacs::storage {

/**
 * @brief Key image record from the database
 *
 * Represents a key image marker on a DICOM image.
 * Maps directly to the key_images table in the database.
 *
 * Key images are used to mark significant findings or important
 * images within a study for quick reference and reporting.
 */
struct key_image_record {
    /// Primary key (auto-generated)
    int64_t pk{0};

    /// Unique key image identifier (UUID)
    std::string key_image_id;

    /// Study Instance UID - DICOM tag (0020,000D)
    std::string study_uid;

    /// SOP Instance UID - DICOM tag (0008,0018)
    std::string sop_instance_uid;

    /// Frame number for multi-frame images (1-based)
    std::optional<int> frame_number;

    /// User who marked the key image
    std::string user_id;

    /// Reason for marking as key image
    std::string reason;

    /// Document title for Key Object Selection
    std::string document_title;

    /// Record creation timestamp
    std::chrono::system_clock::time_point created_at;

    /**
     * @brief Check if this record has valid data
     *
     * @return true if key_image_id, study_uid, and sop_instance_uid are not empty
     */
    [[nodiscard]] auto is_valid() const noexcept -> bool {
        return !key_image_id.empty() && !study_uid.empty() &&
               !sop_instance_uid.empty();
    }
};

/**
 * @brief Query parameters for key image search
 *
 * Supports filtering by study, instance, and user.
 * Empty fields are not included in the query filter.
 *
 * @example
 * @code
 * key_image_query query;
 * query.study_uid = "1.2.3.4.5";
 * auto results = repo.search(query);
 * @endcode
 */
struct key_image_query {
    /// Study Instance UID filter
    std::optional<std::string> study_uid;

    /// SOP Instance UID filter
    std::optional<std::string> sop_instance_uid;

    /// User ID filter
    std::optional<std::string> user_id;

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
        return study_uid.has_value() || sop_instance_uid.has_value() ||
               user_id.has_value();
    }
};

}  // namespace pacs::storage
