/**
 * @file mg_iod_validator.hpp
 * @brief Digital Mammography X-Ray Image IOD Validator
 *
 * Provides validation for Digital Mammography X-Ray Image Information Object
 * Definitions as specified in DICOM PS3.3 Section A.26.2 (Digital Mammography
 * X-Ray Image IOD).
 *
 * Digital Mammography has specific requirements beyond general DX imaging:
 * - Breast laterality must be specified (0020,0060)
 * - View position with mammography-specific codes (0018,5101)
 * - Compression force documentation (0018,11A2)
 * - Breast implant presence indication
 *
 * @see DICOM PS3.3 Section A.26.2 - Digital Mammography X-Ray Image IOD
 * @see DICOM PS3.3 Section C.8.11.7 - Mammography Image Module
 * @see ACR BI-RADS Mammography Atlas for clinical context
 */

#ifndef PACS_SERVICES_VALIDATION_MG_IOD_VALIDATOR_HPP
#define PACS_SERVICES_VALIDATION_MG_IOD_VALIDATOR_HPP

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_tag.hpp"
#include "pacs/services/validation/us_iod_validator.hpp"  // For shared validation types

#include <optional>
#include <string>
#include <vector>

namespace pacs::services::validation {

// =============================================================================
// MG Validation Options
// =============================================================================

/**
 * @brief Options for Mammography IOD validation
 *
 * Provides fine-grained control over which aspects of mammography images
 * are validated. These options allow customization based on workflow needs
 * and the stringency required for different use cases.
 */
struct mg_validation_options {
    /// Check Type 1 (required) attributes
    bool check_type1 = true;

    /// Check Type 2 (required, can be empty) attributes
    bool check_type2 = true;

    /// Check Type 1C/2C (conditionally required) attributes
    bool check_conditional = true;

    /// Validate pixel data consistency (rows, columns, bits)
    bool validate_pixel_data = true;

    /// Validate mammography-specific attributes
    bool validate_mg_specific = true;

    /// Validate breast laterality (0020,0060)
    bool validate_laterality = true;

    /// Validate mammography view position (0018,5101)
    bool validate_view_position = true;

    /// Validate compression force (0018,11A2)
    bool validate_compression = true;

    /// Validate breast implant attributes
    bool validate_implant_attributes = true;

    /// Validate For Presentation specific requirements
    bool validate_presentation_requirements = true;

    /// Validate For Processing specific requirements
    bool validate_processing_requirements = true;

    /// Validate acquisition dose parameters
    bool validate_dose_parameters = true;

    /// Allow both MONOCHROME1 and MONOCHROME2 photometric interpretations
    bool allow_both_photometric = true;

    /// Strict mode - treat warnings as errors
    bool strict_mode = false;

    /// Validate CAD-related attributes if present
    bool validate_cad_attributes = false;
};

// =============================================================================
// MG IOD Validator
// =============================================================================

/**
 * @brief Validator for Digital Mammography X-Ray Image IODs
 *
 * Validates DICOM datasets against the Digital Mammography X-Ray Image IOD
 * specification. This validator extends standard DX validation with
 * mammography-specific requirements.
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
 * - DX Anatomy Imaged Module (with breast-specific requirements)
 * - Mammography Image Module
 * - DX Detector Module
 * - SOP Common Module
 *
 * ### Mammography-Specific Modules (M/C)
 * - Mammography Series Module (M)
 * - Mammography Image Module (M) - includes laterality, view position
 * - X-Ray Acquisition Dose Module (C) - compression force, dose
 * - Breast Implant Module (C) - if implants present
 *
 * ### Conditional Modules (C)
 * - DX Positioning Module
 * - VOI LUT Module (For Presentation images)
 *
 * ## Mammography-Specific Validations
 *
 * 1. **Breast Laterality** (0020,0060):
 *    - Must be L, R, or B
 *    - Should match Image Laterality (0020,0062) if present
 *
 * 2. **View Position** (0018,5101):
 *    - Must be valid mammography view code (CC, MLO, ML, LM, etc.)
 *    - View Code Sequence (0054,0220) preferred for coded values
 *
 * 3. **Compression Force** (0018,11A2):
 *    - Validated against typical range (50-200 N)
 *    - Warning if outside normal range
 *
 * @example
 * @code
 * mg_iod_validator validator;
 * auto result = validator.validate(dataset);
 *
 * if (!result.is_valid) {
 *     for (const auto& finding : result.findings) {
 *         if (finding.code.starts_with("MG-")) {
 *             // Mammography-specific issue
 *             std::cerr << finding.code << ": " << finding.message << "\n";
 *         }
 *     }
 * }
 * @endcode
 */
class mg_iod_validator {
public:
    /**
     * @brief Construct validator with default options
     */
    mg_iod_validator() = default;

    /**
     * @brief Construct validator with custom options
     * @param options Validation options
     */
    explicit mg_iod_validator(const mg_validation_options& options);

    /**
     * @brief Validate a DICOM dataset against Mammography IOD
     *
     * Performs comprehensive validation including patient, study, series,
     * image, and mammography-specific modules.
     *
     * @param dataset The dataset to validate
     * @return Validation result with all findings
     */
    [[nodiscard]] validation_result validate(const core::dicom_dataset& dataset) const;

    /**
     * @brief Validate a For Presentation mammography dataset
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
     * @brief Validate a For Processing mammography dataset
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
     * @brief Validate breast laterality attribute
     *
     * Checks that laterality is present, valid, and consistent with
     * related attributes.
     *
     * @param dataset The dataset to validate
     * @return Validation result for laterality
     */
    [[nodiscard]] validation_result
    validate_laterality(const core::dicom_dataset& dataset) const;

    /**
     * @brief Validate mammography view position
     *
     * Checks that view position is present and contains a valid
     * mammography view code.
     *
     * @param dataset The dataset to validate
     * @return Validation result for view position
     */
    [[nodiscard]] validation_result
    validate_view_position(const core::dicom_dataset& dataset) const;

    /**
     * @brief Validate compression force
     *
     * Checks compression force is present and within typical range.
     *
     * @param dataset The dataset to validate
     * @return Validation result for compression force
     */
    [[nodiscard]] validation_result
    validate_compression_force(const core::dicom_dataset& dataset) const;

    /**
     * @brief Quick check if dataset has minimum required attributes
     *
     * Faster than full validation - only checks critical Type 1 attributes
     * and mammography-specific requirements.
     *
     * @param dataset The dataset to check
     * @return true if all critical attributes are present
     */
    [[nodiscard]] bool quick_check(const core::dicom_dataset& dataset) const;

    /**
     * @brief Get the validation options
     */
    [[nodiscard]] const mg_validation_options& options() const noexcept;

    /**
     * @brief Set validation options
     */
    void set_options(const mg_validation_options& options);

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

    void validate_mammography_series_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_mammography_image_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_dx_anatomy_imaged_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_dx_detector_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_acquisition_dose_module(
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

    void validate_breast_implant_module(
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

    void check_laterality_consistency(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void check_view_position_validity(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void check_compression_force_range(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void check_pixel_data_consistency(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void check_photometric_interpretation(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    mg_validation_options options_;
};

// =============================================================================
// Convenience Functions
// =============================================================================

/**
 * @brief Validate a mammography dataset with default options
 *
 * @param dataset The dataset to validate
 * @return Validation result
 */
[[nodiscard]] validation_result validate_mg_iod(const core::dicom_dataset& dataset);

/**
 * @brief Quick check if a dataset is a valid mammography image
 *
 * @param dataset The dataset to check
 * @return true if the dataset passes basic mammography IOD validation
 */
[[nodiscard]] bool is_valid_mg_dataset(const core::dicom_dataset& dataset);

/**
 * @brief Check if dataset is a For Presentation mammography image
 *
 * Examines the SOP Class UID to determine the image type.
 *
 * @param dataset The dataset to check
 * @return true if this is a For Presentation mammography image
 */
[[nodiscard]] bool is_for_presentation_mg(const core::dicom_dataset& dataset);

/**
 * @brief Check if dataset is a For Processing mammography image
 *
 * Examines the SOP Class UID to determine the image type.
 *
 * @param dataset The dataset to check
 * @return true if this is a For Processing mammography image
 */
[[nodiscard]] bool is_for_processing_mg(const core::dicom_dataset& dataset);

/**
 * @brief Check if dataset indicates breast implant presence
 *
 * @param dataset The dataset to check
 * @return true if breast implant is present according to the dataset
 */
[[nodiscard]] bool has_breast_implant(const core::dicom_dataset& dataset);

/**
 * @brief Check if dataset is a screening mammogram
 *
 * Determines if the image is part of a standard screening exam
 * based on view position (CC or MLO).
 *
 * @param dataset The dataset to check
 * @return true if this appears to be a screening mammogram
 */
[[nodiscard]] bool is_screening_mammogram(const core::dicom_dataset& dataset);

}  // namespace pacs::services::validation

#endif  // PACS_SERVICES_VALIDATION_MG_IOD_VALIDATOR_HPP
