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
 * @file ct_storage.hpp
 * @brief CT Image Storage SOP Classes
 *
 * Provides SOP Class definitions and utilities for Computed Tomography (CT)
 * image storage. Supports both standard CT Image Storage and Enhanced CT
 * Image Storage.
 *
 * @see DICOM PS3.4 Section B - Storage Service Class
 * @see DICOM PS3.3 Section A.3 - CT Image IOD
 * @see Issue #717 - Add CT Image IOD Validator
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_SERVICES_SOP_CLASSES_CT_STORAGE_HPP
#define PACS_SERVICES_SOP_CLASSES_CT_STORAGE_HPP

#include <string>
#include <string_view>
#include <vector>

namespace pacs::services::sop_classes {

// =============================================================================
// CT Storage SOP Class UIDs
// =============================================================================

/// CT Image Storage SOP Class UID
inline constexpr std::string_view ct_image_storage_uid =
    "1.2.840.10008.5.1.4.1.1.2";

/// Enhanced CT Image Storage SOP Class UID
inline constexpr std::string_view enhanced_ct_image_storage_uid =
    "1.2.840.10008.5.1.4.1.1.2.1";

// =============================================================================
// CT SOP Class Utilities
// =============================================================================

/**
 * @brief Get all CT Storage SOP Class UIDs
 * @return Vector of CT Storage SOP Class UIDs
 */
[[nodiscard]] std::vector<std::string> get_ct_storage_sop_classes();

/**
 * @brief Check if a SOP Class UID is a CT Storage SOP Class
 * @param uid The SOP Class UID to check
 * @return true if this is a CT storage SOP class
 */
[[nodiscard]] bool is_ct_storage_sop_class(std::string_view uid) noexcept;

/**
 * @brief Check if photometric interpretation is valid for CT
 *
 * CT images are always grayscale (MONOCHROME1 or MONOCHROME2).
 *
 * @param value The photometric interpretation string
 * @return true if valid for CT images
 */
[[nodiscard]] bool is_valid_ct_photometric(std::string_view value) noexcept;

}  // namespace pacs::services::sop_classes

#endif  // PACS_SERVICES_SOP_CLASSES_CT_STORAGE_HPP
