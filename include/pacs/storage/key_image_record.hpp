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
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <chrono>
#include <optional>
#include <string>

namespace kcenon::pacs::storage {

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

}  // namespace kcenon::pacs::storage
