/**
 * @file explicit_vr_codec_test.cpp
 * @brief Unit tests for Explicit VR Little Endian codec
 */

#include <catch2/catch_test_macros.hpp>

#include <pacs/core/dicom_dataset.hpp>
#include <pacs/core/dicom_element.hpp>
#include <pacs/core/dicom_tag.hpp>
#include <pacs/core/dicom_tag_constants.hpp>
#include <pacs/encoding/explicit_vr_codec.hpp>
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
// Element Encoding Tests - 16-bit Length VRs
// ============================================================================

TEST_CASE("explicit_vr_codec 16-bit length VRs", "[encoding][explicit]") {
    SECTION("Person Name (PN) encoding") {
        auto elem = dicom_element::from_string(
            tags::patient_name, vr_type::PN, "DOE^JOHN");

        auto bytes = explicit_vr_codec::encode_element(elem);

        // Group (2) + Element (2) + VR (2) + Length16 (2) + Value (8) = 16
        REQUIRE(bytes.size() == 16);

        // Verify tag
        CHECK(read_le16(bytes.data()) == 0x0010);      // Group
        CHECK(read_le16(bytes.data() + 2) == 0x0010);  // Element

        // Verify VR
        CHECK(bytes[4] == 'P');
        CHECK(bytes[5] == 'N');

        // Verify 16-bit length
        CHECK(read_le16(bytes.data() + 6) == 8);

        // Verify value
        std::string value(bytes.begin() + 8, bytes.end());
        CHECK(value == "DOE^JOHN");
    }

    SECTION("Unsigned Short (US) encoding") {
        auto elem = dicom_element::from_numeric<uint16_t>(
            tags::rows, vr_type::US, 512);

        auto bytes = explicit_vr_codec::encode_element(elem);

        // Group (2) + Element (2) + VR (2) + Length16 (2) + Value (2) = 10
        REQUIRE(bytes.size() == 10);

        // Verify VR
        CHECK(bytes[4] == 'U');
        CHECK(bytes[5] == 'S');

        // Verify 16-bit length
        CHECK(read_le16(bytes.data() + 6) == 2);

        // Verify value
        CHECK(read_le16(bytes.data() + 8) == 512);
    }

    SECTION("Code String (CS) encoding") {
        auto elem = dicom_element::from_string(
            tags::modality, vr_type::CS, "CT");

        auto bytes = explicit_vr_codec::encode_element(elem);

        // Verify VR
        CHECK(bytes[4] == 'C');
        CHECK(bytes[5] == 'S');

        // CS uses 16-bit length
        CHECK(read_le16(bytes.data() + 6) == 2);
    }

    SECTION("Date (DA) encoding") {
        auto elem = dicom_element::from_string(
            tags::study_date, vr_type::DA, "20250101");

        auto bytes = explicit_vr_codec::encode_element(elem);

        CHECK(bytes[4] == 'D');
        CHECK(bytes[5] == 'A');
        CHECK(read_le16(bytes.data() + 6) == 8);
    }
}

// ============================================================================
// Element Encoding Tests - 32-bit Length VRs
// ============================================================================

TEST_CASE("explicit_vr_codec 32-bit length VRs", "[encoding][explicit]") {
    SECTION("Other Word (OW) encoding") {
        std::vector<uint8_t> pixel_data(1024);
        for (size_t i = 0; i < pixel_data.size(); ++i) {
            pixel_data[i] = static_cast<uint8_t>(i & 0xFF);
        }
        dicom_element elem{tags::pixel_data, vr_type::OW, pixel_data};

        auto bytes = explicit_vr_codec::encode_element(elem);

        // Group (2) + Element (2) + VR (2) + Reserved (2) + Length32 (4) + Value (1024)
        REQUIRE(bytes.size() == 12 + 1024);

        // Verify VR
        CHECK(bytes[4] == 'O');
        CHECK(bytes[5] == 'W');

        // Verify reserved bytes
        CHECK(read_le16(bytes.data() + 6) == 0);

        // Verify 32-bit length
        CHECK(read_le32(bytes.data() + 8) == 1024);
    }

    SECTION("Unknown (UN) encoding") {
        std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
        dicom_element elem{dicom_tag{0x0011, 0x0010}, vr_type::UN, data};

        auto bytes = explicit_vr_codec::encode_element(elem);

        // UN uses 32-bit length format
        CHECK(bytes[4] == 'U');
        CHECK(bytes[5] == 'N');
        CHECK(read_le16(bytes.data() + 6) == 0);  // Reserved
        CHECK(read_le32(bytes.data() + 8) == 4);  // 32-bit length
    }

    SECTION("Sequence (SQ) encoding") {
        dicom_element seq_elem{tags::scheduled_procedure_step_sequence, vr_type::SQ};

        auto bytes = explicit_vr_codec::encode_element(seq_elem);

        // Verify VR
        CHECK(bytes[4] == 'S');
        CHECK(bytes[5] == 'Q');

        // Verify reserved bytes
        CHECK(read_le16(bytes.data() + 6) == 0);

        // SQ uses undefined length
        CHECK(read_le32(bytes.data() + 8) == 0xFFFFFFFF);
    }
}

// ============================================================================
// Element Decoding Tests
// ============================================================================

TEST_CASE("explicit_vr_codec element decoding", "[encoding][explicit]") {
    SECTION("decode 16-bit length element") {
        // Manually construct Patient Name element bytes (Explicit VR)
        std::vector<uint8_t> bytes = {
            0x10, 0x00,  // Group 0x0010
            0x10, 0x00,  // Element 0x0010 (Patient Name)
            'P', 'N',    // VR
            0x08, 0x00,  // Length 8 (16-bit)
            'D', 'O', 'E', '^', 'J', 'O', 'H', 'N'
        };

        std::span<const uint8_t> data(bytes);
        auto result = explicit_vr_codec::decode_element(data);

        REQUIRE(result.is_ok());
        auto& elem = pacs::get_value(result);

        CHECK(elem.tag() == tags::patient_name);
        CHECK(elem.vr() == vr_type::PN);  // VR should come from stream
        CHECK(elem.length() == 8);
        CHECK(data.empty());
    }

    SECTION("decode 32-bit length element") {
        // Construct OW element with 32-bit length
        std::vector<uint8_t> bytes = {
            0xE0, 0x7F,  // Group 0x7FE0
            0x10, 0x00,  // Element 0x0010 (Pixel Data)
            'O', 'W',    // VR
            0x00, 0x00,  // Reserved
            0x04, 0x00, 0x00, 0x00,  // Length 4 (32-bit)
            0x01, 0x02, 0x03, 0x04   // Value
        };

        std::span<const uint8_t> data(bytes);
        auto result = explicit_vr_codec::decode_element(data);

        REQUIRE(result.is_ok());
        auto& elem = pacs::get_value(result);

        CHECK(elem.tag() == tags::pixel_data);
        CHECK(elem.vr() == vr_type::OW);
        CHECK(elem.length() == 4);
    }

    SECTION("VR is read from stream, not dictionary") {
        // Create element with non-standard VR for known tag
        std::vector<uint8_t> bytes = {
            0x10, 0x00,  // Group
            0x20, 0x00,  // Element (Patient ID)
            'S', 'H',    // VR = SH (could be different from dictionary)
            0x04, 0x00,  // Length 4
            'T', 'E', 'S', 'T'
        };

        std::span<const uint8_t> data(bytes);
        auto result = explicit_vr_codec::decode_element(data);

        REQUIRE(result.is_ok());
        // VR should be SH as read from stream
        CHECK(pacs::get_value(result).vr() == vr_type::SH);
    }
}

// ============================================================================
// Dataset Round-Trip Tests
// ============================================================================

TEST_CASE("explicit_vr_codec dataset round-trip", "[encoding][explicit]") {
    SECTION("basic patient information") {
        dicom_dataset original;
        original.set_string(tags::patient_name, vr_type::PN, "DOE^JOHN");
        original.set_string(tags::patient_id, vr_type::LO, "12345");
        original.set_numeric<uint16_t>(tags::rows, vr_type::US, 512);
        original.set_numeric<uint16_t>(tags::columns, vr_type::US, 256);

        auto encoded = explicit_vr_codec::encode(original);
        auto result = explicit_vr_codec::decode(encoded);

        REQUIRE(result.is_ok());
        auto& decoded = pacs::get_value(result);

        // Verify VRs are preserved
        auto* name_elem = decoded.get(tags::patient_name);
        REQUIRE(name_elem != nullptr);
        CHECK(name_elem->vr() == vr_type::PN);

        auto* rows_elem = decoded.get(tags::rows);
        REQUIRE(rows_elem != nullptr);
        CHECK(rows_elem->vr() == vr_type::US);

        // Verify values
        CHECK(decoded.get_numeric<uint16_t>(tags::rows) == 512);
        CHECK(decoded.get_numeric<uint16_t>(tags::columns) == 256);
    }

    SECTION("all 16-bit length VR types") {
        dicom_dataset original;
        original.set_string(tags::specific_character_set, vr_type::CS, "ISO_IR 100");
        original.set_string(tags::study_date, vr_type::DA, "20250101");
        original.set_string(tags::study_time, vr_type::TM, "120000");
        original.set_string(tags::patient_name, vr_type::PN, "TEST");
        original.set_string(tags::patient_id, vr_type::LO, "ID123");
        original.set_numeric<uint16_t>(tags::rows, vr_type::US, 100);
        original.set_numeric<uint16_t>(tags::columns, vr_type::US, 100);
        original.set_numeric<float>(tags::rescale_slope, vr_type::DS, 1.0f);

        auto encoded = explicit_vr_codec::encode(original);
        auto result = explicit_vr_codec::decode(encoded);

        REQUIRE(result.is_ok());
        CHECK(pacs::get_value(result).size() == original.size());
    }

    SECTION("mixed 16-bit and 32-bit length VRs") {
        dicom_dataset original;
        original.set_string(tags::patient_name, vr_type::PN, "TEST");  // 16-bit

        // Add binary data element (32-bit length)
        std::vector<uint8_t> binary_data(100, 0xAB);
        original.insert(dicom_element{
            tags::pixel_data, vr_type::OW, binary_data});

        auto encoded = explicit_vr_codec::encode(original);
        auto result = explicit_vr_codec::decode(encoded);

        REQUIRE(result.is_ok());
        CHECK(pacs::get_value(result).size() == 2);

        auto* pixel_elem = pacs::get_value(result).get(tags::pixel_data);
        REQUIRE(pixel_elem != nullptr);
        CHECK(pixel_elem->length() == 100);
    }
}

// ============================================================================
// Sequence Handling Tests
// ============================================================================

TEST_CASE("explicit_vr_codec sequence handling", "[encoding][explicit]") {
    SECTION("sequence with items") {
        dicom_element seq_elem{tags::scheduled_procedure_step_sequence, vr_type::SQ};

        dicom_dataset item;
        item.set_string(tags::modality, vr_type::CS, "CT");
        item.set_string(tags::scheduled_station_ae_title, vr_type::AE, "SCANNER1");

        seq_elem.sequence_items().push_back(item);

        auto bytes = explicit_vr_codec::encode_element(seq_elem);

        // Verify SQ encoding
        CHECK(bytes[4] == 'S');
        CHECK(bytes[5] == 'Q');

        // Decode and verify
        std::span<const uint8_t> data(bytes);
        auto result = explicit_vr_codec::decode_element(data);

        REQUIRE(result.is_ok());
        CHECK(pacs::get_value(result).is_sequence());
        REQUIRE(pacs::get_value(result).sequence_items().size() == 1);

        auto& decoded_item = pacs::get_value(result).sequence_items()[0];
        auto* modality_elem = decoded_item.get(tags::modality);
        REQUIRE(modality_elem != nullptr);
        CHECK(modality_elem->vr() == vr_type::CS);
    }

    SECTION("nested sequences") {
        dicom_element outer_seq{tags::scheduled_procedure_step_sequence, vr_type::SQ};

        dicom_dataset outer_item;
        outer_item.set_string(tags::modality, vr_type::CS, "MR");

        // Add inner sequence (using a different sequence tag)
        dicom_element inner_seq{dicom_tag{0x0040, 0x0200}, vr_type::SQ};
        dicom_dataset inner_item;
        inner_item.set_string(tags::scheduled_station_name, vr_type::SH, "STATION1");
        inner_seq.sequence_items().push_back(inner_item);

        outer_item.insert(std::move(inner_seq));
        outer_seq.sequence_items().push_back(outer_item);

        auto bytes = explicit_vr_codec::encode_element(outer_seq);

        std::span<const uint8_t> data(bytes);
        auto result = explicit_vr_codec::decode_element(data);

        REQUIRE(result.is_ok());
        CHECK(pacs::get_value(result).is_sequence());
    }
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_CASE("explicit_vr_codec error handling", "[encoding][explicit]") {
    SECTION("unknown VR returns error") {
        std::vector<uint8_t> bytes = {
            0x10, 0x00,
            0x10, 0x00,
            'X', 'X',    // Invalid VR
            0x04, 0x00,
            'T', 'E', 'S', 'T'
        };

        std::span<const uint8_t> data(bytes);
        auto result = explicit_vr_codec::decode_element(data);

        REQUIRE(!result.is_ok());
        CHECK(pacs::get_error(result).code == pacs::error_codes::unknown_vr);
    }

    SECTION("insufficient data for header") {
        std::vector<uint8_t> bytes = {0x10, 0x00, 0x10};  // Only 3 bytes

        std::span<const uint8_t> data(bytes);
        auto result = explicit_vr_codec::decode_element(data);

        REQUIRE(!result.is_ok());
        CHECK(pacs::get_error(result).code == pacs::error_codes::insufficient_data);
    }

    SECTION("insufficient data for 32-bit length header") {
        std::vector<uint8_t> bytes = {
            0xE0, 0x7F,
            0x10, 0x00,
            'O', 'W',
            0x00, 0x00   // Missing 4 bytes for length
        };

        std::span<const uint8_t> data(bytes);
        auto result = explicit_vr_codec::decode_element(data);

        REQUIRE(!result.is_ok());
        CHECK(pacs::get_error(result).code == pacs::error_codes::insufficient_data);
    }

    SECTION("truncated value data") {
        std::vector<uint8_t> bytes = {
            0x10, 0x00,
            0x10, 0x00,
            'P', 'N',
            0x10, 0x00,  // Length 16
            'T', 'E', 'S', 'T'  // Only 4 bytes
        };

        std::span<const uint8_t> data(bytes);
        auto result = explicit_vr_codec::decode_element(data);

        REQUIRE(!result.is_ok());
        CHECK(pacs::get_error(result).code == pacs::error_codes::insufficient_data);
    }
}

// ============================================================================
// VR Classification Tests
// ============================================================================

TEST_CASE("explicit_vr_codec VR length classification", "[encoding][explicit]") {
    SECTION("16-bit length VRs are encoded correctly") {
        // VRs that use 16-bit length
        std::vector<vr_type> short_vrs = {
            vr_type::AE, vr_type::AS, vr_type::AT, vr_type::CS,
            vr_type::DA, vr_type::DS, vr_type::DT, vr_type::FL,
            vr_type::FD, vr_type::IS, vr_type::LO, vr_type::LT,
            vr_type::PN, vr_type::SH, vr_type::SL, vr_type::SS,
            vr_type::ST, vr_type::TM, vr_type::UI, vr_type::UL,
            vr_type::US
        };

        for (auto vr : short_vrs) {
            auto elem = dicom_element{dicom_tag{0x0010, 0x0010}, vr};
            auto bytes = explicit_vr_codec::encode_element(elem);

            // Standard format: header is 8 bytes (tag=4, VR=2, length=2)
            CHECK(bytes.size() == 8);
        }
    }

    SECTION("32-bit length VRs are encoded correctly") {
        // VRs that use 32-bit length
        std::vector<vr_type> long_vrs = {
            vr_type::OB, vr_type::OD, vr_type::OF, vr_type::OL,
            vr_type::OW, vr_type::SQ, vr_type::UC, vr_type::UN,
            vr_type::UR, vr_type::UT
        };

        for (auto vr : long_vrs) {
            dicom_element elem{dicom_tag{0x0010, 0x0010}, vr};

            // Skip SQ as it has special encoding
            if (vr == vr_type::SQ) continue;

            auto bytes = explicit_vr_codec::encode_element(elem);

            // Extended format: header is 12 bytes (tag=4, VR=2, reserved=2, length=4)
            CHECK(bytes.size() == 12);
        }
    }
}
