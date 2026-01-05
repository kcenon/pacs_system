/**
 * @file explicit_vr_big_endian_codec.cpp
 * @brief Implementation of Explicit VR Big Endian encoder/decoder
 */

#include "pacs/encoding/explicit_vr_big_endian_codec.hpp"

#include <pacs/encoding/byte_swap.hpp>
#include <pacs/encoding/vr_info.hpp>
#include <pacs/encoding/vr_type.hpp>

#include <cstring>

namespace pacs::encoding {

namespace {

// ============================================================================
// DICOM Special Tags
// ============================================================================

constexpr uint16_t ITEM_GROUP = 0xFFFE;
constexpr uint16_t ITEM_TAG_ELEMENT = 0xE000;       // Item
constexpr uint16_t ITEM_DELIM_ELEMENT = 0xE00D;     // Item Delimitation Item
constexpr uint16_t SEQ_DELIM_ELEMENT = 0xE0DD;      // Sequence Delimitation Item

constexpr uint32_t UNDEFINED_LENGTH = 0xFFFFFFFF;

constexpr bool is_sequence_delimiter(core::dicom_tag tag) {
    return tag.group() == ITEM_GROUP && tag.element() == SEQ_DELIM_ELEMENT;
}

constexpr bool is_item_delimiter(core::dicom_tag tag) {
    return tag.group() == ITEM_GROUP && tag.element() == ITEM_DELIM_ELEMENT;
}

constexpr bool is_item_tag(core::dicom_tag tag) {
    return tag.group() == ITEM_GROUP && tag.element() == ITEM_TAG_ELEMENT;
}

constexpr bool is_special_tag(core::dicom_tag tag) {
    return tag.group() == ITEM_GROUP;
}

// ============================================================================
// VR Classification for Byte Swapping
// ============================================================================

/**
 * @brief Determines if a VR requires byte swapping for big-endian encoding.
 * @param vr The VR type
 * @return true if byte swapping is needed
 */
constexpr bool requires_byte_swap(vr_type vr) {
    switch (vr) {
        // 16-bit numeric
        case vr_type::US:
        case vr_type::SS:
        // 32-bit numeric
        case vr_type::UL:
        case vr_type::SL:
        case vr_type::FL:
        // 64-bit numeric
        case vr_type::FD:
        // Attribute tag (2x 16-bit)
        case vr_type::AT:
        // Bulk data with word/long ordering
        case vr_type::OW:
        case vr_type::OL:
        case vr_type::OF:
        case vr_type::OD:
            return true;

        default:
            // String VRs and OB don't need byte swapping
            return false;
    }
}

/**
 * @brief Gets the swap unit size for a VR type.
 * @param vr The VR type
 * @return Number of bytes per swap unit (2, 4, or 8)
 */
constexpr size_t swap_unit_size(vr_type vr) {
    switch (vr) {
        case vr_type::US:
        case vr_type::SS:
        case vr_type::OW:
        case vr_type::AT:
            return 2;

        case vr_type::UL:
        case vr_type::SL:
        case vr_type::FL:
        case vr_type::OL:
        case vr_type::OF:
            return 4;

        case vr_type::FD:
        case vr_type::OD:
            return 8;

        default:
            return 1;  // No swapping
    }
}

}  // namespace

// ============================================================================
// Error Helpers
// ============================================================================

namespace {

template <typename T>
pacs::Result<T> make_codec_error(int code, const std::string& message) {
    return pacs::pacs_error<T>(code, message);
}

}  // namespace

// ============================================================================
// Byte Order Conversion
// ============================================================================

std::vector<uint8_t> explicit_vr_big_endian_codec::to_big_endian(
    vr_type vr, std::span<const uint8_t> data) {
    if (!requires_byte_swap(vr) || data.empty()) {
        return {data.begin(), data.end()};
    }

    switch (swap_unit_size(vr)) {
        case 2:
            return swap_ow_bytes(data);
        case 4:
            return swap_ol_bytes(data);
        case 8:
            return swap_od_bytes(data);
        default:
            return {data.begin(), data.end()};
    }
}

std::vector<uint8_t> explicit_vr_big_endian_codec::from_big_endian(
    vr_type vr, std::span<const uint8_t> data) {
    // Byte swapping is symmetric - same operation in both directions
    return to_big_endian(vr, data);
}

// ============================================================================
// Dataset Encoding
// ============================================================================

std::vector<uint8_t> explicit_vr_big_endian_codec::encode(
    const core::dicom_dataset& dataset) {
    std::vector<uint8_t> buffer;
    buffer.reserve(4096);  // Initial capacity

    for (const auto& [tag, element] : dataset) {
        auto encoded = encode_element(element);
        buffer.insert(buffer.end(), encoded.begin(), encoded.end());
    }

    return buffer;
}

std::vector<uint8_t> explicit_vr_big_endian_codec::encode_element(
    const core::dicom_element& element) {
    std::vector<uint8_t> buffer;

    // Write tag (4 bytes: group + element, big-endian)
    write_be16(buffer, element.tag().group());
    write_be16(buffer, element.tag().element());

    // Write VR (2 ASCII characters)
    auto vr_str = to_string(element.vr());
    buffer.push_back(static_cast<uint8_t>(vr_str[0]));
    buffer.push_back(static_cast<uint8_t>(vr_str[1]));

    // Handle sequences specially
    if (element.is_sequence()) {
        encode_sequence(buffer, element);
        return buffer;
    }

    // Get padded data and convert to big-endian
    auto data = element.raw_data();
    auto padded_data = pad_to_even(element.vr(), data);
    auto be_data = to_big_endian(element.vr(), padded_data);
    uint32_t length = static_cast<uint32_t>(be_data.size());

    // Write length based on VR type
    if (has_explicit_32bit_length(element.vr())) {
        // Extended format: 2 reserved bytes + 4 byte length
        write_be16(buffer, 0x0000);  // Reserved
        write_be32(buffer, length);
    } else {
        // Standard format: 2 byte length
        write_be16(buffer, static_cast<uint16_t>(length));
    }

    // Write value (already byte-swapped)
    buffer.insert(buffer.end(), be_data.begin(), be_data.end());

    return buffer;
}

void explicit_vr_big_endian_codec::encode_sequence(
    std::vector<uint8_t>& buffer,
    const core::dicom_element& element) {
    // Write reserved bytes for SQ VR (uses 32-bit length)
    write_be16(buffer, 0x0000);

    // Write undefined length for sequence (big-endian)
    write_be32(buffer, UNDEFINED_LENGTH);

    // Encode each sequence item
    const auto& items = element.sequence_items();
    for (const auto& item : items) {
        encode_sequence_item(buffer, item);
    }

    // Write sequence delimitation item (big-endian for tag and length)
    write_be16(buffer, ITEM_GROUP);
    write_be16(buffer, SEQ_DELIM_ELEMENT);
    write_be32(buffer, 0);  // Length is always 0 for delimiter
}

void explicit_vr_big_endian_codec::encode_sequence_item(
    std::vector<uint8_t>& buffer,
    const core::dicom_dataset& item) {
    // Write Item tag (big-endian)
    write_be16(buffer, ITEM_GROUP);
    write_be16(buffer, ITEM_TAG_ELEMENT);

    // Encode item content first to determine length
    auto item_content = encode(item);

    // Write item length (big-endian)
    write_be32(buffer, static_cast<uint32_t>(item_content.size()));

    // Write item content
    buffer.insert(buffer.end(), item_content.begin(), item_content.end());
}

// ============================================================================
// Dataset Decoding
// ============================================================================

explicit_vr_big_endian_codec::result<core::dicom_dataset>
explicit_vr_big_endian_codec::decode(std::span<const uint8_t> data) {
    core::dicom_dataset dataset;

    while (!data.empty()) {
        // Peek at tag to check for sequence delimiters
        if (data.size() >= 4) {
            uint16_t group = read_be16(data.data());
            uint16_t elem = read_be16(data.data() + 2);
            core::dicom_tag tag{group, elem};

            // Stop at sequence delimiter
            if (is_sequence_delimiter(tag)) {
                break;
            }
            // Stop at item delimiter
            if (is_item_delimiter(tag)) {
                break;
            }
        }

        auto result = decode_element(data);
        if (!result.is_ok()) {
            return pacs::pacs_error<core::dicom_dataset>(
                result.error().code,
                result.error().message);
        }

        dataset.insert(std::move(result.value()));
    }

    return dataset;
}

explicit_vr_big_endian_codec::result<core::dicom_element>
explicit_vr_big_endian_codec::decode_element(std::span<const uint8_t>& data) {
    // Need at least 8 bytes for standard format: tag (4) + VR (2) + length (2)
    if (data.size() < 8) {
        return make_codec_error<core::dicom_element>(
            pacs::error_codes::insufficient_data,
            "Insufficient data to decode element");
    }

    // Read tag (big-endian)
    uint16_t group = read_be16(data.data());
    uint16_t elem = read_be16(data.data() + 2);
    core::dicom_tag tag{group, elem};

    // Special handling for sequence delimiter/item tags
    if (is_special_tag(tag)) {
        data = data.subspan(8);

        // For delimiter tags, return a placeholder element
        return core::dicom_element(tag, vr_type::UN);
    }

    // Read VR (2 ASCII characters)
    char vr_chars[2];
    vr_chars[0] = static_cast<char>(data[4]);
    vr_chars[1] = static_cast<char>(data[5]);
    std::string_view vr_str(vr_chars, 2);

    auto vr_opt = from_string(vr_str);
    if (!vr_opt) {
        return make_codec_error<core::dicom_element>(
            pacs::error_codes::unknown_vr,
            "Unknown VR type");
    }
    vr_type vr = *vr_opt;

    uint32_t length;
    size_t header_size;

    // Determine length format based on VR
    if (has_explicit_32bit_length(vr)) {
        // Extended format: need 12 bytes total
        if (data.size() < 12) {
            return make_codec_error<core::dicom_element>(
                pacs::error_codes::insufficient_data,
                "Insufficient data for extended VR format");
        }
        // Skip reserved 2 bytes, then read 4-byte length (big-endian)
        length = read_be32(data.data() + 8);
        header_size = 12;
    } else {
        // Standard format: 2-byte length (big-endian)
        length = read_be16(data.data() + 6);
        header_size = 8;
    }

    data = data.subspan(header_size);

    // Handle undefined length (sequences and encapsulated data)
    if (length == UNDEFINED_LENGTH) {
        return decode_undefined_length(tag, vr, data);
    }

    // Check if we have enough data
    if (data.size() < length) {
        return make_codec_error<core::dicom_element>(
            pacs::error_codes::insufficient_data,
            "Insufficient data for element value");
    }

    // Read value and convert from big-endian
    auto value_data = data.subspan(0, length);
    data = data.subspan(length);

    // Convert from big-endian to native (little-endian)
    auto le_data = from_big_endian(vr, value_data);

    return core::dicom_element(tag, vr, le_data);
}

explicit_vr_big_endian_codec::result<core::dicom_element>
explicit_vr_big_endian_codec::decode_undefined_length(
    core::dicom_tag tag, vr_type vr,
    std::span<const uint8_t>& data) {
    // If this is a sequence (SQ), decode sequence items
    if (vr == vr_type::SQ) {
        core::dicom_element seq_element(tag, vr_type::SQ);

        while (!data.empty()) {
            // Check for sequence delimitation
            if (data.size() < 8) {
                return make_codec_error<core::dicom_element>(
                    pacs::error_codes::insufficient_data,
                    "Insufficient data for sequence delimiter");
            }

            uint16_t item_group = read_be16(data.data());
            uint16_t item_elem = read_be16(data.data() + 2);
            core::dicom_tag item_tag{item_group, item_elem};

            // Check for sequence delimitation item
            if (is_sequence_delimiter(item_tag)) {
                // Skip the delimiter (8 bytes: tag + length)
                data = data.subspan(8);
                break;
            }

            // Must be an Item tag
            if (!is_item_tag(item_tag)) {
                return make_codec_error<core::dicom_element>(
                    pacs::error_codes::invalid_sequence,
                    "Expected Item tag in sequence");
            }

            // Decode the sequence item
            auto item_result = decode_sequence_item(data);
            if (!item_result.is_ok()) {
                return make_codec_error<core::dicom_element>(
                    item_result.error().code,
                    item_result.error().message);
            }

            seq_element.sequence_items().push_back(std::move(item_result.value()));
        }

        return seq_element;
    }

    // For other undefined-length elements (like encapsulated pixel data),
    // read until we find a sequence delimitation item
    std::vector<uint8_t> accumulated_data;

    while (!data.empty()) {
        if (data.size() < 8) {
            return make_codec_error<core::dicom_element>(
                pacs::error_codes::insufficient_data,
                "Insufficient data for encapsulated data");
        }

        uint16_t item_group = read_be16(data.data());
        uint16_t item_elem = read_be16(data.data() + 2);
        core::dicom_tag item_tag{item_group, item_elem};

        // Check for sequence delimitation item
        if (is_sequence_delimiter(item_tag)) {
            data = data.subspan(8);
            break;
        }

        // For encapsulated data, read item tag + length + data
        if (is_item_tag(item_tag)) {
            uint32_t item_length = read_be32(data.data() + 4);
            data = data.subspan(8);

            if (item_length != UNDEFINED_LENGTH && data.size() >= item_length) {
                // Convert from big-endian
                auto item_data = data.subspan(0, item_length);
                auto le_item = from_big_endian(vr, item_data);
                accumulated_data.insert(accumulated_data.end(),
                                        le_item.begin(), le_item.end());
                data = data.subspan(item_length);
            }
        } else {
            return make_codec_error<core::dicom_element>(
                pacs::error_codes::invalid_sequence,
                "Invalid tag in encapsulated data");
        }
    }

    return core::dicom_element(tag, vr, accumulated_data);
}

explicit_vr_big_endian_codec::result<core::dicom_dataset>
explicit_vr_big_endian_codec::decode_sequence_item(
    std::span<const uint8_t>& data) {
    // Read Item tag and length (big-endian)
    if (data.size() < 8) {
        return make_codec_error<core::dicom_dataset>(
            pacs::error_codes::insufficient_data,
            "Insufficient data for sequence item");
    }

    uint16_t group = read_be16(data.data());
    uint16_t elem = read_be16(data.data() + 2);
    core::dicom_tag tag{group, elem};

    if (!is_item_tag(tag)) {
        return make_codec_error<core::dicom_dataset>(
            pacs::error_codes::invalid_sequence,
            "Expected Item tag for sequence item");
    }

    uint32_t item_length = read_be32(data.data() + 4);
    data = data.subspan(8);

    if (item_length == UNDEFINED_LENGTH) {
        // Decode until Item Delimitation Item
        core::dicom_dataset item;

        while (!data.empty()) {
            if (data.size() < 4) {
                return make_codec_error<core::dicom_dataset>(
                    pacs::error_codes::insufficient_data,
                    "Insufficient data for item delimiter check");
            }

            uint16_t elem_group = read_be16(data.data());
            uint16_t elem_elem = read_be16(data.data() + 2);
            core::dicom_tag elem_tag{elem_group, elem_elem};

            if (is_item_delimiter(elem_tag)) {
                // Skip delimiter
                data = data.subspan(8);
                break;
            }

            auto elem_result = decode_element(data);
            if (!elem_result.is_ok()) {
                return make_codec_error<core::dicom_dataset>(
                    elem_result.error().code,
                    elem_result.error().message);
            }

            item.insert(std::move(elem_result.value()));
        }

        return item;
    }

    // Explicit item length
    if (data.size() < item_length) {
        return make_codec_error<core::dicom_dataset>(
            pacs::error_codes::insufficient_data,
            "Insufficient data for item content");
    }

    auto item_data = data.subspan(0, item_length);
    data = data.subspan(item_length);

    return decode(item_data);
}

}  // namespace pacs::encoding
