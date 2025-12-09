/**
 * @file pet_iod_validator.hpp
 * @brief PET (Positron Emission Tomography) Image IOD Validator
 *
 * Provides validation for PET Image Information Object Definitions
 * as specified in DICOM PS3.3 Section A.21 (PET Image IOD).
 *
 * @see DICOM PS3.3 Section A.21 - PET Image IOD
 * @see DICOM PS3.3 Section C.8.9 - PET Modules
 */

#ifndef PACS_SERVICES_VALIDATION_PET_IOD_VALIDATOR_HPP
#define PACS_SERVICES_VALIDATION_PET_IOD_VALIDATOR_HPP

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_tag.hpp"
#include "pacs/services/validation/us_iod_validator.hpp"  // For shared validation types

#include <optional>
#include <string>
#include <vector>

namespace pacs::services::validation {

// =============================================================================
// PET Validation Options
// =============================================================================

/**
 * @brief Options for PET IOD validation
 */
struct pet_validation_options {
    /// Check Type 1 (required) attributes
    bool check_type1 = true;

    /// Check Type 2 (required, can be empty) attributes
    bool check_type2 = true;

    /// Check Type 1C/2C (conditionally required) attributes
    bool check_conditional = true;

    /// Validate pixel data consistency (rows, columns, bits)
    bool validate_pixel_data = true;

    /// Validate PET-specific attributes (SUV, reconstruction, etc.)
    bool validate_pet_specific = true;

    /// Validate radiopharmaceutical information
    bool validate_radiopharmaceutical = true;

    /// Validate attenuation and scatter correction
    bool validate_corrections = true;

    /// Allow retired attributes
    bool allow_retired = true;

    /// Strict mode - treat warnings as errors
    bool strict_mode = false;
};

// =============================================================================
// PET IOD Validator
// =============================================================================

/**
 * @brief Validator for PET Image IODs
 *
 * Validates DICOM datasets against the PET Image IOD specification.
 * Checks required modules, attributes, and PET-specific value constraints.
 *
 * ## Validated Modules
 *
 * ### Mandatory Modules
 * - Patient Module (M)
 * - General Study Module (M)
 * - Patient Study Module (U)
 * - General Series Module (M)
 * - PET Series Module (M)
 * - Frame of Reference Module (M)
 * - General Equipment Module (M)
 * - General Image Module (M)
 * - Image Plane Module (M)
 * - Image Pixel Module (M)
 * - PET Image Module (M)
 * - SOP Common Module (M)
 *
 * ### PET-Specific Validation
 * - Radiopharmaceutical Information Sequence
 * - SUV calculation parameters
 * - Reconstruction algorithm
 * - Attenuation correction method
 * - Scatter correction method
 *
 * @example
 * @code
 * pet_iod_validator validator;
 * auto result = validator.validate(dataset);
 *
 * if (!result.is_valid) {
 *     for (const auto& finding : result.findings) {
 *         std::cerr << finding.message << "\n";
 *     }
 * }
 * @endcode
 */
class pet_iod_validator {
public:
    /**
     * @brief Construct validator with default options
     */
    pet_iod_validator() = default;

    /**
     * @brief Construct validator with custom options
     * @param options Validation options
     */
    explicit pet_iod_validator(const pet_validation_options& options);

    /**
     * @brief Validate a DICOM dataset against PET IOD
     *
     * @param dataset The dataset to validate
     * @return Validation result with all findings
     */
    [[nodiscard]] validation_result validate(const core::dicom_dataset& dataset) const;

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
    [[nodiscard]] const pet_validation_options& options() const noexcept;

    /**
     * @brief Set validation options
     */
    void set_options(const pet_validation_options& options);

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

    void validate_pet_series_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_frame_of_reference_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_pet_image_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_image_pixel_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_image_plane_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_sop_common_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    // PET-specific validation
    void validate_radiopharmaceutical_info(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_suv_parameters(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_reconstruction_info(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_corrections(
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

    pet_validation_options options_;
};

// =============================================================================
// Convenience Functions
// =============================================================================

/**
 * @brief Validate a PET dataset with default options
 *
 * @param dataset The dataset to validate
 * @return Validation result
 */
[[nodiscard]] validation_result validate_pet_iod(const core::dicom_dataset& dataset);

/**
 * @brief Quick check if a dataset is a valid PET image
 *
 * @param dataset The dataset to check
 * @return true if the dataset passes basic PET IOD validation
 */
[[nodiscard]] bool is_valid_pet_dataset(const core::dicom_dataset& dataset);

}  // namespace pacs::services::validation

#endif  // PACS_SERVICES_VALIDATION_PET_IOD_VALIDATOR_HPP
