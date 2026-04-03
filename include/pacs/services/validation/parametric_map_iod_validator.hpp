// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file parametric_map_iod_validator.hpp
 * @brief Parametric Map IOD Validator
 *
 * Provides validation for Parametric Map Information Object Definitions
 * as specified in DICOM PS3.3 Section A.75 (Parametric Map IOD).
 *
 * Parametric Maps encode voxel-level quantitative data (ADC maps, perfusion
 * maps, T1/T2 relaxometry maps) using IEEE 754 float or double pixel values.
 * They are always multi-frame and require Real World Value Mapping sequences.
 *
 * @see DICOM PS3.3 Section A.75 - Parametric Map IOD
 * @see DICOM PS3.3 Section C.7.6.16.2.17 - Real World Value Mapping
 * @see Issue #834 - Add Parametric Map IOD validator
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_SERVICES_VALIDATION_PARAMETRIC_MAP_IOD_VALIDATOR_HPP
#define PACS_SERVICES_VALIDATION_PARAMETRIC_MAP_IOD_VALIDATOR_HPP

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_tag.hpp"
#include "pacs/services/validation/us_iod_validator.hpp"  // Reuse validation types

#include <string>
#include <vector>

namespace kcenon::pacs::services::validation {

// =============================================================================
// Parametric Map-Specific DICOM Tags for Validation
// =============================================================================

namespace pmap_iod_tags {

/// Real World Value Mapping Sequence (0040,9096)
inline constexpr core::dicom_tag real_world_value_mapping_sequence{0x0040, 0x9096};

/// Real World Value Intercept (0040,9224)
inline constexpr core::dicom_tag real_world_value_intercept{0x0040, 0x9224};

/// Real World Value Slope (0040,9225)
inline constexpr core::dicom_tag real_world_value_slope{0x0040, 0x9225};

/// Measurement Units Code Sequence (0040,08EA)
inline constexpr core::dicom_tag measurement_units_code_sequence{0x0040, 0x08EA};

/// Content Label (0070,0080) — Type 1
inline constexpr core::dicom_tag content_label{0x0070, 0x0080};

/// Content Description (0070,0081) — Type 2
inline constexpr core::dicom_tag content_description{0x0070, 0x0081};

/// Content Creator's Name (0070,0084) — Type 2
inline constexpr core::dicom_tag content_creator_name{0x0070, 0x0084};

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

/// Referenced Series Sequence (0008,1115)
inline constexpr core::dicom_tag referenced_series_sequence{0x0008, 0x1115};

/// Manufacturer (0008,0070)
inline constexpr core::dicom_tag manufacturer{0x0008, 0x0070};

/// Manufacturer's Model Name (0008,1090)
inline constexpr core::dicom_tag manufacturer_model_name{0x0008, 0x1090};

/// Device Serial Number (0018,1000)
inline constexpr core::dicom_tag device_serial_number{0x0018, 0x1000};

/// Software Versions (0018,1020)
inline constexpr core::dicom_tag software_versions{0x0018, 0x1020};

/// Float Pixel Data (7FE0,0008)
inline constexpr core::dicom_tag float_pixel_data{0x7FE0, 0x0008};

/// Double Float Pixel Data (7FE0,0009)
inline constexpr core::dicom_tag double_float_pixel_data{0x7FE0, 0x0009};

}  // namespace pmap_iod_tags

// =============================================================================
// Parametric Map Validation Options
// =============================================================================

/**
 * @brief Options for Parametric Map IOD validation
 */
struct pmap_validation_options {
    /// Check Type 1 (required) attributes
    bool check_type1 = true;

    /// Check Type 2 (required, can be empty) attributes
    bool check_type2 = true;

    /// Check Type 1C/2C (conditionally required) attributes
    bool check_conditional = true;

    /// Validate pixel data consistency (bits, photometric, float constraints)
    bool validate_pixel_data = true;

    /// Validate Real World Value Mapping Sequence
    bool validate_rwvm = true;

    /// Validate multi-frame structure
    bool validate_multiframe = true;

    /// Validate referenced series/instances
    bool validate_references = true;

    /// Strict mode - treat warnings as errors
    bool strict_mode = false;
};

// =============================================================================
// Parametric Map IOD Validator
// =============================================================================

/**
 * @brief Validator for Parametric Map IODs
 *
 * Validates DICOM datasets against the Parametric Map IOD specification.
 * Checks required modules, attributes, float pixel data constraints,
 * Real World Value Mapping, and multi-frame requirements.
 *
 * ## Validated Modules
 *
 * ### Mandatory Modules
 * - Patient Module (M)
 * - General Study Module (M)
 * - General Series Module (M)
 * - General Equipment Module (M)
 * - Enhanced General Equipment Module (M)
 * - Image Pixel Module (M)
 * - Parametric Map Image Module (M)
 * - Multi-frame Functional Groups Module (M)
 * - Multi-frame Dimension Module (M)
 * - Common Instance Reference Module (M)
 * - SOP Common Module (M)
 *
 * @example
 * @code
 * parametric_map_iod_validator validator;
 * auto result = validator.validate(dataset);
 *
 * if (!result.is_valid) {
 *     for (const auto& finding : result.findings) {
 *         std::cerr << finding.code << ": " << finding.message << "\n";
 *     }
 * }
 * @endcode
 */
class parametric_map_iod_validator {
public:
    /**
     * @brief Construct validator with default options
     */
    parametric_map_iod_validator() = default;

    /**
     * @brief Construct validator with custom options
     * @param options Validation options
     */
    explicit parametric_map_iod_validator(const pmap_validation_options& options);

    /**
     * @brief Validate a DICOM dataset against Parametric Map IOD
     * @param dataset The dataset to validate
     * @return Validation result with all findings
     */
    [[nodiscard]] validation_result validate(
        const core::dicom_dataset& dataset) const;

    /**
     * @brief Quick check if dataset has minimum required parametric map attributes
     * @param dataset The dataset to check
     * @return true if all Type 1 attributes are present
     */
    [[nodiscard]] bool quick_check(
        const core::dicom_dataset& dataset) const;

    /**
     * @brief Get the validation options
     */
    [[nodiscard]] const pmap_validation_options& options() const noexcept;

    /**
     * @brief Set validation options
     */
    void set_options(const pmap_validation_options& options);

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

    void validate_general_equipment_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_enhanced_general_equipment_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_image_pixel_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_parametric_map_image_module(
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

    void check_pixel_data_consistency(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    pmap_validation_options options_;
};

// =============================================================================
// Convenience Functions
// =============================================================================

/**
 * @brief Validate a Parametric Map dataset with default options
 * @param dataset The dataset to validate
 * @return Validation result
 */
[[nodiscard]] validation_result validate_parametric_map_iod(
    const core::dicom_dataset& dataset);

/**
 * @brief Quick check if a dataset is a valid Parametric Map object
 * @param dataset The dataset to check
 * @return true if the dataset passes basic Parametric Map IOD validation
 */
[[nodiscard]] bool is_valid_parametric_map_dataset(
    const core::dicom_dataset& dataset);

}  // namespace kcenon::pacs::services::validation

#endif  // PACS_SERVICES_VALIDATION_PARAMETRIC_MAP_IOD_VALIDATOR_HPP
