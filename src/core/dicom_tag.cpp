/**
 * @file dicom_tag.cpp
 * @brief Implementation of dicom_tag class methods
 */

#include "pacs/core/dicom_tag.hpp"

#include <array>
#include <charconv>
#include <iomanip>
#include <sstream>

namespace pacs::core {

namespace {

/**
 * @brief Convert a hexadecimal character to its numeric value
 * @param ch The hexadecimal character ('0'-'9', 'A'-'F', 'a'-'f')
 * @return The numeric value (0-15), or -1 if invalid
 */
constexpr auto hex_char_to_int(char ch) noexcept -> int {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    return -1;
}

/**
 * @brief Parse 4 hexadecimal characters into a uint16_t
 * @param str Pointer to the start of the hex string (must have at least 4 chars)
 * @return Optional containing the parsed value, or nullopt if invalid
 */
auto parse_hex4(const char* str) noexcept -> std::optional<uint16_t> {
    uint16_t result = 0;
    for (int i = 0; i < 4; ++i) {
        const int digit = hex_char_to_int(str[i]);
        if (digit < 0) {
            return std::nullopt;
        }
        result = static_cast<uint16_t>((result << 4) | digit);
    }
    return result;
}

}  // namespace

auto dicom_tag::from_string(std::string_view str) -> std::optional<dicom_tag> {
    // Remove leading/trailing whitespace
    while (!str.empty() && (str.front() == ' ' || str.front() == '\t')) {
        str.remove_prefix(1);
    }
    while (!str.empty() && (str.back() == ' ' || str.back() == '\t')) {
        str.remove_suffix(1);
    }

    if (str.empty()) {
        return std::nullopt;
    }

    // Format 1: "(GGGG,EEEE)"
    if (str.size() == 11 && str.front() == '(' && str.back() == ')' &&
        str[5] == ',') {
        const auto group = parse_hex4(str.data() + 1);
        const auto element = parse_hex4(str.data() + 6);

        if (group && element) {
            return dicom_tag{*group, *element};
        }
        return std::nullopt;
    }

    // Format 2: "GGGGEEEE" (8 hex digits)
    if (str.size() == 8) {
        const auto group = parse_hex4(str.data());
        const auto element = parse_hex4(str.data() + 4);

        if (group && element) {
            return dicom_tag{*group, *element};
        }
        return std::nullopt;
    }

    // Format 3: "GGGG,EEEE" (without parentheses)
    if (str.size() == 9 && str[4] == ',') {
        const auto group = parse_hex4(str.data());
        const auto element = parse_hex4(str.data() + 5);

        if (group && element) {
            return dicom_tag{*group, *element};
        }
        return std::nullopt;
    }

    return std::nullopt;
}

auto dicom_tag::to_string() const -> std::string {
    // Pre-allocate exact size: "(" + 4 hex + "," + 4 hex + ")" = 11 chars
    std::string result;
    result.reserve(11);

    // Use lookup table for hex conversion (faster than std::format)
    static constexpr std::array<char, 16> hex_chars = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

    const auto grp = group();
    const auto elem = element();

    result += '(';
    result += hex_chars[(grp >> 12) & 0xF];
    result += hex_chars[(grp >> 8) & 0xF];
    result += hex_chars[(grp >> 4) & 0xF];
    result += hex_chars[grp & 0xF];
    result += ',';
    result += hex_chars[(elem >> 12) & 0xF];
    result += hex_chars[(elem >> 8) & 0xF];
    result += hex_chars[(elem >> 4) & 0xF];
    result += hex_chars[elem & 0xF];
    result += ')';

    return result;
}

}  // namespace pacs::core
