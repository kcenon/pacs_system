/**
 * @file nm_iod_validator_test.cpp
 * @brief Unit tests for Nuclear Medicine (NM) IOD Validator
 */

#include <pacs/services/validation/nm_iod_validator.hpp>
#include <pacs/services/sop_classes/nm_storage.hpp>
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

// General DICOM tags
[[maybe_unused]] constexpr dicom_tag tag_image_type{0x0008, 0x0008};
[[maybe_unused]] constexpr dicom_tag tag_frame_of_reference_uid{0x0020, 0x0052};
[[maybe_unused]] constexpr dicom_tag tag_position_reference_indicator{0x0020, 0x1040};

// NM Image Module tags
[[maybe_unused]] constexpr dicom_tag tag_number_of_frames{0x0028, 0x0008};
[[maybe_unused]] constexpr dicom_tag tag_type_of_data{0x0054, 0x0400};
[[maybe_unused]] constexpr dicom_tag tag_image_index{0x0054, 0x1330};

// NM Series Module tags
[[maybe_unused]] constexpr dicom_tag tag_patient_orientation_code_seq{0x0054, 0x0410};

// Energy Window tags
[[maybe_unused]] constexpr dicom_tag tag_energy_window_info_seq{0x0054, 0x0012};
[[maybe_unused]] constexpr dicom_tag tag_energy_window_range_seq{0x0054, 0x0013};
[[maybe_unused]] constexpr dicom_tag tag_energy_window_lower{0x0054, 0x0014};
[[maybe_unused]] constexpr dicom_tag tag_energy_window_upper{0x0054, 0x0015};
[[maybe_unused]] constexpr dicom_tag tag_energy_window_name{0x0054, 0x0018};

// Radiopharmaceutical tags
[[maybe_unused]] constexpr dicom_tag tag_radiopharmaceutical_info_seq{0x0054, 0x0016};
[[maybe_unused]] constexpr dicom_tag tag_radionuclide_code_seq{0x0054, 0x0300};
[[maybe_unused]] constexpr dicom_tag tag_radiopharmaceutical_start_time{0x0018, 0x1072};
[[maybe_unused]] constexpr dicom_tag tag_radionuclide_total_dose{0x0018, 0x1074};
[[maybe_unused]] constexpr dicom_tag tag_radionuclide_half_life{0x0018, 0x1075};

// Detector Module tags
[[maybe_unused]] constexpr dicom_tag tag_detector_info_seq{0x0054, 0x0022};
[[maybe_unused]] constexpr dicom_tag tag_collimator_type{0x0018, 0x1181};
[[maybe_unused]] constexpr dicom_tag tag_collimator_grid_name{0x0018, 0x1180};
[[maybe_unused]] constexpr dicom_tag tag_field_of_view_shape{0x0018, 0x1147};
[[maybe_unused]] constexpr dicom_tag tag_field_of_view_dimension{0x0018, 0x1149};
[[maybe_unused]] constexpr dicom_tag tag_focal_distance{0x0018, 0x1182};
[[maybe_unused]] constexpr dicom_tag tag_zoom_factor{0x0018, 0x1114};

// TOMO Acquisition tags
[[maybe_unused]] constexpr dicom_tag tag_rotation_info_seq{0x0054, 0x0052};
[[maybe_unused]] constexpr dicom_tag tag_rotation_direction{0x0018, 0x1140};
[[maybe_unused]] constexpr dicom_tag tag_start_angle{0x0054, 0x0200};
[[maybe_unused]] constexpr dicom_tag tag_angular_step{0x0018, 0x1144};
[[maybe_unused]] constexpr dicom_tag tag_number_of_frames_in_rotation{0x0054, 0x0053};

// Gated Acquisition tags
[[maybe_unused]] constexpr dicom_tag tag_gated_info_seq{0x0054, 0x0062};
[[maybe_unused]] constexpr dicom_tag tag_trigger_time{0x0018, 0x1060};
[[maybe_unused]] constexpr dicom_tag tag_cardiac_framing_type{0x0018, 0x1064};
[[maybe_unused]] constexpr dicom_tag tag_rr_interval{0x0018, 0x1062};

// NM Image tags (tag_image_index defined above)
[[maybe_unused]] constexpr dicom_tag tag_counts_accumulated{0x0018, 0x0070};
[[maybe_unused]] constexpr dicom_tag tag_acquisition_start_condition{0x0018, 0x0073};
[[maybe_unused]] constexpr dicom_tag tag_acquisition_termination_condition{0x0018, 0x0071};
[[maybe_unused]] constexpr dicom_tag tag_actual_frame_duration{0x0018, 0x1242};

// Pixel representation tags
[[maybe_unused]] constexpr dicom_tag tag_pixel_spacing{0x0028, 0x0030};
[[maybe_unused]] constexpr dicom_tag tag_slice_thickness{0x0018, 0x0050};

// Helper to check if validation result has info-level findings
[[maybe_unused]] [[nodiscard]] bool has_info_findings(const validation_result& result) noexcept {
    return std::any_of(result.findings.begin(), result.findings.end(),
                       [](const validation_finding& f) {
                           return f.severity == validation_severity::info;
                       });
}

// Helper to create a minimal valid NM dataset
dicom_dataset create_minimal_nm_dataset() {
    dicom_dataset ds;

    // Patient Module (Type 2)
    ds.set_string(tags::patient_name, vr_type::PN, "Test^Patient");
    ds.set_string(tags::patient_id, vr_type::LO, "12345");
    ds.set_string(tags::patient_birth_date, vr_type::DA, "19550101");
    ds.set_string(tags::patient_sex, vr_type::CS, "M");

    // General Study Module
    ds.set_string(tags::study_instance_uid, vr_type::UI, "1.2.3.4.5.6.7.8.9");
    ds.set_string(tags::study_date, vr_type::DA, "20240101");
    ds.set_string(tags::study_time, vr_type::TM, "100000");
    ds.set_string(tags::referring_physician_name, vr_type::PN, "Dr^Referring");
    ds.set_string(tags::study_id, vr_type::SH, "STUDY001");
    ds.set_string(tags::accession_number, vr_type::SH, "ACC001");

    // General Series Module
    ds.set_string(tags::modality, vr_type::CS, "NM");
    ds.set_string(tags::series_instance_uid, vr_type::UI, "1.2.3.4.5.6.7.8.9.1");
    ds.set_string(tags::series_number, vr_type::IS, "1");

    // NM Series Module
    ds.set_string(tag_type_of_data, vr_type::CS, "STATIC");  // Required Type 1

    // Image Pixel Module
    ds.set_numeric<uint16_t>(tags::samples_per_pixel, vr_type::US, 1);
    ds.set_string(tags::photometric_interpretation, vr_type::CS, "MONOCHROME2");
    ds.set_numeric<uint16_t>(tags::rows, vr_type::US, 64);       // Typical NM matrix
    ds.set_numeric<uint16_t>(tags::columns, vr_type::US, 64);
    ds.set_numeric<uint16_t>(tags::bits_allocated, vr_type::US, 16);
    ds.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, 16);
    ds.set_numeric<uint16_t>(tags::high_bit, vr_type::US, 15);
    ds.set_numeric<uint16_t>(tags::pixel_representation, vr_type::US, 0);

    // Pixel Data (minimal placeholder)
    std::vector<uint8_t> pixel_data(100, 0);
    ds.insert(dicom_element(tags::pixel_data, vr_type::OW, pixel_data));

    // NM Image Module
    ds.set_string(tag_image_type, vr_type::CS, "ORIGINAL\\PRIMARY\\STATIC");
    ds.set_numeric<uint32_t>(tag_image_index, vr_type::UL, 1);
    ds.set_numeric<uint32_t>(tag_actual_frame_duration, vr_type::IS, 600000);  // 10 min
    ds.set_numeric<uint32_t>(tag_counts_accumulated, vr_type::IS, 1000000);

    // Multiframe info (NM images are typically multiframe)
    ds.set_numeric<uint32_t>(tag_number_of_frames, vr_type::IS, 1);

    // Detector info
    ds.set_string(tag_collimator_type, vr_type::CS, "PARA");  // Parallel collimator
    ds.set_string(tag_field_of_view_shape, vr_type::CS, "RECTANGLE");
    ds.set_string(tag_field_of_view_dimension, vr_type::IS, "400\\400");  // mm
    ds.set_numeric<double>(tag_zoom_factor, vr_type::DS, 1.0);

    // Pixel spacing
    ds.set_string(tag_pixel_spacing, vr_type::DS, "6.4\\6.4");  // Typical NM spacing

    // SOP Common Module
    ds.set_string(tags::sop_class_uid, vr_type::UI, std::string(nm_image_storage_uid));
    ds.set_string(tags::sop_instance_uid, vr_type::UI, "1.2.3.4.5.6.7.8.9.2");

    return ds;
}

// Helper to create a TOMO (SPECT) dataset
dicom_dataset create_tomo_nm_dataset() {
    auto ds = create_minimal_nm_dataset();

    // Change image type to TOMO
    ds.set_string(tag_image_type, vr_type::CS, "ORIGINAL\\PRIMARY\\TOMO");

    // Add rotation info
    ds.set_string(tag_rotation_direction, vr_type::CS, "CW");
    ds.set_numeric<double>(tag_start_angle, vr_type::DS, 0.0);
    ds.set_numeric<double>(tag_angular_step, vr_type::DS, 6.0);  // 6 degrees per step
    ds.set_numeric<uint32_t>(tag_number_of_frames_in_rotation, vr_type::UL, 60);

    // Multiple frames
    ds.set_numeric<uint32_t>(tag_number_of_frames, vr_type::IS, 60);

    return ds;
}

// Helper to create a gated dataset
dicom_dataset create_gated_nm_dataset() {
    auto ds = create_minimal_nm_dataset();

    // Change image type to GATED
    ds.set_string(tag_image_type, vr_type::CS, "ORIGINAL\\PRIMARY\\GATED");

    // Add gating info
    ds.set_string(tag_cardiac_framing_type, vr_type::CS, "FORWARD");
    ds.set_numeric<double>(tag_trigger_time, vr_type::DS, 0.0);
    ds.set_numeric<double>(tag_rr_interval, vr_type::DS, 800.0);  // ms

    // Multiple frames (typically 8-16 phases)
    ds.set_numeric<uint32_t>(tag_number_of_frames, vr_type::IS, 8);

    return ds;
}

}  // namespace

// ============================================================================
// NM IOD Validator Basic Tests
// ============================================================================

TEST_CASE("nm_iod_validator validates minimal valid dataset", "[services][nm][validator]") {
    nm_iod_validator validator;
    auto dataset = create_minimal_nm_dataset();

    auto result = validator.validate(dataset);

    CHECK(result.is_valid);
    CHECK_FALSE(result.has_errors());
}

TEST_CASE("nm_iod_validator detects missing Type 1 attributes", "[services][nm][validator]") {
    nm_iod_validator validator;
    auto dataset = create_minimal_nm_dataset();

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

TEST_CASE("nm_iod_validator checks modality value", "[services][nm][validator]") {
    nm_iod_validator validator;
    auto dataset = create_minimal_nm_dataset();

    SECTION("correct modality - NM") {
        dataset.set_string(tags::modality, vr_type::CS, "NM");
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
    }

    SECTION("wrong modality - PT") {
        dataset.set_string(tags::modality, vr_type::CS, "PT");
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);

        bool found_modality_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "NM-ERR-002") {
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
// NM Image Module Tests
// ============================================================================

TEST_CASE("nm_iod_validator validates NM Image Module", "[services][nm][validator][image]") {
    nm_iod_validator validator;
    auto dataset = create_minimal_nm_dataset();

    SECTION("valid image types") {
        for (const auto& type : {"ORIGINAL\\PRIMARY\\STATIC",
                                  "ORIGINAL\\PRIMARY\\DYNAMIC",
                                  "ORIGINAL\\PRIMARY\\TOMO",
                                  "ORIGINAL\\PRIMARY\\GATED"}) {
            dataset.set_string(tag_image_type, vr_type::CS, type);
            auto result = validator.validate(dataset);
            CHECK(result.is_valid);
        }
    }

    SECTION("missing image type generates warning") {
        dataset.remove(tag_image_type);
        auto result = validator.validate(dataset);
        CHECK(result.has_warnings());
    }

    SECTION("valid counts accumulated") {
        dataset.set_numeric<uint32_t>(tag_counts_accumulated, vr_type::IS, 1000000);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
    }

    SECTION("missing frame duration is acceptable") {
        dataset.remove(tag_actual_frame_duration);
        auto result = validator.validate(dataset);
        // Frame duration is not a required attribute
        CHECK(result.is_valid);
    }
}

// ============================================================================
// Detector Module Tests
// ============================================================================

TEST_CASE("nm_iod_validator validates detector module", "[services][nm][validator][detector]") {
    nm_iod_validator validator;
    auto dataset = create_minimal_nm_dataset();

    SECTION("valid collimator types") {
        for (const auto& type : {"PARA", "FANB", "CONE", "PINH", "NONE"}) {
            dataset.set_string(tag_collimator_type, vr_type::CS, type);
            auto result = validator.validate(dataset);
            CHECK(result.is_valid);
        }
    }

    SECTION("invalid collimator type generates warning") {
        dataset.set_string(tag_collimator_type, vr_type::CS, "INVALID");
        auto result = validator.validate(dataset);
        CHECK(result.has_warnings());
    }

    SECTION("valid field of view shapes") {
        for (const auto& shape : {"RECTANGLE", "ROUND", "HEXAGONAL"}) {
            dataset.set_string(tag_field_of_view_shape, vr_type::CS, shape);
            auto result = validator.validate(dataset);
            CHECK(result.is_valid);
        }
    }

    SECTION("valid zoom factor") {
        dataset.set_numeric<double>(tag_zoom_factor, vr_type::DS, 1.5);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
    }

    SECTION("zero zoom factor generates warning") {
        dataset.set_numeric<double>(tag_zoom_factor, vr_type::DS, 0.0);
        auto result = validator.validate(dataset);
        CHECK(result.has_warnings());
    }
}

// ============================================================================
// Photometric Interpretation Tests
// ============================================================================

TEST_CASE("nm_iod_validator checks photometric interpretation", "[services][nm][validator]") {
    nm_iod_validator validator;
    auto dataset = create_minimal_nm_dataset();

    SECTION("MONOCHROME2 is valid") {
        dataset.set_string(tags::photometric_interpretation, vr_type::CS, "MONOCHROME2");
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
    }

    SECTION("PALETTE COLOR is valid for NM") {
        dataset.set_string(tags::photometric_interpretation, vr_type::CS, "PALETTE COLOR");
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
    }

    SECTION("MONOCHROME1 generates warning for NM") {
        dataset.set_string(tags::photometric_interpretation, vr_type::CS, "MONOCHROME1");
        auto result = validator.validate(dataset);
        // NM typically uses MONOCHROME2, MONOCHROME1 is unusual
        CHECK(result.has_warnings());
    }

    SECTION("RGB generates warning for NM") {
        dataset.set_string(tags::photometric_interpretation, vr_type::CS, "RGB");
        auto result = validator.validate(dataset);
        // RGB is unusual for NM (which is typically grayscale)
        CHECK(result.has_warnings());
    }
}

// ============================================================================
// SOP Class UID Tests
// ============================================================================

TEST_CASE("nm_iod_validator checks SOP Class UID", "[services][nm][validator]") {
    nm_iod_validator validator;
    auto dataset = create_minimal_nm_dataset();

    SECTION("valid NM SOP Class") {
        dataset.set_string(tags::sop_class_uid, vr_type::UI,
                          std::string(nm_image_storage_uid));
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
    }

    SECTION("retired NM SOP Class is valid by default") {
        dataset.set_string(tags::sop_class_uid, vr_type::UI,
                          std::string(nm_image_storage_retired_uid));
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);  // Retired classes are allowed by default
    }

    SECTION("non-NM SOP Class") {
        // CT SOP Class
        dataset.set_string(tags::sop_class_uid, vr_type::UI, "1.2.840.10008.5.1.4.1.1.2");
        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);

        // PET SOP Class
        dataset.set_string(tags::sop_class_uid, vr_type::UI, "1.2.840.10008.5.1.4.1.1.128");
        result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);
    }
}

// ============================================================================
// Pixel Data Consistency Tests
// ============================================================================

TEST_CASE("nm_iod_validator checks pixel data consistency", "[services][nm][validator]") {
    nm_iod_validator validator;
    auto dataset = create_minimal_nm_dataset();

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
        // NM images should typically be grayscale (SamplesPerPixel=1)
        // but non-grayscale is a warning, not an error
        CHECK(result.has_warnings());
    }

    SECTION("typical NM matrix size") {
        dataset.set_numeric<uint16_t>(tags::rows, vr_type::US, 64);
        dataset.set_numeric<uint16_t>(tags::columns, vr_type::US, 64);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
    }

    SECTION("larger matrix size is valid") {
        dataset.set_numeric<uint16_t>(tags::rows, vr_type::US, 128);
        dataset.set_numeric<uint16_t>(tags::columns, vr_type::US, 128);
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
    }
}

// ============================================================================
// TOMO (SPECT) Acquisition Tests
// ============================================================================

TEST_CASE("nm_iod_validator validates TOMO acquisition", "[services][nm][validator][tomo]") {
    nm_iod_validator validator;
    auto dataset = create_tomo_nm_dataset();

    SECTION("valid TOMO dataset") {
        auto result = validator.validate_multiframe(dataset);
        CHECK(result.is_valid);
    }

    SECTION("valid rotation directions") {
        for (const auto& dir : {"CW", "CC"}) {
            dataset.set_string(tag_rotation_direction, vr_type::CS, dir);
            auto result = validator.validate_multiframe(dataset);
            CHECK(result.is_valid);
        }
    }

    SECTION("invalid rotation direction generates warning") {
        dataset.set_string(tag_rotation_direction, vr_type::CS, "INVALID");
        auto result = validator.validate_multiframe(dataset);
        CHECK(result.has_warnings());
    }

    SECTION("missing angular step generates warning") {
        dataset.remove(tag_angular_step);
        auto result = validator.validate_multiframe(dataset);
        CHECK(result.has_warnings());
    }

    SECTION("inconsistent frame count generates warning") {
        dataset.set_numeric<uint32_t>(tag_number_of_frames, vr_type::IS, 30);  // Different from rotation
        auto result = validator.validate_multiframe(dataset);
        CHECK(result.has_warnings());
    }
}

// ============================================================================
// Gated Acquisition Tests
// ============================================================================

TEST_CASE("nm_iod_validator validates gated acquisition", "[services][nm][validator][gated]") {
    nm_iod_validator validator;
    auto dataset = create_gated_nm_dataset();

    SECTION("valid gated dataset") {
        auto result = validator.validate_multiframe(dataset);
        CHECK(result.is_valid);
    }

    SECTION("valid cardiac framing types") {
        for (const auto& type : {"FORWARD", "BACKWARD", "BOTH"}) {
            dataset.set_string(tag_cardiac_framing_type, vr_type::CS, type);
            auto result = validator.validate_multiframe(dataset);
            CHECK(result.is_valid);
        }
    }

    SECTION("invalid RR interval generates warning") {
        dataset.set_numeric<double>(tag_rr_interval, vr_type::DS, 0.0);
        auto result = validator.validate_multiframe(dataset);
        CHECK(result.has_warnings());
    }

    SECTION("physiologically unrealistic RR interval generates warning") {
        dataset.set_numeric<double>(tag_rr_interval, vr_type::DS, 100.0);  // Too short
        auto result = validator.validate_multiframe(dataset);
        CHECK(result.has_warnings());

        dataset.set_numeric<double>(tag_rr_interval, vr_type::DS, 3000.0);  // Too long
        result = validator.validate_multiframe(dataset);
        CHECK(result.has_warnings());
    }
}

// ============================================================================
// Multiframe Validation Tests
// ============================================================================

TEST_CASE("nm_iod_validator validates multiframe datasets", "[services][nm][validator][multiframe]") {
    nm_iod_validator validator;
    auto dataset = create_minimal_nm_dataset();

    SECTION("single frame is valid") {
        dataset.set_numeric<uint32_t>(tag_number_of_frames, vr_type::IS, 1);
        auto result = validator.validate_multiframe(dataset);
        CHECK(result.is_valid);
    }

    SECTION("multiple frames") {
        dataset.set_numeric<uint32_t>(tag_number_of_frames, vr_type::IS, 32);
        auto result = validator.validate_multiframe(dataset);
        CHECK(result.is_valid);
    }

    SECTION("zero frames is valid (validator does not check frame count value)") {
        dataset.set_numeric<uint32_t>(tag_number_of_frames, vr_type::IS, 0);
        auto result = validator.validate_multiframe(dataset);
        // Validator only checks presence, not value semantics
        CHECK(result.is_valid);
    }
}

// ============================================================================
// Quick Check Tests
// ============================================================================

TEST_CASE("nm_iod_validator quick_check works correctly", "[services][nm][validator]") {
    nm_iod_validator validator;

    SECTION("valid dataset passes quick check") {
        auto dataset = create_minimal_nm_dataset();
        CHECK(validator.quick_check(dataset));
    }

    SECTION("invalid modality fails quick check") {
        auto dataset = create_minimal_nm_dataset();
        dataset.set_string(tags::modality, vr_type::CS, "PT");
        CHECK_FALSE(validator.quick_check(dataset));
    }

    SECTION("missing required attribute fails quick check") {
        auto dataset = create_minimal_nm_dataset();
        dataset.remove(tags::rows);
        CHECK_FALSE(validator.quick_check(dataset));
    }

    SECTION("TOMO dataset passes quick check") {
        auto dataset = create_tomo_nm_dataset();
        CHECK(validator.quick_check(dataset));
    }

    SECTION("gated dataset passes quick check") {
        auto dataset = create_gated_nm_dataset();
        CHECK(validator.quick_check(dataset));
    }
}

// ============================================================================
// Custom Options Tests
// ============================================================================

TEST_CASE("nm_iod_validator with custom options", "[services][nm][validator]") {
    SECTION("strict mode treats warnings as errors") {
        nm_validation_options options;
        options.strict_mode = true;

        nm_iod_validator validator(options);
        auto dataset = create_minimal_nm_dataset();

        // Remove a Type 2 attribute to get a warning
        dataset.remove(tags::patient_name);

        auto result = validator.validate(dataset);
        CHECK_FALSE(result.is_valid);  // Strict mode makes warnings into errors
    }

    SECTION("can disable pixel data validation") {
        nm_validation_options options;
        options.validate_pixel_data = false;

        nm_iod_validator validator(options);
        auto dataset = create_minimal_nm_dataset();
        dataset.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, 20);  // Invalid normally

        auto result = validator.validate(dataset);
        bool found_pixel_error = false;
        for (const auto& f : result.findings) {
            if (f.code == "NM-ERR-003") {  // Pixel data error code
                found_pixel_error = true;
                break;
            }
        }
        CHECK_FALSE(found_pixel_error);
    }

    SECTION("can disable NM-specific validation") {
        nm_validation_options options;
        options.validate_nm_specific = false;

        nm_iod_validator validator(options);
        auto dataset = create_minimal_nm_dataset();
        dataset.set_string(tag_collimator_type, vr_type::CS, "INVALID");

        auto result = validator.validate(dataset);
        bool found_nm_warning = false;
        for (const auto& f : result.findings) {
            if (f.code == "NM-WARN-006") {  // Collimator type warning code
                found_nm_warning = true;
                break;
            }
        }
        CHECK_FALSE(found_nm_warning);
    }

    SECTION("can disable energy window validation") {
        nm_validation_options options;
        options.validate_energy_windows = false;

        nm_iod_validator validator(options);
        auto dataset = create_minimal_nm_dataset();

        auto result = validator.validate(dataset);
        bool found_energy_info = false;
        for (const auto& f : result.findings) {
            if (f.code.find("NM-INFO-ENERGY") != std::string::npos) {
                found_energy_info = true;
                break;
            }
        }
        CHECK_FALSE(found_energy_info);
    }

    SECTION("can disable isotope validation") {
        nm_validation_options options;
        options.validate_isotope = false;

        nm_iod_validator validator(options);
        auto dataset = create_minimal_nm_dataset();

        auto result = validator.validate(dataset);
        bool found_isotope_info = false;
        for (const auto& f : result.findings) {
            if (f.code.find("NM-INFO-ISOTOPE") != std::string::npos) {
                found_isotope_info = true;
                break;
            }
        }
        CHECK_FALSE(found_isotope_info);
    }

    SECTION("can disable retired attributes (option exists but not enforced)") {
        nm_validation_options options;
        options.allow_retired = false;

        nm_iod_validator validator(options);
        auto dataset = create_minimal_nm_dataset();
        dataset.set_string(tags::sop_class_uid, vr_type::UI,
                          std::string(nm_image_storage_retired_uid));

        auto result = validator.validate(dataset);
        // Note: allow_retired option exists but retired checking is not yet implemented
        // This test documents the current behavior
        CHECK(result.is_valid);
    }
}

// ============================================================================
// Convenience Function Tests
// ============================================================================

TEST_CASE("validate_nm_iod convenience function", "[services][nm][validator]") {
    auto dataset = create_minimal_nm_dataset();
    auto result = validate_nm_iod(dataset);
    CHECK(result.is_valid);
}

TEST_CASE("is_valid_nm_dataset convenience function", "[services][nm][validator]") {
    SECTION("valid dataset") {
        auto dataset = create_minimal_nm_dataset();
        CHECK(is_valid_nm_dataset(dataset));
    }

    SECTION("invalid dataset") {
        auto dataset = create_minimal_nm_dataset();
        dataset.set_string(tags::modality, vr_type::CS, "PT");
        CHECK_FALSE(is_valid_nm_dataset(dataset));
    }
}

// ============================================================================
// Pixel Spacing Tests
// ============================================================================

TEST_CASE("nm_iod_validator validates pixel spacing", "[services][nm][validator][spacing]") {
    nm_iod_validator validator;
    auto dataset = create_minimal_nm_dataset();

    SECTION("valid typical NM pixel spacing") {
        dataset.set_string(tag_pixel_spacing, vr_type::DS, "6.4\\6.4");
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
    }

    SECTION("valid fine pixel spacing") {
        dataset.set_string(tag_pixel_spacing, vr_type::DS, "3.2\\3.2");
        auto result = validator.validate(dataset);
        CHECK(result.is_valid);
    }

    SECTION("asymmetric pixel spacing is valid") {
        dataset.set_string(tag_pixel_spacing, vr_type::DS, "6.4\\3.2");
        auto result = validator.validate(dataset);
        // Asymmetric spacing is acceptable, no info generated
        CHECK(result.is_valid);
    }

    SECTION("missing pixel spacing is valid") {
        dataset.remove(tag_pixel_spacing);
        auto result = validator.validate(dataset);
        // Pixel spacing is not a required attribute in the validator
        CHECK(result.is_valid);
    }
}

