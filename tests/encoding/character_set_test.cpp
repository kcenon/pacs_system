/**
 * @file character_set_test.cpp
 * @brief Unit tests for DICOM character set registry, ISO 2022 parser, and decoder
 */

#include <catch2/catch_test_macros.hpp>

#include "pacs/encoding/character_set.hpp"

#include <cstring>

using namespace pacs::encoding;

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

    SECTION("Unknown character set returns nullptr") {
        CHECK(find_character_set("UNKNOWN") == nullptr);
        CHECK(find_character_set("ISO_IR 999") == nullptr);
        CHECK(find_character_set("") == nullptr);
    }

    SECTION("Find by ISO-IR number") {
        CHECK(find_character_set_by_ir(6) != nullptr);
        CHECK(find_character_set_by_ir(100) != nullptr);
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
    CHECK(all.size() == 9);  // 8 in array + GB2312

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
