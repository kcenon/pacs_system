// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file wsi_iod_validator.cpp
 * @brief Implementation of VL Whole Slide Microscopy Image IOD Validator
 *
 * @see DICOM PS3.3 Section A.32.8 - VL Whole Slide Microscopy Image IOD
 * @see Issue #826 - Add WSI IOD validator
 */

#include "kcenon/pacs/services/validation/wsi_iod_validator.h"
#include "kcenon/pacs/core/dicom_tag_constants.h"
#include "kcenon/pacs/services/sop_classes/wsi_storage.h"

namespace kcenon::pacs::services::validation {

using namespace kcenon::pacs::core;

// =============================================================================
// wsi_iod_validator Implementation
// =============================================================================

wsi_iod_validator::wsi_iod_validator(const wsi_validation_options& options)
    : options_(options) {}

validation_result wsi_iod_validator::validate(
    const dicom_dataset& dataset) const {
    validation_result result;
    result.is_valid = true;

    // Validate mandatory modules (PS3.3 Table A.32.8-1)
    if (options_.check_type1 || options_.check_type2) {
        validate_patient_module(dataset, result.findings);
        validate_general_study_module(dataset, result.findings);
        validate_general_series_module(dataset, result.findings);
        validate_sop_common_module(dataset, result.findings);
    }

    if (options_.validate_wsi_params) {
        validate_wsi_image_module(dataset, result.findings);
        validate_multiframe_dimension_module(dataset, result.findings);
    }

    if (options_.validate_optical_path) {
        validate_optical_path_module(dataset, result.findings);
    }

    if (options_.validate_pixel_data) {
        validate_image_pixel_module(dataset, result.findings);
    }

    // Specimen Module is User Optional — info-level checks
    if (options_.check_conditional) {
        validate_specimen_module(dataset, result.findings);
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

bool wsi_iod_validator::quick_check(const dicom_dataset& dataset) const {
    // General Study Module Type 1
    if (!dataset.contains(tags::study_instance_uid)) return false;

    // General Series Module Type 1
    if (!dataset.contains(tags::modality)) return false;
    if (!dataset.contains(tags::series_instance_uid)) return false;

    // Check modality is SM
    auto modality = dataset.get_string(tags::modality);
    if (modality != "SM") return false;

    // WSI Image Module Type 1
    if (!dataset.contains(tags::image_type)) return false;
    if (!dataset.contains(wsi_iod_tags::total_pixel_matrix_columns))
        return false;
    if (!dataset.contains(wsi_iod_tags::total_pixel_matrix_rows)) return false;

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

const wsi_validation_options& wsi_iod_validator::options() const noexcept {
    return options_;
}

void wsi_iod_validator::set_options(const wsi_validation_options& options) {
    options_ = options;
}

// =============================================================================
// Module Validation Methods
// =============================================================================

void wsi_iod_validator::validate_patient_module(
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

void wsi_iod_validator::validate_general_study_module(
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

void wsi_iod_validator::validate_general_series_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Type 1
    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::modality, "Modality", findings);
        check_type1_attribute(dataset, tags::series_instance_uid,
                              "SeriesInstanceUID", findings);

        // Modality must be "SM"
        check_modality(dataset, findings);
    }

    // Type 2
    if (options_.check_type2) {
        check_type2_attribute(dataset, tags::series_number, "SeriesNumber",
                              findings);
    }
}

void wsi_iod_validator::validate_wsi_image_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // WSI Image Module (PS3.3 Section C.8.12.4)

    // ImageType is Type 1
    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::image_type, "ImageType",
                              findings);
    }

    // Total Pixel Matrix Columns/Rows are Type 1
    check_type1_attribute(dataset, wsi_iod_tags::total_pixel_matrix_columns,
                          "TotalPixelMatrixColumns", findings);
    check_type1_attribute(dataset, wsi_iod_tags::total_pixel_matrix_rows,
                          "TotalPixelMatrixRows", findings);

    // Number of Frames is Type 1 for multi-frame WSI
    if (options_.check_type1) {
        check_type1_attribute(dataset, wsi_iod_tags::number_of_frames,
                              "NumberOfFrames", findings);
    }

    // Informational checks for WSI-specific attributes
    if (options_.check_conditional) {
        // Imaged Volume Width/Height — important for physical measurements
        if (!dataset.contains(wsi_iod_tags::imaged_volume_width)) {
            findings.push_back(
                {validation_severity::info, wsi_iod_tags::imaged_volume_width,
                 "ImagedVolumeWidth (0048,0001) not present - physical "
                 "dimensions unavailable",
                 "WSI-INFO-001"});
        }

        if (!dataset.contains(wsi_iod_tags::imaged_volume_height)) {
            findings.push_back(
                {validation_severity::info, wsi_iod_tags::imaged_volume_height,
                 "ImagedVolumeHeight (0048,0002) not present - physical "
                 "dimensions unavailable",
                 "WSI-INFO-002"});
        }

        // Image Orientation (Slide) is Type 1C
        if (dataset.contains(wsi_iod_tags::total_pixel_matrix_columns) &&
            !dataset.contains(wsi_iod_tags::image_orientation_slide)) {
            findings.push_back(
                {validation_severity::warning,
                 wsi_iod_tags::image_orientation_slide,
                 "ImageOrientationSlide (0048,0102) missing - slide "
                 "orientation unknown",
                 "WSI-WARN-001"});
        }
    }
}

void wsi_iod_validator::validate_optical_path_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Optical Path Module — Optical Path Identifier is Type 1
    if (options_.check_type1) {
        check_type1_attribute(dataset, wsi_iod_tags::optical_path_identifier,
                              "OpticalPathIdentifier", findings);
    }

    // Optical Path Description is Type 3 (optional), info if missing
    if (options_.check_conditional) {
        if (!dataset.contains(wsi_iod_tags::optical_path_description)) {
            findings.push_back(
                {validation_severity::info,
                 wsi_iod_tags::optical_path_description,
                 "OpticalPathDescription (0048,0106) not present - optical "
                 "path details unavailable",
                 "WSI-INFO-003"});
        }
    }
}

void wsi_iod_validator::validate_multiframe_dimension_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Dimension Organization Type is Type 1
    check_type1_attribute(dataset, wsi_iod_tags::dimension_organization_type,
                          "DimensionOrganizationType", findings);

    // Validate the Dimension Organization Type value
    if (dataset.contains(wsi_iod_tags::dimension_organization_type)) {
        auto org_type =
            dataset.get_string(wsi_iod_tags::dimension_organization_type);
        if (org_type != "TILED_FULL" && org_type != "TILED_SPARSE") {
            findings.push_back(
                {validation_severity::error,
                 wsi_iod_tags::dimension_organization_type,
                 "DimensionOrganizationType must be 'TILED_FULL' or "
                 "'TILED_SPARSE' for WSI, found: " + org_type,
                 "WSI-ERR-001"});
        }
    }
}

void wsi_iod_validator::validate_image_pixel_module(
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

void wsi_iod_validator::validate_specimen_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Specimen Module is User Optional (U) for WSI
    // Provide info-level findings when specimen data is absent

    if (!dataset.contains(wsi_iod_tags::specimen_identifier)) {
        findings.push_back(
            {validation_severity::info, wsi_iod_tags::specimen_identifier,
             "SpecimenIdentifier (0040,0551) not present - specimen "
             "tracking unavailable",
             "WSI-INFO-004"});
    }

    if (!dataset.contains(wsi_iod_tags::container_identifier)) {
        findings.push_back(
            {validation_severity::info, wsi_iod_tags::container_identifier,
             "ContainerIdentifier (0040,0512) not present - container "
             "tracking unavailable",
             "WSI-INFO-005"});
    }
}

void wsi_iod_validator::validate_sop_common_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Type 1
    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::sop_class_uid, "SOPClassUID",
                              findings);
        check_type1_attribute(dataset, tags::sop_instance_uid,
                              "SOPInstanceUID", findings);
    }

    // Validate SOP Class UID is a WSI storage class
    if (dataset.contains(tags::sop_class_uid)) {
        auto sop_class = dataset.get_string(tags::sop_class_uid);
        if (!sop_classes::is_wsi_storage_sop_class(sop_class)) {
            findings.push_back(
                {validation_severity::error, tags::sop_class_uid,
                 "SOPClassUID is not a recognized WSI Storage SOP Class: " +
                     sop_class,
                 "WSI-ERR-002"});
        }
    }
}

// =============================================================================
// Attribute Validation Helpers
// =============================================================================

void wsi_iod_validator::check_type1_attribute(
    const dicom_dataset& dataset, dicom_tag tag, std::string_view name,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(tag)) {
        findings.push_back(
            {validation_severity::error, tag,
             std::string("Type 1 attribute missing: ") + std::string(name) +
                 " (" + tag.to_string() + ")",
             "WSI-TYPE1-MISSING"});
    } else {
        // Type 1 must have a value (cannot be empty)
        auto value = dataset.get_string(tag);
        if (value.empty()) {
            findings.push_back(
                {validation_severity::error, tag,
                 std::string("Type 1 attribute has empty value: ") +
                     std::string(name) + " (" + tag.to_string() + ")",
                 "WSI-TYPE1-EMPTY"});
        }
    }
}

void wsi_iod_validator::check_type2_attribute(
    const dicom_dataset& dataset, dicom_tag tag, std::string_view name,
    std::vector<validation_finding>& findings) const {

    // Type 2 must be present but can be empty
    if (!dataset.contains(tag)) {
        findings.push_back(
            {validation_severity::warning, tag,
             std::string("Type 2 attribute missing: ") + std::string(name) +
                 " (" + tag.to_string() + ")",
             "WSI-TYPE2-MISSING"});
    }
}

void wsi_iod_validator::check_modality(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(tags::modality)) {
        return;  // Already reported as Type 1 missing
    }

    auto modality = dataset.get_string(tags::modality);
    if (modality != "SM") {
        findings.push_back(
            {validation_severity::error, tags::modality,
             "Modality must be 'SM' for WSI images, found: " + modality,
             "WSI-ERR-003"});
    }
}

void wsi_iod_validator::check_pixel_data_consistency(
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
                 "WSI-ERR-004"});
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
                 "WSI-WARN-002"});
        }
    }

    // Validate PhotometricInterpretation for WSI
    if (dataset.contains(tags::photometric_interpretation)) {
        auto photometric =
            dataset.get_string(tags::photometric_interpretation);
        if (!sop_classes::is_valid_wsi_photometric(photometric)) {
            findings.push_back(
                {validation_severity::error,
                 tags::photometric_interpretation,
                 "WSI images must use RGB, YBR_FULL_422, YBR_ICT, YBR_RCT, "
                 "or MONOCHROME2, found: " + photometric,
                 "WSI-ERR-005"});
        }
    }

    // WSI SamplesPerPixel must be 1 (grayscale/fluorescence) or 3 (color)
    auto samples = dataset.get_numeric<uint16_t>(tags::samples_per_pixel);
    if (samples && *samples != 1 && *samples != 3) {
        findings.push_back(
            {validation_severity::error, tags::samples_per_pixel,
             "WSI images require SamplesPerPixel of 1 or 3, found: " +
                 std::to_string(*samples),
             "WSI-ERR-006"});
    }

    // WSI images must use unsigned pixel representation
    auto pixel_rep =
        dataset.get_numeric<uint16_t>(tags::pixel_representation);
    if (pixel_rep && *pixel_rep != 0) {
        findings.push_back(
            {validation_severity::warning, tags::pixel_representation,
             "WSI images typically use unsigned pixel representation "
             "(PixelRepresentation = 0)",
             "WSI-WARN-003"});
    }
}

// =============================================================================
// Convenience Functions
// =============================================================================

validation_result validate_wsi_iod(const dicom_dataset& dataset) {
    wsi_iod_validator validator;
    return validator.validate(dataset);
}

bool is_valid_wsi_dataset(const dicom_dataset& dataset) {
    wsi_iod_validator validator;
    return validator.quick_check(dataset);
}

}  // namespace kcenon::pacs::services::validation
