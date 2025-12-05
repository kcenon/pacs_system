/**
 * @file dicom_element_test.cpp
 * @brief Unit tests for dicom_element class
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <array>

#include <pacs/core/dicom_element.hpp>
#include <pacs/core/dicom_tag_constants.hpp>

using namespace pacs::core;
using namespace pacs::encoding;

// ============================================================================
// Construction Tests
// ============================================================================

TEST_CASE("dicom_element default construction", "[core][dicom_element]") {
    SECTION("empty element with tag and VR") {
        dicom_element elem{tags::patient_name, vr_type::PN};

        CHECK(elem.tag() == tags::patient_name);
        CHECK(elem.vr() == vr_type::PN);
        CHECK(elem.is_empty());
        CHECK(elem.length() == 0);
    }

    SECTION("element with raw data") {
        std::vector<uint8_t> data = {'T', 'E', 'S', 'T'};
        dicom_element elem{tags::patient_id, vr_type::LO, data};

        CHECK(elem.tag() == tags::patient_id);
        CHECK(elem.vr() == vr_type::LO);
        CHECK_FALSE(elem.is_empty());
        CHECK(elem.length() == 4);
    }
}

// ============================================================================
// String Handling Tests
// ============================================================================

TEST_CASE("dicom_element string handling", "[core][dicom_element]") {
    SECTION("create from string") {
        auto elem = dicom_element::from_string(
            tags::patient_name, vr_type::PN, "DOE^JOHN");

        CHECK(elem.as_string() == "DOE^JOHN");
    }

    SECTION("string with odd length gets padded") {
        auto elem = dicom_element::from_string(
            tags::patient_id, vr_type::LO, "TEST");

        // LO VR should pad to even length with space
        CHECK(elem.length() == 4);  // "TEST" is already even
        CHECK(elem.as_string() == "TEST");

        auto elem_odd = dicom_element::from_string(
            tags::patient_id, vr_type::LO, "ABC");

        CHECK(elem_odd.length() == 4);  // "ABC" + space padding
        CHECK(elem_odd.as_string() == "ABC");  // Padding removed on read
    }

    SECTION("string with existing padding is trimmed") {
        std::vector<uint8_t> data = {'T', 'E', 'S', 'T', ' ', ' '};
        dicom_element elem{tags::patient_id, vr_type::LO, data};

        CHECK(elem.as_string() == "TEST");  // Trailing spaces trimmed
    }

    SECTION("UI VR uses null padding") {
        auto elem = dicom_element::from_string(
            tags::study_instance_uid, vr_type::UI, "1.2.3");

        CHECK(elem.length() == 6);  // "1.2.3" + null padding
        CHECK(elem.as_string() == "1.2.3");
    }

    SECTION("value multiplicity with backslash") {
        auto elem = dicom_element::from_string(
            tags::image_type, vr_type::CS, "ORIGINAL\\PRIMARY\\AXIAL");

        auto values = elem.as_string_list();
        REQUIRE(values.size() == 3);
        CHECK(values[0] == "ORIGINAL");
        CHECK(values[1] == "PRIMARY");
        CHECK(values[2] == "AXIAL");
    }

    SECTION("single value returns list with one element") {
        auto elem = dicom_element::from_string(
            tags::modality, vr_type::CS, "CT");

        auto values = elem.as_string_list();
        REQUIRE(values.size() == 1);
        CHECK(values[0] == "CT");
    }

    SECTION("empty string returns empty list") {
        dicom_element elem{tags::patient_name, vr_type::PN};

        auto values = elem.as_string_list();
        CHECK(values.empty());
    }
}

// ============================================================================
// Numeric Handling Tests
// ============================================================================

TEST_CASE("dicom_element numeric handling", "[core][dicom_element]") {
    SECTION("unsigned short (US)") {
        auto elem = dicom_element::from_numeric<uint16_t>(
            tags::rows, vr_type::US, 512);

        CHECK(elem.as_numeric<uint16_t>() == 512);
        CHECK(elem.length() == 2);
    }

    SECTION("signed short (SS)") {
        auto elem = dicom_element::from_numeric<int16_t>(
            tags::smallest_image_pixel_value, vr_type::SS, -100);

        CHECK(elem.as_numeric<int16_t>() == -100);
    }

    SECTION("unsigned long (UL)") {
        auto elem = dicom_element::from_numeric<uint32_t>(
            dicom_tag{0x0028, 0x0008}, vr_type::UL, 1000000);

        CHECK(elem.as_numeric<uint32_t>() == 1000000);
        CHECK(elem.length() == 4);
    }

    SECTION("float (FL)") {
        auto elem = dicom_element::from_numeric<float>(
            dicom_tag{0x0018, 0x0050}, vr_type::FL, 1.5f);

        CHECK(elem.as_numeric<float>() == Catch::Approx(1.5f));
        CHECK(elem.length() == 4);
    }

    SECTION("double (FD)") {
        auto elem = dicom_element::from_numeric<double>(
            dicom_tag{0x0018, 0x0088}, vr_type::FD, 3.14159265359);

        CHECK(elem.as_numeric<double>() == Catch::Approx(3.14159265359));
        CHECK(elem.length() == 8);
    }

    SECTION("numeric list") {
        std::array<uint16_t, 3> values = {100, 200, 300};
        auto elem = dicom_element::from_numeric_list<uint16_t>(
            dicom_tag{0x0028, 0x0010}, vr_type::US,
            std::span<const uint16_t>{values});

        auto result = elem.as_numeric_list<uint16_t>();
        REQUIRE(result.size() == 3);
        CHECK(result[0] == 100);
        CHECK(result[1] == 200);
        CHECK(result[2] == 300);
    }

    SECTION("numeric as_string conversion") {
        auto elem = dicom_element::from_numeric<uint16_t>(
            tags::rows, vr_type::US, 512);

        CHECK(elem.as_string() == "512");
    }
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_CASE("dicom_element error handling", "[core][dicom_element]") {
    SECTION("numeric conversion with insufficient data throws") {
        std::vector<uint8_t> data = {0x01};  // Only 1 byte
        dicom_element elem{tags::rows, vr_type::US, data};

        CHECK_THROWS_AS(elem.as_numeric<uint16_t>(), value_conversion_error);
    }

    SECTION("numeric list with unaligned data throws") {
        std::vector<uint8_t> data = {0x01, 0x02, 0x03};  // 3 bytes, not divisible by 2
        dicom_element elem{tags::rows, vr_type::US, data};

        CHECK_THROWS_AS(elem.as_numeric_list<uint16_t>(), value_conversion_error);
    }
}

// ============================================================================
// Sequence Tests
// ============================================================================

TEST_CASE("dicom_element sequence handling", "[core][dicom_element]") {
    SECTION("SQ VR is identified as sequence") {
        dicom_element elem{
            tags::scheduled_procedure_step_sequence, vr_type::SQ};

        CHECK(elem.is_sequence());
        CHECK(elem.sequence_items().empty());
    }

    SECTION("non-SQ VR is not a sequence") {
        dicom_element elem{tags::patient_name, vr_type::PN};

        CHECK_FALSE(elem.is_sequence());
    }
}

// ============================================================================
// Modification Tests
// ============================================================================

TEST_CASE("dicom_element modification", "[core][dicom_element]") {
    SECTION("set_value replaces data") {
        dicom_element elem{tags::patient_id, vr_type::LO};
        std::vector<uint8_t> data = {'N', 'E', 'W', ' '};

        elem.set_value(data);

        CHECK(elem.length() == 4);
        CHECK(elem.as_string() == "NEW");
    }

    SECTION("set_string replaces string value") {
        auto elem = dicom_element::from_string(
            tags::patient_name, vr_type::PN, "OLD");

        elem.set_string("NEW^NAME");

        CHECK(elem.as_string() == "NEW^NAME");
    }

    SECTION("set_numeric replaces numeric value") {
        auto elem = dicom_element::from_numeric<uint16_t>(
            tags::rows, vr_type::US, 100);

        elem.set_numeric<uint16_t>(512);

        CHECK(elem.as_numeric<uint16_t>() == 512);
    }
}

// ============================================================================
// Copy/Move Tests
// ============================================================================

TEST_CASE("dicom_element copy and move", "[core][dicom_element]") {
    SECTION("copy construction") {
        auto original = dicom_element::from_string(
            tags::patient_name, vr_type::PN, "DOE^JOHN");

        dicom_element copy{original};

        CHECK(copy.tag() == original.tag());
        CHECK(copy.vr() == original.vr());
        CHECK(copy.as_string() == original.as_string());
    }

    SECTION("move construction") {
        auto original = dicom_element::from_string(
            tags::patient_name, vr_type::PN, "DOE^JOHN");

        dicom_element moved{std::move(original)};

        CHECK(moved.tag() == tags::patient_name);
        CHECK(moved.as_string() == "DOE^JOHN");
    }

    SECTION("copy assignment") {
        auto original = dicom_element::from_string(
            tags::patient_name, vr_type::PN, "DOE^JOHN");
        dicom_element copy{tags::patient_id, vr_type::LO};

        copy = original;

        CHECK(copy.as_string() == "DOE^JOHN");
    }

    SECTION("move assignment") {
        auto original = dicom_element::from_string(
            tags::patient_name, vr_type::PN, "DOE^JOHN");
        dicom_element moved{tags::patient_id, vr_type::LO};

        moved = std::move(original);

        CHECK(moved.as_string() == "DOE^JOHN");
    }
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_CASE("dicom_element edge cases", "[core][dicom_element]") {
    SECTION("empty string value") {
        auto elem = dicom_element::from_string(
            tags::patient_name, vr_type::PN, "");

        CHECK(elem.is_empty());
        CHECK(elem.as_string().empty());
    }

    SECTION("raw_data returns correct span") {
        std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
        dicom_element elem{tags::pixel_data, vr_type::OW, data};

        auto span = elem.raw_data();
        REQUIRE(span.size() == 4);
        CHECK(span[0] == 0x01);
        CHECK(span[1] == 0x02);
        CHECK(span[2] == 0x03);
        CHECK(span[3] == 0x04);
    }

    SECTION("Person Name with component groups") {
        auto elem = dicom_element::from_string(
            tags::patient_name, vr_type::PN,
            "DOE^JOHN^MIDDLE^PREFIX^SUFFIX");

        CHECK(elem.as_string() == "DOE^JOHN^MIDDLE^PREFIX^SUFFIX");
    }
}
