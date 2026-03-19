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
 * @file viewer_state_record.hpp
 * @brief Viewer state record data structures for database operations
 *
 * This file provides the viewer_state_record and recent_study_record
 * structures for storing and retrieving viewer configurations and
 * user study access history.
 *
 * @see Issue #545 - Implement Annotation & Measurement APIs
 * @see Issue #581 - Part 1: Data Models and Repositories
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <chrono>
#include <optional>
#include <string>

namespace kcenon::pacs::storage {

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

}  // namespace kcenon::pacs::storage
