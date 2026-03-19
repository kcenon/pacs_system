/**
 * @file character_set_test.cpp
 * @brief Unit tests for DICOM character set registry, ISO 2022 parser, and decoder
 */

#include <catch2/catch_test_macros.hpp>

#include "pacs/encoding/character_set.hpp"

#include <cstring>

using namespace kcenon::pacs::encoding;

// =============================================================================
// Character Set Registry Tests
// =============================================================================

TEST_CASE("Character set registry lookup", "[encoding][charset]") {
    SECTION("Find ASCII (ISO_IR 6)") {
        const auto* cs = find_character_set("ISO_IR 6");
        REQUIRE(cs != nullptr);
        CHECK(cs->defined_term == "ISO_IR 6");
        CHECK(cs->encoding_name == "ASCII");
        CHECK_FALSE(cs->uses_code_extensions);
        CHECK_FALSE(cs->is_multi_byte);
    }

    SECTION("Find Latin-1 (ISO_IR 100)") {
        const auto* cs = find_character_set("ISO_IR 100");
        REQUIRE(cs != nullptr);
        CHECK(cs->defined_term == "ISO_IR 100");
        CHECK(cs->encoding_name == "ISO-8859-1");
        CHECK_FALSE(cs->uses_code_extensions);
        CHECK_FALSE(cs->is_multi_byte);
    }

    SECTION("Find UTF-8 (ISO_IR 192)") {
        const auto* cs = find_character_set("ISO_IR 192");
        REQUIRE(cs != nullptr);
        CHECK(cs->defined_term == "ISO_IR 192");
        CHECK(cs->encoding_name == "UTF-8");
        CHECK_FALSE(cs->uses_code_extensions);
        CHECK(cs->is_multi_byte);
    }

    SECTION("Find Korean (ISO 2022 IR 149)") {
        const auto* cs = find_character_set("ISO 2022 IR 149");
        REQUIRE(cs != nullptr);
        CHECK(cs->defined_term == "ISO 2022 IR 149");
        CHECK(cs->encoding_name == "EUC-KR");
        CHECK(cs->uses_code_extensions);
        CHECK(cs->is_multi_byte);
        CHECK_FALSE(cs->escape_sequence.empty());
    }

    SECTION("Find Japanese Kanji (ISO 2022 IR 87)") {
        const auto* cs = find_character_set("ISO 2022 IR 87");
        REQUIRE(cs != nullptr);
        CHECK(cs->defined_term == "ISO 2022 IR 87");
        CHECK(cs->encoding_name == "ISO-2022-JP");
        CHECK(cs->uses_code_extensions);
        CHECK(cs->is_multi_byte);
    }

    SECTION("Find Japanese Katakana (ISO 2022 IR 13)") {
        const auto* cs = find_character_set("ISO 2022 IR 13");
        REQUIRE(cs != nullptr);
        CHECK(cs->defined_term == "ISO 2022 IR 13");
        CHECK(cs->uses_code_extensions);
        CHECK_FALSE(cs->is_multi_byte);
    }

    SECTION("Find Chinese GB2312 (ISO 2022 IR 58)") {
        const auto* cs = find_character_set("ISO 2022 IR 58");
        REQUIRE(cs != nullptr);
        CHECK(cs->defined_term == "ISO 2022 IR 58");
        CHECK(cs->encoding_name == "GB2312");
        CHECK(cs->uses_code_extensions);
        CHECK(cs->is_multi_byte);
    }

    SECTION("Find Latin-2 (ISO_IR 101)") {
        const auto* cs = find_character_set("ISO_IR 101");
        REQUIRE(cs != nullptr);
        CHECK(cs->defined_term == "ISO_IR 101");
        CHECK(cs->encoding_name == "ISO-8859-2");
        CHECK_FALSE(cs->uses_code_extensions);
        CHECK_FALSE(cs->is_multi_byte);
    }

    SECTION("Find Greek (ISO_IR 126)") {
        const auto* cs = find_character_set("ISO_IR 126");
        REQUIRE(cs != nullptr);
        CHECK(cs->encoding_name == "ISO-8859-7");
        CHECK_FALSE(cs->is_multi_byte);
    }

    SECTION("Find Arabic (ISO_IR 127)") {
        const auto* cs = find_character_set("ISO_IR 127");
        REQUIRE(cs != nullptr);
        CHECK(cs->encoding_name == "ISO-8859-6");
        CHECK_FALSE(cs->is_multi_byte);
    }

    SECTION("Find Hebrew (ISO_IR 138)") {
        const auto* cs = find_character_set("ISO_IR 138");
        REQUIRE(cs != nullptr);
        CHECK(cs->encoding_name == "ISO-8859-8");
        CHECK_FALSE(cs->is_multi_byte);
    }

    SECTION("Find Cyrillic (ISO_IR 144)") {
        const auto* cs = find_character_set("ISO_IR 144");
        REQUIRE(cs != nullptr);
        CHECK(cs->encoding_name == "ISO-8859-5");
        CHECK_FALSE(cs->is_multi_byte);
    }

    SECTION("Find Thai (ISO_IR 166)") {
        const auto* cs = find_character_set("ISO_IR 166");
        REQUIRE(cs != nullptr);
        CHECK(cs->encoding_name == "TIS-620");
        CHECK_FALSE(cs->is_multi_byte);
    }

    SECTION("Find GB18030") {
        const auto* cs = find_character_set("GB18030");
        REQUIRE(cs != nullptr);
        CHECK(cs->encoding_name == "GB18030");
        CHECK(cs->is_multi_byte);
        CHECK_FALSE(cs->uses_code_extensions);
    }

    SECTION("Find ISO 2022 IR extension forms") {
        CHECK(find_character_set("ISO 2022 IR 101") != nullptr);
        CHECK(find_character_set("ISO 2022 IR 126") != nullptr);
        CHECK(find_character_set("ISO 2022 IR 127") != nullptr);
        CHECK(find_character_set("ISO 2022 IR 138") != nullptr);
        CHECK(find_character_set("ISO 2022 IR 144") != nullptr);
        CHECK(find_character_set("ISO 2022 IR 166") != nullptr);
    }

    SECTION("Unknown character set returns nullptr") {
        CHECK(find_character_set("UNKNOWN") == nullptr);
        CHECK(find_character_set("ISO_IR 999") == nullptr);
        CHECK(find_character_set("") == nullptr);
    }

    SECTION("Find by ISO-IR number") {
        CHECK(find_character_set_by_ir(6) != nullptr);
        CHECK(find_character_set_by_ir(100) != nullptr);
        CHECK(find_character_set_by_ir(101) != nullptr);
        CHECK(find_character_set_by_ir(126) != nullptr);
        CHECK(find_character_set_by_ir(127) != nullptr);
        CHECK(find_character_set_by_ir(138) != nullptr);
        CHECK(find_character_set_by_ir(144) != nullptr);
        CHECK(find_character_set_by_ir(166) != nullptr);
        CHECK(find_character_set_by_ir(192) != nullptr);
        CHECK(find_character_set_by_ir(149) != nullptr);
        CHECK(find_character_set_by_ir(87) != nullptr);
        CHECK(find_character_set_by_ir(13) != nullptr);
        CHECK(find_character_set_by_ir(58) != nullptr);
        CHECK(find_character_set_by_ir(999) == nullptr);
    }
}

TEST_CASE("Default character set", "[encoding][charset]") {
    const auto& cs = default_character_set();
    CHECK(cs.defined_term == "ISO_IR 6");
    CHECK(cs.encoding_name == "ASCII");
}

TEST_CASE("All character sets enumeration", "[encoding][charset]") {
    auto all = all_character_sets();
    CHECK(all.size() == 22);  // 20 in array + GB2312 + GB18030

    // Verify all entries are non-null
    for (const auto* cs : all) {
        REQUIRE(cs != nullptr);
        CHECK_FALSE(cs->defined_term.empty());
        CHECK_FALSE(cs->encoding_name.empty());
    }
}

// =============================================================================
// Specific Character Set Parsing Tests
// =============================================================================

TEST_CASE("Parse Specific Character Set", "[encoding][charset]") {
    SECTION("Empty value defaults to ASCII") {
        auto scs = parse_specific_character_set("");
        REQUIRE(scs.default_set != nullptr);
        CHECK(scs.default_set->defined_term == "ISO_IR 6");
        CHECK(scs.extension_sets.empty());
        CHECK_FALSE(scs.uses_extensions());
    }

    SECTION("Single value: ISO_IR 100 (Latin-1)") {
        auto scs = parse_specific_character_set("ISO_IR 100");
        REQUIRE(scs.default_set != nullptr);
        CHECK(scs.default_set->defined_term == "ISO_IR 100");
        CHECK(scs.extension_sets.empty());
    }

    SECTION("Single value: ISO_IR 192 (UTF-8)") {
        auto scs = parse_specific_character_set("ISO_IR 192");
        CHECK(scs.is_utf8());
        CHECK_FALSE(scs.uses_extensions());
    }

    SECTION("Multi-valued: empty + Korean") {
        // "\\ISO 2022 IR 149" means: default ASCII + Korean extension
        auto scs = parse_specific_character_set("\\ISO 2022 IR 149");
        REQUIRE(scs.default_set != nullptr);
        CHECK(scs.default_set->defined_term == "ISO_IR 6");  // empty = ASCII
        REQUIRE(scs.extension_sets.size() == 1);
        CHECK(scs.extension_sets[0]->defined_term == "ISO 2022 IR 149");
        CHECK(scs.uses_extensions());
    }

    SECTION("Multi-valued: empty + Japanese Kanji") {
        auto scs = parse_specific_character_set("\\ISO 2022 IR 87");
        CHECK(scs.default_set->defined_term == "ISO_IR 6");
        REQUIRE(scs.extension_sets.size() == 1);
        CHECK(scs.extension_sets[0]->defined_term == "ISO 2022 IR 87");
    }

    SECTION("Multi-valued: empty + Chinese") {
        auto scs = parse_specific_character_set("\\ISO 2022 IR 58");
        REQUIRE(scs.extension_sets.size() == 1);
        CHECK(scs.extension_sets[0]->defined_term == "ISO 2022 IR 58");
    }

    SECTION("Multi-valued: Korean + Japanese (multiple extensions)") {
        auto scs = parse_specific_character_set(
            "\\ISO 2022 IR 149\\ISO 2022 IR 87");
        CHECK(scs.default_set->defined_term == "ISO_IR 6");
        REQUIRE(scs.extension_sets.size() == 2);
        CHECK(scs.extension_sets[0]->defined_term == "ISO 2022 IR 149");
        CHECK(scs.extension_sets[1]->defined_term == "ISO 2022 IR 87");
    }

    SECTION("Unknown extension is skipped") {
        auto scs = parse_specific_character_set("\\UNKNOWN_CHARSET");
        CHECK(scs.default_set->defined_term == "ISO_IR 6");
        CHECK(scs.extension_sets.empty());
    }

    SECTION("is_single_byte_only") {
        auto ascii_only = parse_specific_character_set("ISO_IR 6");
        CHECK(ascii_only.is_single_byte_only());

        auto latin1 = parse_specific_character_set("ISO_IR 100");
        CHECK(latin1.is_single_byte_only());

        auto korean = parse_specific_character_set("\\ISO 2022 IR 149");
        CHECK_FALSE(korean.is_single_byte_only());

        auto utf8 = parse_specific_character_set("ISO_IR 192");
        CHECK_FALSE(utf8.is_single_byte_only());
    }
}

// =============================================================================
// ISO 2022 Escape Sequence Splitting Tests
// =============================================================================

TEST_CASE("Split by escape sequences", "[encoding][charset]") {
    SECTION("No escape sequences returns single segment") {
        auto scs = parse_specific_character_set("ISO_IR 100");
        auto segments = split_by_escape_sequences("Hello World", scs);
        REQUIRE(segments.size() == 1);
        CHECK(segments[0].text == "Hello World");
    }

    SECTION("Empty text returns no segments") {
        auto scs = parse_specific_character_set("\\ISO 2022 IR 149");
        auto segments = split_by_escape_sequences("", scs);
        CHECK(segments.empty());
    }

    SECTION("Text without ESC bytes returns single segment") {
        auto scs = parse_specific_character_set("\\ISO 2022 IR 149");
        auto segments = split_by_escape_sequences("ASCII text", scs);
        REQUIRE(segments.size() == 1);
        CHECK(segments[0].text == "ASCII text");
    }

    SECTION("Korean escape sequence splits text") {
        auto scs = parse_specific_character_set("\\ISO 2022 IR 149");

        // Construct: "ABC" + ESC $ ) C + <korean_bytes> + ESC ( B + "DEF"
        std::string input = "ABC";
        input += std::string("\x1B\x24\x29\x43", 4);  // ESC $ ) C
        input += "\xC8\xAB";  // Korean bytes (홍 in EUC-KR)
        input += std::string("\x1B\x28\x42", 3);      // ESC ( B (return to ASCII)
        input += "DEF";

        auto segments = split_by_escape_sequences(input, scs);
        REQUIRE(segments.size() == 3);

        CHECK(segments[0].text == "ABC");
        CHECK(segments[0].charset->defined_term == "ISO_IR 6");

        // Korean segment
        CHECK(segments[1].text.size() == 2);
        CHECK(segments[1].charset->defined_term == "ISO 2022 IR 149");

        CHECK(segments[2].text == "DEF");
    }
}

// =============================================================================
// String Decoding Tests
// =============================================================================

TEST_CASE("UTF-8 passthrough", "[encoding][charset]") {
    auto scs = parse_specific_character_set("ISO_IR 192");

    std::string utf8_text = "Hello \xED\x99\x8D";  // "Hello 홍" in UTF-8
    auto result = decode_to_utf8(utf8_text, scs);
    CHECK(result == utf8_text);
}

TEST_CASE("ASCII passthrough", "[encoding][charset]") {
    auto scs = parse_specific_character_set("");
    auto result = decode_to_utf8("Hello World", scs);
    CHECK(result == "Hello World");
}

TEST_CASE("Empty string decoding", "[encoding][charset]") {
    auto scs = parse_specific_character_set("\\ISO 2022 IR 149");
    auto result = decode_to_utf8("", scs);
    CHECK(result.empty());
}

TEST_CASE("Latin-1 decoding", "[encoding][charset]") {
    auto scs = parse_specific_character_set("ISO_IR 100");

    // Latin-1: e-acute = 0xE9
    std::string latin1_text = "caf\xE9";
    auto result = decode_to_utf8(latin1_text, scs);

    // UTF-8 encoding of e-acute: 0xC3 0xA9
    CHECK(result == "caf\xC3\xA9");
}

TEST_CASE("Person Name decoding", "[encoding][charset]") {
    SECTION("Simple ASCII PN") {
        auto scs = parse_specific_character_set("");
        auto result = decode_person_name("Smith^John", scs);
        CHECK(result == "Smith^John");
    }

    SECTION("PN with component groups") {
        auto scs = parse_specific_character_set("");
        auto result = decode_person_name(
            "Smith^John=Smith^John=Smith^John", scs);
        CHECK(result == "Smith^John=Smith^John=Smith^John");
    }

    SECTION("Empty PN") {
        auto scs = parse_specific_character_set("");
        auto result = decode_person_name("", scs);
        CHECK(result.empty());
    }
}

TEST_CASE("convert_to_utf8 with ASCII", "[encoding][charset]") {
    const auto& ascii = default_character_set();
    auto result = convert_to_utf8("Hello", ascii);
    CHECK(result == "Hello");
}

TEST_CASE("convert_to_utf8 with empty input", "[encoding][charset]") {
    const auto& ascii = default_character_set();
    auto result = convert_to_utf8("", ascii);
    CHECK(result.empty());
}

// =============================================================================
// Escape Sequence Constant Verification
// =============================================================================

TEST_CASE("Escape sequence format verification", "[encoding][charset]") {
    SECTION("Korean ESC sequence starts with 0x1B") {
        const auto* cs = find_character_set("ISO 2022 IR 149");
        REQUIRE(cs != nullptr);
        REQUIRE(cs->escape_sequence.size() == 4);
        CHECK(cs->escape_sequence[0] == '\x1B');  // ESC
        CHECK(cs->escape_sequence[1] == '\x24');  // $
        CHECK(cs->escape_sequence[2] == '\x29');  // )
        CHECK(cs->escape_sequence[3] == '\x43');  // C
    }

    SECTION("Japanese Kanji ESC sequence") {
        const auto* cs = find_character_set("ISO 2022 IR 87");
        REQUIRE(cs != nullptr);
        REQUIRE(cs->escape_sequence.size() == 3);
        CHECK(cs->escape_sequence[0] == '\x1B');  // ESC
        CHECK(cs->escape_sequence[1] == '\x24');  // $
        CHECK(cs->escape_sequence[2] == '\x42');  // B
    }

    SECTION("Chinese GB2312 ESC sequence") {
        const auto* cs = find_character_set("ISO 2022 IR 58");
        REQUIRE(cs != nullptr);
        REQUIRE(cs->escape_sequence.size() == 4);
        CHECK(cs->escape_sequence[0] == '\x1B');  // ESC
        CHECK(cs->escape_sequence[1] == '\x24');  // $
        CHECK(cs->escape_sequence[2] == '\x29');  // )
        CHECK(cs->escape_sequence[3] == '\x41');  // A
    }

    SECTION("Japanese Katakana ESC sequence") {
        const auto* cs = find_character_set("ISO 2022 IR 13");
        REQUIRE(cs != nullptr);
        REQUIRE(cs->escape_sequence.size() == 3);
        CHECK(cs->escape_sequence[0] == '\x1B');  // ESC
        CHECK(cs->escape_sequence[1] == '\x28');  // (
        CHECK(cs->escape_sequence[2] == '\x4A');  // J
    }
}

// =============================================================================
// Encoding Tests (UTF-8 to target encoding)
// =============================================================================

TEST_CASE("convert_from_utf8 with ASCII", "[encoding][charset]") {
    const auto& ascii = default_character_set();
    auto result = convert_from_utf8("Hello", ascii);
    CHECK(result == "Hello");
}

TEST_CASE("convert_from_utf8 with empty input", "[encoding][charset]") {
    const auto& ascii = default_character_set();
    auto result = convert_from_utf8("", ascii);
    CHECK(result.empty());
}

TEST_CASE("convert_from_utf8 with Latin-1", "[encoding][charset]") {
    const auto* cs = find_character_set("ISO_IR 100");
    REQUIRE(cs != nullptr);

    // UTF-8 e-acute (0xC3 0xA9) should convert to Latin-1 (0xE9)
    auto result = convert_from_utf8("caf\xC3\xA9", *cs);
    CHECK(result == "caf\xE9");
}

TEST_CASE("encode_from_utf8 with UTF-8 passthrough", "[encoding][charset]") {
    auto scs = parse_specific_character_set("ISO_IR 192");
    std::string utf8_text = "Hello \xED\x99\x8D";
    auto result = encode_from_utf8(utf8_text, scs);
    CHECK(result == utf8_text);
}

TEST_CASE("encode_from_utf8 with ASCII-only text", "[encoding][charset]") {
    auto scs = parse_specific_character_set("\\ISO 2022 IR 149");
    auto result = encode_from_utf8("Hello World", scs);
    CHECK(result == "Hello World");
}

TEST_CASE("encode_from_utf8 with empty input", "[encoding][charset]") {
    auto scs = parse_specific_character_set("\\ISO 2022 IR 149");
    auto result = encode_from_utf8("", scs);
    CHECK(result.empty());
}

TEST_CASE("Latin-1 round-trip encode/decode", "[encoding][charset]") {
    auto scs = parse_specific_character_set("ISO_IR 100");

    // Original: Latin-1 bytes for "café"
    std::string latin1_raw = "caf\xE9";

    // Decode to UTF-8
    auto utf8 = decode_to_utf8(latin1_raw, scs);
    CHECK(utf8 == "caf\xC3\xA9");

    // Encode back to Latin-1
    auto encoded = encode_from_utf8(utf8, scs);
    CHECK(encoded == latin1_raw);
}

TEST_CASE("Japanese Kanji decoding", "[encoding][charset]") {
    auto scs = parse_specific_character_set("\\ISO 2022 IR 87");

    // Build raw DICOM string: "Yamada" + ESC $ B + JIS(山田) + ESC ( B + "Taro"
    // 山 in JIS X 0208 = 0x3B 0x33, 田 = 0x45 0x44
    std::string raw_input = "Yamada";
    raw_input += std::string("\x1B\x24\x42", 3);  // ESC $ B
    raw_input += "\x3B\x33\x45\x44";              // 山田 in JIS
    raw_input += std::string("\x1B\x28\x42", 3);  // ESC ( B
    raw_input += "Taro";

    auto utf8 = decode_to_utf8(raw_input, scs);
    // Should contain "Yamada" + UTF-8(山田) + "Taro"
    CHECK(utf8.find("Yamada") == 0);
    CHECK(utf8.find("Taro") != std::string::npos);
    // UTF-8 for 山 = E5 B1 B1, 田 = E7 94 B0
    CHECK(utf8.find("\xE5\xB1\xB1\xE7\x94\xB0") != std::string::npos);
}

TEST_CASE("Chinese GB2312 decoding", "[encoding][charset]") {
    auto scs = parse_specific_character_set("\\ISO 2022 IR 58");

    // Build raw DICOM string: "Wang" + ESC $ ) A + GB2312(王五) + ESC ( B + "Wu"
    // 王 in GB2312 = 0xCD 0xF5, 五 = 0xCE 0xE5
    std::string raw_input = "Wang";
    raw_input += std::string("\x1B\x24\x29\x41", 4);  // ESC $ ) A
    raw_input += "\xCD\xF5\xCE\xE5";                  // 王五 in GB2312
    raw_input += std::string("\x1B\x28\x42", 3);       // ESC ( B
    raw_input += "Wu";

    auto utf8 = decode_to_utf8(raw_input, scs);
    // Should contain "Wang" + UTF-8(王五) + "Wu"
    CHECK(utf8.find("Wang") == 0);
    CHECK(utf8.find("Wu") != std::string::npos);
    // UTF-8 for 王 = E7 8E 8B, 五 = E4 BA 94
    CHECK(utf8.find("\xE7\x8E\x8B\xE4\xBA\x94") != std::string::npos);
}

TEST_CASE("Person Name with mixed character sets", "[encoding][charset]") {
    auto scs = parse_specific_character_set("\\ISO 2022 IR 149");

    // Build PN: "Hong^GilDong" = alphabetic + ESC $ ) C + <EUC-KR 홍^길동> + ESC ( B
    std::string alphabetic = "Hong^GilDong";

    std::string ideographic;
    ideographic += std::string("\x1B\x24\x29\x43", 4);  // ESC $ ) C
    ideographic += "\xC8\xAB";                            // 홍 in EUC-KR
    ideographic += "^";
    ideographic += "\xB1\xE6\xB5\xBF";                   // 길동 in EUC-KR
    ideographic += std::string("\x1B\x28\x42", 3);        // ESC ( B

    std::string pn_raw = alphabetic + "=" + ideographic;

    auto result = decode_person_name(pn_raw, scs);
    // Alphabetic group should be unchanged
    CHECK(result.find("Hong^GilDong") == 0);
    // Should have = separator
    CHECK(result.find('=') != std::string::npos);
    // Ideographic group should contain UTF-8 Korean
    // UTF-8 for 홍 = ED 99 8D
    auto eq_pos = result.find('=');
    REQUIRE(eq_pos != std::string::npos);
    auto ideographic_utf8 = result.substr(eq_pos + 1);
    CHECK_FALSE(ideographic_utf8.empty());
}

TEST_CASE("Korean round-trip encode/decode", "[encoding][charset]") {
    auto scs = parse_specific_character_set("\\ISO 2022 IR 149");

    // Build raw DICOM string: "Hong" + ESC $ ) C + <EUC-KR for 홍> + ESC ( B + "GilDong"
    std::string raw_input = "Hong";
    raw_input += std::string("\x1B\x24\x29\x43", 4);  // ESC $ ) C
    raw_input += "\xC8\xAB";  // 홍 in EUC-KR
    raw_input += std::string("\x1B\x28\x42", 3);       // ESC ( B
    raw_input += "GilDong";

    // Decode to UTF-8
    auto utf8 = decode_to_utf8(raw_input, scs);
    // Should contain "Hong" + UTF-8(홍) + "GilDong"
    CHECK(utf8.find("Hong") == 0);
    CHECK(utf8.find("GilDong") != std::string::npos);

    // Encode back: should produce escape-sequence-bracketed output
    auto re_encoded = encode_from_utf8(utf8, scs);
    // The re-encoded string should contain Korean escape sequence
    CHECK(re_encoded.find("\x1B\x24\x29\x43") != std::string::npos);
    // And ASCII return
    CHECK(re_encoded.find("\x1B\x28\x42") != std::string::npos);

    // Round-trip decode should match original UTF-8
    auto round_trip_utf8 = decode_to_utf8(re_encoded, scs);
    CHECK(round_trip_utf8 == utf8);
}

TEST_CASE("Japanese round-trip encode/decode", "[encoding][charset]") {
    auto scs = parse_specific_character_set("\\ISO 2022 IR 87");

    // UTF-8 text with mixed ASCII and Japanese
    // 山田 in UTF-8: E5 B1 B1 E7 94 B0
    std::string utf8_input = "Yamada\xE5\xB1\xB1\xE7\x94\xB0Taro";

    // Encode to ISO 2022 with JIS escape sequences
    auto encoded = encode_from_utf8(utf8_input, scs);
    // Should contain Japanese Kanji escape sequence ESC $ B
    CHECK(encoded.find("\x1B\x24\x42") != std::string::npos);
    // And ASCII return ESC ( B
    CHECK(encoded.find("\x1B\x28\x42") != std::string::npos);
    // ASCII parts should be preserved
    CHECK(encoded.find("Yamada") == 0);
    CHECK(encoded.find("Taro") != std::string::npos);

    // Decode back to UTF-8: round-trip must produce identical result
    auto round_trip = decode_to_utf8(encoded, scs);
    CHECK(round_trip == utf8_input);
}

TEST_CASE("Chinese round-trip encode/decode", "[encoding][charset]") {
    auto scs = parse_specific_character_set("\\ISO 2022 IR 58");

    // UTF-8 text with mixed ASCII and Chinese
    // 王五 in UTF-8: E7 8E 8B E4 BA 94
    std::string utf8_input = "Wang\xE7\x8E\x8B\xE4\xBA\x94Wu";

    // Encode to ISO 2022 with GB2312 escape sequences
    auto encoded = encode_from_utf8(utf8_input, scs);
    // Should contain Chinese escape sequence ESC $ ) A
    CHECK(encoded.find("\x1B\x24\x29\x41") != std::string::npos);
    // And ASCII return ESC ( B
    CHECK(encoded.find("\x1B\x28\x42") != std::string::npos);

    // Decode back to UTF-8: round-trip must produce identical result
    auto round_trip = decode_to_utf8(encoded, scs);
    CHECK(round_trip == utf8_input);
}

// =============================================================================
// Extended Character Set Encoding/Decoding Tests
// =============================================================================

TEST_CASE("Latin-2 round-trip encode/decode", "[encoding][charset]") {
    auto scs = parse_specific_character_set("ISO_IR 101");

    // Latin-2: č = 0xE8, ž = 0xBE (Czech characters)
    std::string latin2_raw = "Pr\xE1ha \xE8\xE9";  // "Práha čé"

    // Decode to UTF-8
    auto utf8 = decode_to_utf8(latin2_raw, scs);
    // á in ISO-8859-2 = 0xE1 → UTF-8 C3 A1
    // č in ISO-8859-2 = 0xE8 → UTF-8 C4 8D
    // é in ISO-8859-2 = 0xE9 → UTF-8 C3 A9
    CHECK(utf8.find("Pr\xC3\xA1ha") == 0);
    CHECK(utf8.find("\xC4\x8D\xC3\xA9") != std::string::npos);

    // Encode back to Latin-2
    auto encoded = encode_from_utf8(utf8, scs);
    CHECK(encoded == latin2_raw);
}

TEST_CASE("Greek round-trip encode/decode", "[encoding][charset]") {
    auto scs = parse_specific_character_set("ISO_IR 126");

    // Greek: α = 0xE1, β = 0xE2 in ISO-8859-7
    std::string greek_raw = "\xE1\xE2";

    // Decode to UTF-8
    auto utf8 = decode_to_utf8(greek_raw, scs);
    // α (U+03B1) → UTF-8 CE B1
    // β (U+03B2) → UTF-8 CE B2
    CHECK(utf8 == "\xCE\xB1\xCE\xB2");

    // Encode back to Greek
    auto encoded = encode_from_utf8(utf8, scs);
    CHECK(encoded == greek_raw);
}

TEST_CASE("Cyrillic round-trip encode/decode", "[encoding][charset]") {
    auto scs = parse_specific_character_set("ISO_IR 144");

    // Cyrillic: М = 0xBC, и = 0xD8, р = 0xE0 in ISO-8859-5 ("Мир" = Peace)
    std::string cyrillic_raw = "\xBC\xD8\xE0";

    // Decode to UTF-8
    auto utf8 = decode_to_utf8(cyrillic_raw, scs);
    // М (U+041C) → UTF-8 D0 9C
    // и (U+0438) → UTF-8 D0 B8
    // р (U+0440) → UTF-8 D1 80
    CHECK(utf8 == "\xD0\x9C\xD0\xB8\xD1\x80");

    // Encode back to Cyrillic
    auto encoded = encode_from_utf8(utf8, scs);
    CHECK(encoded == cyrillic_raw);
}

TEST_CASE("Arabic decoding", "[encoding][charset]") {
    auto scs = parse_specific_character_set("ISO_IR 127");

    // Arabic: Alef = 0xC7, Beh = 0xC8 in ISO-8859-6
    std::string arabic_raw = "\xC7\xC8";

    // Decode to UTF-8
    auto utf8 = decode_to_utf8(arabic_raw, scs);
    // Alef (U+0627) → UTF-8 D8 A7
    // Beh  (U+0628) → UTF-8 D8 A8
    CHECK(utf8 == "\xD8\xA7\xD8\xA8");

    // Round-trip
    auto encoded = encode_from_utf8(utf8, scs);
    CHECK(encoded == arabic_raw);
}

TEST_CASE("Hebrew decoding", "[encoding][charset]") {
    auto scs = parse_specific_character_set("ISO_IR 138");

    // Hebrew: Alef = 0xE0, Bet = 0xE1 in ISO-8859-8
    std::string hebrew_raw = "\xE0\xE1";

    // Decode to UTF-8
    auto utf8 = decode_to_utf8(hebrew_raw, scs);
    // Alef (U+05D0) → UTF-8 D7 90
    // Bet  (U+05D1) → UTF-8 D7 91
    CHECK(utf8 == "\xD7\x90\xD7\x91");

    // Round-trip
    auto encoded = encode_from_utf8(utf8, scs);
    CHECK(encoded == hebrew_raw);
}

TEST_CASE("Thai decoding", "[encoding][charset]") {
    auto scs = parse_specific_character_set("ISO_IR 166");

    // Thai: Ko Kai = 0xA1, Kho Khai = 0xA2 in TIS-620
    std::string thai_raw = "\xA1\xA2";

    // Decode to UTF-8
    auto utf8 = decode_to_utf8(thai_raw, scs);
    // Ko Kai  (U+0E01) → UTF-8 E0 B8 81
    // Kho Khai (U+0E02) → UTF-8 E0 B8 82
    CHECK(utf8 == "\xE0\xB8\x81\xE0\xB8\x82");

    // Round-trip
    auto encoded = encode_from_utf8(utf8, scs);
    CHECK(encoded == thai_raw);
}

TEST_CASE("GB18030 round-trip encode/decode", "[encoding][charset]") {
    auto scs = parse_specific_character_set("GB18030");

    // GB18030 2-byte encoding: 你好
    // 你 = 0xC4 0xE3 in GB18030
    // 好 = 0xBA 0xC3 in GB18030
    std::string gb18030_raw = "\xC4\xE3\xBA\xC3";

    // Decode to UTF-8
    auto utf8 = decode_to_utf8(gb18030_raw, scs);
    // 你 (U+4F60) → UTF-8 E4 BD A0
    // 好 (U+597D) → UTF-8 E5 A5 BD
    CHECK(utf8 == "\xE4\xBD\xA0\xE5\xA5\xBD");

    // Encode back to GB18030
    auto encoded = encode_from_utf8(utf8, scs);
    CHECK(encoded == gb18030_raw);
}

TEST_CASE("New escape sequence format verification", "[encoding][charset]") {
    SECTION("Latin-2 ESC sequence: ESC - B") {
        const auto* cs = find_character_set("ISO 2022 IR 101");
        REQUIRE(cs != nullptr);
        REQUIRE(cs->escape_sequence.size() == 3);
        CHECK(cs->escape_sequence[0] == '\x1B');  // ESC
        CHECK(cs->escape_sequence[1] == '\x2D');  // -
        CHECK(cs->escape_sequence[2] == '\x42');  // B
    }

    SECTION("Greek ESC sequence: ESC - F") {
        const auto* cs = find_character_set("ISO 2022 IR 126");
        REQUIRE(cs != nullptr);
        REQUIRE(cs->escape_sequence.size() == 3);
        CHECK(cs->escape_sequence[0] == '\x1B');
        CHECK(cs->escape_sequence[1] == '\x2D');
        CHECK(cs->escape_sequence[2] == '\x46');
    }

    SECTION("Arabic ESC sequence: ESC - G") {
        const auto* cs = find_character_set("ISO 2022 IR 127");
        REQUIRE(cs != nullptr);
        REQUIRE(cs->escape_sequence.size() == 3);
        CHECK(cs->escape_sequence[0] == '\x1B');
        CHECK(cs->escape_sequence[1] == '\x2D');
        CHECK(cs->escape_sequence[2] == '\x47');
    }

    SECTION("Hebrew ESC sequence: ESC - H") {
        const auto* cs = find_character_set("ISO 2022 IR 138");
        REQUIRE(cs != nullptr);
        REQUIRE(cs->escape_sequence.size() == 3);
        CHECK(cs->escape_sequence[0] == '\x1B');
        CHECK(cs->escape_sequence[1] == '\x2D');
        CHECK(cs->escape_sequence[2] == '\x48');
    }

    SECTION("Cyrillic ESC sequence: ESC - L") {
        const auto* cs = find_character_set("ISO 2022 IR 144");
        REQUIRE(cs != nullptr);
        REQUIRE(cs->escape_sequence.size() == 3);
        CHECK(cs->escape_sequence[0] == '\x1B');
        CHECK(cs->escape_sequence[1] == '\x2D');
        CHECK(cs->escape_sequence[2] == '\x4C');
    }

    SECTION("Thai ESC sequence: ESC - T") {
        const auto* cs = find_character_set("ISO 2022 IR 166");
        REQUIRE(cs != nullptr);
        REQUIRE(cs->escape_sequence.size() == 3);
        CHECK(cs->escape_sequence[0] == '\x1B');
        CHECK(cs->escape_sequence[1] == '\x2D');
        CHECK(cs->escape_sequence[2] == '\x54');
    }
}

TEST_CASE("Multi-valued specific character set with new charsets", "[encoding][charset]") {
    SECTION("ASCII + Latin-2 extension") {
        auto scs = parse_specific_character_set("\\ISO 2022 IR 101");
        CHECK(scs.default_set->defined_term == "ISO_IR 6");
        REQUIRE(scs.extension_sets.size() == 1);
        CHECK(scs.extension_sets[0]->defined_term == "ISO 2022 IR 101");
        CHECK(scs.uses_extensions());
    }

    SECTION("ASCII + Cyrillic extension") {
        auto scs = parse_specific_character_set("\\ISO 2022 IR 144");
        REQUIRE(scs.extension_sets.size() == 1);
        CHECK(scs.extension_sets[0]->encoding_name == "ISO-8859-5");
    }

    SECTION("New charsets are single-byte only") {
        auto latin2 = parse_specific_character_set("ISO_IR 101");
        CHECK(latin2.is_single_byte_only());

        auto greek = parse_specific_character_set("ISO_IR 126");
        CHECK(greek.is_single_byte_only());

        auto arabic = parse_specific_character_set("ISO_IR 127");
        CHECK(arabic.is_single_byte_only());

        auto hebrew = parse_specific_character_set("ISO_IR 138");
        CHECK(hebrew.is_single_byte_only());

        auto cyrillic = parse_specific_character_set("ISO_IR 144");
        CHECK(cyrillic.is_single_byte_only());

        auto thai = parse_specific_character_set("ISO_IR 166");
        CHECK(thai.is_single_byte_only());
    }

    SECTION("GB18030 is multi-byte") {
        auto gb18030 = parse_specific_character_set("GB18030");
        CHECK_FALSE(gb18030.is_single_byte_only());
    }
}
