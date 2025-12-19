/**
 * @file dicom_file_test.cpp
 * @brief Unit tests for dicom_file class
 */

#include <catch2/catch_test_macros.hpp>

#include <pacs/core/dicom_file.hpp>
#include <pacs/core/dicom_tag_constants.hpp>
#include <pacs/core/result.hpp>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

using namespace pacs::core;
using namespace pacs::encoding;

namespace {

/**
 * @brief Create a minimal valid DICOM file in memory
 */
[[nodiscard]] auto create_minimal_dicom_bytes() -> std::vector<uint8_t> {
    std::vector<uint8_t> data;

    // 128-byte preamble (zeros)
    data.resize(128, 0);

    // DICM prefix
    data.push_back('D');
    data.push_back('I');
    data.push_back('C');
    data.push_back('M');

    // Helper to write uint16 LE
    auto write_u16 = [&data](uint16_t val) {
        data.push_back(static_cast<uint8_t>(val & 0xFF));
        data.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    };

    // Helper to write uint32 LE
    auto write_u32 = [&data](uint32_t val) {
        data.push_back(static_cast<uint8_t>(val & 0xFF));
        data.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
        data.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
        data.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
    };

    // Helper to write string with appropriate padding
    auto write_string = [&data](std::string_view str, char pad_char = '\0') {
        for (char c : str) {
            data.push_back(static_cast<uint8_t>(c));
        }
        // Pad to even length
        if (str.size() % 2 != 0) {
            data.push_back(static_cast<uint8_t>(pad_char));
        }
    };

    // === File Meta Information ===

    // (0002,0001) File Meta Information Version - OB, 2 bytes
    write_u16(0x0002);  // Group
    write_u16(0x0001);  // Element
    data.push_back('O');
    data.push_back('B');
    data.push_back(0x00);  // Reserved
    data.push_back(0x00);
    write_u32(2);  // Length
    data.push_back(0x00);
    data.push_back(0x01);

    // (0002,0002) Media Storage SOP Class UID - UI
    std::string_view sop_class = "1.2.840.10008.5.1.4.1.1.2";  // CT Image Storage
    write_u16(0x0002);
    write_u16(0x0002);
    data.push_back('U');
    data.push_back('I');
    write_u16(static_cast<uint16_t>(sop_class.size() % 2 == 0 ? sop_class.size() : sop_class.size() + 1));
    write_string(sop_class);

    // (0002,0003) Media Storage SOP Instance UID - UI
    std::string_view sop_instance = "1.2.3.4.5.6.7.8.9";
    write_u16(0x0002);
    write_u16(0x0003);
    data.push_back('U');
    data.push_back('I');
    write_u16(static_cast<uint16_t>(sop_instance.size() % 2 == 0 ? sop_instance.size() : sop_instance.size() + 1));
    write_string(sop_instance);

    // (0002,0010) Transfer Syntax UID - UI
    std::string_view ts_uid = "1.2.840.10008.1.2.1";  // Explicit VR LE
    write_u16(0x0002);
    write_u16(0x0010);
    data.push_back('U');
    data.push_back('I');
    write_u16(static_cast<uint16_t>(ts_uid.size() % 2 == 0 ? ts_uid.size() : ts_uid.size() + 1));
    write_string(ts_uid);

    // (0002,0012) Implementation Class UID - UI
    std::string_view impl_uid = "1.2.3.4.5";
    write_u16(0x0002);
    write_u16(0x0012);
    data.push_back('U');
    data.push_back('I');
    write_u16(static_cast<uint16_t>(impl_uid.size() % 2 == 0 ? impl_uid.size() : impl_uid.size() + 1));
    write_string(impl_uid);

    // === Main Dataset ===

    // (0008,0016) SOP Class UID - UI
    write_u16(0x0008);
    write_u16(0x0016);
    data.push_back('U');
    data.push_back('I');
    write_u16(static_cast<uint16_t>(sop_class.size() % 2 == 0 ? sop_class.size() : sop_class.size() + 1));
    write_string(sop_class);

    // (0008,0018) SOP Instance UID - UI
    write_u16(0x0008);
    write_u16(0x0018);
    data.push_back('U');
    data.push_back('I');
    write_u16(static_cast<uint16_t>(sop_instance.size() % 2 == 0 ? sop_instance.size() : sop_instance.size() + 1));
    write_string(sop_instance);

    // (0010,0010) Patient Name - PN (space padded)
    std::string_view patient_name = "DOE^JOHN";
    write_u16(0x0010);
    write_u16(0x0010);
    data.push_back('P');
    data.push_back('N');
    write_u16(static_cast<uint16_t>(patient_name.size() % 2 == 0 ? patient_name.size() : patient_name.size() + 1));
    write_string(patient_name, ' ');

    // (0010,0020) Patient ID - LO (space padded)
    std::string_view patient_id = "12345";
    write_u16(0x0010);
    write_u16(0x0020);
    data.push_back('L');
    data.push_back('O');
    write_u16(static_cast<uint16_t>(patient_id.size() % 2 == 0 ? patient_id.size() : patient_id.size() + 1));
    write_string(patient_id, ' ');

    return data;
}

/**
 * @brief Create test file path in temp directory
 */
[[nodiscard]] auto create_temp_file_path(const std::string& name)
    -> std::filesystem::path {
    return std::filesystem::temp_directory_path() / name;
}

}  // namespace

// ============================================================================
// Reading Tests
// ============================================================================

TEST_CASE("dicom_file reading from bytes", "[core][dicom_file]") {
    SECTION("valid DICOM file is parsed correctly") {
        auto data = create_minimal_dicom_bytes();

        auto result = dicom_file::from_bytes(data);

        REQUIRE(result.is_ok());
        auto& file = result.value();

        // Check meta information
        CHECK(file.meta_information().contains(tags::transfer_syntax_uid));
        CHECK(file.meta_information().contains(tags::media_storage_sop_class_uid));

        // Check main dataset
        CHECK(file.dataset().get_string(tags::patient_name) == "DOE^JOHN");
        CHECK(file.dataset().get_string(tags::patient_id) == "12345");
    }

    SECTION("file too small returns error") {
        std::vector<uint8_t> data(100, 0);  // Less than 132 bytes

        auto result = dicom_file::from_bytes(data);

        REQUIRE(result.is_err());
        CHECK(result.error().code == pacs::error_codes::invalid_dicom_file);
    }

    SECTION("missing DICM prefix returns error") {
        std::vector<uint8_t> data(256, 0);  // Large enough but no DICM

        auto result = dicom_file::from_bytes(data);

        REQUIRE(result.is_err());
        CHECK(result.error().code == pacs::error_codes::missing_dicm_prefix);
    }

    SECTION("wrong DICM prefix returns error") {
        auto data = create_minimal_dicom_bytes();
        // Corrupt the DICM prefix
        data[128] = 'X';

        auto result = dicom_file::from_bytes(data);

        REQUIRE(result.is_err());
        CHECK(result.error().code == pacs::error_codes::missing_dicm_prefix);
    }
}

TEST_CASE("dicom_file reading from file", "[core][dicom_file]") {
    SECTION("non-existent file returns error") {
        auto result = dicom_file::open("/nonexistent/path/test.dcm");

        REQUIRE(result.is_err());
        CHECK(result.error().code == pacs::error_codes::file_not_found);
    }

    SECTION("valid file is read correctly") {
        auto data = create_minimal_dicom_bytes();
        auto temp_path = create_temp_file_path("test_read.dcm");

        // Write test file
        {
            std::ofstream file(temp_path, std::ios::binary);
            file.write(reinterpret_cast<const char*>(data.data()),
                       static_cast<std::streamsize>(data.size()));
        }

        auto result = dicom_file::open(temp_path);

        REQUIRE(result.is_ok());
        CHECK(result.value().dataset().get_string(tags::patient_name) == "DOE^JOHN");

        // Cleanup
        std::filesystem::remove(temp_path);
    }
}

// ============================================================================
// Creation Tests
// ============================================================================

TEST_CASE("dicom_file creation", "[core][dicom_file]") {
    SECTION("create generates correct meta information") {
        dicom_dataset ds;
        ds.set_string(tags::sop_class_uid, vr_type::UI, "1.2.840.10008.5.1.4.1.1.2");
        ds.set_string(tags::sop_instance_uid, vr_type::UI, "1.2.3.4.5.6.7.8.9");
        ds.set_string(tags::patient_name, vr_type::PN, "TEST^PATIENT");

        auto file = dicom_file::create(
            std::move(ds),
            transfer_syntax::explicit_vr_little_endian
        );

        // Check meta information was generated
        CHECK(file.meta_information().contains(tags::file_meta_information_version));
        CHECK(file.meta_information().contains(tags::media_storage_sop_class_uid));
        CHECK(file.meta_information().contains(tags::media_storage_sop_instance_uid));
        CHECK(file.meta_information().contains(tags::transfer_syntax_uid));
        CHECK(file.meta_information().contains(tags::implementation_class_uid));
        CHECK(file.meta_information().contains(tags::implementation_version_name));

        // Check values match
        CHECK(file.meta_information().get_string(tags::media_storage_sop_class_uid) ==
              "1.2.840.10008.5.1.4.1.1.2");
        CHECK(file.meta_information().get_string(tags::transfer_syntax_uid) ==
              "1.2.840.10008.1.2.1");
    }

    SECTION("create preserves dataset") {
        dicom_dataset ds;
        ds.set_string(tags::sop_class_uid, vr_type::UI, "1.2.840.10008.5.1.4.1.1.2");
        ds.set_string(tags::sop_instance_uid, vr_type::UI, "1.2.3.4.5");
        ds.set_string(tags::patient_name, vr_type::PN, "DOE^JOHN");
        ds.set_string(tags::patient_id, vr_type::LO, "12345");

        auto file = dicom_file::create(
            std::move(ds),
            transfer_syntax::explicit_vr_little_endian
        );

        CHECK(file.dataset().get_string(tags::patient_name) == "DOE^JOHN");
        CHECK(file.dataset().get_string(tags::patient_id) == "12345");
    }
}

// ============================================================================
// Writing Tests
// ============================================================================

TEST_CASE("dicom_file writing", "[core][dicom_file]") {
    SECTION("to_bytes produces valid output") {
        dicom_dataset ds;
        ds.set_string(tags::sop_class_uid, vr_type::UI, "1.2.840.10008.5.1.4.1.1.2");
        ds.set_string(tags::sop_instance_uid, vr_type::UI, "1.2.3.4.5.6.7.8.9");
        ds.set_string(tags::patient_name, vr_type::PN, "DOE^JOHN");

        auto file = dicom_file::create(
            std::move(ds),
            transfer_syntax::explicit_vr_little_endian
        );

        auto bytes = file.to_bytes();

        // Check minimum size
        CHECK(bytes.size() >= 132);  // Preamble + DICM

        // Check preamble is zeros
        for (size_t i = 0; i < 128; ++i) {
            CHECK(bytes[i] == 0);
        }

        // Check DICM prefix
        CHECK(bytes[128] == 'D');
        CHECK(bytes[129] == 'I');
        CHECK(bytes[130] == 'C');
        CHECK(bytes[131] == 'M');
    }

    SECTION("save creates valid file") {
        dicom_dataset ds;
        ds.set_string(tags::sop_class_uid, vr_type::UI, "1.2.840.10008.5.1.4.1.1.2");
        ds.set_string(tags::sop_instance_uid, vr_type::UI, "1.2.3.4.5");
        ds.set_string(tags::patient_name, vr_type::PN, "SMITH^JANE");

        auto file = dicom_file::create(
            std::move(ds),
            transfer_syntax::explicit_vr_little_endian
        );

        auto temp_path = create_temp_file_path("test_write.dcm");
        auto save_result = file.save(temp_path);

        REQUIRE(save_result.is_ok());
        CHECK(std::filesystem::exists(temp_path));

        // Cleanup
        std::filesystem::remove(temp_path);
    }
}

// ============================================================================
// Round-trip Tests
// ============================================================================

TEST_CASE("dicom_file round-trip", "[core][dicom_file]") {
    SECTION("create -> to_bytes -> from_bytes preserves data") {
        dicom_dataset ds;
        ds.set_string(tags::sop_class_uid, vr_type::UI, "1.2.840.10008.5.1.4.1.1.2");
        ds.set_string(tags::sop_instance_uid, vr_type::UI, "1.2.3.4.5.6.7.8.9");
        ds.set_string(tags::patient_name, vr_type::PN, "DOE^JOHN");
        ds.set_string(tags::patient_id, vr_type::LO, "12345");
        ds.set_string(tags::study_instance_uid, vr_type::UI, "2.3.4.5.6");
        ds.set_string(tags::series_instance_uid, vr_type::UI, "3.4.5.6.7");

        auto original = dicom_file::create(
            std::move(ds),
            transfer_syntax::explicit_vr_little_endian
        );

        // Convert to bytes and back
        auto bytes = original.to_bytes();
        auto restored_result = dicom_file::from_bytes(bytes);

        REQUIRE(restored_result.is_ok());
        auto& restored = restored_result.value();

        // Compare datasets
        CHECK(restored.dataset().get_string(tags::patient_name) ==
              original.dataset().get_string(tags::patient_name));
        CHECK(restored.dataset().get_string(tags::patient_id) ==
              original.dataset().get_string(tags::patient_id));
        CHECK(restored.sop_class_uid() == original.sop_class_uid());
        CHECK(restored.sop_instance_uid() == original.sop_instance_uid());
    }

    SECTION("save -> open preserves data") {
        dicom_dataset ds;
        ds.set_string(tags::sop_class_uid, vr_type::UI, "1.2.840.10008.5.1.4.1.1.2");
        ds.set_string(tags::sop_instance_uid, vr_type::UI, "1.2.3.4.5");
        ds.set_string(tags::patient_name, vr_type::PN, "ROUNDTRIP^TEST");
        ds.set_string(tags::modality, vr_type::CS, "CT");

        auto original = dicom_file::create(
            std::move(ds),
            transfer_syntax::explicit_vr_little_endian
        );

        auto temp_path = create_temp_file_path("test_roundtrip.dcm");

        // Save
        auto save_result = original.save(temp_path);
        REQUIRE(save_result.is_ok());

        // Open
        auto loaded_result = dicom_file::open(temp_path);
        REQUIRE(loaded_result.is_ok());
        auto& loaded = loaded_result.value();

        // Compare
        CHECK(loaded.dataset().get_string(tags::patient_name) == "ROUNDTRIP^TEST");
        CHECK(loaded.dataset().get_string(tags::modality) == "CT");
        CHECK(loaded.transfer_syntax() == transfer_syntax::explicit_vr_little_endian);

        // Cleanup
        std::filesystem::remove(temp_path);
    }
}

// ============================================================================
// Accessor Tests
// ============================================================================

TEST_CASE("dicom_file accessors", "[core][dicom_file]") {
    dicom_dataset ds;
    ds.set_string(tags::sop_class_uid, vr_type::UI, "1.2.840.10008.5.1.4.1.1.2");
    ds.set_string(tags::sop_instance_uid, vr_type::UI, "9.8.7.6.5");
    ds.set_string(tags::patient_name, vr_type::PN, "TEST^PATIENT");

    auto file = dicom_file::create(
        std::move(ds),
        transfer_syntax::explicit_vr_little_endian
    );

    SECTION("sop_class_uid returns correct value") {
        CHECK(file.sop_class_uid() == "1.2.840.10008.5.1.4.1.1.2");
    }

    SECTION("sop_instance_uid returns correct value") {
        CHECK(file.sop_instance_uid() == "9.8.7.6.5");
    }

    SECTION("transfer_syntax returns correct value") {
        auto ts = file.transfer_syntax();
        CHECK(ts.uid() == "1.2.840.10008.1.2.1");
        CHECK(ts.is_valid());
        CHECK(ts.is_supported());
    }

    SECTION("meta_information is accessible") {
        CHECK(file.meta_information().size() >= 5);  // At least 5 required elements
    }

    SECTION("dataset is accessible and modifiable") {
        file.dataset().set_string(tags::patient_age, vr_type::AS, "050Y");
        CHECK(file.dataset().get_string(tags::patient_age) == "050Y");
    }
}

// ============================================================================
// Error Code Tests
// ============================================================================

TEST_CASE("error codes are used correctly", "[core][dicom_file]") {
    SECTION("file_not_found error code") {
        auto result = dicom_file::open("/nonexistent/path/test.dcm");
        REQUIRE(result.is_err());
        CHECK(result.error().code == pacs::error_codes::file_not_found);
        CHECK_FALSE(result.error().message.empty());
    }

    SECTION("invalid_dicom_file error code") {
        std::vector<uint8_t> data(100, 0);
        auto result = dicom_file::from_bytes(data);
        REQUIRE(result.is_err());
        CHECK(result.error().code == pacs::error_codes::invalid_dicom_file);
    }

    SECTION("missing_dicm_prefix error code") {
        std::vector<uint8_t> data(256, 0);
        auto result = dicom_file::from_bytes(data);
        REQUIRE(result.is_err());
        CHECK(result.error().code == pacs::error_codes::missing_dicm_prefix);
    }
}

// ============================================================================
// Construction Tests
// ============================================================================

TEST_CASE("dicom_file construction", "[core][dicom_file]") {
    SECTION("default construction creates empty file") {
        dicom_file file;

        CHECK(file.meta_information().empty());
        CHECK(file.dataset().empty());
    }

    SECTION("copy construction") {
        dicom_dataset ds;
        ds.set_string(tags::sop_class_uid, vr_type::UI, "1.2.3");
        ds.set_string(tags::sop_instance_uid, vr_type::UI, "4.5.6");
        ds.set_string(tags::patient_name, vr_type::PN, "COPY^TEST");

        auto original = dicom_file::create(
            std::move(ds),
            transfer_syntax::explicit_vr_little_endian
        );

        dicom_file copy{original};

        CHECK(copy.dataset().get_string(tags::patient_name) == "COPY^TEST");
        CHECK(original.dataset().get_string(tags::patient_name) == "COPY^TEST");
    }

    SECTION("move construction") {
        dicom_dataset ds;
        ds.set_string(tags::sop_class_uid, vr_type::UI, "1.2.3");
        ds.set_string(tags::sop_instance_uid, vr_type::UI, "4.5.6");
        ds.set_string(tags::patient_name, vr_type::PN, "MOVE^TEST");

        auto original = dicom_file::create(
            std::move(ds),
            transfer_syntax::explicit_vr_little_endian
        );

        dicom_file moved{std::move(original)};

        CHECK(moved.dataset().get_string(tags::patient_name) == "MOVE^TEST");
    }
}

// ============================================================================
// Transfer Syntax Conversion Tests (Issue #280)
// ============================================================================

TEST_CASE("dicom_file transfer syntax conversion", "[core][dicom_file][conversion]") {
    SECTION("convert from Explicit VR LE to Implicit VR LE") {
        // Create original file with Explicit VR LE
        dicom_dataset ds;
        ds.set_string(tags::sop_class_uid, vr_type::UI, "1.2.840.10008.5.1.4.1.1.2");
        ds.set_string(tags::sop_instance_uid, vr_type::UI, "1.2.3.4.5.6.7.8.9");
        ds.set_string(tags::patient_name, vr_type::PN, "CONVERT^TEST");
        ds.set_string(tags::patient_id, vr_type::LO, "CONV123");
        ds.set_string(tags::modality, vr_type::CS, "CT");

        auto original = dicom_file::create(
            ds,  // copy, not move
            transfer_syntax::explicit_vr_little_endian
        );

        // Convert to Implicit VR LE
        auto converted = dicom_file::create(
            original.dataset(),
            transfer_syntax::implicit_vr_little_endian
        );

        // Verify transfer syntax changed
        CHECK(converted.transfer_syntax() == transfer_syntax::implicit_vr_little_endian);
        CHECK(converted.meta_information().get_string(tags::transfer_syntax_uid) ==
              "1.2.840.10008.1.2");

        // Verify data preserved
        CHECK(converted.dataset().get_string(tags::patient_name) == "CONVERT^TEST");
        CHECK(converted.dataset().get_string(tags::patient_id) == "CONV123");
        CHECK(converted.dataset().get_string(tags::modality) == "CT");
    }

    SECTION("convert from Implicit VR LE to Explicit VR LE") {
        // Create original file with Implicit VR LE
        dicom_dataset ds;
        ds.set_string(tags::sop_class_uid, vr_type::UI, "1.2.840.10008.5.1.4.1.1.2");
        ds.set_string(tags::sop_instance_uid, vr_type::UI, "9.8.7.6.5.4.3.2.1");
        ds.set_string(tags::patient_name, vr_type::PN, "IMPLICIT^TO^EXPLICIT");
        ds.set_string(tags::study_description, vr_type::LO, "Test Study");

        auto original = dicom_file::create(
            ds,
            transfer_syntax::implicit_vr_little_endian
        );

        // Convert to Explicit VR LE
        auto converted = dicom_file::create(
            original.dataset(),
            transfer_syntax::explicit_vr_little_endian
        );

        // Verify transfer syntax changed
        CHECK(converted.transfer_syntax() == transfer_syntax::explicit_vr_little_endian);

        // Verify data preserved
        CHECK(converted.dataset().get_string(tags::patient_name) == "IMPLICIT^TO^EXPLICIT");
        CHECK(converted.dataset().get_string(tags::study_description) == "Test Study");
    }

    SECTION("round-trip conversion preserves all data") {
        // Create original with Explicit VR LE
        dicom_dataset ds;
        ds.set_string(tags::sop_class_uid, vr_type::UI, "1.2.840.10008.5.1.4.1.1.2");
        ds.set_string(tags::sop_instance_uid, vr_type::UI, "1.1.1.1.1");
        ds.set_string(tags::patient_name, vr_type::PN, "ROUNDTRIP^CONVERSION");
        ds.set_string(tags::patient_id, vr_type::LO, "RT001");
        ds.set_string(tags::study_instance_uid, vr_type::UI, "2.2.2.2.2");
        ds.set_string(tags::series_instance_uid, vr_type::UI, "3.3.3.3.3");
        ds.set_string(tags::modality, vr_type::CS, "MR");
        ds.set_numeric<uint16_t>(tags::rows, vr_type::US, 512);
        ds.set_numeric<uint16_t>(tags::columns, vr_type::US, 512);

        auto original = dicom_file::create(
            ds,
            transfer_syntax::explicit_vr_little_endian
        );

        // Convert to Implicit VR LE
        auto implicit_file = dicom_file::create(
            original.dataset(),
            transfer_syntax::implicit_vr_little_endian
        );

        // Convert back to Explicit VR LE
        auto back_to_explicit = dicom_file::create(
            implicit_file.dataset(),
            transfer_syntax::explicit_vr_little_endian
        );

        // Verify all data preserved after round-trip
        CHECK(back_to_explicit.dataset().get_string(tags::patient_name) ==
              original.dataset().get_string(tags::patient_name));
        CHECK(back_to_explicit.dataset().get_string(tags::patient_id) ==
              original.dataset().get_string(tags::patient_id));
        CHECK(back_to_explicit.dataset().get_string(tags::modality) ==
              original.dataset().get_string(tags::modality));

        auto orig_rows = original.dataset().get_numeric<uint16_t>(tags::rows);
        auto conv_rows = back_to_explicit.dataset().get_numeric<uint16_t>(tags::rows);
        REQUIRE(orig_rows.has_value());
        REQUIRE(conv_rows.has_value());
        CHECK(*conv_rows == *orig_rows);
    }

    SECTION("save and reload with different transfer syntax") {
        dicom_dataset ds;
        ds.set_string(tags::sop_class_uid, vr_type::UI, "1.2.840.10008.5.1.4.1.1.2");
        ds.set_string(tags::sop_instance_uid, vr_type::UI, "5.5.5.5.5");
        ds.set_string(tags::patient_name, vr_type::PN, "FILE^CONVERSION");

        // Create with Explicit VR LE
        auto original = dicom_file::create(
            ds,
            transfer_syntax::explicit_vr_little_endian
        );

        // Convert and save as Implicit VR LE
        auto converted = dicom_file::create(
            original.dataset(),
            transfer_syntax::implicit_vr_little_endian
        );

        auto temp_path = create_temp_file_path("test_ts_conversion.dcm");
        auto save_result = converted.save(temp_path);
        REQUIRE(save_result.is_ok());

        // Reload and verify
        auto loaded_result = dicom_file::open(temp_path);
        REQUIRE(loaded_result.is_ok());
        auto& loaded = loaded_result.value();

        CHECK(loaded.transfer_syntax() == transfer_syntax::implicit_vr_little_endian);
        CHECK(loaded.dataset().get_string(tags::patient_name) == "FILE^CONVERSION");

        // Cleanup
        std::filesystem::remove(temp_path);
    }

    SECTION("convert to Explicit VR Big Endian") {
        dicom_dataset ds;
        ds.set_string(tags::sop_class_uid, vr_type::UI, "1.2.840.10008.5.1.4.1.1.2");
        ds.set_string(tags::sop_instance_uid, vr_type::UI, "6.6.6.6.6");
        ds.set_string(tags::patient_name, vr_type::PN, "BIGENDIAN^TEST");

        auto original = dicom_file::create(
            ds,
            transfer_syntax::explicit_vr_little_endian
        );

        // Convert to Explicit VR Big Endian
        auto converted = dicom_file::create(
            original.dataset(),
            transfer_syntax::explicit_vr_big_endian
        );

        CHECK(converted.transfer_syntax() == transfer_syntax::explicit_vr_big_endian);
        CHECK(converted.dataset().get_string(tags::patient_name) == "BIGENDIAN^TEST");
    }
}
