// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @file mr_iod_validator.cpp
 * @brief Implementation of MR Image IOD Validator
 *
 * @see DICOM PS3.3 Section A.4 - MR Image IOD
 * @see Issue #718 - Add MR Image IOD Validator
 */

#include "pacs/services/validation/mr_iod_validator.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/services/sop_classes/mr_storage.hpp"

namespace pacs::services::validation {

using namespace pacs::core;

// =============================================================================
// mr_iod_validator Implementation
// =============================================================================

mr_iod_validator::mr_iod_validator(const mr_validation_options& options)
    : options_(options) {}

validation_result mr_iod_validator::validate(
    const dicom_dataset& dataset) const {
    validation_result result;
    result.is_valid = true;

    // Validate mandatory modules (PS3.3 Table A.4-1)
    if (options_.check_type1 || options_.check_type2) {
        validate_patient_module(dataset, result.findings);
        validate_general_study_module(dataset, result.findings);
        validate_general_series_module(dataset, result.findings);
        validate_frame_of_reference_module(dataset, result.findings);
        validate_general_equipment_module(dataset, result.findings);
        validate_sop_common_module(dataset, result.findings);
    }

    if (options_.validate_mr_params) {
        validate_mr_image_module(dataset, result.findings);
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

bool mr_iod_validator::quick_check(const dicom_dataset& dataset) const {
    // Check only Type 1 required attributes for quick validation

    // General Study Module Type 1
    if (!dataset.contains(tags::study_instance_uid)) return false;

    // General Series Module Type 1
    if (!dataset.contains(tags::modality)) return false;
    if (!dataset.contains(tags::series_instance_uid)) return false;

    // Check modality is MR
    auto modality = dataset.get_string(tags::modality);
    if (modality != "MR") return false;

    // Frame of Reference Module Type 1
    if (!dataset.contains(tags::frame_of_reference_uid)) return false;

    // MR Image Module Type 1
    if (!dataset.contains(mr_tags::scanning_sequence)) return false;
    if (!dataset.contains(mr_tags::sequence_variant)) return false;

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

const mr_validation_options& mr_iod_validator::options() const noexcept {
    return options_;
}

void mr_iod_validator::set_options(const mr_validation_options& options) {
    options_ = options;
}

// =============================================================================
// Module Validation Methods
// =============================================================================

void mr_iod_validator::validate_patient_module(
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

void mr_iod_validator::validate_general_study_module(
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

void mr_iod_validator::validate_general_series_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Type 1
    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::modality, "Modality", findings);
        check_type1_attribute(dataset, tags::series_instance_uid,
                              "SeriesInstanceUID", findings);

        // Modality must be "MR"
        check_modality(dataset, findings);
    }

    // Type 2
    if (options_.check_type2) {
        check_type2_attribute(dataset, tags::series_number, "SeriesNumber",
                              findings);
    }
}

void mr_iod_validator::validate_frame_of_reference_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Frame of Reference UID is Type 1 for MR
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

void mr_iod_validator::validate_general_equipment_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Manufacturer is Type 2
    if (options_.check_type2) {
        check_type2_attribute(dataset, tags::manufacturer, "Manufacturer",
                              findings);
    }
}

void mr_iod_validator::validate_mr_image_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // MR Image Module (PS3.3 Section C.8.3.1)

    // Type 1 attributes
    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::image_type, "ImageType",
                              findings);
        check_type1_attribute(dataset, mr_tags::scanning_sequence,
                              "ScanningSequence", findings);
        check_type1_attribute(dataset, mr_tags::sequence_variant,
                              "SequenceVariant", findings);
    }

    // Type 2 attributes specific to MR Image Module
    if (options_.check_type2) {
        check_type2_attribute(dataset, mr_tags::scan_options, "ScanOptions",
                              findings);
        check_type2_attribute(dataset, mr_tags::mr_acquisition_type,
                              "MRAcquisitionType", findings);
        check_type2_attribute(dataset, mr_tags::echo_time, "EchoTime",
                              findings);
        check_type2_attribute(dataset, mr_tags::echo_train_length,
                              "EchoTrainLength", findings);
        check_type2_attribute(dataset, mr_tags::magnetic_field_strength,
                              "MagneticFieldStrength", findings);
    }

    // Type 2C - RepetitionTime is required if ImageType Value 1 is ORIGINAL
    if (options_.check_conditional) {
        if (dataset.contains(tags::image_type)) {
            auto image_type = dataset.get_string(tags::image_type);
            if (image_type.find("ORIGINAL") != std::string::npos &&
                !dataset.contains(mr_tags::repetition_time)) {
                findings.push_back(
                    {validation_severity::warning, mr_tags::repetition_time,
                     "RepetitionTime (0018,0080) missing - required when "
                     "ImageType Value 1 is ORIGINAL",
                     "MR-WARN-001"});
            }
        }

        // InversionTime is Type 2C - required if ScanningSequence contains IR
        if (dataset.contains(mr_tags::scanning_sequence)) {
            auto seq = dataset.get_string(mr_tags::scanning_sequence);
            if (seq.find("IR") != std::string::npos &&
                !dataset.contains(mr_tags::inversion_time)) {
                findings.push_back(
                    {validation_severity::warning, mr_tags::inversion_time,
                     "InversionTime (0018,0082) missing - required when "
                     "ScanningSequence contains IR",
                     "MR-WARN-002"});
            }
        }

        // SliceThickness informational check
        if (!dataset.contains(mr_tags::slice_thickness)) {
            findings.push_back(
                {validation_severity::info, mr_tags::slice_thickness,
                 "SliceThickness (0018,0050) not present - slice geometry "
                 "information unavailable",
                 "MR-INFO-001"});
        }

        // Image Position (Patient) is Type 1C - required if
        // Frame of Reference Module is present
        if (dataset.contains(tags::frame_of_reference_uid) &&
            !dataset.contains(mr_tags::image_position_patient)) {
            findings.push_back(
                {validation_severity::warning,
                 mr_tags::image_position_patient,
                 "ImagePositionPatient (0020,0032) missing - required when "
                 "Frame of Reference is present",
                 "MR-WARN-003"});
        }

        // Image Orientation (Patient) is Type 1C
        if (dataset.contains(tags::frame_of_reference_uid) &&
            !dataset.contains(mr_tags::image_orientation_patient)) {
            findings.push_back(
                {validation_severity::warning,
                 mr_tags::image_orientation_patient,
                 "ImageOrientationPatient (0020,0037) missing - required when "
                 "Frame of Reference is present",
                 "MR-WARN-004"});
        }
    }
}

void mr_iod_validator::validate_image_pixel_module(
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

void mr_iod_validator::validate_sop_common_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Type 1
    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::sop_class_uid, "SOPClassUID",
                              findings);
        check_type1_attribute(dataset, tags::sop_instance_uid,
                              "SOPInstanceUID", findings);
    }

    // Validate SOP Class UID is an MR storage class
    if (dataset.contains(tags::sop_class_uid)) {
        auto sop_class = dataset.get_string(tags::sop_class_uid);
        if (!sop_classes::is_mr_storage_sop_class(sop_class)) {
            findings.push_back(
                {validation_severity::error, tags::sop_class_uid,
                 "SOPClassUID is not a recognized MR Storage SOP Class: " +
                     sop_class,
                 "MR-ERR-001"});
        }
    }
}

// =============================================================================
// Attribute Validation Helpers
// =============================================================================

void mr_iod_validator::check_type1_attribute(
    const dicom_dataset& dataset, dicom_tag tag, std::string_view name,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(tag)) {
        findings.push_back(
            {validation_severity::error, tag,
             std::string("Type 1 attribute missing: ") + std::string(name) +
                 " (" + tag.to_string() + ")",
             "MR-TYPE1-MISSING"});
    } else {
        // Type 1 must have a value (cannot be empty)
        auto value = dataset.get_string(tag);
        if (value.empty()) {
            findings.push_back(
                {validation_severity::error, tag,
                 std::string("Type 1 attribute has empty value: ") +
                     std::string(name) + " (" + tag.to_string() + ")",
                 "MR-TYPE1-EMPTY"});
        }
    }
}

void mr_iod_validator::check_type2_attribute(
    const dicom_dataset& dataset, dicom_tag tag, std::string_view name,
    std::vector<validation_finding>& findings) const {

    // Type 2 must be present but can be empty
    if (!dataset.contains(tag)) {
        findings.push_back(
            {validation_severity::warning, tag,
             std::string("Type 2 attribute missing: ") + std::string(name) +
                 " (" + tag.to_string() + ")",
             "MR-TYPE2-MISSING"});
    }
}

void mr_iod_validator::check_modality(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(tags::modality)) {
        return;  // Already reported as Type 1 missing
    }

    auto modality = dataset.get_string(tags::modality);
    if (modality != "MR") {
        findings.push_back(
            {validation_severity::error, tags::modality,
             "Modality must be 'MR' for MR images, found: " + modality,
             "MR-ERR-002"});
    }
}

void mr_iod_validator::check_pixel_data_consistency(
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
                 "MR-ERR-003"});
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
                 "MR-WARN-005"});
        }
    }

    // MR images must be grayscale (MONOCHROME1 or MONOCHROME2)
    if (dataset.contains(tags::photometric_interpretation)) {
        auto photometric =
            dataset.get_string(tags::photometric_interpretation);
        if (!sop_classes::is_valid_mr_photometric(photometric)) {
            findings.push_back(
                {validation_severity::error,
                 tags::photometric_interpretation,
                 "MR images must use MONOCHROME1 or MONOCHROME2, found: " +
                     photometric,
                 "MR-ERR-004"});
        }
    }

    // MR images must have SamplesPerPixel = 1
    auto samples = dataset.get_numeric<uint16_t>(tags::samples_per_pixel);
    if (samples && *samples != 1) {
        findings.push_back(
            {validation_severity::error, tags::samples_per_pixel,
             "MR images require SamplesPerPixel = 1, found: " +
                 std::to_string(*samples),
             "MR-ERR-005"});
    }
}

// =============================================================================
// Convenience Functions
// =============================================================================

validation_result validate_mr_iod(const dicom_dataset& dataset) {
    mr_iod_validator validator;
    return validator.validate(dataset);
}

bool is_valid_mr_dataset(const dicom_dataset& dataset) {
    mr_iod_validator validator;
    return validator.quick_check(dataset);
}

}  // namespace pacs::services::validation
