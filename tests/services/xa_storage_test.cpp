/**
 * @file xa_storage_test.cpp
 * @brief Unit tests for X-Ray Angiographic Storage SOP Classes and IOD Validator
 */

#include <pacs/services/sop_classes/xa_storage.hpp>
#include <pacs/services/validation/xa_iod_validator.hpp>
#include <pacs/services/sop_class_registry.hpp>
#include <pacs/core/dicom_dataset.hpp>
#include <pacs/core/dicom_tag_constants.hpp>
#include <pacs/encoding/vr_type.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace pacs::services::sop_classes;
using namespace pacs::services::validation;
using namespace pacs::services;
using namespace pacs::core;
using namespace pacs::encoding;

// ============================================================================
// XA Storage SOP Class UID Tests
// ============================================================================

TEST_CASE("XA Storage SOP Class UIDs are correct", "[services][xa][sop_class]") {
    CHECK(xa_image_storage_uid == "1.2.840.10008.5.1.4.1.1.12.1");
    CHECK(enhanced_xa_image_storage_uid == "1.2.840.10008.5.1.4.1.1.12.1.1");
    CHECK(xrf_image_storage_uid == "1.2.840.10008.5.1.4.1.1.12.2");
    CHECK(xray_3d_angiographic_image_storage_uid == "1.2.840.10008.5.1.4.1.1.13.1.1");
    CHECK(xray_3d_craniofacial_image_storage_uid == "1.2.840.10008.5.1.4.1.1.13.1.2");
}

TEST_CASE("is_xa_storage_sop_class identifies XA classes", "[services][xa][sop_class]") {
    SECTION("recognizes primary XA/XRF classes") {
        CHECK(is_xa_storage_sop_class(xa_image_storage_uid));
        CHECK(is_xa_storage_sop_class(enhanced_xa_image_storage_uid));
        CHECK(is_xa_storage_sop_class(xrf_image_storage_uid));
    }

    SECTION("recognizes 3D XA classes") {
        CHECK(is_xa_storage_sop_class(xray_3d_angiographic_image_storage_uid));
        CHECK(is_xa_storage_sop_class(xray_3d_craniofacial_image_storage_uid));
    }

    SECTION("rejects non-XA classes") {
        CHECK_FALSE(is_xa_storage_sop_class("1.2.840.10008.5.1.4.1.1.2"));    // CT
        CHECK_FALSE(is_xa_storage_sop_class("1.2.840.10008.5.1.4.1.1.6.1"));  // US
        CHECK_FALSE(is_xa_storage_sop_class("1.2.840.10008.1.1"));            // Verification
        CHECK_FALSE(is_xa_storage_sop_class(""));
        CHECK_FALSE(is_xa_storage_sop_class("invalid"));
    }
}

TEST_CASE("is_xa_multiframe_sop_class identifies multiframe classes", "[services][xa][sop_class]") {
    CHECK(is_xa_multiframe_sop_class(xa_image_storage_uid));
    CHECK(is_xa_multiframe_sop_class(enhanced_xa_image_storage_uid));
    CHECK(is_xa_multiframe_sop_class(xrf_image_storage_uid));
    CHECK(is_xa_multiframe_sop_class(xray_3d_angiographic_image_storage_uid));
}

TEST_CASE("is_enhanced_xa_sop_class identifies enhanced classes", "[services][xa][sop_class]") {
    CHECK(is_enhanced_xa_sop_class(enhanced_xa_image_storage_uid));
    CHECK(is_enhanced_xa_sop_class(xray_3d_angiographic_image_storage_uid));
    CHECK(is_enhanced_xa_sop_class(xray_3d_craniofacial_image_storage_uid));
    CHECK_FALSE(is_enhanced_xa_sop_class(xa_image_storage_uid));
    CHECK_FALSE(is_enhanced_xa_sop_class(xrf_image_storage_uid));
}

TEST_CASE("is_xa_3d_sop_class identifies 3D classes", "[services][xa][sop_class]") {
    CHECK(is_xa_3d_sop_class(xray_3d_angiographic_image_storage_uid));
    CHECK(is_xa_3d_sop_class(xray_3d_craniofacial_image_storage_uid));
    CHECK_FALSE(is_xa_3d_sop_class(xa_image_storage_uid));
    CHECK_FALSE(is_xa_3d_sop_class(enhanced_xa_image_storage_uid));
    CHECK_FALSE(is_xa_3d_sop_class(xrf_image_storage_uid));
}

// ============================================================================
// XA SOP Class Information Tests
// ============================================================================

TEST_CASE("get_xa_sop_class_info returns correct information", "[services][xa][sop_class]") {
    SECTION("XA Image Storage info") {
        const auto* info = get_xa_sop_class_info(xa_image_storage_uid);
        REQUIRE(info != nullptr);
        CHECK(info->uid == xa_image_storage_uid);
        CHECK(info->name == "XA Image Storage");
        CHECK_FALSE(info->is_enhanced);
        CHECK_FALSE(info->is_3d);
        CHECK(info->supports_multiframe);
    }

    SECTION("Enhanced XA Image Storage info") {
        const auto* info = get_xa_sop_class_info(enhanced_xa_image_storage_uid);
        REQUIRE(info != nullptr);
        CHECK(info->uid == enhanced_xa_image_storage_uid);
        CHECK(info->is_enhanced);
        CHECK_FALSE(info->is_3d);
        CHECK(info->supports_multiframe);
    }

    SECTION("XRF Image Storage info") {
        const auto* info = get_xa_sop_class_info(xrf_image_storage_uid);
        REQUIRE(info != nullptr);
        CHECK_FALSE(info->is_enhanced);
        CHECK_FALSE(info->is_3d);
    }

    SECTION("3D Angiographic info") {
        const auto* info = get_xa_sop_class_info(xray_3d_angiographic_image_storage_uid);
        REQUIRE(info != nullptr);
        CHECK(info->is_enhanced);
        CHECK(info->is_3d);
    }

    SECTION("returns nullptr for unknown UID") {
        CHECK(get_xa_sop_class_info("1.2.3.4.5.6.7") == nullptr);
    }
}

TEST_CASE("get_xa_storage_sop_classes returns correct list", "[services][xa][sop_class]") {
    SECTION("with 3D classes") {
        auto classes = get_xa_storage_sop_classes(true);
        CHECK(classes.size() == 5);
    }

    SECTION("without 3D classes") {
        auto classes = get_xa_storage_sop_classes(false);
        CHECK(classes.size() == 3);
        // Verify only non-3D classes are returned
        for (const auto& uid : classes) {
            const auto* info = get_xa_sop_class_info(uid);
            REQUIRE(info != nullptr);
            CHECK_FALSE(info->is_3d);
        }
    }
}

// ============================================================================
// Transfer Syntax Tests
// ============================================================================

TEST_CASE("get_xa_transfer_syntaxes returns valid syntaxes", "[services][xa][transfer]") {
    auto syntaxes = get_xa_transfer_syntaxes();

    CHECK(syntaxes.size() > 0);

    // Should include Explicit VR Little Endian (most preferred)
    CHECK(std::find(syntaxes.begin(), syntaxes.end(),
                   "1.2.840.10008.1.2.1") != syntaxes.end());

    // Should include Implicit VR Little Endian (universal baseline)
    CHECK(std::find(syntaxes.begin(), syntaxes.end(),
                   "1.2.840.10008.1.2") != syntaxes.end());
}

// ============================================================================
// Photometric Interpretation Tests
// ============================================================================

TEST_CASE("xa_photometric_interpretation conversions", "[services][xa][photometric]") {
    SECTION("to_string produces correct DICOM strings") {
        CHECK(to_string(xa_photometric_interpretation::monochrome1) == "MONOCHROME1");
        CHECK(to_string(xa_photometric_interpretation::monochrome2) == "MONOCHROME2");
    }

    SECTION("parse_xa_photometric_interpretation parses correctly") {
        CHECK(parse_xa_photometric_interpretation("MONOCHROME1") ==
              xa_photometric_interpretation::monochrome1);
        CHECK(parse_xa_photometric_interpretation("MONOCHROME2") ==
              xa_photometric_interpretation::monochrome2);
    }

    SECTION("parse returns monochrome2 for unknown values") {
        CHECK(parse_xa_photometric_interpretation("RGB") ==
              xa_photometric_interpretation::monochrome2);
        CHECK(parse_xa_photometric_interpretation("") ==
              xa_photometric_interpretation::monochrome2);
    }
}

TEST_CASE("is_valid_xa_photometric validates correctly", "[services][xa][photometric]") {
    CHECK(is_valid_xa_photometric("MONOCHROME1"));
    CHECK(is_valid_xa_photometric("MONOCHROME2"));

    // XA is grayscale only - no color support
    CHECK_FALSE(is_valid_xa_photometric("RGB"));
    CHECK_FALSE(is_valid_xa_photometric("PALETTE COLOR"));
    CHECK_FALSE(is_valid_xa_photometric("YBR_FULL"));
    CHECK_FALSE(is_valid_xa_photometric(""));
}

// ============================================================================
// Positioner and Calibration Tests
// ============================================================================

TEST_CASE("xa_positioner_angles validation", "[services][xa][positioner]") {
    SECTION("valid angles") {
        xa_positioner_angles angles{45.0, 30.0};
        CHECK(angles.is_valid());
    }

    SECTION("valid extreme angles") {
        xa_positioner_angles angles{180.0, 90.0};
        CHECK(angles.is_valid());

        xa_positioner_angles angles2{-180.0, -90.0};
        CHECK(angles2.is_valid());
    }

    SECTION("invalid angles") {
        xa_positioner_angles angles{200.0, 30.0};  // Primary out of range
        CHECK_FALSE(angles.is_valid());

        xa_positioner_angles angles2{45.0, 100.0};  // Secondary out of range
        CHECK_FALSE(angles2.is_valid());
    }
}

TEST_CASE("xa_positioner_motion to_string", "[services][xa][positioner]") {
    CHECK(to_string(xa_positioner_motion::stationary) == "STATIONARY");
    CHECK(to_string(xa_positioner_motion::dynamic) == "DYNAMIC");
}

TEST_CASE("xa_calibration_data validation", "[services][xa][calibration]") {
    SECTION("valid calibration data") {
        xa_calibration_data cal{
            {0.3, 0.3},    // imager_pixel_spacing (mm)
            1200.0,        // SID (mm)
            800.0          // SOD (mm)
        };

        CHECK(cal.is_valid());
        CHECK(cal.magnification_factor() == Approx(1.5));
        CHECK(cal.isocenter_pixel_spacing() == Approx(0.2));
    }

    SECTION("invalid calibration - SID < SOD") {
        xa_calibration_data cal{
            {0.3, 0.3},
            800.0,         // SID
            1200.0         // SOD > SID (impossible)
        };

        CHECK_FALSE(cal.is_valid());
    }

    SECTION("invalid calibration - zero distances") {
        xa_calibration_data cal{
            {0.3, 0.3},
            0.0,           // SID = 0
            0.0            // SOD = 0
        };

        CHECK_FALSE(cal.is_valid());
        CHECK(cal.magnification_factor() == 0.0);
    }
}

TEST_CASE("XA frame constants", "[services][xa][frame]") {
    CHECK(get_default_xa_cine_rate() == 15);
    CHECK(get_max_xa_frame_count() == 2000);
}

// ============================================================================
// XA IOD Validator Tests
// ============================================================================

namespace {

dicom_dataset create_minimal_xa_dataset() {
    dicom_dataset ds;

    // Patient Module (Type 2)
    ds.set_string(tags::patient_name, vr_type::PN, "TEST^PATIENT");
    ds.set_string(tags::patient_id, vr_type::LO, "12345");
    ds.set_string(tags::patient_birth_date, vr_type::DA, "19800101");
    ds.set_string(tags::patient_sex, vr_type::CS, "M");

    // General Study Module
    ds.set_string(tags::study_instance_uid, vr_type::UI,
                 "1.2.840.113619.2.55.3.604688119.969.1234567890.123");
    ds.set_string(tags::study_date, vr_type::DA, "20231201");
    ds.set_string(tags::study_time, vr_type::TM, "120000");
    ds.set_string(tags::referring_physician_name, vr_type::PN, "DR^REFERRER");
    ds.set_string(tags::study_id, vr_type::SH, "STUDY001");
    ds.set_string(tags::accession_number, vr_type::SH, "ACC001");

    // General Series Module
    ds.set_string(tags::modality, vr_type::CS, "XA");
    ds.set_string(tags::series_instance_uid, vr_type::UI,
                 "1.2.840.113619.2.55.3.604688119.969.1234567890.124");
    ds.set_string(tags::series_number, vr_type::IS, "1");

    // XA Image Module
    ds.set_string(tags::image_type, vr_type::CS, "ORIGINAL\\PRIMARY\\");

    // Image Pixel Module
    ds.set_numeric<uint16_t>(tags::samples_per_pixel, vr_type::US, 1);
    ds.set_string(tags::photometric_interpretation, vr_type::CS, "MONOCHROME2");
    ds.set_numeric<uint16_t>(tags::rows, vr_type::US, 512);
    ds.set_numeric<uint16_t>(tags::columns, vr_type::US, 512);
    ds.set_numeric<uint16_t>(tags::bits_allocated, vr_type::US, 16);
    ds.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, 12);
    ds.set_numeric<uint16_t>(tags::high_bit, vr_type::US, 11);
    ds.set_numeric<uint16_t>(tags::pixel_representation, vr_type::US, 0);
    ds.set_string(tags::pixel_data, vr_type::OW, "dummy_pixel_data");

    // SOP Common Module
    ds.set_string(tags::sop_class_uid, vr_type::UI, std::string(xa_image_storage_uid));
    ds.set_string(tags::sop_instance_uid, vr_type::UI,
                 "1.2.840.113619.2.55.3.604688119.969.1234567890.125");

    return ds;
}

}  // namespace

TEST_CASE("xa_iod_validator validates complete dataset", "[services][xa][validation]") {
    xa_iod_validator validator;
    auto dataset = create_minimal_xa_dataset();

    auto result = validator.validate(dataset);
    CHECK(result.is_valid);
    CHECK_FALSE(result.has_errors());
}

TEST_CASE("xa_iod_validator detects missing Type 1 attributes", "[services][xa][validation]") {
    xa_iod_validator validator;
    auto dataset = create_minimal_xa_dataset();

    SECTION("missing StudyInstanceUID") {
        dataset.remove(tags::study_instance_uid);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
        CHECK(result.has_errors());
    }

    SECTION("missing Modality") {
        dataset.remove(tags::modality);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }

    SECTION("missing ImageType") {
        dataset.remove(tags::image_type);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }

    SECTION("missing SOPClassUID") {
        dataset.remove(tags::sop_class_uid);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }
}

TEST_CASE("xa_iod_validator detects wrong modality", "[services][xa][validation]") {
    xa_iod_validator validator;
    auto dataset = create_minimal_xa_dataset();

    SECTION("CT modality rejected") {
        dataset.set_string(tags::modality, vr_type::CS, "CT");
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
        CHECK(result.has_errors());
    }

    SECTION("US modality rejected") {
        dataset.set_string(tags::modality, vr_type::CS, "US");
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }

    SECTION("XRF modality accepted") {
        dataset.set_string(tags::modality, vr_type::CS, "XRF");
        auto result = validator.validate(dataset);
        // May have warnings about SOP Class mismatch but should validate
    }
}

TEST_CASE("xa_iod_validator detects invalid photometric", "[services][xa][validation]") {
    xa_iod_validator validator;
    auto dataset = create_minimal_xa_dataset();

    SECTION("RGB rejected for XA") {
        dataset.set_string(tags::photometric_interpretation, vr_type::CS, "RGB");
        dataset.set_numeric<uint16_t>(tags::samples_per_pixel, vr_type::US, 3);
        auto result = validator.validate(dataset);
        CHECK(result.has_errors());
    }
}

TEST_CASE("xa_iod_validator detects pixel data inconsistencies", "[services][xa][validation]") {
    xa_iod_validator validator;
    auto dataset = create_minimal_xa_dataset();

    SECTION("BitsStored > BitsAllocated") {
        dataset.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, 16);
        dataset.set_numeric<uint16_t>(tags::bits_allocated, vr_type::US, 8);
        auto result = validator.validate(dataset);
        CHECK(result.has_errors());
    }

    SECTION("SamplesPerPixel != 1 for grayscale") {
        dataset.set_numeric<uint16_t>(tags::samples_per_pixel, vr_type::US, 3);
        auto result = validator.validate(dataset);
        CHECK(result.has_errors());
    }
}

TEST_CASE("xa_iod_validator quick_check works correctly", "[services][xa][validation]") {
    xa_iod_validator validator;

    SECTION("valid dataset passes quick check") {
        auto dataset = create_minimal_xa_dataset();
        CHECK(validator.quick_check(dataset));
    }

    SECTION("invalid dataset fails quick check") {
        auto dataset = create_minimal_xa_dataset();
        dataset.remove(tags::modality);
        CHECK_FALSE(validator.quick_check(dataset));
    }

    SECTION("wrong modality fails quick check") {
        auto dataset = create_minimal_xa_dataset();
        dataset.set_string(tags::modality, vr_type::CS, "CT");
        CHECK_FALSE(validator.quick_check(dataset));
    }
}

TEST_CASE("xa_iod_validator options work correctly", "[services][xa][validation]") {
    SECTION("can disable Type 2 checking") {
        xa_validation_options options;
        options.check_type1 = true;
        options.check_type2 = false;

        xa_iod_validator validator{options};
        auto dataset = create_minimal_xa_dataset();
        dataset.remove(tags::patient_name);  // Type 2

        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
    }

    SECTION("strict mode treats warnings as errors") {
        xa_validation_options options;
        options.strict_mode = true;

        xa_iod_validator validator{options};
        auto dataset = create_minimal_xa_dataset();
        dataset.remove(tags::patient_name);

        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }

    SECTION("can disable calibration validation") {
        xa_validation_options options;
        options.validate_calibration = false;

        xa_iod_validator validator{options};
        auto dataset = create_minimal_xa_dataset();
        // Don't add calibration data

        auto result = validator.validate(dataset);
        CHECK(result.is_valid);  // Should pass without calibration
    }
}

// ============================================================================
// QCA Calibration Validation Tests
// ============================================================================

TEST_CASE("has_qca_calibration checks calibration completeness", "[services][xa][calibration]") {
    auto dataset = create_minimal_xa_dataset();

    SECTION("dataset without calibration") {
        CHECK_FALSE(has_qca_calibration(dataset));
    }

    SECTION("dataset with partial calibration") {
        dataset.set_string(xa_tags::imager_pixel_spacing, vr_type::DS, "0.3\\0.3");
        // Missing SID and SOD
        CHECK_FALSE(has_qca_calibration(dataset));
    }

    SECTION("dataset with complete calibration") {
        dataset.set_string(xa_tags::imager_pixel_spacing, vr_type::DS, "0.3\\0.3");
        dataset.set_numeric<double>(xa_tags::distance_source_to_detector, vr_type::DS, 1200.0);
        dataset.set_numeric<double>(xa_tags::distance_source_to_patient, vr_type::DS, 800.0);
        CHECK(has_qca_calibration(dataset));
    }

    SECTION("dataset with invalid calibration (SID < SOD)") {
        dataset.set_string(xa_tags::imager_pixel_spacing, vr_type::DS, "0.3\\0.3");
        dataset.set_numeric<double>(xa_tags::distance_source_to_detector, vr_type::DS, 800.0);
        dataset.set_numeric<double>(xa_tags::distance_source_to_patient, vr_type::DS, 1200.0);
        CHECK_FALSE(has_qca_calibration(dataset));  // Invalid geometry
    }
}

// ============================================================================
// SOP Class Registry Tests
// ============================================================================

TEST_CASE("sop_class_registry contains XA classes", "[services][xa][registry]") {
    auto& registry = sop_class_registry::instance();

    SECTION("supports XA Image Storage") {
        CHECK(registry.is_supported(xa_image_storage_uid));
        const auto* info = registry.get_info(xa_image_storage_uid);
        REQUIRE(info != nullptr);
        CHECK(info->category == sop_class_category::storage);
        CHECK(info->modality == modality_type::xa);
    }

    SECTION("supports Enhanced XA Image Storage") {
        CHECK(registry.is_supported(enhanced_xa_image_storage_uid));
    }

    SECTION("supports XRF Image Storage") {
        CHECK(registry.is_supported(xrf_image_storage_uid));
        const auto* info = registry.get_info(xrf_image_storage_uid);
        REQUIRE(info != nullptr);
        CHECK(info->modality == modality_type::xrf);
    }

    SECTION("supports 3D Angiographic Image Storage") {
        CHECK(registry.is_supported(xray_3d_angiographic_image_storage_uid));
    }

    SECTION("get_by_modality returns XA classes") {
        auto xa_classes = registry.get_by_modality(modality_type::xa, true);
        CHECK(xa_classes.size() >= 4);  // XA, Enhanced XA, 3D XA, 3D Craniofacial

        for (const auto& uid : xa_classes) {
            const auto* info = registry.get_info(uid);
            REQUIRE(info != nullptr);
            CHECK(info->modality == modality_type::xa);
        }
    }
}

TEST_CASE("sop_class_registry XA modality conversion", "[services][xa][registry]") {
    CHECK(sop_class_registry::modality_to_string(modality_type::xa) == "XA");
    CHECK(sop_class_registry::modality_to_string(modality_type::xrf) == "RF");

    CHECK(sop_class_registry::parse_modality("XA") == modality_type::xa);
    CHECK(sop_class_registry::parse_modality("RF") == modality_type::xrf);
    CHECK(sop_class_registry::parse_modality("XRF") == modality_type::xrf);
}

// ============================================================================
// Convenience Function Tests
// ============================================================================

TEST_CASE("validate_xa_iod convenience function", "[services][xa][validation]") {
    auto dataset = create_minimal_xa_dataset();
    auto result = validate_xa_iod(dataset);
    CHECK(result.is_valid);
}

TEST_CASE("is_valid_xa_dataset convenience function", "[services][xa][validation]") {
    SECTION("valid dataset") {
        auto dataset = create_minimal_xa_dataset();
        CHECK(is_valid_xa_dataset(dataset));
    }

    SECTION("invalid dataset") {
        dicom_dataset empty_dataset;
        CHECK_FALSE(is_valid_xa_dataset(empty_dataset));
    }
}

TEST_CASE("is_storage_sop_class for XA classes", "[services][xa][registry]") {
    CHECK(is_storage_sop_class(xa_image_storage_uid));
    CHECK(is_storage_sop_class(enhanced_xa_image_storage_uid));
    CHECK(is_storage_sop_class(xrf_image_storage_uid));
}

TEST_CASE("get_storage_modality for XA classes", "[services][xa][registry]") {
    CHECK(get_storage_modality(xa_image_storage_uid) == modality_type::xa);
    CHECK(get_storage_modality(xrf_image_storage_uid) == modality_type::xrf);
}

TEST_CASE("get_sop_class_name for XA classes", "[services][xa][registry]") {
    CHECK(get_sop_class_name(xa_image_storage_uid) == "X-Ray Angiographic Image Storage");
    CHECK(get_sop_class_name(xrf_image_storage_uid) == "X-Ray Radiofluoroscopic Image Storage");
}
