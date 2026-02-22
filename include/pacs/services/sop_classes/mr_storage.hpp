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
 * @file mr_storage.hpp
 * @brief MR Image Storage SOP Classes
 *
 * Provides SOP Class definitions and utilities for Magnetic Resonance (MR)
 * image storage. Supports both standard MR Image Storage and Enhanced MR
 * Image Storage.
 *
 * @see DICOM PS3.4 Section B - Storage Service Class
 * @see DICOM PS3.3 Section A.4 - MR Image IOD
 * @see Issue #718 - Add MR Image IOD Validator
 */

#ifndef PACS_SERVICES_SOP_CLASSES_MR_STORAGE_HPP
#define PACS_SERVICES_SOP_CLASSES_MR_STORAGE_HPP

#include <string>
#include <string_view>
#include <vector>

namespace pacs::services::sop_classes {

// =============================================================================
// MR Storage SOP Class UIDs
// =============================================================================

/// MR Image Storage SOP Class UID
inline constexpr std::string_view mr_image_storage_uid =
    "1.2.840.10008.5.1.4.1.1.4";

/// Enhanced MR Image Storage SOP Class UID
inline constexpr std::string_view enhanced_mr_image_storage_uid =
    "1.2.840.10008.5.1.4.1.1.4.1";

// =============================================================================
// MR SOP Class Utilities
// =============================================================================

/**
 * @brief Get all MR Storage SOP Class UIDs
 * @return Vector of MR Storage SOP Class UIDs
 */
[[nodiscard]] std::vector<std::string> get_mr_storage_sop_classes();

/**
 * @brief Check if a SOP Class UID is an MR Storage SOP Class
 * @param uid The SOP Class UID to check
 * @return true if this is an MR storage SOP class
 */
[[nodiscard]] bool is_mr_storage_sop_class(std::string_view uid) noexcept;

/**
 * @brief Check if photometric interpretation is valid for MR
 *
 * MR images are always grayscale (MONOCHROME1 or MONOCHROME2).
 *
 * @param value The photometric interpretation string
 * @return true if valid for MR images
 */
[[nodiscard]] bool is_valid_mr_photometric(std::string_view value) noexcept;

}  // namespace pacs::services::sop_classes

#endif  // PACS_SERVICES_SOP_CLASSES_MR_STORAGE_HPP
