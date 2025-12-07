/**
 * @file dx_iod_validator.cpp
 * @brief Implementation of Digital X-Ray Image IOD Validator
 */

#include "pacs/services/validation/dx_iod_validator.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/services/sop_classes/dx_storage.hpp"

#include <sstream>

namespace pacs::services::validation {

using namespace pacs::core;

// =============================================================================
// DX-Specific DICOM Tags
// =============================================================================

namespace dx_tags {

// DX Anatomy Imaged Module
inline constexpr dicom_tag body_part_examined{0x0018, 0x0015};
inline constexpr dicom_tag anatomic_region_sequence{0x0008, 0x2218};
inline constexpr dicom_tag primary_anatomic_structure_sequence{0x0008, 0x2228};

// DX Positioning Module
inline constexpr dicom_tag view_position{0x0018, 0x5101};
inline constexpr dicom_tag view_code_sequence{0x0054, 0x0220};
inline constexpr dicom_tag patient_orientation{0x0020, 0x0020};
inline constexpr dicom_tag distance_source_to_detector{0x0018, 0x1110};
inline constexpr dicom_tag distance_source_to_patient{0x0018, 0x1111};

// DX Detector Module
inline constexpr dicom_tag detector_type{0x0018, 0x7004};
inline constexpr dicom_tag detector_configuration{0x0018, 0x7005};
inline constexpr dicom_tag detector_id{0x0018, 0x700A};
inline constexpr dicom_tag detector_description{0x0018, 0x7006};
inline constexpr dicom_tag field_of_view_shape{0x0018, 0x1147};
inline constexpr dicom_tag imager_pixel_spacing{0x0018, 0x1164};

// DX Image Module
inline constexpr dicom_tag image_type{0x0008, 0x0008};
inline constexpr dicom_tag acquisition_device_processing_description{0x0018, 0x1400};
inline constexpr dicom_tag acquisition_device_processing_code{0x0018, 0x1401};
inline constexpr dicom_tag pixel_intensity_relationship{0x0028, 0x1040};
inline constexpr dicom_tag pixel_intensity_relationship_sign{0x0028, 0x1041};
inline constexpr dicom_tag presentation_intent_type{0x0008, 0x0068};

// Exposure Module
inline constexpr dicom_tag kvp{0x0018, 0x0060};
inline constexpr dicom_tag exposure_time{0x0018, 0x1150};
inline constexpr dicom_tag exposure{0x0018, 0x1152};
inline constexpr dicom_tag exposure_in_uas{0x0018, 0x1153};

}  // namespace dx_tags

// =============================================================================
// dx_iod_validator Implementation
// =============================================================================

dx_iod_validator::dx_iod_validator(const dx_validation_options& options)
    : options_(options) {}

validation_result dx_iod_validator::validate(const dicom_dataset& dataset) const {
    validation_result result;
    result.is_valid = true;

    // Validate mandatory modules
    if (options_.check_type1 || options_.check_type2) {
        validate_patient_module(dataset, result.findings);
        validate_general_study_module(dataset, result.findings);
        validate_general_series_module(dataset, result.findings);
        validate_sop_common_module(dataset, result.findings);
    }

    if (options_.validate_pixel_data) {
        validate_image_pixel_module(dataset, result.findings);
    }

    if (options_.validate_dx_specific) {
        validate_dx_image_module(dataset, result.findings);
        validate_dx_detector_module(dataset, result.findings);
    }

    if (options_.validate_anatomy) {
        validate_dx_anatomy_imaged_module(dataset, result.findings);
        validate_dx_positioning_module(dataset, result.findings);
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
dx_iod_validator::validate_for_presentation(const dicom_dataset& dataset) const {
    auto result = validate(dataset);

    // Additional For Presentation validation
    if (options_.validate_presentation_requirements) {
        validate_voi_lut_module(dataset, result.findings);

        // Check Presentation Intent Type
        if (dataset.contains(dx_tags::presentation_intent_type)) {
            auto intent = dataset.get_string(dx_tags::presentation_intent_type);
            if (intent != "FOR PRESENTATION") {
                result.findings.push_back({
                    validation_severity::error,
                    dx_tags::presentation_intent_type,
                    "Presentation Intent Type should be 'FOR PRESENTATION' "
                    "for this SOP Class, found: " + intent,
                    "DX-ERR-010"
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
dx_iod_validator::validate_for_processing(const dicom_dataset& dataset) const {
    auto result = validate(dataset);

    // Additional For Processing validation
    if (options_.validate_processing_requirements) {
        // Check Presentation Intent Type
        if (dataset.contains(dx_tags::presentation_intent_type)) {
            auto intent = dataset.get_string(dx_tags::presentation_intent_type);
            if (intent != "FOR PROCESSING") {
                result.findings.push_back({
                    validation_severity::error,
                    dx_tags::presentation_intent_type,
                    "Presentation Intent Type should be 'FOR PROCESSING' "
                    "for this SOP Class, found: " + intent,
                    "DX-ERR-011"
                });
            }
        }

        // For Processing images should have linear pixel intensity relationship
        if (dataset.contains(dx_tags::pixel_intensity_relationship)) {
            auto relationship = dataset.get_string(dx_tags::pixel_intensity_relationship);
            if (relationship != "LIN") {
                result.findings.push_back({
                    validation_severity::info,
                    dx_tags::pixel_intensity_relationship,
                    "For Processing images typically have linear (LIN) pixel "
                    "intensity relationship, found: " + relationship,
                    "DX-INFO-002"
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

bool dx_iod_validator::quick_check(const dicom_dataset& dataset) const {
    // Check only critical Type 1 attributes for quick validation

    // General Study Module Type 1
    if (!dataset.contains(tags::study_instance_uid)) return false;

    // General Series Module Type 1
    if (!dataset.contains(tags::modality)) return false;
    if (!dataset.contains(tags::series_instance_uid)) return false;

    // Check modality is DX
    auto modality = dataset.get_string(tags::modality);
    if (modality != "DX") return false;

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

    return true;
}

const dx_validation_options& dx_iod_validator::options() const noexcept {
    return options_;
}

void dx_iod_validator::set_options(const dx_validation_options& options) {
    options_ = options;
}

// =============================================================================
// Module Validation Methods
// =============================================================================

void dx_iod_validator::validate_patient_module(
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

void dx_iod_validator::validate_general_study_module(
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

void dx_iod_validator::validate_general_series_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Type 1
    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::modality, "Modality", findings);
        check_type1_attribute(dataset, tags::series_instance_uid, "SeriesInstanceUID", findings);

        // Special check: Modality must be "DX"
        check_modality(dataset, findings);
    }

    // Type 2
    if (options_.check_type2) {
        check_type2_attribute(dataset, tags::series_number, "SeriesNumber", findings);
    }
}

void dx_iod_validator::validate_dx_anatomy_imaged_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Body Part Examined (0018,0015) - Type 2
    if (options_.check_type2) {
        check_type2_attribute(dataset, dx_tags::body_part_examined, "BodyPartExamined", findings);
    }

    // Anatomic Region Sequence (0008,2218) - Type 1C
    // Required if Body Part Examined is not sufficient to describe the anatomy
    if (options_.check_conditional) {
        if (!dataset.contains(dx_tags::body_part_examined) &&
            !dataset.contains(dx_tags::anatomic_region_sequence)) {
            findings.push_back({
                validation_severity::warning,
                dx_tags::anatomic_region_sequence,
                "Neither Body Part Examined nor Anatomic Region Sequence present - "
                "anatomy information may be insufficient for clinical use",
                "DX-WARN-001"
            });
        }
    }
}

void dx_iod_validator::validate_dx_image_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Image Type (0008,0008) - Type 1
    if (options_.check_type1) {
        check_type1_attribute(dataset, dx_tags::image_type, "ImageType", findings);

        // Validate Image Type values for DX
        if (dataset.contains(dx_tags::image_type)) {
            auto image_type = dataset.get_string(dx_tags::image_type);
            // First value should be ORIGINAL or DERIVED
            if (image_type.find("ORIGINAL") == std::string::npos &&
                image_type.find("DERIVED") == std::string::npos) {
                findings.push_back({
                    validation_severity::warning,
                    dx_tags::image_type,
                    "Image Type first value should be ORIGINAL or DERIVED",
                    "DX-WARN-002"
                });
            }
            // Second value should be PRIMARY or SECONDARY
            if (image_type.find("PRIMARY") == std::string::npos &&
                image_type.find("SECONDARY") == std::string::npos) {
                findings.push_back({
                    validation_severity::warning,
                    dx_tags::image_type,
                    "Image Type second value should be PRIMARY or SECONDARY",
                    "DX-WARN-003"
                });
            }
        }
    }

    // Acquisition Device Processing Description (0018,1400) - Type 3
    // No validation required, but informational if missing for presentation images

    // Pixel Intensity Relationship (0028,1040) - Type 1 for DX
    if (options_.check_type1) {
        check_type1_attribute(dataset, dx_tags::pixel_intensity_relationship,
                              "PixelIntensityRelationship", findings);

        if (dataset.contains(dx_tags::pixel_intensity_relationship)) {
            auto relationship = dataset.get_string(dx_tags::pixel_intensity_relationship);
            if (relationship != "LIN" && relationship != "LOG") {
                findings.push_back({
                    validation_severity::error,
                    dx_tags::pixel_intensity_relationship,
                    "Pixel Intensity Relationship must be LIN or LOG, found: " + relationship,
                    "DX-ERR-003"
                });
            }
        }
    }

    // Pixel Intensity Relationship Sign (0028,1041) - Type 1 for DX
    if (options_.check_type1) {
        check_type1_attribute(dataset, dx_tags::pixel_intensity_relationship_sign,
                              "PixelIntensityRelationshipSign", findings);

        if (dataset.contains(dx_tags::pixel_intensity_relationship_sign)) {
            auto sign = dataset.get_numeric<int16_t>(dx_tags::pixel_intensity_relationship_sign);
            if (sign && *sign != 1 && *sign != -1) {
                findings.push_back({
                    validation_severity::error,
                    dx_tags::pixel_intensity_relationship_sign,
                    "Pixel Intensity Relationship Sign must be 1 or -1, found: " +
                    std::to_string(*sign),
                    "DX-ERR-004"
                });
            }
        }
    }
}

void dx_iod_validator::validate_dx_detector_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Detector Type (0018,7004) - Type 2
    if (options_.check_type2) {
        check_type2_attribute(dataset, dx_tags::detector_type, "DetectorType", findings);

        if (dataset.contains(dx_tags::detector_type)) {
            auto type = dataset.get_string(dx_tags::detector_type);
            if (type != "DIRECT" && type != "SCINTILLATOR" &&
                type != "STORAGE" && type != "FILM") {
                findings.push_back({
                    validation_severity::warning,
                    dx_tags::detector_type,
                    "Unusual Detector Type for DX: " + type,
                    "DX-WARN-004"
                });
            }
        }
    }

    // Imager Pixel Spacing (0018,1164) - Type 1 for DX
    if (options_.check_type1) {
        check_type1_attribute(dataset, dx_tags::imager_pixel_spacing,
                              "ImagerPixelSpacing", findings);
    }
}

void dx_iod_validator::validate_dx_positioning_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // View Position (0018,5101) - Type 2C
    // Required if Patient Orientation (0020,0020) is not present
    if (options_.check_conditional) {
        if (!dataset.contains(dx_tags::patient_orientation) &&
            !dataset.contains(dx_tags::view_position)) {
            findings.push_back({
                validation_severity::warning,
                dx_tags::view_position,
                "Neither Patient Orientation nor View Position present - "
                "image orientation may be unclear",
                "DX-WARN-005"
            });
        }
    }

    // Patient Orientation (0020,0020) - Type 2C
    if (options_.check_conditional && dataset.contains(dx_tags::patient_orientation)) {
        auto orientation = dataset.get_string(dx_tags::patient_orientation);
        if (orientation.empty()) {
            findings.push_back({
                validation_severity::info,
                dx_tags::patient_orientation,
                "Patient Orientation is present but empty",
                "DX-INFO-001"
            });
        }
    }

    // Distance Source to Detector (0018,1110) - Type 3, but useful for calibration
    // Distance Source to Patient (0018,1111) - Type 3, but useful for calibration
}

void dx_iod_validator::validate_image_pixel_module(
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

void dx_iod_validator::validate_voi_lut_module(
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
                "For Presentation images should have Window Center/Width or VOI LUT Sequence "
                "for proper display",
                "DX-WARN-006"
            });
        }
    }
}

void dx_iod_validator::validate_sop_common_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Type 1
    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::sop_class_uid, "SOPClassUID", findings);
        check_type1_attribute(dataset, tags::sop_instance_uid, "SOPInstanceUID", findings);
    }

    // Validate SOP Class UID is a DX storage class
    if (dataset.contains(tags::sop_class_uid)) {
        auto sop_class = dataset.get_string(tags::sop_class_uid);
        if (!sop_classes::is_dx_storage_sop_class(sop_class)) {
            findings.push_back({
                validation_severity::error,
                tags::sop_class_uid,
                "SOPClassUID is not a recognized DX Storage SOP Class: " + sop_class,
                "DX-ERR-001"
            });
        }
    }
}

// =============================================================================
// Attribute Validation Helpers
// =============================================================================

void dx_iod_validator::check_type1_attribute(
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
            "DX-TYPE1-MISSING"
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
                "DX-TYPE1-EMPTY"
            });
        }
    }
}

void dx_iod_validator::check_type2_attribute(
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
            "DX-TYPE2-MISSING"
        });
    }
}

void dx_iod_validator::check_modality(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(tags::modality)) {
        return;  // Already reported as Type 1 missing
    }

    auto modality = dataset.get_string(tags::modality);
    if (modality != "DX") {
        findings.push_back({
            validation_severity::error,
            tags::modality,
            "Modality must be 'DX' for digital X-ray images, found: " + modality,
            "DX-ERR-002"
        });
    }
}

void dx_iod_validator::check_pixel_data_consistency(
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
                "DX-ERR-005"
            });
        }

        // DX typically uses 10-16 bits stored
        if (*bits_stored < 10 || *bits_stored > 16) {
            findings.push_back({
                validation_severity::info,
                tags::bits_stored,
                "DX images typically use 10-16 bits, found: " +
                std::to_string(*bits_stored),
                "DX-INFO-003"
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
                "DX-WARN-007"
            });
        }
    }

    // DX must be grayscale - check SamplesPerPixel
    auto samples = dataset.get_numeric<uint16_t>(tags::samples_per_pixel);
    if (samples && *samples != 1) {
        findings.push_back({
            validation_severity::error,
            tags::samples_per_pixel,
            "DX images must be grayscale (SamplesPerPixel = 1), found: " +
            std::to_string(*samples),
            "DX-ERR-006"
        });
    }
}

void dx_iod_validator::check_photometric_interpretation(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(tags::photometric_interpretation)) {
        return;  // Already reported as missing
    }

    auto photometric = dataset.get_string(tags::photometric_interpretation);
    if (!sop_classes::is_valid_dx_photometric(photometric)) {
        findings.push_back({
            validation_severity::error,
            tags::photometric_interpretation,
            "DX images must use MONOCHROME1 or MONOCHROME2, found: " + photometric,
            "DX-ERR-007"
        });
    }
}

// =============================================================================
// Convenience Functions
// =============================================================================

validation_result validate_dx_iod(const dicom_dataset& dataset) {
    dx_iod_validator validator;
    return validator.validate(dataset);
}

bool is_valid_dx_dataset(const dicom_dataset& dataset) {
    dx_iod_validator validator;
    return validator.quick_check(dataset);
}

bool is_for_presentation_dx(const dicom_dataset& dataset) {
    if (!dataset.contains(tags::sop_class_uid)) {
        return false;
    }
    auto sop_class = dataset.get_string(tags::sop_class_uid);
    return sop_classes::is_dx_for_presentation_sop_class(sop_class);
}

bool is_for_processing_dx(const dicom_dataset& dataset) {
    if (!dataset.contains(tags::sop_class_uid)) {
        return false;
    }
    auto sop_class = dataset.get_string(tags::sop_class_uid);
    return sop_classes::is_dx_for_processing_sop_class(sop_class);
}

}  // namespace pacs::services::validation
