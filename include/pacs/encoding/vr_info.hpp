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
 * @file vr_info.hpp
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_ENCODING_VR_INFO_HPP
#define PACS_ENCODING_VR_INFO_HPP

#include "vr_type.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pacs::encoding {

/**
 * @brief Metadata structure containing comprehensive VR properties.
 *
 * This structure provides all the information needed for encoding, decoding,
 * and validating DICOM data element values based on their VR type.
 *
 * @see DICOM PS3.5 Section 6.2 - Value Representation (VR)
 */
struct vr_info {
    vr_type type;                   ///< The VR enumeration value
    std::string_view name;          ///< Human-readable name (e.g., "Person Name")
    uint32_t max_length;            ///< Maximum value length in bytes
    char padding_char;              ///< Padding character (' ' or '\0')
    bool is_fixed_length;           ///< Whether VR has fixed length
    std::size_t fixed_size;         ///< Size for fixed-length VRs (0 if variable)
};

/// @name VR Information Lookup
/// @{

/**
 * @brief Retrieves comprehensive metadata for a VR type.
 * @param vr The VR type to look up
 * @return Reference to vr_info structure containing VR properties
 *
 * This function provides all properties needed for encoding/decoding
 * operations, including name, maximum length, padding requirements,
 * and fixed/variable length information.
 *
 * @note Returns info for vr_type::UN if an unknown VR is provided.
 */
[[nodiscard]] const vr_info& get_vr_info(vr_type vr);

/// @}

/// @name Value Validation Functions
/// @{

/**
 * @brief Validates binary data against VR encoding rules.
 * @param vr The VR type for validation
 * @param data The binary data to validate
 * @return true if the data conforms to VR requirements
 *
 * Performs VR-specific validation including:
 * - Length constraints
 * - Character set restrictions
 * - Format requirements (for structured VRs like DA, TM)
 */
[[nodiscard]] bool validate_value(vr_type vr, std::span<const uint8_t> data);

/**
 * @brief Validates a string value against VR encoding rules.
 * @param vr The VR type for validation
 * @param value The string value to validate
 * @return true if the string conforms to VR requirements
 *
 * Validates string VRs for:
 * - Maximum length
 * - Allowed character sets
 * - Format patterns (dates, times, UIDs, etc.)
 */
[[nodiscard]] bool validate_string(vr_type vr, std::string_view value);

/// @}

/// @name Padding Utilities
/// @{

/**
 * @brief Pads data to even length as required by DICOM.
 * @param vr The VR type (determines padding character)
 * @param data The data to pad
 * @return A new vector with even-length data
 *
 * DICOM requires all data element values to have even length.
 * This function adds the appropriate padding character if needed:
 * - Space (' ') for most string VRs
 * - Null ('\0') for UI and binary VRs
 *
 * @see DICOM PS3.5 Section 7.1.1 - DICOM Data Element Structure
 */
[[nodiscard]] std::vector<uint8_t> pad_to_even(vr_type vr,
                                                std::span<const uint8_t> data);

/**
 * @brief Removes trailing padding characters from a string value.
 * @param vr The VR type (determines padding character to remove)
 * @param value The value to trim
 * @return A string with trailing padding removed
 *
 * Removes:
 * - Trailing spaces for most string VRs
 * - Trailing nulls for UI VR
 */
[[nodiscard]] std::string trim_padding(vr_type vr, std::string_view value);

/// @}

/// @name Character Set Validation
/// @{

/**
 * @brief Validates that a string uses only allowed characters for its VR.
 * @param vr The VR type
 * @param value The string to validate
 * @return true if all characters are valid for the VR
 *
 * Character restrictions by VR type:
 * - CS: A-Z, 0-9, space, underscore
 * - DA: 0-9 only (YYYYMMDD format)
 * - TM: 0-9, period, colon
 * - UI: 0-9, period
 * - DS: 0-9, +, -, ., E, e, space
 * - IS: 0-9, +, -
 * - AS: 0-9, D, W, M, Y
 * - Other string VRs: All printable characters
 */
[[nodiscard]] bool is_valid_charset(vr_type vr, std::string_view value);

/// @}

}  // namespace pacs::encoding

#endif  // PACS_ENCODING_VR_INFO_HPP
