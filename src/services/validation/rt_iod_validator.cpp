/**
 * @file rt_iod_validator.cpp
 * @brief Implementation of Radiation Therapy IOD Validators
 */

#include "pacs/services/validation/rt_iod_validator.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/services/sop_classes/rt_storage.hpp"

#include <sstream>

namespace pacs::services::validation {

using namespace pacs::core;

// =============================================================================
// RT-specific DICOM tags
// =============================================================================
namespace rt_tags {
    // RT Series Module
    constexpr dicom_tag modality{0x0008, 0x0060};
    constexpr dicom_tag series_instance_uid{0x0020, 0x000E};
    constexpr dicom_tag series_number{0x0020, 0x0011};
    constexpr dicom_tag series_description{0x0008, 0x103E};

    // Frame of Reference
    constexpr dicom_tag frame_of_reference_uid{0x0020, 0x0052};
    constexpr dicom_tag position_reference_indicator{0x0020, 0x1040};

    // RT General Plan Module
    constexpr dicom_tag rt_plan_label{0x300A, 0x0002};
    constexpr dicom_tag rt_plan_name{0x300A, 0x0003};
    constexpr dicom_tag rt_plan_description{0x300A, 0x0004};
    constexpr dicom_tag rt_plan_date{0x300A, 0x0006};
    constexpr dicom_tag rt_plan_time{0x300A, 0x0007};
    constexpr dicom_tag rt_plan_geometry{0x300A, 0x000C};
    constexpr dicom_tag treatment_protocols{0x300A, 0x0009};
    constexpr dicom_tag plan_intent{0x300A, 0x000A};
    constexpr dicom_tag treatment_sites{0x300A, 0x000B};
    constexpr dicom_tag referenced_structure_set_sequence{0x300C, 0x0060};
    constexpr dicom_tag referenced_dose_sequence{0x300C, 0x0080};

    // RT Fraction Scheme Module
    constexpr dicom_tag fraction_group_sequence{0x300A, 0x0070};
    constexpr dicom_tag fraction_group_number{0x300A, 0x0071};
    constexpr dicom_tag fraction_group_description{0x300A, 0x0072};
    constexpr dicom_tag number_of_fractions_planned{0x300A, 0x0078};
    constexpr dicom_tag number_of_beams{0x300A, 0x0080};
    constexpr dicom_tag number_of_brachy_application_setups{0x300A, 0x00A0};
    constexpr dicom_tag referenced_beam_sequence{0x300C, 0x0004};

    // RT Beams Module
    constexpr dicom_tag beam_sequence{0x300A, 0x00B0};
    constexpr dicom_tag beam_number{0x300A, 0x00C0};
    constexpr dicom_tag beam_name{0x300A, 0x00C2};
    constexpr dicom_tag beam_description{0x300A, 0x00C3};
    constexpr dicom_tag beam_type{0x300A, 0x00C4};
    constexpr dicom_tag radiation_type{0x300A, 0x00C6};
    constexpr dicom_tag treatment_machine_name{0x300A, 0x00B2};
    constexpr dicom_tag primary_dosimeter_unit{0x300A, 0x00B3};
    constexpr dicom_tag source_axis_distance{0x300A, 0x00B4};
    constexpr dicom_tag beam_limiting_device_sequence{0x300A, 0x00B6};
    constexpr dicom_tag number_of_wedges{0x300A, 0x00D0};
    constexpr dicom_tag number_of_compensators{0x300A, 0x00E0};
    constexpr dicom_tag number_of_boli{0x300A, 0x00ED};
    constexpr dicom_tag number_of_blocks{0x300A, 0x00F0};
    constexpr dicom_tag final_cumulative_meterset_weight{0x300A, 0x010E};
    constexpr dicom_tag number_of_control_points{0x300A, 0x0110};
    constexpr dicom_tag control_point_sequence{0x300A, 0x0111};

    // RT Dose Module
    constexpr dicom_tag dose_units{0x3004, 0x0002};
    constexpr dicom_tag dose_type{0x3004, 0x0004};
    constexpr dicom_tag spatial_transform_of_dose{0x3004, 0x0005};
    constexpr dicom_tag dose_comment{0x3004, 0x0006};
    constexpr dicom_tag normalization_point{0x3004, 0x0008};
    constexpr dicom_tag dose_summation_type{0x3004, 0x000A};
    constexpr dicom_tag grid_frame_offset_vector{0x3004, 0x000C};
    constexpr dicom_tag dose_grid_scaling{0x3004, 0x000E};
    constexpr dicom_tag tissue_heterogeneity_correction{0x3004, 0x0014};
    constexpr dicom_tag referenced_rt_plan_sequence{0x300C, 0x0002};

    // Structure Set Module
    constexpr dicom_tag structure_set_label{0x3006, 0x0002};
    constexpr dicom_tag structure_set_name{0x3006, 0x0004};
    constexpr dicom_tag structure_set_description{0x3006, 0x0006};
    constexpr dicom_tag structure_set_date{0x3006, 0x0008};
    constexpr dicom_tag structure_set_time{0x3006, 0x0009};
    constexpr dicom_tag referenced_frame_of_reference_sequence{0x3006, 0x0010};
    constexpr dicom_tag structure_set_roi_sequence{0x3006, 0x0020};
    constexpr dicom_tag roi_number{0x3006, 0x0022};
    constexpr dicom_tag referenced_frame_of_reference_uid{0x3006, 0x0024};
    constexpr dicom_tag roi_name{0x3006, 0x0026};
    constexpr dicom_tag roi_description{0x3006, 0x0028};
    constexpr dicom_tag roi_volume{0x3006, 0x002C};
    constexpr dicom_tag roi_generation_algorithm{0x3006, 0x0036};
    constexpr dicom_tag roi_generation_description{0x3006, 0x0038};

    // ROI Contour Module
    constexpr dicom_tag roi_contour_sequence{0x3006, 0x0039};
    constexpr dicom_tag referenced_roi_number{0x3006, 0x0084};
    constexpr dicom_tag roi_display_color{0x3006, 0x002A};
    constexpr dicom_tag contour_sequence{0x3006, 0x0040};
    constexpr dicom_tag contour_geometric_type{0x3006, 0x0042};
    constexpr dicom_tag number_of_contour_points{0x3006, 0x0046};
    constexpr dicom_tag contour_data{0x3006, 0x0050};

    // RT ROI Observations Module
    constexpr dicom_tag rt_roi_observations_sequence{0x3006, 0x0080};
    constexpr dicom_tag observation_number{0x3006, 0x0082};
    constexpr dicom_tag rt_roi_interpreted_type{0x3006, 0x00A4};
    constexpr dicom_tag roi_interpreter{0x3006, 0x00A6};

    // Image Pixel Module (for RT Dose)
    constexpr dicom_tag rows{0x0028, 0x0010};
    constexpr dicom_tag columns{0x0028, 0x0011};
    constexpr dicom_tag pixel_spacing{0x0028, 0x0030};
    constexpr dicom_tag bits_allocated{0x0028, 0x0100};
    constexpr dicom_tag bits_stored{0x0028, 0x0101};
    constexpr dicom_tag high_bit{0x0028, 0x0102};
    constexpr dicom_tag pixel_representation{0x0028, 0x0103};
    constexpr dicom_tag pixel_data{0x7FE0, 0x0010};
    constexpr dicom_tag samples_per_pixel{0x0028, 0x0002};
    constexpr dicom_tag photometric_interpretation{0x0028, 0x0004};
    constexpr dicom_tag number_of_frames{0x0028, 0x0008};

    // Image Plane Module
    constexpr dicom_tag image_position_patient{0x0020, 0x0032};
    constexpr dicom_tag image_orientation_patient{0x0020, 0x0037};
    constexpr dicom_tag slice_thickness{0x0018, 0x0050};
}

// =============================================================================
// rt_plan_iod_validator Implementation
// =============================================================================

rt_plan_iod_validator::rt_plan_iod_validator(const rt_validation_options& options)
    : options_(options) {}

validation_result rt_plan_iod_validator::validate(const dicom_dataset& dataset) const {
    validation_result result;
    result.is_valid = true;

    // Validate mandatory modules
    if (options_.check_type1 || options_.check_type2) {
        validate_patient_module(dataset, result.findings);
        validate_general_study_module(dataset, result.findings);
        validate_rt_series_module(dataset, result.findings);
        validate_frame_of_reference_module(dataset, result.findings);
        validate_rt_general_plan_module(dataset, result.findings);
        validate_sop_common_module(dataset, result.findings);
    }

    // RT Plan specific validation
    if (options_.validate_rt_plan) {
        validate_rt_fraction_scheme_module(dataset, result.findings);
        validate_rt_beams_module(dataset, result.findings);
    }

    // Check for errors
    for (const auto& finding : result.findings) {
        if (finding.severity == validation_severity::error) {
            result.is_valid = false;
            break;
        }
        if (options_.strict_mode && finding.severity == validation_severity::warning) {
            result.is_valid = false;
            break;
        }
    }

    return result;
}

bool rt_plan_iod_validator::quick_check(const dicom_dataset& dataset) const {
    // Check Type 1 required attributes
    if (!dataset.contains(tags::study_instance_uid)) return false;
    if (!dataset.contains(tags::series_instance_uid)) return false;
    if (!dataset.contains(tags::modality)) return false;

    auto modality = dataset.get_string(tags::modality);
    if (modality != "RTPLAN") return false;

    if (!dataset.contains(rt_tags::rt_plan_label)) return false;
    if (!dataset.contains(rt_tags::rt_plan_geometry)) return false;
    if (!dataset.contains(tags::sop_class_uid)) return false;
    if (!dataset.contains(tags::sop_instance_uid)) return false;

    return true;
}

const rt_validation_options& rt_plan_iod_validator::options() const noexcept {
    return options_;
}

void rt_plan_iod_validator::set_options(const rt_validation_options& options) {
    options_ = options;
}

void rt_plan_iod_validator::validate_patient_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type2) {
        check_type2_attribute(dataset, tags::patient_name, "PatientName", findings);
        check_type2_attribute(dataset, tags::patient_id, "PatientID", findings);
        check_type2_attribute(dataset, tags::patient_birth_date, "PatientBirthDate", findings);
        check_type2_attribute(dataset, tags::patient_sex, "PatientSex", findings);
    }
}

void rt_plan_iod_validator::validate_general_study_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::study_instance_uid, "StudyInstanceUID", findings);
    }

    if (options_.check_type2) {
        check_type2_attribute(dataset, tags::study_date, "StudyDate", findings);
        check_type2_attribute(dataset, tags::study_time, "StudyTime", findings);
        check_type2_attribute(dataset, tags::referring_physician_name, "ReferringPhysicianName", findings);
        check_type2_attribute(dataset, tags::study_id, "StudyID", findings);
        check_type2_attribute(dataset, tags::accession_number, "AccessionNumber", findings);
    }
}

void rt_plan_iod_validator::validate_rt_series_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::modality, "Modality", findings);
        check_type1_attribute(dataset, tags::series_instance_uid, "SeriesInstanceUID", findings);

        // Validate Modality is RTPLAN
        if (dataset.contains(tags::modality)) {
            auto modality = dataset.get_string(tags::modality);
            if (modality != "RTPLAN") {
                findings.push_back({
                    validation_severity::error,
                    tags::modality,
                    "Modality must be 'RTPLAN' for RT Plan, found: " + modality,
                    "RTPLAN-ERR-001"
                });
            }
        }
    }

    if (options_.check_type2) {
        check_type2_attribute(dataset, rt_tags::series_number, "SeriesNumber", findings);
    }
}

void rt_plan_iod_validator::validate_frame_of_reference_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        check_type1_attribute(dataset, rt_tags::frame_of_reference_uid,
                              "FrameOfReferenceUID", findings);
    }

    if (options_.check_type2) {
        check_type2_attribute(dataset, rt_tags::position_reference_indicator,
                              "PositionReferenceIndicator", findings);
    }
}

void rt_plan_iod_validator::validate_rt_general_plan_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Type 1 attributes
    if (options_.check_type1) {
        check_type1_attribute(dataset, rt_tags::rt_plan_label, "RTPlanLabel", findings);
        check_type1_attribute(dataset, rt_tags::rt_plan_geometry, "RTPlanGeometry", findings);
    }

    // Validate RTPlanGeometry value
    if (dataset.contains(rt_tags::rt_plan_geometry)) {
        auto geometry = dataset.get_string(rt_tags::rt_plan_geometry);
        if (geometry != "PATIENT" && geometry != "TREATMENT_DEVICE") {
            findings.push_back({
                validation_severity::warning,
                rt_tags::rt_plan_geometry,
                "Invalid RTPlanGeometry: " + geometry + " (expected PATIENT or TREATMENT_DEVICE)",
                "RTPLAN-WARN-001"
            });
        }
    }

    // Type 2 attributes
    if (options_.check_type2) {
        check_type2_attribute(dataset, rt_tags::rt_plan_date, "RTPlanDate", findings);
        check_type2_attribute(dataset, rt_tags::rt_plan_time, "RTPlanTime", findings);
    }

    // Type 3 but important: Plan Intent
    if (dataset.contains(rt_tags::plan_intent)) {
        auto intent = dataset.get_string(rt_tags::plan_intent);
        if (intent != "CURATIVE" && intent != "PALLIATIVE" && intent != "PROPHYLACTIC" &&
            intent != "VERIFICATION" && intent != "MACHINE_QA" && intent != "RESEARCH" &&
            intent != "SERVICE") {
            findings.push_back({
                validation_severity::info,
                rt_tags::plan_intent,
                "Non-standard PlanIntent: " + intent,
                "RTPLAN-INFO-001"
            });
        }
    }

    // Referenced Structure Set Sequence is important
    if (options_.validate_references) {
        if (!dataset.contains(rt_tags::referenced_structure_set_sequence)) {
            findings.push_back({
                validation_severity::warning,
                rt_tags::referenced_structure_set_sequence,
                "ReferencedStructureSetSequence not present - plan has no associated structures",
                "RTPLAN-WARN-002"
            });
        }
    }
}

void rt_plan_iod_validator::validate_rt_fraction_scheme_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Fraction Group Sequence is Type 1
    if (options_.check_type1) {
        check_type1_attribute(dataset, rt_tags::fraction_group_sequence,
                              "FractionGroupSequence", findings);
    }

    // Validate fraction scheme details if sequence exists
    if (dataset.contains(rt_tags::fraction_group_sequence)) {
        // NumberOfFractionsPlanned should be present
        if (!dataset.contains(rt_tags::number_of_fractions_planned)) {
            findings.push_back({
                validation_severity::warning,
                rt_tags::number_of_fractions_planned,
                "NumberOfFractionsPlanned not specified in FractionGroupSequence",
                "RTPLAN-WARN-003"
            });
        }
    }
}

void rt_plan_iod_validator::validate_rt_beams_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Beam Sequence is Type 1C (required if NumberOfBeams > 0)
    if (dataset.contains(rt_tags::number_of_beams)) {
        auto num_beams = dataset.get_numeric<int32_t>(rt_tags::number_of_beams);
        if (num_beams && *num_beams > 0) {
            if (!dataset.contains(rt_tags::beam_sequence)) {
                findings.push_back({
                    validation_severity::error,
                    rt_tags::beam_sequence,
                    "BeamSequence required when NumberOfBeams > 0",
                    "RTPLAN-ERR-002"
                });
            }
        }
    }

    // Validate beam details if sequence exists
    if (dataset.contains(rt_tags::beam_sequence)) {
        // Check for treatment machine name
        if (!dataset.contains(rt_tags::treatment_machine_name)) {
            findings.push_back({
                validation_severity::info,
                rt_tags::treatment_machine_name,
                "TreatmentMachineName not specified",
                "RTPLAN-INFO-002"
            });
        }

        // Validate Beam Type if present
        if (dataset.contains(rt_tags::beam_type)) {
            auto beam_type = dataset.get_string(rt_tags::beam_type);
            if (beam_type != "STATIC" && beam_type != "DYNAMIC") {
                findings.push_back({
                    validation_severity::warning,
                    rt_tags::beam_type,
                    "Non-standard BeamType: " + beam_type + " (expected STATIC or DYNAMIC)",
                    "RTPLAN-WARN-004"
                });
            }
        }

        // Validate Radiation Type if present
        if (dataset.contains(rt_tags::radiation_type)) {
            auto radiation_type = dataset.get_string(rt_tags::radiation_type);
            if (radiation_type != "PHOTON" && radiation_type != "ELECTRON" &&
                radiation_type != "NEUTRON" && radiation_type != "PROTON" &&
                radiation_type != "ION") {
                findings.push_back({
                    validation_severity::warning,
                    rt_tags::radiation_type,
                    "Non-standard RadiationType: " + radiation_type,
                    "RTPLAN-WARN-005"
                });
            }
        }
    }
}

void rt_plan_iod_validator::validate_sop_common_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::sop_class_uid, "SOPClassUID", findings);
        check_type1_attribute(dataset, tags::sop_instance_uid, "SOPInstanceUID", findings);
    }

    // Validate SOP Class UID is RT Plan
    if (dataset.contains(tags::sop_class_uid)) {
        auto sop_class = dataset.get_string(tags::sop_class_uid);
        if (!sop_classes::is_rt_plan_sop_class(sop_class)) {
            findings.push_back({
                validation_severity::error,
                tags::sop_class_uid,
                "SOPClassUID is not an RT Plan SOP Class: " + sop_class,
                "RTPLAN-ERR-003"
            });
        }
    }
}

void rt_plan_iod_validator::check_type1_attribute(
    const dicom_dataset& dataset,
    dicom_tag tag,
    std::string_view name,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(tag)) {
        findings.push_back({
            validation_severity::error,
            tag,
            std::string("Type 1 attribute missing: ") + std::string(name) +
            " (" + tag.to_string() + ")",
            "RTPLAN-TYPE1-MISSING"
        });
    } else {
        auto value = dataset.get_string(tag);
        if (value.empty()) {
            findings.push_back({
                validation_severity::error,
                tag,
                std::string("Type 1 attribute has empty value: ") +
                std::string(name) + " (" + tag.to_string() + ")",
                "RTPLAN-TYPE1-EMPTY"
            });
        }
    }
}

void rt_plan_iod_validator::check_type2_attribute(
    const dicom_dataset& dataset,
    dicom_tag tag,
    std::string_view name,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(tag)) {
        findings.push_back({
            validation_severity::warning,
            tag,
            std::string("Type 2 attribute missing: ") + std::string(name) +
            " (" + tag.to_string() + ")",
            "RTPLAN-TYPE2-MISSING"
        });
    }
}

// =============================================================================
// rt_dose_iod_validator Implementation
// =============================================================================

rt_dose_iod_validator::rt_dose_iod_validator(const rt_validation_options& options)
    : options_(options) {}

validation_result rt_dose_iod_validator::validate(const dicom_dataset& dataset) const {
    validation_result result;
    result.is_valid = true;

    // Validate mandatory modules
    if (options_.check_type1 || options_.check_type2) {
        validate_patient_module(dataset, result.findings);
        validate_general_study_module(dataset, result.findings);
        validate_rt_series_module(dataset, result.findings);
        validate_frame_of_reference_module(dataset, result.findings);
        validate_rt_dose_module(dataset, result.findings);
        validate_sop_common_module(dataset, result.findings);
    }

    // Validate pixel data for dose grid
    if (options_.validate_pixel_data) {
        validate_image_pixel_module(dataset, result.findings);
    }

    // Check for errors
    for (const auto& finding : result.findings) {
        if (finding.severity == validation_severity::error) {
            result.is_valid = false;
            break;
        }
        if (options_.strict_mode && finding.severity == validation_severity::warning) {
            result.is_valid = false;
            break;
        }
    }

    return result;
}

bool rt_dose_iod_validator::quick_check(const dicom_dataset& dataset) const {
    if (!dataset.contains(tags::study_instance_uid)) return false;
    if (!dataset.contains(tags::series_instance_uid)) return false;
    if (!dataset.contains(tags::modality)) return false;

    auto modality = dataset.get_string(tags::modality);
    if (modality != "RTDOSE") return false;

    if (!dataset.contains(rt_tags::dose_units)) return false;
    if (!dataset.contains(rt_tags::dose_type)) return false;
    if (!dataset.contains(rt_tags::dose_summation_type)) return false;
    if (!dataset.contains(tags::sop_class_uid)) return false;
    if (!dataset.contains(tags::sop_instance_uid)) return false;

    return true;
}

const rt_validation_options& rt_dose_iod_validator::options() const noexcept {
    return options_;
}

void rt_dose_iod_validator::set_options(const rt_validation_options& options) {
    options_ = options;
}

void rt_dose_iod_validator::validate_patient_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type2) {
        check_type2_attribute(dataset, tags::patient_name, "PatientName", findings);
        check_type2_attribute(dataset, tags::patient_id, "PatientID", findings);
        check_type2_attribute(dataset, tags::patient_birth_date, "PatientBirthDate", findings);
        check_type2_attribute(dataset, tags::patient_sex, "PatientSex", findings);
    }
}

void rt_dose_iod_validator::validate_general_study_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::study_instance_uid, "StudyInstanceUID", findings);
    }

    if (options_.check_type2) {
        check_type2_attribute(dataset, tags::study_date, "StudyDate", findings);
        check_type2_attribute(dataset, tags::study_time, "StudyTime", findings);
        check_type2_attribute(dataset, tags::study_id, "StudyID", findings);
        check_type2_attribute(dataset, tags::accession_number, "AccessionNumber", findings);
    }
}

void rt_dose_iod_validator::validate_rt_series_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::modality, "Modality", findings);
        check_type1_attribute(dataset, tags::series_instance_uid, "SeriesInstanceUID", findings);

        if (dataset.contains(tags::modality)) {
            auto modality = dataset.get_string(tags::modality);
            if (modality != "RTDOSE") {
                findings.push_back({
                    validation_severity::error,
                    tags::modality,
                    "Modality must be 'RTDOSE' for RT Dose, found: " + modality,
                    "RTDOSE-ERR-001"
                });
            }
        }
    }
}

void rt_dose_iod_validator::validate_frame_of_reference_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        check_type1_attribute(dataset, rt_tags::frame_of_reference_uid,
                              "FrameOfReferenceUID", findings);
    }
}

void rt_dose_iod_validator::validate_rt_dose_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Type 1 attributes
    if (options_.check_type1) {
        check_type1_attribute(dataset, rt_tags::dose_units, "DoseUnits", findings);
        check_type1_attribute(dataset, rt_tags::dose_type, "DoseType", findings);
        check_type1_attribute(dataset, rt_tags::dose_summation_type, "DoseSummationType", findings);
    }

    // Validate DoseUnits
    if (dataset.contains(rt_tags::dose_units)) {
        auto units = dataset.get_string(rt_tags::dose_units);
        if (units != "GY" && units != "RELATIVE") {
            findings.push_back({
                validation_severity::warning,
                rt_tags::dose_units,
                "Non-standard DoseUnits: " + units + " (expected GY or RELATIVE)",
                "RTDOSE-WARN-001"
            });
        }
    }

    // Validate DoseType
    if (dataset.contains(rt_tags::dose_type)) {
        auto dose_type = dataset.get_string(rt_tags::dose_type);
        if (dose_type != "PHYSICAL" && dose_type != "EFFECTIVE" && dose_type != "ERROR") {
            findings.push_back({
                validation_severity::warning,
                rt_tags::dose_type,
                "Non-standard DoseType: " + dose_type,
                "RTDOSE-WARN-002"
            });
        }
    }

    // Validate DoseSummationType
    if (dataset.contains(rt_tags::dose_summation_type)) {
        auto summation = dataset.get_string(rt_tags::dose_summation_type);
        if (summation != "PLAN" && summation != "MULTI_PLAN" && summation != "FRACTION" &&
            summation != "BEAM" && summation != "BRACHY" && summation != "FRACTION_SESSION" &&
            summation != "BEAM_SESSION" && summation != "BRACHY_SESSION" &&
            summation != "CONTROL_POINT" && summation != "RECORD") {
            findings.push_back({
                validation_severity::warning,
                rt_tags::dose_summation_type,
                "Non-standard DoseSummationType: " + summation,
                "RTDOSE-WARN-003"
            });
        }
    }

    // DoseGridScaling is required for proper dose value interpretation
    if (!dataset.contains(rt_tags::dose_grid_scaling)) {
        findings.push_back({
            validation_severity::warning,
            rt_tags::dose_grid_scaling,
            "DoseGridScaling not present - dose values may not be properly scaled",
            "RTDOSE-WARN-004"
        });
    }

    // ReferencedRTPlanSequence for traceability
    if (options_.validate_references) {
        if (!dataset.contains(rt_tags::referenced_rt_plan_sequence)) {
            findings.push_back({
                validation_severity::info,
                rt_tags::referenced_rt_plan_sequence,
                "ReferencedRTPlanSequence not present - dose has no associated plan reference",
                "RTDOSE-INFO-001"
            });
        }
    }

    // Validate dose data consistency
    if (options_.validate_pixel_data) {
        check_dose_data_consistency(dataset, findings);
    }
}

void rt_dose_iod_validator::validate_image_pixel_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // For dose grids, pixel data is required
    if (options_.check_type1) {
        check_type1_attribute(dataset, rt_tags::rows, "Rows", findings);
        check_type1_attribute(dataset, rt_tags::columns, "Columns", findings);
        check_type1_attribute(dataset, rt_tags::bits_allocated, "BitsAllocated", findings);
        check_type1_attribute(dataset, rt_tags::bits_stored, "BitsStored", findings);
        check_type1_attribute(dataset, rt_tags::high_bit, "HighBit", findings);
        check_type1_attribute(dataset, rt_tags::pixel_representation, "PixelRepresentation", findings);
    }

    // Pixel Data may not be present for point doses
    if (!dataset.contains(rt_tags::pixel_data)) {
        findings.push_back({
            validation_severity::info,
            rt_tags::pixel_data,
            "PixelData not present - this may be a point dose or DVH-only object",
            "RTDOSE-INFO-002"
        });
    }
}

void rt_dose_iod_validator::validate_sop_common_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::sop_class_uid, "SOPClassUID", findings);
        check_type1_attribute(dataset, tags::sop_instance_uid, "SOPInstanceUID", findings);
    }

    if (dataset.contains(tags::sop_class_uid)) {
        auto sop_class = dataset.get_string(tags::sop_class_uid);
        if (sop_class != sop_classes::rt_dose_storage_uid) {
            findings.push_back({
                validation_severity::error,
                tags::sop_class_uid,
                "SOPClassUID is not RT Dose Storage: " + sop_class,
                "RTDOSE-ERR-002"
            });
        }
    }
}

void rt_dose_iod_validator::check_type1_attribute(
    const dicom_dataset& dataset,
    dicom_tag tag,
    std::string_view name,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(tag)) {
        findings.push_back({
            validation_severity::error,
            tag,
            std::string("Type 1 attribute missing: ") + std::string(name) +
            " (" + tag.to_string() + ")",
            "RTDOSE-TYPE1-MISSING"
        });
    } else {
        auto value = dataset.get_string(tag);
        if (value.empty()) {
            findings.push_back({
                validation_severity::error,
                tag,
                std::string("Type 1 attribute has empty value: ") +
                std::string(name) + " (" + tag.to_string() + ")",
                "RTDOSE-TYPE1-EMPTY"
            });
        }
    }
}

void rt_dose_iod_validator::check_type2_attribute(
    const dicom_dataset& dataset,
    dicom_tag tag,
    std::string_view name,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(tag)) {
        findings.push_back({
            validation_severity::warning,
            tag,
            std::string("Type 2 attribute missing: ") + std::string(name) +
            " (" + tag.to_string() + ")",
            "RTDOSE-TYPE2-MISSING"
        });
    }
}

void rt_dose_iod_validator::check_dose_data_consistency(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Check bits consistency
    auto bits_allocated = dataset.get_numeric<uint16_t>(rt_tags::bits_allocated);
    auto bits_stored = dataset.get_numeric<uint16_t>(rt_tags::bits_stored);
    auto high_bit = dataset.get_numeric<uint16_t>(rt_tags::high_bit);

    if (bits_allocated && bits_stored) {
        if (*bits_stored > *bits_allocated) {
            findings.push_back({
                validation_severity::error,
                rt_tags::bits_stored,
                "BitsStored exceeds BitsAllocated",
                "RTDOSE-ERR-003"
            });
        }
    }

    if (bits_stored && high_bit) {
        if (*high_bit != *bits_stored - 1) {
            findings.push_back({
                validation_severity::warning,
                rt_tags::high_bit,
                "HighBit should typically be BitsStored - 1",
                "RTDOSE-WARN-005"
            });
        }
    }

    // Multi-frame dose grids should have GridFrameOffsetVector
    auto num_frames = dataset.get_numeric<int32_t>(rt_tags::number_of_frames);
    if (num_frames && *num_frames > 1) {
        if (!dataset.contains(rt_tags::grid_frame_offset_vector)) {
            findings.push_back({
                validation_severity::warning,
                rt_tags::grid_frame_offset_vector,
                "GridFrameOffsetVector recommended for multi-frame dose grids",
                "RTDOSE-WARN-006"
            });
        }
    }
}

// =============================================================================
// rt_structure_set_iod_validator Implementation
// =============================================================================

rt_structure_set_iod_validator::rt_structure_set_iod_validator(const rt_validation_options& options)
    : options_(options) {}

validation_result rt_structure_set_iod_validator::validate(const dicom_dataset& dataset) const {
    validation_result result;
    result.is_valid = true;

    // Validate mandatory modules
    if (options_.check_type1 || options_.check_type2) {
        validate_patient_module(dataset, result.findings);
        validate_general_study_module(dataset, result.findings);
        validate_rt_series_module(dataset, result.findings);
        validate_structure_set_module(dataset, result.findings);
        validate_sop_common_module(dataset, result.findings);
    }

    // RT Structure Set specific validation
    if (options_.validate_rt_structure_set) {
        validate_roi_contour_module(dataset, result.findings);
        validate_rt_roi_observations_module(dataset, result.findings);
    }

    // Check ROI consistency
    check_roi_consistency(dataset, result.findings);

    // Check for errors
    for (const auto& finding : result.findings) {
        if (finding.severity == validation_severity::error) {
            result.is_valid = false;
            break;
        }
        if (options_.strict_mode && finding.severity == validation_severity::warning) {
            result.is_valid = false;
            break;
        }
    }

    return result;
}

bool rt_structure_set_iod_validator::quick_check(const dicom_dataset& dataset) const {
    if (!dataset.contains(tags::study_instance_uid)) return false;
    if (!dataset.contains(tags::series_instance_uid)) return false;
    if (!dataset.contains(tags::modality)) return false;

    auto modality = dataset.get_string(tags::modality);
    if (modality != "RTSTRUCT") return false;

    if (!dataset.contains(rt_tags::structure_set_label)) return false;
    if (!dataset.contains(rt_tags::structure_set_roi_sequence)) return false;
    if (!dataset.contains(tags::sop_class_uid)) return false;
    if (!dataset.contains(tags::sop_instance_uid)) return false;

    return true;
}

const rt_validation_options& rt_structure_set_iod_validator::options() const noexcept {
    return options_;
}

void rt_structure_set_iod_validator::set_options(const rt_validation_options& options) {
    options_ = options;
}

void rt_structure_set_iod_validator::validate_patient_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type2) {
        check_type2_attribute(dataset, tags::patient_name, "PatientName", findings);
        check_type2_attribute(dataset, tags::patient_id, "PatientID", findings);
        check_type2_attribute(dataset, tags::patient_birth_date, "PatientBirthDate", findings);
        check_type2_attribute(dataset, tags::patient_sex, "PatientSex", findings);
    }
}

void rt_structure_set_iod_validator::validate_general_study_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::study_instance_uid, "StudyInstanceUID", findings);
    }

    if (options_.check_type2) {
        check_type2_attribute(dataset, tags::study_date, "StudyDate", findings);
        check_type2_attribute(dataset, tags::study_time, "StudyTime", findings);
        check_type2_attribute(dataset, tags::study_id, "StudyID", findings);
        check_type2_attribute(dataset, tags::accession_number, "AccessionNumber", findings);
    }
}

void rt_structure_set_iod_validator::validate_rt_series_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::modality, "Modality", findings);
        check_type1_attribute(dataset, tags::series_instance_uid, "SeriesInstanceUID", findings);

        if (dataset.contains(tags::modality)) {
            auto modality = dataset.get_string(tags::modality);
            if (modality != "RTSTRUCT") {
                findings.push_back({
                    validation_severity::error,
                    tags::modality,
                    "Modality must be 'RTSTRUCT' for RT Structure Set, found: " + modality,
                    "RTSTRUCT-ERR-001"
                });
            }
        }
    }
}

void rt_structure_set_iod_validator::validate_structure_set_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Type 1 attributes
    if (options_.check_type1) {
        check_type1_attribute(dataset, rt_tags::structure_set_label, "StructureSetLabel", findings);
        check_type1_attribute(dataset, rt_tags::structure_set_roi_sequence,
                              "StructureSetROISequence", findings);
    }

    // Type 2 attributes
    if (options_.check_type2) {
        check_type2_attribute(dataset, rt_tags::structure_set_date, "StructureSetDate", findings);
        check_type2_attribute(dataset, rt_tags::structure_set_time, "StructureSetTime", findings);
    }

    // ReferencedFrameOfReferenceSequence is important for spatial registration
    if (options_.validate_references) {
        if (!dataset.contains(rt_tags::referenced_frame_of_reference_sequence)) {
            findings.push_back({
                validation_severity::warning,
                rt_tags::referenced_frame_of_reference_sequence,
                "ReferencedFrameOfReferenceSequence not present - structures have no spatial reference",
                "RTSTRUCT-WARN-001"
            });
        }
    }
}

void rt_structure_set_iod_validator::validate_roi_contour_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // ROI Contour Sequence is Type 1
    if (options_.check_type1) {
        check_type1_attribute(dataset, rt_tags::roi_contour_sequence, "ROIContourSequence", findings);
    }

    // ROI Display Color is Type 3 but useful
    if (!dataset.contains(rt_tags::roi_display_color)) {
        findings.push_back({
            validation_severity::info,
            rt_tags::roi_display_color,
            "ROIDisplayColor not present - ROIs will use default colors",
            "RTSTRUCT-INFO-001"
        });
    }
}

void rt_structure_set_iod_validator::validate_rt_roi_observations_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // RT ROI Observations Sequence is Type 1
    if (options_.check_type1) {
        check_type1_attribute(dataset, rt_tags::rt_roi_observations_sequence,
                              "RTROIObservationsSequence", findings);
    }

    // Check for RT ROI Interpreted Type
    if (dataset.contains(rt_tags::rt_roi_observations_sequence)) {
        if (!dataset.contains(rt_tags::rt_roi_interpreted_type)) {
            findings.push_back({
                validation_severity::info,
                rt_tags::rt_roi_interpreted_type,
                "RTROIInterpretedType not specified - ROI semantic meaning not defined",
                "RTSTRUCT-INFO-002"
            });
        }
    }
}

void rt_structure_set_iod_validator::validate_sop_common_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::sop_class_uid, "SOPClassUID", findings);
        check_type1_attribute(dataset, tags::sop_instance_uid, "SOPInstanceUID", findings);
    }

    if (dataset.contains(tags::sop_class_uid)) {
        auto sop_class = dataset.get_string(tags::sop_class_uid);
        if (sop_class != sop_classes::rt_structure_set_storage_uid) {
            findings.push_back({
                validation_severity::error,
                tags::sop_class_uid,
                "SOPClassUID is not RT Structure Set Storage: " + sop_class,
                "RTSTRUCT-ERR-002"
            });
        }
    }
}

void rt_structure_set_iod_validator::check_type1_attribute(
    const dicom_dataset& dataset,
    dicom_tag tag,
    std::string_view name,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(tag)) {
        findings.push_back({
            validation_severity::error,
            tag,
            std::string("Type 1 attribute missing: ") + std::string(name) +
            " (" + tag.to_string() + ")",
            "RTSTRUCT-TYPE1-MISSING"
        });
    } else {
        auto value = dataset.get_string(tag);
        if (value.empty()) {
            findings.push_back({
                validation_severity::error,
                tag,
                std::string("Type 1 attribute has empty value: ") +
                std::string(name) + " (" + tag.to_string() + ")",
                "RTSTRUCT-TYPE1-EMPTY"
            });
        }
    }
}

void rt_structure_set_iod_validator::check_type2_attribute(
    const dicom_dataset& dataset,
    dicom_tag tag,
    std::string_view name,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(tag)) {
        findings.push_back({
            validation_severity::warning,
            tag,
            std::string("Type 2 attribute missing: ") + std::string(name) +
            " (" + tag.to_string() + ")",
            "RTSTRUCT-TYPE2-MISSING"
        });
    }
}

void rt_structure_set_iod_validator::check_roi_consistency(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Check that ROI Contour Sequence items reference valid ROIs from Structure Set ROI Sequence
    // This is a simplified check - full implementation would iterate through sequences
    if (dataset.contains(rt_tags::structure_set_roi_sequence) &&
        dataset.contains(rt_tags::roi_contour_sequence)) {
        // Both sequences present - good
    } else if (dataset.contains(rt_tags::structure_set_roi_sequence) &&
               !dataset.contains(rt_tags::roi_contour_sequence)) {
        findings.push_back({
            validation_severity::warning,
            rt_tags::roi_contour_sequence,
            "StructureSetROISequence present but ROIContourSequence missing - ROIs have no contours",
            "RTSTRUCT-WARN-002"
        });
    }
}

// =============================================================================
// rt_iod_validator (Unified) Implementation
// =============================================================================

rt_iod_validator::rt_iod_validator(const rt_validation_options& options)
    : options_(options) {}

validation_result rt_iod_validator::validate(const dicom_dataset& dataset) const {
    // Detect RT object type from SOP Class UID or Modality
    std::string sop_class;
    if (dataset.contains(tags::sop_class_uid)) {
        sop_class = dataset.get_string(tags::sop_class_uid);
    }

    // Route to appropriate validator
    if (sop_class == sop_classes::rt_plan_storage_uid ||
        sop_class == sop_classes::rt_ion_plan_storage_uid) {
        rt_plan_iod_validator validator(options_);
        return validator.validate(dataset);
    }

    if (sop_class == sop_classes::rt_dose_storage_uid) {
        rt_dose_iod_validator validator(options_);
        return validator.validate(dataset);
    }

    if (sop_class == sop_classes::rt_structure_set_storage_uid) {
        rt_structure_set_iod_validator validator(options_);
        return validator.validate(dataset);
    }

    // Try to detect from Modality if SOP Class not recognized
    if (dataset.contains(tags::modality)) {
        auto modality = dataset.get_string(tags::modality);

        if (modality == "RTPLAN") {
            rt_plan_iod_validator validator(options_);
            return validator.validate(dataset);
        }
        if (modality == "RTDOSE") {
            rt_dose_iod_validator validator(options_);
            return validator.validate(dataset);
        }
        if (modality == "RTSTRUCT") {
            rt_structure_set_iod_validator validator(options_);
            return validator.validate(dataset);
        }
    }

    // Unknown RT type
    validation_result result;
    result.is_valid = false;
    result.findings.push_back({
        validation_severity::error,
        tags::sop_class_uid,
        "Unable to determine RT object type from SOPClassUID or Modality",
        "RT-ERR-001"
    });

    return result;
}

bool rt_iod_validator::quick_check(const dicom_dataset& dataset) const {
    // Try each RT type
    if (rt_plan_iod_validator(options_).quick_check(dataset)) return true;
    if (rt_dose_iod_validator(options_).quick_check(dataset)) return true;
    if (rt_structure_set_iod_validator(options_).quick_check(dataset)) return true;

    return false;
}

const rt_validation_options& rt_iod_validator::options() const noexcept {
    return options_;
}

void rt_iod_validator::set_options(const rt_validation_options& options) {
    options_ = options;
}

// =============================================================================
// Convenience Functions
// =============================================================================

validation_result validate_rt_plan_iod(const dicom_dataset& dataset) {
    rt_plan_iod_validator validator;
    return validator.validate(dataset);
}

validation_result validate_rt_dose_iod(const dicom_dataset& dataset) {
    rt_dose_iod_validator validator;
    return validator.validate(dataset);
}

validation_result validate_rt_structure_set_iod(const dicom_dataset& dataset) {
    rt_structure_set_iod_validator validator;
    return validator.validate(dataset);
}

validation_result validate_rt_iod(const dicom_dataset& dataset) {
    rt_iod_validator validator;
    return validator.validate(dataset);
}

bool is_valid_rt_plan_dataset(const dicom_dataset& dataset) {
    rt_plan_iod_validator validator;
    return validator.quick_check(dataset);
}

bool is_valid_rt_dose_dataset(const dicom_dataset& dataset) {
    rt_dose_iod_validator validator;
    return validator.quick_check(dataset);
}

bool is_valid_rt_structure_set_dataset(const dicom_dataset& dataset) {
    rt_structure_set_iod_validator validator;
    return validator.quick_check(dataset);
}

bool is_valid_rt_dataset(const dicom_dataset& dataset) {
    rt_iod_validator validator;
    return validator.quick_check(dataset);
}

}  // namespace pacs::services::validation
