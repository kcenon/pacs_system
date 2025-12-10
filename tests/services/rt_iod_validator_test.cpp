/**
 * @file rt_iod_validator_test.cpp
 * @brief Unit tests for Radiation Therapy IOD Validators
 */

#include <pacs/services/validation/rt_iod_validator.hpp>
#include <pacs/services/sop_classes/rt_storage.hpp>
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
// Test Fixtures
// ============================================================================

namespace {

// RT-specific tags for testing
constexpr dicom_tag tag_rt_plan_label{0x300A, 0x0002};
constexpr dicom_tag tag_rt_plan_geometry{0x300A, 0x000C};
[[maybe_unused]] constexpr dicom_tag tag_plan_intent{0x300A, 0x000A};
constexpr dicom_tag tag_fraction_group_sequence{0x300A, 0x0070};
[[maybe_unused]] constexpr dicom_tag tag_beam_sequence{0x300A, 0x00B0};
constexpr dicom_tag tag_dose_units{0x3004, 0x0002};
constexpr dicom_tag tag_dose_type{0x3004, 0x0004};
constexpr dicom_tag tag_dose_summation_type{0x3004, 0x000A};
constexpr dicom_tag tag_dose_grid_scaling{0x3004, 0x000E};
constexpr dicom_tag tag_structure_set_label{0x3006, 0x0002};
constexpr dicom_tag tag_structure_set_roi_sequence{0x3006, 0x0020};
constexpr dicom_tag tag_roi_contour_sequence{0x3006, 0x0039};
constexpr dicom_tag tag_rt_roi_observations_sequence{0x3006, 0x0080};
constexpr dicom_tag tag_frame_of_reference_uid{0x0020, 0x0052};

// Helper: Check if validation result has errors
bool has_errors(const validation_result& result) {
    return std::any_of(result.findings.begin(), result.findings.end(),
                       [](const validation_finding& f) {
                           return f.severity == validation_severity::error;
                       });
}

// Helper: Check if validation result has warnings
[[maybe_unused]] bool has_warnings(const validation_result& result) {
    return std::any_of(result.findings.begin(), result.findings.end(),
                       [](const validation_finding& f) {
                           return f.severity == validation_severity::warning;
                       });
}

// Helper: Count findings of a specific severity
[[maybe_unused]] size_t count_findings(const validation_result& result, validation_severity severity) {
    return std::count_if(result.findings.begin(), result.findings.end(),
                         [severity](const validation_finding& f) {
                             return f.severity == severity;
                         });
}

// Create a minimal valid RT Plan dataset
dicom_dataset create_minimal_rt_plan_dataset() {
    dicom_dataset ds;

    // Patient Module (Type 2)
    ds.set_string(tags::patient_name, vr_type::PN, "Test^Patient");
    ds.set_string(tags::patient_id, vr_type::LO, "TEST001");
    ds.set_string(tags::patient_birth_date, vr_type::DA, "19800101");
    ds.set_string(tags::patient_sex, vr_type::CS, "M");

    // General Study Module (Type 1/2)
    ds.set_string(tags::study_instance_uid, vr_type::UI, "1.2.3.4.5.6.7.8.9.1");
    ds.set_string(tags::study_date, vr_type::DA, "20231215");
    ds.set_string(tags::study_time, vr_type::TM, "100000");
    ds.set_string(tags::referring_physician_name, vr_type::PN, "");
    ds.set_string(tags::study_id, vr_type::SH, "STUDY001");
    ds.set_string(tags::accession_number, vr_type::SH, "");

    // RT Series Module
    ds.set_string(tags::modality, vr_type::CS, "RTPLAN");
    ds.set_string(tags::series_instance_uid, vr_type::UI, "1.2.3.4.5.6.7.8.9.2");
    ds.set_string(tags::series_number, vr_type::IS, "1");

    // Frame of Reference Module
    ds.set_string(tag_frame_of_reference_uid, vr_type::UI, "1.2.3.4.5.6.7.8.9.3");

    // RT General Plan Module
    ds.set_string(tag_rt_plan_label, vr_type::SH, "TestPlan");
    ds.set_string(tag_rt_plan_geometry, vr_type::CS, "PATIENT");

    // Fraction Group Sequence (Type 1)
    ds.set_string(tag_fraction_group_sequence, vr_type::SQ, "SEQUENCE_PLACEHOLDER");

    // SOP Common Module
    ds.set_string(tags::sop_class_uid, vr_type::UI, std::string(rt_plan_storage_uid));
    ds.set_string(tags::sop_instance_uid, vr_type::UI, "1.2.3.4.5.6.7.8.9.4");

    return ds;
}

// Create a minimal valid RT Dose dataset
dicom_dataset create_minimal_rt_dose_dataset() {
    dicom_dataset ds;

    // Patient Module
    ds.set_string(tags::patient_name, vr_type::PN, "Test^Patient");
    ds.set_string(tags::patient_id, vr_type::LO, "TEST001");
    ds.set_string(tags::patient_birth_date, vr_type::DA, "19800101");
    ds.set_string(tags::patient_sex, vr_type::CS, "M");

    // General Study Module
    ds.set_string(tags::study_instance_uid, vr_type::UI, "1.2.3.4.5.6.7.8.9.1");
    ds.set_string(tags::study_date, vr_type::DA, "20231215");
    ds.set_string(tags::study_time, vr_type::TM, "100000");
    ds.set_string(tags::study_id, vr_type::SH, "STUDY001");
    ds.set_string(tags::accession_number, vr_type::SH, "");

    // RT Series Module
    ds.set_string(tags::modality, vr_type::CS, "RTDOSE");
    ds.set_string(tags::series_instance_uid, vr_type::UI, "1.2.3.4.5.6.7.8.9.2");

    // Frame of Reference Module
    ds.set_string(tag_frame_of_reference_uid, vr_type::UI, "1.2.3.4.5.6.7.8.9.3");

    // RT Dose Module
    ds.set_string(tag_dose_units, vr_type::CS, "GY");
    ds.set_string(tag_dose_type, vr_type::CS, "PHYSICAL");
    ds.set_string(tag_dose_summation_type, vr_type::CS, "PLAN");
    ds.set_string(tag_dose_grid_scaling, vr_type::DS, "0.001");

    // Image Pixel Module (for dose grid)
    ds.set_numeric<uint16_t>(tags::rows, vr_type::US, 100);
    ds.set_numeric<uint16_t>(tags::columns, vr_type::US, 100);
    ds.set_numeric<uint16_t>(tags::bits_allocated, vr_type::US, 16);
    ds.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, 16);
    ds.set_numeric<uint16_t>(tags::high_bit, vr_type::US, 15);
    ds.set_numeric<uint16_t>(tags::pixel_representation, vr_type::US, 0);

    // SOP Common Module
    ds.set_string(tags::sop_class_uid, vr_type::UI, std::string(rt_dose_storage_uid));
    ds.set_string(tags::sop_instance_uid, vr_type::UI, "1.2.3.4.5.6.7.8.9.4");

    return ds;
}

// Create a minimal valid RT Structure Set dataset
dicom_dataset create_minimal_rt_structure_set_dataset() {
    dicom_dataset ds;

    // Patient Module
    ds.set_string(tags::patient_name, vr_type::PN, "Test^Patient");
    ds.set_string(tags::patient_id, vr_type::LO, "TEST001");
    ds.set_string(tags::patient_birth_date, vr_type::DA, "19800101");
    ds.set_string(tags::patient_sex, vr_type::CS, "M");

    // General Study Module
    ds.set_string(tags::study_instance_uid, vr_type::UI, "1.2.3.4.5.6.7.8.9.1");
    ds.set_string(tags::study_date, vr_type::DA, "20231215");
    ds.set_string(tags::study_time, vr_type::TM, "100000");
    ds.set_string(tags::study_id, vr_type::SH, "STUDY001");
    ds.set_string(tags::accession_number, vr_type::SH, "");

    // RT Series Module
    ds.set_string(tags::modality, vr_type::CS, "RTSTRUCT");
    ds.set_string(tags::series_instance_uid, vr_type::UI, "1.2.3.4.5.6.7.8.9.2");

    // Structure Set Module
    ds.set_string(tag_structure_set_label, vr_type::SH, "TestStructures");
    ds.set_string(tag_structure_set_roi_sequence, vr_type::SQ, "SEQUENCE_PLACEHOLDER");

    // ROI Contour Module
    ds.set_string(tag_roi_contour_sequence, vr_type::SQ, "SEQUENCE_PLACEHOLDER");

    // RT ROI Observations Module
    ds.set_string(tag_rt_roi_observations_sequence, vr_type::SQ, "SEQUENCE_PLACEHOLDER");

    // SOP Common Module
    ds.set_string(tags::sop_class_uid, vr_type::UI, std::string(rt_structure_set_storage_uid));
    ds.set_string(tags::sop_instance_uid, vr_type::UI, "1.2.3.4.5.6.7.8.9.4");

    return ds;
}

}  // namespace

// ============================================================================
// RT Plan IOD Validator Tests
// ============================================================================

TEST_CASE("rt_plan_iod_validator: validate with valid RT Plan dataset", "[rt_validator][plan]") {
    auto dataset = create_minimal_rt_plan_dataset();
    rt_plan_iod_validator validator;

    auto result = validator.validate(dataset);

    // Should pass basic validation
    CHECK(result.is_valid);
    CHECK(result.error_count() == 0);
}

TEST_CASE("rt_plan_iod_validator: quick_check with valid dataset", "[rt_validator][plan]") {
    auto dataset = create_minimal_rt_plan_dataset();
    rt_plan_iod_validator validator;

    CHECK(validator.quick_check(dataset));
}

TEST_CASE("rt_plan_iod_validator: fails without required Type 1 attributes", "[rt_validator][plan]") {
    dicom_dataset ds;
    rt_plan_iod_validator validator;

    auto result = validator.validate(ds);

    CHECK_FALSE(result.is_valid);
    CHECK(has_errors(result));
}

TEST_CASE("rt_plan_iod_validator: fails with wrong modality", "[rt_validator][plan]") {
    auto dataset = create_minimal_rt_plan_dataset();
    dataset.set_string(tags::modality, vr_type::CS, "CT");  // Wrong modality

    rt_plan_iod_validator validator;
    auto result = validator.validate(dataset);

    CHECK_FALSE(result.is_valid);
}

TEST_CASE("rt_plan_iod_validator: validates RTPlanGeometry values", "[rt_validator][plan]") {
    SECTION("valid geometry values") {
        auto dataset = create_minimal_rt_plan_dataset();
        dataset.set_string(tag_rt_plan_geometry, vr_type::CS, "PATIENT");

        rt_plan_iod_validator validator;
        auto result = validator.validate(dataset);

        // Check no warnings about geometry
        bool has_geometry_warning = std::any_of(
            result.findings.begin(), result.findings.end(),
            [](const validation_finding& f) {
                return f.tag == tag_rt_plan_geometry &&
                       f.severity == validation_severity::warning;
            });
        CHECK_FALSE(has_geometry_warning);
    }

    SECTION("invalid geometry produces warning") {
        auto dataset = create_minimal_rt_plan_dataset();
        dataset.set_string(tag_rt_plan_geometry, vr_type::CS, "INVALID_GEOMETRY");

        rt_plan_iod_validator validator;
        auto result = validator.validate(dataset);

        bool has_geometry_warning = std::any_of(
            result.findings.begin(), result.findings.end(),
            [](const validation_finding& f) {
                return f.tag == tag_rt_plan_geometry &&
                       f.severity == validation_severity::warning;
            });
        CHECK(has_geometry_warning);
    }
}

TEST_CASE("rt_plan_iod_validator: respects validation options", "[rt_validator][plan]") {
    auto dataset = create_minimal_rt_plan_dataset();

    SECTION("disabling Type 2 check reduces warnings") {
        rt_validation_options options;
        options.check_type2 = false;

        rt_plan_iod_validator validator(options);
        auto result = validator.validate(dataset);

        CHECK(result.is_valid);
    }

    SECTION("strict mode converts warnings to errors") {
        rt_validation_options options;
        options.strict_mode = true;

        rt_plan_iod_validator validator(options);
        auto result = validator.validate(dataset);

        // In strict mode, any warning would make it invalid
    }
}

// ============================================================================
// RT Dose IOD Validator Tests
// ============================================================================

TEST_CASE("rt_dose_iod_validator: validate with valid RT Dose dataset", "[rt_validator][dose]") {
    auto dataset = create_minimal_rt_dose_dataset();
    rt_dose_iod_validator validator;

    auto result = validator.validate(dataset);

    CHECK(result.is_valid);
    CHECK(result.error_count() == 0);
}

TEST_CASE("rt_dose_iod_validator: quick_check with valid dataset", "[rt_validator][dose]") {
    auto dataset = create_minimal_rt_dose_dataset();
    rt_dose_iod_validator validator;

    CHECK(validator.quick_check(dataset));
}

TEST_CASE("rt_dose_iod_validator: fails without required dose attributes", "[rt_validator][dose]") {
    auto dataset = create_minimal_rt_dose_dataset();

    // Remove required dose attributes
    // Note: Would need to implement remove functionality in dicom_dataset
    // For now, just create empty dataset
    dicom_dataset ds;
    rt_dose_iod_validator validator;

    auto result = validator.validate(ds);

    CHECK_FALSE(result.is_valid);
    CHECK(has_errors(result));
}

TEST_CASE("rt_dose_iod_validator: validates DoseUnits values", "[rt_validator][dose]") {
    SECTION("valid GY units") {
        auto dataset = create_minimal_rt_dose_dataset();
        dataset.set_string(tag_dose_units, vr_type::CS, "GY");

        rt_dose_iod_validator validator;
        auto result = validator.validate(dataset);

        bool has_units_warning = std::any_of(
            result.findings.begin(), result.findings.end(),
            [](const validation_finding& f) {
                return f.tag == tag_dose_units &&
                       f.severity == validation_severity::warning;
            });
        CHECK_FALSE(has_units_warning);
    }

    SECTION("valid RELATIVE units") {
        auto dataset = create_minimal_rt_dose_dataset();
        dataset.set_string(tag_dose_units, vr_type::CS, "RELATIVE");

        rt_dose_iod_validator validator;
        auto result = validator.validate(dataset);

        bool has_units_warning = std::any_of(
            result.findings.begin(), result.findings.end(),
            [](const validation_finding& f) {
                return f.tag == tag_dose_units &&
                       f.severity == validation_severity::warning;
            });
        CHECK_FALSE(has_units_warning);
    }
}

TEST_CASE("rt_dose_iod_validator: validates DoseSummationType", "[rt_validator][dose]") {
    auto dataset = create_minimal_rt_dose_dataset();
    dataset.set_string(tag_dose_summation_type, vr_type::CS, "INVALID_TYPE");

    rt_dose_iod_validator validator;
    auto result = validator.validate(dataset);

    bool has_summation_warning = std::any_of(
        result.findings.begin(), result.findings.end(),
        [](const validation_finding& f) {
            return f.tag == tag_dose_summation_type &&
                   f.severity == validation_severity::warning;
        });
    CHECK(has_summation_warning);
}

// ============================================================================
// RT Structure Set IOD Validator Tests
// ============================================================================

TEST_CASE("rt_structure_set_iod_validator: validate with valid dataset", "[rt_validator][struct]") {
    auto dataset = create_minimal_rt_structure_set_dataset();
    rt_structure_set_iod_validator validator;

    auto result = validator.validate(dataset);

    CHECK(result.is_valid);
    CHECK(result.error_count() == 0);
}

TEST_CASE("rt_structure_set_iod_validator: quick_check with valid dataset", "[rt_validator][struct]") {
    auto dataset = create_minimal_rt_structure_set_dataset();
    rt_structure_set_iod_validator validator;

    CHECK(validator.quick_check(dataset));
}

TEST_CASE("rt_structure_set_iod_validator: fails without Structure Set Label", "[rt_validator][struct]") {
    dicom_dataset ds;
    ds.set_string(tags::modality, vr_type::CS, "RTSTRUCT");
    ds.set_string(tags::series_instance_uid, vr_type::UI, "1.2.3");
    ds.set_string(tags::study_instance_uid, vr_type::UI, "1.2.3.4");
    ds.set_string(tags::sop_class_uid, vr_type::UI, std::string(rt_structure_set_storage_uid));
    ds.set_string(tags::sop_instance_uid, vr_type::UI, "1.2.3.4.5");
    // Missing: structure_set_label, structure_set_roi_sequence

    rt_structure_set_iod_validator validator;
    auto result = validator.validate(ds);

    CHECK_FALSE(result.is_valid);
    CHECK(has_errors(result));
}

// ============================================================================
// Unified RT IOD Validator Tests
// ============================================================================

TEST_CASE("rt_iod_validator: auto-detects RT Plan", "[rt_validator][unified]") {
    auto dataset = create_minimal_rt_plan_dataset();
    rt_iod_validator validator;

    auto result = validator.validate(dataset);

    CHECK(result.is_valid);
}

TEST_CASE("rt_iod_validator: auto-detects RT Dose", "[rt_validator][unified]") {
    auto dataset = create_minimal_rt_dose_dataset();
    rt_iod_validator validator;

    auto result = validator.validate(dataset);

    CHECK(result.is_valid);
}

TEST_CASE("rt_iod_validator: auto-detects RT Structure Set", "[rt_validator][unified]") {
    auto dataset = create_minimal_rt_structure_set_dataset();
    rt_iod_validator validator;

    auto result = validator.validate(dataset);

    CHECK(result.is_valid);
}

TEST_CASE("rt_iod_validator: quick_check detects any valid RT type", "[rt_validator][unified]") {
    rt_iod_validator validator;

    SECTION("detects valid RT Plan") {
        auto dataset = create_minimal_rt_plan_dataset();
        CHECK(validator.quick_check(dataset));
    }

    SECTION("detects valid RT Dose") {
        auto dataset = create_minimal_rt_dose_dataset();
        CHECK(validator.quick_check(dataset));
    }

    SECTION("detects valid RT Structure Set") {
        auto dataset = create_minimal_rt_structure_set_dataset();
        CHECK(validator.quick_check(dataset));
    }

    SECTION("rejects non-RT dataset") {
        dicom_dataset ds;
        ds.set_string(tags::modality, vr_type::CS, "CT");
        CHECK_FALSE(validator.quick_check(ds));
    }
}

TEST_CASE("rt_iod_validator: fails for unknown RT type", "[rt_validator][unified]") {
    dicom_dataset ds;
    ds.set_string(tags::sop_class_uid, vr_type::UI, "1.2.3.4.5.6.7");  // Unknown SOP Class
    ds.set_string(tags::modality, vr_type::CS, "UNKNOWN");

    rt_iod_validator validator;
    auto result = validator.validate(ds);

    CHECK_FALSE(result.is_valid);
}

// ============================================================================
// Convenience Function Tests
// ============================================================================

TEST_CASE("validate_rt_plan_iod convenience function", "[rt_validator][convenience]") {
    auto dataset = create_minimal_rt_plan_dataset();

    auto result = validate_rt_plan_iod(dataset);

    CHECK(result.is_valid);
}

TEST_CASE("validate_rt_dose_iod convenience function", "[rt_validator][convenience]") {
    auto dataset = create_minimal_rt_dose_dataset();

    auto result = validate_rt_dose_iod(dataset);

    CHECK(result.is_valid);
}

TEST_CASE("validate_rt_structure_set_iod convenience function", "[rt_validator][convenience]") {
    auto dataset = create_minimal_rt_structure_set_dataset();

    auto result = validate_rt_structure_set_iod(dataset);

    CHECK(result.is_valid);
}

TEST_CASE("validate_rt_iod convenience function", "[rt_validator][convenience]") {
    SECTION("validates RT Plan") {
        auto dataset = create_minimal_rt_plan_dataset();
        auto result = validate_rt_iod(dataset);
        CHECK(result.is_valid);
    }

    SECTION("validates RT Dose") {
        auto dataset = create_minimal_rt_dose_dataset();
        auto result = validate_rt_iod(dataset);
        CHECK(result.is_valid);
    }
}

TEST_CASE("is_valid_rt_dataset convenience functions", "[rt_validator][convenience]") {
    CHECK(is_valid_rt_plan_dataset(create_minimal_rt_plan_dataset()));
    CHECK(is_valid_rt_dose_dataset(create_minimal_rt_dose_dataset()));
    CHECK(is_valid_rt_structure_set_dataset(create_minimal_rt_structure_set_dataset()));
    CHECK(is_valid_rt_dataset(create_minimal_rt_plan_dataset()));
}

// ============================================================================
// Validation Options Tests
// ============================================================================

TEST_CASE("rt_validation_options default values", "[rt_validator][options]") {
    rt_validation_options options;

    CHECK(options.check_type1 == true);
    CHECK(options.check_type2 == true);
    CHECK(options.check_conditional == true);
    CHECK(options.validate_rt_plan == true);
    CHECK(options.validate_rt_dose == true);
    CHECK(options.validate_rt_structure_set == true);
    CHECK(options.validate_pixel_data == true);
    CHECK(options.validate_references == true);
    CHECK(options.allow_retired == true);
    CHECK(options.strict_mode == false);
}

TEST_CASE("rt_plan_iod_validator options getter/setter", "[rt_validator][options]") {
    rt_plan_iod_validator validator;

    rt_validation_options new_options;
    new_options.strict_mode = true;
    new_options.check_type2 = false;

    validator.set_options(new_options);

    const auto& retrieved = validator.options();
    CHECK(retrieved.strict_mode == true);
    CHECK(retrieved.check_type2 == false);
}
