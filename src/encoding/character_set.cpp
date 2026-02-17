/**
 * @file character_set.cpp
 * @brief Implementation of DICOM Character Set registry, ISO 2022 parser, and decoder
 */

#include "pacs/encoding/character_set.hpp"

#include <algorithm>
#include <array>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <iconv.h>
#endif

namespace pacs::encoding {

// =============================================================================
// Character Set Registry
// =============================================================================

namespace {

// Escape sequence constants (raw bytes including ESC = 0x1B)
constexpr char ESC = '\x1B';

// Korean: ESC $ ) C
constexpr std::string_view esc_ir_149{"\x1B\x24\x29\x43", 4};

// Japanese Kanji: ESC $ B
constexpr std::string_view esc_ir_87{"\x1B\x24\x42", 3};

// Japanese Katakana: ESC ( J
constexpr std::string_view esc_ir_13{"\x1B\x28\x4A", 3};

// Chinese GB2312: ESC $ ) A
constexpr std::string_view esc_ir_58{"\x1B\x24\x29\x41", 4};

// ASCII return: ESC ( B
constexpr std::string_view esc_ir_6{"\x1B\x28\x42", 3};

// Latin-1: ESC - A
constexpr std::string_view esc_ir_100{"\x1B\x2D\x41", 3};

/// Static registry of supported character sets
const std::array<character_set_info, 8> charset_registry = {{
    {
        "ISO_IR 6",             // defined_term
        "ASCII (Default)",      // description
        "ISO-IR 6",             // iso_ir
        false,                  // uses_code_extensions
        {},                     // escape_sequence (none for default)
        "ASCII",                // encoding_name
        false                   // is_multi_byte
    },
    {
        "ISO_IR 100",
        "Latin-1 (Western European)",
        "ISO-IR 100",
        false,
        {},
        "ISO-8859-1",
        false
    },
    {
        "ISO_IR 192",
        "UTF-8 (Unicode)",
        "ISO-IR 192",
        false,
        {},
        "UTF-8",
        true
    },
    {
        "ISO 2022 IR 6",
        "ASCII (Default, with extensions)",
        "ISO-IR 6",
        true,
        esc_ir_6,
        "ASCII",
        false
    },
    {
        "ISO 2022 IR 100",
        "Latin-1 (with extensions)",
        "ISO-IR 100",
        true,
        esc_ir_100,
        "ISO-8859-1",
        false
    },
    {
        "ISO 2022 IR 149",
        "Korean (KS X 1001)",
        "ISO-IR 149",
        true,
        esc_ir_149,
        "EUC-KR",
        true
    },
    {
        "ISO 2022 IR 87",
        "Japanese Kanji (JIS X 0208)",
        "ISO-IR 87",
        true,
        esc_ir_87,
        "ISO-2022-JP",
        true
    },
    {
        "ISO 2022 IR 13",
        "Japanese Katakana (JIS X 0201)",
        "ISO-IR 13",
        true,
        esc_ir_13,
        "JIS_X0201",
        false
    },
}};

// GB2312 is separate because it shares the same pattern as Korean
const character_set_info charset_ir_58 = {
    "ISO 2022 IR 58",
    "Chinese (GB2312)",
    "ISO-IR 58",
    true,
    esc_ir_58,
    "GB2312",
    true
};

const character_set_info* find_in_registry(std::string_view term) noexcept {
    for (const auto& entry : charset_registry) {
        if (entry.defined_term == term) {
            return &entry;
        }
    }
    if (charset_ir_58.defined_term == term) {
        return &charset_ir_58;
    }
    return nullptr;
}

/// Map ISO-IR number to Defined Term for lookup
const character_set_info* find_by_ir_number(int ir_number) noexcept {
    switch (ir_number) {
        case 6: return find_in_registry("ISO_IR 6");
        case 100: return find_in_registry("ISO_IR 100");
        case 192: return find_in_registry("ISO_IR 192");
        case 149: return find_in_registry("ISO 2022 IR 149");
        case 87: return find_in_registry("ISO 2022 IR 87");
        case 13: return find_in_registry("ISO 2022 IR 13");
        case 58: return &charset_ir_58;
        default: return nullptr;
    }
}

/// Find character set by its escape sequence
const character_set_info* find_by_escape_sequence(
    std::string_view text, size_t pos,
    const specific_character_set& scs) noexcept {
    // Check extension sets first (more specific)
    for (const auto* cs : scs.extension_sets) {
        if (cs && !cs->escape_sequence.empty()) {
            auto esc_len = cs->escape_sequence.size();
            if (pos + esc_len <= text.size() &&
                text.substr(pos, esc_len) == cs->escape_sequence) {
                return cs;
            }
        }
    }
    // Check ASCII return sequence
    if (pos + esc_ir_6.size() <= text.size() &&
        text.substr(pos, esc_ir_6.size()) == esc_ir_6) {
        return scs.default_set;
    }
    return nullptr;
}

#ifdef _WIN32

// Windows: use MultiByteToWideChar + WideCharToMultiByte
std::string iconv_convert(std::string_view input,
                          const character_set_info& charset) {
    // Determine Windows code page
    UINT code_page = CP_ACP;
    if (charset.encoding_name == "EUC-KR") {
        code_page = 949;
    } else if (charset.encoding_name == "ISO-2022-JP") {
        code_page = 50220;
    } else if (charset.encoding_name == "GB2312") {
        code_page = 936;
    } else if (charset.encoding_name == "ISO-8859-1") {
        code_page = 28591;
    } else if (charset.encoding_name == "JIS_X0201") {
        code_page = 50222;
    } else if (charset.encoding_name == "ASCII" ||
               charset.encoding_name == "UTF-8") {
        return std::string(input);
    }

    // Convert to wide char
    int wide_len = MultiByteToWideChar(
        code_page, 0, input.data(), static_cast<int>(input.size()),
        nullptr, 0);
    if (wide_len <= 0) {
        return std::string(input);  // fallback
    }

    std::wstring wide(static_cast<size_t>(wide_len), L'\0');
    MultiByteToWideChar(
        code_page, 0, input.data(), static_cast<int>(input.size()),
        wide.data(), wide_len);

    // Convert wide char to UTF-8
    int utf8_len = WideCharToMultiByte(
        CP_UTF8, 0, wide.data(), wide_len, nullptr, 0, nullptr, nullptr);
    if (utf8_len <= 0) {
        return std::string(input);  // fallback
    }

    std::string result(static_cast<size_t>(utf8_len), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, wide.data(), wide_len,
        result.data(), utf8_len, nullptr, nullptr);

    return result;
}

#else

// POSIX: use iconv
std::string iconv_convert(std::string_view input,
                          const character_set_info& charset) {
    if (charset.encoding_name == "ASCII" ||
        charset.encoding_name == "UTF-8") {
        return std::string(input);
    }

    iconv_t cd = iconv_open("UTF-8", charset.encoding_name.data());
    if (cd == reinterpret_cast<iconv_t>(-1)) {
        // Encoding not supported on this platform, return raw bytes
        return std::string(input);
    }

    // iconv requires non-const input pointer
    std::string input_copy(input);
    char* in_ptr = input_copy.data();
    size_t in_left = input_copy.size();

    // Allocate output buffer (UTF-8 can be up to 4x the input size)
    size_t out_size = input.size() * 4 + 4;
    std::string output(out_size, '\0');
    char* out_ptr = output.data();
    size_t out_left = out_size;

    size_t result = iconv(cd, &in_ptr, &in_left, &out_ptr, &out_left);
    iconv_close(cd);

    if (result == static_cast<size_t>(-1)) {
        // Conversion failed, return raw bytes
        return std::string(input);
    }

    output.resize(out_size - out_left);
    return output;
}

#endif

}  // anonymous namespace

// =============================================================================
// Public Registry Functions
// =============================================================================

const character_set_info* find_character_set(
    std::string_view defined_term) noexcept {
    return find_in_registry(defined_term);
}

const character_set_info* find_character_set_by_ir(
    int iso_ir_number) noexcept {
    return find_by_ir_number(iso_ir_number);
}

const character_set_info& default_character_set() noexcept {
    // ISO_IR 6 (ASCII) is always the first entry
    return charset_registry[0];
}

std::vector<const character_set_info*> all_character_sets() noexcept {
    std::vector<const character_set_info*> result;
    result.reserve(charset_registry.size() + 1);
    for (const auto& entry : charset_registry) {
        result.push_back(&entry);
    }
    result.push_back(&charset_ir_58);
    return result;
}

// =============================================================================
// Specific Character Set Parsing
// =============================================================================

bool specific_character_set::uses_extensions() const noexcept {
    return !extension_sets.empty();
}

bool specific_character_set::is_single_byte_only() const noexcept {
    if (default_set && default_set->is_multi_byte) {
        return false;
    }
    for (const auto* cs : extension_sets) {
        if (cs && cs->is_multi_byte) {
            return false;
        }
    }
    return true;
}

bool specific_character_set::is_utf8() const noexcept {
    return default_set && default_set->defined_term == "ISO_IR 192";
}

specific_character_set parse_specific_character_set(std::string_view value) {
    specific_character_set result;
    result.default_set = &default_character_set();

    if (value.empty()) {
        return result;
    }

    // Split by backslash
    std::vector<std::string_view> components;
    size_t start = 0;
    while (start <= value.size()) {
        size_t pos = value.find('\\', start);
        if (pos == std::string_view::npos) {
            components.push_back(value.substr(start));
            break;
        }
        components.push_back(value.substr(start, pos - start));
        start = pos + 1;
    }

    if (components.empty()) {
        return result;
    }

    // First component: default character set
    auto first = components[0];
    // Trim whitespace
    while (!first.empty() && first.front() == ' ') first.remove_prefix(1);
    while (!first.empty() && first.back() == ' ') first.remove_suffix(1);

    if (!first.empty()) {
        const auto* cs = find_in_registry(first);
        if (cs) {
            result.default_set = cs;
        }
    }
    // Empty first component: ASCII default (already set)

    // Remaining components: extension character sets
    for (size_t i = 1; i < components.size(); ++i) {
        auto term = components[i];
        while (!term.empty() && term.front() == ' ') term.remove_prefix(1);
        while (!term.empty() && term.back() == ' ') term.remove_suffix(1);

        if (!term.empty()) {
            const auto* cs = find_in_registry(term);
            if (cs) {
                result.extension_sets.push_back(cs);
            }
        }
    }

    return result;
}

// =============================================================================
// ISO 2022 Escape Sequence Handling
// =============================================================================

std::vector<text_segment> split_by_escape_sequences(
    std::string_view text,
    const specific_character_set& scs) {

    std::vector<text_segment> segments;

    if (text.empty()) {
        return segments;
    }

    // If no extensions, the entire text uses the default charset
    if (!scs.uses_extensions()) {
        segments.push_back({text, scs.default_set});
        return segments;
    }

    const character_set_info* current_charset = scs.default_set;
    size_t segment_start = 0;

    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == ESC) {
            // Try to match an escape sequence
            const auto* new_charset = find_by_escape_sequence(text, i, scs);
            if (new_charset) {
                // Emit the segment before this escape sequence
                if (i > segment_start) {
                    segments.push_back({
                        text.substr(segment_start, i - segment_start),
                        current_charset
                    });
                }

                // Skip the escape sequence
                size_t esc_len = new_charset->escape_sequence.empty()
                    ? esc_ir_6.size()  // ASCII return
                    : new_charset->escape_sequence.size();
                i += esc_len - 1;  // -1 because loop increments
                segment_start = i + 1;
                current_charset = new_charset;
            }
        }
    }

    // Emit the remaining segment
    if (segment_start < text.size()) {
        segments.push_back({
            text.substr(segment_start),
            current_charset
        });
    }

    return segments;
}

// =============================================================================
// String Decoding
// =============================================================================

std::string convert_to_utf8(
    std::string_view text,
    const character_set_info& charset) {

    if (text.empty()) {
        return {};
    }

    // ASCII and UTF-8 need no conversion
    if (charset.encoding_name == "ASCII" ||
        charset.encoding_name == "UTF-8") {
        return std::string(text);
    }

    return iconv_convert(text, charset);
}

std::string decode_to_utf8(
    std::string_view text,
    const specific_character_set& scs) {

    if (text.empty()) {
        return {};
    }

    // Fast path: UTF-8 passthrough
    if (scs.is_utf8()) {
        return std::string(text);
    }

    // Fast path: single-byte charset without extensions
    if (!scs.uses_extensions()) {
        return convert_to_utf8(text, *scs.default_set);
    }

    // ISO 2022: split by escape sequences and convert each segment
    auto segments = split_by_escape_sequences(text, scs);

    std::string result;
    result.reserve(text.size() * 2);

    for (const auto& seg : segments) {
        if (seg.charset) {
            result += convert_to_utf8(seg.text, *seg.charset);
        } else {
            result.append(seg.text);
        }
    }

    return result;
}

std::string decode_person_name(
    std::string_view pn_value,
    const specific_character_set& scs) {

    if (pn_value.empty()) {
        return {};
    }

    // Split by '=' into component groups
    // (Alphabetic=Ideographic=Phonetic)
    std::string result;
    result.reserve(pn_value.size() * 2);

    size_t start = 0;
    bool first = true;

    while (start <= pn_value.size()) {
        size_t eq_pos = pn_value.find('=', start);
        std::string_view group;
        if (eq_pos == std::string_view::npos) {
            group = pn_value.substr(start);
            start = pn_value.size() + 1;
        } else {
            group = pn_value.substr(start, eq_pos - start);
            start = eq_pos + 1;
        }

        if (!first) {
            result += '=';
        }
        first = false;

        // Decode each component group independently
        result += decode_to_utf8(group, scs);
    }

    return result;
}

}  // namespace pacs::encoding
