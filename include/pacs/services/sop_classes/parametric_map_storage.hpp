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
 * @file parametric_map_storage.hpp
 * @brief Parametric Map Storage SOP Class
 *
 * Provides SOP Class definition and utilities for Parametric Map storage.
 * Parametric Maps encode voxel-level quantitative data (ADC maps, perfusion
 * maps, T1/T2 relaxometry maps) using float or double pixel values.
 *
 * @see DICOM PS3.3 Section A.75 - Parametric Map IOD
 * @see DICOM Supplement 172 - Parametric Map Storage
 * @see Issue #833 - Add Parametric Map Storage SOP Class registration
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_SERVICES_SOP_CLASSES_PARAMETRIC_MAP_STORAGE_HPP
#define PACS_SERVICES_SOP_CLASSES_PARAMETRIC_MAP_STORAGE_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace kcenon::pacs::services::sop_classes {

// =============================================================================
// Parametric Map Storage SOP Class UIDs
// =============================================================================

/// Parametric Map Storage SOP Class UID
inline constexpr std::string_view parametric_map_storage_uid =
    "1.2.840.10008.5.1.4.1.1.30";

// =============================================================================
// Parametric Map Transfer Syntaxes
// =============================================================================

/**
 * @brief Get recommended transfer syntaxes for Parametric Map objects
 *
 * Returns a prioritized list of transfer syntaxes suitable for parametric
 * map storage. Float pixel data requires uncompressed or lossless transfer
 * syntaxes to preserve quantitative accuracy.
 *
 * @return Vector of transfer syntax UIDs in priority order
 */
[[nodiscard]] std::vector<std::string> get_parametric_map_transfer_syntaxes();

// =============================================================================
// Pixel Value Representation
// =============================================================================

/**
 * @brief Pixel value representation for parametric map data
 *
 * Defines how the pixel data is encoded in the Parametric Map.
 * Per DICOM PS3.3 C.7.6.16.2.17, parametric maps use float or double.
 */
enum class pixel_value_representation {
    float32,    ///< 32-bit IEEE 754 floating point (BitsAllocated=32)
    float64     ///< 64-bit IEEE 754 floating point (BitsAllocated=64)
};

/**
 * @brief Get bits allocated for a pixel value representation
 * @param repr The pixel value representation
 * @return BitsAllocated value (32 or 64)
 */
[[nodiscard]] uint16_t get_bits_allocated(pixel_value_representation repr) noexcept;

/**
 * @brief Parse pixel value representation from BitsAllocated value
 * @param bits_allocated The BitsAllocated value
 * @return The pixel value representation, defaults to float32 for unknown values
 */
[[nodiscard]] pixel_value_representation
parse_pixel_value_representation(uint16_t bits_allocated) noexcept;

/**
 * @brief Check if a BitsAllocated value is valid for parametric maps
 * @param bits_allocated The BitsAllocated value to check
 * @return true if 32 or 64
 */
[[nodiscard]] bool is_valid_parametric_map_bits_allocated(uint16_t bits_allocated) noexcept;

// =============================================================================
// Parametric Map SOP Class Information
// =============================================================================

/**
 * @brief Information about the Parametric Map Storage SOP Class
 */
struct parametric_map_sop_class_info {
    std::string_view uid;           ///< SOP Class UID
    std::string_view name;          ///< Human-readable name
    std::string_view description;   ///< Brief description
    bool is_retired;                ///< Whether this SOP class is retired
};

/**
 * @brief Get all Parametric Map Storage SOP Class UIDs
 * @return Vector of Parametric Map Storage SOP Class UIDs
 */
[[nodiscard]] std::vector<std::string> get_parametric_map_storage_sop_classes();

/**
 * @brief Get information about the Parametric Map SOP Class
 * @param uid The SOP Class UID to look up
 * @return Pointer to SOP class info, or nullptr if not a Parametric Map class
 */
[[nodiscard]] const parametric_map_sop_class_info*
get_parametric_map_sop_class_info(std::string_view uid) noexcept;

/**
 * @brief Check if a SOP Class UID is Parametric Map Storage
 * @param uid The SOP Class UID to check
 * @return true if this is the Parametric Map Storage SOP class
 */
[[nodiscard]] bool is_parametric_map_storage_sop_class(std::string_view uid) noexcept;

// =============================================================================
// Parametric Map DICOM Tags
// =============================================================================

namespace parametric_map_tags {

/// Real World Value Mapping Sequence (0040,9096)
inline constexpr uint32_t real_world_value_mapping_sequence = 0x00409096;

/// Real World Value Intercept (0040,9224)
inline constexpr uint32_t real_world_value_intercept = 0x00409224;

/// Real World Value Slope (0040,9225)
inline constexpr uint32_t real_world_value_slope = 0x00409225;

/// Real World Value First Value Mapped (0040,9216)
inline constexpr uint32_t real_world_value_first_value_mapped = 0x00409216;

/// Real World Value Last Value Mapped (0040,9211)
inline constexpr uint32_t real_world_value_last_value_mapped = 0x00409211;

/// Measurement Units Code Sequence (0040,08EA)
inline constexpr uint32_t measurement_units_code_sequence = 0x004008EA;

/// Content Label (0070,0080)
inline constexpr uint32_t content_label = 0x00700080;

/// Content Description (0070,0081)
inline constexpr uint32_t content_description = 0x00700081;

/// Content Creator Name (0070,0084)
inline constexpr uint32_t content_creator_name = 0x00700084;

}  // namespace parametric_map_tags

// =============================================================================
// Photometric Interpretation Validation
// =============================================================================

/**
 * @brief Check if photometric interpretation is valid for parametric maps
 *
 * Parametric Maps use MONOCHROME2 for single-component float/double data.
 *
 * @param value The photometric interpretation string
 * @return true if valid for parametric maps
 */
[[nodiscard]] bool is_valid_parametric_map_photometric(std::string_view value) noexcept;

}  // namespace kcenon::pacs::services::sop_classes

#endif  // PACS_SERVICES_SOP_CLASSES_PARAMETRIC_MAP_STORAGE_HPP
