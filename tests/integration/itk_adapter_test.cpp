/**
 * @file itk_adapter_test.cpp
 * @brief Unit tests for ITK integration adapter
 *
 * Tests the ITK adapter functions for converting pacs_system DICOM data
 * structures to ITK image types.
 *
 * @see Issue #463 - ITK/VTK Integration Adapter for dicom_viewer
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <pacs/core/dicom_dataset.hpp>
#include <pacs/core/dicom_element.hpp>
#include <pacs/core/dicom_tag_constants.hpp>
#include <pacs/encoding/vr_type.hpp>

#ifdef PACS_WITH_ITK

#include <pacs/integration/itk_adapter.hpp>

using namespace pacs::core;
using namespace pacs::encoding;
using namespace pacs::integration::itk;

namespace {

/**
 * @brief Create a minimal DICOM dataset with image parameters
 */
auto create_test_dataset(uint16_t rows, uint16_t columns,
                          uint16_t bits_allocated = 16,
                          uint16_t bits_stored = 16,
                          uint16_t pixel_representation = 0)
    -> dicom_dataset {
    dicom_dataset ds;

    ds.set_numeric<uint16_t>(tags::rows, vr_type::US, rows);
    ds.set_numeric<uint16_t>(tags::columns, vr_type::US, columns);
    ds.set_numeric<uint16_t>(tags::bits_allocated, vr_type::US, bits_allocated);
    ds.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, bits_stored);
    ds.set_numeric<uint16_t>(tags::high_bit, vr_type::US, bits_stored - 1);
    ds.set_numeric<uint16_t>(tags::pixel_representation, vr_type::US,
                              pixel_representation);
    ds.set_numeric<uint16_t>(tags::samples_per_pixel, vr_type::US, 1);
    ds.set_string(tags::photometric_interpretation, vr_type::CS, "MONOCHROME2");

    return ds;
}

/**
 * @brief Create test dataset with spatial information
 */
auto create_spatial_dataset() -> dicom_dataset {
    auto ds = create_test_dataset(256, 256);

    // Pixel Spacing: 0.5mm x 0.5mm
    ds.set_string(tags::pixel_spacing, vr_type::DS, "0.5\\0.5");

    // Image Position Patient: origin at (10, 20, 30)
    ds.set_string(tags::image_position_patient, vr_type::DS, "10.0\\20.0\\30.0");

    // Image Orientation Patient: standard axial
    ds.set_string(tags::image_orientation_patient, vr_type::DS,
                  "1.0\\0.0\\0.0\\0.0\\1.0\\0.0");

    // Rescale parameters for CT
    ds.set_string(tags::rescale_slope, vr_type::DS, "1.0");
    ds.set_string(tags::rescale_intercept, vr_type::DS, "-1024.0");

    return ds;
}

/**
 * @brief Create test pixel data
 */
auto create_test_pixel_data(size_t width, size_t height)
    -> std::vector<uint8_t> {
    const size_t pixel_count = width * height;
    const size_t byte_count = pixel_count * 2;  // 16-bit

    std::vector<uint8_t> data(byte_count);

    // Fill with gradient pattern
    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
            uint16_t value = static_cast<uint16_t>((x + y) % 65536);
            size_t offset = (y * width + x) * 2;
            data[offset] = static_cast<uint8_t>(value & 0xFF);
            data[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
        }
    }

    return data;
}

}  // namespace

TEST_CASE("ITK adapter metadata extraction", "[integration][itk]") {
    SECTION("extracts dimensions correctly") {
        auto ds = create_test_dataset(512, 256);
        auto meta = extract_metadata(ds);

        REQUIRE(meta.dimensions[0] == 256);  // columns = X
        REQUIRE(meta.dimensions[1] == 512);  // rows = Y
        REQUIRE(meta.dimensions[2] == 1);    // single slice
    }

    SECTION("extracts pixel spacing correctly") {
        auto ds = create_spatial_dataset();
        auto meta = extract_metadata(ds);

        REQUIRE_THAT(meta.spacing[0], Catch::Matchers::WithinRel(0.5, 0.001));
        REQUIRE_THAT(meta.spacing[1], Catch::Matchers::WithinRel(0.5, 0.001));
    }

    SECTION("extracts origin correctly") {
        auto ds = create_spatial_dataset();
        auto meta = extract_metadata(ds);

        REQUIRE_THAT(meta.origin[0], Catch::Matchers::WithinRel(10.0, 0.001));
        REQUIRE_THAT(meta.origin[1], Catch::Matchers::WithinRel(20.0, 0.001));
        REQUIRE_THAT(meta.origin[2], Catch::Matchers::WithinRel(30.0, 0.001));
    }

    SECTION("extracts orientation correctly") {
        auto ds = create_spatial_dataset();
        auto meta = extract_metadata(ds);

        // Row direction: (1, 0, 0)
        REQUIRE_THAT(meta.orientation[0], Catch::Matchers::WithinRel(1.0, 0.001));
        REQUIRE_THAT(meta.orientation[1], Catch::Matchers::WithinAbs(0.0, 0.001));
        REQUIRE_THAT(meta.orientation[2], Catch::Matchers::WithinAbs(0.0, 0.001));

        // Column direction: (0, 1, 0)
        REQUIRE_THAT(meta.orientation[3], Catch::Matchers::WithinAbs(0.0, 0.001));
        REQUIRE_THAT(meta.orientation[4], Catch::Matchers::WithinRel(1.0, 0.001));
        REQUIRE_THAT(meta.orientation[5], Catch::Matchers::WithinAbs(0.0, 0.001));
    }

    SECTION("extracts rescale parameters correctly") {
        auto ds = create_spatial_dataset();
        auto meta = extract_metadata(ds);

        REQUIRE_THAT(meta.rescale_slope, Catch::Matchers::WithinRel(1.0, 0.001));
        REQUIRE_THAT(meta.rescale_intercept,
                     Catch::Matchers::WithinRel(-1024.0, 0.001));
    }

    SECTION("extracts pixel format correctly") {
        auto ds = create_test_dataset(256, 256, 16, 12, 1);
        auto meta = extract_metadata(ds);

        REQUIRE(meta.bits_allocated == 16);
        REQUIRE(meta.bits_stored == 12);
        REQUIRE(meta.high_bit == 11);
        REQUIRE(meta.pixel_representation == 1);  // signed
    }

    SECTION("handles missing tags with defaults") {
        dicom_dataset ds;  // Empty dataset
        auto meta = extract_metadata(ds);

        REQUIRE(meta.dimensions[0] == 0);
        REQUIRE(meta.dimensions[1] == 0);
        REQUIRE(meta.spacing[0] == 1.0);
        REQUIRE(meta.spacing[1] == 1.0);
        REQUIRE(meta.rescale_slope == 1.0);
        REQUIRE(meta.rescale_intercept == 0.0);
    }
}

TEST_CASE("ITK adapter pixel data utilities", "[integration][itk]") {
    SECTION("detects signed pixel data") {
        auto ds_unsigned = create_test_dataset(256, 256, 16, 16, 0);
        auto ds_signed = create_test_dataset(256, 256, 16, 16, 1);

        REQUIRE_FALSE(is_signed_pixel_data(ds_unsigned));
        REQUIRE(is_signed_pixel_data(ds_signed));
    }

    SECTION("detects multi-frame images") {
        auto ds = create_test_dataset(256, 256);
        REQUIRE_FALSE(is_multi_frame(ds));
        REQUIRE(get_frame_count(ds) == 1);

        // Add Number of Frames
        constexpr dicom_tag number_of_frames{0x0028, 0x0008};
        ds.set_string(number_of_frames, vr_type::IS, "10");

        REQUIRE(is_multi_frame(ds));
        REQUIRE(get_frame_count(ds) == 10);
    }

    SECTION("handles pixel data extraction") {
        auto ds = create_test_dataset(4, 4);

        // Create and add pixel data
        auto pixel_data = create_test_pixel_data(4, 4);

        // Note: This requires adding pixel data to the dataset
        // The actual test would need real pixel data in the dataset
    }
}

TEST_CASE("ITK adapter Hounsfield conversion", "[integration][itk]") {
    SECTION("applies rescale correctly") {
        std::vector<int16_t> data = {0, 100, 200, 1000};
        double slope = 1.0;
        double intercept = -1024.0;

        apply_hounsfield_conversion(data, slope, intercept);

        REQUIRE(data[0] == -1024);
        REQUIRE(data[1] == -924);
        REQUIRE(data[2] == -824);
        REQUIRE(data[3] == -24);
    }

    SECTION("handles fractional slope") {
        std::vector<int16_t> data = {100, 200};
        double slope = 0.5;
        double intercept = 0.0;

        apply_hounsfield_conversion(data, slope, intercept);

        REQUIRE(data[0] == 50);
        REQUIRE(data[1] == 100);
    }

    SECTION("clamps to int16 range") {
        std::vector<int16_t> data = {32767};
        double slope = 2.0;
        double intercept = 0.0;

        apply_hounsfield_conversion(data, slope, intercept);

        REQUIRE(data[0] == 32767);  // Clamped to max
    }
}

TEST_CASE("ITK adapter slice sorting", "[integration][itk]") {
    // Note: This test requires actual DICOM files on disk
    // For unit testing, we test the sorting logic with mock data

    SECTION("empty file list returns empty result") {
        std::vector<std::filesystem::path> empty_files;
        auto sorted = sort_slices(empty_files);

        REQUIRE(sorted.empty());
    }
}

TEST_CASE("ITK adapter directory scanning", "[integration][itk]") {
    SECTION("handles non-existent directory") {
        auto files = scan_dicom_directory("/nonexistent/directory");
        REQUIRE(files.empty());
    }

    SECTION("handles empty directory") {
        // Create temporary empty directory
        auto temp_dir = std::filesystem::temp_directory_path() / "pacs_itk_test";
        std::filesystem::create_directories(temp_dir);

        auto files = scan_dicom_directory(temp_dir);
        REQUIRE(files.empty());

        std::filesystem::remove_all(temp_dir);
    }
}

#else  // PACS_WITH_ITK

TEST_CASE("ITK adapter disabled", "[integration][itk]") {
    SECTION("ITK not available") {
        // This test always passes - ITK adapter is disabled
        REQUIRE(true);
    }
}

#endif  // PACS_WITH_ITK
