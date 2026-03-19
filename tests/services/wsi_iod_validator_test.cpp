/**
 * @file wsi_iod_validator_test.cpp
 * @brief Unit tests for VL Whole Slide Microscopy Image IOD Validator
 *
 * @see DICOM PS3.3 Section A.32.8 - VL Whole Slide Microscopy Image IOD
 * @see Issue #826 - Add WSI IOD validator
 */

#include <pacs/services/validation/wsi_iod_validator.hpp>
#include <pacs/services/sop_classes/wsi_storage.hpp>
#include <pacs/core/dicom_dataset.hpp>
#include <pacs/core/dicom_element.hpp>
#include <pacs/core/dicom_tag_constants.hpp>
#include <pacs/encoding/vr_type.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace kcenon::pacs::services::validation;
using namespace kcenon::pacs::services::sop_classes;
using namespace kcenon::pacs::core;
using namespace kcenon::pacs::encoding;

// ============================================================================
// Test Fixtures - Helper Functions
// ============================================================================

namespace {

/// Helper to create a minimal valid WSI dataset
dicom_dataset create_minimal_wsi_dataset() {
    dicom_dataset ds;

    // Patient Module (Type 2)
    ds.set_string(tags::patient_name, vr_type::PN, "Test^Patient");
    ds.set_string(tags::patient_id, vr_type::LO, "WSI12345");
    ds.set_string(tags::patient_birth_date, vr_type::DA, "19700101");
    ds.set_string(tags::patient_sex, vr_type::CS, "F");

    // General Study Module
    ds.set_string(tags::study_instance_uid, vr_type::UI,
                  "1.2.3.4.5.6.7.8.9");
    ds.set_string(tags::study_date, vr_type::DA, "20240101");
    ds.set_string(tags::study_time, vr_type::TM, "120000");
    ds.set_string(tags::referring_physician_name, vr_type::PN,
                  "Dr^Pathologist");
    ds.set_string(tags::study_id, vr_type::SH, "STUDY001");
    ds.set_string(tags::accession_number, vr_type::SH, "ACC001");

    // General Series Module — Modality = "SM" (Slide Microscopy)
    ds.set_string(tags::modality, vr_type::CS, "SM");
    ds.set_string(tags::series_instance_uid, vr_type::UI,
                  "1.2.3.4.5.6.7.8.9.1");
    ds.set_string(tags::series_number, vr_type::IS, "1");

    // Image Pixel Module — RGB for brightfield WSI
    ds.set_numeric<uint16_t>(tags::samples_per_pixel, vr_type::US, 3);
    ds.set_string(tags::photometric_interpretation, vr_type::CS, "RGB");
    ds.set_numeric<uint16_t>(tags::rows, vr_type::US, 256);
    ds.set_numeric<uint16_t>(tags::columns, vr_type::US, 256);
    ds.set_numeric<uint16_t>(tags::bits_allocated, vr_type::US, 8);
    ds.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, 8);
    ds.set_numeric<uint16_t>(tags::high_bit, vr_type::US, 7);
    ds.set_numeric<uint16_t>(tags::pixel_representation, vr_type::US, 0);

    // WSI Image Module — Type 1 attributes
    ds.set_string(tags::image_type, vr_type::CS,
                  "ORIGINAL\\PRIMARY\\VOLUME\\NONE");
    ds.set_numeric<uint32_t>(wsi_iod_tags::total_pixel_matrix_columns,
                              vr_type::UL, 98304);
    ds.set_numeric<uint32_t>(wsi_iod_tags::total_pixel_matrix_rows,
                              vr_type::UL, 65536);
    ds.set_string(wsi_iod_tags::number_of_frames, vr_type::IS, "1536");

    // Image Orientation (Slide) — Type 1C
    ds.set_string(wsi_iod_tags::image_orientation_slide, vr_type::DS,
                  "0.0\\-1.0\\0.0\\-1.0\\0.0\\0.0");

    // Imaged Volume Width/Height
    ds.set_string(wsi_iod_tags::imaged_volume_width, vr_type::DS, "25.0");
    ds.set_string(wsi_iod_tags::imaged_volume_height, vr_type::DS, "15.0");

    // Optical Path Module — Type 1
    ds.set_string(wsi_iod_tags::optical_path_identifier, vr_type::SH, "1");
    ds.set_string(wsi_iod_tags::optical_path_description, vr_type::ST,
                  "Brightfield");

    // Multi-frame Dimension Module — Type 1
    ds.set_string(wsi_iod_tags::dimension_organization_type, vr_type::CS,
                  "TILED_FULL");

    // Specimen Module (User Optional)
    ds.set_string(wsi_iod_tags::specimen_identifier, vr_type::LO,
                  "SPEC-001");
    ds.set_string(wsi_iod_tags::container_identifier, vr_type::LO,
                  "SLIDE-001");

    // SOP Common Module
    ds.set_string(tags::sop_class_uid, vr_type::UI,
                  std::string(wsi_image_storage_uid));
    ds.set_string(tags::sop_instance_uid, vr_type::UI,
                  "1.2.3.4.5.6.7.8.9.2");

    return ds;
}

}  // namespace

// ============================================================================
// WSI IOD Validator Basic Tests
// ============================================================================

TEST_CASE("wsi_iod_validator validates minimal valid dataset",
          "[services][wsi][validator]") {
    wsi_iod_validator validator;
    auto dataset = create_minimal_wsi_dataset();

    auto result = validator.validate(dataset);

    CHECK(result.is_valid);
    CHECK_FALSE(result.has_errors());
}

TEST_CASE("wsi_iod_validator detects missing Type 1 attributes",
          "[services][wsi][validator]") {
    wsi_iod_validator validator;
    auto dataset = create_minimal_wsi_dataset();

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

    SECTION("missing TotalPixelMatrixColumns") {
        dataset.remove(wsi_iod_tags::total_pixel_matrix_columns);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }

    SECTION("missing TotalPixelMatrixRows") {
        dataset.remove(wsi_iod_tags::total_pixel_matrix_rows);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }

    SECTION("missing NumberOfFrames") {
        dataset.remove(wsi_iod_tags::number_of_frames);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }

    SECTION("missing OpticalPathIdentifier") {
        dataset.remove(wsi_iod_tags::optical_path_identifier);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }

    SECTION("missing DimensionOrganizationType") {
        dataset.remove(wsi_iod_tags::dimension_organization_type);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }
}

// ============================================================================
// Type 2 Attribute Tests
// ============================================================================

TEST_CASE("wsi_iod_validator detects missing Type 2 attributes",
          "[services][wsi][validator]") {
    wsi_iod_validator validator;
    auto dataset = create_minimal_wsi_dataset();

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

    SECTION("missing SeriesNumber generates warning") {
        dataset.remove(tags::series_number);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
        CHECK(result.has_warnings());
    }

    SECTION("missing AccessionNumber generates warning") {
        dataset.remove(tags::accession_number);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
        CHECK(result.has_warnings());
    }
}

// ============================================================================
// Modality Validation Tests
// ============================================================================

TEST_CASE("wsi_iod_validator checks modality value",
          "[services][wsi][validator]") {
    wsi_iod_validator validator;
    auto dataset = create_minimal_wsi_dataset();

    SECTION("wrong modality - CT") {
        dataset.set_string(tags::modality, vr_type::CS, "CT");
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);

        bool found_modality_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "WSI-ERR-003") {
                found_modality_error = true;
                break;
            }
        }
        CHECK(found_modality_error);
    }

    SECTION("wrong modality - MR") {
        dataset.set_string(tags::modality, vr_type::CS, "MR");
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }
}

// ============================================================================
// SOP Class UID Tests
// ============================================================================

TEST_CASE("wsi_iod_validator checks SOP Class UID",
          "[services][wsi][validator]") {
    wsi_iod_validator validator;
    auto dataset = create_minimal_wsi_dataset();

    SECTION("WSI Image Storage is valid") {
        dataset.set_string(tags::sop_class_uid, vr_type::UI,
                          std::string(wsi_image_storage_uid));
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
    }

    SECTION("non-WSI SOP Class is invalid") {
        // CT SOP Class
        dataset.set_string(tags::sop_class_uid, vr_type::UI,
                          "1.2.840.10008.5.1.4.1.1.2");
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);

        bool found_sop_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "WSI-ERR-002") {
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

TEST_CASE("wsi_iod_validator checks pixel data consistency",
          "[services][wsi][validator]") {
    wsi_iod_validator validator;
    auto dataset = create_minimal_wsi_dataset();

    SECTION("BitsStored exceeds BitsAllocated") {
        dataset.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, 16);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);

        bool found_bits_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "WSI-ERR-004") {
                found_bits_error = true;
                break;
            }
        }
        CHECK(found_bits_error);
    }

    SECTION("wrong HighBit generates warning") {
        dataset.set_numeric<uint16_t>(tags::high_bit, vr_type::US, 5);
        auto result = validator.validate(dataset);
        CHECK(result.has_warnings());

        bool found_highbit_warning = false;
        for (const auto& f : result.findings) {
            if (f.code == "WSI-WARN-002") {
                found_highbit_warning = true;
                break;
            }
        }
        CHECK(found_highbit_warning);
    }

    SECTION("invalid PhotometricInterpretation") {
        dataset.set_string(tags::photometric_interpretation, vr_type::CS,
                          "PALETTE COLOR");
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);

        bool found_photometric_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "WSI-ERR-005") {
                found_photometric_error = true;
                break;
            }
        }
        CHECK(found_photometric_error);
    }

    SECTION("valid YBR_FULL_422 photometric") {
        dataset.set_string(tags::photometric_interpretation, vr_type::CS,
                          "YBR_FULL_422");
        auto result = validator.validate(dataset);
        bool found_photometric_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "WSI-ERR-005") {
                found_photometric_error = true;
                break;
            }
        }
        CHECK_FALSE(found_photometric_error);
    }

    SECTION("valid MONOCHROME2 with SamplesPerPixel=1") {
        dataset.set_string(tags::photometric_interpretation, vr_type::CS,
                          "MONOCHROME2");
        dataset.set_numeric<uint16_t>(tags::samples_per_pixel, vr_type::US, 1);
        auto result = validator.validate(dataset);
        bool found_photometric_error = false;
        bool found_samples_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "WSI-ERR-005") found_photometric_error = true;
            if (f.code == "WSI-ERR-006") found_samples_error = true;
        }
        CHECK_FALSE(found_photometric_error);
        CHECK_FALSE(found_samples_error);
    }

    SECTION("invalid SamplesPerPixel (not 1 or 3)") {
        dataset.set_numeric<uint16_t>(tags::samples_per_pixel, vr_type::US, 4);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);

        bool found_samples_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "WSI-ERR-006") {
                found_samples_error = true;
                break;
            }
        }
        CHECK(found_samples_error);
    }

    SECTION("signed pixel representation generates warning") {
        dataset.set_numeric<uint16_t>(tags::pixel_representation, vr_type::US,
                                      1);
        auto result = validator.validate(dataset);
        CHECK(result.has_warnings());

        bool found_repr_warning = false;
        for (const auto& f : result.findings) {
            if (f.code == "WSI-WARN-003") {
                found_repr_warning = true;
                break;
            }
        }
        CHECK(found_repr_warning);
    }
}

// ============================================================================
// WSI-Specific Attribute Tests
// ============================================================================

TEST_CASE("wsi_iod_validator checks WSI-specific attributes",
          "[services][wsi][validator]") {
    wsi_iod_validator validator;
    auto dataset = create_minimal_wsi_dataset();

    SECTION("invalid DimensionOrganizationType") {
        dataset.set_string(wsi_iod_tags::dimension_organization_type,
                          vr_type::CS, "3D");
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);

        bool found_dim_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "WSI-ERR-001") {
                found_dim_error = true;
                break;
            }
        }
        CHECK(found_dim_error);
    }

    SECTION("TILED_SPARSE is valid") {
        dataset.set_string(wsi_iod_tags::dimension_organization_type,
                          vr_type::CS, "TILED_SPARSE");
        auto result = validator.validate(dataset);
        bool found_dim_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "WSI-ERR-001") {
                found_dim_error = true;
                break;
            }
        }
        CHECK_FALSE(found_dim_error);
    }

    SECTION("missing ImageOrientationSlide generates warning") {
        dataset.remove(wsi_iod_tags::image_orientation_slide);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
        CHECK(result.has_warnings());

        bool found_orientation_warning = false;
        for (const auto& f : result.findings) {
            if (f.code == "WSI-WARN-001") {
                found_orientation_warning = true;
                break;
            }
        }
        CHECK(found_orientation_warning);
    }

    SECTION("missing ImagedVolumeWidth generates info") {
        dataset.remove(wsi_iod_tags::imaged_volume_width);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);

        bool found_volume_info = false;
        for (const auto& f : result.findings) {
            if (f.code == "WSI-INFO-001") {
                found_volume_info = true;
                break;
            }
        }
        CHECK(found_volume_info);
    }

    SECTION("missing ImagedVolumeHeight generates info") {
        dataset.remove(wsi_iod_tags::imaged_volume_height);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);

        bool found_volume_info = false;
        for (const auto& f : result.findings) {
            if (f.code == "WSI-INFO-002") {
                found_volume_info = true;
                break;
            }
        }
        CHECK(found_volume_info);
    }

    SECTION("missing OpticalPathDescription generates info") {
        dataset.remove(wsi_iod_tags::optical_path_description);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);

        bool found_path_info = false;
        for (const auto& f : result.findings) {
            if (f.code == "WSI-INFO-003") {
                found_path_info = true;
                break;
            }
        }
        CHECK(found_path_info);
    }

    SECTION("missing SpecimenIdentifier generates info") {
        dataset.remove(wsi_iod_tags::specimen_identifier);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);

        bool found_specimen_info = false;
        for (const auto& f : result.findings) {
            if (f.code == "WSI-INFO-004") {
                found_specimen_info = true;
                break;
            }
        }
        CHECK(found_specimen_info);
    }

    SECTION("missing ContainerIdentifier generates info") {
        dataset.remove(wsi_iod_tags::container_identifier);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);

        bool found_container_info = false;
        for (const auto& f : result.findings) {
            if (f.code == "WSI-INFO-005") {
                found_container_info = true;
                break;
            }
        }
        CHECK(found_container_info);
    }
}

// ============================================================================
// Quick Check Tests
// ============================================================================

TEST_CASE("wsi_iod_validator quick_check works correctly",
          "[services][wsi][validator]") {
    wsi_iod_validator validator;

    SECTION("valid dataset passes quick check") {
        auto dataset = create_minimal_wsi_dataset();
        CHECK(validator.quick_check(dataset));
    }

    SECTION("invalid modality fails quick check") {
        auto dataset = create_minimal_wsi_dataset();
        dataset.set_string(tags::modality, vr_type::CS, "CT");
        CHECK_FALSE(validator.quick_check(dataset));
    }

    SECTION("missing required attribute fails quick check") {
        auto dataset = create_minimal_wsi_dataset();
        dataset.remove(tags::rows);
        CHECK_FALSE(validator.quick_check(dataset));
    }

    SECTION("missing TotalPixelMatrixColumns fails quick check") {
        auto dataset = create_minimal_wsi_dataset();
        dataset.remove(wsi_iod_tags::total_pixel_matrix_columns);
        CHECK_FALSE(validator.quick_check(dataset));
    }

    SECTION("missing TotalPixelMatrixRows fails quick check") {
        auto dataset = create_minimal_wsi_dataset();
        dataset.remove(wsi_iod_tags::total_pixel_matrix_rows);
        CHECK_FALSE(validator.quick_check(dataset));
    }

    SECTION("missing SOPClassUID fails quick check") {
        auto dataset = create_minimal_wsi_dataset();
        dataset.remove(tags::sop_class_uid);
        CHECK_FALSE(validator.quick_check(dataset));
    }
}

// ============================================================================
// Custom Options Tests
// ============================================================================

TEST_CASE("wsi_iod_validator with custom options",
          "[services][wsi][validator]") {

    SECTION("strict mode treats warnings as errors") {
        wsi_validation_options options;
        options.strict_mode = true;

        wsi_iod_validator validator(options);
        auto dataset = create_minimal_wsi_dataset();

        // Remove a Type 2 attribute to get a warning
        dataset.remove(tags::patient_name);

        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }

    SECTION("can disable WSI parameter validation") {
        wsi_validation_options options;
        options.validate_wsi_params = false;

        wsi_iod_validator validator(options);
        auto dataset = create_minimal_wsi_dataset();

        // Verify the option doesn't crash
        auto result = validator.validate(dataset);
        CHECK(result.findings.size() >= 0);
    }

    SECTION("can disable pixel data validation") {
        wsi_validation_options options;
        options.validate_pixel_data = false;

        wsi_iod_validator validator(options);
        auto dataset = create_minimal_wsi_dataset();
        dataset.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, 16);

        auto result = validator.validate(dataset);
        // Should not have pixel data errors when validation is disabled
        bool found_pixel_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "WSI-ERR-004") {
                found_pixel_error = true;
                break;
            }
        }
        CHECK_FALSE(found_pixel_error);
    }

    SECTION("can disable optical path validation") {
        wsi_validation_options options;
        options.validate_optical_path = false;

        wsi_iod_validator validator(options);
        auto dataset = create_minimal_wsi_dataset();
        dataset.remove(wsi_iod_tags::optical_path_identifier);

        auto result = validator.validate(dataset);
        // Should not have optical path errors when validation is disabled
        bool found_optical_error = false;
        for (const auto& f : result.findings) {
            if (f.tag == wsi_iod_tags::optical_path_identifier &&
                f.severity == validation_severity::error) {
                found_optical_error = true;
                break;
            }
        }
        CHECK_FALSE(found_optical_error);
    }

    SECTION("options getter returns current options") {
        wsi_validation_options options;
        options.strict_mode = true;
        options.validate_wsi_params = false;

        wsi_iod_validator validator(options);
        CHECK(validator.options().strict_mode);
        CHECK_FALSE(validator.options().validate_wsi_params);
    }

    SECTION("set_options updates options") {
        wsi_iod_validator validator;
        CHECK_FALSE(validator.options().strict_mode);

        wsi_validation_options new_options;
        new_options.strict_mode = true;
        validator.set_options(new_options);

        CHECK(validator.options().strict_mode);
    }
}

// ============================================================================
// Convenience Function Tests
// ============================================================================

TEST_CASE("validate_wsi_iod convenience function",
          "[services][wsi][validator]") {
    auto dataset = create_minimal_wsi_dataset();
    auto result = validate_wsi_iod(dataset);
    CHECK(result.is_valid);
}

TEST_CASE("is_valid_wsi_dataset convenience function",
          "[services][wsi][validator]") {
    SECTION("valid dataset") {
        auto dataset = create_minimal_wsi_dataset();
        CHECK(is_valid_wsi_dataset(dataset));
    }

    SECTION("invalid dataset - wrong modality") {
        auto dataset = create_minimal_wsi_dataset();
        dataset.set_string(tags::modality, vr_type::CS, "CT");
        CHECK_FALSE(is_valid_wsi_dataset(dataset));
    }

    SECTION("invalid dataset - missing TotalPixelMatrixColumns") {
        auto dataset = create_minimal_wsi_dataset();
        dataset.remove(wsi_iod_tags::total_pixel_matrix_columns);
        CHECK_FALSE(is_valid_wsi_dataset(dataset));
    }
}
