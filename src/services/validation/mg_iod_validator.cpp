/**
 * @file mg_iod_validator.cpp
 * @brief Implementation of Digital Mammography X-Ray Image IOD Validator
 */

#include "pacs/services/validation/mg_iod_validator.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/services/sop_classes/mg_storage.hpp"

#include <sstream>

namespace pacs::services::validation {

using namespace pacs::core;
using namespace pacs::services::sop_classes;

// =============================================================================
// Mammography-Specific DICOM Tags
// =============================================================================

namespace mg_tags {

// Laterality Tags
inline constexpr dicom_tag laterality{0x0020, 0x0060};
inline constexpr dicom_tag image_laterality{0x0020, 0x0062};

// Mammography Series Module
inline constexpr dicom_tag modality{0x0008, 0x0060};
inline constexpr dicom_tag request_attributes_sequence{0x0040, 0x0275};

// Mammography Image Module
inline constexpr dicom_tag view_position{0x0018, 0x5101};
inline constexpr dicom_tag view_code_sequence{0x0054, 0x0220};
inline constexpr dicom_tag image_type{0x0008, 0x0008};
inline constexpr dicom_tag presentation_intent_type{0x0008, 0x0068};
inline constexpr dicom_tag partial_view{0x0028, 0x1350};
inline constexpr dicom_tag partial_view_description{0x0028, 0x1351};
inline constexpr dicom_tag partial_view_code_sequence{0x0028, 0x1352};

// DX Anatomy Imaged Module
inline constexpr dicom_tag body_part_examined{0x0018, 0x0015};
inline constexpr dicom_tag anatomic_region_sequence{0x0008, 0x2218};

// X-Ray Acquisition Dose Module
inline constexpr dicom_tag compression_force{0x0018, 0x11A2};
inline constexpr dicom_tag body_part_thickness{0x0018, 0x11A0};
inline constexpr dicom_tag measured_ap_dimension{0x0010, 0x1023};
inline constexpr dicom_tag entrance_dose{0x0040, 0x0302};
inline constexpr dicom_tag entrance_dose_derivation{0x0040, 0x0303};
inline constexpr dicom_tag organ_dose{0x0040, 0x0316};
inline constexpr dicom_tag half_value_layer{0x0040, 0x0314};
inline constexpr dicom_tag relative_x_ray_exposure{0x0018, 0x1405};

// DX Detector Module
inline constexpr dicom_tag detector_type{0x0018, 0x7004};
inline constexpr dicom_tag detector_id{0x0018, 0x700A};
inline constexpr dicom_tag imager_pixel_spacing{0x0018, 0x1164};

// Exposure Module
inline constexpr dicom_tag kvp{0x0018, 0x0060};
inline constexpr dicom_tag exposure_time{0x0018, 0x1150};
inline constexpr dicom_tag exposure{0x0018, 0x1152};
inline constexpr dicom_tag exposure_in_uas{0x0018, 0x1153};
inline constexpr dicom_tag tube_current{0x0018, 0x1151};
inline constexpr dicom_tag anode_target_material{0x0018, 0x1191};
inline constexpr dicom_tag filter_material{0x0018, 0x7050};
inline constexpr dicom_tag filter_thickness{0x0018, 0x7052};
inline constexpr dicom_tag focal_spot{0x0018, 0x1190};

// Breast Implant Module
inline constexpr dicom_tag breast_implant_present{0x0028, 0x1300};

// Image Pixel Module
inline constexpr dicom_tag pixel_intensity_relationship{0x0028, 0x1040};
inline constexpr dicom_tag pixel_intensity_relationship_sign{0x0028, 0x1041};

}  // namespace mg_tags

// =============================================================================
// mg_iod_validator Implementation
// =============================================================================

mg_iod_validator::mg_iod_validator(const mg_validation_options& options)
    : options_(options) {}

validation_result mg_iod_validator::validate(const dicom_dataset& dataset) const {
    validation_result result;
    result.is_valid = true;

    // Validate standard modules
    if (options_.check_type1 || options_.check_type2) {
        validate_patient_module(dataset, result.findings);
        validate_general_study_module(dataset, result.findings);
        validate_general_series_module(dataset, result.findings);
        validate_sop_common_module(dataset, result.findings);
    }

    // Validate mammography series module
    validate_mammography_series_module(dataset, result.findings);

    if (options_.validate_pixel_data) {
        validate_image_pixel_module(dataset, result.findings);
    }

    if (options_.validate_mg_specific) {
        validate_mammography_image_module(dataset, result.findings);
        validate_dx_anatomy_imaged_module(dataset, result.findings);
        validate_dx_detector_module(dataset, result.findings);
    }

    if (options_.validate_compression || options_.validate_dose_parameters) {
        validate_acquisition_dose_module(dataset, result.findings);
    }

    if (options_.validate_implant_attributes) {
        validate_breast_implant_module(dataset, result.findings);
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

validation_result
mg_iod_validator::validate_for_presentation(const dicom_dataset& dataset) const {
    auto result = validate(dataset);

    // Additional For Presentation validation
    if (options_.validate_presentation_requirements) {
        validate_voi_lut_module(dataset, result.findings);

        // Check Presentation Intent Type
        if (dataset.contains(mg_tags::presentation_intent_type)) {
            auto intent = dataset.get_string(mg_tags::presentation_intent_type);
            if (intent != "FOR PRESENTATION") {
                result.findings.push_back({
                    validation_severity::error,
                    mg_tags::presentation_intent_type,
                    "Presentation Intent Type should be 'FOR PRESENTATION' "
                    "for this SOP Class, found: " + intent,
                    "MG-ERR-010"
                });
            }
        }
    }

    // Re-check validity
    for (const auto& finding : result.findings) {
        if (finding.severity == validation_severity::error) {
            result.is_valid = false;
            break;
        }
    }

    return result;
}

validation_result
mg_iod_validator::validate_for_processing(const dicom_dataset& dataset) const {
    auto result = validate(dataset);

    // Additional For Processing validation
    if (options_.validate_processing_requirements) {
        // Check Presentation Intent Type
        if (dataset.contains(mg_tags::presentation_intent_type)) {
            auto intent = dataset.get_string(mg_tags::presentation_intent_type);
            if (intent != "FOR PROCESSING") {
                result.findings.push_back({
                    validation_severity::error,
                    mg_tags::presentation_intent_type,
                    "Presentation Intent Type should be 'FOR PROCESSING' "
                    "for this SOP Class, found: " + intent,
                    "MG-ERR-011"
                });
            }
        }

        // For Processing images should have linear pixel intensity relationship
        if (dataset.contains(mg_tags::pixel_intensity_relationship)) {
            auto relationship = dataset.get_string(mg_tags::pixel_intensity_relationship);
            if (relationship != "LIN") {
                result.findings.push_back({
                    validation_severity::info,
                    mg_tags::pixel_intensity_relationship,
                    "For Processing mammography images typically have linear (LIN) "
                    "pixel intensity relationship, found: " + relationship,
                    "MG-INFO-002"
                });
            }
        }
    }

    // Re-check validity
    for (const auto& finding : result.findings) {
        if (finding.severity == validation_severity::error) {
            result.is_valid = false;
            break;
        }
    }

    return result;
}

validation_result
mg_iod_validator::validate_laterality(const dicom_dataset& dataset) const {
    validation_result result;
    result.is_valid = true;

    check_laterality_consistency(dataset, result.findings);

    for (const auto& finding : result.findings) {
        if (finding.severity == validation_severity::error) {
            result.is_valid = false;
            break;
        }
    }

    return result;
}

validation_result
mg_iod_validator::validate_view_position(const dicom_dataset& dataset) const {
    validation_result result;
    result.is_valid = true;

    check_view_position_validity(dataset, result.findings);

    for (const auto& finding : result.findings) {
        if (finding.severity == validation_severity::error) {
            result.is_valid = false;
            break;
        }
    }

    return result;
}

validation_result
mg_iod_validator::validate_compression_force(const dicom_dataset& dataset) const {
    validation_result result;
    result.is_valid = true;

    check_compression_force_range(dataset, result.findings);

    for (const auto& finding : result.findings) {
        if (finding.severity == validation_severity::error) {
            result.is_valid = false;
            break;
        }
    }

    return result;
}

bool mg_iod_validator::quick_check(const dicom_dataset& dataset) const {
    // Check only critical Type 1 attributes for quick validation

    // General Study Module Type 1
    if (!dataset.contains(tags::study_instance_uid)) return false;

    // General Series Module Type 1
    if (!dataset.contains(tags::modality)) return false;
    if (!dataset.contains(tags::series_instance_uid)) return false;

    // Check modality is MG
    auto modality = dataset.get_string(tags::modality);
    if (modality != "MG") return false;

    // Image Pixel Module Type 1
    if (!dataset.contains(tags::samples_per_pixel)) return false;
    if (!dataset.contains(tags::photometric_interpretation)) return false;
    if (!dataset.contains(tags::rows)) return false;
    if (!dataset.contains(tags::columns)) return false;
    if (!dataset.contains(tags::bits_allocated)) return false;
    if (!dataset.contains(tags::bits_stored)) return false;
    if (!dataset.contains(tags::high_bit)) return false;
    if (!dataset.contains(tags::pixel_representation)) return false;
    if (!dataset.contains(tags::pixel_data)) return false;

    // SOP Common Module Type 1
    if (!dataset.contains(tags::sop_class_uid)) return false;
    if (!dataset.contains(tags::sop_instance_uid)) return false;

    // Mammography-specific: Laterality should be present
    if (!dataset.contains(mg_tags::laterality) &&
        !dataset.contains(mg_tags::image_laterality)) {
        return false;
    }

    return true;
}

const mg_validation_options& mg_iod_validator::options() const noexcept {
    return options_;
}

void mg_iod_validator::set_options(const mg_validation_options& options) {
    options_ = options;
}

// =============================================================================
// Module Validation Methods
// =============================================================================

void mg_iod_validator::validate_patient_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Patient Module - All attributes are Type 2
    if (options_.check_type2) {
        check_type2_attribute(dataset, tags::patient_name, "PatientName", findings);
        check_type2_attribute(dataset, tags::patient_id, "PatientID", findings);
        check_type2_attribute(dataset, tags::patient_birth_date, "PatientBirthDate", findings);
        check_type2_attribute(dataset, tags::patient_sex, "PatientSex", findings);
    }
}

void mg_iod_validator::validate_general_study_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Type 1
    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::study_instance_uid, "StudyInstanceUID", findings);
    }

    // Type 2
    if (options_.check_type2) {
        check_type2_attribute(dataset, tags::study_date, "StudyDate", findings);
        check_type2_attribute(dataset, tags::study_time, "StudyTime", findings);
        check_type2_attribute(dataset, tags::referring_physician_name, "ReferringPhysicianName", findings);
        check_type2_attribute(dataset, tags::study_id, "StudyID", findings);
        check_type2_attribute(dataset, tags::accession_number, "AccessionNumber", findings);
    }
}

void mg_iod_validator::validate_general_series_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Type 1
    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::modality, "Modality", findings);
        check_type1_attribute(dataset, tags::series_instance_uid, "SeriesInstanceUID", findings);

        // Special check: Modality must be "MG"
        check_modality(dataset, findings);
    }

    // Type 2
    if (options_.check_type2) {
        check_type2_attribute(dataset, tags::series_number, "SeriesNumber", findings);
    }
}

void mg_iod_validator::validate_mammography_series_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Mammography Series Module includes Request Attributes Sequence (0040,0275)
    // which is Type 3, so no validation required but informational

    if (options_.check_conditional) {
        // The Request Attributes Sequence is useful for workflow integration
        if (!dataset.contains(mg_tags::request_attributes_sequence)) {
            findings.push_back({
                validation_severity::info,
                mg_tags::request_attributes_sequence,
                "Request Attributes Sequence not present - may limit workflow integration",
                "MG-INFO-001"
            });
        }
    }
}

void mg_iod_validator::validate_mammography_image_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Image Type (0008,0008) - Type 1
    if (options_.check_type1) {
        check_type1_attribute(dataset, mg_tags::image_type, "ImageType", findings);

        if (dataset.contains(mg_tags::image_type)) {
            auto image_type = dataset.get_string(mg_tags::image_type);
            // First value should be ORIGINAL or DERIVED
            if (image_type.find("ORIGINAL") == std::string::npos &&
                image_type.find("DERIVED") == std::string::npos) {
                findings.push_back({
                    validation_severity::warning,
                    mg_tags::image_type,
                    "Image Type first value should be ORIGINAL or DERIVED",
                    "MG-WARN-002"
                });
            }
        }
    }

    // Validate laterality if enabled
    if (options_.validate_laterality) {
        check_laterality_consistency(dataset, findings);
    }

    // Validate view position if enabled
    if (options_.validate_view_position) {
        check_view_position_validity(dataset, findings);
    }

    // Partial View (0028,1350) - Type 3
    if (dataset.contains(mg_tags::partial_view)) {
        auto partial = dataset.get_string(mg_tags::partial_view);
        if (partial == "YES" && !dataset.contains(mg_tags::partial_view_description) &&
            !dataset.contains(mg_tags::partial_view_code_sequence)) {
            findings.push_back({
                validation_severity::info,
                mg_tags::partial_view_description,
                "Partial View is YES but no description or code sequence provided",
                "MG-INFO-003"
            });
        }
    }
}

void mg_iod_validator::validate_dx_anatomy_imaged_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Body Part Examined (0018,0015) - Type 2
    if (options_.check_type2) {
        check_type2_attribute(dataset, mg_tags::body_part_examined, "BodyPartExamined", findings);

        if (dataset.contains(mg_tags::body_part_examined)) {
            auto body_part = dataset.get_string(mg_tags::body_part_examined);
            if (body_part != "BREAST") {
                findings.push_back({
                    validation_severity::warning,
                    mg_tags::body_part_examined,
                    "Body Part Examined should be 'BREAST' for mammography, found: " + body_part,
                    "MG-WARN-003"
                });
            }
        }
    }

    // Anatomic Region Sequence (0008,2218) - Type 1C
    if (options_.check_conditional) {
        if (!dataset.contains(mg_tags::body_part_examined) &&
            !dataset.contains(mg_tags::anatomic_region_sequence)) {
            findings.push_back({
                validation_severity::warning,
                mg_tags::anatomic_region_sequence,
                "Neither Body Part Examined nor Anatomic Region Sequence present - "
                "anatomy information may be insufficient for clinical use",
                "MG-WARN-004"
            });
        }
    }
}

void mg_iod_validator::validate_dx_detector_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Detector Type (0018,7004) - Type 2
    if (options_.check_type2) {
        check_type2_attribute(dataset, mg_tags::detector_type, "DetectorType", findings);

        if (dataset.contains(mg_tags::detector_type)) {
            auto type = dataset.get_string(mg_tags::detector_type);
            // Mammography typically uses DIRECT (a-Se) or SCINTILLATOR (CsI)
            if (type != "DIRECT" && type != "SCINTILLATOR" && type != "STORAGE") {
                findings.push_back({
                    validation_severity::info,
                    mg_tags::detector_type,
                    "Unusual Detector Type for mammography: " + type +
                    " (typical: DIRECT, SCINTILLATOR)",
                    "MG-INFO-004"
                });
            }
        }
    }

    // Imager Pixel Spacing (0018,1164) - Type 1 for DX/MG
    if (options_.check_type1) {
        check_type1_attribute(dataset, mg_tags::imager_pixel_spacing,
                              "ImagerPixelSpacing", findings);

        // Mammography typically has very fine pixel spacing (< 0.1 mm)
        // This is informational only
    }
}

void mg_iod_validator::validate_acquisition_dose_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Compression Force (0018,11A2) - Type 3 but important for mammography
    if (options_.validate_compression) {
        check_compression_force_range(dataset, findings);
    }

    // Body Part Thickness (0018,11A0) - Type 3
    if (options_.validate_dose_parameters) {
        if (dataset.contains(mg_tags::body_part_thickness)) {
            auto thickness = dataset.get_numeric<double>(mg_tags::body_part_thickness);
            if (thickness && !is_valid_compressed_breast_thickness(*thickness)) {
                findings.push_back({
                    validation_severity::warning,
                    mg_tags::body_part_thickness,
                    "Compressed breast thickness outside typical range (10-150mm): " +
                    std::to_string(*thickness) + " mm",
                    "MG-WARN-007"
                });
            }
        } else {
            // Recommend including thickness for quality assessment
            findings.push_back({
                validation_severity::info,
                mg_tags::body_part_thickness,
                "Compressed breast thickness not documented - "
                "recommended for dose assessment and quality control",
                "MG-INFO-005"
            });
        }

        // KVP (0018,0060) - Type 3 but important for dose
        if (!dataset.contains(mg_tags::kvp)) {
            findings.push_back({
                validation_severity::info,
                mg_tags::kvp,
                "KVP not documented - recommended for dose assessment",
                "MG-INFO-006"
            });
        }

        // Entrance Dose (0040,0302) - Type 3
        // Organ Dose (0040,0316) - Type 3 but important for breast dose tracking
    }
}

void mg_iod_validator::validate_image_pixel_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Type 1 attributes
    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::samples_per_pixel, "SamplesPerPixel", findings);
        check_type1_attribute(dataset, tags::photometric_interpretation, "PhotometricInterpretation", findings);
        check_type1_attribute(dataset, tags::rows, "Rows", findings);
        check_type1_attribute(dataset, tags::columns, "Columns", findings);
        check_type1_attribute(dataset, tags::bits_allocated, "BitsAllocated", findings);
        check_type1_attribute(dataset, tags::bits_stored, "BitsStored", findings);
        check_type1_attribute(dataset, tags::high_bit, "HighBit", findings);
        check_type1_attribute(dataset, tags::pixel_representation, "PixelRepresentation", findings);
        check_type1_attribute(dataset, tags::pixel_data, "PixelData", findings);
    }

    // Validate pixel data consistency
    if (options_.validate_pixel_data) {
        check_pixel_data_consistency(dataset, findings);
        check_photometric_interpretation(dataset, findings);
    }
}

void mg_iod_validator::validate_voi_lut_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Window Center (0028,1050) and Window Width (0028,1051) - Type 1C for For Presentation
    if (options_.check_conditional) {
        bool has_window = dataset.contains(tags::window_center) &&
                         dataset.contains(tags::window_width);

        constexpr dicom_tag voi_lut_sequence{0x0028, 0x3010};
        bool has_voi_lut = dataset.contains(voi_lut_sequence);

        if (!has_window && !has_voi_lut) {
            findings.push_back({
                validation_severity::warning,
                tags::window_center,
                "For Presentation mammography images should have Window Center/Width "
                "or VOI LUT Sequence for proper display",
                "MG-WARN-008"
            });
        }
    }
}

void mg_iod_validator::validate_sop_common_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Type 1
    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::sop_class_uid, "SOPClassUID", findings);
        check_type1_attribute(dataset, tags::sop_instance_uid, "SOPInstanceUID", findings);
    }

    // Validate SOP Class UID is a mammography storage class
    if (dataset.contains(tags::sop_class_uid)) {
        auto sop_class = dataset.get_string(tags::sop_class_uid);
        if (!is_mg_storage_sop_class(sop_class)) {
            findings.push_back({
                validation_severity::error,
                tags::sop_class_uid,
                "SOPClassUID is not a recognized Mammography Storage SOP Class: " + sop_class,
                "MG-ERR-001"
            });
        }
    }
}

void mg_iod_validator::validate_breast_implant_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Breast Implant Present (0028,1300) - Type 3
    if (dataset.contains(mg_tags::breast_implant_present)) {
        auto implant = dataset.get_string(mg_tags::breast_implant_present);
        if (implant != "YES" && implant != "NO") {
            findings.push_back({
                validation_severity::warning,
                mg_tags::breast_implant_present,
                "Breast Implant Present should be 'YES' or 'NO', found: " + implant,
                "MG-WARN-009"
            });
        }

        // If implant is present, check for implant displaced views
        if (implant == "YES") {
            // Informational: implant displacement technique (Eklund) may be needed
            if (dataset.contains(mg_tags::view_position)) {
                auto view = dataset.get_string(mg_tags::view_position);
                auto parsed_view = parse_mg_view_position(view);
                if (parsed_view != mg_view_position::implant &&
                    parsed_view != mg_view_position::id) {
                    findings.push_back({
                        validation_severity::info,
                        mg_tags::view_position,
                        "Breast implant present but view is not implant-displaced (ID). "
                        "Implant displacement technique (Eklund) may improve visualization.",
                        "MG-INFO-007"
                    });
                }
            }
        }
    }
}

// =============================================================================
// Attribute Validation Helpers
// =============================================================================

void mg_iod_validator::check_type1_attribute(
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
            "MG-TYPE1-MISSING"
        });
    } else {
        // Type 1 must have a value (cannot be empty)
        auto value = dataset.get_string(tag);
        if (value.empty()) {
            findings.push_back({
                validation_severity::error,
                tag,
                std::string("Type 1 attribute has empty value: ") +
                std::string(name) + " (" + tag.to_string() + ")",
                "MG-TYPE1-EMPTY"
            });
        }
    }
}

void mg_iod_validator::check_type2_attribute(
    const dicom_dataset& dataset,
    dicom_tag tag,
    std::string_view name,
    std::vector<validation_finding>& findings) const {

    // Type 2 must be present but can be empty
    if (!dataset.contains(tag)) {
        findings.push_back({
            validation_severity::warning,
            tag,
            std::string("Type 2 attribute missing: ") + std::string(name) +
            " (" + tag.to_string() + ")",
            "MG-TYPE2-MISSING"
        });
    }
}

void mg_iod_validator::check_modality(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(tags::modality)) {
        return;  // Already reported as Type 1 missing
    }

    auto modality = dataset.get_string(tags::modality);
    if (modality != "MG") {
        findings.push_back({
            validation_severity::error,
            tags::modality,
            "Modality must be 'MG' for mammography images, found: " + modality,
            "MG-ERR-002"
        });
    }
}

void mg_iod_validator::check_laterality_consistency(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Check Laterality (0020,0060) - Series level
    bool has_series_laterality = dataset.contains(mg_tags::laterality);
    // Check Image Laterality (0020,0062) - Image level
    bool has_image_laterality = dataset.contains(mg_tags::image_laterality);

    // At least one should be present for mammography
    if (!has_series_laterality && !has_image_laterality) {
        findings.push_back({
            validation_severity::error,
            mg_tags::laterality,
            "Neither Laterality (0020,0060) nor Image Laterality (0020,0062) is present. "
            "Breast laterality must be specified for mammography images.",
            "MG-ERR-003"
        });
        return;
    }

    // Validate Laterality value if present
    if (has_series_laterality) {
        auto lat = dataset.get_string(mg_tags::laterality);
        if (!is_valid_breast_laterality(lat)) {
            findings.push_back({
                validation_severity::error,
                mg_tags::laterality,
                "Invalid Laterality value: '" + lat +
                "'. Must be 'L' (left), 'R' (right), or 'B' (bilateral).",
                "MG-ERR-004"
            });
        }
    }

    // Validate Image Laterality value if present
    if (has_image_laterality) {
        auto img_lat = dataset.get_string(mg_tags::image_laterality);
        if (!is_valid_breast_laterality(img_lat)) {
            findings.push_back({
                validation_severity::error,
                mg_tags::image_laterality,
                "Invalid Image Laterality value: '" + img_lat +
                "'. Must be 'L' (left), 'R' (right), or 'B' (bilateral).",
                "MG-ERR-005"
            });
        }
    }

    // Check consistency between series and image laterality if both present
    if (has_series_laterality && has_image_laterality) {
        auto series_lat = dataset.get_string(mg_tags::laterality);
        auto image_lat = dataset.get_string(mg_tags::image_laterality);

        if (series_lat != image_lat) {
            findings.push_back({
                validation_severity::warning,
                mg_tags::image_laterality,
                "Laterality mismatch: Series Laterality is '" + series_lat +
                "' but Image Laterality is '" + image_lat + "'.",
                "MG-WARN-001"
            });
        }
    }
}

void mg_iod_validator::check_view_position_validity(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // View Position (0018,5101) - Type 2 for mammography
    if (!dataset.contains(mg_tags::view_position)) {
        findings.push_back({
            validation_severity::warning,
            mg_tags::view_position,
            "View Position (0018,5101) is not present. "
            "This attribute should be specified for mammography images.",
            "MG-WARN-005"
        });
        return;
    }

    auto view_str = dataset.get_string(mg_tags::view_position);
    if (view_str.empty()) {
        findings.push_back({
            validation_severity::warning,
            mg_tags::view_position,
            "View Position is present but empty.",
            "MG-WARN-006"
        });
        return;
    }

    // Parse and validate view position
    auto view = parse_mg_view_position(view_str);
    if (view == mg_view_position::other) {
        // Check if it's a valid view we just don't recognize
        auto valid_views = get_valid_mg_view_positions();
        std::string valid_list;
        for (size_t i = 0; i < valid_views.size(); ++i) {
            if (i > 0) valid_list += ", ";
            valid_list += std::string(valid_views[i]);
        }

        findings.push_back({
            validation_severity::warning,
            mg_tags::view_position,
            "Unrecognized View Position: '" + view_str +
            "'. Common mammography views include: " + valid_list,
            "MG-WARN-010"
        });
    }

    // Additional validation: check laterality-view consistency
    if (dataset.contains(mg_tags::laterality) || dataset.contains(mg_tags::image_laterality)) {
        auto lat_str = dataset.contains(mg_tags::image_laterality)
            ? dataset.get_string(mg_tags::image_laterality)
            : dataset.get_string(mg_tags::laterality);

        auto laterality = parse_breast_laterality(lat_str);

        if (!is_valid_laterality_view_combination(laterality, view)) {
            findings.push_back({
                validation_severity::warning,
                mg_tags::view_position,
                "Unusual laterality-view combination: " + lat_str + " / " + view_str,
                "MG-WARN-011"
            });
        }
    }
}

void mg_iod_validator::check_compression_force_range(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(mg_tags::compression_force)) {
        // Compression force is Type 3 but important for mammography QC
        findings.push_back({
            validation_severity::info,
            mg_tags::compression_force,
            "Compression Force (0018,11A2) is not present. "
            "This information is valuable for quality control and patient safety.",
            "MG-INFO-008"
        });
        return;
    }

    auto force = dataset.get_numeric<double>(mg_tags::compression_force);
    if (!force) {
        findings.push_back({
            validation_severity::warning,
            mg_tags::compression_force,
            "Compression Force is present but could not be parsed as a number.",
            "MG-WARN-012"
        });
        return;
    }

    auto [min_typical, max_typical] = get_typical_compression_force_range();

    if (!is_valid_compression_force(*force)) {
        findings.push_back({
            validation_severity::warning,
            mg_tags::compression_force,
            "Compression Force (" + std::to_string(*force) +
            " N) is outside the typical range (20-300 N). "
            "This may indicate a measurement error or non-standard technique.",
            "MG-WARN-013"
        });
    } else if (*force < min_typical || *force > max_typical) {
        findings.push_back({
            validation_severity::info,
            mg_tags::compression_force,
            "Compression Force (" + std::to_string(*force) +
            " N) is outside the typical screening range (" +
            std::to_string(min_typical) + "-" + std::to_string(max_typical) + " N).",
            "MG-INFO-009"
        });
    }
}

void mg_iod_validator::check_pixel_data_consistency(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Check BitsStored <= BitsAllocated
    auto bits_allocated = dataset.get_numeric<uint16_t>(tags::bits_allocated);
    auto bits_stored = dataset.get_numeric<uint16_t>(tags::bits_stored);
    auto high_bit = dataset.get_numeric<uint16_t>(tags::high_bit);

    if (bits_allocated && bits_stored) {
        if (*bits_stored > *bits_allocated) {
            findings.push_back({
                validation_severity::error,
                tags::bits_stored,
                "BitsStored (" + std::to_string(*bits_stored) +
                ") exceeds BitsAllocated (" + std::to_string(*bits_allocated) + ")",
                "MG-ERR-006"
            });
        }

        // Mammography typically uses 12-16 bits stored for high dynamic range
        if (*bits_stored < 12 || *bits_stored > 16) {
            findings.push_back({
                validation_severity::info,
                tags::bits_stored,
                "Mammography images typically use 12-16 bits, found: " +
                std::to_string(*bits_stored),
                "MG-INFO-010"
            });
        }
    }

    // Check HighBit == BitsStored - 1
    if (bits_stored && high_bit) {
        if (*high_bit != *bits_stored - 1) {
            findings.push_back({
                validation_severity::warning,
                tags::high_bit,
                "HighBit (" + std::to_string(*high_bit) +
                ") should typically be BitsStored - 1 (" +
                std::to_string(*bits_stored - 1) + ")",
                "MG-WARN-014"
            });
        }
    }

    // Mammography must be grayscale - check SamplesPerPixel
    auto samples = dataset.get_numeric<uint16_t>(tags::samples_per_pixel);
    if (samples && *samples != 1) {
        findings.push_back({
            validation_severity::error,
            tags::samples_per_pixel,
            "Mammography images must be grayscale (SamplesPerPixel = 1), found: " +
            std::to_string(*samples),
            "MG-ERR-007"
        });
    }
}

void mg_iod_validator::check_photometric_interpretation(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(tags::photometric_interpretation)) {
        return;  // Already reported as missing
    }

    auto photometric = dataset.get_string(tags::photometric_interpretation);
    if (photometric != "MONOCHROME1" && photometric != "MONOCHROME2") {
        findings.push_back({
            validation_severity::error,
            tags::photometric_interpretation,
            "Mammography images must use MONOCHROME1 or MONOCHROME2, found: " + photometric,
            "MG-ERR-008"
        });
    }
}

// =============================================================================
// Convenience Functions
// =============================================================================

validation_result validate_mg_iod(const dicom_dataset& dataset) {
    mg_iod_validator validator;
    return validator.validate(dataset);
}

bool is_valid_mg_dataset(const dicom_dataset& dataset) {
    mg_iod_validator validator;
    return validator.quick_check(dataset);
}

bool is_for_presentation_mg(const dicom_dataset& dataset) {
    if (!dataset.contains(tags::sop_class_uid)) {
        return false;
    }
    auto sop_class = dataset.get_string(tags::sop_class_uid);
    return is_mg_for_presentation_sop_class(sop_class);
}

bool is_for_processing_mg(const dicom_dataset& dataset) {
    if (!dataset.contains(tags::sop_class_uid)) {
        return false;
    }
    auto sop_class = dataset.get_string(tags::sop_class_uid);
    return is_mg_for_processing_sop_class(sop_class);
}

bool has_breast_implant(const dicom_dataset& dataset) {
    constexpr dicom_tag breast_implant_present{0x0028, 0x1300};
    if (!dataset.contains(breast_implant_present)) {
        return false;
    }
    auto value = dataset.get_string(breast_implant_present);
    return value == "YES";
}

bool is_screening_mammogram(const dicom_dataset& dataset) {
    constexpr dicom_tag view_position{0x0018, 0x5101};
    if (!dataset.contains(view_position)) {
        return false;
    }
    auto view_str = dataset.get_string(view_position);
    auto view = parse_mg_view_position(view_str);
    return is_screening_view(view);
}

}  // namespace pacs::services::validation
