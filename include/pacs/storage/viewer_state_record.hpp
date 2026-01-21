/**
 * @file viewer_state_record.hpp
 * @brief Viewer state record data structures for database operations
 *
 * This file provides the viewer_state_record and recent_study_record
 * structures for storing and retrieving viewer configurations and
 * user study access history.
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
 * @brief Viewer state record from the database
 *
 * Represents a saved viewer state including layout, viewport
 * configurations, and window/level settings.
 * Maps directly to the viewer_states table in the database.
 */
struct viewer_state_record {
    /// Primary key (auto-generated)
    int64_t pk{0};

    /// Unique state identifier (UUID)
    std::string state_id;

    /// Study Instance UID - DICOM tag (0020,000D)
    std::string study_uid;

    /// User who saved the state
    std::string user_id;

    /// Full viewer state as JSON (layout, viewports, settings)
    std::string state_json;

    /// Record creation timestamp
    std::chrono::system_clock::time_point created_at;

    /// Record last update timestamp
    std::chrono::system_clock::time_point updated_at;

    /**
     * @brief Check if this record has valid data
     *
     * @return true if state_id and study_uid are not empty
     */
    [[nodiscard]] auto is_valid() const noexcept -> bool {
        return !state_id.empty() && !study_uid.empty();
    }
};

/**
 * @brief Query parameters for viewer state search
 *
 * Supports filtering by study and user.
 * Empty fields are not included in the query filter.
 *
 * @example
 * @code
 * viewer_state_query query;
 * query.study_uid = "1.2.3.4.5";
 * query.user_id = "user123";
 * auto results = repo.search(query);
 * @endcode
 */
struct viewer_state_query {
    /// Study Instance UID filter
    std::optional<std::string> study_uid;

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
        return study_uid.has_value() || user_id.has_value();
    }
};

/**
 * @brief Recent study access record from the database
 *
 * Tracks which studies a user has recently accessed
 * for quick navigation in the viewer.
 * Maps directly to the recent_studies table in the database.
 */
struct recent_study_record {
    /// Primary key (auto-generated)
    int64_t pk{0};

    /// User who accessed the study
    std::string user_id;

    /// Study Instance UID - DICOM tag (0020,000D)
    std::string study_uid;

    /// When the study was accessed
    std::chrono::system_clock::time_point accessed_at;

    /**
     * @brief Check if this record has valid data
     *
     * @return true if user_id and study_uid are not empty
     */
    [[nodiscard]] auto is_valid() const noexcept -> bool {
        return !user_id.empty() && !study_uid.empty();
    }
};

}  // namespace pacs::storage
