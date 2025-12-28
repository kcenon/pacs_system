/**
 * @file dicom_element_test.cpp
 * @brief Unit tests for dicom_element class
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <array>

#include <pacs/core/dicom_dataset.hpp>
#include <pacs/core/dicom_element.hpp>
#include <pacs/core/dicom_tag_constants.hpp>
#include <pacs/core/result.hpp>

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

        auto result = elem.as_string();
        REQUIRE(result.is_ok());
        CHECK(result.value() == "DOE^JOHN");
    }

    SECTION("string with odd length gets padded") {
        auto elem = dicom_element::from_string(
            tags::patient_id, vr_type::LO, "TEST");

        // LO VR should pad to even length with space
        CHECK(elem.length() == 4);  // "TEST" is already even
        auto result = elem.as_string();
        REQUIRE(result.is_ok());
        CHECK(result.value() == "TEST");

        auto elem_odd = dicom_element::from_string(
            tags::patient_id, vr_type::LO, "ABC");

        CHECK(elem_odd.length() == 4);  // "ABC" + space padding
        auto result_odd = elem_odd.as_string();
        REQUIRE(result_odd.is_ok());
        CHECK(result_odd.value() == "ABC");  // Padding removed on read
    }

    SECTION("string with existing padding is trimmed") {
        std::vector<uint8_t> data = {'T', 'E', 'S', 'T', ' ', ' '};
        dicom_element elem{tags::patient_id, vr_type::LO, data};

        auto result = elem.as_string();
        REQUIRE(result.is_ok());
        CHECK(result.value() == "TEST");  // Trailing spaces trimmed
    }

    SECTION("UI VR uses null padding") {
        auto elem = dicom_element::from_string(
            tags::study_instance_uid, vr_type::UI, "1.2.3");

        CHECK(elem.length() == 6);  // "1.2.3" + null padding
        auto result = elem.as_string();
        REQUIRE(result.is_ok());
        CHECK(result.value() == "1.2.3");
    }

    SECTION("value multiplicity with backslash") {
        auto elem = dicom_element::from_string(
            tags::image_type, vr_type::CS, "ORIGINAL\\PRIMARY\\AXIAL");

        auto result = elem.as_string_list();
        REQUIRE(result.is_ok());
        auto values = result.value();
        REQUIRE(values.size() == 3);
        CHECK(values[0] == "ORIGINAL");
        CHECK(values[1] == "PRIMARY");
        CHECK(values[2] == "AXIAL");
    }

    SECTION("single value returns list with one element") {
        auto elem = dicom_element::from_string(
            tags::modality, vr_type::CS, "CT");

        auto result = elem.as_string_list();
        REQUIRE(result.is_ok());
        auto values = result.value();
        REQUIRE(values.size() == 1);
        CHECK(values[0] == "CT");
    }

    SECTION("empty string returns empty list") {
        dicom_element elem{tags::patient_name, vr_type::PN};

        auto result = elem.as_string_list();
        REQUIRE(result.is_ok());
        CHECK(result.value().empty());
    }
}

// ============================================================================
// Numeric Handling Tests
// ============================================================================

TEST_CASE("dicom_element numeric handling", "[core][dicom_element]") {
    SECTION("unsigned short (US)") {
        auto elem = dicom_element::from_numeric<uint16_t>(
            tags::rows, vr_type::US, 512);

        auto result = elem.as_numeric<uint16_t>();
        REQUIRE(result.is_ok());
        CHECK(result.value() == 512);
        CHECK(elem.length() == 2);
    }

    SECTION("signed short (SS)") {
        auto elem = dicom_element::from_numeric<int16_t>(
            tags::smallest_image_pixel_value, vr_type::SS, -100);

        auto result = elem.as_numeric<int16_t>();
        REQUIRE(result.is_ok());
        CHECK(result.value() == -100);
    }

    SECTION("unsigned long (UL)") {
        auto elem = dicom_element::from_numeric<uint32_t>(
            dicom_tag{0x0028, 0x0008}, vr_type::UL, 1000000);

        auto result = elem.as_numeric<uint32_t>();
        REQUIRE(result.is_ok());
        CHECK(result.value() == 1000000);
        CHECK(elem.length() == 4);
    }

    SECTION("float (FL)") {
        auto elem = dicom_element::from_numeric<float>(
            dicom_tag{0x0018, 0x0050}, vr_type::FL, 1.5f);

        auto result = elem.as_numeric<float>();
        REQUIRE(result.is_ok());
        CHECK(result.value() == Catch::Approx(1.5f));
        CHECK(elem.length() == 4);
    }

    SECTION("double (FD)") {
        auto elem = dicom_element::from_numeric<double>(
            dicom_tag{0x0018, 0x0088}, vr_type::FD, 3.14159265359);

        auto result = elem.as_numeric<double>();
        REQUIRE(result.is_ok());
        CHECK(result.value() == Catch::Approx(3.14159265359));
        CHECK(elem.length() == 8);
    }

    SECTION("numeric list") {
        std::array<uint16_t, 3> values = {100, 200, 300};
        auto elem = dicom_element::from_numeric_list<uint16_t>(
            dicom_tag{0x0028, 0x0010}, vr_type::US,
            std::span<const uint16_t>{values});

        auto result = elem.as_numeric_list<uint16_t>();
        REQUIRE(result.is_ok());
        auto list = result.value();
        REQUIRE(list.size() == 3);
        CHECK(list[0] == 100);
        CHECK(list[1] == 200);
        CHECK(list[2] == 300);
    }

    SECTION("numeric as_string conversion") {
        auto elem = dicom_element::from_numeric<uint16_t>(
            tags::rows, vr_type::US, 512);

        auto result = elem.as_string();
        REQUIRE(result.is_ok());
        CHECK(result.value() == "512");
    }
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_CASE("dicom_element error handling", "[core][dicom_element]") {
    SECTION("numeric conversion with insufficient data returns error") {
        std::vector<uint8_t> data = {0x01};  // Only 1 byte
        dicom_element elem{tags::rows, vr_type::US, data};

        auto result = elem.as_numeric<uint16_t>();
        CHECK(result.is_err());
        CHECK(result.error().code == pacs::error_codes::data_size_mismatch);
    }

    SECTION("numeric list with unaligned data returns error") {
        std::vector<uint8_t> data = {0x01, 0x02, 0x03};  // 3 bytes, not divisible by 2
        dicom_element elem{tags::rows, vr_type::US, data};

        auto result = elem.as_numeric_list<uint16_t>();
        CHECK(result.is_err());
        CHECK(result.error().code == pacs::error_codes::data_size_mismatch);
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
        CHECK(elem.sequence_item_count() == 0);
    }

    SECTION("non-SQ VR is not a sequence") {
        dicom_element elem{tags::patient_name, vr_type::PN};

        CHECK_FALSE(elem.is_sequence());
    }

    SECTION("add_sequence_item adds items to sequence") {
        dicom_element elem{
            tags::scheduled_procedure_step_sequence, vr_type::SQ};

        dicom_dataset item1;
        item1.set_string(tags::modality, vr_type::CS, "CT");

        dicom_dataset item2;
        item2.set_string(tags::modality, vr_type::CS, "MR");

        elem.add_sequence_item(item1);
        elem.add_sequence_item(std::move(item2));

        CHECK(elem.sequence_item_count() == 2);
        CHECK(elem.sequence_items().size() == 2);
    }

    SECTION("sequence_item returns item at index") {
        dicom_element elem{
            tags::scheduled_procedure_step_sequence, vr_type::SQ};

        dicom_dataset item1;
        item1.set_string(tags::modality, vr_type::CS, "CT");
        item1.set_string(tags::station_name, vr_type::SH, "STATION1");

        dicom_dataset item2;
        item2.set_string(tags::modality, vr_type::CS, "MR");
        item2.set_string(tags::station_name, vr_type::SH, "STATION2");

        elem.add_sequence_item(item1);
        elem.add_sequence_item(item2);

        const auto& first_item = elem.sequence_item(0);
        CHECK(first_item.get_string(tags::modality) == "CT");
        CHECK(first_item.get_string(tags::station_name) == "STATION1");

        const auto& second_item = elem.sequence_item(1);
        CHECK(second_item.get_string(tags::modality) == "MR");
        CHECK(second_item.get_string(tags::station_name) == "STATION2");
    }

    SECTION("sequence_item throws on out of range index") {
        dicom_element elem{
            tags::scheduled_procedure_step_sequence, vr_type::SQ};

        CHECK_THROWS_AS(elem.sequence_item(0), std::out_of_range);

        dicom_dataset item;
        elem.add_sequence_item(item);

        CHECK_NOTHROW(elem.sequence_item(0));
        CHECK_THROWS_AS(elem.sequence_item(1), std::out_of_range);
    }

    SECTION("mutable sequence_item allows modification") {
        dicom_element elem{
            tags::scheduled_procedure_step_sequence, vr_type::SQ};

        dicom_dataset item;
        item.set_string(tags::modality, vr_type::CS, "CT");
        elem.add_sequence_item(item);

        // Modify through mutable access
        elem.sequence_item(0).set_string(tags::modality, vr_type::CS, "MR");

        CHECK(elem.sequence_item(0).get_string(tags::modality) == "MR");
    }

    SECTION("sequence supports range-based for loop") {
        dicom_element elem{
            tags::scheduled_procedure_step_sequence, vr_type::SQ};

        for (int i = 0; i < 3; ++i) {
            dicom_dataset item;
            item.set_string(tags::modality, vr_type::CS, std::to_string(i));
            elem.add_sequence_item(item);
        }

        int count = 0;
        for (const auto& item : elem.sequence_items()) {
            CHECK(item.get_string(tags::modality) == std::to_string(count));
            ++count;
        }
        CHECK(count == 3);
    }

    SECTION("nested sequences are supported") {
        // Outer sequence
        dicom_element outer_seq{
            tags::scheduled_procedure_step_sequence, vr_type::SQ};

        // Create item with nested sequence
        dicom_dataset outer_item;
        outer_item.set_string(tags::modality, vr_type::CS, "CT");

        // Create nested sequence element
        dicom_element inner_seq{
            dicom_tag{0x0040, 0x0321}, vr_type::SQ};  // Referenced SOP Sequence

        dicom_dataset inner_item;
        inner_item.set_string(tags::sop_class_uid, vr_type::UI, "1.2.3.4");
        inner_seq.add_sequence_item(inner_item);

        outer_item.insert(std::move(inner_seq));
        outer_seq.add_sequence_item(outer_item);

        // Verify nested structure
        CHECK(outer_seq.sequence_item_count() == 1);

        const auto& retrieved_outer = outer_seq.sequence_item(0);
        CHECK(retrieved_outer.get_string(tags::modality) == "CT");

        const auto* inner = retrieved_outer.get_sequence(dicom_tag{0x0040, 0x0321});
        REQUIRE(inner != nullptr);
        REQUIRE(inner->size() == 1);
        CHECK((*inner)[0].get_string(tags::sop_class_uid) == "1.2.3.4");
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
        auto result = elem.as_string();
        REQUIRE(result.is_ok());
        CHECK(result.value() == "NEW");
    }

    SECTION("set_string replaces string value") {
        auto elem = dicom_element::from_string(
            tags::patient_name, vr_type::PN, "OLD");

        elem.set_string("NEW^NAME");

        auto result = elem.as_string();
        REQUIRE(result.is_ok());
        CHECK(result.value() == "NEW^NAME");
    }

    SECTION("set_numeric replaces numeric value") {
        auto elem = dicom_element::from_numeric<uint16_t>(
            tags::rows, vr_type::US, 100);

        elem.set_numeric<uint16_t>(512);

        auto result = elem.as_numeric<uint16_t>();
        REQUIRE(result.is_ok());
        CHECK(result.value() == 512);
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
        auto copy_result = copy.as_string();
        auto orig_result = original.as_string();
        REQUIRE(copy_result.is_ok());
        REQUIRE(orig_result.is_ok());
        CHECK(copy_result.value() == orig_result.value());
    }

    SECTION("move construction") {
        auto original = dicom_element::from_string(
            tags::patient_name, vr_type::PN, "DOE^JOHN");

        dicom_element moved{std::move(original)};

        CHECK(moved.tag() == tags::patient_name);
        auto result = moved.as_string();
        REQUIRE(result.is_ok());
        CHECK(result.value() == "DOE^JOHN");
    }

    SECTION("copy assignment") {
        auto original = dicom_element::from_string(
            tags::patient_name, vr_type::PN, "DOE^JOHN");
        dicom_element copy{tags::patient_id, vr_type::LO};

        copy = original;

        auto result = copy.as_string();
        REQUIRE(result.is_ok());
        CHECK(result.value() == "DOE^JOHN");
    }

    SECTION("move assignment") {
        auto original = dicom_element::from_string(
            tags::patient_name, vr_type::PN, "DOE^JOHN");
        dicom_element moved{tags::patient_id, vr_type::LO};

        moved = std::move(original);

        auto result = moved.as_string();
        REQUIRE(result.is_ok());
        CHECK(result.value() == "DOE^JOHN");
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
        auto result = elem.as_string();
        REQUIRE(result.is_ok());
        CHECK(result.value().empty());
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

        auto result = elem.as_string();
        REQUIRE(result.is_ok());
        CHECK(result.value() == "DOE^JOHN^MIDDLE^PREFIX^SUFFIX");
    }
}
