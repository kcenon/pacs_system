// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file ct_iod_validator.cpp
 * @brief Implementation of CT Image IOD Validator
 *
 * @see DICOM PS3.3 Section A.3 - CT Image IOD
 * @see Issue #717 - Add CT Image IOD Validator
 */

#include "kcenon/pacs/services/validation/ct_iod_validator.h"
#include "kcenon/pacs/core/dicom_tag_constants.h"
#include "kcenon/pacs/services/sop_classes/ct_storage.h"

namespace kcenon::pacs::services::validation {

using namespace kcenon::pacs::core;

// =============================================================================
// ct_iod_validator Implementation
// =============================================================================

ct_iod_validator::ct_iod_validator(const ct_validation_options& options)
    : options_(options) {}

validation_result ct_iod_validator::validate(
    const dicom_dataset& dataset) const {
    validation_result result;
    result.is_valid = true;

    // Validate mandatory modules (PS3.3 Table A.3-1)
    if (options_.check_type1 || options_.check_type2) {
        validate_patient_module(dataset, result.findings);
        validate_general_study_module(dataset, result.findings);
        validate_general_series_module(dataset, result.findings);
        validate_frame_of_reference_module(dataset, result.findings);
        validate_general_equipment_module(dataset, result.findings);
        validate_sop_common_module(dataset, result.findings);
    }

    if (options_.validate_ct_params) {
        validate_ct_image_module(dataset, result.findings);
    }

    if (options_.validate_pixel_data) {
        validate_image_pixel_module(dataset, result.findings);
    }

    // Determine overall validity
    for (const auto& finding : result.findings) {
        if (finding.severity == validation_severity::error) {
            result.is_valid = false;
            break;
        }
        if (options_.strict_mode &&
            finding.severity == validation_severity::warning) {
            result.is_valid = false;
            break;
        }
    }

    return result;
}

bool ct_iod_validator::quick_check(const dicom_dataset& dataset) const {
    // Check only Type 1 required attributes for quick validation

    // General Study Module Type 1
    if (!dataset.contains(tags::study_instance_uid)) return false;

    // General Series Module Type 1
    if (!dataset.contains(tags::modality)) return false;
    if (!dataset.contains(tags::series_instance_uid)) return false;

    // Check modality is CT
    auto modality = dataset.get_string(tags::modality);
    if (modality != "CT") return false;

    // Frame of Reference Module Type 1
    if (!dataset.contains(tags::frame_of_reference_uid)) return false;

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

const ct_validation_options& ct_iod_validator::options() const noexcept {
    return options_;
}

void ct_iod_validator::set_options(const ct_validation_options& options) {
    options_ = options;
}

// =============================================================================
// Module Validation Methods
// =============================================================================

void ct_iod_validator::validate_patient_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Patient Module - All attributes are Type 2
    if (options_.check_type2) {
        check_type2_attribute(dataset, tags::patient_name, "PatientName",
                              findings);
        check_type2_attribute(dataset, tags::patient_id, "PatientID",
                              findings);
        check_type2_attribute(dataset, tags::patient_birth_date,
                              "PatientBirthDate", findings);
        check_type2_attribute(dataset, tags::patient_sex, "PatientSex",
                              findings);
    }
}

void ct_iod_validator::validate_general_study_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Type 1
    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::study_instance_uid,
                              "StudyInstanceUID", findings);
    }

    // Type 2
    if (options_.check_type2) {
        check_type2_attribute(dataset, tags::study_date, "StudyDate",
                              findings);
        check_type2_attribute(dataset, tags::study_time, "StudyTime",
                              findings);
        check_type2_attribute(dataset, tags::referring_physician_name,
                              "ReferringPhysicianName", findings);
        check_type2_attribute(dataset, tags::study_id, "StudyID", findings);
        check_type2_attribute(dataset, tags::accession_number,
                              "AccessionNumber", findings);
    }
}

void ct_iod_validator::validate_general_series_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Type 1
    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::modality, "Modality", findings);
        check_type1_attribute(dataset, tags::series_instance_uid,
                              "SeriesInstanceUID", findings);

        // Modality must be "CT"
        check_modality(dataset, findings);
    }

    // Type 2
    if (options_.check_type2) {
        check_type2_attribute(dataset, tags::series_number, "SeriesNumber",
                              findings);
    }
}

void ct_iod_validator::validate_frame_of_reference_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Frame of Reference UID is Type 1 for CT
    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::frame_of_reference_uid,
                              "FrameOfReferenceUID", findings);
    }

    // Position Reference Indicator is Type 2
    if (options_.check_type2) {
        constexpr dicom_tag position_reference_indicator{0x0020, 0x1040};
        check_type2_attribute(dataset, position_reference_indicator,
                              "PositionReferenceIndicator", findings);
    }
}

void ct_iod_validator::validate_general_equipment_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Manufacturer is Type 2
    if (options_.check_type2) {
        check_type2_attribute(dataset, tags::manufacturer, "Manufacturer",
                              findings);
    }
}

void ct_iod_validator::validate_ct_image_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // CT Image Module (PS3.3 Section C.8.2.1)

    // ImageType is Type 1
    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::image_type, "ImageType",
                              findings);
    }

    // Type 2 attributes specific to CT Image Module
    if (options_.check_type2) {
        check_type2_attribute(dataset, ct_tags::kvp, "KVP", findings);
        check_type2_attribute(dataset, tags::rescale_intercept,
                              "RescaleIntercept", findings);
        check_type2_attribute(dataset, tags::rescale_slope, "RescaleSlope",
                              findings);
    }

    // Type 2C/3 - informational checks for common CT attributes
    if (options_.check_conditional) {
        if (!dataset.contains(ct_tags::slice_thickness)) {
            findings.push_back(
                {validation_severity::info, ct_tags::slice_thickness,
                 "SliceThickness (0018,0050) not present - slice geometry "
                 "information unavailable",
                 "CT-INFO-001"});
        }

        if (!dataset.contains(ct_tags::convolution_kernel)) {
            findings.push_back(
                {validation_severity::info, ct_tags::convolution_kernel,
                 "ConvolutionKernel (0018,1210) not present - reconstruction "
                 "algorithm unknown",
                 "CT-INFO-002"});
        }

        // Image Position (Patient) is Type 1C - required if
        // Frame of Reference Module is present
        if (dataset.contains(tags::frame_of_reference_uid) &&
            !dataset.contains(ct_tags::image_position_patient)) {
            findings.push_back(
                {validation_severity::warning,
                 ct_tags::image_position_patient,
                 "ImagePositionPatient (0020,0032) missing - required when "
                 "Frame of Reference is present",
                 "CT-WARN-001"});
        }

        // Image Orientation (Patient) is Type 1C
        if (dataset.contains(tags::frame_of_reference_uid) &&
            !dataset.contains(ct_tags::image_orientation_patient)) {
            findings.push_back(
                {validation_severity::warning,
                 ct_tags::image_orientation_patient,
                 "ImageOrientationPatient (0020,0037) missing - required when "
                 "Frame of Reference is present",
                 "CT-WARN-002"});
        }
    }
}

void ct_iod_validator::validate_image_pixel_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Type 1 attributes
    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::samples_per_pixel,
                              "SamplesPerPixel", findings);
        check_type1_attribute(dataset, tags::photometric_interpretation,
                              "PhotometricInterpretation", findings);
        check_type1_attribute(dataset, tags::rows, "Rows", findings);
        check_type1_attribute(dataset, tags::columns, "Columns", findings);
        check_type1_attribute(dataset, tags::bits_allocated, "BitsAllocated",
                              findings);
        check_type1_attribute(dataset, tags::bits_stored, "BitsStored",
                              findings);
        check_type1_attribute(dataset, tags::high_bit, "HighBit", findings);
        check_type1_attribute(dataset, tags::pixel_representation,
                              "PixelRepresentation", findings);
        check_type1_attribute(dataset, tags::pixel_data, "PixelData",
                              findings);
    }

    // Validate pixel data consistency
    if (options_.validate_pixel_data) {
        check_pixel_data_consistency(dataset, findings);
    }
}

void ct_iod_validator::validate_sop_common_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Type 1
    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::sop_class_uid, "SOPClassUID",
                              findings);
        check_type1_attribute(dataset, tags::sop_instance_uid,
                              "SOPInstanceUID", findings);
    }

    // Validate SOP Class UID is a CT storage class
    if (dataset.contains(tags::sop_class_uid)) {
        auto sop_class = dataset.get_string(tags::sop_class_uid);
        if (!sop_classes::is_ct_storage_sop_class(sop_class)) {
            findings.push_back(
                {validation_severity::error, tags::sop_class_uid,
                 "SOPClassUID is not a recognized CT Storage SOP Class: " +
                     sop_class,
                 "CT-ERR-001"});
        }
    }
}

// =============================================================================
// Attribute Validation Helpers
// =============================================================================

void ct_iod_validator::check_type1_attribute(
    const dicom_dataset& dataset, dicom_tag tag, std::string_view name,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(tag)) {
        findings.push_back(
            {validation_severity::error, tag,
             std::string("Type 1 attribute missing: ") + std::string(name) +
                 " (" + tag.to_string() + ")",
             "CT-TYPE1-MISSING"});
    } else {
        // Type 1 must have a value (cannot be empty)
        auto value = dataset.get_string(tag);
        if (value.empty()) {
            findings.push_back(
                {validation_severity::error, tag,
                 std::string("Type 1 attribute has empty value: ") +
                     std::string(name) + " (" + tag.to_string() + ")",
                 "CT-TYPE1-EMPTY"});
        }
    }
}

void ct_iod_validator::check_type2_attribute(
    const dicom_dataset& dataset, dicom_tag tag, std::string_view name,
    std::vector<validation_finding>& findings) const {

    // Type 2 must be present but can be empty
    if (!dataset.contains(tag)) {
        findings.push_back(
            {validation_severity::warning, tag,
             std::string("Type 2 attribute missing: ") + std::string(name) +
                 " (" + tag.to_string() + ")",
             "CT-TYPE2-MISSING"});
    }
}

void ct_iod_validator::check_modality(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(tags::modality)) {
        return;  // Already reported as Type 1 missing
    }

    auto modality = dataset.get_string(tags::modality);
    if (modality != "CT") {
        findings.push_back(
            {validation_severity::error, tags::modality,
             "Modality must be 'CT' for CT images, found: " + modality,
             "CT-ERR-002"});
    }
}

void ct_iod_validator::check_pixel_data_consistency(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Check BitsStored <= BitsAllocated
    auto bits_allocated =
        dataset.get_numeric<uint16_t>(tags::bits_allocated);
    auto bits_stored = dataset.get_numeric<uint16_t>(tags::bits_stored);
    auto high_bit = dataset.get_numeric<uint16_t>(tags::high_bit);

    if (bits_allocated && bits_stored) {
        if (*bits_stored > *bits_allocated) {
            findings.push_back(
                {validation_severity::error, tags::bits_stored,
                 "BitsStored (" + std::to_string(*bits_stored) +
                     ") exceeds BitsAllocated (" +
                     std::to_string(*bits_allocated) + ")",
                 "CT-ERR-003"});
        }
    }

    // Check HighBit == BitsStored - 1
    if (bits_stored && high_bit) {
        if (*high_bit != *bits_stored - 1) {
            findings.push_back(
                {validation_severity::warning, tags::high_bit,
                 "HighBit (" + std::to_string(*high_bit) +
                     ") should typically be BitsStored - 1 (" +
                     std::to_string(*bits_stored - 1) + ")",
                 "CT-WARN-003"});
        }
    }

    // CT images must be grayscale (MONOCHROME1 or MONOCHROME2)
    if (dataset.contains(tags::photometric_interpretation)) {
        auto photometric =
            dataset.get_string(tags::photometric_interpretation);
        if (!sop_classes::is_valid_ct_photometric(photometric)) {
            findings.push_back(
                {validation_severity::error,
                 tags::photometric_interpretation,
                 "CT images must use MONOCHROME1 or MONOCHROME2, found: " +
                     photometric,
                 "CT-ERR-004"});
        }
    }

    // CT images must have SamplesPerPixel = 1
    auto samples = dataset.get_numeric<uint16_t>(tags::samples_per_pixel);
    if (samples && *samples != 1) {
        findings.push_back(
            {validation_severity::error, tags::samples_per_pixel,
             "CT images require SamplesPerPixel = 1, found: " +
                 std::to_string(*samples),
             "CT-ERR-005"});
    }

    // CT images typically use signed pixel representation (for HU values)
    auto pixel_rep =
        dataset.get_numeric<uint16_t>(tags::pixel_representation);
    if (pixel_rep && *pixel_rep != 1) {
        findings.push_back(
            {validation_severity::info, tags::pixel_representation,
             "CT images typically use signed pixel representation "
             "(PixelRepresentation = 1) for Hounsfield Unit values",
             "CT-INFO-003"});
    }
}

// =============================================================================
// Convenience Functions
// =============================================================================

validation_result validate_ct_iod(const dicom_dataset& dataset) {
    ct_iod_validator validator;
    return validator.validate(dataset);
}

bool is_valid_ct_dataset(const dicom_dataset& dataset) {
    ct_iod_validator validator;
    return validator.quick_check(dataset);
}

}  // namespace kcenon::pacs::services::validation
