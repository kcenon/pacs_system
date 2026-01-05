/**
 * @file explicit_vr_big_endian_codec_test.cpp
 * @brief Unit tests for Explicit VR Big Endian codec
 */

#include <catch2/catch_test_macros.hpp>

#include <pacs/core/dicom_dataset.hpp>
#include <pacs/core/dicom_element.hpp>
#include <pacs/core/dicom_tag.hpp>
#include <pacs/core/dicom_tag_constants.hpp>
#include <pacs/encoding/byte_swap.hpp>
#include <pacs/encoding/explicit_vr_big_endian_codec.hpp>
#include <pacs/encoding/explicit_vr_codec.hpp>
#include <pacs/encoding/vr_type.hpp>

#include <cstring>

using namespace pacs::core;
using namespace pacs::encoding;

// ============================================================================
// Element Encoding Tests - 16-bit Length VRs
// ============================================================================

TEST_CASE("explicit_vr_big_endian_codec 16-bit length VRs", "[encoding][big_endian]") {
    SECTION("Person Name (PN) encoding") {
        auto elem = dicom_element::from_string(
            tags::patient_name, vr_type::PN, "DOE^JOHN");

        auto bytes = explicit_vr_big_endian_codec::encode_element(elem);

        // Group (2) + Element (2) + VR (2) + Length16 (2) + Value (8) = 16
        REQUIRE(bytes.size() == 16);

        // Verify tag (big-endian)
        CHECK(read_be16(bytes.data()) == 0x0010);      // Group
        CHECK(read_be16(bytes.data() + 2) == 0x0010);  // Element

        // Verify VR
        CHECK(bytes[4] == 'P');
        CHECK(bytes[5] == 'N');

        // Verify 16-bit length (big-endian)
        CHECK(read_be16(bytes.data() + 6) == 8);

        // Verify value (string, no byte swap needed)
        std::string value(bytes.begin() + 8, bytes.end());
        CHECK(value == "DOE^JOHN");
    }

    SECTION("Unsigned Short (US) encoding with byte swap") {
        auto elem = dicom_element::from_numeric<uint16_t>(
            tags::rows, vr_type::US, 512);

        auto bytes = explicit_vr_big_endian_codec::encode_element(elem);

        // Group (2) + Element (2) + VR (2) + Length16 (2) + Value (2) = 10
        REQUIRE(bytes.size() == 10);

        // Verify VR
        CHECK(bytes[4] == 'U');
        CHECK(bytes[5] == 'S');

        // Verify 16-bit length (big-endian)
        CHECK(read_be16(bytes.data() + 6) == 2);

        // Verify value is big-endian (512 = 0x0200)
        CHECK(read_be16(bytes.data() + 8) == 512);
    }

    SECTION("Code String (CS) encoding - no byte swap") {
        auto elem = dicom_element::from_string(
            tags::modality, vr_type::CS, "CT");

        auto bytes = explicit_vr_big_endian_codec::encode_element(elem);

        // Verify VR
        CHECK(bytes[4] == 'C');
        CHECK(bytes[5] == 'S');

        // CS uses 16-bit length (big-endian)
        CHECK(read_be16(bytes.data() + 6) == 2);

        // Value is string, no swap
        CHECK(bytes[8] == 'C');
        CHECK(bytes[9] == 'T');
    }
}

// ============================================================================
// Element Encoding Tests - 32-bit Length VRs
// ============================================================================

TEST_CASE("explicit_vr_big_endian_codec 32-bit length VRs", "[encoding][big_endian]") {
    SECTION("Other Word (OW) encoding with byte swap") {
        std::vector<uint8_t> pixel_data = {0x12, 0x34, 0xAB, 0xCD};
        dicom_element elem{tags::pixel_data, vr_type::OW, pixel_data};

        auto bytes = explicit_vr_big_endian_codec::encode_element(elem);

        // Group (2) + Element (2) + VR (2) + Reserved (2) + Length32 (4) + Value (4) = 16
        REQUIRE(bytes.size() == 16);

        // Verify VR
        CHECK(bytes[4] == 'O');
        CHECK(bytes[5] == 'W');

        // Verify reserved bytes
        CHECK(read_be16(bytes.data() + 6) == 0);

        // Verify 32-bit length (big-endian)
        CHECK(read_be32(bytes.data() + 8) == 4);

        // OW data should have each 16-bit word swapped
        CHECK(bytes[12] == 0x34);  // First word swapped
        CHECK(bytes[13] == 0x12);
        CHECK(bytes[14] == 0xCD);  // Second word swapped
        CHECK(bytes[15] == 0xAB);
    }

    SECTION("Sequence (SQ) encoding") {
        dicom_element seq_elem{tags::scheduled_procedure_step_sequence, vr_type::SQ};

        auto bytes = explicit_vr_big_endian_codec::encode_element(seq_elem);

        // Verify VR
        CHECK(bytes[4] == 'S');
        CHECK(bytes[5] == 'Q');

        // Verify reserved bytes
        CHECK(read_be16(bytes.data() + 6) == 0);

        // SQ uses undefined length (big-endian 0xFFFFFFFF)
        CHECK(read_be32(bytes.data() + 8) == 0xFFFFFFFF);
    }
}

// ============================================================================
// Element Decoding Tests
// ============================================================================

TEST_CASE("explicit_vr_big_endian_codec element decoding", "[encoding][big_endian]") {
    SECTION("decode 16-bit length element") {
        // Manually construct Patient Name element bytes (Explicit VR Big Endian)
        std::vector<uint8_t> bytes = {
            0x00, 0x10,  // Group 0x0010 (big-endian)
            0x00, 0x10,  // Element 0x0010 (big-endian)
            'P', 'N',    // VR
            0x00, 0x08,  // Length 8 (big-endian)
            'D', 'O', 'E', '^', 'J', 'O', 'H', 'N'
        };

        std::span<const uint8_t> data(bytes);
        auto result = explicit_vr_big_endian_codec::decode_element(data);

        REQUIRE(result.is_ok());
        auto& elem = result.value();

        CHECK(elem.tag() == tags::patient_name);
        CHECK(elem.vr() == vr_type::PN);
        CHECK(elem.length() == 8);
        CHECK(data.empty());
    }

    SECTION("decode 16-bit numeric with byte swap") {
        // US value 512 (0x0200) in big-endian: 0x02, 0x00
        std::vector<uint8_t> bytes = {
            0x00, 0x28,  // Group 0x0028 (big-endian)
            0x00, 0x10,  // Element 0x0010 (Rows) (big-endian)
            'U', 'S',    // VR
            0x00, 0x02,  // Length 2 (big-endian)
            0x02, 0x00   // Value 512 (big-endian)
        };

        std::span<const uint8_t> data(bytes);
        auto result = explicit_vr_big_endian_codec::decode_element(data);

        REQUIRE(result.is_ok());
        auto& elem = result.value();

        CHECK(elem.tag() == tags::rows);
        CHECK(elem.vr() == vr_type::US);

        // Value should be converted back to native (little-endian)
        auto raw = elem.raw_data();
        REQUIRE(raw.size() == 2);
        CHECK(raw[0] == 0x00);  // Little-endian: low byte first
        CHECK(raw[1] == 0x02);
    }

    SECTION("decode 32-bit length element") {
        // Construct OW element with 32-bit length
        std::vector<uint8_t> bytes = {
            0x7F, 0xE0,  // Group 0x7FE0 (big-endian)
            0x00, 0x10,  // Element 0x0010 (big-endian)
            'O', 'W',    // VR
            0x00, 0x00,  // Reserved
            0x00, 0x00, 0x00, 0x04,  // Length 4 (big-endian)
            0x12, 0x34, 0xAB, 0xCD   // Value (big-endian words)
        };

        std::span<const uint8_t> data(bytes);
        auto result = explicit_vr_big_endian_codec::decode_element(data);

        REQUIRE(result.is_ok());
        auto& elem = result.value();

        CHECK(elem.tag() == tags::pixel_data);
        CHECK(elem.vr() == vr_type::OW);

        // OW data should be converted back to little-endian
        auto raw = elem.raw_data();
        REQUIRE(raw.size() == 4);
        CHECK(raw[0] == 0x34);  // First word: BE 0x1234 -> LE 0x3412
        CHECK(raw[1] == 0x12);
        CHECK(raw[2] == 0xCD);  // Second word: BE 0xABCD -> LE 0xCDAB
        CHECK(raw[3] == 0xAB);
    }
}

// ============================================================================
// Dataset Round-Trip Tests
// ============================================================================

TEST_CASE("explicit_vr_big_endian_codec dataset round-trip", "[encoding][big_endian]") {
    SECTION("basic patient information") {
        dicom_dataset original;
        original.set_string(tags::patient_name, vr_type::PN, "DOE^JOHN");
        original.set_string(tags::patient_id, vr_type::LO, "12345");
        original.set_numeric<uint16_t>(tags::rows, vr_type::US, 512);
        original.set_numeric<uint16_t>(tags::columns, vr_type::US, 256);

        auto encoded = explicit_vr_big_endian_codec::encode(original);
        auto result = explicit_vr_big_endian_codec::decode(encoded);

        REQUIRE(result.is_ok());
        auto& decoded = result.value();

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

    SECTION("numeric VR types with byte swap") {
        dicom_dataset original;
        original.set_numeric<uint16_t>(tags::rows, vr_type::US, 0x1234);
        original.set_numeric<uint32_t>(dicom_tag{0x0008, 0x0050}, vr_type::UL, 0x12345678);
        original.set_numeric<int16_t>(dicom_tag{0x0028, 0x0106}, vr_type::SS, -1000);
        original.set_numeric<int32_t>(dicom_tag{0x0028, 0x0107}, vr_type::SL, -100000);

        auto encoded = explicit_vr_big_endian_codec::encode(original);
        auto result = explicit_vr_big_endian_codec::decode(encoded);

        REQUIRE(result.is_ok());
        auto& decoded = result.value();

        CHECK(decoded.get_numeric<uint16_t>(tags::rows) == 0x1234);
        CHECK(decoded.get_numeric<int16_t>(dicom_tag{0x0028, 0x0106}) == -1000);
    }

    SECTION("OW pixel data round-trip") {
        dicom_dataset original;
        std::vector<uint8_t> pixel_data(1024);
        for (size_t i = 0; i < pixel_data.size(); ++i) {
            pixel_data[i] = static_cast<uint8_t>(i & 0xFF);
        }
        original.insert(dicom_element{tags::pixel_data, vr_type::OW, pixel_data});

        auto encoded = explicit_vr_big_endian_codec::encode(original);
        auto result = explicit_vr_big_endian_codec::decode(encoded);

        REQUIRE(result.is_ok());
        auto& decoded = result.value();

        auto* pixel_elem = decoded.get(tags::pixel_data);
        REQUIRE(pixel_elem != nullptr);
        CHECK(pixel_elem->length() == 1024);

        // Verify data integrity after round-trip
        auto decoded_data = pixel_elem->raw_data();
        REQUIRE(decoded_data.size() == pixel_data.size());
        CHECK(std::equal(decoded_data.begin(), decoded_data.end(), pixel_data.begin()));
    }
}

// ============================================================================
// Sequence Handling Tests
// ============================================================================

TEST_CASE("explicit_vr_big_endian_codec sequence handling", "[encoding][big_endian]") {
    SECTION("sequence with items") {
        dicom_element seq_elem{tags::scheduled_procedure_step_sequence, vr_type::SQ};

        dicom_dataset item;
        item.set_string(tags::modality, vr_type::CS, "CT");
        item.set_string(tags::scheduled_station_ae_title, vr_type::AE, "SCANNER1");

        seq_elem.sequence_items().push_back(item);

        auto bytes = explicit_vr_big_endian_codec::encode_element(seq_elem);

        // Verify SQ encoding
        CHECK(bytes[4] == 'S');
        CHECK(bytes[5] == 'Q');

        // Decode and verify
        std::span<const uint8_t> data(bytes);
        auto result = explicit_vr_big_endian_codec::decode_element(data);

        REQUIRE(result.is_ok());
        CHECK(result.value().is_sequence());
        REQUIRE(result.value().sequence_items().size() == 1);

        auto& decoded_item = result.value().sequence_items()[0];
        auto* modality_elem = decoded_item.get(tags::modality);
        REQUIRE(modality_elem != nullptr);
        CHECK(modality_elem->vr() == vr_type::CS);
    }
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_CASE("explicit_vr_big_endian_codec error handling", "[encoding][big_endian]") {
    SECTION("unknown VR returns error") {
        std::vector<uint8_t> bytes = {
            0x00, 0x10,  // Group (big-endian)
            0x00, 0x10,  // Element (big-endian)
            'X', 'X',    // Invalid VR
            0x00, 0x04,  // Length (big-endian)
            'T', 'E', 'S', 'T'
        };

        std::span<const uint8_t> data(bytes);
        auto result = explicit_vr_big_endian_codec::decode_element(data);

        REQUIRE(!result.is_ok());
        CHECK(result.error().code == pacs::error_codes::unknown_vr);
    }

    SECTION("insufficient data for header") {
        std::vector<uint8_t> bytes = {0x00, 0x10, 0x00};  // Only 3 bytes

        std::span<const uint8_t> data(bytes);
        auto result = explicit_vr_big_endian_codec::decode_element(data);

        REQUIRE(!result.is_ok());
        CHECK(result.error().code == pacs::error_codes::insufficient_data);
    }

    SECTION("truncated value data") {
        std::vector<uint8_t> bytes = {
            0x00, 0x10,
            0x00, 0x10,
            'P', 'N',
            0x00, 0x10,  // Length 16 (big-endian)
            'T', 'E', 'S', 'T'  // Only 4 bytes
        };

        std::span<const uint8_t> data(bytes);
        auto result = explicit_vr_big_endian_codec::decode_element(data);

        REQUIRE(!result.is_ok());
        CHECK(result.error().code == pacs::error_codes::insufficient_data);
    }
}

// ============================================================================
// Interoperability Tests: LE <-> BE Conversion
// ============================================================================

TEST_CASE("interoperability between LE and BE codecs", "[encoding][big_endian]") {
    SECTION("LE to BE to LE round-trip") {
        // Create original dataset
        dicom_dataset original;
        original.set_string(tags::patient_name, vr_type::PN, "SMITH^ALICE");
        original.set_string(tags::patient_id, vr_type::LO, "PAT123");
        original.set_numeric<uint16_t>(tags::rows, vr_type::US, 512);
        original.set_numeric<uint16_t>(tags::columns, vr_type::US, 512);
        original.set_numeric<uint16_t>(tags::bits_allocated, vr_type::US, 16);

        // Encode as LE
        auto le_bytes = explicit_vr_codec::encode(original);

        // Decode LE
        auto le_result = explicit_vr_codec::decode(le_bytes);
        REQUIRE(le_result.is_ok());

        // Encode as BE
        auto be_bytes = explicit_vr_big_endian_codec::encode(le_result.value());

        // Decode BE
        auto be_result = explicit_vr_big_endian_codec::decode(be_bytes);
        REQUIRE(be_result.is_ok());

        // Re-encode as LE
        auto final_le_bytes = explicit_vr_codec::encode(be_result.value());

        // Verify LE bytes are identical
        CHECK(final_le_bytes == le_bytes);

        // Verify values
        CHECK(be_result.value().get_numeric<uint16_t>(tags::rows) == 512);
        CHECK(be_result.value().get_numeric<uint16_t>(tags::columns) == 512);
    }

    SECTION("BE to LE conversion preserves data") {
        // Create a BE-encoded dataset manually
        std::vector<uint8_t> be_bytes = {
            // Patient Name (0010,0010) PN "TEST"
            0x00, 0x10,              // Group (big-endian)
            0x00, 0x10,              // Element (big-endian)
            'P', 'N',                // VR
            0x00, 0x04,              // Length 4 (big-endian)
            'T', 'E', 'S', 'T',      // Value
            // Rows (0028,0010) US 256
            0x00, 0x28,              // Group (big-endian)
            0x00, 0x10,              // Element (big-endian)
            'U', 'S',                // VR
            0x00, 0x02,              // Length 2 (big-endian)
            0x01, 0x00               // Value 256 (big-endian: 0x0100)
        };

        // Decode BE
        auto be_result = explicit_vr_big_endian_codec::decode(be_bytes);
        REQUIRE(be_result.is_ok());

        // Verify decoded values
        CHECK(be_result.value().get_numeric<uint16_t>(tags::rows) == 256);

        // Encode as LE
        auto le_bytes = explicit_vr_codec::encode(be_result.value());

        // Decode LE and verify
        auto le_result = explicit_vr_codec::decode(le_bytes);
        REQUIRE(le_result.is_ok());
        CHECK(le_result.value().get_numeric<uint16_t>(tags::rows) == 256);
    }
}

// ============================================================================
// Byte Order Conversion Utility Tests
// ============================================================================

TEST_CASE("to_big_endian and from_big_endian utilities", "[encoding][big_endian]") {
    SECTION("string VRs are not swapped") {
        std::vector<uint8_t> data = {'H', 'E', 'L', 'L', 'O'};
        auto be_data = explicit_vr_big_endian_codec::to_big_endian(vr_type::LO, data);
        CHECK(be_data == data);  // No change for strings
    }

    SECTION("US is swapped") {
        std::vector<uint8_t> le_data = {0x34, 0x12};  // 0x1234 in LE
        auto be_data = explicit_vr_big_endian_codec::to_big_endian(vr_type::US, le_data);
        CHECK(be_data[0] == 0x12);
        CHECK(be_data[1] == 0x34);
    }

    SECTION("UL is swapped") {
        std::vector<uint8_t> le_data = {0x78, 0x56, 0x34, 0x12};  // 0x12345678 in LE
        auto be_data = explicit_vr_big_endian_codec::to_big_endian(vr_type::UL, le_data);
        CHECK(be_data[0] == 0x12);
        CHECK(be_data[1] == 0x34);
        CHECK(be_data[2] == 0x56);
        CHECK(be_data[3] == 0x78);
    }

    SECTION("OB is not swapped") {
        std::vector<uint8_t> data = {0x12, 0x34, 0x56, 0x78};
        auto be_data = explicit_vr_big_endian_codec::to_big_endian(vr_type::OB, data);
        CHECK(be_data == data);  // OB is byte data, no swap
    }

    SECTION("symmetric swap (to_big == from_big)") {
        std::vector<uint8_t> original = {0x12, 0x34};
        auto swapped = explicit_vr_big_endian_codec::to_big_endian(vr_type::US, original);
        auto restored = explicit_vr_big_endian_codec::from_big_endian(vr_type::US, swapped);
        CHECK(restored == original);
    }
}
