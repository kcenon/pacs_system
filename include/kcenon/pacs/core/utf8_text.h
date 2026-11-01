// BSD 3-Clause License
// Copyright (c) 2021-2026, kcenon
// See the LICENSE file in the project root for full license information.

#pragma once

#include "kcenon/pacs/core/dicom_dataset.h"
#include "kcenon/pacs/core/dicom_tag.h"
#include "kcenon/pacs/encoding/character_set.h"
#include "kcenon/pacs/encoding/vr_type.h"

#include <algorithm>
#include <string>
#include <string_view>

namespace kcenon::pacs::core {

inline constexpr dicom_tag kSpecificCharacterSet{0x0008, 0x0005};
inline constexpr std::string_view kUtf8CharacterSet = "ISO_IR 192";

/// VRs whose bytes depend on Specific Character Set (PS3.5 Section 6.1.2.2).
[[nodiscard]] inline bool has_character_set_text(encoding::vr_type vr) noexcept {
    using encoding::vr_type;
    return vr == vr_type::SH || vr == vr_type::LO || vr == vr_type::ST || vr == vr_type::LT ||
           vr == vr_type::UC || vr == vr_type::UT || vr == vr_type::PN;
}

/**
 * @brief Copy a dataset with its text decoded to UTF-8, as PS3.18 and PS3.19 require.
 *
 * A sequence item inherits its parent's character set unless it declares its own.
 * Every declaration in the copy names ISO_IR 192, since that is now what the text is.
 */
[[nodiscard]] inline dicom_dataset utf8_text_copy(const dicom_dataset& source, const std::string& inherited = {}) {
    const std::string declared = source.contains(kSpecificCharacterSet)
                                     ? source.get_string(kSpecificCharacterSet)
                                     : inherited;
    const auto charset = encoding::parse_specific_character_set(declared);
    dicom_dataset result = source;
    for (const auto& [tag, element] : source) {
        if (element.is_sequence()) {
            auto& items = result.get(tag)->sequence_items();
            std::transform(element.sequence_items().begin(), element.sequence_items().end(),
                           items.begin(), [&declared](const dicom_dataset& item) {
                               return utf8_text_copy(item, declared);
                           });
        } else if (has_character_set_text(element.vr())) {
            if (auto text = element.as_string(); text.is_ok()) {
                result.set_string(tag, element.vr(), encoding::decode_to_utf8(text.value(), charset));
            }
        }
    }
    if (source.contains(kSpecificCharacterSet)) {
        result.set_string(kSpecificCharacterSet, encoding::vr_type::CS, kUtf8CharacterSet);
    }
    return result;
}

/// True when any text element, at any depth, holds a non-ASCII byte.
[[nodiscard]] inline bool has_non_ascii_text(const dicom_dataset& dataset) {
    for (const auto& [tag, element] : dataset) {
        if (element.is_sequence()) {
            for (const auto& item : element.sequence_items()) {
                if (has_non_ascii_text(item)) {
                    return true;
                }
            }
        } else if (has_character_set_text(element.vr())) {
            const auto bytes = element.raw_data();
            if (std::any_of(bytes.begin(), bytes.end(), [](auto byte) { return byte > 0x7F; })) {
                return true;
            }
        }
    }
    return false;
}

/**
 * @brief Make a dataset built from JSON or XML say that its text is UTF-8.
 *
 * Those formats carry UTF-8 whatever (0008,0005) they repeat from their source, so
 * keeping another declared character set would corrupt every non-ASCII value.
 */
inline void declare_utf8_text(dicom_dataset& dataset) {
    if (dataset.contains(kSpecificCharacterSet) || has_non_ascii_text(dataset)) {
        dataset.set_string(kSpecificCharacterSet, encoding::vr_type::CS, kUtf8CharacterSet);
    }
    for (auto& [tag, element] : dataset) {
        if (element.is_sequence()) {
            for (auto& item : element.sequence_items()) {
                if (item.contains(kSpecificCharacterSet)) {
                    item.set_string(kSpecificCharacterSet, encoding::vr_type::CS, kUtf8CharacterSet);
                }
            }
        }
    }
}

}  // namespace kcenon::pacs::core
