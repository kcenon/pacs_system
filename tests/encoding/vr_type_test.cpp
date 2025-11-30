#include <catch2/catch_test_macros.hpp>

#include "pacs/encoding/vr_type.hpp"

using namespace pacs::encoding;

TEST_CASE("vr_type string conversion", "[encoding][vr_type]") {
    SECTION("to_string returns correct 2-character code") {
        CHECK(to_string(vr_type::PN) == "PN");
        CHECK(to_string(vr_type::US) == "US");
        CHECK(to_string(vr_type::SQ) == "SQ");
        CHECK(to_string(vr_type::OB) == "OB");
        CHECK(to_string(vr_type::UI) == "UI");
        CHECK(to_string(vr_type::DA) == "DA");
        CHECK(to_string(vr_type::TM) == "TM");
        CHECK(to_string(vr_type::LO) == "LO");
        CHECK(to_string(vr_type::SH) == "SH");
    }

    SECTION("from_string parses valid VR codes") {
        CHECK(from_string("PN") == vr_type::PN);
        CHECK(from_string("US") == vr_type::US);
        CHECK(from_string("SQ") == vr_type::SQ);
        CHECK(from_string("OB") == vr_type::OB);
        CHECK(from_string("UI") == vr_type::UI);
        CHECK(from_string("DA") == vr_type::DA);
    }

    SECTION("from_string returns nullopt for invalid codes") {
        CHECK(from_string("XX") == std::nullopt);
        CHECK(from_string("ZZ") == std::nullopt);
        CHECK(from_string("99") == std::nullopt);
        CHECK(from_string("ab") == std::nullopt);  // lowercase not valid
    }

    SECTION("from_string returns nullopt for wrong length") {
        CHECK(from_string("") == std::nullopt);
        CHECK(from_string("P") == std::nullopt);
        CHECK(from_string("PNN") == std::nullopt);
        CHECK(from_string("PNXX") == std::nullopt);
    }

    SECTION("round-trip conversion") {
        CHECK(from_string(to_string(vr_type::PN)) == vr_type::PN);
        CHECK(from_string(to_string(vr_type::US)) == vr_type::US);
        CHECK(from_string(to_string(vr_type::SQ)) == vr_type::SQ);
        CHECK(from_string(to_string(vr_type::OW)) == vr_type::OW);
    }
}

TEST_CASE("vr_type enum values match ASCII encoding", "[encoding][vr_type]") {
    SECTION("VR enum values are two ASCII characters packed into uint16_t") {
        // Verify big-endian packing: first char is high byte, second is low byte
        CHECK(static_cast<uint16_t>(vr_type::PN) == 0x504E);  // 'P'=0x50, 'N'=0x4E
        CHECK(static_cast<uint16_t>(vr_type::US) == 0x5553);  // 'U'=0x55, 'S'=0x53
        CHECK(static_cast<uint16_t>(vr_type::AE) == 0x4145);  // 'A'=0x41, 'E'=0x45
        CHECK(static_cast<uint16_t>(vr_type::SQ) == 0x5351);  // 'S'=0x53, 'Q'=0x51
        CHECK(static_cast<uint16_t>(vr_type::OB) == 0x4F42);  // 'O'=0x4F, 'B'=0x42
    }
}

TEST_CASE("vr_type categories - string VRs", "[encoding][vr_type]") {
    SECTION("is_string_vr returns true for string VRs") {
        CHECK(is_string_vr(vr_type::AE));
        CHECK(is_string_vr(vr_type::AS));
        CHECK(is_string_vr(vr_type::CS));
        CHECK(is_string_vr(vr_type::DA));
        CHECK(is_string_vr(vr_type::DS));
        CHECK(is_string_vr(vr_type::DT));
        CHECK(is_string_vr(vr_type::IS));
        CHECK(is_string_vr(vr_type::LO));
        CHECK(is_string_vr(vr_type::LT));
        CHECK(is_string_vr(vr_type::PN));
        CHECK(is_string_vr(vr_type::SH));
        CHECK(is_string_vr(vr_type::ST));
        CHECK(is_string_vr(vr_type::TM));
        CHECK(is_string_vr(vr_type::UC));
        CHECK(is_string_vr(vr_type::UI));
        CHECK(is_string_vr(vr_type::UR));
        CHECK(is_string_vr(vr_type::UT));
    }

    SECTION("is_string_vr returns false for non-string VRs") {
        CHECK_FALSE(is_string_vr(vr_type::US));
        CHECK_FALSE(is_string_vr(vr_type::UL));
        CHECK_FALSE(is_string_vr(vr_type::SS));
        CHECK_FALSE(is_string_vr(vr_type::SL));
        CHECK_FALSE(is_string_vr(vr_type::FL));
        CHECK_FALSE(is_string_vr(vr_type::FD));
        CHECK_FALSE(is_string_vr(vr_type::OB));
        CHECK_FALSE(is_string_vr(vr_type::OW));
        CHECK_FALSE(is_string_vr(vr_type::SQ));
        CHECK_FALSE(is_string_vr(vr_type::AT));
    }
}

TEST_CASE("vr_type categories - binary VRs", "[encoding][vr_type]") {
    SECTION("is_binary_vr returns true for binary VRs") {
        CHECK(is_binary_vr(vr_type::OB));
        CHECK(is_binary_vr(vr_type::OD));
        CHECK(is_binary_vr(vr_type::OF));
        CHECK(is_binary_vr(vr_type::OL));
        CHECK(is_binary_vr(vr_type::OV));
        CHECK(is_binary_vr(vr_type::OW));
        CHECK(is_binary_vr(vr_type::UN));
    }

    SECTION("is_binary_vr returns false for non-binary VRs") {
        CHECK_FALSE(is_binary_vr(vr_type::CS));
        CHECK_FALSE(is_binary_vr(vr_type::PN));
        CHECK_FALSE(is_binary_vr(vr_type::US));
        CHECK_FALSE(is_binary_vr(vr_type::SQ));
        CHECK_FALSE(is_binary_vr(vr_type::AT));
    }
}

TEST_CASE("vr_type categories - numeric VRs", "[encoding][vr_type]") {
    SECTION("is_numeric_vr returns true for numeric VRs") {
        CHECK(is_numeric_vr(vr_type::FL));
        CHECK(is_numeric_vr(vr_type::FD));
        CHECK(is_numeric_vr(vr_type::SL));
        CHECK(is_numeric_vr(vr_type::SS));
        CHECK(is_numeric_vr(vr_type::SV));
        CHECK(is_numeric_vr(vr_type::UL));
        CHECK(is_numeric_vr(vr_type::US));
        CHECK(is_numeric_vr(vr_type::UV));
    }

    SECTION("is_numeric_vr returns false for non-numeric VRs") {
        CHECK_FALSE(is_numeric_vr(vr_type::PN));
        CHECK_FALSE(is_numeric_vr(vr_type::OB));
        CHECK_FALSE(is_numeric_vr(vr_type::SQ));
        CHECK_FALSE(is_numeric_vr(vr_type::DS));  // Decimal String is a string, not binary
        CHECK_FALSE(is_numeric_vr(vr_type::IS));  // Integer String is a string
    }
}

TEST_CASE("vr_type 32-bit length in Explicit VR", "[encoding][vr_type]") {
    SECTION("has_explicit_32bit_length returns true for extended VRs") {
        CHECK(has_explicit_32bit_length(vr_type::OB));
        CHECK(has_explicit_32bit_length(vr_type::OD));
        CHECK(has_explicit_32bit_length(vr_type::OF));
        CHECK(has_explicit_32bit_length(vr_type::OL));
        CHECK(has_explicit_32bit_length(vr_type::OV));
        CHECK(has_explicit_32bit_length(vr_type::OW));
        CHECK(has_explicit_32bit_length(vr_type::SQ));
        CHECK(has_explicit_32bit_length(vr_type::SV));
        CHECK(has_explicit_32bit_length(vr_type::UC));
        CHECK(has_explicit_32bit_length(vr_type::UN));
        CHECK(has_explicit_32bit_length(vr_type::UR));
        CHECK(has_explicit_32bit_length(vr_type::UT));
        CHECK(has_explicit_32bit_length(vr_type::UV));
    }

    SECTION("has_explicit_32bit_length returns false for 16-bit length VRs") {
        CHECK_FALSE(has_explicit_32bit_length(vr_type::AE));
        CHECK_FALSE(has_explicit_32bit_length(vr_type::AS));
        CHECK_FALSE(has_explicit_32bit_length(vr_type::AT));
        CHECK_FALSE(has_explicit_32bit_length(vr_type::CS));
        CHECK_FALSE(has_explicit_32bit_length(vr_type::DA));
        CHECK_FALSE(has_explicit_32bit_length(vr_type::DS));
        CHECK_FALSE(has_explicit_32bit_length(vr_type::DT));
        CHECK_FALSE(has_explicit_32bit_length(vr_type::FL));
        CHECK_FALSE(has_explicit_32bit_length(vr_type::FD));
        CHECK_FALSE(has_explicit_32bit_length(vr_type::IS));
        CHECK_FALSE(has_explicit_32bit_length(vr_type::LO));
        CHECK_FALSE(has_explicit_32bit_length(vr_type::LT));
        CHECK_FALSE(has_explicit_32bit_length(vr_type::PN));
        CHECK_FALSE(has_explicit_32bit_length(vr_type::SH));
        CHECK_FALSE(has_explicit_32bit_length(vr_type::SL));
        CHECK_FALSE(has_explicit_32bit_length(vr_type::SS));
        CHECK_FALSE(has_explicit_32bit_length(vr_type::ST));
        CHECK_FALSE(has_explicit_32bit_length(vr_type::TM));
        CHECK_FALSE(has_explicit_32bit_length(vr_type::UI));
        CHECK_FALSE(has_explicit_32bit_length(vr_type::UL));
        CHECK_FALSE(has_explicit_32bit_length(vr_type::US));
    }
}

TEST_CASE("vr_type fixed length", "[encoding][vr_type]") {
    SECTION("fixed_length returns correct size for fixed-length VRs") {
        CHECK(fixed_length(vr_type::AT) == 4);
        CHECK(fixed_length(vr_type::FL) == 4);
        CHECK(fixed_length(vr_type::FD) == 8);
        CHECK(fixed_length(vr_type::SL) == 4);
        CHECK(fixed_length(vr_type::SS) == 2);
        CHECK(fixed_length(vr_type::SV) == 8);
        CHECK(fixed_length(vr_type::UL) == 4);
        CHECK(fixed_length(vr_type::US) == 2);
        CHECK(fixed_length(vr_type::UV) == 8);
    }

    SECTION("fixed_length returns 0 for variable-length VRs") {
        CHECK(fixed_length(vr_type::PN) == 0);
        CHECK(fixed_length(vr_type::LO) == 0);
        CHECK(fixed_length(vr_type::OB) == 0);
        CHECK(fixed_length(vr_type::SQ) == 0);
        CHECK(fixed_length(vr_type::UI) == 0);
    }

    SECTION("is_fixed_length matches fixed_length") {
        CHECK(is_fixed_length(vr_type::US));
        CHECK(is_fixed_length(vr_type::UL));
        CHECK(is_fixed_length(vr_type::FL));
        CHECK(is_fixed_length(vr_type::FD));
        CHECK(is_fixed_length(vr_type::AT));
        CHECK_FALSE(is_fixed_length(vr_type::PN));
        CHECK_FALSE(is_fixed_length(vr_type::LO));
        CHECK_FALSE(is_fixed_length(vr_type::OB));
    }
}

TEST_CASE("vr_type padding character", "[encoding][vr_type]") {
    SECTION("UI uses null padding") {
        CHECK(padding_char(vr_type::UI) == '\0');
    }

    SECTION("String VRs use space padding") {
        CHECK(padding_char(vr_type::PN) == ' ');
        CHECK(padding_char(vr_type::LO) == ' ');
        CHECK(padding_char(vr_type::SH) == ' ');
        CHECK(padding_char(vr_type::CS) == ' ');
        CHECK(padding_char(vr_type::AE) == ' ');
        CHECK(padding_char(vr_type::DA) == ' ');
        CHECK(padding_char(vr_type::TM) == ' ');
    }

    SECTION("Binary VRs use null padding") {
        CHECK(padding_char(vr_type::OB) == '\0');
        CHECK(padding_char(vr_type::OW) == '\0');
        CHECK(padding_char(vr_type::US) == '\0');
        CHECK(padding_char(vr_type::SQ) == '\0');
    }
}

TEST_CASE("vr_type constexpr evaluation", "[encoding][vr_type]") {
    SECTION("Functions are constexpr") {
        // These should all be evaluated at compile time
        static_assert(to_string(vr_type::PN) == "PN");
        static_assert(from_string("PN") == vr_type::PN);
        static_assert(is_string_vr(vr_type::PN));
        static_assert(!is_string_vr(vr_type::US));
        static_assert(is_binary_vr(vr_type::OB));
        static_assert(is_numeric_vr(vr_type::US));
        static_assert(has_explicit_32bit_length(vr_type::SQ));
        static_assert(!has_explicit_32bit_length(vr_type::US));
        static_assert(fixed_length(vr_type::US) == 2);
        static_assert(is_fixed_length(vr_type::US));
        static_assert(padding_char(vr_type::UI) == '\0');
        static_assert(padding_char(vr_type::PN) == ' ');

        // If we get here, all constexpr assertions passed
        CHECK(true);
    }
}

TEST_CASE("vr_type all VRs coverage", "[encoding][vr_type]") {
    SECTION("All 31 VRs can be converted to string and back") {
        const vr_type all_vrs[] = {
            vr_type::AE, vr_type::AS, vr_type::AT, vr_type::CS,
            vr_type::DA, vr_type::DS, vr_type::DT, vr_type::FD,
            vr_type::FL, vr_type::IS, vr_type::LO, vr_type::LT,
            vr_type::OB, vr_type::OD, vr_type::OF, vr_type::OL,
            vr_type::OV, vr_type::OW, vr_type::PN, vr_type::SH,
            vr_type::SL, vr_type::SQ, vr_type::SS, vr_type::ST,
            vr_type::SV, vr_type::TM, vr_type::UC, vr_type::UI,
            vr_type::UL, vr_type::UN, vr_type::UR, vr_type::US,
            vr_type::UT, vr_type::UV
        };

        for (const auto vr : all_vrs) {
            auto str = to_string(vr);
            INFO("Testing VR: " << str);
            CHECK(str.size() == 2);
            CHECK(str != "??");

            auto parsed = from_string(str);
            REQUIRE(parsed.has_value());
            CHECK(*parsed == vr);
        }
    }

    SECTION("Each VR belongs to exactly one primary category") {
        const vr_type all_vrs[] = {
            vr_type::AE, vr_type::AS, vr_type::AT, vr_type::CS,
            vr_type::DA, vr_type::DS, vr_type::DT, vr_type::FD,
            vr_type::FL, vr_type::IS, vr_type::LO, vr_type::LT,
            vr_type::OB, vr_type::OD, vr_type::OF, vr_type::OL,
            vr_type::OV, vr_type::OW, vr_type::PN, vr_type::SH,
            vr_type::SL, vr_type::SQ, vr_type::SS, vr_type::ST,
            vr_type::SV, vr_type::TM, vr_type::UC, vr_type::UI,
            vr_type::UL, vr_type::UN, vr_type::UR, vr_type::US,
            vr_type::UT, vr_type::UV
        };

        for (const auto vr : all_vrs) {
            int category_count = 0;
            if (is_string_vr(vr)) category_count++;
            if (is_binary_vr(vr)) category_count++;
            if (is_numeric_vr(vr)) category_count++;

            // AT and SQ are special cases - they don't belong to any of the three
            if (vr == vr_type::AT || vr == vr_type::SQ) {
                INFO("Testing special VR: " << to_string(vr));
                CHECK(category_count == 0);
            } else {
                INFO("Testing VR: " << to_string(vr));
                CHECK(category_count == 1);
            }
        }
    }
}
