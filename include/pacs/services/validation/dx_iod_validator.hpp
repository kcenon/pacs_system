/**
 * @file dx_iod_validator.hpp
 * @brief Digital X-Ray (DX) Image IOD Validator
 *
 * Provides validation for Digital X-Ray Image Information Object Definitions
 * as specified in DICOM PS3.3 Section A.26 (DX Image IOD).
 *
 * Digital X-Ray (DX) covers general radiography using digital detectors,
 * supporting both For Presentation and For Processing image types.
 *
 * @see DICOM PS3.3 Section A.26 - DX Image IOD
 * @see DICOM PS3.3 Section C.8.11 - DX Modules
 * @see DES-SVC-009 - Digital X-Ray Storage Implementation
 */

#ifndef PACS_SERVICES_VALIDATION_DX_IOD_VALIDATOR_HPP
#define PACS_SERVICES_VALIDATION_DX_IOD_VALIDATOR_HPP

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_tag.hpp"
#include "pacs/services/validation/us_iod_validator.hpp"  // For shared types

#include <optional>
#include <string>
#include <vector>

namespace pacs::services::validation {

// =============================================================================
// DX Validation Options
// =============================================================================

/**
 * @brief Options for DX IOD validation
 */
struct dx_validation_options {
    /// Check Type 1 (required) attributes
    bool check_type1 = true;

    /// Check Type 2 (required, can be empty) attributes
    bool check_type2 = true;

    /// Check Type 1C/2C (conditionally required) attributes
    bool check_conditional = true;

    /// Validate pixel data consistency (rows, columns, bits)
    bool validate_pixel_data = true;

    /// Validate DX-specific attributes (detector, acquisition)
    bool validate_dx_specific = true;

    /// Validate body part and view position
    bool validate_anatomy = true;

    /// Validate For Presentation specific requirements
    bool validate_presentation_requirements = true;

    /// Validate For Processing specific requirements
    bool validate_processing_requirements = true;

    /// Allow both MONOCHROME1 and MONOCHROME2
    bool allow_both_photometric = true;

    /// Strict mode - treat warnings as errors
    bool strict_mode = false;
};

// =============================================================================
// DX IOD Validator
// =============================================================================

/**
 * @brief Validator for Digital X-Ray Image IODs
 *
 * Validates DICOM datasets against the DX Image IOD specification.
 * Checks required modules, attributes, and value constraints specific
 * to digital radiography.
 *
 * ## Validated Modules
 *
 * ### Mandatory Modules (M)
 * - Patient Module
 * - General Study Module
 * - Patient Study Module
 * - General Series Module
 * - General Equipment Module
 * - General Image Module
 * - Image Pixel Module
 * - DX Anatomy Imaged Module
 * - DX Image Module
 * - DX Detector Module
 * - SOP Common Module
 *
 * ### Conditional Modules (C)
 * - DX Positioning Module (required if view position is specified)
 * - VOI LUT Module (For Presentation images)
 *
 * @example
 * @code
 * dx_iod_validator validator;
 * auto result = validator.validate(dataset);
 *
 * if (!result.is_valid) {
 *     for (const auto& finding : result.findings) {
 *         std::cerr << finding.code << ": " << finding.message << "\n";
 *     }
 * }
 * @endcode
 */
class dx_iod_validator {
public:
    /**
     * @brief Construct validator with default options
     */
    dx_iod_validator() = default;

    /**
     * @brief Construct validator with custom options
     * @param options Validation options
     */
    explicit dx_iod_validator(const dx_validation_options& options);

    /**
     * @brief Validate a DICOM dataset against DX IOD
     *
     * Performs comprehensive validation including patient, study, series,
     * image, and DX-specific modules.
     *
     * @param dataset The dataset to validate
     * @return Validation result with all findings
     */
    [[nodiscard]] validation_result validate(const core::dicom_dataset& dataset) const;

    /**
     * @brief Validate a For Presentation DX dataset
     *
     * Performs additional validation for For Presentation specific
     * requirements including VOI LUT and display parameters.
     *
     * @param dataset The dataset to validate
     * @return Validation result with all findings
     */
    [[nodiscard]] validation_result
    validate_for_presentation(const core::dicom_dataset& dataset) const;

    /**
     * @brief Validate a For Processing DX dataset
     *
     * Performs validation for For Processing specific requirements
     * including raw data attributes.
     *
     * @param dataset The dataset to validate
     * @return Validation result with all findings
     */
    [[nodiscard]] validation_result
    validate_for_processing(const core::dicom_dataset& dataset) const;

    /**
     * @brief Quick check if dataset has minimum required attributes
     *
     * Faster than full validation - only checks critical Type 1 attributes.
     *
     * @param dataset The dataset to check
     * @return true if all critical Type 1 attributes are present
     */
    [[nodiscard]] bool quick_check(const core::dicom_dataset& dataset) const;

    /**
     * @brief Get the validation options
     */
    [[nodiscard]] const dx_validation_options& options() const noexcept;

    /**
     * @brief Set validation options
     */
    void set_options(const dx_validation_options& options);

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

    void validate_dx_anatomy_imaged_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_dx_image_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_dx_detector_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_dx_positioning_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_image_pixel_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_voi_lut_module(
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

    void check_photometric_interpretation(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    dx_validation_options options_;
};

// =============================================================================
// Convenience Functions
// =============================================================================

/**
 * @brief Validate a DX dataset with default options
 *
 * @param dataset The dataset to validate
 * @return Validation result
 */
[[nodiscard]] validation_result validate_dx_iod(const core::dicom_dataset& dataset);

/**
 * @brief Quick check if a dataset is a valid DX image
 *
 * @param dataset The dataset to check
 * @return true if the dataset passes basic DX IOD validation
 */
[[nodiscard]] bool is_valid_dx_dataset(const core::dicom_dataset& dataset);

/**
 * @brief Check if dataset is a For Presentation DX image
 *
 * Examines the SOP Class UID to determine the image type.
 *
 * @param dataset The dataset to check
 * @return true if this is a For Presentation DX image
 */
[[nodiscard]] bool is_for_presentation_dx(const core::dicom_dataset& dataset);

/**
 * @brief Check if dataset is a For Processing DX image
 *
 * Examines the SOP Class UID to determine the image type.
 *
 * @param dataset The dataset to check
 * @return true if this is a For Processing DX image
 */
[[nodiscard]] bool is_for_processing_dx(const core::dicom_dataset& dataset);

}  // namespace pacs::services::validation

#endif  // PACS_SERVICES_VALIDATION_DX_IOD_VALIDATOR_HPP
