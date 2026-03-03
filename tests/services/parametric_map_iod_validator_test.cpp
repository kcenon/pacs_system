/**
 * @file parametric_map_iod_validator_test.cpp
 * @brief Unit tests for Parametric Map IOD Validator
 *
 * @see DICOM PS3.3 Section A.75 - Parametric Map IOD
 * @see Issue #834 - Add Parametric Map IOD validator
 */

#include <pacs/services/validation/parametric_map_iod_validator.hpp>
#include <pacs/services/sop_classes/parametric_map_storage.hpp>
#include <pacs/core/dicom_dataset.hpp>
#include <pacs/core/dicom_element.hpp>
#include <pacs/core/dicom_tag_constants.hpp>
#include <pacs/encoding/vr_type.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace pacs::services::validation;
using namespace pacs::services::sop_classes;
using namespace pacs::core;
using namespace pacs::encoding;

// ============================================================================
// Test Fixtures - Helper Functions
// ============================================================================

namespace {

/// Helper to create a minimal valid parametric map dataset (float32 ADC map)
dicom_dataset create_minimal_parametric_map_dataset() {
    dicom_dataset ds;

    // Patient Module (Type 2)
    ds.set_string(tags::patient_name, vr_type::PN, "Test^Patient");
    ds.set_string(tags::patient_id, vr_type::LO, "PMAP12345");
    ds.set_string(tags::patient_birth_date, vr_type::DA, "19700101");
    ds.set_string(tags::patient_sex, vr_type::CS, "M");

    // General Study Module
    ds.set_string(tags::study_instance_uid, vr_type::UI,
                  "1.2.3.4.5.6.7.8.9");
    ds.set_string(tags::study_date, vr_type::DA, "20240601");
    ds.set_string(tags::study_time, vr_type::TM, "093000");
    ds.set_string(tags::referring_physician_name, vr_type::PN,
                  "Dr^Radiologist");
    ds.set_string(tags::study_id, vr_type::SH, "STUDY001");
    ds.set_string(tags::accession_number, vr_type::SH, "ACC001");

    // General Series Module — Modality = "RWV"
    ds.set_string(tags::modality, vr_type::CS, "RWV");
    ds.set_string(tags::series_instance_uid, vr_type::UI,
                  "1.2.3.4.5.6.7.8.9.1");
    ds.set_string(tags::series_number, vr_type::IS, "100");

    // Enhanced General Equipment Module
    ds.set_string(pmap_iod_tags::manufacturer, vr_type::LO, "TestVendor");
    ds.set_string(pmap_iod_tags::manufacturer_model_name, vr_type::LO,
                  "PMAP-Generator");
    ds.set_string(pmap_iod_tags::device_serial_number, vr_type::LO,
                  "SN12345");
    ds.set_string(pmap_iod_tags::software_versions, vr_type::LO, "1.0.0");

    // Image Pixel Module — float32 MONOCHROME2
    ds.set_numeric<uint16_t>(tags::samples_per_pixel, vr_type::US, 1);
    ds.set_string(tags::photometric_interpretation, vr_type::CS,
                  "MONOCHROME2");
    ds.set_numeric<uint16_t>(tags::rows, vr_type::US, 256);
    ds.set_numeric<uint16_t>(tags::columns, vr_type::US, 256);
    ds.set_numeric<uint16_t>(tags::bits_allocated, vr_type::US, 32);
    ds.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, 32);
    ds.set_numeric<uint16_t>(tags::high_bit, vr_type::US, 31);
    ds.set_numeric<uint16_t>(tags::pixel_representation, vr_type::US, 0);

    // Multi-frame (always required for Parametric Map)
    ds.set_string(pmap_iod_tags::number_of_frames, vr_type::IS, "10");
    ds.set_string(pmap_iod_tags::shared_functional_groups_sequence,
                  vr_type::SQ, "");
    ds.set_string(pmap_iod_tags::per_frame_functional_groups_sequence,
                  vr_type::SQ, "");
    ds.set_string(pmap_iod_tags::dimension_organization_sequence,
                  vr_type::SQ, "");
    ds.set_string(pmap_iod_tags::dimension_index_sequence,
                  vr_type::SQ, "");

    // Parametric Map Image Module
    ds.set_string(pmap_iod_tags::content_label, vr_type::CS, "ADC_MAP");
    ds.set_string(pmap_iod_tags::content_description, vr_type::LO,
                  "Apparent Diffusion Coefficient Map");
    ds.set_string(pmap_iod_tags::content_creator_name, vr_type::PN,
                  "AutoProcessor");
    ds.set_string(pmap_iod_tags::real_world_value_mapping_sequence,
                  vr_type::SQ, "");

    // SOP Common Module
    ds.set_string(tags::sop_class_uid, vr_type::UI,
                  std::string(parametric_map_storage_uid));
    ds.set_string(tags::sop_instance_uid, vr_type::UI,
                  "1.2.3.4.5.6.7.8.9.2");

    return ds;
}

/// Helper to create a float64 parametric map dataset
dicom_dataset create_float64_parametric_map_dataset() {
    auto ds = create_minimal_parametric_map_dataset();

    // Override for float64
    ds.set_numeric<uint16_t>(tags::bits_allocated, vr_type::US, 64);
    ds.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, 64);
    ds.set_numeric<uint16_t>(tags::high_bit, vr_type::US, 63);

    return ds;
}

}  // namespace

// ============================================================================
// Parametric Map IOD Validator Basic Tests
// ============================================================================

TEST_CASE("parametric_map_iod_validator validates minimal valid dataset",
          "[services][parametric_map][validator]") {
    parametric_map_iod_validator validator;
    auto dataset = create_minimal_parametric_map_dataset();

    auto result = validator.validate(dataset);

    CHECK(result.is_valid);
    CHECK_FALSE(result.has_errors());
}

TEST_CASE("parametric_map_iod_validator validates float64 dataset",
          "[services][parametric_map][validator]") {
    parametric_map_iod_validator validator;
    auto dataset = create_float64_parametric_map_dataset();

    auto result = validator.validate(dataset);

    CHECK(result.is_valid);
    CHECK_FALSE(result.has_errors());
}

// ============================================================================
// Type 1 Attribute Tests
// ============================================================================

TEST_CASE("parametric_map_iod_validator detects missing Type 1 attributes",
          "[services][parametric_map][validator]") {
    parametric_map_iod_validator validator;
    auto dataset = create_minimal_parametric_map_dataset();

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

    SECTION("missing ContentLabel") {
        dataset.remove(pmap_iod_tags::content_label);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }

    SECTION("missing Manufacturer") {
        dataset.remove(pmap_iod_tags::manufacturer);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }
}

// ============================================================================
// Type 2 Attribute Tests
// ============================================================================

TEST_CASE("parametric_map_iod_validator detects missing Type 2 attributes",
          "[services][parametric_map][validator]") {
    parametric_map_iod_validator validator;
    auto dataset = create_minimal_parametric_map_dataset();

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

    SECTION("missing ContentDescription generates warning") {
        dataset.remove(pmap_iod_tags::content_description);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
        CHECK(result.has_warnings());
    }

    SECTION("missing ContentCreatorName generates warning") {
        dataset.remove(pmap_iod_tags::content_creator_name);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
        CHECK(result.has_warnings());
    }
}

// ============================================================================
// Modality Validation Tests
// ============================================================================

TEST_CASE("parametric_map_iod_validator checks modality value",
          "[services][parametric_map][validator]") {
    parametric_map_iod_validator validator;
    auto dataset = create_minimal_parametric_map_dataset();

    SECTION("RWV modality is valid") {
        dataset.set_string(tags::modality, vr_type::CS, "RWV");
        auto result = validator.validate(dataset);
        bool found_modality_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "PMAP-ERR-003") {
                found_modality_error = true;
                break;
            }
        }
        CHECK_FALSE(found_modality_error);
    }

    SECTION("PMAP modality is valid") {
        dataset.set_string(tags::modality, vr_type::CS, "PMAP");
        auto result = validator.validate(dataset);
        bool found_modality_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "PMAP-ERR-003") {
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
            if (f.code == "PMAP-ERR-003") {
                found_modality_error = true;
                break;
            }
        }
        CHECK(found_modality_error);
    }

    SECTION("wrong modality - SEG") {
        dataset.set_string(tags::modality, vr_type::CS, "SEG");
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }
}

// ============================================================================
// SOP Class UID Tests
// ============================================================================

TEST_CASE("parametric_map_iod_validator checks SOP Class UID",
          "[services][parametric_map][validator]") {
    parametric_map_iod_validator validator;
    auto dataset = create_minimal_parametric_map_dataset();

    SECTION("Parametric Map Storage SOP Class is valid") {
        dataset.set_string(tags::sop_class_uid, vr_type::UI,
                          std::string(parametric_map_storage_uid));
        auto result = validator.validate(dataset);
        bool found_sop_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "PMAP-ERR-002") {
                found_sop_error = true;
                break;
            }
        }
        CHECK_FALSE(found_sop_error);
    }

    SECTION("non-parametric-map SOP Class is invalid") {
        // CT SOP Class
        dataset.set_string(tags::sop_class_uid, vr_type::UI,
                          "1.2.840.10008.5.1.4.1.1.2");
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);

        bool found_sop_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "PMAP-ERR-002") {
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

TEST_CASE("parametric_map_iod_validator checks pixel data consistency",
          "[services][parametric_map][validator]") {
    parametric_map_iod_validator validator;
    auto dataset = create_minimal_parametric_map_dataset();

    SECTION("BitsStored exceeds BitsAllocated") {
        dataset.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, 64);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);

        bool found_bits_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "PMAP-ERR-005") {
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
            if (f.code == "PMAP-WARN-002") {
                found_highbit_warning = true;
                break;
            }
        }
        CHECK(found_highbit_warning);
    }

    SECTION("invalid PhotometricInterpretation - RGB") {
        dataset.set_string(tags::photometric_interpretation, vr_type::CS,
                          "RGB");
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);

        bool found_photometric_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "PMAP-ERR-006") {
                found_photometric_error = true;
                break;
            }
        }
        CHECK(found_photometric_error);
    }

    SECTION("invalid PhotometricInterpretation - MONOCHROME1") {
        dataset.set_string(tags::photometric_interpretation, vr_type::CS,
                          "MONOCHROME1");
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);

        bool found_photometric_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "PMAP-ERR-006") {
                found_photometric_error = true;
                break;
            }
        }
        CHECK(found_photometric_error);
    }

    SECTION("invalid SamplesPerPixel (not 1)") {
        dataset.set_numeric<uint16_t>(tags::samples_per_pixel, vr_type::US, 3);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);

        bool found_samples_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "PMAP-ERR-007") {
                found_samples_error = true;
                break;
            }
        }
        CHECK(found_samples_error);
    }

    SECTION("invalid BitsAllocated - 8") {
        dataset.set_numeric<uint16_t>(tags::bits_allocated, vr_type::US, 8);
        dataset.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, 8);
        dataset.set_numeric<uint16_t>(tags::high_bit, vr_type::US, 7);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);

        bool found_bits_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "PMAP-ERR-008") {
                found_bits_error = true;
                break;
            }
        }
        CHECK(found_bits_error);
    }

    SECTION("invalid BitsAllocated - 16") {
        dataset.set_numeric<uint16_t>(tags::bits_allocated, vr_type::US, 16);
        dataset.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, 16);
        dataset.set_numeric<uint16_t>(tags::high_bit, vr_type::US, 15);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);

        bool found_bits_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "PMAP-ERR-008") {
                found_bits_error = true;
                break;
            }
        }
        CHECK(found_bits_error);
    }

    SECTION("valid BitsAllocated - 32") {
        dataset.set_numeric<uint16_t>(tags::bits_allocated, vr_type::US, 32);
        auto result = validator.validate(dataset);
        bool found_bits_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "PMAP-ERR-008") {
                found_bits_error = true;
                break;
            }
        }
        CHECK_FALSE(found_bits_error);
    }

    SECTION("valid BitsAllocated - 64") {
        dataset.set_numeric<uint16_t>(tags::bits_allocated, vr_type::US, 64);
        dataset.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, 64);
        dataset.set_numeric<uint16_t>(tags::high_bit, vr_type::US, 63);
        auto result = validator.validate(dataset);
        bool found_bits_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "PMAP-ERR-008") {
                found_bits_error = true;
                break;
            }
        }
        CHECK_FALSE(found_bits_error);
    }
}

// ============================================================================
// Multi-frame Tests
// ============================================================================

TEST_CASE("parametric_map_iod_validator checks multi-frame requirements",
          "[services][parametric_map][validator]") {
    parametric_map_iod_validator validator;

    SECTION("missing NumberOfFrames generates error") {
        auto dataset = create_minimal_parametric_map_dataset();
        dataset.remove(pmap_iod_tags::number_of_frames);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);

        bool found_frames_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "PMAP-ERR-001") {
                found_frames_error = true;
                break;
            }
        }
        CHECK(found_frames_error);
    }

    SECTION("present NumberOfFrames is valid") {
        auto dataset = create_minimal_parametric_map_dataset();
        auto result = validator.validate(dataset);
        bool found_frames_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "PMAP-ERR-001") {
                found_frames_error = true;
                break;
            }
        }
        CHECK_FALSE(found_frames_error);
    }

    SECTION("missing SharedFunctionalGroupsSequence generates warning") {
        auto dataset = create_minimal_parametric_map_dataset();
        dataset.remove(pmap_iod_tags::shared_functional_groups_sequence);
        auto result = validator.validate(dataset);

        bool found_warning = false;
        for (const auto& f : result.findings) {
            if (f.code == "PMAP-WARN-001") {
                found_warning = true;
                break;
            }
        }
        CHECK(found_warning);
    }

    SECTION("missing PerFrameFunctionalGroupsSequence generates warning") {
        auto dataset = create_minimal_parametric_map_dataset();
        dataset.remove(pmap_iod_tags::per_frame_functional_groups_sequence);
        auto result = validator.validate(dataset);

        bool found_warning = false;
        for (const auto& f : result.findings) {
            if (f.code == "PMAP-WARN-004") {
                found_warning = true;
                break;
            }
        }
        CHECK(found_warning);
    }
}

// ============================================================================
// Real World Value Mapping Tests
// ============================================================================

TEST_CASE("parametric_map_iod_validator checks RWVM sequence",
          "[services][parametric_map][validator]") {
    parametric_map_iod_validator validator;
    auto dataset = create_minimal_parametric_map_dataset();

    SECTION("missing RWVM sequence generates error") {
        dataset.remove(pmap_iod_tags::real_world_value_mapping_sequence);
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);

        bool found_rwvm_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "PMAP-ERR-004") {
                found_rwvm_error = true;
                break;
            }
        }
        CHECK(found_rwvm_error);
    }

    SECTION("present RWVM sequence is valid") {
        auto result = validator.validate(dataset);
        bool found_rwvm_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "PMAP-ERR-004") {
                found_rwvm_error = true;
                break;
            }
        }
        CHECK_FALSE(found_rwvm_error);
    }
}

// ============================================================================
// Referenced Series Tests
// ============================================================================

TEST_CASE("parametric_map_iod_validator checks referenced series",
          "[services][parametric_map][validator]") {
    parametric_map_iod_validator validator;
    auto dataset = create_minimal_parametric_map_dataset();

    SECTION("missing ReferencedSeriesSequence generates warning") {
        auto result = validator.validate(dataset);

        bool found_ref_warning = false;
        for (const auto& f : result.findings) {
            if (f.code == "PMAP-WARN-003") {
                found_ref_warning = true;
                break;
            }
        }
        CHECK(found_ref_warning);
    }

    SECTION("present ReferencedSeriesSequence suppresses warning") {
        dataset.set_string(pmap_iod_tags::referenced_series_sequence,
                          vr_type::SQ, "");
        auto result = validator.validate(dataset);

        bool found_ref_warning = false;
        for (const auto& f : result.findings) {
            if (f.code == "PMAP-WARN-003") {
                found_ref_warning = true;
                break;
            }
        }
        CHECK_FALSE(found_ref_warning);
    }
}

// ============================================================================
// Quick Check Tests
// ============================================================================

TEST_CASE("parametric_map_iod_validator quick_check works correctly",
          "[services][parametric_map][validator]") {
    parametric_map_iod_validator validator;

    SECTION("valid dataset passes quick check") {
        auto dataset = create_minimal_parametric_map_dataset();
        CHECK(validator.quick_check(dataset));
    }

    SECTION("valid float64 dataset passes quick check") {
        auto dataset = create_float64_parametric_map_dataset();
        CHECK(validator.quick_check(dataset));
    }

    SECTION("invalid modality fails quick check") {
        auto dataset = create_minimal_parametric_map_dataset();
        dataset.set_string(tags::modality, vr_type::CS, "CT");
        CHECK_FALSE(validator.quick_check(dataset));
    }

    SECTION("missing required attribute fails quick check") {
        auto dataset = create_minimal_parametric_map_dataset();
        dataset.remove(tags::rows);
        CHECK_FALSE(validator.quick_check(dataset));
    }

    SECTION("missing ContentLabel fails quick check") {
        auto dataset = create_minimal_parametric_map_dataset();
        dataset.remove(pmap_iod_tags::content_label);
        CHECK_FALSE(validator.quick_check(dataset));
    }

    SECTION("missing RWVM sequence fails quick check") {
        auto dataset = create_minimal_parametric_map_dataset();
        dataset.remove(pmap_iod_tags::real_world_value_mapping_sequence);
        CHECK_FALSE(validator.quick_check(dataset));
    }

    SECTION("missing NumberOfFrames fails quick check") {
        auto dataset = create_minimal_parametric_map_dataset();
        dataset.remove(pmap_iod_tags::number_of_frames);
        CHECK_FALSE(validator.quick_check(dataset));
    }

    SECTION("missing SOPClassUID fails quick check") {
        auto dataset = create_minimal_parametric_map_dataset();
        dataset.remove(tags::sop_class_uid);
        CHECK_FALSE(validator.quick_check(dataset));
    }

    SECTION("non-parametric-map SOP Class fails quick check") {
        auto dataset = create_minimal_parametric_map_dataset();
        dataset.set_string(tags::sop_class_uid, vr_type::UI,
                          "1.2.840.10008.5.1.4.1.1.2");
        CHECK_FALSE(validator.quick_check(dataset));
    }

    SECTION("PMAP modality also passes quick check") {
        auto dataset = create_minimal_parametric_map_dataset();
        dataset.set_string(tags::modality, vr_type::CS, "PMAP");
        CHECK(validator.quick_check(dataset));
    }
}

// ============================================================================
// Custom Options Tests
// ============================================================================

TEST_CASE("parametric_map_iod_validator with custom options",
          "[services][parametric_map][validator]") {

    SECTION("strict mode treats warnings as errors") {
        pmap_validation_options options;
        options.strict_mode = true;

        parametric_map_iod_validator validator(options);
        auto dataset = create_minimal_parametric_map_dataset();

        // Remove a Type 2 attribute to get a warning
        dataset.remove(tags::patient_name);

        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }

    SECTION("can disable pixel data validation") {
        pmap_validation_options options;
        options.validate_pixel_data = false;

        parametric_map_iod_validator validator(options);
        auto dataset = create_minimal_parametric_map_dataset();
        // Set invalid BitsAllocated — should be ignored
        dataset.set_numeric<uint16_t>(tags::bits_allocated, vr_type::US, 8);
        dataset.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, 8);
        dataset.set_numeric<uint16_t>(tags::high_bit, vr_type::US, 7);

        auto result = validator.validate(dataset);
        bool found_pixel_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "PMAP-ERR-008") {
                found_pixel_error = true;
                break;
            }
        }
        CHECK_FALSE(found_pixel_error);
    }

    SECTION("can disable RWVM validation") {
        pmap_validation_options options;
        options.validate_rwvm = false;

        parametric_map_iod_validator validator(options);
        auto dataset = create_minimal_parametric_map_dataset();
        dataset.remove(pmap_iod_tags::real_world_value_mapping_sequence);

        auto result = validator.validate(dataset);
        bool found_rwvm_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "PMAP-ERR-004") {
                found_rwvm_error = true;
                break;
            }
        }
        CHECK_FALSE(found_rwvm_error);
    }

    SECTION("can disable multi-frame validation") {
        pmap_validation_options options;
        options.validate_multiframe = false;

        parametric_map_iod_validator validator(options);
        auto dataset = create_minimal_parametric_map_dataset();
        dataset.remove(pmap_iod_tags::number_of_frames);

        auto result = validator.validate(dataset);
        bool found_frames_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "PMAP-ERR-001") {
                found_frames_error = true;
                break;
            }
        }
        CHECK_FALSE(found_frames_error);
    }

    SECTION("can disable reference validation") {
        pmap_validation_options options;
        options.validate_references = false;

        parametric_map_iod_validator validator(options);
        auto dataset = create_minimal_parametric_map_dataset();

        auto result = validator.validate(dataset);
        bool found_ref_warning = false;
        for (const auto& f : result.findings) {
            if (f.code == "PMAP-WARN-003") {
                found_ref_warning = true;
                break;
            }
        }
        CHECK_FALSE(found_ref_warning);
    }

    SECTION("options getter returns current options") {
        pmap_validation_options options;
        options.strict_mode = true;
        options.validate_rwvm = false;

        parametric_map_iod_validator validator(options);
        CHECK(validator.options().strict_mode);
        CHECK_FALSE(validator.options().validate_rwvm);
    }

    SECTION("set_options updates options") {
        parametric_map_iod_validator validator;
        CHECK_FALSE(validator.options().strict_mode);

        pmap_validation_options new_options;
        new_options.strict_mode = true;
        validator.set_options(new_options);

        CHECK(validator.options().strict_mode);
    }
}

// ============================================================================
// Convenience Function Tests
// ============================================================================

TEST_CASE("validate_parametric_map_iod convenience function",
          "[services][parametric_map][validator]") {
    auto dataset = create_minimal_parametric_map_dataset();
    auto result = validate_parametric_map_iod(dataset);
    CHECK(result.is_valid);
}

TEST_CASE("is_valid_parametric_map_dataset convenience function",
          "[services][parametric_map][validator]") {
    SECTION("valid dataset") {
        auto dataset = create_minimal_parametric_map_dataset();
        CHECK(is_valid_parametric_map_dataset(dataset));
    }

    SECTION("invalid dataset - wrong modality") {
        auto dataset = create_minimal_parametric_map_dataset();
        dataset.set_string(tags::modality, vr_type::CS, "CT");
        CHECK_FALSE(is_valid_parametric_map_dataset(dataset));
    }

    SECTION("invalid dataset - missing RWVM") {
        auto dataset = create_minimal_parametric_map_dataset();
        dataset.remove(pmap_iod_tags::real_world_value_mapping_sequence);
        CHECK_FALSE(is_valid_parametric_map_dataset(dataset));
    }
}
