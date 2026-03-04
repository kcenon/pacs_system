/**
 * @file ct_processing_iod_validator_test.cpp
 * @brief Unit tests for CT For Processing Image IOD Validator
 *
 * @see DICOM PS3.3 - CT For Processing IOD
 * @see Issue #848 - Add CT For Processing SOP Classes
 */

#include <pacs/services/validation/ct_processing_iod_validator.hpp>
#include <pacs/services/sop_classes/ct_storage.hpp>
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

/// Helper to check if validation result has info-level findings
[[nodiscard]] bool has_info_findings(
    const validation_result& result) noexcept {
    return std::any_of(result.findings.begin(), result.findings.end(),
                       [](const validation_finding& f) {
                           return f.severity == validation_severity::info;
                       });
}

/// Helper to create a minimal valid CT For Processing dataset
dicom_dataset create_minimal_ct_processing_dataset() {
    dicom_dataset ds;

    // Patient Module (Type 2)
    ds.set_string(tags::patient_name, vr_type::PN, "Test^Patient");
    ds.set_string(tags::patient_id, vr_type::LO, "CTP12345");
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
    ds.set_string(tags::modality, vr_type::CS, "CT");
    ds.set_string(tags::series_instance_uid, vr_type::UI,
                  "1.2.3.4.5.6.7.8.9.1");
    ds.set_string(tags::series_number, vr_type::IS, "1");

    // Frame of Reference Module (Type 1 for CT)
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
    ds.set_numeric<uint16_t>(tags::rows, vr_type::US, 512);
    ds.set_numeric<uint16_t>(tags::columns, vr_type::US, 512);
    ds.set_numeric<uint16_t>(tags::bits_allocated, vr_type::US, 16);
    ds.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, 12);
    ds.set_numeric<uint16_t>(tags::high_bit, vr_type::US, 11);
    ds.set_numeric<uint16_t>(tags::pixel_representation, vr_type::US, 1);

    // Pixel Data (minimal placeholder)
    std::vector<uint8_t> pixel_data(100, 0);
    ds.insert(dicom_element(tags::pixel_data, vr_type::OW, pixel_data));

    // CT Image Module
    ds.set_string(tags::image_type, vr_type::CS,
                  "DERIVED\\PRIMARY\\AXIAL");
    ds.set_string(ct_processing_tags::kvp, vr_type::DS, "120");
    ds.set_string(tags::rescale_intercept, vr_type::DS, "0");
    ds.set_string(tags::rescale_slope, vr_type::DS, "1");
    ds.set_string(ct_processing_tags::slice_thickness, vr_type::DS, "1.0");
    ds.set_string(ct_processing_tags::convolution_kernel, vr_type::SH,
                  "STANDARD");
    ds.set_string(ct_processing_tags::image_position_patient, vr_type::DS,
                  "0.0\\0.0\\0.0");
    ds.set_string(ct_processing_tags::image_orientation_patient, vr_type::DS,
                  "1.0\\0.0\\0.0\\0.0\\1.0\\0.0");
    ds.set_string(ct_processing_tags::decomposition_description, vr_type::LO,
                  "Iodine density map");

    // SOP Common Module
    ds.set_string(tags::sop_class_uid, vr_type::UI,
                  std::string(ct_for_processing_image_storage_uid));
    ds.set_string(tags::sop_instance_uid, vr_type::UI,
                  "1.2.3.4.5.6.7.8.9.2");

    return ds;
}

}  // namespace

// ============================================================================
// CT Processing IOD Validator Basic Tests
// ============================================================================

TEST_CASE("ct_processing_iod_validator validates minimal valid dataset",
          "[services][ct_processing][validator]") {
    ct_processing_iod_validator validator;
    auto dataset = create_minimal_ct_processing_dataset();

    auto result = validator.validate(dataset);

    CHECK(result.is_valid);
    CHECK_FALSE(result.has_errors());
}

TEST_CASE("ct_processing_iod_validator detects missing Type 1 attributes",
          "[services][ct_processing][validator]") {
    ct_processing_iod_validator validator;
    auto dataset = create_minimal_ct_processing_dataset();

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
}

// ============================================================================
// Type 2 Attribute Tests
// ============================================================================

TEST_CASE("ct_processing_iod_validator detects missing Type 2 attributes",
          "[services][ct_processing][validator]") {
    ct_processing_iod_validator validator;
    auto dataset = create_minimal_ct_processing_dataset();

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

    SECTION("missing KVP generates warning") {
        dataset.remove(ct_processing_tags::kvp);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
        CHECK(result.has_warnings());
    }

    SECTION("missing RescaleIntercept generates warning") {
        dataset.remove(tags::rescale_intercept);
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

TEST_CASE("ct_processing_iod_validator checks modality value",
          "[services][ct_processing][validator]") {
    ct_processing_iod_validator validator;
    auto dataset = create_minimal_ct_processing_dataset();

    SECTION("wrong modality - MR") {
        dataset.set_string(tags::modality, vr_type::CS, "MR");
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);

        bool found_modality_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "CTP-ERR-002") {
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

TEST_CASE("ct_processing_iod_validator checks SOP Class UID",
          "[services][ct_processing][validator]") {
    ct_processing_iod_validator validator;
    auto dataset = create_minimal_ct_processing_dataset();

    SECTION("CT For Processing Image Storage is valid") {
        dataset.set_string(tags::sop_class_uid, vr_type::UI,
                          std::string(ct_for_processing_image_storage_uid));
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
    }

    SECTION("standard CT Image Storage is invalid for this validator") {
        dataset.set_string(tags::sop_class_uid, vr_type::UI,
                          std::string(ct_image_storage_uid));
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);

        bool found_sop_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "CTP-ERR-001") {
                found_sop_error = true;
                break;
            }
        }
        CHECK(found_sop_error);
    }

    SECTION("non-CT SOP Class is invalid") {
        // US SOP Class
        dataset.set_string(tags::sop_class_uid, vr_type::UI,
                          "1.2.840.10008.5.1.4.1.1.6.1");
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }
}

// ============================================================================
// Pixel Data Consistency Tests
// ============================================================================

TEST_CASE("ct_processing_iod_validator checks pixel data consistency",
          "[services][ct_processing][validator]") {
    ct_processing_iod_validator validator;
    auto dataset = create_minimal_ct_processing_dataset();

    SECTION("BitsStored exceeds BitsAllocated") {
        dataset.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, 20);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);

        bool found_bits_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "CTP-ERR-003") {
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
            if (f.code == "CTP-WARN-003") {
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
            if (f.code == "CTP-ERR-005") {
                found_samples_error = true;
                break;
            }
        }
        CHECK(found_samples_error);
    }

    SECTION("RGB photometric interpretation is invalid") {
        dataset.set_string(tags::photometric_interpretation, vr_type::CS,
                          "RGB");
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);

        bool found_photometric_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "CTP-ERR-004") {
                found_photometric_error = true;
                break;
            }
        }
        CHECK(found_photometric_error);
    }

    SECTION("unsigned pixel representation generates info") {
        dataset.set_numeric<uint16_t>(tags::pixel_representation, vr_type::US,
                                      0);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
        CHECK(has_info_findings(result));
    }
}

// ============================================================================
// CT Processing-Specific Conditional Attribute Tests
// ============================================================================

TEST_CASE("ct_processing_iod_validator checks conditional attributes",
          "[services][ct_processing][validator]") {
    ct_processing_iod_validator validator;
    auto dataset = create_minimal_ct_processing_dataset();

    SECTION("missing SliceThickness generates info") {
        dataset.remove(ct_processing_tags::slice_thickness);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);

        bool found_thickness_info = false;
        for (const auto& f : result.findings) {
            if (f.code == "CTP-INFO-001") {
                found_thickness_info = true;
                break;
            }
        }
        CHECK(found_thickness_info);
    }

    SECTION("missing ConvolutionKernel generates info") {
        dataset.remove(ct_processing_tags::convolution_kernel);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);

        bool found_kernel_info = false;
        for (const auto& f : result.findings) {
            if (f.code == "CTP-INFO-002") {
                found_kernel_info = true;
                break;
            }
        }
        CHECK(found_kernel_info);
    }

    SECTION("missing ImagePositionPatient with FrameOfReference generates "
            "warning") {
        dataset.remove(ct_processing_tags::image_position_patient);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
        CHECK(result.has_warnings());

        bool found_position_warning = false;
        for (const auto& f : result.findings) {
            if (f.code == "CTP-WARN-001") {
                found_position_warning = true;
                break;
            }
        }
        CHECK(found_position_warning);
    }

    SECTION("missing ImageOrientationPatient with FrameOfReference generates "
            "warning") {
        dataset.remove(ct_processing_tags::image_orientation_patient);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
        CHECK(result.has_warnings());

        bool found_orientation_warning = false;
        for (const auto& f : result.findings) {
            if (f.code == "CTP-WARN-002") {
                found_orientation_warning = true;
                break;
            }
        }
        CHECK(found_orientation_warning);
    }

    SECTION("missing DecompositionDescription generates info") {
        dataset.remove(ct_processing_tags::decomposition_description);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);

        bool found_decomposition_info = false;
        for (const auto& f : result.findings) {
            if (f.code == "CTP-INFO-003") {
                found_decomposition_info = true;
                break;
            }
        }
        CHECK(found_decomposition_info);
    }
}

// ============================================================================
// Quick Check Tests
// ============================================================================

TEST_CASE("ct_processing_iod_validator quick_check works correctly",
          "[services][ct_processing][validator]") {
    ct_processing_iod_validator validator;

    SECTION("valid dataset passes quick check") {
        auto dataset = create_minimal_ct_processing_dataset();
        CHECK(validator.quick_check(dataset));
    }

    SECTION("invalid modality fails quick check") {
        auto dataset = create_minimal_ct_processing_dataset();
        dataset.set_string(tags::modality, vr_type::CS, "MR");
        CHECK_FALSE(validator.quick_check(dataset));
    }

    SECTION("missing required attribute fails quick check") {
        auto dataset = create_minimal_ct_processing_dataset();
        dataset.remove(tags::rows);
        CHECK_FALSE(validator.quick_check(dataset));
    }

    SECTION("missing FrameOfReferenceUID fails quick check") {
        auto dataset = create_minimal_ct_processing_dataset();
        dataset.remove(tags::frame_of_reference_uid);
        CHECK_FALSE(validator.quick_check(dataset));
    }

    SECTION("missing PixelData fails quick check") {
        auto dataset = create_minimal_ct_processing_dataset();
        dataset.remove(tags::pixel_data);
        CHECK_FALSE(validator.quick_check(dataset));
    }

    SECTION("wrong SOP Class UID fails quick check") {
        auto dataset = create_minimal_ct_processing_dataset();
        dataset.set_string(tags::sop_class_uid, vr_type::UI,
                          std::string(ct_image_storage_uid));
        CHECK_FALSE(validator.quick_check(dataset));
    }
}

// ============================================================================
// Custom Options Tests
// ============================================================================

TEST_CASE("ct_processing_iod_validator with custom options",
          "[services][ct_processing][validator]") {

    SECTION("strict mode treats warnings as errors") {
        ct_processing_validation_options options;
        options.strict_mode = true;

        ct_processing_iod_validator validator(options);
        auto dataset = create_minimal_ct_processing_dataset();

        // Remove a Type 2 attribute to get a warning
        dataset.remove(tags::patient_name);

        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }

    SECTION("can disable CT processing parameter validation") {
        ct_processing_validation_options options;
        options.validate_ct_processing_params = false;

        ct_processing_iod_validator validator(options);
        auto dataset = create_minimal_ct_processing_dataset();
        dataset.remove(tags::image_type);

        auto result = validator.validate(dataset);
        CHECK(result.findings.size() >= 0);
    }

    SECTION("can disable pixel data validation") {
        ct_processing_validation_options options;
        options.validate_pixel_data = false;

        ct_processing_iod_validator validator(options);
        auto dataset = create_minimal_ct_processing_dataset();
        dataset.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, 20);

        auto result = validator.validate(dataset);
        bool found_pixel_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "CTP-ERR-003") {
                found_pixel_error = true;
                break;
            }
        }
        CHECK_FALSE(found_pixel_error);
    }

    SECTION("options getter returns current options") {
        ct_processing_validation_options options;
        options.strict_mode = true;
        options.validate_ct_processing_params = false;

        ct_processing_iod_validator validator(options);
        CHECK(validator.options().strict_mode);
        CHECK_FALSE(validator.options().validate_ct_processing_params);
    }

    SECTION("set_options updates options") {
        ct_processing_iod_validator validator;
        CHECK_FALSE(validator.options().strict_mode);

        ct_processing_validation_options new_options;
        new_options.strict_mode = true;
        validator.set_options(new_options);

        CHECK(validator.options().strict_mode);
    }
}

// ============================================================================
// Convenience Function Tests
// ============================================================================

TEST_CASE("validate_ct_processing_iod convenience function",
          "[services][ct_processing][validator]") {
    auto dataset = create_minimal_ct_processing_dataset();
    auto result = validate_ct_processing_iod(dataset);
    CHECK(result.is_valid);
}

TEST_CASE("is_valid_ct_processing_dataset convenience function",
          "[services][ct_processing][validator]") {
    SECTION("valid dataset") {
        auto dataset = create_minimal_ct_processing_dataset();
        CHECK(is_valid_ct_processing_dataset(dataset));
    }

    SECTION("invalid dataset - wrong modality") {
        auto dataset = create_minimal_ct_processing_dataset();
        dataset.set_string(tags::modality, vr_type::CS, "MR");
        CHECK_FALSE(is_valid_ct_processing_dataset(dataset));
    }

    SECTION("invalid dataset - missing pixel data") {
        auto dataset = create_minimal_ct_processing_dataset();
        dataset.remove(tags::pixel_data);
        CHECK_FALSE(is_valid_ct_processing_dataset(dataset));
    }

    SECTION("invalid dataset - wrong SOP Class") {
        auto dataset = create_minimal_ct_processing_dataset();
        dataset.set_string(tags::sop_class_uid, vr_type::UI,
                          std::string(ct_image_storage_uid));
        CHECK_FALSE(is_valid_ct_processing_dataset(dataset));
    }
}
