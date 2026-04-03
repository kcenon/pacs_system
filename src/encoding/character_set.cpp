// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file character_set.cpp
 * @brief Implementation of DICOM Character Set registry, ISO 2022 parser, and decoder
 */

#include "pacs/encoding/character_set.hpp"

#include <algorithm>
#include <array>
#include <cstring>

#include <unicode/ucnv.h>
#include <unicode/utypes.h>

namespace kcenon::pacs::encoding {

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

// Latin-2 (Central European): ESC - B
constexpr std::string_view esc_ir_101{"\x1B\x2D\x42", 3};

// Greek: ESC - F
constexpr std::string_view esc_ir_126{"\x1B\x2D\x46", 3};

// Arabic: ESC - G
constexpr std::string_view esc_ir_127{"\x1B\x2D\x47", 3};

// Hebrew: ESC - H
constexpr std::string_view esc_ir_138{"\x1B\x2D\x48", 3};

// Cyrillic: ESC - L
constexpr std::string_view esc_ir_144{"\x1B\x2D\x4C", 3};

// Thai: ESC - T
constexpr std::string_view esc_ir_166{"\x1B\x2D\x54", 3};

/// Static registry of supported character sets
const std::array<character_set_info, 20> charset_registry = {{
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
        "ISO_IR 101",
        "Latin-2 (Central European)",
        "ISO-IR 101",
        false,
        {},
        "ISO-8859-2",
        false
    },
    {
        "ISO_IR 126",
        "Greek",
        "ISO-IR 126",
        false,
        {},
        "ISO-8859-7",
        false
    },
    {
        "ISO_IR 127",
        "Arabic",
        "ISO-IR 127",
        false,
        {},
        "ISO-8859-6",
        false
    },
    {
        "ISO_IR 138",
        "Hebrew",
        "ISO-IR 138",
        false,
        {},
        "ISO-8859-8",
        false
    },
    {
        "ISO_IR 144",
        "Cyrillic",
        "ISO-IR 144",
        false,
        {},
        "ISO-8859-5",
        false
    },
    {
        "ISO_IR 166",
        "Thai (TIS 620-2533)",
        "ISO-IR 166",
        false,
        {},
        "TIS-620",
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
        "ISO 2022 IR 101",
        "Latin-2 (with extensions)",
        "ISO-IR 101",
        true,
        esc_ir_101,
        "ISO-8859-2",
        false
    },
    {
        "ISO 2022 IR 126",
        "Greek (with extensions)",
        "ISO-IR 126",
        true,
        esc_ir_126,
        "ISO-8859-7",
        false
    },
    {
        "ISO 2022 IR 127",
        "Arabic (with extensions)",
        "ISO-IR 127",
        true,
        esc_ir_127,
        "ISO-8859-6",
        false
    },
    {
        "ISO 2022 IR 138",
        "Hebrew (with extensions)",
        "ISO-IR 138",
        true,
        esc_ir_138,
        "ISO-8859-8",
        false
    },
    {
        "ISO 2022 IR 144",
        "Cyrillic (with extensions)",
        "ISO-IR 144",
        true,
        esc_ir_144,
        "ISO-8859-5",
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
        "ISO 2022 IR 166",
        "Thai (with extensions)",
        "ISO-IR 166",
        true,
        esc_ir_166,
        "TIS-620",
        false
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

// GB18030 is separate: replacement encoding without ISO 2022 escape sequences
const character_set_info charset_gb18030 = {
    "GB18030",
    "Chinese (GB18030, full)",
    "GB18030",
    false,
    {},
    "GB18030",
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
    if (charset_gb18030.defined_term == term) {
        return &charset_gb18030;
    }
    return nullptr;
}

/// Map ISO-IR number to Defined Term for lookup
const character_set_info* find_by_ir_number(int ir_number) noexcept {
    switch (ir_number) {
        case 6: return find_in_registry("ISO_IR 6");
        case 100: return find_in_registry("ISO_IR 100");
        case 101: return find_in_registry("ISO_IR 101");
        case 126: return find_in_registry("ISO_IR 126");
        case 127: return find_in_registry("ISO_IR 127");
        case 138: return find_in_registry("ISO_IR 138");
        case 144: return find_in_registry("ISO_IR 144");
        case 166: return find_in_registry("ISO_IR 166");
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

// ICU-based encoding conversion: source encoding → UTF-8
std::string icu_convert_to_utf8(std::string_view input,
                                const character_set_info& charset) {
    if (charset.encoding_name == "ASCII" ||
        charset.encoding_name == "UTF-8") {
        return std::string(input);
    }

    // ISO-2022-JP is a stateful encoding: ICU expects escape sequences
    // in the input. Since split_by_escape_sequences strips them, we must
    // re-wrap the raw JIS bytes before calling ucnv_convert.
    std::string wrapped_input;
    if (charset.encoding_name == "ISO-2022-JP") {
        wrapped_input.append(charset.escape_sequence);
        wrapped_input.append(input);
        wrapped_input.append(esc_ir_6);
    }
    const auto& actual_input = (charset.encoding_name == "ISO-2022-JP")
        ? std::string_view(wrapped_input)
        : input;

    // Allocate output buffer (UTF-8 can be up to 4x the input size)
    auto out_size = static_cast<int32_t>(actual_input.size() * 4 + 4);
    std::string output(static_cast<size_t>(out_size), '\0');

    UErrorCode status = U_ZERO_ERROR;
    int32_t result_len = ucnv_convert(
        "UTF-8", charset.encoding_name.data(),
        output.data(), out_size,
        actual_input.data(), static_cast<int32_t>(actual_input.size()),
        &status);

    if (U_FAILURE(status)) {
        // Conversion failed, return raw bytes
        return std::string(input);
    }

    output.resize(static_cast<size_t>(result_len));
    return output;
}

// ICU-based encoding conversion: UTF-8 → target encoding
std::string icu_convert_from_utf8(std::string_view utf8_input,
                                   const character_set_info& charset) {
    if (charset.encoding_name == "ASCII" ||
        charset.encoding_name == "UTF-8") {
        return std::string(utf8_input);
    }

    auto out_size = static_cast<int32_t>(utf8_input.size() * 4 + 4);
    std::string output(static_cast<size_t>(out_size), '\0');

    UErrorCode status = U_ZERO_ERROR;
    int32_t result_len = ucnv_convert(
        charset.encoding_name.data(), "UTF-8",
        output.data(), out_size,
        utf8_input.data(), static_cast<int32_t>(utf8_input.size()),
        &status);

    if (U_FAILURE(status)) {
        return std::string(utf8_input);
    }

    output.resize(static_cast<size_t>(result_len));

    // ISO-2022-JP is stateful: ICU output includes escape sequences
    // (ESC $ B ... ESC ( B). Since encode_from_utf8() adds its own escape
    // sequences, strip the ICU-generated ones to avoid duplication.
    if (charset.encoding_name == "ISO-2022-JP" && output.size() >= 6) {
        auto esc_prefix = charset.escape_sequence;
        auto esc_suffix = esc_ir_6;
        bool has_prefix = (output.size() >= esc_prefix.size() &&
            std::string_view(output).substr(0, esc_prefix.size()) == esc_prefix);
        bool has_suffix = (output.size() >= esc_suffix.size() &&
            std::string_view(output).substr(
                output.size() - esc_suffix.size()) == esc_suffix);
        if (has_prefix && has_suffix) {
            output = output.substr(
                esc_prefix.size(),
                output.size() - esc_prefix.size() - esc_suffix.size());
        }
    }

    return output;
}

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
    result.reserve(charset_registry.size() + 2);
    for (const auto& entry : charset_registry) {
        result.push_back(&entry);
    }
    result.push_back(&charset_ir_58);
    result.push_back(&charset_gb18030);
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

    return icu_convert_to_utf8(text, charset);
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

// =============================================================================
// String Encoding (UTF-8 to target encoding)
// =============================================================================

std::string convert_from_utf8(
    std::string_view utf8_text,
    const character_set_info& charset) {

    if (utf8_text.empty()) {
        return {};
    }

    if (charset.encoding_name == "ASCII" ||
        charset.encoding_name == "UTF-8") {
        return std::string(utf8_text);
    }

    return icu_convert_from_utf8(utf8_text, charset);
}

std::string encode_from_utf8(
    std::string_view utf8_text,
    const specific_character_set& scs) {

    if (utf8_text.empty()) {
        return {};
    }

    // Fast path: UTF-8 passthrough
    if (scs.is_utf8()) {
        return std::string(utf8_text);
    }

    // No extensions: simple single-charset conversion
    if (!scs.uses_extensions()) {
        return convert_from_utf8(utf8_text, *scs.default_set);
    }

    // ISO 2022 with extensions: detect non-ASCII runs and wrap with
    // escape sequences for the first available CJK extension charset.
    // Strategy: scan UTF-8 text byte by byte. ASCII bytes (< 0x80) use
    // the default charset. Non-ASCII byte runs are converted using the
    // first multi-byte extension charset, bracketed by escape sequences.

    const character_set_info* mb_charset = nullptr;
    for (const auto* cs : scs.extension_sets) {
        if (cs && cs->is_multi_byte) {
            mb_charset = cs;
            break;
        }
    }

    if (!mb_charset) {
        // No multi-byte extension available; convert with default
        return convert_from_utf8(utf8_text, *scs.default_set);
    }

    std::string result;
    result.reserve(utf8_text.size() * 2);

    bool in_multibyte = false;
    size_t run_start = 0;

    for (size_t i = 0; i < utf8_text.size(); ) {
        auto uc = static_cast<unsigned char>(utf8_text[i]);
        bool is_ascii = (uc < 0x80);

        if (is_ascii) {
            if (in_multibyte) {
                // Flush multi-byte run
                auto mb_run = utf8_text.substr(run_start, i - run_start);
                result.append(mb_charset->escape_sequence);
                result += convert_from_utf8(mb_run, *mb_charset);
                // Return to ASCII
                result.append(esc_ir_6);
                in_multibyte = false;
            }
            if (!in_multibyte && i == run_start) {
                // Continue ASCII
            }
            run_start = i;
            result += utf8_text[i];
            ++i;
            run_start = i;
        } else {
            if (!in_multibyte) {
                in_multibyte = true;
                run_start = i;
            }
            // Skip multi-byte UTF-8 sequence
            if (uc < 0xC0) { ++i; }
            else if (uc < 0xE0) { i += 2; }
            else if (uc < 0xF0) { i += 3; }
            else { i += 4; }
            // Clamp to text size
            if (i > utf8_text.size()) { i = utf8_text.size(); }
        }
    }

    // Flush remaining multi-byte run
    if (in_multibyte && run_start < utf8_text.size()) {
        auto mb_run = utf8_text.substr(run_start);
        result.append(mb_charset->escape_sequence);
        result += convert_from_utf8(mb_run, *mb_charset);
        result.append(esc_ir_6);
    }

    return result;
}

}  // namespace kcenon::pacs::encoding
