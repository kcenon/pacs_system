// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file ct_processing_iod_validator.h
 * @brief CT For Processing Image IOD Validator
 *
 * Provides validation for CT For Processing Image Information Object
 * Definitions as specified in DICOM PS3.3. CT For Processing images
 * store basis material decomposition images from multi-energy/spectral
 * CT acquisitions (dual-energy, photon-counting CT).
 *
 * @see DICOM PS3.3 - CT For Processing IOD
 * @see DICOM PS3.4 Section B - Storage Service Class
 * @see Issue #848 - Add CT For Processing SOP Classes
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_SERVICES_VALIDATION_CT_PROCESSING_IOD_VALIDATOR_HPP
#define PACS_SERVICES_VALIDATION_CT_PROCESSING_IOD_VALIDATOR_HPP

#include "kcenon/pacs/core/dicom_dataset.h"
#include "kcenon/pacs/core/dicom_tag.h"
#include "kcenon/pacs/services/validation/us_iod_validator.h"  // Reuse validation types

#include <string>
#include <vector>

namespace kcenon::pacs::services::validation {

// =============================================================================
// CT Processing-Specific DICOM Tags
// =============================================================================

namespace ct_processing_tags {

/// KVP (0018,0060)
inline constexpr core::dicom_tag kvp{0x0018, 0x0060};

/// Slice Thickness (0018,0050)
inline constexpr core::dicom_tag slice_thickness{0x0018, 0x0050};

/// Convolution Kernel (0018,1210)
inline constexpr core::dicom_tag convolution_kernel{0x0018, 0x1210};

/// Image Position (Patient) (0020,0032)
inline constexpr core::dicom_tag image_position_patient{0x0020, 0x0032};

/// Image Orientation (Patient) (0020,0037)
inline constexpr core::dicom_tag image_orientation_patient{0x0020, 0x0037};

/// Decomposition Description (0018,9380) - describes the material decomposition
inline constexpr core::dicom_tag decomposition_description{0x0018, 0x9380};

/// Decomposition Algorithm Identification Sequence (0018,9381)
inline constexpr core::dicom_tag decomposition_algorithm_id_seq{0x0018, 0x9381};

/// Acquisition Type (0018,9302)
inline constexpr core::dicom_tag acquisition_type{0x0018, 0x9302};

}  // namespace ct_processing_tags

// =============================================================================
// CT Processing Validation Options
// =============================================================================

/**
 * @brief Options for CT For Processing IOD validation
 */
struct ct_processing_validation_options {
    /// Check Type 1 (required) attributes
    bool check_type1 = true;

    /// Check Type 2 (required, can be empty) attributes
    bool check_type2 = true;

    /// Check Type 1C/2C (conditionally required) attributes
    bool check_conditional = true;

    /// Validate pixel data consistency (rows, columns, bits)
    bool validate_pixel_data = true;

    /// Validate CT processing-specific attributes
    bool validate_ct_processing_params = true;

    /// Strict mode - treat warnings as errors
    bool strict_mode = false;
};

// =============================================================================
// CT Processing IOD Validator
// =============================================================================

/**
 * @brief Validator for CT For Processing Image IODs
 *
 * Validates DICOM datasets against the CT For Processing Image IOD
 * specification. CT For Processing images contain basis material
 * decomposition images from multi-energy/spectral CT:
 * - Iodine density maps
 * - Virtual non-contrast (VNC) images
 * - Virtual monoenergetic images
 * - Electron density maps
 * - Effective atomic number (Z-effective) maps
 *
 * ## Validated Modules
 *
 * ### Mandatory Modules
 * - Patient Module (M)
 * - General Study Module (M)
 * - General Series Module (M)
 * - Frame of Reference Module (M)
 * - General Equipment Module (M)
 * - General Image Module (M)
 * - Image Pixel Module (M)
 * - CT Image Module (M)
 * - SOP Common Module (M)
 *
 * @par Example:
 * @code
 * ct_processing_iod_validator validator;
 * auto result = validator.validate(dataset);
 *
 * if (!result.is_valid) {
 *     for (const auto& finding : result.findings) {
 *         std::cerr << finding.message << "\n";
 *     }
 * }
 * @endcode
 */
class ct_processing_iod_validator {
public:
    /**
     * @brief Construct validator with default options
     */
    ct_processing_iod_validator() = default;

    /**
     * @brief Construct validator with custom options
     * @param options Validation options
     */
    explicit ct_processing_iod_validator(
        const ct_processing_validation_options& options);

    /**
     * @brief Validate a DICOM dataset against CT For Processing IOD
     * @param dataset The dataset to validate
     * @return Validation result with all findings
     */
    [[nodiscard]] validation_result validate(
        const core::dicom_dataset& dataset) const;

    /**
     * @brief Quick check if dataset has minimum required attributes
     * @param dataset The dataset to check
     * @return true if all Type 1 attributes are present
     */
    [[nodiscard]] bool quick_check(
        const core::dicom_dataset& dataset) const;

    /**
     * @brief Get the validation options
     */
    [[nodiscard]] const ct_processing_validation_options& options() const noexcept;

    /**
     * @brief Set validation options
     */
    void set_options(const ct_processing_validation_options& options);

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

    void validate_frame_of_reference_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_general_equipment_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_ct_image_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_image_pixel_module(
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

    ct_processing_validation_options options_;
};

// =============================================================================
// Convenience Functions
// =============================================================================

/**
 * @brief Validate a CT For Processing dataset with default options
 * @param dataset The dataset to validate
 * @return Validation result
 */
[[nodiscard]] validation_result validate_ct_processing_iod(
    const core::dicom_dataset& dataset);

/**
 * @brief Quick check if a dataset is a valid CT For Processing image
 * @param dataset The dataset to check
 * @return true if the dataset passes basic validation
 */
[[nodiscard]] bool is_valid_ct_processing_dataset(
    const core::dicom_dataset& dataset);

}  // namespace kcenon::pacs::services::validation

#endif  // PACS_SERVICES_VALIDATION_CT_PROCESSING_IOD_VALIDATOR_HPP
