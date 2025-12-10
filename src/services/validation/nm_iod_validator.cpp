/**
 * @file nm_iod_validator.cpp
 * @brief Implementation of Nuclear Medicine Image IOD Validator
 */

#include "pacs/services/validation/nm_iod_validator.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/services/sop_classes/nm_storage.hpp"

#include <sstream>

namespace pacs::services::validation {

using namespace pacs::core;

// NM-specific DICOM tags
namespace nm_tags {
    // NM Image Module
    constexpr dicom_tag image_type{0x0008, 0x0008};
    constexpr dicom_tag number_of_frames{0x0028, 0x0008};
    constexpr dicom_tag frame_increment_pointer{0x0028, 0x0009};

    // NM Isotope Module
    constexpr dicom_tag energy_window_info_sequence{0x0054, 0x0012};
    constexpr dicom_tag energy_window_range_sequence{0x0054, 0x0013};
    constexpr dicom_tag radiopharmaceutical_info_sequence{0x0054, 0x0016};
    constexpr dicom_tag radionuclide_total_dose{0x0018, 0x1074};
    constexpr dicom_tag radionuclide_half_life{0x0018, 0x1075};
    constexpr dicom_tag radiopharmaceutical_start_time{0x0018, 0x1072};

    // NM Detector Module
    constexpr dicom_tag detector_info_sequence{0x0054, 0x0022};
    constexpr dicom_tag collimator_type{0x0018, 0x1181};

    // NM TOMO Acquisition Module (SPECT)
    constexpr dicom_tag rotation_info_sequence{0x0054, 0x0052};
    constexpr dicom_tag type_of_data{0x0054, 0x0400};
    constexpr dicom_tag start_angle{0x0054, 0x0200};
    constexpr dicom_tag scan_arc{0x0018, 0x1144};
    constexpr dicom_tag rotation_direction{0x0018, 0x1140};

    // NM Multi-gated Acquisition Module
    constexpr dicom_tag gated_info_sequence{0x0054, 0x0062};
    constexpr dicom_tag trigger_source_or_type{0x0018, 0x1061};
    constexpr dicom_tag number_of_time_slots{0x0054, 0x0071};
}

// =============================================================================
// nm_iod_validator Implementation
// =============================================================================

nm_iod_validator::nm_iod_validator(const nm_validation_options& options)
    : options_(options) {}

validation_result nm_iod_validator::validate(const dicom_dataset& dataset) const {
    validation_result result;
    result.is_valid = true;

    // Validate mandatory modules
    if (options_.check_type1 || options_.check_type2) {
        validate_patient_module(dataset, result.findings);
        validate_general_study_module(dataset, result.findings);
        validate_general_series_module(dataset, result.findings);
        validate_nm_series_module(dataset, result.findings);
        validate_nm_image_module(dataset, result.findings);
        validate_sop_common_module(dataset, result.findings);
    }

    if (options_.validate_pixel_data) {
        validate_image_pixel_module(dataset, result.findings);
    }

    // NM-specific validation
    if (options_.validate_nm_specific) {
        if (options_.validate_energy_windows) {
            validate_energy_window_info(dataset, result.findings);
        }
        if (options_.validate_isotope) {
            validate_nm_isotope_module(dataset, result.findings);
            validate_radiopharmaceutical_info(dataset, result.findings);
        }
        validate_nm_detector_module(dataset, result.findings);
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
nm_iod_validator::validate_multiframe(const dicom_dataset& dataset) const {
    // First do standard validation
    auto result = validate(dataset);

    // Then add multi-frame specific validation
    validate_multiframe_module(dataset, result.findings);

    // Validate SPECT-specific if applicable
    if (dataset.contains(nm_tags::type_of_data)) {
        auto type = dataset.get_string(nm_tags::type_of_data);
        if (type.find("TOMO") != std::string::npos) {
            validate_nm_tomo_module(dataset, result.findings);
        }
        if (type.find("GATED") != std::string::npos) {
            validate_nm_gated_module(dataset, result.findings);
        }
    }

    // Re-check validity after multi-frame validation
    for (const auto& finding : result.findings) {
        if (finding.severity == validation_severity::error) {
            result.is_valid = false;
            break;
        }
    }

    return result;
}

bool nm_iod_validator::quick_check(const dicom_dataset& dataset) const {
    // Check only Type 1 required attributes for quick validation

    // General Study Module Type 1
    if (!dataset.contains(tags::study_instance_uid)) return false;

    // General Series Module Type 1
    if (!dataset.contains(tags::modality)) return false;
    if (!dataset.contains(tags::series_instance_uid)) return false;

    // Check modality is NM
    auto modality = dataset.get_string(tags::modality);
    if (modality != "NM") return false;

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

const nm_validation_options& nm_iod_validator::options() const noexcept {
    return options_;
}

void nm_iod_validator::set_options(const nm_validation_options& options) {
    options_ = options;
}

// =============================================================================
// Module Validation Methods
// =============================================================================

void nm_iod_validator::validate_patient_module(
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

void nm_iod_validator::validate_general_study_module(
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

void nm_iod_validator::validate_general_series_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Type 1
    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::modality, "Modality", findings);
        check_type1_attribute(dataset, tags::series_instance_uid, "SeriesInstanceUID", findings);

        // Special check: Modality must be "NM"
        check_modality(dataset, findings);
    }

    // Type 2
    if (options_.check_type2) {
        check_type2_attribute(dataset, tags::series_number, "SeriesNumber", findings);
    }
}

void nm_iod_validator::validate_nm_series_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Type of Data is important for NM series classification
    if (options_.check_type1) {
        check_type1_attribute(dataset, nm_tags::type_of_data, "TypeOfData", findings);
    }

    // Validate TypeOfData value
    if (dataset.contains(nm_tags::type_of_data)) {
        auto type = dataset.get_string(nm_tags::type_of_data);
        // Valid values: STATIC, DYNAMIC, GATED, WHOLE BODY, TOMO, GATED TOMO, RECON TOMO, RECON GATED TOMO
        if (type != "STATIC" && type != "DYNAMIC" && type != "GATED" &&
            type != "WHOLE BODY" && type != "TOMO" && type != "GATED TOMO" &&
            type != "RECON TOMO" && type != "RECON GATED TOMO") {
            findings.push_back({
                validation_severity::warning,
                nm_tags::type_of_data,
                "Unusual TypeOfData value for NM: " + type,
                "NM-WARN-001"
            });
        }
    }
}

void nm_iod_validator::validate_nm_image_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Type 1
    if (options_.check_type1) {
        check_type1_attribute(dataset, nm_tags::image_type, "ImageType", findings);
    }

    // NumberOfFrames is Type 1 for NM (often multi-frame)
    if (options_.check_type1) {
        check_type1_attribute(dataset, nm_tags::number_of_frames, "NumberOfFrames", findings);
    }

    // Validate ImageType format
    if (dataset.contains(nm_tags::image_type)) {
        auto image_type = dataset.get_string(nm_tags::image_type);
        if (image_type.find('\\') == std::string::npos) {
            findings.push_back({
                validation_severity::info,
                nm_tags::image_type,
                "ImageType should contain backslash separators",
                "NM-INFO-001"
            });
        }
    }
}

void nm_iod_validator::validate_nm_isotope_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Energy Window Information Sequence is Type 2 per DICOM standard
    // (Required, but may be empty)
    if (options_.check_type2) {
        check_type2_attribute(dataset, nm_tags::energy_window_info_sequence,
                              "EnergyWindowInformationSequence", findings);
    }

    // Radiopharmaceutical Information Sequence is Type 2
    if (options_.check_type2) {
        check_type2_attribute(dataset, nm_tags::radiopharmaceutical_info_sequence,
                              "RadiopharmaceuticalInformationSequence", findings);
    }
}

void nm_iod_validator::validate_nm_detector_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Detector Information Sequence is Type 2
    if (options_.check_type2) {
        check_type2_attribute(dataset, nm_tags::detector_info_sequence,
                              "DetectorInformationSequence", findings);
    }

    // Collimator Type is informational but important
    if (!dataset.contains(nm_tags::collimator_type)) {
        findings.push_back({
            validation_severity::info,
            nm_tags::collimator_type,
            "CollimatorType not specified - helps with interpretation",
            "NM-INFO-002"
        });
    } else {
        auto collimator = dataset.get_string(nm_tags::collimator_type);
        // Validate collimator type
        if (collimator != "PARA" && collimator != "FANB" && collimator != "CONE" &&
            collimator != "PINH" && collimator != "DIVG" && collimator != "NONE") {
            findings.push_back({
                validation_severity::warning,
                nm_tags::collimator_type,
                "Non-standard CollimatorType: " + collimator,
                "NM-WARN-002"
            });
        }
    }
}

void nm_iod_validator::validate_nm_tomo_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Rotation Information Sequence is Type 2 for SPECT
    if (options_.check_type2) {
        check_type2_attribute(dataset, nm_tags::rotation_info_sequence,
                              "RotationInformationSequence", findings);
    }

    // Start Angle and Scan Arc are important for SPECT reconstruction
    if (!dataset.contains(nm_tags::start_angle)) {
        findings.push_back({
            validation_severity::warning,
            nm_tags::start_angle,
            "StartAngle not specified for SPECT acquisition",
            "NM-WARN-003"
        });
    }

    if (!dataset.contains(nm_tags::scan_arc)) {
        findings.push_back({
            validation_severity::warning,
            nm_tags::scan_arc,
            "ScanArc not specified for SPECT acquisition",
            "NM-WARN-004"
        });
    }

    // Rotation Direction
    if (dataset.contains(nm_tags::rotation_direction)) {
        auto direction = dataset.get_string(nm_tags::rotation_direction);
        if (direction != "CW" && direction != "CC") {
            findings.push_back({
                validation_severity::warning,
                nm_tags::rotation_direction,
                "Invalid RotationDirection: " + direction + " (expected CW or CC)",
                "NM-WARN-005"
            });
        }
    }
}

void nm_iod_validator::validate_nm_gated_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Gated Information Sequence is Type 2 for gated acquisitions
    if (options_.check_type2) {
        check_type2_attribute(dataset, nm_tags::gated_info_sequence,
                              "GatedInformationSequence", findings);
    }

    // Trigger Source for gated acquisition
    if (!dataset.contains(nm_tags::trigger_source_or_type)) {
        findings.push_back({
            validation_severity::info,
            nm_tags::trigger_source_or_type,
            "TriggerSourceOrType not specified for gated acquisition",
            "NM-INFO-003"
        });
    }

    // Number of Time Slots is important for gated analysis
    if (!dataset.contains(nm_tags::number_of_time_slots)) {
        findings.push_back({
            validation_severity::warning,
            nm_tags::number_of_time_slots,
            "NumberOfTimeSlots not specified for gated acquisition",
            "NM-WARN-006"
        });
    }
}

void nm_iod_validator::validate_image_pixel_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Type 1 attributes
    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::samples_per_pixel, "SamplesPerPixel", findings);
        check_type1_attribute(dataset, tags::photometric_interpretation,
                              "PhotometricInterpretation", findings);
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

void nm_iod_validator::validate_multiframe_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // NumberOfFrames should be present for multi-frame NM
    if (options_.check_type1) {
        check_type1_attribute(dataset, nm_tags::number_of_frames, "NumberOfFrames", findings);
    }

    // FrameIncrementPointer is Type 1C for multi-frame
    if (options_.check_conditional) {
        auto num_frames = dataset.get_numeric<int32_t>(nm_tags::number_of_frames);
        if (num_frames && *num_frames > 1) {
            if (!dataset.contains(nm_tags::frame_increment_pointer)) {
                findings.push_back({
                    validation_severity::warning,
                    nm_tags::frame_increment_pointer,
                    "FrameIncrementPointer recommended for multi-frame NM images",
                    "NM-WARN-007"
                });
            }
        }
    }
}

void nm_iod_validator::validate_sop_common_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Type 1
    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::sop_class_uid, "SOPClassUID", findings);
        check_type1_attribute(dataset, tags::sop_instance_uid, "SOPInstanceUID", findings);
    }

    // Validate SOP Class UID is a NM storage class
    if (dataset.contains(tags::sop_class_uid)) {
        auto sop_class = dataset.get_string(tags::sop_class_uid);
        if (!sop_classes::is_nm_storage_sop_class(sop_class)) {
            findings.push_back({
                validation_severity::error,
                tags::sop_class_uid,
                "SOPClassUID is not a recognized NM Storage SOP Class: " + sop_class,
                "NM-ERR-001"
            });
        }
    }
}

// =============================================================================
// NM-Specific Validation
// =============================================================================

void nm_iod_validator::validate_energy_window_info(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Energy Window Information Sequence is Type 1
    if (!dataset.contains(nm_tags::energy_window_info_sequence)) {
        // Already reported as Type 1 missing
        return;
    }

    // Energy Window Range Sequence should be present
    if (!dataset.contains(nm_tags::energy_window_range_sequence)) {
        findings.push_back({
            validation_severity::warning,
            nm_tags::energy_window_range_sequence,
            "EnergyWindowRangeSequence not present - energy window details missing",
            "NM-WARN-008"
        });
    }
}

void nm_iod_validator::validate_radiopharmaceutical_info(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // If sequence is present, validate its contents
    if (dataset.contains(nm_tags::radiopharmaceutical_info_sequence)) {
        // Radionuclide Total Dose is important for quantitative analysis
        if (!dataset.contains(nm_tags::radionuclide_total_dose)) {
            findings.push_back({
                validation_severity::info,
                nm_tags::radionuclide_total_dose,
                "RadionuclideTotalDose not specified",
                "NM-INFO-004"
            });
        }

        // Radionuclide Half Life is important for decay correction
        if (!dataset.contains(nm_tags::radionuclide_half_life)) {
            findings.push_back({
                validation_severity::info,
                nm_tags::radionuclide_half_life,
                "RadionuclideHalfLife not specified",
                "NM-INFO-005"
            });
        }

        // Radiopharmaceutical Start Time for timing calculations
        if (!dataset.contains(nm_tags::radiopharmaceutical_start_time)) {
            findings.push_back({
                validation_severity::info,
                nm_tags::radiopharmaceutical_start_time,
                "RadiopharmaceuticalStartTime not specified",
                "NM-INFO-006"
            });
        }
    }
}

// =============================================================================
// Attribute Validation Helpers
// =============================================================================

void nm_iod_validator::check_type1_attribute(
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
            "NM-TYPE1-MISSING"
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
                "NM-TYPE1-EMPTY"
            });
        }
    }
}

void nm_iod_validator::check_type2_attribute(
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
            "NM-TYPE2-MISSING"
        });
    }
}

void nm_iod_validator::check_modality(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(tags::modality)) {
        return;  // Already reported as Type 1 missing
    }

    auto modality = dataset.get_string(tags::modality);
    if (modality != "NM") {
        findings.push_back({
            validation_severity::error,
            tags::modality,
            "Modality must be 'NM' for nuclear medicine images, found: " + modality,
            "NM-ERR-002"
        });
    }
}

void nm_iod_validator::check_pixel_data_consistency(
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
                "NM-ERR-003"
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
                "NM-WARN-009"
            });
        }
    }

    // Check photometric interpretation is valid for NM
    if (dataset.contains(tags::photometric_interpretation)) {
        auto photometric = dataset.get_string(tags::photometric_interpretation);
        if (!sop_classes::is_valid_nm_photometric(photometric)) {
            findings.push_back({
                validation_severity::warning,
                tags::photometric_interpretation,
                "Unusual photometric interpretation for NM: " + photometric +
                " (expected MONOCHROME2 or PALETTE COLOR)",
                "NM-WARN-010"
            });
        }
    }

    // NM images should be grayscale (SamplesPerPixel = 1)
    auto samples = dataset.get_numeric<uint16_t>(tags::samples_per_pixel);
    if (samples && *samples != 1) {
        findings.push_back({
            validation_severity::warning,
            tags::samples_per_pixel,
            "NM images should have SamplesPerPixel = 1, found: " +
            std::to_string(*samples),
            "NM-WARN-011"
        });
    }
}

// =============================================================================
// Convenience Functions
// =============================================================================

validation_result validate_nm_iod(const dicom_dataset& dataset) {
    nm_iod_validator validator;
    return validator.validate(dataset);
}

bool is_valid_nm_dataset(const dicom_dataset& dataset) {
    nm_iod_validator validator;
    return validator.quick_check(dataset);
}

}  // namespace pacs::services::validation
