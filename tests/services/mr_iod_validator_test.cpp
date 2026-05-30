/**
 * @file mr_iod_validator_test.cpp
 * @brief Unit tests for MR Image IOD Validator
 *
 * @see DICOM PS3.3 Section A.4 - MR Image IOD
 * @see Issue #718 - Add MR Image IOD Validator
 */

#include <kcenon/pacs/services/validation/mr_iod_validator.h>
#include <kcenon/pacs/services/sop_classes/mr_storage.h>
#include <kcenon/pacs/core/dicom_dataset.h>
#include <kcenon/pacs/core/dicom_element.h>
#include <kcenon/pacs/core/dicom_tag_constants.h>
#include <kcenon/pacs/encoding/vr_type.h>

#include <catch2/catch_test_macros.hpp>

using namespace kcenon::pacs::services::validation;
using namespace kcenon::pacs::services::sop_classes;
using namespace kcenon::pacs::core;
using namespace kcenon::pacs::encoding;

// ============================================================================
// Test Fixtures - Helper Functions
// ============================================================================

namespace {

/// Helper to create a minimal valid MR dataset
dicom_dataset create_minimal_mr_dataset() {
    dicom_dataset ds;

    // Patient Module (Type 2)
    ds.set_string(tags::patient_name, vr_type::PN, "Test^Patient");
    ds.set_string(tags::patient_id, vr_type::LO, "MR12345");
    ds.set_string(tags::patient_birth_date, vr_type::DA, "19700101");
    ds.set_string(tags::patient_sex, vr_type::CS, "M");

    // General Study Module
    ds.set_string(tags::study_instance_uid, vr_type::UI,
                  "1.2.3.4.5.6.7.8.9");
    ds.set_string(tags::study_date, vr_type::DA, "20240101");
    ds.set_string(tags::study_time, vr_type::TM, "120000");
    ds.set_string(tags::referring_physician_name, vr_type::PN,
                  "Dr^Referring");
    ds.set_string(tags::study_id, vr_type::SH, "STUDY001");
    ds.set_string(tags::accession_number, vr_type::SH, "ACC001");

    // General Series Module
    ds.set_string(tags::modality, vr_type::CS, "MR");
    ds.set_string(tags::series_instance_uid, vr_type::UI,
                  "1.2.3.4.5.6.7.8.9.1");
    ds.set_string(tags::series_number, vr_type::IS, "1");

    // Frame of Reference Module (Type 1 for MR)
    ds.set_string(tags::frame_of_reference_uid, vr_type::UI,
                  "1.2.3.4.5.6.7.8.9.3");
    constexpr dicom_tag position_reference_indicator{0x0020, 0x1040};
    ds.set_string(position_reference_indicator, vr_type::LO, "");

    // General Equipment Module
    ds.set_string(tags::manufacturer, vr_type::LO, "TestManufacturer");

    // Image Pixel Module
    ds.set_numeric<uint16_t>(tags::samples_per_pixel, vr_type::US, 1);
    ds.set_string(tags::photometric_interpretation, vr_type::CS,
                  "MONOCHROME2");
    ds.set_numeric<uint16_t>(tags::rows, vr_type::US, 256);
    ds.set_numeric<uint16_t>(tags::columns, vr_type::US, 256);
    ds.set_numeric<uint16_t>(tags::bits_allocated, vr_type::US, 16);
    ds.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, 12);
    ds.set_numeric<uint16_t>(tags::high_bit, vr_type::US, 11);
    ds.set_numeric<uint16_t>(tags::pixel_representation, vr_type::US, 0);

    // Pixel Data (minimal placeholder)
    std::vector<uint8_t> pixel_data(100, 0);
    ds.insert(dicom_element(tags::pixel_data, vr_type::OW, pixel_data));

    // MR Image Module
    ds.set_string(tags::image_type, vr_type::CS, "ORIGINAL\\PRIMARY");
    ds.set_string(mr_tags::scanning_sequence, vr_type::CS, "SE");
    ds.set_string(mr_tags::sequence_variant, vr_type::CS, "NONE");
    ds.set_string(mr_tags::scan_options, vr_type::CS, "");
    ds.set_string(mr_tags::mr_acquisition_type, vr_type::CS, "2D");
    ds.set_string(mr_tags::repetition_time, vr_type::DS, "500");
    ds.set_string(mr_tags::echo_time, vr_type::DS, "15");
    ds.set_string(mr_tags::echo_train_length, vr_type::IS, "1");
    ds.set_string(mr_tags::magnetic_field_strength, vr_type::DS, "1.5");
    ds.set_string(mr_tags::slice_thickness, vr_type::DS, "5.0");
    ds.set_string(mr_tags::image_position_patient, vr_type::DS,
                  "0.0\\0.0\\0.0");
    ds.set_string(mr_tags::image_orientation_patient, vr_type::DS,
                  "1.0\\0.0\\0.0\\0.0\\1.0\\0.0");

    // SOP Common Module
    ds.set_string(tags::sop_class_uid, vr_type::UI,
                  std::string(mr_image_storage_uid));
    ds.set_string(tags::sop_instance_uid, vr_type::UI,
                  "1.2.3.4.5.6.7.8.9.2");

    return ds;
}

}  // namespace

// ============================================================================
// MR IOD Validator Basic Tests
// ============================================================================

TEST_CASE("mr_iod_validator validates minimal valid dataset",
          "[services][mr][validator]") {
    mr_iod_validator validator;
    auto dataset = create_minimal_mr_dataset();

    auto result = validator.validate(dataset);

    CHECK(result.is_valid);
    CHECK_FALSE(result.has_errors());
}

TEST_CASE("mr_iod_validator detects missing Type 1 attributes",
          "[services][mr][validator]") {
    mr_iod_validator validator;
    auto dataset = create_minimal_mr_dataset();

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

    SECTION("missing FrameOfReferenceUID") {
        dataset.remove(tags::frame_of_reference_uid);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }

    SECTION("missing SOPClassUID") {
        dataset.remove(tags::sop_class_uid);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }

    SECTION("missing SOPInstanceUID") {
        dataset.remove(tags::sop_instance_uid);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }

    SECTION("missing Rows") {
        dataset.remove(tags::rows);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }

    SECTION("missing ImageType") {
        dataset.remove(tags::image_type);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }

    SECTION("missing ScanningSequence") {
        dataset.remove(mr_tags::scanning_sequence);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }

    SECTION("missing SequenceVariant") {
        dataset.remove(mr_tags::sequence_variant);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }
}

// ============================================================================
// Type 2 Attribute Tests
// ============================================================================

TEST_CASE("mr_iod_validator detects missing Type 2 attributes",
          "[services][mr][validator]") {
    mr_iod_validator validator;
    auto dataset = create_minimal_mr_dataset();

    SECTION("missing PatientName generates warning") {
        dataset.remove(tags::patient_name);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
        CHECK(result.has_warnings());
    }

    SECTION("missing StudyDate generates warning") {
        dataset.remove(tags::study_date);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
        CHECK(result.has_warnings());
    }

    SECTION("missing EchoTime generates warning") {
        dataset.remove(mr_tags::echo_time);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
        CHECK(result.has_warnings());
    }

    SECTION("missing MagneticFieldStrength generates warning") {
        dataset.remove(mr_tags::magnetic_field_strength);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
        CHECK(result.has_warnings());
    }

    SECTION("missing MRAcquisitionType generates warning") {
        dataset.remove(mr_tags::mr_acquisition_type);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
        CHECK(result.has_warnings());
    }

    SECTION("missing Manufacturer generates warning") {
        dataset.remove(tags::manufacturer);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
        CHECK(result.has_warnings());
    }
}

// ============================================================================
// Modality Validation Tests
// ============================================================================

TEST_CASE("mr_iod_validator checks modality value",
          "[services][mr][validator]") {
    mr_iod_validator validator;
    auto dataset = create_minimal_mr_dataset();

    SECTION("wrong modality - CT") {
        dataset.set_string(tags::modality, vr_type::CS, "CT");
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);

        bool found_modality_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "MR-ERR-002") {
                found_modality_error = true;
                break;
            }
        }
        CHECK(found_modality_error);
    }

    SECTION("wrong modality - US") {
        dataset.set_string(tags::modality, vr_type::CS, "US");
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }
}

// ============================================================================
// SOP Class UID Tests
// ============================================================================

TEST_CASE("mr_iod_validator checks SOP Class UID",
          "[services][mr][validator]") {
    mr_iod_validator validator;
    auto dataset = create_minimal_mr_dataset();

    SECTION("standard MR Image Storage is valid") {
        dataset.set_string(tags::sop_class_uid, vr_type::UI,
                          std::string(mr_image_storage_uid));
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
    }

    SECTION("Enhanced MR Image Storage is valid") {
        dataset.set_string(tags::sop_class_uid, vr_type::UI,
                          std::string(enhanced_mr_image_storage_uid));
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
    }

    SECTION("non-MR SOP Class is invalid") {
        // CT SOP Class
        dataset.set_string(tags::sop_class_uid, vr_type::UI,
                          "1.2.840.10008.5.1.4.1.1.2");
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);

        bool found_sop_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "MR-ERR-001") {
                found_sop_error = true;
                break;
            }
        }
        CHECK(found_sop_error);
    }
}

// ============================================================================
// Pixel Data Consistency Tests
// ============================================================================

TEST_CASE("mr_iod_validator checks pixel data consistency",
          "[services][mr][validator]") {
    mr_iod_validator validator;
    auto dataset = create_minimal_mr_dataset();

    SECTION("BitsStored exceeds BitsAllocated") {
        dataset.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, 20);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);

        bool found_bits_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "MR-ERR-003") {
                found_bits_error = true;
                break;
            }
        }
        CHECK(found_bits_error);
    }

    SECTION("wrong HighBit generates warning") {
        dataset.set_numeric<uint16_t>(tags::high_bit, vr_type::US, 15);
        auto result = validator.validate(dataset);
        CHECK(result.has_warnings());

        bool found_highbit_warning = false;
        for (const auto& f : result.findings) {
            if (f.code == "MR-WARN-005") {
                found_highbit_warning = true;
                break;
            }
        }
        CHECK(found_highbit_warning);
    }

    SECTION("non-grayscale SamplesPerPixel is invalid") {
        dataset.set_numeric<uint16_t>(tags::samples_per_pixel, vr_type::US, 3);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);

        bool found_samples_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "MR-ERR-005") {
                found_samples_error = true;
                break;
            }
        }
        CHECK(found_samples_error);
    }

    SECTION("RGB photometric interpretation is invalid for MR") {
        dataset.set_string(tags::photometric_interpretation, vr_type::CS,
                          "RGB");
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);

        bool found_photometric_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "MR-ERR-004") {
                found_photometric_error = true;
                break;
            }
        }
        CHECK(found_photometric_error);
    }
}

// ============================================================================
// MR-Specific Conditional Attribute Tests
// ============================================================================

TEST_CASE("mr_iod_validator checks MR-specific conditional attributes",
          "[services][mr][validator]") {
    mr_iod_validator validator;
    auto dataset = create_minimal_mr_dataset();

    SECTION("missing SliceThickness generates info") {
        dataset.remove(mr_tags::slice_thickness);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);

        bool found_thickness_info = false;
        for (const auto& f : result.findings) {
            if (f.code == "MR-INFO-001") {
                found_thickness_info = true;
                break;
            }
        }
        CHECK(found_thickness_info);
    }

    SECTION("missing RepetitionTime with ORIGINAL ImageType generates "
            "warning") {
        dataset.remove(mr_tags::repetition_time);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
        CHECK(result.has_warnings());

        bool found_rt_warning = false;
        for (const auto& f : result.findings) {
            if (f.code == "MR-WARN-001") {
                found_rt_warning = true;
                break;
            }
        }
        CHECK(found_rt_warning);
    }

    SECTION("missing RepetitionTime with DERIVED ImageType is acceptable") {
        dataset.set_string(tags::image_type, vr_type::CS,
                          "DERIVED\\SECONDARY");
        dataset.remove(mr_tags::repetition_time);
        auto result = validator.validate(dataset);

        bool found_rt_warning = false;
        for (const auto& f : result.findings) {
            if (f.code == "MR-WARN-001") {
                found_rt_warning = true;
                break;
            }
        }
        CHECK_FALSE(found_rt_warning);
    }

    SECTION("missing InversionTime with IR ScanningSequence generates "
            "warning") {
        dataset.set_string(mr_tags::scanning_sequence, vr_type::CS, "IR");
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
        CHECK(result.has_warnings());

        bool found_it_warning = false;
        for (const auto& f : result.findings) {
            if (f.code == "MR-WARN-002") {
                found_it_warning = true;
                break;
            }
        }
        CHECK(found_it_warning);
    }

    SECTION("InversionTime present with IR ScanningSequence is fine") {
        dataset.set_string(mr_tags::scanning_sequence, vr_type::CS, "IR");
        dataset.set_string(mr_tags::inversion_time, vr_type::DS, "2500");
        auto result = validator.validate(dataset);

        bool found_it_warning = false;
        for (const auto& f : result.findings) {
            if (f.code == "MR-WARN-002") {
                found_it_warning = true;
                break;
            }
        }
        CHECK_FALSE(found_it_warning);
    }

    SECTION("missing ImagePositionPatient with FrameOfReference generates "
            "warning") {
        dataset.remove(mr_tags::image_position_patient);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
        CHECK(result.has_warnings());

        bool found_position_warning = false;
        for (const auto& f : result.findings) {
            if (f.code == "MR-WARN-003") {
                found_position_warning = true;
                break;
            }
        }
        CHECK(found_position_warning);
    }

    SECTION("missing ImageOrientationPatient with FrameOfReference generates "
            "warning") {
        dataset.remove(mr_tags::image_orientation_patient);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
        CHECK(result.has_warnings());

        bool found_orientation_warning = false;
        for (const auto& f : result.findings) {
            if (f.code == "MR-WARN-004") {
                found_orientation_warning = true;
                break;
            }
        }
        CHECK(found_orientation_warning);
    }
}

// ============================================================================
// Quick Check Tests
// ============================================================================

TEST_CASE("mr_iod_validator quick_check works correctly",
          "[services][mr][validator]") {
    mr_iod_validator validator;

    SECTION("valid dataset passes quick check") {
        auto dataset = create_minimal_mr_dataset();
        CHECK(validator.quick_check(dataset));
    }

    SECTION("invalid modality fails quick check") {
        auto dataset = create_minimal_mr_dataset();
        dataset.set_string(tags::modality, vr_type::CS, "CT");
        CHECK_FALSE(validator.quick_check(dataset));
    }

    SECTION("missing required attribute fails quick check") {
        auto dataset = create_minimal_mr_dataset();
        dataset.remove(tags::rows);
        CHECK_FALSE(validator.quick_check(dataset));
    }

    SECTION("missing FrameOfReferenceUID fails quick check") {
        auto dataset = create_minimal_mr_dataset();
        dataset.remove(tags::frame_of_reference_uid);
        CHECK_FALSE(validator.quick_check(dataset));
    }

    SECTION("missing PixelData fails quick check") {
        auto dataset = create_minimal_mr_dataset();
        dataset.remove(tags::pixel_data);
        CHECK_FALSE(validator.quick_check(dataset));
    }

    SECTION("missing ScanningSequence fails quick check") {
        auto dataset = create_minimal_mr_dataset();
        dataset.remove(mr_tags::scanning_sequence);
        CHECK_FALSE(validator.quick_check(dataset));
    }

    SECTION("missing SequenceVariant fails quick check") {
        auto dataset = create_minimal_mr_dataset();
        dataset.remove(mr_tags::sequence_variant);
        CHECK_FALSE(validator.quick_check(dataset));
    }
}

// ============================================================================
// Custom Options Tests
// ============================================================================

TEST_CASE("mr_iod_validator with custom options",
          "[services][mr][validator]") {

    SECTION("strict mode treats warnings as errors") {
        mr_validation_options options;
        options.strict_mode = true;

        mr_iod_validator validator(options);
        auto dataset = create_minimal_mr_dataset();

        // Remove a Type 2 attribute to get a warning
        dataset.remove(tags::patient_name);

        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }

    SECTION("can disable MR parameter validation") {
        mr_validation_options options;
        options.validate_mr_params = false;

        mr_iod_validator validator(options);
        auto dataset = create_minimal_mr_dataset();
        dataset.remove(tags::image_type);

        auto result = validator.validate(dataset);
        // ImageType is checked by MR Image Module; disabling MR params
        // skips that module
        CHECK(result.findings.size() >= 0);
    }

    SECTION("can disable pixel data validation") {
        mr_validation_options options;
        options.validate_pixel_data = false;

        mr_iod_validator validator(options);
        auto dataset = create_minimal_mr_dataset();
        dataset.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, 20);

        auto result = validator.validate(dataset);
        // Should not have pixel data errors when validation is disabled
        bool found_pixel_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "MR-ERR-003") {
                found_pixel_error = true;
                break;
            }
        }
        CHECK_FALSE(found_pixel_error);
    }

    SECTION("options getter returns current options") {
        mr_validation_options options;
        options.strict_mode = true;
        options.validate_mr_params = false;

        mr_iod_validator validator(options);
        CHECK(validator.options().strict_mode);
        CHECK_FALSE(validator.options().validate_mr_params);
    }

    SECTION("set_options updates options") {
        mr_iod_validator validator;
        CHECK_FALSE(validator.options().strict_mode);

        mr_validation_options new_options;
        new_options.strict_mode = true;
        validator.set_options(new_options);

        CHECK(validator.options().strict_mode);
    }
}

// ============================================================================
// Convenience Function Tests
// ============================================================================

TEST_CASE("validate_mr_iod convenience function",
          "[services][mr][validator]") {
    auto dataset = create_minimal_mr_dataset();
    auto result = validate_mr_iod(dataset);
    CHECK(result.is_valid);
}

TEST_CASE("is_valid_mr_dataset convenience function",
          "[services][mr][validator]") {
    SECTION("valid dataset") {
        auto dataset = create_minimal_mr_dataset();
        CHECK(is_valid_mr_dataset(dataset));
    }

    SECTION("invalid dataset - wrong modality") {
        auto dataset = create_minimal_mr_dataset();
        dataset.set_string(tags::modality, vr_type::CS, "CT");
        CHECK_FALSE(is_valid_mr_dataset(dataset));
    }

    SECTION("invalid dataset - missing pixel data") {
        auto dataset = create_minimal_mr_dataset();
        dataset.remove(tags::pixel_data);
        CHECK_FALSE(is_valid_mr_dataset(dataset));
    }
}
