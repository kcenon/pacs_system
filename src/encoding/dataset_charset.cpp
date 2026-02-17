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
