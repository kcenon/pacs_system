#include <catch2/catch_test_macros.hpp>

#include "pacs/encoding/vr_info.hpp"

#include <cstring>

using namespace pacs::encoding;

TEST_CASE("vr_info lookup returns correct properties", "[encoding][vr_info]") {
    SECTION("Person Name (PN) properties") {
        const auto& info = get_vr_info(vr_type::PN);
        CHECK(info.type == vr_type::PN);
        CHECK(info.name == "Person Name");
        CHECK(info.max_length == 324);
        CHECK(info.padding_char == ' ');
        CHECK_FALSE(info.is_fixed_length);
        CHECK(info.fixed_size == 0);
    }

    SECTION("Unsigned Short (US) properties") {
        const auto& info = get_vr_info(vr_type::US);
        CHECK(info.type == vr_type::US);
        CHECK(info.name == "Unsigned Short");
        CHECK(info.max_length == 2);
        CHECK(info.padding_char == '\0');
        CHECK(info.is_fixed_length);
        CHECK(info.fixed_size == 2);
    }

    SECTION("Unique Identifier (UI) properties") {
        const auto& info = get_vr_info(vr_type::UI);
        CHECK(info.type == vr_type::UI);
        CHECK(info.name == "Unique Identifier");
        CHECK(info.max_length == 64);
        CHECK(info.padding_char == '\0');  // UI uses null padding
        CHECK_FALSE(info.is_fixed_length);
    }

    SECTION("Date (DA) properties") {
        const auto& info = get_vr_info(vr_type::DA);
        CHECK(info.type == vr_type::DA);
        CHECK(info.name == "Date");
        CHECK(info.max_length == 8);
        CHECK(info.is_fixed_length);
        CHECK(info.fixed_size == 8);
    }

    SECTION("Sequence (SQ) properties") {
        const auto& info = get_vr_info(vr_type::SQ);
        CHECK(info.type == vr_type::SQ);
        CHECK(info.name == "Sequence of Items");
        CHECK_FALSE(info.is_fixed_length);
    }

    SECTION("Attribute Tag (AT) properties") {
        const auto& info = get_vr_info(vr_type::AT);
        CHECK(info.type == vr_type::AT);
        CHECK(info.name == "Attribute Tag");
        CHECK(info.max_length == 4);
        CHECK(info.is_fixed_length);
        CHECK(info.fixed_size == 4);
    }
}

TEST_CASE("validate_string validates Code String (CS)", "[encoding][vr_info]") {
    SECTION("valid CS values") {
        CHECK(validate_string(vr_type::CS, "ORIGINAL"));
        CHECK(validate_string(vr_type::CS, "TYPE_1"));
        CHECK(validate_string(vr_type::CS, "CT"));
        CHECK(validate_string(vr_type::CS, "MR"));
        CHECK(validate_string(vr_type::CS, "TEST 123"));
    }

    SECTION("invalid CS values") {
        CHECK_FALSE(validate_string(vr_type::CS, "lowercase"));
        CHECK_FALSE(validate_string(vr_type::CS, "SPECIAL@CHAR"));
        CHECK_FALSE(validate_string(vr_type::CS, "HYPHEN-ATED"));
        CHECK_FALSE(validate_string(vr_type::CS, "TAB\tHERE"));
    }

    SECTION("CS length validation") {
        CHECK(validate_string(vr_type::CS, "1234567890123456"));  // 16 chars OK
        CHECK_FALSE(validate_string(vr_type::CS, "12345678901234567"));  // 17 chars
    }
}

TEST_CASE("validate_string validates Date (DA)", "[encoding][vr_info]") {
    SECTION("valid DA values") {
        CHECK(validate_string(vr_type::DA, "20250101"));
        CHECK(validate_string(vr_type::DA, "19850615"));
        CHECK(validate_string(vr_type::DA, "20001231"));
    }

    SECTION("invalid DA values") {
        CHECK_FALSE(validate_string(vr_type::DA, "2025-01-01"));  // Wrong format (dashes)
        CHECK_FALSE(validate_string(vr_type::DA, "2025/01/01")); // Wrong format (slashes)
        CHECK_FALSE(validate_string(vr_type::DA, "202501"));      // Too short
        CHECK_FALSE(validate_string(vr_type::DA, "202501011"));   // Too long
    }
}

TEST_CASE("validate_string validates Time (TM)", "[encoding][vr_info]") {
    SECTION("valid TM values") {
        CHECK(validate_string(vr_type::TM, "120000"));
        CHECK(validate_string(vr_type::TM, "235959.999999"));
        CHECK(validate_string(vr_type::TM, "12:30:00"));
    }

    SECTION("invalid TM values") {
        CHECK_FALSE(validate_string(vr_type::TM, "12-30-00"));
        CHECK_FALSE(validate_string(vr_type::TM, "12h30m00s"));
    }
}

TEST_CASE("validate_string validates Unique Identifier (UI)", "[encoding][vr_info]") {
    SECTION("valid UI values") {
        CHECK(validate_string(vr_type::UI, "1.2.840.10008.5.1.4.1.1.2"));
        CHECK(validate_string(vr_type::UI, "1.2.3"));
        CHECK(validate_string(vr_type::UI, "2.16.840.1.113883.3.51.1.1"));
    }

    SECTION("invalid UI values") {
        CHECK_FALSE(validate_string(vr_type::UI, "1.2.840.10008.invalid"));
        CHECK_FALSE(validate_string(vr_type::UI, "1.2.3.4a"));
        CHECK_FALSE(validate_string(vr_type::UI, "uid:1.2.3"));
    }

    SECTION("UI length validation") {
        // 64 chars is maximum
        std::string long_uid(65, '1');
        CHECK_FALSE(validate_string(vr_type::UI, long_uid));

        std::string max_uid(64, '1');
        CHECK(validate_string(vr_type::UI, max_uid));
    }
}

TEST_CASE("validate_string validates Age String (AS)", "[encoding][vr_info]") {
    SECTION("valid AS values") {
        CHECK(validate_string(vr_type::AS, "030Y"));  // 30 years
        CHECK(validate_string(vr_type::AS, "006M"));  // 6 months
        CHECK(validate_string(vr_type::AS, "012W"));  // 12 weeks
        CHECK(validate_string(vr_type::AS, "001D"));  // 1 day
    }

    SECTION("invalid AS values") {
        CHECK_FALSE(validate_string(vr_type::AS, "30Y"));    // Missing leading zeros
        CHECK_FALSE(validate_string(vr_type::AS, "030X"));   // Invalid suffix
        CHECK_FALSE(validate_string(vr_type::AS, "0030Y")); // Too long
        CHECK_FALSE(validate_string(vr_type::AS, "30"));     // Too short
    }
}

TEST_CASE("validate_string validates Decimal String (DS)", "[encoding][vr_info]") {
    SECTION("valid DS values") {
        CHECK(validate_string(vr_type::DS, "123.456"));
        CHECK(validate_string(vr_type::DS, "-123.456"));
        CHECK(validate_string(vr_type::DS, "+1.5E10"));
        CHECK(validate_string(vr_type::DS, "1.5e-10"));
        CHECK(validate_string(vr_type::DS, " 42 "));
    }

    SECTION("invalid DS values") {
        CHECK_FALSE(validate_string(vr_type::DS, "12,345"));   // Comma not allowed
        CHECK_FALSE(validate_string(vr_type::DS, "NaN"));      // Not a number
    }
}

TEST_CASE("validate_string validates Integer String (IS)", "[encoding][vr_info]") {
    SECTION("valid IS values") {
        CHECK(validate_string(vr_type::IS, "12345"));
        CHECK(validate_string(vr_type::IS, "-12345"));
        CHECK(validate_string(vr_type::IS, "+42"));
    }

    SECTION("invalid IS values") {
        CHECK_FALSE(validate_string(vr_type::IS, "12.5"));     // Decimal not allowed
        CHECK_FALSE(validate_string(vr_type::IS, "12 34"));    // Space not allowed
    }
}

TEST_CASE("validate_string validates Long String (LO)", "[encoding][vr_info]") {
    SECTION("valid LO values") {
        CHECK(validate_string(vr_type::LO, "Patient Name"));
        CHECK(validate_string(vr_type::LO, "CT Scanner"));
        CHECK(validate_string(vr_type::LO, "Hospital ABC - Room 123"));
    }

    SECTION("LO length validation") {
        std::string max_lo(64, 'A');
        CHECK(validate_string(vr_type::LO, max_lo));

        std::string too_long(65, 'A');
        CHECK_FALSE(validate_string(vr_type::LO, too_long));
    }
}

TEST_CASE("pad_to_even adds correct padding", "[encoding][vr_info]") {
    SECTION("string VR padding with space") {
        std::vector<uint8_t> odd_data = {'T', 'E', 'S', 'T', 'X'};
        auto padded = pad_to_even(vr_type::LO, odd_data);
        CHECK(padded.size() == 6);
        CHECK(padded.back() == ' ');
    }

    SECTION("even length data not padded") {
        std::vector<uint8_t> even_data = {'T', 'E', 'S', 'T'};
        auto padded = pad_to_even(vr_type::LO, even_data);
        CHECK(padded.size() == 4);
        CHECK(padded == even_data);
    }

    SECTION("UI null padding") {
        std::vector<uint8_t> uid = {'1', '.', '2', '.', '3'};
        auto padded = pad_to_even(vr_type::UI, uid);
        CHECK(padded.size() == 6);
        CHECK(padded.back() == '\0');
    }

    SECTION("binary VR null padding") {
        std::vector<uint8_t> data = {0x01, 0x02, 0x03};
        auto padded = pad_to_even(vr_type::OB, data);
        CHECK(padded.size() == 4);
        CHECK(padded.back() == '\0');
    }

    SECTION("empty data remains empty") {
        std::vector<uint8_t> empty_data;
        auto padded = pad_to_even(vr_type::LO, empty_data);
        CHECK(padded.empty());
    }
}

TEST_CASE("trim_padding removes trailing padding", "[encoding][vr_info]") {
    SECTION("trim trailing spaces for string VRs") {
        CHECK(trim_padding(vr_type::LO, "TEST   ") == "TEST");
        CHECK(trim_padding(vr_type::PN, "DOE^JOHN   ") == "DOE^JOHN");
        CHECK(trim_padding(vr_type::SH, "   TRIM   ") == "   TRIM");  // Only trailing
    }

    SECTION("trim trailing nulls for UI") {
        std::string ui_with_null = "1.2.3";
        ui_with_null += '\0';
        ui_with_null += '\0';
        CHECK(trim_padding(vr_type::UI, ui_with_null) == "1.2.3");
    }

    SECTION("empty string returns empty") {
        CHECK(trim_padding(vr_type::LO, "") == "");
    }

    SECTION("all padding returns empty") {
        CHECK(trim_padding(vr_type::LO, "    ") == "");
    }
}

TEST_CASE("is_valid_charset validates character sets", "[encoding][vr_info]") {
    SECTION("CS charset") {
        CHECK(is_valid_charset(vr_type::CS, "VALID_CODE"));
        CHECK_FALSE(is_valid_charset(vr_type::CS, "invalid"));
    }

    SECTION("UI charset") {
        CHECK(is_valid_charset(vr_type::UI, "1.2.840.10008"));
        CHECK_FALSE(is_valid_charset(vr_type::UI, "1.2.3.abc"));
    }

    SECTION("Text VRs allow control characters") {
        CHECK(is_valid_charset(vr_type::LT, "Line1\nLine2"));
        CHECK(is_valid_charset(vr_type::ST, "Tab\there"));
    }
}

TEST_CASE("validate_value validates binary data", "[encoding][vr_info]") {
    SECTION("valid US value") {
        std::vector<uint8_t> us_data = {0x01, 0x00};  // 2 bytes
        CHECK(validate_value(vr_type::US, us_data));

        // Multiple values (VM > 1)
        std::vector<uint8_t> multi_us = {0x01, 0x00, 0x02, 0x00};  // 4 bytes
        CHECK(validate_value(vr_type::US, multi_us));
    }

    SECTION("invalid US value - odd size") {
        std::vector<uint8_t> invalid_us = {0x01, 0x00, 0x02};  // 3 bytes
        CHECK_FALSE(validate_value(vr_type::US, invalid_us));
    }

    SECTION("valid string value as bytes") {
        std::vector<uint8_t> cs_data = {'O', 'R', 'I', 'G', 'I', 'N', 'A', 'L'};
        CHECK(validate_value(vr_type::CS, cs_data));
    }

    SECTION("invalid string value as bytes") {
        std::vector<uint8_t> invalid_cs = {'l', 'o', 'w', 'e', 'r'};  // lowercase
        CHECK_FALSE(validate_value(vr_type::CS, invalid_cs));
    }
}
