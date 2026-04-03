// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file heightmap_seg_iod_validator.cpp
 * @brief Implementation of Heightmap Segmentation IOD Validator
 */

#include "pacs/services/validation/heightmap_seg_iod_validator.hpp"
#include "pacs/core/dicom_tag_constants.hpp"

#include <sstream>

namespace kcenon::pacs::services::validation {

using namespace kcenon::pacs::core;

// Heightmap Segmentation Storage SOP Class UID (Supplement 240)
static constexpr std::string_view heightmap_segmentation_storage_uid =
    "1.2.840.10008.5.1.4.1.1.66.7";

// =============================================================================
// heightmap_seg_iod_validator Implementation
// =============================================================================

heightmap_seg_iod_validator::heightmap_seg_iod_validator(
    const heightmap_seg_validation_options& options)
    : options_(options) {}

validation_result heightmap_seg_iod_validator::validate(
    const dicom_dataset& dataset) const {
    validation_result result;
    result.is_valid = true;

    // Validate mandatory modules
    if (options_.check_type1 || options_.check_type2) {
        validate_patient_module(dataset, result.findings);
        validate_general_study_module(dataset, result.findings);
        validate_general_series_module(dataset, result.findings);
        validate_heightmap_segmentation_series_module(dataset, result.findings);
        validate_general_equipment_module(dataset, result.findings);
        validate_enhanced_general_equipment_module(dataset, result.findings);
        validate_general_image_module(dataset, result.findings);
        validate_sop_common_module(dataset, result.findings);
    }

    if (options_.validate_heightmap_instance) {
        validate_heightmap_segmentation_image_module(dataset, result.findings);
    }

    if (options_.validate_segment_sequence) {
        validate_segment_sequence(dataset, result.findings);
    }

    if (options_.validate_pixel_data) {
        validate_image_pixel_module(dataset, result.findings);
    }

    if (options_.validate_references) {
        validate_common_instance_reference_module(dataset, result.findings);
    }

    // Multi-frame validation
    validate_multiframe_functional_groups_module(dataset, result.findings);
    validate_multiframe_dimension_module(dataset, result.findings);

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

validation_result heightmap_seg_iod_validator::validate_segments(
    const dicom_dataset& dataset) const {
    validation_result result;
    result.is_valid = true;

    validate_segment_sequence(dataset, result.findings);

    for (const auto& finding : result.findings) {
        if (finding.severity == validation_severity::error) {
            result.is_valid = false;
            break;
        }
    }

    return result;
}

bool heightmap_seg_iod_validator::quick_check(
    const dicom_dataset& dataset) const {
    // Check essential Type 1 attributes

    // General Study Module
    if (!dataset.contains(tags::study_instance_uid)) return false;

    // General Series Module
    if (!dataset.contains(tags::modality)) return false;
    if (!dataset.contains(tags::series_instance_uid)) return false;

    // Check modality is SEG
    auto modality = dataset.get_string(tags::modality);
    if (modality != "SEG") return false;

    // Heightmap Segmentation Image Module
    if (!dataset.contains(heightmap_seg_tags::segmentation_type)) return false;
    auto seg_type = dataset.get_string(heightmap_seg_tags::segmentation_type);
    if (seg_type != "HEIGHTMAP") return false;

    if (!dataset.contains(heightmap_seg_tags::segment_sequence)) return false;

    // SOP Common Module
    if (!dataset.contains(tags::sop_class_uid)) return false;
    if (!dataset.contains(tags::sop_instance_uid)) return false;

    // Verify SOP Class is Heightmap Segmentation
    auto sop_class = dataset.get_string(tags::sop_class_uid);
    if (sop_class != heightmap_segmentation_storage_uid) return false;

    return true;
}

const heightmap_seg_validation_options&
heightmap_seg_iod_validator::options() const noexcept {
    return options_;
}

void heightmap_seg_iod_validator::set_options(
    const heightmap_seg_validation_options& options) {
    options_ = options;
}

// =============================================================================
// Module Validation Methods
// =============================================================================

void heightmap_seg_iod_validator::validate_patient_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type2) {
        check_type2_attribute(
            dataset, tags::patient_name, "PatientName", findings);
        check_type2_attribute(
            dataset, tags::patient_id, "PatientID", findings);
        check_type2_attribute(
            dataset, tags::patient_birth_date, "PatientBirthDate", findings);
        check_type2_attribute(
            dataset, tags::patient_sex, "PatientSex", findings);
    }
}

void heightmap_seg_iod_validator::validate_general_study_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        check_type1_attribute(
            dataset, tags::study_instance_uid, "StudyInstanceUID", findings);
    }

    if (options_.check_type2) {
        check_type2_attribute(
            dataset, tags::study_date, "StudyDate", findings);
        check_type2_attribute(
            dataset, tags::study_time, "StudyTime", findings);
        check_type2_attribute(
            dataset, tags::referring_physician_name,
            "ReferringPhysicianName", findings);
        check_type2_attribute(
            dataset, tags::study_id, "StudyID", findings);
        check_type2_attribute(
            dataset, tags::accession_number, "AccessionNumber", findings);
    }
}

void heightmap_seg_iod_validator::validate_general_series_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        check_type1_attribute(
            dataset, tags::modality, "Modality", findings);
        check_type1_attribute(
            dataset, tags::series_instance_uid, "SeriesInstanceUID", findings);
        check_modality(dataset, findings);

        // Frame of Reference Module - Type 1 for segmentation
        check_type1_attribute(
            dataset, tags::frame_of_reference_uid,
            "FrameOfReferenceUID", findings);
    }

    if (options_.check_type2) {
        check_type2_attribute(
            dataset, tags::series_number, "SeriesNumber", findings);
    }
}

void heightmap_seg_iod_validator::validate_heightmap_segmentation_series_module(
    [[maybe_unused]] const dicom_dataset& dataset,
    [[maybe_unused]] std::vector<validation_finding>& findings) const {

    // Modality must be "SEG" - already checked in general series validation
    // This module primarily constrains Modality to SEG for heightmap segmentation
}

void heightmap_seg_iod_validator::validate_general_equipment_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type2) {
        check_type2_attribute(
            dataset, heightmap_seg_tags::manufacturer, "Manufacturer", findings);
    }
}

void heightmap_seg_iod_validator::validate_enhanced_general_equipment_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        check_type1_attribute(
            dataset, heightmap_seg_tags::manufacturer,
            "Manufacturer", findings);
        check_type1_attribute(
            dataset, heightmap_seg_tags::manufacturer_model_name,
            "ManufacturerModelName", findings);
        check_type1_attribute(
            dataset, heightmap_seg_tags::device_serial_number,
            "DeviceSerialNumber", findings);
        check_type1_attribute(
            dataset, heightmap_seg_tags::software_versions,
            "SoftwareVersions", findings);
    }
}

void heightmap_seg_iod_validator::validate_general_image_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type2) {
        constexpr dicom_tag instance_number{0x0020, 0x0013};
        check_type2_attribute(
            dataset, instance_number, "InstanceNumber", findings);
    }
}

void heightmap_seg_iod_validator::validate_image_pixel_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        check_type1_attribute(
            dataset, tags::samples_per_pixel, "SamplesPerPixel", findings);
        check_type1_attribute(
            dataset, tags::photometric_interpretation,
            "PhotometricInterpretation", findings);
        check_type1_attribute(
            dataset, tags::rows, "Rows", findings);
        check_type1_attribute(
            dataset, tags::columns, "Columns", findings);
        check_type1_attribute(
            dataset, tags::bits_allocated, "BitsAllocated", findings);
        check_type1_attribute(
            dataset, tags::bits_stored, "BitsStored", findings);
        check_type1_attribute(
            dataset, tags::high_bit, "HighBit", findings);
        check_type1_attribute(
            dataset, tags::pixel_representation,
            "PixelRepresentation", findings);
    }

    // Heightmap-specific pixel data constraints
    check_pixel_data_consistency(dataset, findings);
}

void heightmap_seg_iod_validator::validate_heightmap_segmentation_image_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        check_type1_attribute(
            dataset, heightmap_seg_tags::segmentation_type,
            "SegmentationType", findings);
        check_type1_attribute(
            dataset, heightmap_seg_tags::segment_sequence,
            "SegmentSequence", findings);
    }

    // Validate segmentation type is HEIGHTMAP
    check_segmentation_type(dataset, findings);
}

void heightmap_seg_iod_validator::validate_segment_sequence(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(heightmap_seg_tags::segment_sequence)) {
        // Already reported as Type 1 missing
        return;
    }

    const auto* element = dataset.get(heightmap_seg_tags::segment_sequence);
    if (!element || !element->is_sequence()
        || element->sequence_items().empty()) {
        findings.push_back({
            validation_severity::error,
            heightmap_seg_tags::segment_sequence,
            "SegmentSequence must contain at least one segment",
            "HMSEG-ERR-004"
        });
        return;
    }

    const auto& sequence = element->sequence_items();
    for (size_t i = 0; i < sequence.size(); ++i) {
        const auto& segment_item = sequence[i];
        validate_single_segment(segment_item, i, findings);
    }
}

void heightmap_seg_iod_validator::validate_single_segment(
    const dicom_dataset& segment_item,
    size_t segment_index,
    std::vector<validation_finding>& findings) const {

    std::string prefix = "Segment[" + std::to_string(segment_index) + "]: ";

    // Type 1 attributes within segment
    if (!segment_item.contains(heightmap_seg_tags::segment_number)) {
        findings.push_back({
            validation_severity::error,
            heightmap_seg_tags::segment_number,
            prefix + "SegmentNumber (0062,0004) is required",
            "HMSEG-SEQ-ERR-001"
        });
    }

    if (!segment_item.contains(heightmap_seg_tags::segment_label)) {
        findings.push_back({
            validation_severity::error,
            heightmap_seg_tags::segment_label,
            prefix + "SegmentLabel (0062,0005) is required",
            "HMSEG-SEQ-ERR-002"
        });
    }

    if (!segment_item.contains(heightmap_seg_tags::segment_algorithm_type)) {
        findings.push_back({
            validation_severity::error,
            heightmap_seg_tags::segment_algorithm_type,
            prefix + "SegmentAlgorithmType (0062,0008) is required",
            "HMSEG-SEQ-ERR-003"
        });
    } else if (options_.validate_algorithm_info) {
        auto algo_type =
            segment_item.get_string(heightmap_seg_tags::segment_algorithm_type);
        if (algo_type != "AUTOMATIC" && algo_type != "SEMIAUTOMATIC"
            && algo_type != "MANUAL") {
            findings.push_back({
                validation_severity::warning,
                heightmap_seg_tags::segment_algorithm_type,
                prefix + "Invalid SegmentAlgorithmType value: " + algo_type,
                "HMSEG-SEQ-WARN-001"
            });
        }
    }

    // Segmented Property Category Code Sequence is Type 1
    if (!segment_item.contains(
            heightmap_seg_tags::segmented_property_category_code_sequence)) {
        findings.push_back({
            validation_severity::error,
            heightmap_seg_tags::segmented_property_category_code_sequence,
            prefix + "SegmentedPropertyCategoryCodeSequence (0062,0003) "
                     "is required",
            "HMSEG-SEQ-ERR-004"
        });
    }

    // Segmented Property Type Code Sequence is Type 1
    if (!segment_item.contains(
            heightmap_seg_tags::segmented_property_type_code_sequence)) {
        findings.push_back({
            validation_severity::error,
            heightmap_seg_tags::segmented_property_type_code_sequence,
            prefix + "SegmentedPropertyTypeCodeSequence (0062,000F) "
                     "is required",
            "HMSEG-SEQ-ERR-005"
        });
    }

    // Validate segment label is not empty
    if (segment_item.contains(heightmap_seg_tags::segment_label)) {
        auto label =
            segment_item.get_string(heightmap_seg_tags::segment_label);
        if (label.empty()) {
            findings.push_back({
                validation_severity::warning,
                heightmap_seg_tags::segment_label,
                prefix + "SegmentLabel should not be empty",
                "HMSEG-SEQ-WARN-002"
            });
        }
    }
}

void heightmap_seg_iod_validator::validate_multiframe_functional_groups_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        check_type1_attribute(
            dataset, heightmap_seg_tags::number_of_frames,
            "NumberOfFrames", findings);
        check_type1_attribute(
            dataset, heightmap_seg_tags::shared_functional_groups_sequence,
            "SharedFunctionalGroupsSequence", findings);
        check_type1_attribute(
            dataset, heightmap_seg_tags::per_frame_functional_groups_sequence,
            "PerFrameFunctionalGroupsSequence", findings);
    }
}

void heightmap_seg_iod_validator::validate_multiframe_dimension_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        check_type1_attribute(
            dataset, heightmap_seg_tags::dimension_organization_sequence,
            "DimensionOrganizationSequence", findings);
        check_type1_attribute(
            dataset, heightmap_seg_tags::dimension_index_sequence,
            "DimensionIndexSequence", findings);
    }
}

void heightmap_seg_iod_validator::validate_common_instance_reference_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_conditional) {
        if (!dataset.contains(heightmap_seg_tags::referenced_series_sequence)) {
            findings.push_back({
                validation_severity::warning,
                heightmap_seg_tags::referenced_series_sequence,
                "ReferencedSeriesSequence (0008,1115) should be present "
                "for source image references",
                "HMSEG-REF-WARN-001"
            });
        }
    }
}

void heightmap_seg_iod_validator::validate_sop_common_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        check_type1_attribute(
            dataset, tags::sop_class_uid, "SOPClassUID", findings);
        check_type1_attribute(
            dataset, tags::sop_instance_uid, "SOPInstanceUID", findings);
    }

    // Validate SOP Class UID is Heightmap Segmentation
    if (dataset.contains(tags::sop_class_uid)) {
        auto sop_class = dataset.get_string(tags::sop_class_uid);
        if (sop_class != heightmap_segmentation_storage_uid) {
            findings.push_back({
                validation_severity::error,
                tags::sop_class_uid,
                "SOPClassUID is not Heightmap Segmentation Storage: "
                    + sop_class,
                "HMSEG-ERR-005"
            });
        }
    }
}

// =============================================================================
// Attribute Validation Helpers
// =============================================================================

void heightmap_seg_iod_validator::check_type1_attribute(
    const dicom_dataset& dataset,
    dicom_tag tag,
    std::string_view name,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(tag)) {
        findings.push_back({
            validation_severity::error,
            tag,
            std::string("Type 1 attribute missing: ") + std::string(name)
                + " (" + tag.to_string() + ")",
            "HMSEG-TYPE1-MISSING"
        });
    } else {
        const auto* element = dataset.get(tag);
        if (element != nullptr) {
            if (element->is_sequence()) {
                if (element->sequence_items().empty()) {
                    findings.push_back({
                        validation_severity::error,
                        tag,
                        std::string("Type 1 sequence has no items: ")
                            + std::string(name) + " (" + tag.to_string() + ")",
                        "HMSEG-TYPE1-EMPTY"
                    });
                }
            } else {
                auto value = dataset.get_string(tag);
                if (value.empty()) {
                    findings.push_back({
                        validation_severity::error,
                        tag,
                        std::string("Type 1 attribute has empty value: ")
                            + std::string(name) + " (" + tag.to_string() + ")",
                        "HMSEG-TYPE1-EMPTY"
                    });
                }
            }
        }
    }
}

void heightmap_seg_iod_validator::check_type2_attribute(
    const dicom_dataset& dataset,
    dicom_tag tag,
    std::string_view name,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(tag)) {
        findings.push_back({
            validation_severity::warning,
            tag,
            std::string("Type 2 attribute missing: ") + std::string(name)
                + " (" + tag.to_string() + ")",
            "HMSEG-TYPE2-MISSING"
        });
    }
}

void heightmap_seg_iod_validator::check_modality(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(tags::modality)) {
        return;  // Already reported
    }

    auto modality = dataset.get_string(tags::modality);
    if (modality != "SEG") {
        findings.push_back({
            validation_severity::error,
            tags::modality,
            "Modality must be 'SEG' for Heightmap Segmentation objects, "
            "found: " + modality,
            "HMSEG-ERR-001"
        });
    }
}

void heightmap_seg_iod_validator::check_segmentation_type(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(heightmap_seg_tags::segmentation_type)) {
        return;  // Already reported
    }

    auto seg_type =
        dataset.get_string(heightmap_seg_tags::segmentation_type);
    if (seg_type != "HEIGHTMAP") {
        findings.push_back({
            validation_severity::error,
            heightmap_seg_tags::segmentation_type,
            "SegmentationType must be 'HEIGHTMAP' for Heightmap Segmentation "
            "objects, found: " + seg_type,
            "HMSEG-ERR-006"
        });
    }
}

void heightmap_seg_iod_validator::check_pixel_data_consistency(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // For Heightmap SEG, SamplesPerPixel must be 1
    auto samples = dataset.get_numeric<uint16_t>(tags::samples_per_pixel);
    if (samples && *samples != 1) {
        findings.push_back({
            validation_severity::error,
            tags::samples_per_pixel,
            "SamplesPerPixel must be 1 for Heightmap Segmentation objects",
            "HMSEG-PXL-ERR-001"
        });
    }

    // PhotometricInterpretation must be MONOCHROME2
    if (dataset.contains(tags::photometric_interpretation)) {
        auto photometric =
            dataset.get_string(tags::photometric_interpretation);
        if (photometric != "MONOCHROME2") {
            findings.push_back({
                validation_severity::error,
                tags::photometric_interpretation,
                "PhotometricInterpretation must be MONOCHROME2 for "
                "Heightmap Segmentation",
                "HMSEG-PXL-ERR-002"
            });
        }
    }

    // Heightmap segmentation uses 32-bit float or 16-bit unsigned
    // for height values (sub-pixel precision)
    auto bits_allocated = dataset.get_numeric<uint16_t>(tags::bits_allocated);
    if (bits_allocated) {
        if (*bits_allocated != 16 && *bits_allocated != 32) {
            findings.push_back({
                validation_severity::warning,
                tags::bits_allocated,
                "BitsAllocated is typically 16 or 32 for Heightmap "
                "Segmentation, found: "
                    + std::to_string(*bits_allocated),
                "HMSEG-PXL-WARN-001"
            });
        }
    }
}

// =============================================================================
// Convenience Functions
// =============================================================================

validation_result validate_heightmap_seg_iod(const dicom_dataset& dataset) {
    heightmap_seg_iod_validator validator;
    return validator.validate(dataset);
}

bool is_valid_heightmap_seg_dataset(const dicom_dataset& dataset) {
    heightmap_seg_iod_validator validator;
    return validator.quick_check(dataset);
}

bool is_heightmap_segmentation(const dicom_dataset& dataset) {
    constexpr dicom_tag segmentation_type{0x0062, 0x0001};
    if (!dataset.contains(segmentation_type)) {
        return false;
    }
    return dataset.get_string(segmentation_type) == "HEIGHTMAP";
}

}  // namespace kcenon::pacs::services::validation
