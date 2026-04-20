// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file label_map_seg_iod_validator.h
 * @brief Label Map Segmentation IOD Validator
 *
 * Provides validation for Label Map Segmentation Information Object Definitions
 * as specified in DICOM Supplement 243. Label Map segmentation stores a single
 * integer label per voxel for non-overlapping, mutually exclusive classification
 * of all segments in one frame set.
 *
 * @see DICOM Supplement 243 - Label Map Segmentation IOD
 * @see DICOM PS3.3 - Segmentation IOD (existing binary segmentation)
 * @see Issue #850 - Add Label Map Segmentation IOD support
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_SERVICES_VALIDATION_LABEL_MAP_SEG_IOD_VALIDATOR_HPP
#define PACS_SERVICES_VALIDATION_LABEL_MAP_SEG_IOD_VALIDATOR_HPP

#include "kcenon/pacs/core/dicom_dataset.h"
#include "kcenon/pacs/core/dicom_tag.h"
#include "kcenon/pacs/services/validation/us_iod_validator.h"  // For validation_result types

#include <string>
#include <vector>

namespace kcenon::pacs::services::validation {

// =============================================================================
// Label Map Segmentation-Specific DICOM Tags
// =============================================================================

namespace label_map_seg_tags {

/// Segmentation Type (0062,0001) - must be "LABELMAP"
inline constexpr core::dicom_tag segmentation_type{0x0062, 0x0001};

/// Segment Sequence (0062,0002)
inline constexpr core::dicom_tag segment_sequence{0x0062, 0x0002};

/// Segment Number (0062,0004)
inline constexpr core::dicom_tag segment_number{0x0062, 0x0004};

/// Segment Label (0062,0005)
inline constexpr core::dicom_tag segment_label{0x0062, 0x0005};

/// Segment Algorithm Type (0062,0008)
inline constexpr core::dicom_tag segment_algorithm_type{0x0062, 0x0008};

/// Segmented Property Category Code Sequence (0062,0003)
inline constexpr core::dicom_tag segmented_property_category_code_sequence{
    0x0062, 0x0003};

/// Segmented Property Type Code Sequence (0062,000F)
inline constexpr core::dicom_tag segmented_property_type_code_sequence{
    0x0062, 0x000F};

/// Number of Frames (0028,0008)
inline constexpr core::dicom_tag number_of_frames{0x0028, 0x0008};

/// Shared Functional Groups Sequence (5200,9229)
inline constexpr core::dicom_tag shared_functional_groups_sequence{
    0x5200, 0x9229};

/// Per-Frame Functional Groups Sequence (5200,9230)
inline constexpr core::dicom_tag per_frame_functional_groups_sequence{
    0x5200, 0x9230};

/// Dimension Organization Sequence (0020,9221)
inline constexpr core::dicom_tag dimension_organization_sequence{
    0x0020, 0x9221};

/// Dimension Index Sequence (0020,9222)
inline constexpr core::dicom_tag dimension_index_sequence{0x0020, 0x9222};

/// Referenced Series Sequence (0008,1115)
inline constexpr core::dicom_tag referenced_series_sequence{0x0008, 0x1115};

/// Enhanced General Equipment Module tags
inline constexpr core::dicom_tag manufacturer{0x0008, 0x0070};
inline constexpr core::dicom_tag manufacturer_model_name{0x0008, 0x1090};
inline constexpr core::dicom_tag device_serial_number{0x0018, 0x1000};
inline constexpr core::dicom_tag software_versions{0x0018, 0x1020};

}  // namespace label_map_seg_tags

// =============================================================================
// Label Map Segmentation Validation Options
// =============================================================================

/**
 * @brief Options for Label Map Segmentation IOD validation
 */
struct label_map_seg_validation_options {
    /// Check Type 1 (required) attributes
    bool check_type1 = true;

    /// Check Type 2 (required, can be empty) attributes
    bool check_type2 = true;

    /// Check Type 1C/2C (conditionally required) attributes
    bool check_conditional = true;

    /// Validate Segment Sequence structure
    bool validate_segment_sequence = true;

    /// Validate referenced series/instances
    bool validate_references = true;

    /// Validate pixel data consistency for label map
    bool validate_pixel_data = true;

    /// Validate segment algorithm identification
    bool validate_algorithm_info = true;

    /// Validate label map-specific instance attributes
    bool validate_label_map_instance = true;

    /// Strict mode - treat warnings as errors
    bool strict_mode = false;
};

// =============================================================================
// Label Map Segmentation IOD Validator
// =============================================================================

/**
 * @brief Validator for Label Map Segmentation IODs
 *
 * Validates DICOM datasets against the Label Map Segmentation IOD
 * specification as defined in Supplement 243. Unlike binary segmentation
 * which stores one segment per frame, Label Map stores a single integer
 * label per voxel with all segments in one frame set.
 *
 * ## Key Constraints
 * - Segments are non-overlapping (mutually exclusive classification)
 * - Pixel data contains unsigned integer labels (8-bit or 16-bit)
 * - Label 0 is reserved for background
 * - Each non-zero label value maps to a segment in the Segment Sequence
 *
 * ## Validated Modules
 *
 * ### Mandatory Modules
 * - Patient Module (M)
 * - General Study Module (M)
 * - General Series Module (M)
 * - Label Map Segmentation Series Module (M) -- Modality = "SEG"
 * - General Equipment Module (M)
 * - Enhanced General Equipment Module (M)
 * - General Image Module (M)
 * - Image Pixel Module (M)
 * - Label Map Segmentation Image Module (M)
 * - Multi-frame Functional Groups Module (M)
 * - Multi-frame Dimension Module (M)
 * - Common Instance Reference Module (M)
 * - SOP Common Module (M)
 *
 * @par Example:
 * @code
 * label_map_seg_iod_validator validator;
 * auto result = validator.validate(dataset);
 *
 * if (!result.is_valid) {
 *     for (const auto& finding : result.findings) {
 *         std::cerr << finding.code << ": " << finding.message << "\n";
 *     }
 * }
 * @endcode
 */
class label_map_seg_iod_validator {
public:
    /**
     * @brief Construct validator with default options
     */
    label_map_seg_iod_validator() = default;

    /**
     * @brief Construct validator with custom options
     * @param options Validation options
     */
    explicit label_map_seg_iod_validator(
        const label_map_seg_validation_options& options);

    /**
     * @brief Validate a DICOM dataset against Label Map Segmentation IOD
     *
     * @param dataset The dataset to validate
     * @return Validation result with all findings
     */
    [[nodiscard]] validation_result validate(
        const core::dicom_dataset& dataset) const;

    /**
     * @brief Validate segment sequence completeness
     *
     * @param dataset The dataset to validate
     * @return Validation result for segment sequence
     */
    [[nodiscard]] validation_result
    validate_segments(const core::dicom_dataset& dataset) const;

    /**
     * @brief Quick check if dataset has minimum required attributes
     *
     * @param dataset The dataset to check
     * @return true if all Type 1 attributes are present
     */
    [[nodiscard]] bool quick_check(const core::dicom_dataset& dataset) const;

    /**
     * @brief Get the validation options
     */
    [[nodiscard]] const label_map_seg_validation_options&
    options() const noexcept;

    /**
     * @brief Set validation options
     */
    void set_options(const label_map_seg_validation_options& options);

private:
    void validate_patient_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_general_study_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_general_series_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_label_map_segmentation_series_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_general_equipment_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_enhanced_general_equipment_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_general_image_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_image_pixel_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_label_map_segmentation_image_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_segment_sequence(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_multiframe_functional_groups_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_multiframe_dimension_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_common_instance_reference_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_sop_common_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    // Attribute validation helpers
    void check_type1_attribute(
        const core::dicom_dataset& dataset,
        core::dicom_tag tag,
        std::string_view name,
        std::vector<validation_finding>& findings) const;

    void check_type2_attribute(
        const core::dicom_dataset& dataset,
        core::dicom_tag tag,
        std::string_view name,
        std::vector<validation_finding>& findings) const;

    void check_modality(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void check_segmentation_type(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void check_pixel_data_consistency(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_single_segment(
        const core::dicom_dataset& segment_item,
        size_t segment_index,
        std::vector<validation_finding>& findings) const;

    label_map_seg_validation_options options_;
};

// =============================================================================
// Convenience Functions
// =============================================================================

/**
 * @brief Validate a Label Map Segmentation dataset with default options
 *
 * @param dataset The dataset to validate
 * @return Validation result
 */
[[nodiscard]] validation_result validate_label_map_seg_iod(
    const core::dicom_dataset& dataset);

/**
 * @brief Quick check if a dataset is a valid Label Map Segmentation object
 *
 * @param dataset The dataset to check
 * @return true if the dataset passes basic Label Map Segmentation validation
 */
[[nodiscard]] bool is_valid_label_map_seg_dataset(
    const core::dicom_dataset& dataset);

/**
 * @brief Check if dataset is a label map segmentation
 *
 * @param dataset The dataset to check
 * @return true if Segmentation Type is LABELMAP
 */
[[nodiscard]] bool is_label_map_segmentation(
    const core::dicom_dataset& dataset);

}  // namespace kcenon::pacs::services::validation

#endif  // PACS_SERVICES_VALIDATION_LABEL_MAP_SEG_IOD_VALIDATOR_HPP
