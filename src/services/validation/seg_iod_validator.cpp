/**
 * @file seg_iod_validator.cpp
 * @brief Implementation of Segmentation IOD Validator
 */

#include "pacs/services/validation/seg_iod_validator.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/services/sop_classes/seg_storage.hpp"

#include <sstream>

namespace pacs::services::validation {

using namespace pacs::core;

// =============================================================================
// SEG-specific DICOM Tags
// =============================================================================

namespace seg_tags {

// Segmentation Series Module
constexpr dicom_tag modality{0x0008, 0x0060};

// Segmentation Image Module
constexpr dicom_tag segmentation_type{0x0062, 0x0001};
constexpr dicom_tag segmentation_fractional_type{0x0062, 0x0010};
constexpr dicom_tag max_fractional_value{0x0062, 0x000E};
constexpr dicom_tag segment_sequence{0x0062, 0x0002};
constexpr dicom_tag segments_overlap{0x0062, 0x0013};

// Segment Sequence Item attributes
constexpr dicom_tag segment_number{0x0062, 0x0004};
constexpr dicom_tag segment_label{0x0062, 0x0005};
constexpr dicom_tag segment_description{0x0062, 0x0006};
constexpr dicom_tag segment_algorithm_type{0x0062, 0x0008};
constexpr dicom_tag segment_algorithm_name{0x0062, 0x0009};
constexpr dicom_tag segmented_property_category_code_sequence{0x0062, 0x0003};
constexpr dicom_tag segmented_property_type_code_sequence{0x0062, 0x000F};
constexpr dicom_tag anatomic_region_sequence{0x0008, 0x2218};
constexpr dicom_tag recommended_display_cieLab_value{0x0062, 0x000D};
constexpr dicom_tag tracking_id{0x0062, 0x0020};
constexpr dicom_tag tracking_uid{0x0062, 0x0021};

// Multi-frame related
constexpr dicom_tag number_of_frames{0x0028, 0x0008};
constexpr dicom_tag shared_functional_groups_sequence{0x5200, 0x9229};
constexpr dicom_tag per_frame_functional_groups_sequence{0x5200, 0x9230};
constexpr dicom_tag dimension_organization_sequence{0x0020, 0x9221};
constexpr dicom_tag dimension_index_sequence{0x0020, 0x9222};

// Referenced Series Sequence
constexpr dicom_tag referenced_series_sequence{0x0008, 0x1115};
constexpr dicom_tag referenced_instance_sequence{0x0008, 0x114A};

// Common Instance Reference Module
constexpr dicom_tag referenced_series_sequence_cir{0x0008, 0x1115};
constexpr dicom_tag studies_containing_other_referenced_instances_sequence{0x0008, 0x1200};

// Enhanced General Equipment Module
constexpr dicom_tag manufacturer{0x0008, 0x0070};
constexpr dicom_tag manufacturer_model_name{0x0008, 0x1090};
constexpr dicom_tag device_serial_number{0x0018, 0x1000};
constexpr dicom_tag software_versions{0x0018, 0x1020};

// Code Sequence attributes
constexpr dicom_tag code_value{0x0008, 0x0100};
constexpr dicom_tag coding_scheme_designator{0x0008, 0x0102};
constexpr dicom_tag code_meaning{0x0008, 0x0104};

}  // namespace seg_tags

// =============================================================================
// seg_iod_validator Implementation
// =============================================================================

seg_iod_validator::seg_iod_validator(const seg_validation_options& options)
    : options_(options) {}

validation_result seg_iod_validator::validate(const dicom_dataset& dataset) const {
    validation_result result;
    result.is_valid = true;

    // Validate mandatory modules
    if (options_.check_type1 || options_.check_type2) {
        validate_patient_module(dataset, result.findings);
        validate_general_study_module(dataset, result.findings);
        validate_general_series_module(dataset, result.findings);
        validate_segmentation_series_module(dataset, result.findings);
        validate_general_equipment_module(dataset, result.findings);
        validate_enhanced_general_equipment_module(dataset, result.findings);
        validate_general_image_module(dataset, result.findings);
        validate_segmentation_image_module(dataset, result.findings);
        validate_sop_common_module(dataset, result.findings);
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
        if (options_.strict_mode && finding.severity == validation_severity::warning) {
            result.is_valid = false;
            break;
        }
    }

    return result;
}

validation_result
seg_iod_validator::validate_segments(const dicom_dataset& dataset) const {
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

validation_result
seg_iod_validator::validate_references(const dicom_dataset& dataset) const {
    validation_result result;
    result.is_valid = true;

    validate_common_instance_reference_module(dataset, result.findings);

    for (const auto& finding : result.findings) {
        if (finding.severity == validation_severity::error) {
            result.is_valid = false;
            break;
        }
    }

    return result;
}

bool seg_iod_validator::quick_check(const dicom_dataset& dataset) const {
    // Check essential Type 1 attributes for SEG

    // General Study Module
    if (!dataset.contains(tags::study_instance_uid)) return false;

    // General Series Module
    if (!dataset.contains(tags::modality)) return false;
    if (!dataset.contains(tags::series_instance_uid)) return false;

    // Check modality is SEG
    auto modality = dataset.get_string(tags::modality);
    if (modality != "SEG") return false;

    // Segmentation Image Module
    if (!dataset.contains(seg_tags::segmentation_type)) return false;
    if (!dataset.contains(seg_tags::segment_sequence)) return false;

    // SOP Common Module
    if (!dataset.contains(tags::sop_class_uid)) return false;
    if (!dataset.contains(tags::sop_instance_uid)) return false;

    // Verify SOP Class is SEG
    auto sop_class = dataset.get_string(tags::sop_class_uid);
    if (!sop_classes::is_seg_storage_sop_class(sop_class)) return false;

    return true;
}

const seg_validation_options& seg_iod_validator::options() const noexcept {
    return options_;
}

void seg_iod_validator::set_options(const seg_validation_options& options) {
    options_ = options;
}

// =============================================================================
// Module Validation Methods
// =============================================================================

void seg_iod_validator::validate_patient_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type2) {
        check_type2_attribute(dataset, tags::patient_name, "PatientName", findings);
        check_type2_attribute(dataset, tags::patient_id, "PatientID", findings);
        check_type2_attribute(dataset, tags::patient_birth_date, "PatientBirthDate", findings);
        check_type2_attribute(dataset, tags::patient_sex, "PatientSex", findings);
    }
}

void seg_iod_validator::validate_general_study_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::study_instance_uid, "StudyInstanceUID", findings);
    }

    if (options_.check_type2) {
        check_type2_attribute(dataset, tags::study_date, "StudyDate", findings);
        check_type2_attribute(dataset, tags::study_time, "StudyTime", findings);
        check_type2_attribute(dataset, tags::referring_physician_name, "ReferringPhysicianName", findings);
        check_type2_attribute(dataset, tags::study_id, "StudyID", findings);
        check_type2_attribute(dataset, tags::accession_number, "AccessionNumber", findings);
    }
}

void seg_iod_validator::validate_general_series_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::modality, "Modality", findings);
        check_type1_attribute(dataset, tags::series_instance_uid, "SeriesInstanceUID", findings);
        check_modality(dataset, findings);
    }

    if (options_.check_type2) {
        check_type2_attribute(dataset, tags::series_number, "SeriesNumber", findings);
    }
}

void seg_iod_validator::validate_segmentation_series_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Modality must be "SEG" - already checked in general series validation
    // This module primarily constrains Modality to SEG
}

void seg_iod_validator::validate_general_equipment_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type2) {
        check_type2_attribute(dataset, seg_tags::manufacturer, "Manufacturer", findings);
    }
}

void seg_iod_validator::validate_enhanced_general_equipment_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        check_type1_attribute(dataset, seg_tags::manufacturer, "Manufacturer", findings);
        check_type1_attribute(dataset, seg_tags::manufacturer_model_name, "ManufacturerModelName", findings);
        check_type1_attribute(dataset, seg_tags::device_serial_number, "DeviceSerialNumber", findings);
        check_type1_attribute(dataset, seg_tags::software_versions, "SoftwareVersions", findings);
    }
}

void seg_iod_validator::validate_general_image_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type2) {
        constexpr dicom_tag instance_number{0x0020, 0x0013};
        check_type2_attribute(dataset, instance_number, "InstanceNumber", findings);
    }
}

void seg_iod_validator::validate_image_pixel_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::samples_per_pixel, "SamplesPerPixel", findings);
        check_type1_attribute(dataset, tags::photometric_interpretation, "PhotometricInterpretation", findings);
        check_type1_attribute(dataset, tags::rows, "Rows", findings);
        check_type1_attribute(dataset, tags::columns, "Columns", findings);
        check_type1_attribute(dataset, tags::bits_allocated, "BitsAllocated", findings);
        check_type1_attribute(dataset, tags::bits_stored, "BitsStored", findings);
        check_type1_attribute(dataset, tags::high_bit, "HighBit", findings);
        check_type1_attribute(dataset, tags::pixel_representation, "PixelRepresentation", findings);
    }

    // SEG-specific pixel data constraints
    check_pixel_data_consistency(dataset, findings);
}

void seg_iod_validator::validate_segmentation_image_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        check_type1_attribute(dataset, seg_tags::segmentation_type, "SegmentationType", findings);
        check_type1_attribute(dataset, seg_tags::segment_sequence, "SegmentSequence", findings);
    }

    // Validate segmentation type value
    check_segmentation_type(dataset, findings);

    // Check conditional attributes based on segmentation type
    if (options_.check_conditional && dataset.contains(seg_tags::segmentation_type)) {
        auto seg_type = dataset.get_string(seg_tags::segmentation_type);

        if (seg_type == "FRACTIONAL") {
            // MaxFractionalValue is Type 1 for FRACTIONAL
            if (!dataset.contains(seg_tags::max_fractional_value)) {
                findings.push_back({
                    validation_severity::error,
                    seg_tags::max_fractional_value,
                    "MaxFractionalValue (0062,000E) is required when SegmentationType is FRACTIONAL",
                    "SEG-ERR-002"
                });
            }

            // SegmentationFractionalType is Type 1 for FRACTIONAL
            if (!dataset.contains(seg_tags::segmentation_fractional_type)) {
                findings.push_back({
                    validation_severity::error,
                    seg_tags::segmentation_fractional_type,
                    "SegmentationFractionalType (0062,0010) is required when SegmentationType is FRACTIONAL",
                    "SEG-ERR-003"
                });
            }
        }
    }
}

void seg_iod_validator::validate_segment_sequence(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(seg_tags::segment_sequence)) {
        // Already reported as Type 1 missing
        return;
    }

    // Get sequence items
    auto sequence = dataset.get_sequence(seg_tags::segment_sequence);
    if (!sequence || sequence->empty()) {
        findings.push_back({
            validation_severity::error,
            seg_tags::segment_sequence,
            "SegmentSequence must contain at least one segment",
            "SEG-ERR-004"
        });
        return;
    }

    // Validate each segment
    for (size_t i = 0; i < sequence->size(); ++i) {
        const auto& segment_item = (*sequence)[i];
        validate_single_segment(segment_item, i, findings);
    }
}

void seg_iod_validator::validate_single_segment(
    const dicom_dataset& segment_item,
    size_t segment_index,
    std::vector<validation_finding>& findings) const {

    std::string prefix = "Segment[" + std::to_string(segment_index) + "]: ";

    // Type 1 attributes within segment
    if (!segment_item.contains(seg_tags::segment_number)) {
        findings.push_back({
            validation_severity::error,
            seg_tags::segment_number,
            prefix + "SegmentNumber (0062,0004) is required",
            "SEG-SEQ-ERR-001"
        });
    }

    if (!segment_item.contains(seg_tags::segment_label)) {
        findings.push_back({
            validation_severity::error,
            seg_tags::segment_label,
            prefix + "SegmentLabel (0062,0005) is required",
            "SEG-SEQ-ERR-002"
        });
    }

    if (!segment_item.contains(seg_tags::segment_algorithm_type)) {
        findings.push_back({
            validation_severity::error,
            seg_tags::segment_algorithm_type,
            prefix + "SegmentAlgorithmType (0062,0008) is required",
            "SEG-SEQ-ERR-003"
        });
    } else if (options_.validate_algorithm_info) {
        auto algo_type = segment_item.get_string(seg_tags::segment_algorithm_type);
        if (!sop_classes::is_valid_segment_algorithm_type(algo_type)) {
            findings.push_back({
                validation_severity::warning,
                seg_tags::segment_algorithm_type,
                prefix + "Invalid SegmentAlgorithmType value: " + algo_type,
                "SEG-SEQ-WARN-001"
            });
        }
    }

    // Segmented Property Category Code Sequence is Type 1
    if (!segment_item.contains(seg_tags::segmented_property_category_code_sequence)) {
        findings.push_back({
            validation_severity::error,
            seg_tags::segmented_property_category_code_sequence,
            prefix + "SegmentedPropertyCategoryCodeSequence (0062,0003) is required",
            "SEG-SEQ-ERR-004"
        });
    }

    // Segmented Property Type Code Sequence is Type 1
    if (!segment_item.contains(seg_tags::segmented_property_type_code_sequence)) {
        findings.push_back({
            validation_severity::error,
            seg_tags::segmented_property_type_code_sequence,
            prefix + "SegmentedPropertyTypeCodeSequence (0062,000F) is required",
            "SEG-SEQ-ERR-005"
        });
    }

    // Validate segment label is not empty
    if (options_.validate_segment_labels && segment_item.contains(seg_tags::segment_label)) {
        auto label = segment_item.get_string(seg_tags::segment_label);
        if (label.empty()) {
            findings.push_back({
                validation_severity::warning,
                seg_tags::segment_label,
                prefix + "SegmentLabel should not be empty",
                "SEG-SEQ-WARN-002"
            });
        }
    }
}

void seg_iod_validator::validate_multiframe_functional_groups_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        check_type1_attribute(dataset, seg_tags::number_of_frames, "NumberOfFrames", findings);
        check_type1_attribute(dataset, seg_tags::shared_functional_groups_sequence,
                             "SharedFunctionalGroupsSequence", findings);
        check_type1_attribute(dataset, seg_tags::per_frame_functional_groups_sequence,
                             "PerFrameFunctionalGroupsSequence", findings);
    }
}

void seg_iod_validator::validate_multiframe_dimension_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        check_type1_attribute(dataset, seg_tags::dimension_organization_sequence,
                             "DimensionOrganizationSequence", findings);
        check_type1_attribute(dataset, seg_tags::dimension_index_sequence,
                             "DimensionIndexSequence", findings);
    }
}

void seg_iod_validator::validate_common_instance_reference_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // Referenced Series Sequence is Type 1C - required if references exist
    if (options_.check_conditional) {
        if (!dataset.contains(seg_tags::referenced_series_sequence_cir)) {
            findings.push_back({
                validation_severity::warning,
                seg_tags::referenced_series_sequence_cir,
                "ReferencedSeriesSequence (0008,1115) should be present for source image references",
                "SEG-REF-WARN-001"
            });
        }
    }
}

void seg_iod_validator::validate_sop_common_module(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (options_.check_type1) {
        check_type1_attribute(dataset, tags::sop_class_uid, "SOPClassUID", findings);
        check_type1_attribute(dataset, tags::sop_instance_uid, "SOPInstanceUID", findings);
    }

    // Validate SOP Class UID is a SEG storage class
    if (dataset.contains(tags::sop_class_uid)) {
        auto sop_class = dataset.get_string(tags::sop_class_uid);
        if (!sop_classes::is_seg_storage_sop_class(sop_class)) {
            findings.push_back({
                validation_severity::error,
                tags::sop_class_uid,
                "SOPClassUID is not a recognized SEG Storage SOP Class: " + sop_class,
                "SEG-ERR-005"
            });
        }
    }
}

// =============================================================================
// Attribute Validation Helpers
// =============================================================================

void seg_iod_validator::check_type1_attribute(
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
            "SEG-TYPE1-MISSING"
        });
    } else {
        auto value = dataset.get_string(tag);
        if (value.empty()) {
            findings.push_back({
                validation_severity::error,
                tag,
                std::string("Type 1 attribute has empty value: ") +
                std::string(name) + " (" + tag.to_string() + ")",
                "SEG-TYPE1-EMPTY"
            });
        }
    }
}

void seg_iod_validator::check_type2_attribute(
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
            "SEG-TYPE2-MISSING"
        });
    }
}

void seg_iod_validator::check_modality(
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
            "Modality must be 'SEG' for Segmentation objects, found: " + modality,
            "SEG-ERR-001"
        });
    }
}

void seg_iod_validator::check_segmentation_type(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    if (!dataset.contains(seg_tags::segmentation_type)) {
        return;  // Already reported
    }

    auto seg_type = dataset.get_string(seg_tags::segmentation_type);
    if (!sop_classes::is_valid_segmentation_type(seg_type)) {
        findings.push_back({
            validation_severity::error,
            seg_tags::segmentation_type,
            "Invalid SegmentationType value: " + seg_type + " (must be BINARY or FRACTIONAL)",
            "SEG-ERR-006"
        });
    }
}

void seg_iod_validator::check_pixel_data_consistency(
    const dicom_dataset& dataset,
    std::vector<validation_finding>& findings) const {

    // For SEG, SamplesPerPixel must be 1
    auto samples = dataset.get_numeric<uint16_t>(tags::samples_per_pixel);
    if (samples && *samples != 1) {
        findings.push_back({
            validation_severity::error,
            tags::samples_per_pixel,
            "SamplesPerPixel must be 1 for Segmentation objects",
            "SEG-PXL-ERR-001"
        });
    }

    // PhotometricInterpretation must be MONOCHROME2
    if (dataset.contains(tags::photometric_interpretation)) {
        auto photometric = dataset.get_string(tags::photometric_interpretation);
        if (photometric != "MONOCHROME2") {
            findings.push_back({
                validation_severity::error,
                tags::photometric_interpretation,
                "PhotometricInterpretation must be MONOCHROME2 for Segmentation",
                "SEG-PXL-ERR-002"
            });
        }
    }

    // Check BitsAllocated based on SegmentationType
    auto bits_allocated = dataset.get_numeric<uint16_t>(tags::bits_allocated);
    auto seg_type = dataset.get_string(seg_tags::segmentation_type);

    if (bits_allocated && !seg_type.empty()) {
        if (seg_type == "BINARY" && *bits_allocated != 1) {
            findings.push_back({
                validation_severity::warning,
                tags::bits_allocated,
                "BitsAllocated is typically 1 for BINARY segmentation, found: " +
                std::to_string(*bits_allocated),
                "SEG-PXL-WARN-001"
            });
        }

        if (seg_type == "FRACTIONAL" && *bits_allocated != 8) {
            findings.push_back({
                validation_severity::warning,
                tags::bits_allocated,
                "BitsAllocated is typically 8 for FRACTIONAL segmentation, found: " +
                std::to_string(*bits_allocated),
                "SEG-PXL-WARN-002"
            });
        }
    }
}

// =============================================================================
// Convenience Functions
// =============================================================================

validation_result validate_seg_iod(const dicom_dataset& dataset) {
    seg_iod_validator validator;
    return validator.validate(dataset);
}

bool is_valid_seg_dataset(const dicom_dataset& dataset) {
    seg_iod_validator validator;
    return validator.quick_check(dataset);
}

bool is_binary_segmentation(const dicom_dataset& dataset) {
    constexpr dicom_tag segmentation_type{0x0062, 0x0001};
    if (!dataset.contains(segmentation_type)) {
        return false;
    }
    return dataset.get_string(segmentation_type) == "BINARY";
}

bool is_fractional_segmentation(const dicom_dataset& dataset) {
    constexpr dicom_tag segmentation_type{0x0062, 0x0001};
    if (!dataset.contains(segmentation_type)) {
        return false;
    }
    return dataset.get_string(segmentation_type) == "FRACTIONAL";
}

size_t get_segment_count(const dicom_dataset& dataset) {
    constexpr dicom_tag segment_sequence{0x0062, 0x0002};
    auto sequence = dataset.get_sequence(segment_sequence);
    if (sequence) {
        return sequence->size();
    }
    return 0;
}

}  // namespace pacs::services::validation
