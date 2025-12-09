/**
 * @file nm_iod_validator.hpp
 * @brief Nuclear Medicine (NM) Image IOD Validator
 *
 * Provides validation for Nuclear Medicine Image Information Object Definitions
 * as specified in DICOM PS3.3 Section A.5 (NM Image IOD).
 *
 * @see DICOM PS3.3 Section A.5 - NM Image IOD
 * @see DICOM PS3.3 Section C.8.4 - NM Modules
 */

#ifndef PACS_SERVICES_VALIDATION_NM_IOD_VALIDATOR_HPP
#define PACS_SERVICES_VALIDATION_NM_IOD_VALIDATOR_HPP

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_tag.hpp"
#include "pacs/services/validation/us_iod_validator.hpp"  // For shared validation types

#include <optional>
#include <string>
#include <vector>

namespace pacs::services::validation {

// =============================================================================
// NM Validation Options
// =============================================================================

/**
 * @brief Options for NM IOD validation
 */
struct nm_validation_options {
    /// Check Type 1 (required) attributes
    bool check_type1 = true;

    /// Check Type 2 (required, can be empty) attributes
    bool check_type2 = true;

    /// Check Type 1C/2C (conditionally required) attributes
    bool check_conditional = true;

    /// Validate pixel data consistency (rows, columns, bits)
    bool validate_pixel_data = true;

    /// Validate NM-specific attributes (detector, collimator, etc.)
    bool validate_nm_specific = true;

    /// Validate energy window information
    bool validate_energy_windows = true;

    /// Validate isotope information
    bool validate_isotope = true;

    /// Allow retired attributes
    bool allow_retired = true;

    /// Strict mode - treat warnings as errors
    bool strict_mode = false;
};

// =============================================================================
// NM IOD Validator
// =============================================================================

/**
 * @brief Validator for Nuclear Medicine Image IODs
 *
 * Validates DICOM datasets against the NM Image IOD specification.
 * Checks required modules, attributes, and NM-specific value constraints.
 *
 * ## Validated Modules
 *
 * ### Mandatory Modules
 * - Patient Module (M)
 * - General Study Module (M)
 * - General Series Module (M)
 * - NM/PET Patient Orientation Module (M)
 * - Frame of Reference Module (C)
 * - General Equipment Module (M)
 * - General Image Module (M)
 * - Image Pixel Module (M)
 * - NM Image Module (M)
 * - NM Isotope Module (M)
 * - NM Detector Module (C)
 * - NM TOMO Acquisition Module (C)
 * - NM Multi-gated Acquisition Module (C)
 * - SOP Common Module (M)
 *
 * ### NM-Specific Validation
 * - Energy Window Information Sequence
 * - Radiopharmaceutical Information Sequence
 * - Detector Information Sequence
 * - Rotation Information
 *
 * @example
 * @code
 * nm_iod_validator validator;
 * auto result = validator.validate(dataset);
 *
 * if (!result.is_valid) {
 *     for (const auto& finding : result.findings) {
 *         std::cerr << finding.message << "\n";
 *     }
 * }
 * @endcode
 */
class nm_iod_validator {
public:
    /**
     * @brief Construct validator with default options
     */
    nm_iod_validator() = default;

    /**
     * @brief Construct validator with custom options
     * @param options Validation options
     */
    explicit nm_iod_validator(const nm_validation_options& options);

    /**
     * @brief Validate a DICOM dataset against NM IOD
     *
     * @param dataset The dataset to validate
     * @return Validation result with all findings
     */
    [[nodiscard]] validation_result validate(const core::dicom_dataset& dataset) const;

    /**
     * @brief Validate a multi-frame NM dataset
     *
     * Performs additional validation for SPECT, dynamic, and gated acquisitions.
     *
     * @param dataset The dataset to validate
     * @return Validation result with all findings
     */
    [[nodiscard]] validation_result
    validate_multiframe(const core::dicom_dataset& dataset) const;

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
    [[nodiscard]] const nm_validation_options& options() const noexcept;

    /**
     * @brief Set validation options
     */
    void set_options(const nm_validation_options& options);

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

    void validate_nm_series_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_nm_image_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_nm_isotope_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_nm_detector_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_nm_tomo_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_nm_gated_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_image_pixel_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_multiframe_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_sop_common_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    // NM-specific validation
    void validate_energy_window_info(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_radiopharmaceutical_info(
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

    nm_validation_options options_;
};

// =============================================================================
// Convenience Functions
// =============================================================================

/**
 * @brief Validate a NM dataset with default options
 *
 * @param dataset The dataset to validate
 * @return Validation result
 */
[[nodiscard]] validation_result validate_nm_iod(const core::dicom_dataset& dataset);

/**
 * @brief Quick check if a dataset is a valid NM image
 *
 * @param dataset The dataset to check
 * @return true if the dataset passes basic NM IOD validation
 */
[[nodiscard]] bool is_valid_nm_dataset(const core::dicom_dataset& dataset);

}  // namespace pacs::services::validation

#endif  // PACS_SERVICES_VALIDATION_NM_IOD_VALIDATOR_HPP
