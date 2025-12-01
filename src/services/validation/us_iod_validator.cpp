/**
 * @file us_iod_validator.cpp
 * @brief Implementation of Ultrasound Image IOD Validator
 */

#include "pacs/services/validation/us_iod_validator.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/services/sop_classes/us_storage.hpp"

#include <sstream>

namespace pacs::services::validation {

using namespace pacs::core;

// =============================================================================
// validation_result Implementation
// =============================================================================

bool validation_result::has_errors() const noexcept {
    for (const auto& f : findings) {
        if (f.severity == validation_severity::error) {
            return true;
        }
    }
    return false;
}

bool validation_result::has_warnings() const noexcept {
    for (const auto& f : findings) {
        if (f.severity == validation_severity::warning) {
            return true;
        }
    }
    return false;
}

size_t validation_result::error_count() const noexcept {
    size_t count = 0;
    for (const auto& f : findings) {
        if (f.severity == validation_severity::error) {
            ++count;
        }
    }
    return count;
}

size_t validation_result::warning_count() const noexcept {
    size_t count = 0;
    for (const auto& f : findings) {
        if (f.severity == validation_severity::warning) {
            ++count;
        }
    }
    return count;
}

std::string validation_result::summary() const {
    std::ostringstream oss;
    oss << "Validation " << (is_valid ? "PASSED" : "FAILED");
    oss << " - " << error_count() << " error(s), "
        << warning_count() << " warning(s)";
    return oss.str();
}

// =============================================================================
// us_iod_validator Implementation
// =============================================================================

us_iod_validator::us_iod_validator(const us_validation_options& options)
    : options_(options) {}

validation_result us_iod_validator::validate(const dicom_dataset& dataset) const {
    validation_result result;
    result.is_valid = true;

    // Validate mandatory modules
    if (options_.check_type1 || options_.check_type2) {
        validate_patient_module(dataset, result.findings);
        validate_general_study_module(dataset, result.findings);
        validate_general_series_module(dataset, result.findings);
        validate_us_image_module(dataset, result.findings);
        validate_sop_common_module(dataset, result.findings);
    }

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

validation_result
us_iod_validator::validate_multiframe(const dicom_dataset& dataset) const {
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

bool us_iod_validator::quick_check(const dicom_dataset& dataset) const {
    // Check only Type 1 required attributes for quick validation

    // Patient Module Type 1
    // (Patient module has Type 2 attributes, no Type 1)

    // General Study Module Type 1
    if (!dataset.contains(tags::study_instance_uid)) return false;

    // General Series Module Type 1
    if (!dataset.contains(tags::modality)) return false;
    if (!dataset.contains(tags::series_instance_uid)) return false;

    // Check modality is US
    auto modality = dataset.get_string(tags::modality);
    if (modality != "US") return false;

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

const us_validation_options& us_iod_validator::options() const noexcept {
    return options_;
}

void us_iod_validator::set_options(const us_validation_options& options) {
    options_ = options;
}

// =============================================================================
// Module Validation Methods
// =============================================================================

void us_iod_validator::validate_patient_module(
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

void us_iod_validator::validate_general_study_module(
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

void us_iod_validator::validate_general_series_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Type 1
    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::modality, "Modality", findings);
        check_type1_attribute(dataset, tags::series_instance_uid, "SeriesInstanceUID", findings);

        // Special check: Modality must be "US"
        check_modality(dataset, findings);
    }

    // Type 2
    if (options_.check_type2) {
        check_type2_attribute(dataset, tags::series_number, "SeriesNumber", findings);
    }
}

void us_iod_validator::validate_us_image_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // US Image Module validation
    // Sequence of Ultrasound Regions (0018,6011) is Type 1C - required if
    // the Ultrasound image contains calibrated regions

    // Note: The exact conditional logic depends on the presence of
    // calibration data. For now, we add an info finding if it's missing.
    if (options_.check_conditional) {
        constexpr dicom_tag sequence_of_ultrasound_regions{0x0018, 0x6011};
        if (!dataset.contains(sequence_of_ultrasound_regions)) {
            findings.push_back({
                validation_severity::info,
                sequence_of_ultrasound_regions,
                "SequenceOfUltrasoundRegions (0018,6011) not present - "
                "no calibration information available",
                "US-INFO-001"
            });
        }
    }
}

void us_iod_validator::validate_image_pixel_module(
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

void us_iod_validator::validate_multiframe_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    constexpr dicom_tag number_of_frames{0x0028, 0x0008};
    constexpr dicom_tag frame_time{0x0018, 0x1063};
    constexpr dicom_tag frame_time_vector{0x0018, 0x1065};

    // NumberOfFrames is Type 1 for multi-frame
    if (options_.check_type1) {
        check_type1_attribute(dataset, number_of_frames, "NumberOfFrames", findings);
    }

    // FrameTime or FrameTimeVector should be present (Type 1C)
    if (options_.check_conditional) {
        if (!dataset.contains(frame_time) && !dataset.contains(frame_time_vector)) {
            findings.push_back({
                validation_severity::warning,
                frame_time,
                "Neither FrameTime nor FrameTimeVector present - "
                "frame timing information missing",
                "US-WARN-001"
            });
        }
    }
}

void us_iod_validator::validate_sop_common_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Type 1
    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::sop_class_uid, "SOPClassUID", findings);
        check_type1_attribute(dataset, tags::sop_instance_uid, "SOPInstanceUID", findings);
    }

    // Validate SOP Class UID is a US storage class
    if (dataset.contains(tags::sop_class_uid)) {
        auto sop_class = dataset.get_string(tags::sop_class_uid);
        if (!sop_classes::is_us_storage_sop_class(sop_class)) {
            findings.push_back({
                validation_severity::error,
                tags::sop_class_uid,
                "SOPClassUID is not a recognized US Storage SOP Class: " + sop_class,
                "US-ERR-001"
            });
        }
    }
}

// =============================================================================
// Attribute Validation Helpers
// =============================================================================

void us_iod_validator::check_type1_attribute(
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
            "US-TYPE1-MISSING"
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
                "US-TYPE1-EMPTY"
            });
        }
    }
}

void us_iod_validator::check_type2_attribute(
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
            "US-TYPE2-MISSING"
        });
    }
}

void us_iod_validator::check_modality(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(tags::modality)) {
        return;  // Already reported as Type 1 missing
    }

    auto modality = dataset.get_string(tags::modality);
    if (modality != "US") {
        findings.push_back({
            validation_severity::error,
            tags::modality,
            "Modality must be 'US' for ultrasound images, found: " + modality,
            "US-ERR-002"
        });
    }
}

void us_iod_validator::check_pixel_data_consistency(
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
                "US-ERR-003"
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
                "US-WARN-002"
            });
        }
    }

    // Check photometric interpretation is valid for US
    if (dataset.contains(tags::photometric_interpretation)) {
        auto photometric = dataset.get_string(tags::photometric_interpretation);
        if (!sop_classes::is_valid_us_photometric(photometric)) {
            findings.push_back({
                validation_severity::warning,
                tags::photometric_interpretation,
                "Unusual photometric interpretation for US: " + photometric,
                "US-WARN-003"
            });
        }
    }

    // Check SamplesPerPixel consistency with photometric
    auto samples = dataset.get_numeric<uint16_t>(tags::samples_per_pixel);
    if (samples && dataset.contains(tags::photometric_interpretation)) {
        auto photometric = dataset.get_string(tags::photometric_interpretation);
        bool is_color = (photometric == "RGB" || photometric == "YBR_FULL" ||
                        photometric == "YBR_FULL_422" || photometric == "PALETTE COLOR");

        if (is_color && *samples != 3) {
            findings.push_back({
                validation_severity::error,
                tags::samples_per_pixel,
                "Color photometric interpretation requires SamplesPerPixel = 3",
                "US-ERR-004"
            });
        }

        if (!is_color && *samples != 1) {
            findings.push_back({
                validation_severity::error,
                tags::samples_per_pixel,
                "Grayscale photometric interpretation requires SamplesPerPixel = 1",
                "US-ERR-005"
            });
        }
    }
}

// =============================================================================
// Convenience Functions
// =============================================================================

validation_result validate_us_iod(const dicom_dataset& dataset) {
    us_iod_validator validator;
    return validator.validate(dataset);
}

bool is_valid_us_dataset(const dicom_dataset& dataset) {
    us_iod_validator validator;
    return validator.quick_check(dataset);
}

}  // namespace pacs::services::validation
