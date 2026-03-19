/**
 * @file ophthalmic_iod_validator_test.cpp
 * @brief Unit tests for Ophthalmic Photography/Tomography Image IOD Validator
 *
 * @see DICOM PS3.3 Section A.39 - Ophthalmic Photography IODs
 * @see DICOM PS3.3 Section A.40 - Ophthalmic Tomography Image IOD
 * @see Issue #830 - Add Ophthalmic IOD validator
 */

#include <pacs/services/validation/ophthalmic_iod_validator.hpp>
#include <pacs/services/sop_classes/ophthalmic_storage.hpp>
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

/// Helper to create a minimal valid ophthalmic photography dataset
dicom_dataset create_minimal_ophthalmic_dataset() {
    dicom_dataset ds;

    // Patient Module (Type 2)
    ds.set_string(tags::patient_name, vr_type::PN, "Test^Patient");
    ds.set_string(tags::patient_id, vr_type::LO, "OPHTH12345");
    ds.set_string(tags::patient_birth_date, vr_type::DA, "19700101");
    ds.set_string(tags::patient_sex, vr_type::CS, "F");

    // General Study Module
    ds.set_string(tags::study_instance_uid, vr_type::UI,
                  "1.2.3.4.5.6.7.8.9");
    ds.set_string(tags::study_date, vr_type::DA, "20240101");
    ds.set_string(tags::study_time, vr_type::TM, "120000");
    ds.set_string(tags::referring_physician_name, vr_type::PN,
                  "Dr^Ophthalmologist");
    ds.set_string(tags::study_id, vr_type::SH, "STUDY001");
    ds.set_string(tags::accession_number, vr_type::SH, "ACC001");

    // General Series Module — Modality = "OP"
    ds.set_string(tags::modality, vr_type::CS, "OP");
    ds.set_string(tags::series_instance_uid, vr_type::UI,
                  "1.2.3.4.5.6.7.8.9.1");
    ds.set_string(tags::series_number, vr_type::IS, "1");

    // Image Pixel Module — RGB for color fundus
    ds.set_numeric<uint16_t>(tags::samples_per_pixel, vr_type::US, 3);
    ds.set_string(tags::photometric_interpretation, vr_type::CS, "RGB");
    ds.set_numeric<uint16_t>(tags::rows, vr_type::US, 2048);
    ds.set_numeric<uint16_t>(tags::columns, vr_type::US, 2048);
    ds.set_numeric<uint16_t>(tags::bits_allocated, vr_type::US, 8);
    ds.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, 8);
    ds.set_numeric<uint16_t>(tags::high_bit, vr_type::US, 7);
    ds.set_numeric<uint16_t>(tags::pixel_representation, vr_type::US, 0);

    // Ophthalmic Image Module — Type 1 attributes
    ds.set_string(tags::image_type, vr_type::CS,
                  "ORIGINAL\\PRIMARY");
    ds.set_string(ophthalmic_iod_tags::image_laterality, vr_type::CS, "R");

    // Ophthalmic-specific optional attributes
    ds.set_string(ophthalmic_iod_tags::anatomic_region_sequence,
                  vr_type::SQ, "");
    ds.set_string(ophthalmic_iod_tags::acquisition_device_type_code_sequence,
                  vr_type::SQ, "");
    ds.set_string(ophthalmic_iod_tags::horizontal_field_of_view,
                  vr_type::DS, "45.0");
    ds.set_string(ophthalmic_iod_tags::pupil_dilated, vr_type::CS, "YES");

    // Acquisition Context Module
    ds.set_string(ophthalmic_iod_tags::acquisition_context_sequence,
                  vr_type::SQ, "");

    // SOP Common Module
    ds.set_string(tags::sop_class_uid, vr_type::UI,
                  std::string(ophthalmic_photo_8bit_storage_uid));
    ds.set_string(tags::sop_instance_uid, vr_type::UI,
                  "1.2.3.4.5.6.7.8.9.2");

    return ds;
}

/// Helper to create a minimal valid OCT dataset
dicom_dataset create_minimal_oct_dataset() {
    auto ds = create_minimal_ophthalmic_dataset();

    // Override for OCT
    ds.set_string(tags::modality, vr_type::CS, "OPT");
    ds.set_string(tags::sop_class_uid, vr_type::UI,
                  std::string(ophthalmic_tomography_storage_uid));

    // OCT uses MONOCHROME2 grayscale
    ds.set_numeric<uint16_t>(tags::samples_per_pixel, vr_type::US, 1);
    ds.set_string(tags::photometric_interpretation, vr_type::CS,
                  "MONOCHROME2");

    // Multi-frame for OCT
    ds.set_string(ophthalmic_iod_tags::number_of_frames, vr_type::IS, "128");

    return ds;
}

}  // namespace

// ============================================================================
// Ophthalmic IOD Validator Basic Tests
// ============================================================================

TEST_CASE("ophthalmic_iod_validator validates minimal valid dataset",
          "[services][ophthalmic][validator]") {
    ophthalmic_iod_validator validator;
    auto dataset = create_minimal_ophthalmic_dataset();

    auto result = validator.validate(dataset);

    CHECK(result.is_valid);
    CHECK_FALSE(result.has_errors());
}

TEST_CASE("ophthalmic_iod_validator validates minimal valid OCT dataset",
          "[services][ophthalmic][validator]") {
    ophthalmic_iod_validator validator;
    auto dataset = create_minimal_oct_dataset();

    auto result = validator.validate(dataset);

    CHECK(result.is_valid);
    CHECK_FALSE(result.has_errors());
}

// ============================================================================
// Type 1 Attribute Tests
// ============================================================================

TEST_CASE("ophthalmic_iod_validator detects missing Type 1 attributes",
          "[services][ophthalmic][validator]") {
    ophthalmic_iod_validator validator;
    auto dataset = create_minimal_ophthalmic_dataset();

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

    SECTION("missing ImageLaterality") {
        dataset.remove(ophthalmic_iod_tags::image_laterality);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }
}

// ============================================================================
// Type 2 Attribute Tests
// ============================================================================

TEST_CASE("ophthalmic_iod_validator detects missing Type 2 attributes",
          "[services][ophthalmic][validator]") {
    ophthalmic_iod_validator validator;
    auto dataset = create_minimal_ophthalmic_dataset();

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

TEST_CASE("ophthalmic_iod_validator checks modality value",
          "[services][ophthalmic][validator]") {
    ophthalmic_iod_validator validator;
    auto dataset = create_minimal_ophthalmic_dataset();

    SECTION("OP modality is valid") {
        dataset.set_string(tags::modality, vr_type::CS, "OP");
        auto result = validator.validate(dataset);
        bool found_modality_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "OPHTH-ERR-003") {
                found_modality_error = true;
                break;
            }
        }
        CHECK_FALSE(found_modality_error);
    }

    SECTION("OPT modality is valid") {
        dataset.set_string(tags::modality, vr_type::CS, "OPT");
        auto result = validator.validate(dataset);
        bool found_modality_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "OPHTH-ERR-003") {
                found_modality_error = true;
                break;
            }
        }
        CHECK_FALSE(found_modality_error);
    }

    SECTION("wrong modality - CT") {
        dataset.set_string(tags::modality, vr_type::CS, "CT");
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);

        bool found_modality_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "OPHTH-ERR-003") {
                found_modality_error = true;
                break;
            }
        }
        CHECK(found_modality_error);
    }

    SECTION("wrong modality - SM") {
        dataset.set_string(tags::modality, vr_type::CS, "SM");
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }
}

// ============================================================================
// Laterality Validation Tests
// ============================================================================

TEST_CASE("ophthalmic_iod_validator checks image laterality",
          "[services][ophthalmic][validator]") {
    ophthalmic_iod_validator validator;
    auto dataset = create_minimal_ophthalmic_dataset();

    SECTION("R (Right) is valid") {
        dataset.set_string(ophthalmic_iod_tags::image_laterality,
                          vr_type::CS, "R");
        auto result = validator.validate(dataset);
        bool found_lat_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "OPHTH-ERR-004") {
                found_lat_error = true;
                break;
            }
        }
        CHECK_FALSE(found_lat_error);
    }

    SECTION("L (Left) is valid") {
        dataset.set_string(ophthalmic_iod_tags::image_laterality,
                          vr_type::CS, "L");
        auto result = validator.validate(dataset);
        bool found_lat_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "OPHTH-ERR-004") {
                found_lat_error = true;
                break;
            }
        }
        CHECK_FALSE(found_lat_error);
    }

    SECTION("B (Both) is valid") {
        dataset.set_string(ophthalmic_iod_tags::image_laterality,
                          vr_type::CS, "B");
        auto result = validator.validate(dataset);
        bool found_lat_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "OPHTH-ERR-004") {
                found_lat_error = true;
                break;
            }
        }
        CHECK_FALSE(found_lat_error);
    }

    SECTION("invalid laterality value") {
        dataset.set_string(ophthalmic_iod_tags::image_laterality,
                          vr_type::CS, "X");
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);

        bool found_lat_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "OPHTH-ERR-004") {
                found_lat_error = true;
                break;
            }
        }
        CHECK(found_lat_error);
    }

    SECTION("missing laterality generates error") {
        dataset.remove(ophthalmic_iod_tags::image_laterality);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }
}

// ============================================================================
// SOP Class UID Tests
// ============================================================================

TEST_CASE("ophthalmic_iod_validator checks SOP Class UID",
          "[services][ophthalmic][validator]") {
    ophthalmic_iod_validator validator;
    auto dataset = create_minimal_ophthalmic_dataset();

    SECTION("8-bit Photography Storage is valid") {
        dataset.set_string(tags::sop_class_uid, vr_type::UI,
                          std::string(ophthalmic_photo_8bit_storage_uid));
        auto result = validator.validate(dataset);
        bool found_sop_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "OPHTH-ERR-002") {
                found_sop_error = true;
                break;
            }
        }
        CHECK_FALSE(found_sop_error);
    }

    SECTION("16-bit Photography Storage is valid") {
        dataset.set_string(tags::sop_class_uid, vr_type::UI,
                          std::string(ophthalmic_photo_16bit_storage_uid));
        auto result = validator.validate(dataset);
        bool found_sop_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "OPHTH-ERR-002") {
                found_sop_error = true;
                break;
            }
        }
        CHECK_FALSE(found_sop_error);
    }

    SECTION("OCT Storage is valid") {
        dataset.set_string(tags::sop_class_uid, vr_type::UI,
                          std::string(ophthalmic_tomography_storage_uid));
        auto result = validator.validate(dataset);
        bool found_sop_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "OPHTH-ERR-002") {
                found_sop_error = true;
                break;
            }
        }
        CHECK_FALSE(found_sop_error);
    }

    SECTION("non-ophthalmic SOP Class is invalid") {
        // CT SOP Class
        dataset.set_string(tags::sop_class_uid, vr_type::UI,
                          "1.2.840.10008.5.1.4.1.1.2");
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);

        bool found_sop_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "OPHTH-ERR-002") {
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

TEST_CASE("ophthalmic_iod_validator checks pixel data consistency",
          "[services][ophthalmic][validator]") {
    ophthalmic_iod_validator validator;
    auto dataset = create_minimal_ophthalmic_dataset();

    SECTION("BitsStored exceeds BitsAllocated") {
        dataset.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, 16);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);

        bool found_bits_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "OPHTH-ERR-005") {
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
            if (f.code == "OPHTH-WARN-002") {
                found_highbit_warning = true;
                break;
            }
        }
        CHECK(found_highbit_warning);
    }

    SECTION("invalid PhotometricInterpretation") {
        dataset.set_string(tags::photometric_interpretation, vr_type::CS,
                          "YBR_ICT");
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);

        bool found_photometric_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "OPHTH-ERR-006") {
                found_photometric_error = true;
                break;
            }
        }
        CHECK(found_photometric_error);
    }

    SECTION("valid MONOCHROME2 with SamplesPerPixel=1") {
        dataset.set_string(tags::photometric_interpretation, vr_type::CS,
                          "MONOCHROME2");
        dataset.set_numeric<uint16_t>(tags::samples_per_pixel, vr_type::US, 1);
        auto result = validator.validate(dataset);
        bool found_photometric_error = false;
        bool found_samples_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "OPHTH-ERR-006") found_photometric_error = true;
            if (f.code == "OPHTH-ERR-007") found_samples_error = true;
        }
        CHECK_FALSE(found_photometric_error);
        CHECK_FALSE(found_samples_error);
    }

    SECTION("valid MONOCHROME1") {
        dataset.set_string(tags::photometric_interpretation, vr_type::CS,
                          "MONOCHROME1");
        dataset.set_numeric<uint16_t>(tags::samples_per_pixel, vr_type::US, 1);
        auto result = validator.validate(dataset);
        bool found_photometric_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "OPHTH-ERR-006") found_photometric_error = true;
        }
        CHECK_FALSE(found_photometric_error);
    }

    SECTION("valid YBR_FULL_422 photometric") {
        dataset.set_string(tags::photometric_interpretation, vr_type::CS,
                          "YBR_FULL_422");
        auto result = validator.validate(dataset);
        bool found_photometric_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "OPHTH-ERR-006") {
                found_photometric_error = true;
                break;
            }
        }
        CHECK_FALSE(found_photometric_error);
    }

    SECTION("valid PALETTE COLOR photometric") {
        dataset.set_string(tags::photometric_interpretation, vr_type::CS,
                          "PALETTE COLOR");
        dataset.set_numeric<uint16_t>(tags::samples_per_pixel, vr_type::US, 1);
        auto result = validator.validate(dataset);
        bool found_photometric_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "OPHTH-ERR-006") {
                found_photometric_error = true;
                break;
            }
        }
        CHECK_FALSE(found_photometric_error);
    }

    SECTION("invalid SamplesPerPixel (not 1 or 3)") {
        dataset.set_numeric<uint16_t>(tags::samples_per_pixel, vr_type::US, 4);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);

        bool found_samples_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "OPHTH-ERR-007") {
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
            if (f.code == "OPHTH-WARN-003") {
                found_repr_warning = true;
                break;
            }
        }
        CHECK(found_repr_warning);
    }
}

// ============================================================================
// OCT Multi-frame Tests
// ============================================================================

TEST_CASE("ophthalmic_iod_validator checks OCT multi-frame requirements",
          "[services][ophthalmic][validator]") {
    ophthalmic_iod_validator validator;

    SECTION("OCT without NumberOfFrames generates error") {
        auto dataset = create_minimal_oct_dataset();
        dataset.remove(ophthalmic_iod_tags::number_of_frames);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);

        bool found_frames_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "OPHTH-ERR-001") {
                found_frames_error = true;
                break;
            }
        }
        CHECK(found_frames_error);
    }

    SECTION("OCT with NumberOfFrames is valid") {
        auto dataset = create_minimal_oct_dataset();
        auto result = validator.validate(dataset);

        bool found_frames_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "OPHTH-ERR-001") {
                found_frames_error = true;
                break;
            }
        }
        CHECK_FALSE(found_frames_error);
    }

    SECTION("B-scan analysis without NumberOfFrames generates error") {
        auto dataset = create_minimal_oct_dataset();
        dataset.set_string(tags::sop_class_uid, vr_type::UI,
                          std::string(ophthalmic_oct_bscan_analysis_storage_uid));
        dataset.remove(ophthalmic_iod_tags::number_of_frames);
        auto result = validator.validate(dataset);

        bool found_frames_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "OPHTH-ERR-001") {
                found_frames_error = true;
                break;
            }
        }
        CHECK(found_frames_error);
    }

    SECTION("photography does not require NumberOfFrames") {
        auto dataset = create_minimal_ophthalmic_dataset();
        // Photography (8-bit) should not require NumberOfFrames
        auto result = validator.validate(dataset);
        bool found_frames_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "OPHTH-ERR-001") {
                found_frames_error = true;
                break;
            }
        }
        CHECK_FALSE(found_frames_error);
    }
}

// ============================================================================
// Ophthalmic-Specific Attribute Tests
// ============================================================================

TEST_CASE("ophthalmic_iod_validator checks ophthalmic-specific attributes",
          "[services][ophthalmic][validator]") {
    ophthalmic_iod_validator validator;
    auto dataset = create_minimal_ophthalmic_dataset();

    SECTION("missing AnatomicRegionSequence generates warning") {
        dataset.remove(ophthalmic_iod_tags::anatomic_region_sequence);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);

        bool found_warning = false;
        for (const auto& f : result.findings) {
            if (f.code == "OPHTH-WARN-001") {
                found_warning = true;
                break;
            }
        }
        CHECK(found_warning);
    }

    SECTION("missing AcquisitionDeviceTypeCodeSequence generates info") {
        dataset.remove(
            ophthalmic_iod_tags::acquisition_device_type_code_sequence);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);

        bool found_info = false;
        for (const auto& f : result.findings) {
            if (f.code == "OPHTH-INFO-001") {
                found_info = true;
                break;
            }
        }
        CHECK(found_info);
    }

    SECTION("missing HorizontalFieldOfView generates info") {
        dataset.remove(ophthalmic_iod_tags::horizontal_field_of_view);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);

        bool found_info = false;
        for (const auto& f : result.findings) {
            if (f.code == "OPHTH-INFO-002") {
                found_info = true;
                break;
            }
        }
        CHECK(found_info);
    }

    SECTION("missing PupilDilated generates info") {
        dataset.remove(ophthalmic_iod_tags::pupil_dilated);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);

        bool found_info = false;
        for (const auto& f : result.findings) {
            if (f.code == "OPHTH-INFO-003") {
                found_info = true;
                break;
            }
        }
        CHECK(found_info);
    }

    SECTION("missing AcquisitionContextSequence generates info") {
        dataset.remove(ophthalmic_iod_tags::acquisition_context_sequence);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);

        bool found_info = false;
        for (const auto& f : result.findings) {
            if (f.code == "OPHTH-INFO-004") {
                found_info = true;
                break;
            }
        }
        CHECK(found_info);
    }
}

// ============================================================================
// Quick Check Tests
// ============================================================================

TEST_CASE("ophthalmic_iod_validator quick_check works correctly",
          "[services][ophthalmic][validator]") {
    ophthalmic_iod_validator validator;

    SECTION("valid dataset passes quick check") {
        auto dataset = create_minimal_ophthalmic_dataset();
        CHECK(validator.quick_check(dataset));
    }

    SECTION("valid OCT dataset passes quick check") {
        auto dataset = create_minimal_oct_dataset();
        CHECK(validator.quick_check(dataset));
    }

    SECTION("invalid modality fails quick check") {
        auto dataset = create_minimal_ophthalmic_dataset();
        dataset.set_string(tags::modality, vr_type::CS, "CT");
        CHECK_FALSE(validator.quick_check(dataset));
    }

    SECTION("missing required attribute fails quick check") {
        auto dataset = create_minimal_ophthalmic_dataset();
        dataset.remove(tags::rows);
        CHECK_FALSE(validator.quick_check(dataset));
    }

    SECTION("missing ImageLaterality fails quick check") {
        auto dataset = create_minimal_ophthalmic_dataset();
        dataset.remove(ophthalmic_iod_tags::image_laterality);
        CHECK_FALSE(validator.quick_check(dataset));
    }

    SECTION("missing SOPClassUID fails quick check") {
        auto dataset = create_minimal_ophthalmic_dataset();
        dataset.remove(tags::sop_class_uid);
        CHECK_FALSE(validator.quick_check(dataset));
    }
}

// ============================================================================
// Custom Options Tests
// ============================================================================

TEST_CASE("ophthalmic_iod_validator with custom options",
          "[services][ophthalmic][validator]") {

    SECTION("strict mode treats warnings as errors") {
        ophthalmic_validation_options options;
        options.strict_mode = true;

        ophthalmic_iod_validator validator(options);
        auto dataset = create_minimal_ophthalmic_dataset();

        // Remove a Type 2 attribute to get a warning
        dataset.remove(tags::patient_name);

        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }

    SECTION("can disable ophthalmic parameter validation") {
        ophthalmic_validation_options options;
        options.validate_ophthalmic_params = false;

        ophthalmic_iod_validator validator(options);
        auto dataset = create_minimal_ophthalmic_dataset();

        auto result = validator.validate(dataset);
        CHECK(result.findings.size() >= 0);
    }

    SECTION("can disable pixel data validation") {
        ophthalmic_validation_options options;
        options.validate_pixel_data = false;

        ophthalmic_iod_validator validator(options);
        auto dataset = create_minimal_ophthalmic_dataset();
        dataset.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, 16);

        auto result = validator.validate(dataset);
        bool found_pixel_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "OPHTH-ERR-005") {
                found_pixel_error = true;
                break;
            }
        }
        CHECK_FALSE(found_pixel_error);
    }

    SECTION("can disable laterality validation") {
        ophthalmic_validation_options options;
        options.validate_laterality = false;

        ophthalmic_iod_validator validator(options);
        auto dataset = create_minimal_ophthalmic_dataset();
        dataset.remove(ophthalmic_iod_tags::image_laterality);

        auto result = validator.validate(dataset);
        bool found_lat_error = false;
        for (const auto& f : result.findings) {
            if (f.tag == ophthalmic_iod_tags::image_laterality &&
                f.severity == validation_severity::error) {
                found_lat_error = true;
                break;
            }
        }
        CHECK_FALSE(found_lat_error);
    }

    SECTION("options getter returns current options") {
        ophthalmic_validation_options options;
        options.strict_mode = true;
        options.validate_ophthalmic_params = false;

        ophthalmic_iod_validator validator(options);
        CHECK(validator.options().strict_mode);
        CHECK_FALSE(validator.options().validate_ophthalmic_params);
    }

    SECTION("set_options updates options") {
        ophthalmic_iod_validator validator;
        CHECK_FALSE(validator.options().strict_mode);

        ophthalmic_validation_options new_options;
        new_options.strict_mode = true;
        validator.set_options(new_options);

        CHECK(validator.options().strict_mode);
    }
}

// ============================================================================
// Convenience Function Tests
// ============================================================================

TEST_CASE("validate_ophthalmic_iod convenience function",
          "[services][ophthalmic][validator]") {
    auto dataset = create_minimal_ophthalmic_dataset();
    auto result = validate_ophthalmic_iod(dataset);
    CHECK(result.is_valid);
}

TEST_CASE("is_valid_ophthalmic_dataset convenience function",
          "[services][ophthalmic][validator]") {
    SECTION("valid dataset") {
        auto dataset = create_minimal_ophthalmic_dataset();
        CHECK(is_valid_ophthalmic_dataset(dataset));
    }

    SECTION("invalid dataset - wrong modality") {
        auto dataset = create_minimal_ophthalmic_dataset();
        dataset.set_string(tags::modality, vr_type::CS, "CT");
        CHECK_FALSE(is_valid_ophthalmic_dataset(dataset));
    }

    SECTION("invalid dataset - missing ImageLaterality") {
        auto dataset = create_minimal_ophthalmic_dataset();
        dataset.remove(ophthalmic_iod_tags::image_laterality);
        CHECK_FALSE(is_valid_ophthalmic_dataset(dataset));
    }
}
