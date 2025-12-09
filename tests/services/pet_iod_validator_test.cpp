/**
 * @file pet_iod_validator_test.cpp
 * @brief Unit tests for PET (Positron Emission Tomography) IOD Validator
 */

#include <pacs/services/validation/pet_iod_validator.hpp>
#include <pacs/services/sop_classes/pet_storage.hpp>
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

// PET-specific DICOM tags
constexpr dicom_tag tag_image_type{0x0008, 0x0008};
constexpr dicom_tag tag_frame_of_reference_uid{0x0020, 0x0052};
constexpr dicom_tag tag_position_reference_indicator{0x0020, 0x1040};
constexpr dicom_tag tag_slice_thickness{0x0018, 0x0050};
constexpr dicom_tag tag_image_position_patient{0x0020, 0x0032};
constexpr dicom_tag tag_image_orientation_patient{0x0020, 0x0037};
constexpr dicom_tag tag_pixel_spacing{0x0028, 0x0030};

// PET Series Module tags
constexpr dicom_tag tag_series_type{0x0054, 0x1000};
constexpr dicom_tag tag_units{0x0054, 0x1001};
constexpr dicom_tag tag_counts_source{0x0054, 0x1002};
constexpr dicom_tag tag_series_date{0x0008, 0x0021};
constexpr dicom_tag tag_series_time{0x0008, 0x0031};

// PET Image Module tags
constexpr dicom_tag tag_image_index{0x0054, 0x1330};
constexpr dicom_tag tag_frame_reference_time{0x0054, 0x1300};  // Required Type 1
constexpr dicom_tag tag_acquisition_date{0x0008, 0x0022};
constexpr dicom_tag tag_acquisition_time{0x0008, 0x0032};
constexpr dicom_tag tag_actual_frame_duration{0x0018, 0x1242};
constexpr dicom_tag tag_decay_correction{0x0054, 0x1102};
constexpr dicom_tag tag_reconstruction_diameter{0x0018, 0x1100};
constexpr dicom_tag tag_rescale_intercept{0x0028, 0x1052};
constexpr dicom_tag tag_rescale_slope{0x0028, 0x1053};
constexpr dicom_tag tag_rescale_type{0x0028, 0x1054};

// Radiopharmaceutical tags
constexpr dicom_tag tag_radiopharmaceutical_info_seq{0x0054, 0x0016};
constexpr dicom_tag tag_radionuclide_code_seq{0x0054, 0x0300};
constexpr dicom_tag tag_radiopharmaceutical_start_time{0x0018, 0x1072};
constexpr dicom_tag tag_radionuclide_total_dose{0x0018, 0x1074};
constexpr dicom_tag tag_radionuclide_half_life{0x0018, 0x1075};

// SUV-related tags
constexpr dicom_tag tag_patient_weight{0x0010, 0x1030};
constexpr dicom_tag tag_patient_size{0x0010, 0x1020};

// Attenuation/Scatter correction tags
constexpr dicom_tag tag_attenuation_correction_method{0x0054, 0x1101};
constexpr dicom_tag tag_scatter_correction_method{0x0054, 0x1105};
constexpr dicom_tag tag_reconstruction_method{0x0054, 0x1103};
constexpr dicom_tag tag_convolution_kernel{0x0018, 0x1210};

// Helper to check if validation result has info-level findings
[[nodiscard]] bool has_info_findings(const validation_result& result) noexcept {
    return std::any_of(result.findings.begin(), result.findings.end(),
                       [](const validation_finding& f) {
                           return f.severity == validation_severity::info;
                       });
}

// Helper to create a minimal valid PET dataset
dicom_dataset create_minimal_pet_dataset() {
    dicom_dataset ds;

    // Patient Module (Type 2)
    ds.set_string(tags::patient_name, vr_type::PN, "Test^Patient");
    ds.set_string(tags::patient_id, vr_type::LO, "12345");
    ds.set_string(tags::patient_birth_date, vr_type::DA, "19600101");
    ds.set_string(tags::patient_sex, vr_type::CS, "M");

    // Patient weight for SUV calculation
    ds.set_numeric<double>(tag_patient_weight, vr_type::DS, 70.0);  // kg
    ds.set_numeric<double>(tag_patient_size, vr_type::DS, 1.75);    // meters

    // General Study Module
    ds.set_string(tags::study_instance_uid, vr_type::UI, "1.2.3.4.5.6.7.8.9");
    ds.set_string(tags::study_date, vr_type::DA, "20240101");
    ds.set_string(tags::study_time, vr_type::TM, "080000");
    ds.set_string(tags::referring_physician_name, vr_type::PN, "Dr^Referring");
    ds.set_string(tags::study_id, vr_type::SH, "STUDY001");
    ds.set_string(tags::accession_number, vr_type::SH, "ACC001");

    // General Series Module
    ds.set_string(tags::modality, vr_type::CS, "PT");  // PT for PET
    ds.set_string(tags::series_instance_uid, vr_type::UI, "1.2.3.4.5.6.7.8.9.1");
    ds.set_string(tags::series_number, vr_type::IS, "1");
    ds.set_string(tag_series_date, vr_type::DA, "20240101");
    ds.set_string(tag_series_time, vr_type::TM, "083000");

    // PET Series Module
    ds.set_string(tag_series_type, vr_type::CS, "WHOLE BODY\\IMAGE");
    ds.set_string(tag_units, vr_type::CS, "BQML");  // Bq/ml
    ds.set_string(tag_counts_source, vr_type::CS, "EMISSION");

    // Frame of Reference Module
    ds.set_string(tag_frame_of_reference_uid, vr_type::UI, "1.2.3.4.5.6.7.8.9.2");
    ds.set_string(tag_position_reference_indicator, vr_type::LO, "");

    // Image Plane Module
    ds.set_numeric<double>(tag_slice_thickness, vr_type::DS, 4.0);  // mm
    ds.set_string(tag_image_position_patient, vr_type::DS, "0\\0\\0");
    ds.set_string(tag_image_orientation_patient, vr_type::DS, "1\\0\\0\\0\\1\\0");
    ds.set_string(tag_pixel_spacing, vr_type::DS, "4.0\\4.0");  // Typical PET resolution

    // Image Pixel Module
    ds.set_numeric<uint16_t>(tags::samples_per_pixel, vr_type::US, 1);
    ds.set_string(tags::photometric_interpretation, vr_type::CS, "MONOCHROME2");
    ds.set_numeric<uint16_t>(tags::rows, vr_type::US, 128);      // Typical PET matrix
    ds.set_numeric<uint16_t>(tags::columns, vr_type::US, 128);
    ds.set_numeric<uint16_t>(tags::bits_allocated, vr_type::US, 16);
    ds.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, 16);
    ds.set_numeric<uint16_t>(tags::high_bit, vr_type::US, 15);
    ds.set_numeric<uint16_t>(tags::pixel_representation, vr_type::US, 0);

    // Pixel Data (minimal placeholder)
    std::vector<uint8_t> pixel_data(100, 0);
    ds.insert(dicom_element(tags::pixel_data, vr_type::OW, pixel_data));

    // PET Image Module
    ds.set_string(tag_image_type, vr_type::CS, "ORIGINAL\\PRIMARY");
    ds.set_numeric<uint32_t>(tag_image_index, vr_type::UL, 1);
    ds.set_numeric<double>(tag_frame_reference_time, vr_type::DS, 0.0);  // Required Type 1
    ds.set_string(tag_acquisition_date, vr_type::DA, "20240101");
    ds.set_string(tag_acquisition_time, vr_type::TM, "083000");
    ds.set_numeric<uint32_t>(tag_actual_frame_duration, vr_type::IS, 180000);  // 3 min in ms
    ds.set_string(tag_decay_correction, vr_type::CS, "START");
    ds.set_numeric<double>(tag_reconstruction_diameter, vr_type::DS, 700.0);  // mm

    // Rescale parameters
    ds.set_numeric<double>(tag_rescale_intercept, vr_type::DS, 0.0);
    ds.set_numeric<double>(tag_rescale_slope, vr_type::DS, 1.0);
    ds.set_string(tag_rescale_type, vr_type::LO, "BQML");

    // Attenuation and correction
    ds.set_string(tag_attenuation_correction_method, vr_type::LO, "CT");
    ds.set_string(tag_scatter_correction_method, vr_type::LO, "MODEL");
    ds.set_string(tag_reconstruction_method, vr_type::LO, "OSEM3D");
    ds.set_string(tag_convolution_kernel, vr_type::SH, "GAUSSIAN");

    // SOP Common Module
    ds.set_string(tags::sop_class_uid, vr_type::UI, std::string(pet_image_storage_uid));
    ds.set_string(tags::sop_instance_uid, vr_type::UI, "1.2.3.4.5.6.7.8.9.3");

    return ds;
}

}  // namespace

// ============================================================================
// PET IOD Validator Basic Tests
// ============================================================================

TEST_CASE("pet_iod_validator validates minimal valid dataset", "[services][pet][validator]") {
    pet_iod_validator validator;
    auto dataset = create_minimal_pet_dataset();

    auto result = validator.validate(dataset);

    CHECK(result.is_valid);
    CHECK_FALSE(result.has_errors());
}

TEST_CASE("pet_iod_validator detects missing Type 1 attributes", "[services][pet][validator]") {
    pet_iod_validator validator;
    auto dataset = create_minimal_pet_dataset();

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

    SECTION("missing FrameOfReferenceUID") {
        dataset.remove(tag_frame_of_reference_uid);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }
}

TEST_CASE("pet_iod_validator checks modality value", "[services][pet][validator]") {
    pet_iod_validator validator;
    auto dataset = create_minimal_pet_dataset();

    SECTION("correct modality - PT") {
        dataset.set_string(tags::modality, vr_type::CS, "PT");
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
    }

    SECTION("wrong modality - NM") {
        dataset.set_string(tags::modality, vr_type::CS, "NM");
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);

        bool found_modality_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "PT-ERR-003") {  // Modality error code
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
// PET Series Module Tests
// ============================================================================

TEST_CASE("pet_iod_validator validates PET Series Module", "[services][pet][validator][series]") {
    pet_iod_validator validator;
    auto dataset = create_minimal_pet_dataset();

    SECTION("valid series type values") {
        for (const auto& type : {"WHOLE BODY\\IMAGE", "STATIC\\IMAGE", "DYNAMIC\\IMAGE"}) {
            dataset.set_string(tag_series_type, vr_type::CS, type);
            auto result = validator.validate(dataset);
            CHECK(result.is_valid);
        }
    }

    SECTION("valid units values") {
        for (const auto& unit : {"BQML", "CNTS", "GML"}) {
            dataset.set_string(tag_units, vr_type::CS, unit);
            auto result = validator.validate(dataset);
            CHECK(result.is_valid);
        }
    }

    SECTION("missing Series Type") {
        dataset.remove(tag_series_type);
        auto result = validator.validate(dataset);
        // Series type is Type 1 in PET Series Module
        CHECK(result.has_warnings());
    }

    SECTION("missing Units") {
        dataset.remove(tag_units);
        auto result = validator.validate(dataset);
        CHECK(result.has_warnings());
    }
}

// ============================================================================
// PET Image Module Tests
// ============================================================================

TEST_CASE("pet_iod_validator validates PET Image Module", "[services][pet][validator][image]") {
    pet_iod_validator validator;
    auto dataset = create_minimal_pet_dataset();

    SECTION("valid decay correction values") {
        for (const auto& dc : {"START", "ADMIN", "NONE"}) {
            dataset.set_string(tag_decay_correction, vr_type::CS, dc);
            auto result = validator.validate(dataset);
            CHECK(result.is_valid);
        }
    }

    SECTION("invalid decay correction value") {
        dataset.set_string(tag_decay_correction, vr_type::CS, "INVALID");
        auto result = validator.validate(dataset);
        CHECK(result.has_warnings());
    }

    SECTION("missing Rescale parameters triggers warning") {
        dataset.remove(tag_rescale_slope);
        auto result = validator.validate(dataset);
        CHECK(has_info_findings(result));
    }
}

// ============================================================================
// Photometric Interpretation Tests
// ============================================================================

TEST_CASE("pet_iod_validator checks photometric interpretation", "[services][pet][validator]") {
    pet_iod_validator validator;
    auto dataset = create_minimal_pet_dataset();

    SECTION("MONOCHROME2 is valid") {
        dataset.set_string(tags::photometric_interpretation, vr_type::CS, "MONOCHROME2");
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
    }

    SECTION("MONOCHROME1 generates warning for PET") {
        dataset.set_string(tags::photometric_interpretation, vr_type::CS, "MONOCHROME1");
        auto result = validator.validate(dataset);
        // PET typically uses MONOCHROME2
        CHECK(has_info_findings(result));
    }

    SECTION("RGB generates warning for PET") {
        dataset.set_string(tags::photometric_interpretation, vr_type::CS, "RGB");
        auto result = validator.validate(dataset);
        // RGB is unusual for PET but generates warning, not error
        CHECK(result.has_warnings());
    }
}

// ============================================================================
// SOP Class UID Tests
// ============================================================================

TEST_CASE("pet_iod_validator checks SOP Class UID", "[services][pet][validator]") {
    pet_iod_validator validator;
    auto dataset = create_minimal_pet_dataset();

    SECTION("valid PET SOP Classes") {
        dataset.set_string(tags::sop_class_uid, vr_type::UI,
                          std::string(pet_image_storage_uid));
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);

        dataset.set_string(tags::sop_class_uid, vr_type::UI,
                          std::string(enhanced_pet_image_storage_uid));
        result = validator.validate(dataset);
        CHECK(result.is_valid);
    }

    SECTION("non-PET SOP Class") {
        // CT SOP Class
        dataset.set_string(tags::sop_class_uid, vr_type::UI, "1.2.840.10008.5.1.4.1.1.2");
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);

        // NM SOP Class
        dataset.set_string(tags::sop_class_uid, vr_type::UI, "1.2.840.10008.5.1.4.1.1.20");
        result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }
}

// ============================================================================
// Pixel Data Consistency Tests
// ============================================================================

TEST_CASE("pet_iod_validator checks pixel data consistency", "[services][pet][validator]") {
    pet_iod_validator validator;
    auto dataset = create_minimal_pet_dataset();

    SECTION("BitsStored exceeds BitsAllocated") {
        dataset.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, 20);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }

    SECTION("wrong HighBit") {
        dataset.set_numeric<uint16_t>(tags::high_bit, vr_type::US, 14);  // Should be 15
        auto result = validator.validate(dataset);
        CHECK(result.has_warnings());
    }

    SECTION("non-grayscale SamplesPerPixel generates warning") {
        dataset.set_numeric<uint16_t>(tags::samples_per_pixel, vr_type::US, 3);
        auto result = validator.validate(dataset);
        // Non-grayscale is unusual for PET but generates warning, not error
        CHECK(result.has_warnings());
    }

    SECTION("typical PET matrix size") {
        dataset.set_numeric<uint16_t>(tags::rows, vr_type::US, 128);
        dataset.set_numeric<uint16_t>(tags::columns, vr_type::US, 128);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
    }
}

// ============================================================================
// SUV Parameters Tests
// ============================================================================

TEST_CASE("pet_iod_validator validates SUV parameters", "[services][pet][validator][suv]") {
    pet_iod_validator validator;
    auto dataset = create_minimal_pet_dataset();

    SECTION("valid patient weight for SUV") {
        dataset.set_numeric<double>(tag_patient_weight, vr_type::DS, 70.0);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
    }

    SECTION("missing patient weight generates warning") {
        dataset.remove(tag_patient_weight);
        auto result = validator.validate(dataset);
        // SUV calculation requires patient weight
        CHECK(has_info_findings(result));
    }

    SECTION("zero patient weight generates warning") {
        dataset.set_numeric<double>(tag_patient_weight, vr_type::DS, 0.0);
        auto result = validator.validate(dataset);
        CHECK(result.has_warnings());
    }

    SECTION("unrealistic patient weight generates warning") {
        dataset.set_numeric<double>(tag_patient_weight, vr_type::DS, 500.0);  // 500kg
        auto result = validator.validate(dataset);
        CHECK(result.has_warnings());
    }
}

// ============================================================================
// Attenuation and Scatter Correction Tests
// ============================================================================

TEST_CASE("pet_iod_validator validates correction methods", "[services][pet][validator][correction]") {
    pet_iod_validator validator;
    auto dataset = create_minimal_pet_dataset();

    SECTION("valid attenuation correction methods") {
        for (const auto& method : {"CT", "NONE", "MEASURED", "CALCULATED"}) {
            dataset.set_string(tag_attenuation_correction_method, vr_type::LO, method);
            auto result = validator.validate(dataset);
            CHECK(result.is_valid);
        }
    }

    SECTION("missing attenuation correction method generates info") {
        dataset.remove(tag_attenuation_correction_method);
        auto result = validator.validate(dataset);
        CHECK(has_info_findings(result));
    }

    SECTION("valid scatter correction methods") {
        for (const auto& method : {"MODEL", "NONE", "MEASURED"}) {
            dataset.set_string(tag_scatter_correction_method, vr_type::LO, method);
            auto result = validator.validate(dataset);
            CHECK(result.is_valid);
        }
    }

    SECTION("valid reconstruction methods") {
        for (const auto& method : {"OSEM3D", "FBP", "MLEM", "TOF-OSEM"}) {
            dataset.set_string(tag_reconstruction_method, vr_type::LO, method);
            auto result = validator.validate(dataset);
            CHECK(result.is_valid);
        }
    }
}

// ============================================================================
// Quick Check Tests
// ============================================================================

TEST_CASE("pet_iod_validator quick_check works correctly", "[services][pet][validator]") {
    pet_iod_validator validator;

    SECTION("valid dataset passes quick check") {
        auto dataset = create_minimal_pet_dataset();
        CHECK(validator.quick_check(dataset));
    }

    SECTION("invalid modality fails quick check") {
        auto dataset = create_minimal_pet_dataset();
        dataset.set_string(tags::modality, vr_type::CS, "NM");
        CHECK_FALSE(validator.quick_check(dataset));
    }

    SECTION("missing required attribute fails quick check") {
        auto dataset = create_minimal_pet_dataset();
        dataset.remove(tags::rows);
        CHECK_FALSE(validator.quick_check(dataset));
    }

    SECTION("missing Frame of Reference fails quick check") {
        auto dataset = create_minimal_pet_dataset();
        dataset.remove(tag_frame_of_reference_uid);
        CHECK_FALSE(validator.quick_check(dataset));
    }
}

// ============================================================================
// Custom Options Tests
// ============================================================================

TEST_CASE("pet_iod_validator with custom options", "[services][pet][validator]") {
    SECTION("strict mode treats warnings as errors") {
        pet_validation_options options;
        options.strict_mode = true;

        pet_iod_validator validator(options);
        auto dataset = create_minimal_pet_dataset();

        // Remove a Type 2 attribute to get a warning
        dataset.remove(tags::patient_name);

        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);  // Strict mode makes warnings into errors
    }

    SECTION("can disable pixel data validation") {
        pet_validation_options options;
        options.validate_pixel_data = false;

        pet_iod_validator validator(options);
        auto dataset = create_minimal_pet_dataset();
        dataset.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, 20);  // Invalid normally

        auto result = validator.validate(dataset);
        bool found_pixel_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "PT-ERR-004") {  // Pixel data error code
                found_pixel_error = true;
                break;
            }
        }
        CHECK_FALSE(found_pixel_error);
    }

    SECTION("can disable PET-specific validation") {
        pet_validation_options options;
        options.validate_pet_specific = false;

        pet_iod_validator validator(options);
        auto dataset = create_minimal_pet_dataset();

        auto result = validator.validate(dataset);
        // When validate_pet_specific is false, SUV/reconstruction/correction validation is skipped
        // Check that no PT-INFO codes from these validations appear
        bool found_suv_info = false;
        for (const auto& f : result.findings) {
            if (f.code == "PT-INFO-002" || f.code == "PT-INFO-003" ||
                f.code == "PT-INFO-004" || f.code == "PT-INFO-005" ||
                f.code == "PT-INFO-006") {
                found_suv_info = true;
                break;
            }
        }
        CHECK_FALSE(found_suv_info);
    }

    SECTION("can disable radiopharmaceutical validation") {
        pet_validation_options options;
        options.validate_radiopharmaceutical = false;

        pet_iod_validator validator(options);
        auto dataset = create_minimal_pet_dataset();

        auto result = validator.validate(dataset);
        // Should not have radiopharmaceutical info when validation is disabled
        bool found_radio_info = false;
        for (const auto& f : result.findings) {
            if (f.code.find("PT-INFO-RADIO") != std::string::npos) {
                found_radio_info = true;
                break;
            }
        }
        CHECK_FALSE(found_radio_info);
    }

    SECTION("can disable corrections validation") {
        pet_validation_options options;
        options.validate_corrections = false;

        pet_iod_validator validator(options);
        auto dataset = create_minimal_pet_dataset();
        dataset.remove(tag_attenuation_correction_method);

        auto result = validator.validate(dataset);
        bool found_correction_info = false;
        for (const auto& f : result.findings) {
            if (f.code == "PT-INFO-007") {
                found_correction_info = true;
                break;
            }
        }
        CHECK_FALSE(found_correction_info);
    }
}

// ============================================================================
// Convenience Function Tests
// ============================================================================

TEST_CASE("validate_pet_iod convenience function", "[services][pet][validator]") {
    auto dataset = create_minimal_pet_dataset();
    auto result = validate_pet_iod(dataset);
    CHECK(result.is_valid);
}

TEST_CASE("is_valid_pet_dataset convenience function", "[services][pet][validator]") {
    SECTION("valid dataset") {
        auto dataset = create_minimal_pet_dataset();
        CHECK(is_valid_pet_dataset(dataset));
    }

    SECTION("invalid dataset") {
        auto dataset = create_minimal_pet_dataset();
        dataset.set_string(tags::modality, vr_type::CS, "NM");
        CHECK_FALSE(is_valid_pet_dataset(dataset));
    }
}

// ============================================================================
// Enhanced PET Tests
// ============================================================================

TEST_CASE("pet_iod_validator handles Enhanced PET", "[services][pet][validator][enhanced]") {
    pet_iod_validator validator;
    auto dataset = create_minimal_pet_dataset();

    SECTION("Enhanced PET SOP Class is valid") {
        dataset.set_string(tags::sop_class_uid, vr_type::UI,
                          std::string(enhanced_pet_image_storage_uid));
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
    }

    SECTION("Legacy Converted Enhanced PET is valid") {
        dataset.set_string(tags::sop_class_uid, vr_type::UI,
                          std::string(legacy_converted_enhanced_pet_image_storage_uid));
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
    }
}

// ============================================================================
// Image Plane Module Tests
// ============================================================================

TEST_CASE("pet_iod_validator validates Image Plane Module", "[services][pet][validator][plane]") {
    pet_iod_validator validator;
    auto dataset = create_minimal_pet_dataset();

    SECTION("valid slice thickness") {
        dataset.set_numeric<double>(tag_slice_thickness, vr_type::DS, 4.0);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
    }

    SECTION("zero slice thickness generates warning") {
        dataset.set_numeric<double>(tag_slice_thickness, vr_type::DS, 0.0);
        auto result = validator.validate(dataset);
        CHECK(result.has_warnings());
    }

    SECTION("missing image position generates warning") {
        dataset.remove(tag_image_position_patient);
        auto result = validator.validate(dataset);
        CHECK(result.has_warnings());
    }

    SECTION("missing image orientation generates warning") {
        dataset.remove(tag_image_orientation_patient);
        auto result = validator.validate(dataset);
        CHECK(result.has_warnings());
    }

    SECTION("valid pixel spacing") {
        dataset.set_string(tag_pixel_spacing, vr_type::DS, "4.0\\4.0");
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
    }
}

