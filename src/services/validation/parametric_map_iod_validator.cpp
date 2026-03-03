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
 * @file parametric_map_iod_validator.cpp
 * @brief Implementation of Parametric Map IOD Validator
 *
 * @see Issue #834 - Add Parametric Map IOD validator
 */

#include "pacs/services/validation/parametric_map_iod_validator.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/services/sop_classes/parametric_map_storage.hpp"

namespace pacs::services::validation {

using namespace pacs::core;

// =============================================================================
// parametric_map_iod_validator Implementation
// =============================================================================

parametric_map_iod_validator::parametric_map_iod_validator(
    const pmap_validation_options& options)
    : options_(options) {}

validation_result
parametric_map_iod_validator::validate(const dicom_dataset& dataset) const {
    validation_result result;
    result.is_valid = true;

    // Validate mandatory modules
    if (options_.check_type1 || options_.check_type2) {
        validate_patient_module(dataset, result.findings);
        validate_general_study_module(dataset, result.findings);
        validate_general_series_module(dataset, result.findings);
        validate_general_equipment_module(dataset, result.findings);
        validate_enhanced_general_equipment_module(dataset, result.findings);
        validate_sop_common_module(dataset, result.findings);
    }

    // Parametric Map Image Module (Content Label, RWVM)
    validate_parametric_map_image_module(dataset, result.findings);

    if (options_.validate_pixel_data) {
        validate_image_pixel_module(dataset, result.findings);
    }

    if (options_.validate_multiframe) {
        validate_multiframe_functional_groups_module(dataset, result.findings);
        validate_multiframe_dimension_module(dataset, result.findings);
    }

    if (options_.validate_references) {
        validate_common_instance_reference_module(dataset, result.findings);
    }

    // Check for errors
    for (const auto& finding : result.findings) {
        if (finding.severity == validation_severity::error) {
            result.is_valid = false;
            break;
        }
        if (options_.strict_mode
            && finding.severity == validation_severity::warning) {
            result.is_valid = false;
            break;
        }
    }

    return result;
}

bool parametric_map_iod_validator::quick_check(
    const dicom_dataset& dataset) const {

    // General Study Module
    if (!dataset.contains(tags::study_instance_uid)) return false;

    // General Series Module
    if (!dataset.contains(tags::modality)) return false;
    if (!dataset.contains(tags::series_instance_uid)) return false;

    // Check modality
    auto modality = dataset.get_string(tags::modality);
    if (modality != "RWV" && modality != "PMAP") return false;

    // Parametric Map Image Module
    if (!dataset.contains(pmap_iod_tags::content_label)) return false;
    if (!dataset.contains(pmap_iod_tags::real_world_value_mapping_sequence))
        return false;

    // Multi-frame
    if (!dataset.contains(pmap_iod_tags::number_of_frames)) return false;

    // Image Pixel Module
    if (!dataset.contains(tags::rows)) return false;
    if (!dataset.contains(tags::columns)) return false;

    // SOP Common Module
    if (!dataset.contains(tags::sop_class_uid)) return false;
    if (!dataset.contains(tags::sop_instance_uid)) return false;

    // Verify SOP Class is Parametric Map
    auto sop_class = dataset.get_string(tags::sop_class_uid);
    if (!sop_classes::is_parametric_map_storage_sop_class(sop_class))
        return false;

    return true;
}

const pmap_validation_options&
parametric_map_iod_validator::options() const noexcept {
    return options_;
}

void parametric_map_iod_validator::set_options(
    const pmap_validation_options& options) {
    options_ = options;
}

// =============================================================================
// Module Validation Methods
// =============================================================================

void parametric_map_iod_validator::validate_patient_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type2) {
        check_type2_attribute(dataset, tags::patient_name,
                             "PatientName", findings);
        check_type2_attribute(dataset, tags::patient_id,
                             "PatientID", findings);
        check_type2_attribute(dataset, tags::patient_birth_date,
                             "PatientBirthDate", findings);
        check_type2_attribute(dataset, tags::patient_sex,
                             "PatientSex", findings);
    }
}

void parametric_map_iod_validator::validate_general_study_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::study_instance_uid,
                             "StudyInstanceUID", findings);
    }

    if (options_.check_type2) {
        check_type2_attribute(dataset, tags::study_date,
                             "StudyDate", findings);
        check_type2_attribute(dataset, tags::study_time,
                             "StudyTime", findings);
        check_type2_attribute(dataset, tags::referring_physician_name,
                             "ReferringPhysicianName", findings);
        check_type2_attribute(dataset, tags::study_id,
                             "StudyID", findings);
        check_type2_attribute(dataset, tags::accession_number,
                             "AccessionNumber", findings);
    }
}

void parametric_map_iod_validator::validate_general_series_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::modality,
                             "Modality", findings);
        check_type1_attribute(dataset, tags::series_instance_uid,
                             "SeriesInstanceUID", findings);
        check_modality(dataset, findings);
    }

    if (options_.check_type2) {
        check_type2_attribute(dataset, tags::series_number,
                             "SeriesNumber", findings);
    }
}

void parametric_map_iod_validator::validate_general_equipment_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type2) {
        check_type2_attribute(dataset, pmap_iod_tags::manufacturer,
                             "Manufacturer", findings);
    }
}

void parametric_map_iod_validator::validate_enhanced_general_equipment_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        check_type1_attribute(dataset, pmap_iod_tags::manufacturer,
                             "Manufacturer", findings);
        check_type1_attribute(dataset, pmap_iod_tags::manufacturer_model_name,
                             "ManufacturerModelName", findings);
        check_type1_attribute(dataset, pmap_iod_tags::device_serial_number,
                             "DeviceSerialNumber", findings);
        check_type1_attribute(dataset, pmap_iod_tags::software_versions,
                             "SoftwareVersions", findings);
    }
}

void parametric_map_iod_validator::validate_image_pixel_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::samples_per_pixel,
                             "SamplesPerPixel", findings);
        check_type1_attribute(dataset, tags::photometric_interpretation,
                             "PhotometricInterpretation", findings);
        check_type1_attribute(dataset, tags::rows, "Rows", findings);
        check_type1_attribute(dataset, tags::columns, "Columns", findings);
        check_type1_attribute(dataset, tags::bits_allocated,
                             "BitsAllocated", findings);
        check_type1_attribute(dataset, tags::bits_stored,
                             "BitsStored", findings);
        check_type1_attribute(dataset, tags::high_bit,
                             "HighBit", findings);
        check_type1_attribute(dataset, tags::pixel_representation,
                             "PixelRepresentation", findings);
    }

    // Parametric Map-specific pixel data constraints
    check_pixel_data_consistency(dataset, findings);
}

void parametric_map_iod_validator::validate_parametric_map_image_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Content Label is Type 1
    if (options_.check_type1) {
        check_type1_attribute(dataset, pmap_iod_tags::content_label,
                             "ContentLabel", findings);
    }

    // Content Description is Type 2
    if (options_.check_type2) {
        check_type2_attribute(dataset, pmap_iod_tags::content_description,
                             "ContentDescription", findings);
        check_type2_attribute(dataset, pmap_iod_tags::content_creator_name,
                             "ContentCreatorName", findings);
    }

    // Real World Value Mapping Sequence is Type 1
    if (options_.validate_rwvm) {
        if (!dataset.contains(pmap_iod_tags::real_world_value_mapping_sequence)) {
            findings.push_back({
                validation_severity::error,
                pmap_iod_tags::real_world_value_mapping_sequence,
                "RealWorldValueMappingSequence (0040,9096) is required for "
                "Parametric Map objects",
                "PMAP-ERR-004"
            });
        }
    }
}

void parametric_map_iod_validator::validate_multiframe_functional_groups_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // NumberOfFrames is Type 1 (Parametric Maps are always multi-frame)
    if (!dataset.contains(pmap_iod_tags::number_of_frames)) {
        findings.push_back({
            validation_severity::error,
            pmap_iod_tags::number_of_frames,
            "NumberOfFrames (0028,0008) is required for Parametric Map objects "
            "(always multi-frame)",
            "PMAP-ERR-001"
        });
    }

    if (options_.check_type1) {
        if (!dataset.contains(
                pmap_iod_tags::shared_functional_groups_sequence)) {
            findings.push_back({
                validation_severity::warning,
                pmap_iod_tags::shared_functional_groups_sequence,
                "SharedFunctionalGroupsSequence (5200,9229) should be present "
                "for multi-frame Parametric Map objects",
                "PMAP-WARN-001"
            });
        }

        if (!dataset.contains(
                pmap_iod_tags::per_frame_functional_groups_sequence)) {
            findings.push_back({
                validation_severity::warning,
                pmap_iod_tags::per_frame_functional_groups_sequence,
                "PerFrameFunctionalGroupsSequence (5200,9230) should be present "
                "for multi-frame Parametric Map objects",
                "PMAP-WARN-004"
            });
        }
    }
}

void parametric_map_iod_validator::validate_multiframe_dimension_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        if (!dataset.contains(
                pmap_iod_tags::dimension_organization_sequence)) {
            findings.push_back({
                validation_severity::error,
                pmap_iod_tags::dimension_organization_sequence,
                "Type 1 attribute missing: DimensionOrganizationSequence "
                "(0020,9221)",
                "PMAP-TYPE1-MISSING"
            });
        }
        if (!dataset.contains(pmap_iod_tags::dimension_index_sequence)) {
            findings.push_back({
                validation_severity::error,
                pmap_iod_tags::dimension_index_sequence,
                "Type 1 attribute missing: DimensionIndexSequence "
                "(0020,9222)",
                "PMAP-TYPE1-MISSING"
            });
        }
    }
}

void parametric_map_iod_validator::validate_common_instance_reference_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_conditional) {
        if (!dataset.contains(pmap_iod_tags::referenced_series_sequence)) {
            findings.push_back({
                validation_severity::warning,
                pmap_iod_tags::referenced_series_sequence,
                "ReferencedSeriesSequence (0008,1115) should be present for "
                "source image references",
                "PMAP-WARN-003"
            });
        }
    }
}

void parametric_map_iod_validator::validate_sop_common_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::sop_class_uid,
                             "SOPClassUID", findings);
        check_type1_attribute(dataset, tags::sop_instance_uid,
                             "SOPInstanceUID", findings);
    }

    // Validate SOP Class UID is a Parametric Map storage class
    if (dataset.contains(tags::sop_class_uid)) {
        auto sop_class = dataset.get_string(tags::sop_class_uid);
        if (!sop_classes::is_parametric_map_storage_sop_class(sop_class)) {
            findings.push_back({
                validation_severity::error,
                tags::sop_class_uid,
                "SOPClassUID is not a recognized Parametric Map Storage SOP "
                "Class: " + sop_class,
                "PMAP-ERR-002"
            });
        }
    }
}

// =============================================================================
// Attribute Validation Helpers
// =============================================================================

void parametric_map_iod_validator::check_type1_attribute(
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
            "PMAP-TYPE1-MISSING"
        });
    } else {
        const auto* element = dataset.get(tag);
        if (element != nullptr) {
            if (element->is_sequence()) {
                if (element->sequence_items().empty()) {
                    findings.push_back({
                        validation_severity::error,
                        tag,
                        std::string("Type 1 sequence has no items: ") +
                        std::string(name) + " (" + tag.to_string() + ")",
                        "PMAP-TYPE1-EMPTY"
                    });
                }
            } else {
                auto value = dataset.get_string(tag);
                if (value.empty()) {
                    findings.push_back({
                        validation_severity::error,
                        tag,
                        std::string("Type 1 attribute has empty value: ") +
                        std::string(name) + " (" + tag.to_string() + ")",
                        "PMAP-TYPE1-EMPTY"
                    });
                }
            }
        }
    }
}

void parametric_map_iod_validator::check_type2_attribute(
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
            "PMAP-TYPE2-MISSING"
        });
    }
}

void parametric_map_iod_validator::check_modality(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(tags::modality)) {
        return;  // Already reported
    }

    auto modality = dataset.get_string(tags::modality);
    if (modality != "RWV" && modality != "PMAP") {
        findings.push_back({
            validation_severity::error,
            tags::modality,
            "Modality must be 'RWV' or 'PMAP' for Parametric Map objects, "
            "found: " + modality,
            "PMAP-ERR-003"
        });
    }
}

void parametric_map_iod_validator::check_pixel_data_consistency(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // SamplesPerPixel must be 1
    auto samples = dataset.get_numeric<uint16_t>(tags::samples_per_pixel);
    if (samples && *samples != 1) {
        findings.push_back({
            validation_severity::error,
            tags::samples_per_pixel,
            "SamplesPerPixel must be 1 for Parametric Map objects",
            "PMAP-ERR-007"
        });
    }

    // PhotometricInterpretation must be MONOCHROME2
    if (dataset.contains(tags::photometric_interpretation)) {
        auto photometric = dataset.get_string(
            tags::photometric_interpretation);
        if (!sop_classes::is_valid_parametric_map_photometric(photometric)) {
            findings.push_back({
                validation_severity::error,
                tags::photometric_interpretation,
                "PhotometricInterpretation must be MONOCHROME2 for Parametric "
                "Map objects, found: " + photometric,
                "PMAP-ERR-006"
            });
        }
    }

    // BitsAllocated must be 32 or 64 (float or double)
    auto bits_allocated = dataset.get_numeric<uint16_t>(tags::bits_allocated);
    if (bits_allocated
        && !sop_classes::is_valid_parametric_map_bits_allocated(
               *bits_allocated)) {
        findings.push_back({
            validation_severity::error,
            tags::bits_allocated,
            "BitsAllocated must be 32 or 64 for Parametric Map objects "
            "(float or double), found: " +
            std::to_string(*bits_allocated),
            "PMAP-ERR-008"
        });
    }

    // BitsStored must not exceed BitsAllocated
    auto bits_stored = dataset.get_numeric<uint16_t>(tags::bits_stored);
    if (bits_allocated && bits_stored && *bits_stored > *bits_allocated) {
        findings.push_back({
            validation_severity::error,
            tags::bits_stored,
            "BitsStored (" + std::to_string(*bits_stored) +
            ") exceeds BitsAllocated (" +
            std::to_string(*bits_allocated) + ")",
            "PMAP-ERR-005"
        });
    }

    // HighBit should be BitsStored - 1
    auto high_bit = dataset.get_numeric<uint16_t>(tags::high_bit);
    if (bits_stored && high_bit && *high_bit != (*bits_stored - 1)) {
        findings.push_back({
            validation_severity::warning,
            tags::high_bit,
            "HighBit (" + std::to_string(*high_bit) +
            ") does not match BitsStored-1 (" +
            std::to_string(*bits_stored - 1) + ")",
            "PMAP-WARN-002"
        });
    }
}

// =============================================================================
// Convenience Functions
// =============================================================================

validation_result validate_parametric_map_iod(const dicom_dataset& dataset) {
    parametric_map_iod_validator validator;
    return validator.validate(dataset);
}

bool is_valid_parametric_map_dataset(const dicom_dataset& dataset) {
    parametric_map_iod_validator validator;
    return validator.quick_check(dataset);
}

}  // namespace pacs::services::validation
