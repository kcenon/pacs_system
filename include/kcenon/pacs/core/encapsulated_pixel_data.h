// BSD 3-Clause License
// Copyright (c) 2021-2026, kcenon
// See the LICENSE file in the project root for full license information.

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace kcenon::pacs::core {

struct encapsulated_pixel_data_layout {
    /// Bytes occupied by the Basic Offset Table and fragments, excluding delimiter.
    std::size_t items_size;
    /// Total bytes consumed, including the delimiter when present.
    std::size_t encoded_size;
};

/**
 * Inspect an encapsulated Pixel Data item stream without flattening its items.
 * A wire value requires a delimiter; an in-memory value may omit it. Trailing
 * bytes are allowed only when decoding one value from a larger dataset.
 * Frame offset interpretation remains the responsibility of encapsulated_frames.
 * @see https://dicom.nema.org/Medical/dicom/current/output/chtml/part05/sect_A.4.html
 */
[[nodiscard]] inline auto inspect_encapsulated_pixel_data(
    std::span<const uint8_t> value, bool require_delimiter = false,
    bool allow_trailing = false) -> std::optional<encapsulated_pixel_data_layout> {
    const auto le32 = [](const uint8_t* p) {
        return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8)
            | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
    };
    std::size_t offset = 0;
    std::size_t items = 0;
    while (value.size()-offset >= 8) {
        const auto tag = le32(value.data()+offset);
        const auto length = le32(value.data()+offset+4);
        if (tag == 0xE0DDFFFE) {
            if (length != 0 || items < 2 || (!allow_trailing && offset+8 != value.size())) {
                return std::nullopt;
            }
            return encapsulated_pixel_data_layout{offset,offset+8};
        }
        if (tag != 0xE000FFFE || length == 0xFFFFFFFF
            || length > value.size()-offset-8 || length%2 != 0
            || (items == 0 ? length%4 != 0 : length < 2)) {
            return std::nullopt;
        }
        offset += 8+static_cast<std::size_t>(length);
        ++items;
    }
    if (!require_delimiter && items >= 2 && offset == value.size()) {
        return encapsulated_pixel_data_layout{offset,offset};
    }
    return std::nullopt;
}

}  // namespace kcenon::pacs::core
