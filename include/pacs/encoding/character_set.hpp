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
 * @file character_set.hpp
 * @brief DICOM Character Set registry, ISO 2022 parser, and string decoder
 *
 * Provides support for decoding DICOM strings encoded with CJK character sets
 * (Korean, Japanese, Chinese) using ISO 2022 escape sequence-based code extensions
 * as specified in DICOM PS3.5 Section 6.1 and PS3.3 Annex C.12.1.1.2.
 *
 * Supported character sets:
 * - ISO-IR 6 (ASCII, default repertoire)
 * - ISO-IR 100 (Latin-1, Western European)
 * - ISO-IR 192 (UTF-8, Unicode)
 * - ISO-IR 149 (Korean, KS X 1001 / EUC-KR)
 * - ISO-IR 87 (Japanese Kanji, JIS X 0208)
 * - ISO-IR 13 (Japanese Katakana, JIS X 0201)
 * - ISO-IR 58 (Chinese, GB2312)
 *
 * @see DICOM PS3.5 Section 6.1 - Support of Character Repertoires
 * @see DICOM PS3.3 Section C.12.1.1.2 - Specific Character Set
 */

#ifndef PACS_ENCODING_CHARACTER_SET_HPP
#define PACS_ENCODING_CHARACTER_SET_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pacs::encoding {

/**
 * @brief Information about a DICOM character set
 *
 * Maps a DICOM Defined Term (e.g., "ISO 2022 IR 149") to its encoding
 * parameters needed for string conversion.
 */
struct character_set_info {
    std::string_view defined_term;     ///< DICOM Defined Term (e.g., "ISO 2022 IR 149")
    std::string_view description;      ///< Human-readable name (e.g., "Korean (KS X 1001)")
    std::string_view iso_ir;           ///< ISO-IR designation (e.g., "ISO-IR 149")
    bool uses_code_extensions;         ///< true if ISO 2022 escape sequences are used
    std::string_view escape_sequence;  ///< Raw escape sequence bytes (empty if none)
    std::string_view encoding_name;    ///< Platform encoding name for iconv (e.g., "EUC-KR")
    bool is_multi_byte;               ///< true if characters can be multi-byte
};

/// @name Character Set Registry
/// @{

/**
 * @brief Look up character set info by DICOM Defined Term.
 * @param defined_term The DICOM Defined Term (e.g., "ISO 2022 IR 149", "ISO_IR 100")
 * @return Pointer to character_set_info, or nullptr if not found
 *
 * Supports both forms: "ISO_IR 100" (without code extensions) and
 * "ISO 2022 IR 149" (with code extensions).
 */
[[nodiscard]] const character_set_info* find_character_set(
    std::string_view defined_term) noexcept;

/**
 * @brief Look up character set info by ISO-IR number.
 * @param iso_ir_number The ISO-IR number (e.g., 149, 87, 100)
 * @return Pointer to character_set_info, or nullptr if not found
 */
[[nodiscard]] const character_set_info* find_character_set_by_ir(
    int iso_ir_number) noexcept;

/**
 * @brief Get the default character set (ISO-IR 6, ASCII).
 * @return Reference to the ASCII character set info
 */
[[nodiscard]] const character_set_info& default_character_set() noexcept;

/**
 * @brief Get all registered character sets.
 * @return Vector of pointers to all registered character_set_info entries
 */
[[nodiscard]] std::vector<const character_set_info*> all_character_sets() noexcept;

/// @}

/// @name Specific Character Set Parsing
/// @{

/**
 * @brief Parsed representation of a multi-valued Specific Character Set.
 *
 * DICOM Specific Character Set (0008,0005) can be multi-valued with
 * backslash separators. The first value is the default character set
 * for G0, and subsequent values define character sets used via
 * ISO 2022 escape sequences.
 *
 * Example: "\\ISO 2022 IR 149" means:
 * - Component 0: empty → ASCII (ISO-IR 6) default
 * - Component 1: ISO 2022 IR 149 → Korean
 */
struct specific_character_set {
    /// Character set for default (G0) repertoire
    const character_set_info* default_set;

    /// Additional character sets activated by escape sequences
    std::vector<const character_set_info*> extension_sets;

    /// Whether this uses ISO 2022 code extensions
    [[nodiscard]] bool uses_extensions() const noexcept;

    /// Whether this is a single-byte-only configuration
    [[nodiscard]] bool is_single_byte_only() const noexcept;

    /// Check if UTF-8 is the active character set
    [[nodiscard]] bool is_utf8() const noexcept;
};

/**
 * @brief Parse a Specific Character Set (0008,0005) value.
 * @param value The raw attribute value (e.g., "\\ISO 2022 IR 149")
 * @return Parsed specific_character_set with resolved character set pointers
 *
 * Handles multi-valued strings separated by backslash ('\\').
 * Empty first component defaults to ISO-IR 6 (ASCII).
 * Unknown Defined Terms are skipped with a warning.
 */
[[nodiscard]] specific_character_set parse_specific_character_set(
    std::string_view value);

/// @}

/// @name ISO 2022 Escape Sequence Detection
/// @{

/**
 * @brief A text segment with its associated character set.
 *
 * Represents a contiguous portion of a DICOM string that uses
 * a single character encoding. The decoder splits strings at
 * escape sequence boundaries into these segments.
 */
struct text_segment {
    std::string_view text;                 ///< The raw bytes of this segment
    const character_set_info* charset;     ///< The character set for this segment
};

/**
 * @brief Split a string into segments by ISO 2022 escape sequences.
 * @param text The raw DICOM string bytes
 * @param scs The parsed Specific Character Set configuration
 * @return Vector of text segments, each with its character set
 *
 * Scans for ESC (0x1B) bytes, matches escape sequences against
 * the configured character sets, and splits the text into segments.
 * Segments between escape sequences inherit the previous character set.
 */
[[nodiscard]] std::vector<text_segment> split_by_escape_sequences(
    std::string_view text,
    const specific_character_set& scs);

/// @}

/// @name String Decoding
/// @{

/**
 * @brief Decode a DICOM string to UTF-8 using the given character set.
 * @param text The raw DICOM string bytes
 * @param scs The Specific Character Set configuration
 * @return UTF-8 encoded string
 *
 * This is the main entry point for character set conversion. It:
 * 1. Checks if conversion is needed (UTF-8 passthrough)
 * 2. Splits text by escape sequences (if ISO 2022)
 * 3. Converts each segment to UTF-8
 * 4. Concatenates the results
 *
 * If conversion fails for any segment, the raw bytes are passed through.
 */
[[nodiscard]] std::string decode_to_utf8(
    std::string_view text,
    const specific_character_set& scs);

/**
 * @brief Decode a single segment from a specific encoding to UTF-8.
 * @param text The raw bytes in the source encoding
 * @param charset The character set of the source bytes
 * @return UTF-8 encoded string
 *
 * Uses platform iconv for encoding conversion. Falls back to
 * raw bytes if conversion fails.
 */
[[nodiscard]] std::string convert_to_utf8(
    std::string_view text,
    const character_set_info& charset);

/**
 * @brief Decode a Person Name (PN) value to UTF-8.
 * @param pn_value The raw PN value (may contain '=' component group separators)
 * @param scs The Specific Character Set configuration
 * @return UTF-8 encoded Person Name string
 *
 * PN values have up to three component groups separated by '=':
 * - Alphabetic (single-byte)
 * - Ideographic (multi-byte, e.g., kanji/hangul)
 * - Phonetic (single-byte, e.g., romaji/jamo)
 *
 * Each component group is decoded independently.
 */
[[nodiscard]] std::string decode_person_name(
    std::string_view pn_value,
    const specific_character_set& scs);

/**
 * @brief Encode a UTF-8 string to the target character set encoding.
 * @param utf8_text The UTF-8 encoded source string
 * @param scs The Specific Character Set configuration
 * @return Encoded string with ISO 2022 escape sequences if needed
 *
 * This is the reverse of decode_to_utf8(). It converts a UTF-8 string
 * to the encoding specified by the Specific Character Set, inserting
 * ISO 2022 escape sequences as appropriate for CJK text.
 *
 * If the charset is UTF-8, the input is returned unchanged.
 * If conversion fails, the raw UTF-8 bytes are returned as-is.
 */
[[nodiscard]] std::string encode_from_utf8(
    std::string_view utf8_text,
    const specific_character_set& scs);

/**
 * @brief Encode a single UTF-8 segment to a specific character set.
 * @param utf8_text The UTF-8 encoded source string
 * @param charset The target character set
 * @return Encoded string in the target encoding
 */
[[nodiscard]] std::string convert_from_utf8(
    std::string_view utf8_text,
    const character_set_info& charset);

/// @}

}  // namespace pacs::encoding

#endif  // PACS_ENCODING_CHARACTER_SET_HPP
