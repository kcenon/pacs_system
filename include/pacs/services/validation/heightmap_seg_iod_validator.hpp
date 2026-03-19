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
 * @file heightmap_seg_iod_validator.hpp
 * @brief Heightmap Segmentation IOD Validator
 *
 * Provides validation for Heightmap Segmentation Information Object Definitions
 * as specified in DICOM Supplement 240 (DICOM 2024d). Heightmap segmentation
 * stores boundary positions as scalar height values per A-scan column, enabling
 * compact representation of retinal layer boundaries in ophthalmic OCT imaging.
 *
 * @see DICOM Supplement 240 - Heightmap Segmentation IOD
 * @see DICOM PS3.3 - Heightmap Segmentation IOD Definition
 * @see Issue #849 - Add Heightmap Segmentation IOD support
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_SERVICES_VALIDATION_HEIGHTMAP_SEG_IOD_VALIDATOR_HPP
#define PACS_SERVICES_VALIDATION_HEIGHTMAP_SEG_IOD_VALIDATOR_HPP

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_tag.hpp"
#include "pacs/services/validation/us_iod_validator.hpp"  // For validation_result types

#include <string>
#include <vector>

namespace kcenon::pacs::services::validation {

// =============================================================================
// Heightmap Segmentation-Specific DICOM Tags
// =============================================================================

namespace heightmap_seg_tags {

/// Heightmap Segmentation Series Module - Modality (0008,0060) must be "SEG"
inline constexpr core::dicom_tag modality{0x0008, 0x0060};

/// Segmentation Type (0062,0001) - must be "HEIGHTMAP"
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
inline constexpr core::dicom_tag segmented_property_category_code_sequence{0x0062, 0x0003};

/// Segmented Property Type Code Sequence (0062,000F)
inline constexpr core::dicom_tag segmented_property_type_code_sequence{0x0062, 0x000F};

/// Number of Frames (0028,0008)
inline constexpr core::dicom_tag number_of_frames{0x0028, 0x0008};

/// Shared Functional Groups Sequence (5200,9229)
inline constexpr core::dicom_tag shared_functional_groups_sequence{0x5200, 0x9229};

/// Per-Frame Functional Groups Sequence (5200,9230)
inline constexpr core::dicom_tag per_frame_functional_groups_sequence{0x5200, 0x9230};

/// Dimension Organization Sequence (0020,9221)
inline constexpr core::dicom_tag dimension_organization_sequence{0x0020, 0x9221};

/// Dimension Index Sequence (0020,9222)
inline constexpr core::dicom_tag dimension_index_sequence{0x0020, 0x9222};

/// Referenced Series Sequence (0008,1115) - Common Instance Reference Module
inline constexpr core::dicom_tag referenced_series_sequence{0x0008, 0x1115};

/// Enhanced General Equipment Module tags
inline constexpr core::dicom_tag manufacturer{0x0008, 0x0070};
inline constexpr core::dicom_tag manufacturer_model_name{0x0008, 0x1090};
inline constexpr core::dicom_tag device_serial_number{0x0018, 0x1000};
inline constexpr core::dicom_tag software_versions{0x0018, 0x1020};

/// Image Laterality (0020,0062) - required for ophthalmic context
inline constexpr core::dicom_tag image_laterality{0x0020, 0x0062};

/// Anatomic Region Sequence (0008,2218)
inline constexpr core::dicom_tag anatomic_region_sequence{0x0008, 0x2218};

}  // namespace heightmap_seg_tags

// =============================================================================
// Heightmap Segmentation Validation Options
// =============================================================================

/**
 * @brief Options for Heightmap Segmentation IOD validation
 */
struct heightmap_seg_validation_options {
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

    /// Validate pixel data consistency for heightmap
    bool validate_pixel_data = true;

    /// Validate segment algorithm identification
    bool validate_algorithm_info = true;

    /// Validate heightmap-specific instance attributes
    bool validate_heightmap_instance = true;

    /// Strict mode - treat warnings as errors
    bool strict_mode = false;
};

// =============================================================================
// Heightmap Segmentation IOD Validator
// =============================================================================

/**
 * @brief Validator for Heightmap Segmentation IODs
 *
 * Validates DICOM datasets against the Heightmap Segmentation IOD specification
 * as defined in Supplement 240. Unlike binary/fractional segmentation which
 * stores per-pixel labels, heightmap segmentation stores boundary positions
 * as scalar height values per A-scan column.
 *
 * ## Validated Modules
 *
 * ### Mandatory Modules
 * - Patient Module (M)
 * - General Study Module (M)
 * - General Series Module (M)
 * - Heightmap Segmentation Series Module (M) -- Modality = "SEG"
 * - General Equipment Module (M)
 * - Enhanced General Equipment Module (M)
 * - General Image Module (M)
 * - Image Pixel Module (M)
 * - Heightmap Segmentation Image Module (M)
 * - Multi-frame Functional Groups Module (M)
 * - Multi-frame Dimension Module (M)
 * - Common Instance Reference Module (M)
 * - SOP Common Module (M)
 *
 * @example
 * @code
 * heightmap_seg_iod_validator validator;
 * auto result = validator.validate(dataset);
 *
 * if (!result.is_valid) {
 *     for (const auto& finding : result.findings) {
 *         std::cerr << finding.code << ": " << finding.message << "\n";
 *     }
 * }
 * @endcode
 */
class heightmap_seg_iod_validator {
public:
    /**
     * @brief Construct validator with default options
     */
    heightmap_seg_iod_validator() = default;

    /**
     * @brief Construct validator with custom options
     * @param options Validation options
     */
    explicit heightmap_seg_iod_validator(
        const heightmap_seg_validation_options& options);

    /**
     * @brief Validate a DICOM dataset against Heightmap Segmentation IOD
     *
     * @param dataset The dataset to validate
     * @return Validation result with all findings
     */
    [[nodiscard]] validation_result validate(
        const core::dicom_dataset& dataset) const;

    /**
     * @brief Validate segment sequence completeness
     *
     * Checks that all segments are properly defined with required attributes.
     *
     * @param dataset The dataset to validate
     * @return Validation result for segment sequence
     */
    [[nodiscard]] validation_result
    validate_segments(const core::dicom_dataset& dataset) const;

    /**
     * @brief Quick check if dataset has minimum required attributes
     *
     * Faster than full validation - only checks Type 1 attributes.
     *
     * @param dataset The dataset to check
     * @return true if all Type 1 attributes are present
     */
    [[nodiscard]] bool quick_check(const core::dicom_dataset& dataset) const;

    /**
     * @brief Get the validation options
     */
    [[nodiscard]] const heightmap_seg_validation_options& options() const noexcept;

    /**
     * @brief Set validation options
     */
    void set_options(const heightmap_seg_validation_options& options);

private:
    // Module validation methods
    void validate_patient_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_general_study_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_general_series_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_heightmap_segmentation_series_module(
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

    void validate_heightmap_segmentation_image_module(
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

    heightmap_seg_validation_options options_;
};

// =============================================================================
// Convenience Functions
// =============================================================================

/**
 * @brief Validate a Heightmap Segmentation dataset with default options
 *
 * @param dataset The dataset to validate
 * @return Validation result
 */
[[nodiscard]] validation_result validate_heightmap_seg_iod(
    const core::dicom_dataset& dataset);

/**
 * @brief Quick check if a dataset is a valid Heightmap Segmentation object
 *
 * @param dataset The dataset to check
 * @return true if the dataset passes basic Heightmap Segmentation IOD validation
 */
[[nodiscard]] bool is_valid_heightmap_seg_dataset(
    const core::dicom_dataset& dataset);

/**
 * @brief Check if dataset is a heightmap segmentation
 *
 * @param dataset The dataset to check
 * @return true if Segmentation Type is HEIGHTMAP
 */
[[nodiscard]] bool is_heightmap_segmentation(
    const core::dicom_dataset& dataset);

}  // namespace kcenon::pacs::services::validation

#endif  // PACS_SERVICES_VALIDATION_HEIGHTMAP_SEG_IOD_VALIDATOR_HPP
