/**
 * @file mg_iod_validator_test.cpp
 * @brief Unit tests for Digital Mammography IOD Validator
 */

#include <pacs/services/validation/mg_iod_validator.hpp>
#include <pacs/services/sop_classes/mg_storage.hpp>
#include <pacs/core/dicom_dataset.hpp>
#include <pacs/core/dicom_element.hpp>
#include <pacs/core/dicom_tag_constants.hpp>
#include <pacs/encoding/vr_type.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace pacs::services::validation;
using namespace pacs::services::sop_classes;
using namespace pacs::core;
using namespace pacs::encoding;

// ============================================================================
// Test Fixtures - Helper Functions
// ============================================================================

namespace {

// Mammography-specific DICOM tags
constexpr dicom_tag tag_laterality{0x0020, 0x0060};
constexpr dicom_tag tag_image_laterality{0x0020, 0x0062};
constexpr dicom_tag tag_view_position{0x0018, 0x5101};
constexpr dicom_tag tag_compression_force{0x0018, 0x11A2};
constexpr dicom_tag tag_body_part_thickness{0x0018, 0x11A0};
constexpr dicom_tag tag_breast_implant_present{0x0028, 0x1300};
constexpr dicom_tag tag_image_type{0x0008, 0x0008};
constexpr dicom_tag tag_presentation_intent_type{0x0008, 0x0068};
constexpr dicom_tag tag_detector_type{0x0018, 0x7004};
constexpr dicom_tag tag_imager_pixel_spacing{0x0018, 0x1164};
constexpr dicom_tag tag_body_part_examined{0x0018, 0x0015};
constexpr dicom_tag tag_pixel_intensity_relationship{0x0028, 0x1040};
constexpr dicom_tag tag_pixel_intensity_relationship_sign{0x0028, 0x1041};

// Helper to check if validation result has info-level findings
[[nodiscard]] bool has_info_findings(const validation_result& result) noexcept {
    return std::any_of(result.findings.begin(), result.findings.end(),
                       [](const validation_finding& f) {
                           return f.severity == validation_severity::info;
                       });
}

// Helper to create a minimal valid mammography dataset
dicom_dataset create_minimal_mg_dataset() {
    dicom_dataset ds;

    // Patient Module (Type 2)
    ds.set_string(tags::patient_name, vr_type::PN, "Test^Patient");
    ds.set_string(tags::patient_id, vr_type::LO, "12345");
    ds.set_string(tags::patient_birth_date, vr_type::DA, "19700101");
    ds.set_string(tags::patient_sex, vr_type::CS, "F");  // Typically female for mammography

    // General Study Module
    ds.set_string(tags::study_instance_uid, vr_type::UI, "1.2.3.4.5.6.7.8.9");
    ds.set_string(tags::study_date, vr_type::DA, "20240101");
    ds.set_string(tags::study_time, vr_type::TM, "120000");
    ds.set_string(tags::referring_physician_name, vr_type::PN, "Dr^Referring");
    ds.set_string(tags::study_id, vr_type::SH, "STUDY001");
    ds.set_string(tags::accession_number, vr_type::SH, "ACC001");

    // General Series Module
    ds.set_string(tags::modality, vr_type::CS, "MG");
    ds.set_string(tags::series_instance_uid, vr_type::UI, "1.2.3.4.5.6.7.8.9.1");
    ds.set_string(tags::series_number, vr_type::IS, "1");

    // Image Pixel Module
    ds.set_numeric<uint16_t>(tags::samples_per_pixel, vr_type::US, 1);
    ds.set_string(tags::photometric_interpretation, vr_type::CS, "MONOCHROME1");
    ds.set_numeric<uint16_t>(tags::rows, vr_type::US, 4096);     // High resolution
    ds.set_numeric<uint16_t>(tags::columns, vr_type::US, 3328);  // Typical mammography size
    ds.set_numeric<uint16_t>(tags::bits_allocated, vr_type::US, 16);
    ds.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, 14);  // High dynamic range
    ds.set_numeric<uint16_t>(tags::high_bit, vr_type::US, 13);
    ds.set_numeric<uint16_t>(tags::pixel_representation, vr_type::US, 0);

    // Pixel Data (minimal placeholder)
    std::vector<uint8_t> pixel_data(100, 0);
    ds.insert(dicom_element(tags::pixel_data, vr_type::OW, pixel_data));

    // Mammography Image Module
    ds.set_string(tag_image_type, vr_type::CS, "ORIGINAL\\PRIMARY");
    ds.set_string(tag_pixel_intensity_relationship, vr_type::CS, "LIN");
    ds.set_numeric<int16_t>(tag_pixel_intensity_relationship_sign, vr_type::SS, 1);

    // Mammography-specific: Laterality
    ds.set_string(tag_laterality, vr_type::CS, "L");  // Left breast

    // Mammography-specific: View Position
    ds.set_string(tag_view_position, vr_type::CS, "CC");  // Craniocaudal view

    // DX Anatomy Imaged Module
    ds.set_string(tag_body_part_examined, vr_type::CS, "BREAST");

    // DX Detector Module
    ds.set_string(tag_detector_type, vr_type::CS, "DIRECT");  // a-Se detector
    ds.set_string(tag_imager_pixel_spacing, vr_type::DS, "0.07\\0.07");  // Fine spacing for MG

    // X-Ray Acquisition Dose Module
    ds.set_numeric<double>(tag_compression_force, vr_type::DS, 120.0);  // Newtons
    ds.set_numeric<double>(tag_body_part_thickness, vr_type::DS, 55.0);  // mm

    // SOP Common Module
    ds.set_string(tags::sop_class_uid, vr_type::UI,
                  std::string(mg_image_storage_for_presentation_uid));
    ds.set_string(tags::sop_instance_uid, vr_type::UI, "1.2.3.4.5.6.7.8.9.2");

    return ds;
}

}  // namespace

// ============================================================================
// MG IOD Validator Basic Tests
// ============================================================================

TEST_CASE("mg_iod_validator validates minimal valid dataset", "[services][mg][validator]") {
    mg_iod_validator validator;
    auto dataset = create_minimal_mg_dataset();

    auto result = validator.validate(dataset);

    CHECK(result.is_valid);
    CHECK_FALSE(result.has_errors());
}

TEST_CASE("mg_iod_validator detects missing Type 1 attributes", "[services][mg][validator]") {
    mg_iod_validator validator;
    auto dataset = create_minimal_mg_dataset();

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

    SECTION("missing SeriesInstanceUID") {
        dataset.remove(tags::series_instance_uid);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }

    SECTION("missing SOPClassUID") {
        dataset.remove(tags::sop_class_uid);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }

    SECTION("missing Rows") {
        dataset.remove(tags::rows);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }
}

TEST_CASE("mg_iod_validator checks modality value", "[services][mg][validator]") {
    mg_iod_validator validator;
    auto dataset = create_minimal_mg_dataset();

    SECTION("wrong modality - DX") {
        dataset.set_string(tags::modality, vr_type::CS, "DX");
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);

        // Should have an error about modality
        bool found_modality_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "MG-ERR-002") {
                found_modality_error = true;
                break;
            }
        }
        CHECK(found_modality_error);
    }

    SECTION("wrong modality - CT") {
        dataset.set_string(tags::modality, vr_type::CS, "CT");
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }
}

// ============================================================================
// Laterality Validation Tests
// ============================================================================

TEST_CASE("mg_iod_validator validates laterality", "[services][mg][validator][laterality]") {
    mg_iod_validator validator;

    SECTION("valid laterality values") {
        auto dataset = create_minimal_mg_dataset();

        dataset.set_string(tag_laterality, vr_type::CS, "L");
        auto result = validator.validate_laterality(dataset);
        CHECK(result.is_valid);

        dataset.set_string(tag_laterality, vr_type::CS, "R");
        result = validator.validate_laterality(dataset);
        CHECK(result.is_valid);

        dataset.set_string(tag_laterality, vr_type::CS, "B");
        result = validator.validate_laterality(dataset);
        CHECK(result.is_valid);
    }

    SECTION("invalid laterality value") {
        auto dataset = create_minimal_mg_dataset();
        dataset.set_string(tag_laterality, vr_type::CS, "X");

        auto result = validator.validate_laterality(dataset);
        CHECK_FALSE(result.is_valid);

        bool found_laterality_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "MG-ERR-004") {
                found_laterality_error = true;
                break;
            }
        }
        CHECK(found_laterality_error);
    }

    SECTION("missing laterality") {
        auto dataset = create_minimal_mg_dataset();
        dataset.remove(tag_laterality);
        dataset.remove(tag_image_laterality);

        auto result = validator.validate_laterality(dataset);
        CHECK_FALSE(result.is_valid);

        bool found_laterality_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "MG-ERR-003") {
                found_laterality_error = true;
                break;
            }
        }
        CHECK(found_laterality_error);
    }

    SECTION("image laterality used instead of series laterality") {
        auto dataset = create_minimal_mg_dataset();
        dataset.remove(tag_laterality);
        dataset.set_string(tag_image_laterality, vr_type::CS, "R");

        auto result = validator.validate_laterality(dataset);
        CHECK(result.is_valid);
    }

    SECTION("laterality mismatch warning") {
        auto dataset = create_minimal_mg_dataset();
        dataset.set_string(tag_laterality, vr_type::CS, "L");
        dataset.set_string(tag_image_laterality, vr_type::CS, "R");

        auto result = validator.validate_laterality(dataset);
        // Mismatch is a warning, not an error
        CHECK(result.is_valid);
        CHECK(result.has_warnings());

        bool found_mismatch_warning = false;
        for (const auto& f : result.findings) {
            if (f.code == "MG-WARN-001") {
                found_mismatch_warning = true;
                break;
            }
        }
        CHECK(found_mismatch_warning);
    }
}

// ============================================================================
// View Position Validation Tests
// ============================================================================

TEST_CASE("mg_iod_validator validates view position", "[services][mg][validator][view]") {
    mg_iod_validator validator;

    SECTION("valid standard views") {
        auto dataset = create_minimal_mg_dataset();

        for (const auto& view : {"CC", "MLO", "ML", "LM"}) {
            dataset.set_string(tag_view_position, vr_type::CS, view);
            auto result = validator.validate_view_position(dataset);
            CHECK(result.is_valid);
        }
    }

    SECTION("valid extended views") {
        auto dataset = create_minimal_mg_dataset();

        for (const auto& view : {"XCCL", "XCCM", "FB", "SPOT", "MAG", "ID"}) {
            dataset.set_string(tag_view_position, vr_type::CS, view);
            auto result = validator.validate_view_position(dataset);
            CHECK(result.is_valid);
        }
    }

    SECTION("missing view position") {
        auto dataset = create_minimal_mg_dataset();
        dataset.remove(tag_view_position);

        auto result = validator.validate_view_position(dataset);
        // Missing view position is a warning for mammography
        CHECK(result.is_valid);
        CHECK(result.has_warnings());
    }

    SECTION("empty view position") {
        auto dataset = create_minimal_mg_dataset();
        dataset.set_string(tag_view_position, vr_type::CS, "");

        auto result = validator.validate_view_position(dataset);
        CHECK(result.has_warnings());
    }

    SECTION("unrecognized view position") {
        auto dataset = create_minimal_mg_dataset();
        dataset.set_string(tag_view_position, vr_type::CS, "INVALID");

        auto result = validator.validate_view_position(dataset);
        CHECK(result.has_warnings());

        bool found_view_warning = false;
        for (const auto& f : result.findings) {
            if (f.code == "MG-WARN-010") {
                found_view_warning = true;
                break;
            }
        }
        CHECK(found_view_warning);
    }
}

// ============================================================================
// Compression Force Validation Tests
// ============================================================================

TEST_CASE("mg_iod_validator validates compression force", "[services][mg][validator][compression]") {
    mg_iod_validator validator;

    SECTION("typical compression force") {
        auto dataset = create_minimal_mg_dataset();
        dataset.set_numeric<double>(tag_compression_force, vr_type::DS, 120.0);

        auto result = validator.validate_compression_force(dataset);
        CHECK(result.is_valid);
    }

    SECTION("force at typical range boundaries") {
        auto dataset = create_minimal_mg_dataset();

        // At lower typical boundary
        dataset.set_numeric<double>(tag_compression_force, vr_type::DS, 50.0);
        auto result = validator.validate_compression_force(dataset);
        CHECK(result.is_valid);

        // At upper typical boundary
        dataset.set_numeric<double>(tag_compression_force, vr_type::DS, 200.0);
        result = validator.validate_compression_force(dataset);
        CHECK(result.is_valid);
    }

    SECTION("force outside typical but within valid range") {
        auto dataset = create_minimal_mg_dataset();

        // Below typical (info warning)
        dataset.set_numeric<double>(tag_compression_force, vr_type::DS, 30.0);
        auto result = validator.validate_compression_force(dataset);
        CHECK(result.is_valid);  // Still valid, just informational
        CHECK(has_info_findings(result));

        // Above typical (info warning)
        dataset.set_numeric<double>(tag_compression_force, vr_type::DS, 250.0);
        result = validator.validate_compression_force(dataset);
        CHECK(result.is_valid);
        CHECK(has_info_findings(result));
    }

    SECTION("force outside valid range") {
        auto dataset = create_minimal_mg_dataset();

        // Too low
        dataset.set_numeric<double>(tag_compression_force, vr_type::DS, 10.0);
        auto result = validator.validate_compression_force(dataset);
        CHECK(result.has_warnings());

        // Too high
        dataset.set_numeric<double>(tag_compression_force, vr_type::DS, 350.0);
        result = validator.validate_compression_force(dataset);
        CHECK(result.has_warnings());
    }

    SECTION("missing compression force") {
        auto dataset = create_minimal_mg_dataset();
        dataset.remove(tag_compression_force);

        auto result = validator.validate_compression_force(dataset);
        // Missing compression force is informational
        CHECK(result.is_valid);
        CHECK(has_info_findings(result));
    }
}

// ============================================================================
// Photometric Interpretation Tests
// ============================================================================

TEST_CASE("mg_iod_validator checks photometric interpretation", "[services][mg][validator]") {
    mg_iod_validator validator;
    auto dataset = create_minimal_mg_dataset();

    SECTION("MONOCHROME1 is valid") {
        dataset.set_string(tags::photometric_interpretation, vr_type::CS, "MONOCHROME1");
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
    }

    SECTION("MONOCHROME2 is valid") {
        dataset.set_string(tags::photometric_interpretation, vr_type::CS, "MONOCHROME2");
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
    }

    SECTION("RGB is invalid for mammography") {
        dataset.set_string(tags::photometric_interpretation, vr_type::CS, "RGB");
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);

        bool found_photometric_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "MG-ERR-008") {
                found_photometric_error = true;
                break;
            }
        }
        CHECK(found_photometric_error);
    }
}

// ============================================================================
// SOP Class UID Tests
// ============================================================================

TEST_CASE("mg_iod_validator checks SOP Class UID", "[services][mg][validator]") {
    mg_iod_validator validator;
    auto dataset = create_minimal_mg_dataset();

    SECTION("valid mammography SOP Classes") {
        dataset.set_string(tags::sop_class_uid, vr_type::UI,
                          std::string(mg_image_storage_for_presentation_uid));
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);

        dataset.set_string(tags::sop_class_uid, vr_type::UI,
                          std::string(mg_image_storage_for_processing_uid));
        result = validator.validate(dataset);
        CHECK(result.is_valid);
    }

    SECTION("non-MG SOP Class") {
        // DX SOP Class
        dataset.set_string(tags::sop_class_uid, vr_type::UI, "1.2.840.10008.5.1.4.1.1.1.1");
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);

        // CT SOP Class
        dataset.set_string(tags::sop_class_uid, vr_type::UI, "1.2.840.10008.5.1.4.1.1.2");
        result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }
}

// ============================================================================
// Pixel Data Consistency Tests
// ============================================================================

TEST_CASE("mg_iod_validator checks pixel data consistency", "[services][mg][validator]") {
    mg_iod_validator validator;
    auto dataset = create_minimal_mg_dataset();

    SECTION("BitsStored exceeds BitsAllocated") {
        dataset.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, 20);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }

    SECTION("wrong HighBit") {
        dataset.set_numeric<uint16_t>(tags::high_bit, vr_type::US, 15);  // Should be 13
        auto result = validator.validate(dataset);
        CHECK(result.has_warnings());
    }

    SECTION("non-grayscale SamplesPerPixel") {
        dataset.set_numeric<uint16_t>(tags::samples_per_pixel, vr_type::US, 3);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }

    SECTION("typical mammography bit depth is 12-16") {
        dataset.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, 8);
        auto result = validator.validate(dataset);
        // Low bit depth is informational only
        CHECK(has_info_findings(result));
    }
}

// ============================================================================
// Body Part Validation Tests
// ============================================================================

TEST_CASE("mg_iod_validator validates body part", "[services][mg][validator]") {
    mg_iod_validator validator;
    auto dataset = create_minimal_mg_dataset();

    SECTION("BREAST is correct body part") {
        dataset.set_string(tag_body_part_examined, vr_type::CS, "BREAST");
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
    }

    SECTION("non-BREAST body part generates warning") {
        dataset.set_string(tag_body_part_examined, vr_type::CS, "CHEST");
        auto result = validator.validate(dataset);
        CHECK(result.has_warnings());

        bool found_body_part_warning = false;
        for (const auto& f : result.findings) {
            if (f.code == "MG-WARN-003") {
                found_body_part_warning = true;
                break;
            }
        }
        CHECK(found_body_part_warning);
    }
}

// ============================================================================
// Breast Implant Tests
// ============================================================================

TEST_CASE("mg_iod_validator validates breast implant attributes", "[services][mg][validator]") {
    mg_iod_validator validator;
    auto dataset = create_minimal_mg_dataset();

    SECTION("breast implant YES") {
        dataset.set_string(tag_breast_implant_present, vr_type::CS, "YES");
        auto result = validator.validate(dataset);
        // Implant present with non-ID view generates info
        CHECK(has_info_findings(result));
    }

    SECTION("breast implant NO") {
        dataset.set_string(tag_breast_implant_present, vr_type::CS, "NO");
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
    }

    SECTION("invalid breast implant value") {
        dataset.set_string(tag_breast_implant_present, vr_type::CS, "MAYBE");
        auto result = validator.validate(dataset);
        CHECK(result.has_warnings());
    }

    SECTION("breast implant with ID view") {
        dataset.set_string(tag_breast_implant_present, vr_type::CS, "YES");
        dataset.set_string(tag_view_position, vr_type::CS, "ID");
        auto result = validator.validate(dataset);
        // Should not have the info about missing ID view
        bool found_implant_info = false;
        for (const auto& f : result.findings) {
            if (f.code == "MG-INFO-007") {
                found_implant_info = true;
                break;
            }
        }
        CHECK_FALSE(found_implant_info);
    }
}

// ============================================================================
// Quick Check Tests
// ============================================================================

TEST_CASE("mg_iod_validator quick_check works correctly", "[services][mg][validator]") {
    mg_iod_validator validator;

    SECTION("valid dataset passes quick check") {
        auto dataset = create_minimal_mg_dataset();
        CHECK(validator.quick_check(dataset));
    }

    SECTION("invalid modality fails quick check") {
        auto dataset = create_minimal_mg_dataset();
        dataset.set_string(tags::modality, vr_type::CS, "DX");
        CHECK_FALSE(validator.quick_check(dataset));
    }

    SECTION("missing required attribute fails quick check") {
        auto dataset = create_minimal_mg_dataset();
        dataset.remove(tags::rows);
        CHECK_FALSE(validator.quick_check(dataset));
    }

    SECTION("missing laterality fails quick check") {
        auto dataset = create_minimal_mg_dataset();
        dataset.remove(tag_laterality);
        dataset.remove(tag_image_laterality);
        CHECK_FALSE(validator.quick_check(dataset));
    }
}

// ============================================================================
// For Presentation / For Processing Tests
// ============================================================================

TEST_CASE("mg_iod_validator validates For Presentation images", "[services][mg][validator]") {
    mg_iod_validator validator;
    auto dataset = create_minimal_mg_dataset();

    // Add Window Center/Width for presentation
    dataset.set_string(tags::window_center, vr_type::DS, "8192");
    dataset.set_string(tags::window_width, vr_type::DS, "16384");

    auto result = validator.validate_for_presentation(dataset);
    CHECK(result.is_valid);
}

TEST_CASE("mg_iod_validator validates For Processing images", "[services][mg][validator]") {
    mg_iod_validator validator;
    auto dataset = create_minimal_mg_dataset();

    // Change to For Processing SOP Class
    dataset.set_string(tags::sop_class_uid, vr_type::UI,
                      std::string(mg_image_storage_for_processing_uid));

    auto result = validator.validate_for_processing(dataset);
    CHECK(result.is_valid);
}

// ============================================================================
// Custom Options Tests
// ============================================================================

TEST_CASE("mg_iod_validator with custom options", "[services][mg][validator]") {
    SECTION("strict mode treats warnings as errors") {
        mg_validation_options options;
        options.strict_mode = true;

        mg_iod_validator validator(options);
        auto dataset = create_minimal_mg_dataset();

        // Remove a Type 2 attribute to get a warning
        dataset.remove(tags::patient_name);

        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);  // Strict mode makes warnings into errors
    }

    SECTION("can disable pixel data validation") {
        mg_validation_options options;
        options.validate_pixel_data = false;

        mg_iod_validator validator(options);
        auto dataset = create_minimal_mg_dataset();
        dataset.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, 20);  // Invalid normally

        auto result = validator.validate(dataset);
        // Should not have pixel data errors when validation is disabled
        bool found_pixel_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "MG-ERR-006") {
                found_pixel_error = true;
                break;
            }
        }
        CHECK_FALSE(found_pixel_error);
    }

    SECTION("can disable laterality validation") {
        mg_validation_options options;
        options.validate_laterality = false;

        mg_iod_validator validator(options);
        auto dataset = create_minimal_mg_dataset();
        dataset.set_string(tag_laterality, vr_type::CS, "X");  // Invalid normally

        auto result = validator.validate(dataset);
        // Should not have laterality errors when validation is disabled
        bool found_laterality_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "MG-ERR-004") {
                found_laterality_error = true;
                break;
            }
        }
        CHECK_FALSE(found_laterality_error);
    }

    SECTION("can disable compression validation") {
        mg_validation_options options;
        options.validate_compression = false;

        mg_iod_validator validator(options);
        auto dataset = create_minimal_mg_dataset();
        dataset.remove(tag_compression_force);

        auto result = validator.validate(dataset);
        // Should not have compression info when validation is disabled
        bool found_compression_info = false;
        for (const auto& f : result.findings) {
            if (f.code == "MG-INFO-008") {
                found_compression_info = true;
                break;
            }
        }
        CHECK_FALSE(found_compression_info);
    }
}

// ============================================================================
// Convenience Function Tests
// ============================================================================

TEST_CASE("validate_mg_iod convenience function", "[services][mg][validator]") {
    auto dataset = create_minimal_mg_dataset();
    auto result = validate_mg_iod(dataset);
    CHECK(result.is_valid);
}

TEST_CASE("is_valid_mg_dataset convenience function", "[services][mg][validator]") {
    SECTION("valid dataset") {
        auto dataset = create_minimal_mg_dataset();
        CHECK(is_valid_mg_dataset(dataset));
    }

    SECTION("invalid dataset") {
        auto dataset = create_minimal_mg_dataset();
        dataset.set_string(tags::modality, vr_type::CS, "DX");
        CHECK_FALSE(is_valid_mg_dataset(dataset));
    }
}

TEST_CASE("is_for_presentation_mg detects presentation images", "[services][mg][validator]") {
    auto dataset = create_minimal_mg_dataset();
    CHECK(is_for_presentation_mg(dataset));

    dataset.set_string(tags::sop_class_uid, vr_type::UI,
                      std::string(mg_image_storage_for_processing_uid));
    CHECK_FALSE(is_for_presentation_mg(dataset));
}

TEST_CASE("is_for_processing_mg detects processing images", "[services][mg][validator]") {
    auto dataset = create_minimal_mg_dataset();
    CHECK_FALSE(is_for_processing_mg(dataset));

    dataset.set_string(tags::sop_class_uid, vr_type::UI,
                      std::string(mg_image_storage_for_processing_uid));
    CHECK(is_for_processing_mg(dataset));
}

TEST_CASE("has_breast_implant detects implant presence", "[services][mg][validator]") {
    auto dataset = create_minimal_mg_dataset();

    SECTION("no implant attribute") {
        CHECK_FALSE(has_breast_implant(dataset));
    }

    SECTION("implant YES") {
        dataset.set_string(tag_breast_implant_present, vr_type::CS, "YES");
        CHECK(has_breast_implant(dataset));
    }

    SECTION("implant NO") {
        dataset.set_string(tag_breast_implant_present, vr_type::CS, "NO");
        CHECK_FALSE(has_breast_implant(dataset));
    }
}

TEST_CASE("is_screening_mammogram detects screening views", "[services][mg][validator]") {
    auto dataset = create_minimal_mg_dataset();

    SECTION("CC is screening") {
        dataset.set_string(tag_view_position, vr_type::CS, "CC");
        CHECK(is_screening_mammogram(dataset));
    }

    SECTION("MLO is screening") {
        dataset.set_string(tag_view_position, vr_type::CS, "MLO");
        CHECK(is_screening_mammogram(dataset));
    }

    SECTION("ML is not screening") {
        dataset.set_string(tag_view_position, vr_type::CS, "ML");
        CHECK_FALSE(is_screening_mammogram(dataset));
    }

    SECTION("SPOT is not screening") {
        dataset.set_string(tag_view_position, vr_type::CS, "SPOT");
        CHECK_FALSE(is_screening_mammogram(dataset));
    }

    SECTION("no view position") {
        dataset.remove(tag_view_position);
        CHECK_FALSE(is_screening_mammogram(dataset));
    }
}
