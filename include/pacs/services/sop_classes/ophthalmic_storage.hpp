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
 * @file ophthalmic_storage.hpp
 * @brief Ophthalmic Photography and Tomography Storage SOP Classes
 *
 * Provides SOP Class definitions and utilities for Ophthalmic Photography
 * (fundus cameras) and Optical Coherence Tomography (OCT) image storage.
 *
 * @see DICOM PS3.3 Section A.39 - Ophthalmic Photography IODs
 * @see DICOM PS3.3 Section A.40 - Ophthalmic Tomography Image IOD
 * @see Issue #829 - Add Ophthalmic SOP Class registration and storage definition
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_SERVICES_SOP_CLASSES_OPHTHALMIC_STORAGE_HPP
#define PACS_SERVICES_SOP_CLASSES_OPHTHALMIC_STORAGE_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace kcenon::pacs::services::sop_classes {

// =============================================================================
// Ophthalmic Storage SOP Class UIDs
// =============================================================================

/// Ophthalmic Photography 8 Bit Image Storage
inline constexpr std::string_view ophthalmic_photo_8bit_storage_uid =
    "1.2.840.10008.5.1.4.1.1.77.1.5.1";

/// Ophthalmic Photography 16 Bit Image Storage
inline constexpr std::string_view ophthalmic_photo_16bit_storage_uid =
    "1.2.840.10008.5.1.4.1.1.77.1.5.2";

/// Ophthalmic Tomography Image Storage (OCT)
inline constexpr std::string_view ophthalmic_tomography_storage_uid =
    "1.2.840.10008.5.1.4.1.1.77.1.5.4";

/// Wide Field Ophthalmic Photography SOP Class Storage
inline constexpr std::string_view wide_field_ophthalmic_photo_storage_uid =
    "1.2.840.10008.5.1.4.1.1.77.1.5.7";

/// Ophthalmic Optical Coherence Tomography B-scan Volume Analysis Storage
inline constexpr std::string_view ophthalmic_oct_bscan_analysis_storage_uid =
    "1.2.840.10008.5.1.4.1.1.77.1.5.8";

// =============================================================================
// Ophthalmic DICOM Tags
// =============================================================================

namespace ophthalmic_tags {

/// Image Laterality (0020,0062) — R, L, or B
inline constexpr uint32_t image_laterality = 0x00200062;

/// Anatomic Region Sequence (0008,2218)
inline constexpr uint32_t anatomic_region_sequence = 0x00082218;

/// Acquisition Device Type Code Sequence (0022,0015)
inline constexpr uint32_t acquisition_device_type_code_sequence = 0x00220015;

/// Detector Type (0018,7004)
inline constexpr uint32_t detector_type = 0x00187004;

/// Pupil Dilated (0022,000D)
inline constexpr uint32_t pupil_dilated = 0x0022000D;

/// Depth of Field (0022,0035)
inline constexpr uint32_t depth_of_field = 0x00220035;

/// Axial Length of the Eye (0022,0030)
inline constexpr uint32_t axial_length_of_eye = 0x00220030;

/// Emmetropic Magnification (0022,000A)
inline constexpr uint32_t emmetropic_magnification = 0x0022000A;

/// Intra Ocular Pressure (0022,000B)
inline constexpr uint32_t intra_ocular_pressure = 0x0022000B;

/// Horizontal Field of View (0022,000C)
inline constexpr uint32_t horizontal_field_of_view = 0x0022000C;

}  // namespace ophthalmic_tags

// =============================================================================
// Ophthalmic SOP Class Utilities
// =============================================================================

/**
 * @brief Get all Ophthalmic Storage SOP Class UIDs
 * @return Vector of Ophthalmic Storage SOP Class UIDs
 */
[[nodiscard]] std::vector<std::string> get_ophthalmic_storage_sop_classes();

/**
 * @brief Check if a SOP Class UID is an Ophthalmic Storage SOP Class
 * @param uid The SOP Class UID to check
 * @return true if this is an ophthalmic storage SOP class
 */
[[nodiscard]] bool is_ophthalmic_storage_sop_class(std::string_view uid) noexcept;

/**
 * @brief Check if photometric interpretation is valid for ophthalmic imaging
 *
 * Ophthalmic Photography supports RGB (color fundus) and MONOCHROME2
 * (fluorescein angiography, ICG). OCT supports MONOCHROME2.
 * MONOCHROME1 is also valid for certain ophthalmic applications.
 *
 * @param value The photometric interpretation string
 * @return true if valid for ophthalmic images
 */
[[nodiscard]] bool is_valid_ophthalmic_photometric(std::string_view value) noexcept;

}  // namespace kcenon::pacs::services::sop_classes

#endif  // PACS_SERVICES_SOP_CLASSES_OPHTHALMIC_STORAGE_HPP
