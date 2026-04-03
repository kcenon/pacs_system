// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file dataset_charset.h
 * @brief Character set-aware string access for DICOM datasets
 *
 * Provides free functions that combine dicom_dataset string access with
 * character set decoding/encoding based on Specific Character Set (0008,0005).
 *
 * These are free functions in kcenon::pacs::encoding rather than dicom_dataset member
 * functions to avoid circular dependency between pacs_core and pacs_encoding.
 *
 * @see DICOM PS3.5 Section 6.1 - Support of Character Repertoires
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_ENCODING_DATASET_CHARSET_HPP
#define PACS_ENCODING_DATASET_CHARSET_HPP

#include <kcenon/pacs/core/dicom_dataset.h>
#include <kcenon/pacs/core/dicom_tag.h>
#include <kcenon/pacs/encoding/character_set.h>
#include <kcenon/pacs/encoding/vr_type.h>

#include <string>
#include <string_view>

namespace kcenon::pacs::encoding {

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

}  // namespace kcenon::pacs::encoding

#endif  // PACS_ENCODING_DATASET_CHARSET_HPP
