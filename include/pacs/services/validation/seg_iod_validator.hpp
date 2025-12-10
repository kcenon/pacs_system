/**
 * @file seg_iod_validator.hpp
 * @brief Segmentation IOD Validator
 *
 * Provides validation for Segmentation Information Object Definitions
 * as specified in DICOM PS3.3 Section A.51 (Segmentation IOD).
 *
 * @see DICOM PS3.3 Section A.51 - Segmentation IOD
 * @see DICOM PS3.3 Section C.8.20 - Segmentation Module
 */

#ifndef PACS_SERVICES_VALIDATION_SEG_IOD_VALIDATOR_HPP
#define PACS_SERVICES_VALIDATION_SEG_IOD_VALIDATOR_HPP

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_tag.hpp"
#include "pacs/services/validation/us_iod_validator.hpp"  // For validation_result types

#include <optional>
#include <string>
#include <vector>

namespace pacs::services::validation {

// =============================================================================
// SEG Validation Options
// =============================================================================

/**
 * @brief Options for SEG IOD validation
 */
struct seg_validation_options {
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

    /// Validate pixel data matches segmentation type
    bool validate_pixel_data = true;

    /// Validate segment algorithm identification
    bool validate_algorithm_info = true;

    /// Validate segment labels and descriptions
    bool validate_segment_labels = true;

    /// Validate anatomical region coding
    bool validate_anatomic_region = true;

    /// Strict mode - treat warnings as errors
    bool strict_mode = false;
};

// =============================================================================
// SEG IOD Validator
// =============================================================================

/**
 * @brief Validator for Segmentation IODs
 *
 * Validates DICOM datasets against the Segmentation IOD specification.
 * Checks required modules, attributes, segment sequences, and value constraints.
 *
 * ## Validated Modules
 *
 * ### Mandatory Modules
 * - Patient Module (M)
 * - Clinical Trial Subject Module (U)
 * - General Study Module (M)
 * - Patient Study Module (U)
 * - General Series Module (M)
 * - Segmentation Series Module (M)
 * - General Equipment Module (M)
 * - Enhanced General Equipment Module (M)
 * - General Image Module (M)
 * - Image Pixel Module (M)
 * - Segmentation Image Module (M)
 * - Multi-frame Functional Groups Module (M)
 * - Multi-frame Dimension Module (M)
 * - Specimen Module (U)
 * - Common Instance Reference Module (M)
 * - SOP Common Module (M)
 *
 * @example
 * @code
 * seg_iod_validator validator;
 * auto result = validator.validate(dataset);
 *
 * if (!result.is_valid) {
 *     for (const auto& finding : result.findings) {
 *         std::cerr << finding.code << ": " << finding.message << "\n";
 *     }
 * }
 * @endcode
 */
class seg_iod_validator {
public:
    /**
     * @brief Construct validator with default options
     */
    seg_iod_validator() = default;

    /**
     * @brief Construct validator with custom options
     * @param options Validation options
     */
    explicit seg_iod_validator(const seg_validation_options& options);

    /**
     * @brief Validate a DICOM dataset against SEG IOD
     *
     * @param dataset The dataset to validate
     * @return Validation result with all findings
     */
    [[nodiscard]] validation_result validate(const core::dicom_dataset& dataset) const;

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
     * @brief Validate referenced instances
     *
     * Checks that all referenced series and instances are properly specified.
     *
     * @param dataset The dataset to validate
     * @return Validation result for references
     */
    [[nodiscard]] validation_result
    validate_references(const core::dicom_dataset& dataset) const;

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
    [[nodiscard]] const seg_validation_options& options() const noexcept;

    /**
     * @brief Set validation options
     */
    void set_options(const seg_validation_options& options);

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

    void validate_segmentation_series_module(
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

    void validate_segmentation_image_module(
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

    seg_validation_options options_;
};

// =============================================================================
// Convenience Functions
// =============================================================================

/**
 * @brief Validate a SEG dataset with default options
 *
 * @param dataset The dataset to validate
 * @return Validation result
 */
[[nodiscard]] validation_result validate_seg_iod(const core::dicom_dataset& dataset);

/**
 * @brief Quick check if a dataset is a valid SEG object
 *
 * @param dataset The dataset to check
 * @return true if the dataset passes basic SEG IOD validation
 */
[[nodiscard]] bool is_valid_seg_dataset(const core::dicom_dataset& dataset);

/**
 * @brief Check if dataset is binary segmentation
 *
 * @param dataset The dataset to check
 * @return true if Segmentation Type is BINARY
 */
[[nodiscard]] bool is_binary_segmentation(const core::dicom_dataset& dataset);

/**
 * @brief Check if dataset is fractional segmentation
 *
 * @param dataset The dataset to check
 * @return true if Segmentation Type is FRACTIONAL
 */
[[nodiscard]] bool is_fractional_segmentation(const core::dicom_dataset& dataset);

/**
 * @brief Get segment count from dataset
 *
 * @param dataset The dataset to check
 * @return Number of segments, or 0 if cannot be determined
 */
[[nodiscard]] size_t get_segment_count(const core::dicom_dataset& dataset);

}  // namespace pacs::services::validation

#endif  // PACS_SERVICES_VALIDATION_SEG_IOD_VALIDATOR_HPP
