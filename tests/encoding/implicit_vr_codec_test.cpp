/**
 * @file implicit_vr_codec_test.cpp
 * @brief Unit tests for Implicit VR Little Endian codec
 */

#include <catch2/catch_test_macros.hpp>

#include <pacs/core/dicom_dataset.hpp>
#include <pacs/core/dicom_element.hpp>
#include <pacs/core/dicom_tag.hpp>
#include <pacs/core/dicom_tag_constants.hpp>
#include <pacs/core/result.hpp>
#include <pacs/encoding/implicit_vr_codec.hpp>
#include <pacs/encoding/vr_type.hpp>

#include <cstring>

using namespace pacs::core;
using namespace pacs::encoding;

namespace {

// Helper to read little-endian values from byte buffer
uint16_t read_le16(const uint8_t* data) {
    return static_cast<uint16_t>(data[0]) |
           (static_cast<uint16_t>(data[1]) << 8);
}

uint32_t read_le32(const uint8_t* data) {
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) |
           (static_cast<uint32_t>(data[3]) << 24);
}

}  // namespace

// ============================================================================
// Element Encoding Tests
// ============================================================================

TEST_CASE("implicit_vr_codec element encoding", "[encoding][implicit]") {
    SECTION("simple string element") {
        auto elem = dicom_element::from_string(
            tags::patient_name, vr_type::PN, "DOE^JOHN");

        auto bytes = implicit_vr_codec::encode_element(elem);

        // Group (2) + Element (2) + Length (4) + Value (8) = 16 bytes
        REQUIRE(bytes.size() == 16);

        // Verify tag
        CHECK(read_le16(bytes.data()) == 0x0010);      // Group
        CHECK(read_le16(bytes.data() + 2) == 0x0010);  // Element

        // Verify length
        CHECK(read_le32(bytes.data() + 4) == 8);       // Length

        // Verify value
        std::string value(bytes.begin() + 8, bytes.end());
        CHECK(value == "DOE^JOHN");
    }

    SECTION("numeric element (US - unsigned short)") {
        auto elem = dicom_element::from_numeric<uint16_t>(
            tags::rows, vr_type::US, 512);

        auto bytes = implicit_vr_codec::encode_element(elem);

        // Group (2) + Element (2) + Length (4) + Value (2) = 10 bytes
        REQUIRE(bytes.size() == 10);

        // Verify tag
        CHECK(read_le16(bytes.data()) == 0x0028);      // Group
        CHECK(read_le16(bytes.data() + 2) == 0x0010);  // Element (Rows)

        // Verify length
        CHECK(read_le32(bytes.data() + 4) == 2);

        // Verify value
        CHECK(read_le16(bytes.data() + 8) == 512);
    }

    SECTION("odd-length string gets padded") {
        auto elem = dicom_element::from_string(
            tags::patient_id, vr_type::LO, "12345");  // 5 chars (odd)

        auto bytes = implicit_vr_codec::encode_element(elem);

        // Should be padded to 6 bytes
        REQUIRE(read_le32(bytes.data() + 4) == 6);

        // Value should be "12345 " (with trailing space)
        std::string value(bytes.begin() + 8, bytes.begin() + 14);
        CHECK(value.size() == 6);
        CHECK(value.substr(0, 5) == "12345");
        CHECK(value[5] == ' ');
    }

    SECTION("empty element") {
        dicom_element elem{tags::patient_comments, vr_type::LT};

        auto bytes = implicit_vr_codec::encode_element(elem);

        // Group (2) + Element (2) + Length (4) + Value (0) = 8 bytes
        REQUIRE(bytes.size() == 8);
        CHECK(read_le32(bytes.data() + 4) == 0);
    }
}

// ============================================================================
// Element Decoding Tests
// ============================================================================

TEST_CASE("implicit_vr_codec element decoding", "[encoding][implicit]") {
    SECTION("decode simple string element") {
        // Manually construct Patient Name element bytes
        std::vector<uint8_t> bytes = {
            0x10, 0x00,  // Group 0x0010
            0x10, 0x00,  // Element 0x0010 (Patient Name)
            0x08, 0x00, 0x00, 0x00,  // Length 8
            'D', 'O', 'E', '^', 'J', 'O', 'H', 'N'
        };

        std::span<const uint8_t> data(bytes);
        auto result = implicit_vr_codec::decode_element(data);

        REQUIRE(result.is_ok());
        auto& elem = pacs::get_value(result);

        CHECK(elem.tag() == tags::patient_name);
        CHECK(elem.length() == 8);

        // Data should be fully consumed
        CHECK(data.empty());
    }

    SECTION("decode numeric element") {
        // Manually construct Rows element bytes (US VR)
        std::vector<uint8_t> bytes = {
            0x28, 0x00,  // Group 0x0028
            0x10, 0x00,  // Element 0x0010 (Rows)
            0x02, 0x00, 0x00, 0x00,  // Length 2
            0x00, 0x02   // Value 512 (little-endian)
        };

        std::span<const uint8_t> data(bytes);
        auto result = implicit_vr_codec::decode_element(data);

        REQUIRE(result.is_ok());
        auto& elem = pacs::get_value(result);

        CHECK(elem.tag() == tags::rows);
        CHECK(elem.as_numeric<uint16_t>() == 512);
    }

    SECTION("insufficient data returns error") {
        std::vector<uint8_t> bytes = {0x10, 0x00, 0x10};  // Only 3 bytes

        std::span<const uint8_t> data(bytes);
        auto result = implicit_vr_codec::decode_element(data);

        REQUIRE(!pacs::is_ok(result));
        CHECK(pacs::get_error(result).code == pacs::error_codes::insufficient_data);
    }
}

// ============================================================================
// Dataset Round-Trip Tests
// ============================================================================

TEST_CASE("implicit_vr_codec dataset round-trip", "[encoding][implicit]") {
    SECTION("basic patient information") {
        dicom_dataset original;
        original.set_string(tags::patient_name, vr_type::PN, "DOE^JOHN");
        original.set_string(tags::patient_id, vr_type::LO, "12345");
        original.set_numeric<uint16_t>(tags::rows, vr_type::US, 512);
        original.set_numeric<uint16_t>(tags::columns, vr_type::US, 256);

        auto encoded = implicit_vr_codec::encode(original);
        auto result = implicit_vr_codec::decode(encoded);

        REQUIRE(result.is_ok());
        auto& decoded = pacs::get_value(result);

        // Verify string values (may have trailing padding)
        auto patient_name = decoded.get_string(tags::patient_name);
        CHECK(patient_name.find("DOE^JOHN") == 0);

        auto patient_id = decoded.get_string(tags::patient_id);
        CHECK(patient_id.find("12345") == 0);

        // Verify numeric values
        CHECK(decoded.get_numeric<uint16_t>(tags::rows) == 512);
        CHECK(decoded.get_numeric<uint16_t>(tags::columns) == 256);
    }

    SECTION("multiple data types") {
        dicom_dataset original;
        original.set_string(tags::study_date, vr_type::DA, "20250101");
        original.set_string(tags::study_time, vr_type::TM, "120000");
        original.set_string(tags::modality, vr_type::CS, "CT");
        original.set_numeric<int32_t>(tags::instance_number, vr_type::IS, 1);

        auto encoded = implicit_vr_codec::encode(original);
        auto result = implicit_vr_codec::decode(encoded);

        REQUIRE(result.is_ok());
        auto& decoded = pacs::get_value(result);

        CHECK(decoded.get_string(tags::study_date).find("20250101") == 0);
        CHECK(decoded.get_string(tags::study_time).find("120000") == 0);
        CHECK(decoded.get_string(tags::modality).find("CT") == 0);
    }

    SECTION("empty dataset") {
        dicom_dataset original;

        auto encoded = implicit_vr_codec::encode(original);
        CHECK(encoded.empty());

        auto result = implicit_vr_codec::decode(encoded);
        REQUIRE(result.is_ok());
        CHECK(pacs::get_value(result).empty());
    }
}

// ============================================================================
// Sequence Encoding Tests
// ============================================================================

TEST_CASE("implicit_vr_codec sequence handling", "[encoding][implicit]") {
    SECTION("sequence with single item") {
        // Create a sequence element
        dicom_element seq_elem{tags::scheduled_procedure_step_sequence, vr_type::SQ};

        // Create an item
        dicom_dataset item;
        item.set_string(tags::modality, vr_type::CS, "CT");
        item.set_string(tags::scheduled_station_ae_title, vr_type::AE, "STATION1");

        seq_elem.sequence_items().push_back(item);

        // Encode and verify structure
        auto bytes = implicit_vr_codec::encode_element(seq_elem);

        // Should have: tag(4) + undefined_length(4) + item(8+content) + seq_delim(8)
        REQUIRE(bytes.size() > 16);

        // Check tag
        CHECK(read_le16(bytes.data()) == 0x0040);      // Group
        CHECK(read_le16(bytes.data() + 2) == 0x0100);  // Element

        // Check undefined length marker
        CHECK(read_le32(bytes.data() + 4) == 0xFFFFFFFF);

        // Decode and verify
        std::span<const uint8_t> data(bytes);
        auto result = implicit_vr_codec::decode_element(data);

        REQUIRE(result.is_ok());
        auto& decoded = pacs::get_value(result);

        CHECK(decoded.is_sequence());
        REQUIRE(decoded.sequence_items().size() == 1);

        auto& decoded_item = decoded.sequence_items()[0];
        CHECK(decoded_item.get_string(tags::modality).find("CT") == 0);
    }

    SECTION("sequence with multiple items") {
        dicom_element seq_elem{tags::scheduled_procedure_step_sequence, vr_type::SQ};

        // Add multiple items
        for (int i = 1; i <= 3; ++i) {
            dicom_dataset item;
            item.set_string(tags::scheduled_procedure_step_id, vr_type::SH,
                           "STEP" + std::to_string(i));
            seq_elem.sequence_items().push_back(item);
        }

        auto bytes = implicit_vr_codec::encode_element(seq_elem);

        std::span<const uint8_t> data(bytes);
        auto result = implicit_vr_codec::decode_element(data);

        REQUIRE(result.is_ok());
        CHECK(pacs::get_value(result).sequence_items().size() == 3);
    }

    SECTION("empty sequence") {
        dicom_element seq_elem{tags::scheduled_procedure_step_sequence, vr_type::SQ};

        auto bytes = implicit_vr_codec::encode_element(seq_elem);

        std::span<const uint8_t> data(bytes);
        auto result = implicit_vr_codec::decode_element(data);

        REQUIRE(result.is_ok());
        CHECK(pacs::get_value(result).is_sequence());
        CHECK(pacs::get_value(result).sequence_items().empty());
    }
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_CASE("implicit_vr_codec error handling", "[encoding][implicit]") {
    SECTION("truncated element") {
        std::vector<uint8_t> bytes = {
            0x10, 0x00,  // Group
            0x10, 0x00,  // Element
            0x10, 0x00, 0x00, 0x00,  // Length 16
            'T', 'E', 'S', 'T'  // Only 4 bytes instead of 16
        };

        std::span<const uint8_t> data(bytes);
        auto result = implicit_vr_codec::decode_element(data);

        REQUIRE(!pacs::is_ok(result));
        CHECK(pacs::get_error(result).code == pacs::error_codes::insufficient_data);
    }

    SECTION("error codes are properly defined") {
        // Verify encoding-related error codes exist and have expected values
        CHECK(pacs::error_codes::insufficient_data == -746);
        CHECK(pacs::error_codes::invalid_sequence == -747);
        CHECK(pacs::error_codes::unknown_vr == -748);
    }
}
