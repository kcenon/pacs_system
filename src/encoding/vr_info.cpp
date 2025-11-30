#include "pacs/encoding/vr_info.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <unordered_map>

namespace pacs::encoding {

namespace {

// Static VR properties table
// DICOM PS3.5 Table 6.2-1 defines these properties
constexpr std::array<vr_info, 31> vr_table = {{
    // String VRs
    {vr_type::AE, "Application Entity",    16,       ' ',  false, 0},
    {vr_type::AS, "Age String",            4,        ' ',  true,  4},
    {vr_type::CS, "Code String",           16,       ' ',  false, 0},
    {vr_type::DA, "Date",                  8,        ' ',  true,  8},
    {vr_type::DS, "Decimal String",        16,       ' ',  false, 0},
    {vr_type::DT, "Date Time",             26,       ' ',  false, 0},
    {vr_type::IS, "Integer String",        12,       ' ',  false, 0},
    {vr_type::LO, "Long String",           64,       ' ',  false, 0},
    {vr_type::LT, "Long Text",             10240,    ' ',  false, 0},
    {vr_type::PN, "Person Name",           324,      ' ',  false, 0},
    {vr_type::SH, "Short String",          16,       ' ',  false, 0},
    {vr_type::ST, "Short Text",            1024,     ' ',  false, 0},
    {vr_type::TM, "Time",                  14,       ' ',  false, 0},
    {vr_type::UC, "Unlimited Characters",  0xFFFFFFFE, ' ', false, 0},
    {vr_type::UI, "Unique Identifier",     64,       '\0', false, 0},
    {vr_type::UR, "Universal Resource Identifier", 0xFFFFFFFE, ' ', false, 0},
    {vr_type::UT, "Unlimited Text",        0xFFFFFFFE, ' ', false, 0},

    // Numeric VRs (binary encoded, fixed length per value)
    {vr_type::FL, "Floating Point Single", 4,        '\0', true,  4},
    {vr_type::FD, "Floating Point Double", 8,        '\0', true,  8},
    {vr_type::SL, "Signed Long",           4,        '\0', true,  4},
    {vr_type::SS, "Signed Short",          2,        '\0', true,  2},
    {vr_type::SV, "Signed 64-bit Very Long", 8,      '\0', true,  8},
    {vr_type::UL, "Unsigned Long",         4,        '\0', true,  4},
    {vr_type::US, "Unsigned Short",        2,        '\0', true,  2},
    {vr_type::UV, "Unsigned 64-bit Very Long", 8,    '\0', true,  8},

    // Binary VRs (raw bytes, variable length)
    {vr_type::OB, "Other Byte",            0xFFFFFFFE, '\0', false, 0},
    {vr_type::OD, "Other Double",          0xFFFFFFFE, '\0', false, 0},
    {vr_type::OF, "Other Float",           0xFFFFFFFE, '\0', false, 0},
    {vr_type::OL, "Other Long",            0xFFFFFFFE, '\0', false, 0},
    {vr_type::OV, "Other 64-bit Very Long", 0xFFFFFFFE, '\0', false, 0},
    {vr_type::OW, "Other Word",            0xFFFFFFFE, '\0', false, 0},
}};

// Additional VR info for special types
constexpr vr_info at_info = {vr_type::AT, "Attribute Tag", 4, '\0', true, 4};
constexpr vr_info sq_info = {vr_type::SQ, "Sequence of Items", 0xFFFFFFFF, '\0', false, 0};
constexpr vr_info un_info = {vr_type::UN, "Unknown", 0xFFFFFFFE, '\0', false, 0};

// Build lookup map at runtime
const std::unordered_map<vr_type, const vr_info*>& get_vr_map() {
    static const auto map = []() {
        std::unordered_map<vr_type, const vr_info*> m;
        for (const auto& info : vr_table) {
            m[info.type] = &info;
        }
        m[vr_type::AT] = &at_info;
        m[vr_type::SQ] = &sq_info;
        m[vr_type::UN] = &un_info;
        return m;
    }();
    return map;
}

// Character validation helpers
bool is_cs_char(char c) {
    return std::isupper(static_cast<unsigned char>(c)) ||
           std::isdigit(static_cast<unsigned char>(c)) ||
           c == ' ' || c == '_';
}

bool is_da_char(char c) {
    return std::isdigit(static_cast<unsigned char>(c));
}

bool is_tm_char(char c) {
    return std::isdigit(static_cast<unsigned char>(c)) ||
           c == '.' || c == ':';
}

bool is_ui_char(char c) {
    return std::isdigit(static_cast<unsigned char>(c)) || c == '.';
}

bool is_ds_char(char c) {
    return std::isdigit(static_cast<unsigned char>(c)) ||
           c == '+' || c == '-' || c == '.' ||
           c == 'E' || c == 'e' || c == ' ';
}

bool is_is_char(char c) {
    return std::isdigit(static_cast<unsigned char>(c)) ||
           c == '+' || c == '-';
}

bool is_as_char(char c) {
    return std::isdigit(static_cast<unsigned char>(c)) ||
           c == 'D' || c == 'W' || c == 'M' || c == 'Y';
}

bool is_dt_char(char c) {
    return std::isdigit(static_cast<unsigned char>(c)) ||
           c == '.' || c == '+' || c == '-';
}

bool is_printable(char c) {
    auto uc = static_cast<unsigned char>(c);
    // Allow printable ASCII (0x20-0x7E) plus CR, LF, FF, TAB for text VRs
    return (uc >= 0x20 && uc <= 0x7E) ||
           c == '\r' || c == '\n' || c == '\f' || c == '\t';
}

}  // namespace

const vr_info& get_vr_info(vr_type vr) {
    const auto& map = get_vr_map();
    auto it = map.find(vr);
    if (it != map.end()) {
        return *it->second;
    }
    // Return UN info for unknown VR types
    return un_info;
}

bool validate_value(vr_type vr, std::span<const uint8_t> data) {
    const auto& info = get_vr_info(vr);

    // For fixed-length VRs, check that size is a multiple of fixed_size (VM >= 1)
    if (info.is_fixed_length && info.fixed_size > 0) {
        if (data.size() % info.fixed_size != 0) {
            return false;
        }
        // Fixed-length VRs pass validation if size is correct multiple
        return true;
    }

    // Check maximum length for variable-length VRs (0xFFFFFFFE means unlimited)
    if (info.max_length != 0xFFFFFFFE && info.max_length != 0xFFFFFFFF) {
        if (data.size() > info.max_length) {
            return false;
        }
    }

    // String VRs need character validation
    if (is_string_vr(vr)) {
        std::string_view str_value(reinterpret_cast<const char*>(data.data()),
                                   data.size());
        return is_valid_charset(vr, str_value);
    }

    return true;
}

bool validate_string(vr_type vr, std::string_view value) {
    const auto& info = get_vr_info(vr);

    // Non-string VRs cannot be validated as strings
    if (!is_string_vr(vr)) {
        return false;
    }

    // Check maximum length
    if (info.max_length != 0xFFFFFFFE) {
        if (value.size() > info.max_length) {
            return false;
        }
    }

    // Fixed-length string VRs (AS, DA) must match exact length
    if (info.is_fixed_length && info.fixed_size > 0) {
        if (value.size() != info.fixed_size) {
            return false;
        }
    }

    // Validate character set
    return is_valid_charset(vr, value);
}

std::vector<uint8_t> pad_to_even(vr_type vr, std::span<const uint8_t> data) {
    std::vector<uint8_t> result(data.begin(), data.end());

    if (result.size() % 2 != 0) {
        const auto& info = get_vr_info(vr);
        result.push_back(static_cast<uint8_t>(info.padding_char));
    }

    return result;
}

std::string trim_padding(vr_type vr, std::string_view value) {
    if (value.empty()) {
        return std::string{};
    }

    const auto& info = get_vr_info(vr);
    char pad = info.padding_char;

    // Find the last non-padding character
    auto end = value.find_last_not_of(pad);
    if (end == std::string_view::npos) {
        // All characters are padding
        return std::string{};
    }

    return std::string{value.substr(0, end + 1)};
}

bool is_valid_charset(vr_type vr, std::string_view value) {
    switch (vr) {
        case vr_type::CS:
            // Code String: A-Z, 0-9, space, underscore
            return std::all_of(value.begin(), value.end(), is_cs_char);

        case vr_type::DA:
            // Date: YYYYMMDD format, digits only
            return value.size() == 8 &&
                   std::all_of(value.begin(), value.end(), is_da_char);

        case vr_type::TM:
            // Time: digits, period, colon
            return std::all_of(value.begin(), value.end(), is_tm_char);

        case vr_type::UI:
            // UID: digits and periods only, max 64 chars
            return value.size() <= 64 &&
                   std::all_of(value.begin(), value.end(), is_ui_char);

        case vr_type::DS:
            // Decimal String: digits, +, -, ., E, e, space
            return std::all_of(value.begin(), value.end(), is_ds_char);

        case vr_type::IS:
            // Integer String: digits, +, -
            return std::all_of(value.begin(), value.end(), is_is_char);

        case vr_type::AS:
            // Age String: nnnX format where X is D/W/M/Y
            if (value.size() != 4) {
                return false;
            }
            return std::all_of(value.begin(), value.end(), is_as_char) &&
                   (value[3] == 'D' || value[3] == 'W' ||
                    value[3] == 'M' || value[3] == 'Y');

        case vr_type::DT:
            // Date Time: digits, period, +, -
            return std::all_of(value.begin(), value.end(), is_dt_char);

        case vr_type::AE:
        case vr_type::LO:
        case vr_type::SH:
        case vr_type::PN:
            // These allow most printable characters but no control chars
            // except leading/trailing spaces
            return std::all_of(value.begin(), value.end(), is_printable);

        case vr_type::LT:
        case vr_type::ST:
        case vr_type::UT:
        case vr_type::UC:
        case vr_type::UR:
            // Text VRs allow all printable plus CR, LF, FF, TAB
            return std::all_of(value.begin(), value.end(), is_printable);

        default:
            // Non-string VRs - charset validation not applicable
            return true;
    }
}

}  // namespace pacs::encoding
