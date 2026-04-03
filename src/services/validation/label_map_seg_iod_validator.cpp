// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file label_map_seg_iod_validator.cpp
 * @brief Implementation of Label Map Segmentation IOD Validator
 */

#include "kcenon/pacs/services/validation/label_map_seg_iod_validator.h"
#include "kcenon/pacs/core/dicom_tag_constants.h"

#include <sstream>

namespace kcenon::pacs::services::validation {

using namespace kcenon::pacs::core;

// Label Map Segmentation Storage SOP Class UID (Supplement 243)
static constexpr std::string_view label_map_segmentation_storage_uid =
    "1.2.840.10008.5.1.4.1.1.66.8";

// =============================================================================
// label_map_seg_iod_validator Implementation
// =============================================================================

label_map_seg_iod_validator::label_map_seg_iod_validator(
    const label_map_seg_validation_options& options)
    : options_(options) {}

validation_result label_map_seg_iod_validator::validate(
    const dicom_dataset& dataset) const {
    validation_result result;
    result.is_valid = true;

    // Validate mandatory modules
    if (options_.check_type1 || options_.check_type2) {
        validate_patient_module(dataset, result.findings);
        validate_general_study_module(dataset, result.findings);
        validate_general_series_module(dataset, result.findings);
        validate_label_map_segmentation_series_module(
            dataset, result.findings);
        validate_general_equipment_module(dataset, result.findings);
        validate_enhanced_general_equipment_module(dataset, result.findings);
        validate_general_image_module(dataset, result.findings);
        validate_sop_common_module(dataset, result.findings);
    }

    if (options_.validate_label_map_instance) {
        validate_label_map_segmentation_image_module(
            dataset, result.findings);
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

validation_result label_map_seg_iod_validator::validate_segments(
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

bool label_map_seg_iod_validator::quick_check(
    const dicom_dataset& dataset) const {
    // General Study Module
    if (!dataset.contains(tags::study_instance_uid)) return false;

    // General Series Module
    if (!dataset.contains(tags::modality)) return false;
    if (!dataset.contains(tags::series_instance_uid)) return false;

    // Check modality is SEG
    auto modality = dataset.get_string(tags::modality);
    if (modality != "SEG") return false;

    // Label Map Segmentation Image Module
    if (!dataset.contains(label_map_seg_tags::segmentation_type)) return false;
    auto seg_type =
        dataset.get_string(label_map_seg_tags::segmentation_type);
    if (seg_type != "LABELMAP") return false;

    if (!dataset.contains(label_map_seg_tags::segment_sequence)) return false;

    // SOP Common Module
    if (!dataset.contains(tags::sop_class_uid)) return false;
    if (!dataset.contains(tags::sop_instance_uid)) return false;

    // Verify SOP Class is Label Map Segmentation
    auto sop_class = dataset.get_string(tags::sop_class_uid);
    if (sop_class != label_map_segmentation_storage_uid) return false;

    return true;
}

const label_map_seg_validation_options&
label_map_seg_iod_validator::options() const noexcept {
    return options_;
}

void label_map_seg_iod_validator::set_options(
    const label_map_seg_validation_options& options) {
    options_ = options;
}

// =============================================================================
// Module Validation Methods
// =============================================================================

void label_map_seg_iod_validator::validate_patient_module(
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

void label_map_seg_iod_validator::validate_general_study_module(
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

void label_map_seg_iod_validator::validate_general_series_module(
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

void label_map_seg_iod_validator::
    validate_label_map_segmentation_series_module(
        [[maybe_unused]] const dicom_dataset& dataset,
        [[maybe_unused]] std::vector<validation_finding>& findings) const {

    // Modality must be "SEG" - already checked in general series validation
}

void label_map_seg_iod_validator::validate_general_equipment_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type2) {
        check_type2_attribute(
            dataset, label_map_seg_tags::manufacturer,
            "Manufacturer", findings);
    }
}

void label_map_seg_iod_validator::validate_enhanced_general_equipment_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        check_type1_attribute(
            dataset, label_map_seg_tags::manufacturer,
            "Manufacturer", findings);
        check_type1_attribute(
            dataset, label_map_seg_tags::manufacturer_model_name,
            "ManufacturerModelName", findings);
        check_type1_attribute(
            dataset, label_map_seg_tags::device_serial_number,
            "DeviceSerialNumber", findings);
        check_type1_attribute(
            dataset, label_map_seg_tags::software_versions,
            "SoftwareVersions", findings);
    }
}

void label_map_seg_iod_validator::validate_general_image_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type2) {
        constexpr dicom_tag instance_number{0x0020, 0x0013};
        check_type2_attribute(
            dataset, instance_number, "InstanceNumber", findings);
    }
}

void label_map_seg_iod_validator::validate_image_pixel_module(
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

    // Label map-specific pixel data constraints
    check_pixel_data_consistency(dataset, findings);
}

void label_map_seg_iod_validator::
    validate_label_map_segmentation_image_module(
        const dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        check_type1_attribute(
            dataset, label_map_seg_tags::segmentation_type,
            "SegmentationType", findings);
        check_type1_attribute(
            dataset, label_map_seg_tags::segment_sequence,
            "SegmentSequence", findings);
    }

    // Validate segmentation type is LABELMAP
    check_segmentation_type(dataset, findings);
}

void label_map_seg_iod_validator::validate_segment_sequence(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(label_map_seg_tags::segment_sequence)) {
        return;  // Already reported as Type 1 missing
    }

    const auto* element = dataset.get(label_map_seg_tags::segment_sequence);
    if (!element || !element->is_sequence()
        || element->sequence_items().empty()) {
        findings.push_back({
            validation_severity::error,
            label_map_seg_tags::segment_sequence,
            "SegmentSequence must contain at least one segment",
            "LMSEG-ERR-004"
        });
        return;
    }

    const auto& sequence = element->sequence_items();
    for (size_t i = 0; i < sequence.size(); ++i) {
        const auto& segment_item = sequence[i];
        validate_single_segment(segment_item, i, findings);
    }

    // Validate segment numbers are contiguous starting from 1
    // (Label Map constraint: label values map directly to segment numbers)
    for (size_t i = 0; i < sequence.size(); ++i) {
        if (sequence[i].contains(label_map_seg_tags::segment_number)) {
            auto seg_num = sequence[i].get_numeric<uint16_t>(
                label_map_seg_tags::segment_number);
            if (seg_num && *seg_num != static_cast<uint16_t>(i + 1)) {
                findings.push_back({
                    validation_severity::warning,
                    label_map_seg_tags::segment_number,
                    "Segment[" + std::to_string(i) + "]: "
                    "SegmentNumber should be " + std::to_string(i + 1)
                    + " for Label Map (contiguous from 1), found: "
                    + std::to_string(*seg_num),
                    "LMSEG-SEQ-WARN-003"
                });
            }
        }
    }
}

void label_map_seg_iod_validator::validate_single_segment(
    const dicom_dataset& segment_item,
    size_t segment_index,
    std::vector<validation_finding>& findings) const {

    std::string prefix = "Segment[" + std::to_string(segment_index) + "]: ";

    if (!segment_item.contains(label_map_seg_tags::segment_number)) {
        findings.push_back({
            validation_severity::error,
            label_map_seg_tags::segment_number,
            prefix + "SegmentNumber (0062,0004) is required",
            "LMSEG-SEQ-ERR-001"
        });
    }

    if (!segment_item.contains(label_map_seg_tags::segment_label)) {
        findings.push_back({
            validation_severity::error,
            label_map_seg_tags::segment_label,
            prefix + "SegmentLabel (0062,0005) is required",
            "LMSEG-SEQ-ERR-002"
        });
    }

    if (!segment_item.contains(label_map_seg_tags::segment_algorithm_type)) {
        findings.push_back({
            validation_severity::error,
            label_map_seg_tags::segment_algorithm_type,
            prefix + "SegmentAlgorithmType (0062,0008) is required",
            "LMSEG-SEQ-ERR-003"
        });
    } else if (options_.validate_algorithm_info) {
        auto algo_type =
            segment_item.get_string(
                label_map_seg_tags::segment_algorithm_type);
        if (algo_type != "AUTOMATIC" && algo_type != "SEMIAUTOMATIC"
            && algo_type != "MANUAL") {
            findings.push_back({
                validation_severity::warning,
                label_map_seg_tags::segment_algorithm_type,
                prefix + "Invalid SegmentAlgorithmType value: " + algo_type,
                "LMSEG-SEQ-WARN-001"
            });
        }
    }

    if (!segment_item.contains(
            label_map_seg_tags::segmented_property_category_code_sequence)) {
        findings.push_back({
            validation_severity::error,
            label_map_seg_tags::segmented_property_category_code_sequence,
            prefix + "SegmentedPropertyCategoryCodeSequence (0062,0003) "
                     "is required",
            "LMSEG-SEQ-ERR-004"
        });
    }

    if (!segment_item.contains(
            label_map_seg_tags::segmented_property_type_code_sequence)) {
        findings.push_back({
            validation_severity::error,
            label_map_seg_tags::segmented_property_type_code_sequence,
            prefix + "SegmentedPropertyTypeCodeSequence (0062,000F) "
                     "is required",
            "LMSEG-SEQ-ERR-005"
        });
    }

    if (segment_item.contains(label_map_seg_tags::segment_label)) {
        auto label =
            segment_item.get_string(label_map_seg_tags::segment_label);
        if (label.empty()) {
            findings.push_back({
                validation_severity::warning,
                label_map_seg_tags::segment_label,
                prefix + "SegmentLabel should not be empty",
                "LMSEG-SEQ-WARN-002"
            });
        }
    }
}

void label_map_seg_iod_validator::
    validate_multiframe_functional_groups_module(
        const dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        check_type1_attribute(
            dataset, label_map_seg_tags::number_of_frames,
            "NumberOfFrames", findings);
        check_type1_attribute(
            dataset, label_map_seg_tags::shared_functional_groups_sequence,
            "SharedFunctionalGroupsSequence", findings);
        check_type1_attribute(
            dataset,
            label_map_seg_tags::per_frame_functional_groups_sequence,
            "PerFrameFunctionalGroupsSequence", findings);
    }
}

void label_map_seg_iod_validator::validate_multiframe_dimension_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        check_type1_attribute(
            dataset, label_map_seg_tags::dimension_organization_sequence,
            "DimensionOrganizationSequence", findings);
        check_type1_attribute(
            dataset, label_map_seg_tags::dimension_index_sequence,
            "DimensionIndexSequence", findings);
    }
}

void label_map_seg_iod_validator::validate_common_instance_reference_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_conditional) {
        if (!dataset.contains(
                label_map_seg_tags::referenced_series_sequence)) {
            findings.push_back({
                validation_severity::warning,
                label_map_seg_tags::referenced_series_sequence,
                "ReferencedSeriesSequence (0008,1115) should be present "
                "for source image references",
                "LMSEG-REF-WARN-001"
            });
        }
    }
}

void label_map_seg_iod_validator::validate_sop_common_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        check_type1_attribute(
            dataset, tags::sop_class_uid, "SOPClassUID", findings);
        check_type1_attribute(
            dataset, tags::sop_instance_uid, "SOPInstanceUID", findings);
    }

    if (dataset.contains(tags::sop_class_uid)) {
        auto sop_class = dataset.get_string(tags::sop_class_uid);
        if (sop_class != label_map_segmentation_storage_uid) {
            findings.push_back({
                validation_severity::error,
                tags::sop_class_uid,
                "SOPClassUID is not Label Map Segmentation Storage: "
                    + sop_class,
                "LMSEG-ERR-005"
            });
        }
    }
}

// =============================================================================
// Attribute Validation Helpers
// =============================================================================

void label_map_seg_iod_validator::check_type1_attribute(
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
            "LMSEG-TYPE1-MISSING"
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
                            + std::string(name) + " (" + tag.to_string()
                            + ")",
                        "LMSEG-TYPE1-EMPTY"
                    });
                }
            } else {
                auto value = dataset.get_string(tag);
                if (value.empty()) {
                    findings.push_back({
                        validation_severity::error,
                        tag,
                        std::string("Type 1 attribute has empty value: ")
                            + std::string(name) + " (" + tag.to_string()
                            + ")",
                        "LMSEG-TYPE1-EMPTY"
                    });
                }
            }
        }
    }
}

void label_map_seg_iod_validator::check_type2_attribute(
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
            "LMSEG-TYPE2-MISSING"
        });
    }
}

void label_map_seg_iod_validator::check_modality(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(tags::modality)) {
        return;
    }

    auto modality = dataset.get_string(tags::modality);
    if (modality != "SEG") {
        findings.push_back({
            validation_severity::error,
            tags::modality,
            "Modality must be 'SEG' for Label Map Segmentation objects, "
            "found: " + modality,
            "LMSEG-ERR-001"
        });
    }
}

void label_map_seg_iod_validator::check_segmentation_type(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(label_map_seg_tags::segmentation_type)) {
        return;
    }

    auto seg_type =
        dataset.get_string(label_map_seg_tags::segmentation_type);
    if (seg_type != "LABELMAP") {
        findings.push_back({
            validation_severity::error,
            label_map_seg_tags::segmentation_type,
            "SegmentationType must be 'LABELMAP' for Label Map Segmentation "
            "objects, found: " + seg_type,
            "LMSEG-ERR-006"
        });
    }
}

void label_map_seg_iod_validator::check_pixel_data_consistency(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // SamplesPerPixel must be 1
    auto samples = dataset.get_numeric<uint16_t>(tags::samples_per_pixel);
    if (samples && *samples != 1) {
        findings.push_back({
            validation_severity::error,
            tags::samples_per_pixel,
            "SamplesPerPixel must be 1 for Label Map Segmentation objects",
            "LMSEG-PXL-ERR-001"
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
                "Label Map Segmentation",
                "LMSEG-PXL-ERR-002"
            });
        }
    }

    // Label Map uses unsigned 8-bit or 16-bit integer labels
    auto bits_allocated = dataset.get_numeric<uint16_t>(tags::bits_allocated);
    if (bits_allocated) {
        if (*bits_allocated != 8 && *bits_allocated != 16) {
            findings.push_back({
                validation_severity::warning,
                tags::bits_allocated,
                "BitsAllocated should be 8 or 16 for Label Map "
                "Segmentation (unsigned integer labels), found: "
                    + std::to_string(*bits_allocated),
                "LMSEG-PXL-WARN-001"
            });
        }
    }

    // PixelRepresentation must be 0 (unsigned) for label values
    auto pixel_rep =
        dataset.get_numeric<uint16_t>(tags::pixel_representation);
    if (pixel_rep && *pixel_rep != 0) {
        findings.push_back({
            validation_severity::error,
            tags::pixel_representation,
            "PixelRepresentation must be 0 (unsigned) for Label Map "
            "Segmentation -- label values must be unsigned integers",
            "LMSEG-PXL-ERR-003"
        });
    }
}

// =============================================================================
// Convenience Functions
// =============================================================================

validation_result validate_label_map_seg_iod(const dicom_dataset& dataset) {
    label_map_seg_iod_validator validator;
    return validator.validate(dataset);
}

bool is_valid_label_map_seg_dataset(const dicom_dataset& dataset) {
    label_map_seg_iod_validator validator;
    return validator.quick_check(dataset);
}

bool is_label_map_segmentation(const dicom_dataset& dataset) {
    constexpr dicom_tag segmentation_type{0x0062, 0x0001};
    if (!dataset.contains(segmentation_type)) {
        return false;
    }
    return dataset.get_string(segmentation_type) == "LABELMAP";
}

}  // namespace kcenon::pacs::services::validation
