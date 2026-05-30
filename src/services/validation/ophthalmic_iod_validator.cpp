// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file ophthalmic_iod_validator.cpp
 * @brief Implementation of Ophthalmic Photography/Tomography Image IOD Validator
 *
 * @see DICOM PS3.3 Section A.39 - Ophthalmic Photography IODs
 * @see DICOM PS3.3 Section A.40 - Ophthalmic Tomography Image IOD
 * @see Issue #830 - Add Ophthalmic IOD validator
 */

#include "kcenon/pacs/services/validation/ophthalmic_iod_validator.h"
#include "kcenon/pacs/core/dicom_tag_constants.h"
#include "kcenon/pacs/services/sop_classes/ophthalmic_storage.h"

namespace kcenon::pacs::services::validation {

using namespace kcenon::pacs::core;

// =============================================================================
// ophthalmic_iod_validator Implementation
// =============================================================================

ophthalmic_iod_validator::ophthalmic_iod_validator(
    const ophthalmic_validation_options& options)
    : options_(options) {}

validation_result ophthalmic_iod_validator::validate(
    const dicom_dataset& dataset) const {
    validation_result result;
    result.is_valid = true;

    // Validate mandatory modules (PS3.3 Tables A.39-1 / A.40-1)
    if (options_.check_type1 || options_.check_type2) {
        validate_patient_module(dataset, result.findings);
        validate_general_study_module(dataset, result.findings);
        validate_general_series_module(dataset, result.findings);
        validate_sop_common_module(dataset, result.findings);
    }

    if (options_.validate_ophthalmic_params) {
        validate_ophthalmic_image_module(dataset, result.findings);
    }

    if (options_.validate_pixel_data) {
        validate_image_pixel_module(dataset, result.findings);
    }

    // Multi-frame Module is conditional — applies to OCT images
    if (options_.check_conditional) {
        validate_multiframe_module(dataset, result.findings);
        validate_acquisition_context_module(dataset, result.findings);
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

bool ophthalmic_iod_validator::quick_check(const dicom_dataset& dataset) const {
    // General Study Module Type 1
    if (!dataset.contains(tags::study_instance_uid)) return false;

    // General Series Module Type 1
    if (!dataset.contains(tags::modality)) return false;
    if (!dataset.contains(tags::series_instance_uid)) return false;

    // Check modality is OP or OPT
    auto modality = dataset.get_string(tags::modality);
    if (modality != "OP" && modality != "OPT") return false;

    // Ophthalmic Image Module Type 1
    if (!dataset.contains(tags::image_type)) return false;
    if (!dataset.contains(ophthalmic_iod_tags::image_laterality)) return false;

    // Image Pixel Module Type 1
    if (!dataset.contains(tags::samples_per_pixel)) return false;
    if (!dataset.contains(tags::photometric_interpretation)) return false;
    if (!dataset.contains(tags::rows)) return false;
    if (!dataset.contains(tags::columns)) return false;
    if (!dataset.contains(tags::bits_allocated)) return false;
    if (!dataset.contains(tags::bits_stored)) return false;
    if (!dataset.contains(tags::high_bit)) return false;
    if (!dataset.contains(tags::pixel_representation)) return false;

    // SOP Common Module Type 1
    if (!dataset.contains(tags::sop_class_uid)) return false;
    if (!dataset.contains(tags::sop_instance_uid)) return false;

    return true;
}

const ophthalmic_validation_options&
ophthalmic_iod_validator::options() const noexcept {
    return options_;
}

void ophthalmic_iod_validator::set_options(
    const ophthalmic_validation_options& options) {
    options_ = options;
}

// =============================================================================
// Module Validation Methods
// =============================================================================

void ophthalmic_iod_validator::validate_patient_module(
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

void ophthalmic_iod_validator::validate_general_study_module(
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

void ophthalmic_iod_validator::validate_general_series_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Type 1
    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::modality, "Modality", findings);
        check_type1_attribute(dataset, tags::series_instance_uid,
                              "SeriesInstanceUID", findings);

        // Modality must be "OP" or "OPT"
        check_modality(dataset, findings);
    }

    // Type 2
    if (options_.check_type2) {
        check_type2_attribute(dataset, tags::series_number, "SeriesNumber",
                              findings);
    }
}

void ophthalmic_iod_validator::validate_ophthalmic_image_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // ImageType is Type 1
    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::image_type, "ImageType",
                              findings);
    }

    // Image Laterality is Type 1 — must be R, L, or B
    if (options_.validate_laterality) {
        check_type1_attribute(dataset, ophthalmic_iod_tags::image_laterality,
                              "ImageLaterality", findings);
        check_laterality(dataset, findings);
    }

    // Informational checks for ophthalmic-specific attributes
    if (options_.check_conditional) {
        // Anatomic Region Sequence — Type 1 for ophthalmic
        if (!dataset.contains(
                ophthalmic_iod_tags::anatomic_region_sequence)) {
            findings.push_back(
                {validation_severity::warning,
                 ophthalmic_iod_tags::anatomic_region_sequence,
                 "AnatomicRegionSequence (0008,2218) missing - anatomic "
                 "region unspecified",
                 "OPHTH-WARN-001"});
        }

        // Acquisition Device Type Code Sequence — Type 2
        if (!dataset.contains(
                ophthalmic_iod_tags::acquisition_device_type_code_sequence)) {
            findings.push_back(
                {validation_severity::info,
                 ophthalmic_iod_tags::acquisition_device_type_code_sequence,
                 "AcquisitionDeviceTypeCodeSequence (0022,0015) not present "
                 "- device type unavailable",
                 "OPHTH-INFO-001"});
        }

        // Horizontal Field of View — Type 2C
        if (!dataset.contains(
                ophthalmic_iod_tags::horizontal_field_of_view)) {
            findings.push_back(
                {validation_severity::info,
                 ophthalmic_iod_tags::horizontal_field_of_view,
                 "HorizontalFieldOfView (0022,000C) not present - field of "
                 "view unavailable",
                 "OPHTH-INFO-002"});
        }

        // Pupil Dilated — Type 2C
        if (!dataset.contains(ophthalmic_iod_tags::pupil_dilated)) {
            findings.push_back(
                {validation_severity::info,
                 ophthalmic_iod_tags::pupil_dilated,
                 "PupilDilated (0022,000D) not present - pupil dilation "
                 "status unknown",
                 "OPHTH-INFO-003"});
        }
    }
}

void ophthalmic_iod_validator::validate_image_pixel_module(
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
    }

    // Validate pixel data consistency
    if (options_.validate_pixel_data) {
        check_pixel_data_consistency(dataset, findings);
    }
}

void ophthalmic_iod_validator::validate_multiframe_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Multi-frame Module is conditional — required for OCT images
    if (!dataset.contains(tags::sop_class_uid)) return;

    auto sop_class = dataset.get_string(tags::sop_class_uid);
    bool is_oct =
        (sop_class == sop_classes::ophthalmic_tomography_storage_uid ||
         sop_class == sop_classes::ophthalmic_oct_bscan_analysis_storage_uid);

    if (is_oct) {
        // NumberOfFrames is Type 1 for OCT multi-frame
        if (!dataset.contains(ophthalmic_iod_tags::number_of_frames)) {
            findings.push_back(
                {validation_severity::error,
                 ophthalmic_iod_tags::number_of_frames,
                 "NumberOfFrames (0028,0008) required for OCT multi-frame "
                 "images",
                 "OPHTH-ERR-001"});
        }
    }
}

void ophthalmic_iod_validator::validate_acquisition_context_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Acquisition Context Module is mandatory
    if (!dataset.contains(ophthalmic_iod_tags::acquisition_context_sequence)) {
        findings.push_back(
            {validation_severity::info,
             ophthalmic_iod_tags::acquisition_context_sequence,
             "AcquisitionContextSequence (0040,0555) not present - "
             "acquisition context unavailable",
             "OPHTH-INFO-004"});
    }
}

void ophthalmic_iod_validator::validate_sop_common_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Type 1
    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::sop_class_uid, "SOPClassUID",
                              findings);
        check_type1_attribute(dataset, tags::sop_instance_uid,
                              "SOPInstanceUID", findings);
    }

    // Validate SOP Class UID is an ophthalmic storage class
    if (dataset.contains(tags::sop_class_uid)) {
        auto sop_class = dataset.get_string(tags::sop_class_uid);
        if (!sop_classes::is_ophthalmic_storage_sop_class(sop_class)) {
            findings.push_back(
                {validation_severity::error, tags::sop_class_uid,
                 "SOPClassUID is not a recognized Ophthalmic Storage SOP "
                 "Class: " + sop_class,
                 "OPHTH-ERR-002"});
        }
    }
}

// =============================================================================
// Attribute Validation Helpers
// =============================================================================

void ophthalmic_iod_validator::check_type1_attribute(
    const dicom_dataset& dataset, dicom_tag tag, std::string_view name,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(tag)) {
        findings.push_back(
            {validation_severity::error, tag,
             std::string("Type 1 attribute missing: ") + std::string(name) +
                 " (" + tag.to_string() + ")",
             "OPHTH-TYPE1-MISSING"});
    } else {
        // Type 1 must have a value (cannot be empty)
        auto value = dataset.get_string(tag);
        if (value.empty()) {
            findings.push_back(
                {validation_severity::error, tag,
                 std::string("Type 1 attribute has empty value: ") +
                     std::string(name) + " (" + tag.to_string() + ")",
                 "OPHTH-TYPE1-EMPTY"});
        }
    }
}

void ophthalmic_iod_validator::check_type2_attribute(
    const dicom_dataset& dataset, dicom_tag tag, std::string_view name,
    std::vector<validation_finding>& findings) const {

    // Type 2 must be present but can be empty
    if (!dataset.contains(tag)) {
        findings.push_back(
            {validation_severity::warning, tag,
             std::string("Type 2 attribute missing: ") + std::string(name) +
                 " (" + tag.to_string() + ")",
             "OPHTH-TYPE2-MISSING"});
    }
}

void ophthalmic_iod_validator::check_modality(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(tags::modality)) {
        return;  // Already reported as Type 1 missing
    }

    auto modality = dataset.get_string(tags::modality);
    if (modality != "OP" && modality != "OPT") {
        findings.push_back(
            {validation_severity::error, tags::modality,
             "Modality must be 'OP' or 'OPT' for ophthalmic images, "
             "found: " + modality,
             "OPHTH-ERR-003"});
    }
}

void ophthalmic_iod_validator::check_laterality(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(ophthalmic_iod_tags::image_laterality)) {
        return;  // Already reported as Type 1 missing
    }

    auto laterality =
        dataset.get_string(ophthalmic_iod_tags::image_laterality);
    if (laterality != "R" && laterality != "L" && laterality != "B") {
        findings.push_back(
            {validation_severity::error,
             ophthalmic_iod_tags::image_laterality,
             "ImageLaterality must be 'R' (Right), 'L' (Left), or "
             "'B' (Both), found: " + laterality,
             "OPHTH-ERR-004"});
    }
}

void ophthalmic_iod_validator::check_pixel_data_consistency(
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
                 "OPHTH-ERR-005"});
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
                 "OPHTH-WARN-002"});
        }
    }

    // Validate PhotometricInterpretation for ophthalmic
    if (dataset.contains(tags::photometric_interpretation)) {
        auto photometric =
            dataset.get_string(tags::photometric_interpretation);
        if (!sop_classes::is_valid_ophthalmic_photometric(photometric)) {
            findings.push_back(
                {validation_severity::error,
                 tags::photometric_interpretation,
                 "Ophthalmic images must use MONOCHROME1, MONOCHROME2, "
                 "RGB, YBR_FULL_422, or PALETTE COLOR, found: " + photometric,
                 "OPHTH-ERR-006"});
        }
    }

    // Ophthalmic SamplesPerPixel must be 1 (grayscale) or 3 (color)
    auto samples = dataset.get_numeric<uint16_t>(tags::samples_per_pixel);
    if (samples && *samples != 1 && *samples != 3) {
        findings.push_back(
            {validation_severity::error, tags::samples_per_pixel,
             "Ophthalmic images require SamplesPerPixel of 1 or 3, "
             "found: " + std::to_string(*samples),
             "OPHTH-ERR-007"});
    }

    // Ophthalmic images use unsigned pixel representation
    auto pixel_rep =
        dataset.get_numeric<uint16_t>(tags::pixel_representation);
    if (pixel_rep && *pixel_rep != 0) {
        findings.push_back(
            {validation_severity::warning, tags::pixel_representation,
             "Ophthalmic images typically use unsigned pixel representation "
             "(PixelRepresentation = 0)",
             "OPHTH-WARN-003"});
    }
}

// =============================================================================
// Convenience Functions
// =============================================================================

validation_result validate_ophthalmic_iod(const dicom_dataset& dataset) {
    ophthalmic_iod_validator validator;
    return validator.validate(dataset);
}

bool is_valid_ophthalmic_dataset(const dicom_dataset& dataset) {
    ophthalmic_iod_validator validator;
    return validator.quick_check(dataset);
}

}  // namespace kcenon::pacs::services::validation
