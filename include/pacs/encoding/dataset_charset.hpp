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
 * @file dataset_charset.hpp
 * @brief Character set-aware string access for DICOM datasets
 *
 * Provides free functions that combine dicom_dataset string access with
 * character set decoding/encoding based on Specific Character Set (0008,0005).
 *
 * These are free functions in pacs::encoding rather than dicom_dataset member
 * functions to avoid circular dependency between pacs_core and pacs_encoding.
 *
 * @see DICOM PS3.5 Section 6.1 - Support of Character Repertoires
 */

#ifndef PACS_ENCODING_DATASET_CHARSET_HPP
#define PACS_ENCODING_DATASET_CHARSET_HPP

#include <pacs/core/dicom_dataset.hpp>
#include <pacs/core/dicom_tag.hpp>
#include <pacs/encoding/character_set.hpp>
#include <pacs/encoding/vr_type.hpp>

#include <string>
#include <string_view>

namespace pacs::encoding {

/**
 * @brief Get a string value from dataset, decoded to UTF-8.
 * @param ds The DICOM dataset to read from
 * @param tag The DICOM tag to look up
 * @param default_value Value to return if element not found
 * @return UTF-8 decoded string value, or default_value if not found
 *
 * Looks up Specific Character Set (0008,0005) in the dataset and uses it
 * to decode the raw string bytes to UTF-8. If no Specific Character Set is
 * present, ASCII decoding is assumed (raw passthrough).
 */
[[nodiscard]] std::string get_decoded_string(
    const core::dicom_dataset& ds,
    core::dicom_tag tag,
    std::string_view default_value = "");

/**
 * @brief Set a string value in dataset, encoding from UTF-8.
 * @param ds The DICOM dataset to write to
 * @param tag The DICOM tag
 * @param vr The value representation
 * @param utf8_value The UTF-8 encoded string value
 *
 * Encodes the UTF-8 input to the encoding specified by Specific Character Set
 * (0008,0005) in the dataset, then stores the result. If no Specific Character
 * Set is present, the value is stored as-is (ASCII assumption).
 */
void set_encoded_string(
    core::dicom_dataset& ds,
    core::dicom_tag tag,
    vr_type vr,
    std::string_view utf8_value);

}  // namespace pacs::encoding

#endif  // PACS_ENCODING_DATASET_CHARSET_HPP
