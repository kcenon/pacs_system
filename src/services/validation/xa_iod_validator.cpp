/**
 * @file xa_iod_validator.cpp
 * @brief Implementation of X-Ray Angiographic Image IOD Validator
 */

#include "pacs/services/validation/xa_iod_validator.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/services/sop_classes/xa_storage.hpp"

#include <sstream>

namespace pacs::services::validation {

using namespace pacs::core;

// =============================================================================
// xa_iod_validator Implementation
// =============================================================================

xa_iod_validator::xa_iod_validator(const xa_validation_options& options)
    : options_(options) {}

validation_result xa_iod_validator::validate(const dicom_dataset& dataset) const {
    validation_result result;
    result.is_valid = true;

    // Validate mandatory modules
    if (options_.check_type1 || options_.check_type2) {
        validate_patient_module(dataset, result.findings);
        validate_general_study_module(dataset, result.findings);
        validate_general_series_module(dataset, result.findings);
        validate_xa_acquisition_module(dataset, result.findings);
        validate_xa_image_module(dataset, result.findings);
        validate_sop_common_module(dataset, result.findings);
    }

    if (options_.validate_pixel_data) {
        validate_image_pixel_module(dataset, result.findings);
    }

    if (options_.validate_positioner) {
        check_positioner_angles(dataset, result.findings);
    }

    if (options_.validate_calibration) {
        validate_calibration_module(dataset, result.findings);
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
xa_iod_validator::validate_multiframe(const dicom_dataset& dataset) const {
    // First do standard validation
    auto result = validate(dataset);

    // Then add multi-frame specific validation
    validate_multiframe_module(dataset, result.findings);

    // Re-check validity after multi-frame validation
    for (const auto& finding : result.findings) {
        if (finding.severity == validation_severity::error) {
            result.is_valid = false;
            break;
        }
    }

    return result;
}

bool xa_iod_validator::quick_check(const dicom_dataset& dataset) const {
    // Check only Type 1 required attributes for quick validation

    // General Study Module Type 1
    if (!dataset.contains(tags::study_instance_uid)) return false;

    // General Series Module Type 1
    if (!dataset.contains(tags::modality)) return false;
    if (!dataset.contains(tags::series_instance_uid)) return false;

    // Check modality is XA or XRF
    auto modality = dataset.get_string(tags::modality);
    if (modality != "XA" && modality != "XRF") return false;

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

    // XA Image Module Type 1
    if (!dataset.contains(tags::image_type)) return false;

    // SOP Common Module Type 1
    if (!dataset.contains(tags::sop_class_uid)) return false;
    if (!dataset.contains(tags::sop_instance_uid)) return false;

    return true;
}

validation_result
xa_iod_validator::validate_calibration(const dicom_dataset& dataset) const {
    validation_result result;
    result.is_valid = true;

    validate_calibration_module(dataset, result.findings);

    for (const auto& finding : result.findings) {
        if (finding.severity == validation_severity::error) {
            result.is_valid = false;
            break;
        }
    }

    return result;
}

const xa_validation_options& xa_iod_validator::options() const noexcept {
    return options_;
}

void xa_iod_validator::set_options(const xa_validation_options& options) {
    options_ = options;
}

// =============================================================================
// Module Validation Methods
// =============================================================================

void xa_iod_validator::validate_patient_module(
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

void xa_iod_validator::validate_general_study_module(
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

void xa_iod_validator::validate_general_series_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Type 1
    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::modality, "Modality", findings);
        check_type1_attribute(dataset, tags::series_instance_uid, "SeriesInstanceUID", findings);

        // Special check: Modality must be "XA" or "XRF"
        check_modality(dataset, findings);
    }

    // Type 2
    if (options_.check_type2) {
        check_type2_attribute(dataset, tags::series_number, "SeriesNumber", findings);
    }
}

void xa_iod_validator::validate_xa_acquisition_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // XA/XRF Acquisition Module
    // Most attributes are Type 3 (optional), but we can provide info findings

    if (options_.check_conditional) {
        // Field of View info - Type 3 but useful for QA
        if (!dataset.contains(xa_tags::field_of_view_shape)) {
            findings.push_back({
                validation_severity::info,
                xa_tags::field_of_view_shape,
                "FieldOfViewShape (0018,1147) not present - FOV information unavailable",
                "XA-INFO-001"
            });
        }

        // Positioner motion for rotational acquisitions
        if (!dataset.contains(xa_tags::positioner_motion)) {
            findings.push_back({
                validation_severity::info,
                xa_tags::positioner_motion,
                "PositionerMotion (0018,1500) not present - motion type unknown",
                "XA-INFO-002"
            });
        }
    }
}

void xa_iod_validator::validate_xa_image_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Type 1 - Image Type
    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::image_type, "ImageType", findings);
    }

    // Check photometric interpretation is valid for XA
    check_xa_photometric(dataset, findings);
}

void xa_iod_validator::validate_image_pixel_module(
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
    }
}

void xa_iod_validator::validate_multiframe_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // NumberOfFrames is Type 1 for multi-frame
    if (options_.check_type1) {
        check_type1_attribute(dataset, xa_tags::number_of_frames, "NumberOfFrames", findings);
    }

    // Frame timing validation
    if (options_.validate_multiframe_timing && options_.check_conditional) {
        bool has_frame_time = dataset.contains(xa_tags::frame_time);
        bool has_frame_time_vector = dataset.contains(xa_tags::frame_time_vector);

        if (!has_frame_time && !has_frame_time_vector) {
            findings.push_back({
                validation_severity::warning,
                xa_tags::frame_time,
                "Neither FrameTime (0018,1063) nor FrameTimeVector (0018,1065) present - "
                "frame timing information missing for cine playback",
                "XA-WARN-001"
            });
        }

        // CineRate is useful for proper playback
        if (!dataset.contains(xa_tags::cine_rate) &&
            !dataset.contains(xa_tags::recommended_display_frame_rate)) {
            findings.push_back({
                validation_severity::info,
                xa_tags::cine_rate,
                "Neither CineRate nor RecommendedDisplayFrameRate present - "
                "display timing may be incorrect",
                "XA-INFO-003"
            });
        }
    }
}

void xa_iod_validator::validate_calibration_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // XA Calibration Module (Conditional)
    // Required if quantitative measurements are to be performed

    bool has_imager_spacing = dataset.contains(xa_tags::imager_pixel_spacing);
    bool has_sid = dataset.contains(xa_tags::distance_source_to_detector);
    bool has_sod = dataset.contains(xa_tags::distance_source_to_patient);

    // Imager Pixel Spacing is essential for any calibration
    if (!has_imager_spacing) {
        findings.push_back({
            validation_severity::warning,
            xa_tags::imager_pixel_spacing,
            "ImagerPixelSpacing (0018,1164) not present - "
            "quantitative measurements not possible",
            "XA-WARN-002"
        });
    }

    // SID and SOD needed for accurate isocenter calibration
    if (has_imager_spacing && (!has_sid || !has_sod)) {
        findings.push_back({
            validation_severity::warning,
            xa_tags::distance_source_to_detector,
            "DistanceSourceToDetector or DistanceSourceToPatient missing - "
            "magnification correction not possible for QCA",
            "XA-WARN-003"
        });
    }

    // Validate SID > SOD (physical constraint)
    if (has_sid && has_sod) {
        auto sid = dataset.get_numeric<double>(xa_tags::distance_source_to_detector);
        auto sod = dataset.get_numeric<double>(xa_tags::distance_source_to_patient);

        if (sid && sod && *sid <= *sod) {
            findings.push_back({
                validation_severity::error,
                xa_tags::distance_source_to_detector,
                "DistanceSourceToDetector (" + std::to_string(*sid) +
                ") must be greater than DistanceSourceToPatient (" +
                std::to_string(*sod) + ")",
                "XA-ERR-001"
            });
        }
    }
}

void xa_iod_validator::validate_sop_common_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Type 1
    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::sop_class_uid, "SOPClassUID", findings);
        check_type1_attribute(dataset, tags::sop_instance_uid, "SOPInstanceUID", findings);
    }

    // Validate SOP Class UID is an XA/XRF storage class
    if (dataset.contains(tags::sop_class_uid)) {
        auto sop_class = dataset.get_string(tags::sop_class_uid);
        if (!sop_classes::is_xa_storage_sop_class(sop_class)) {
            findings.push_back({
                validation_severity::error,
                tags::sop_class_uid,
                "SOPClassUID is not a recognized XA/XRF Storage SOP Class: " + sop_class,
                "XA-ERR-002"
            });
        }
    }
}

// =============================================================================
// Attribute Validation Helpers
// =============================================================================

void xa_iod_validator::check_type1_attribute(
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
            "XA-TYPE1-MISSING"
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
                "XA-TYPE1-EMPTY"
            });
        }
    }
}

void xa_iod_validator::check_type2_attribute(
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
            "XA-TYPE2-MISSING"
        });
    }
}

void xa_iod_validator::check_modality(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(tags::modality)) {
        return;  // Already reported as Type 1 missing
    }

    auto modality = dataset.get_string(tags::modality);
    if (modality != "XA" && modality != "XRF") {
        findings.push_back({
            validation_severity::error,
            tags::modality,
            "Modality must be 'XA' or 'XRF' for angiographic images, found: " + modality,
            "XA-ERR-003"
        });
    }
}

void xa_iod_validator::check_xa_photometric(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(tags::photometric_interpretation)) {
        return;  // Will be reported by pixel module validation
    }

    auto photometric = dataset.get_string(tags::photometric_interpretation);
    if (!sop_classes::is_valid_xa_photometric(photometric)) {
        findings.push_back({
            validation_severity::error,
            tags::photometric_interpretation,
            "Invalid photometric interpretation for XA: " + photometric +
            " (must be MONOCHROME1 or MONOCHROME2)",
            "XA-ERR-004"
        });
    }
}

void xa_iod_validator::check_pixel_data_consistency(
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
                "XA-ERR-005"
            });
        }

        // XA typically uses 8, 10, 12, or 16 bits
        if (*bits_stored != 8 && *bits_stored != 10 &&
            *bits_stored != 12 && *bits_stored != 16) {
            findings.push_back({
                validation_severity::warning,
                tags::bits_stored,
                "Unusual BitsStored for XA: " + std::to_string(*bits_stored) +
                " (expected 8, 10, 12, or 16)",
                "XA-WARN-004"
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
                "XA-WARN-005"
            });
        }
    }

    // XA must be grayscale (SamplesPerPixel = 1)
    auto samples = dataset.get_numeric<uint16_t>(tags::samples_per_pixel);
    if (samples && *samples != 1) {
        findings.push_back({
            validation_severity::error,
            tags::samples_per_pixel,
            "XA images must be grayscale (SamplesPerPixel = 1), found: " +
            std::to_string(*samples),
            "XA-ERR-006"
        });
    }

    // XA uses unsigned integers (PixelRepresentation = 0)
    auto pixel_rep = dataset.get_numeric<uint16_t>(tags::pixel_representation);
    if (pixel_rep && *pixel_rep != 0) {
        findings.push_back({
            validation_severity::warning,
            tags::pixel_representation,
            "XA images typically use unsigned integers (PixelRepresentation = 0)",
            "XA-WARN-006"
        });
    }
}

void xa_iod_validator::check_positioner_angles(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    bool has_primary = dataset.contains(xa_tags::positioner_primary_angle);
    bool has_secondary = dataset.contains(xa_tags::positioner_secondary_angle);

    // Both angles should be present for proper geometry reconstruction
    if (has_primary != has_secondary) {
        findings.push_back({
            validation_severity::warning,
            has_primary ? xa_tags::positioner_secondary_angle
                        : xa_tags::positioner_primary_angle,
            "Only one positioner angle present - complete geometry unavailable",
            "XA-WARN-007"
        });
    }

    if (!has_primary && !has_secondary) {
        findings.push_back({
            validation_severity::info,
            xa_tags::positioner_primary_angle,
            "No positioner angle information - geometry reconstruction not possible",
            "XA-INFO-004"
        });
    }

    // Validate angle ranges if present
    if (has_primary) {
        auto angle = dataset.get_numeric<double>(xa_tags::positioner_primary_angle);
        if (angle && (*angle < -180.0 || *angle > 180.0)) {
            findings.push_back({
                validation_severity::warning,
                xa_tags::positioner_primary_angle,
                "PositionerPrimaryAngle (" + std::to_string(*angle) +
                ") outside typical range [-180, 180]",
                "XA-WARN-008"
            });
        }
    }

    if (has_secondary) {
        auto angle = dataset.get_numeric<double>(xa_tags::positioner_secondary_angle);
        if (angle && (*angle < -90.0 || *angle > 90.0)) {
            findings.push_back({
                validation_severity::warning,
                xa_tags::positioner_secondary_angle,
                "PositionerSecondaryAngle (" + std::to_string(*angle) +
                ") outside typical range [-90, 90]",
                "XA-WARN-009"
            });
        }
    }
}

// =============================================================================
// Convenience Functions
// =============================================================================

validation_result validate_xa_iod(const dicom_dataset& dataset) {
    xa_iod_validator validator;
    return validator.validate(dataset);
}

bool is_valid_xa_dataset(const dicom_dataset& dataset) {
    xa_iod_validator validator;
    return validator.quick_check(dataset);
}

bool has_qca_calibration(const dicom_dataset& dataset) {
    // QCA requires ImagerPixelSpacing, SID, and SOD
    if (!dataset.contains(xa_tags::imager_pixel_spacing)) return false;
    if (!dataset.contains(xa_tags::distance_source_to_detector)) return false;
    if (!dataset.contains(xa_tags::distance_source_to_patient)) return false;

    // Validate SID > SOD
    auto sid = dataset.get_numeric<double>(xa_tags::distance_source_to_detector);
    auto sod = dataset.get_numeric<double>(xa_tags::distance_source_to_patient);

    if (!sid || !sod || *sid <= *sod) return false;

    return true;
}

}  // namespace pacs::services::validation
