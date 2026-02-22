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
 * @file dataset_charset.cpp
 * @brief Implementation of character set-aware dataset string access
 */

#include "pacs/encoding/dataset_charset.hpp"

#include <pacs/core/dicom_tag_constants.hpp>

namespace pacs::encoding {

std::string get_decoded_string(
    const core::dicom_dataset& ds,
    core::dicom_tag tag,
    std::string_view default_value) {

    const auto* elem = ds.get(tag);
    if (elem == nullptr) {
        return std::string{default_value};
    }

    auto raw = elem->as_string();
    if (!raw.is_ok()) {
        return std::string{default_value};
    }

    // Look up Specific Character Set (0008,0005)
    auto scs_value = ds.get_string(core::tags::specific_character_set);
    auto scs = parse_specific_character_set(scs_value);

    return decode_to_utf8(raw.value(), scs);
}

void set_encoded_string(
    core::dicom_dataset& ds,
    core::dicom_tag tag,
    vr_type vr,
    std::string_view utf8_value) {

    // Look up Specific Character Set (0008,0005)
    auto scs_value = ds.get_string(core::tags::specific_character_set);
    auto scs = parse_specific_character_set(scs_value);

    auto encoded = encode_from_utf8(utf8_value, scs);
    ds.insert(core::dicom_element::from_string(tag, vr, encoded));
}

}  // namespace pacs::encoding
