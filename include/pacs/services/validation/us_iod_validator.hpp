/**
 * @file us_iod_validator.hpp
 * @brief Ultrasound Image IOD Validator
 *
 * Provides validation for Ultrasound Image Information Object Definitions
 * as specified in DICOM PS3.3 Section A.6 (US Image IOD) and A.7
 * (US Multi-frame Image IOD).
 *
 * @see DICOM PS3.3 Section A.6 - US Image IOD
 * @see DICOM PS3.3 Section A.7 - US Multi-frame Image IOD
 * @see DES-SVC-008 - Ultrasound Storage Implementation
 */

#ifndef PACS_SERVICES_VALIDATION_US_IOD_VALIDATOR_HPP
#define PACS_SERVICES_VALIDATION_US_IOD_VALIDATOR_HPP

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_tag.hpp"

#include <optional>
#include <string>
#include <vector>

namespace pacs::services::validation {

// =============================================================================
// Validation Result Types
// =============================================================================

/**
 * @brief Severity level of validation findings
 */
enum class validation_severity {
    error,      ///< Critical - IOD is non-compliant
    warning,    ///< Non-critical - IOD may have issues
    info        ///< Informational - suggestion for improvement
};

/**
 * @brief Single validation finding
 */
struct validation_finding {
    validation_severity severity;   ///< How serious is this finding
    core::dicom_tag tag;            ///< The tag involved (if applicable)
    std::string message;            ///< Human-readable description
    std::string code;               ///< Machine-readable code (e.g., "US-001")
};

/**
 * @brief Result of IOD validation
 */
struct validation_result {
    bool is_valid;                          ///< Overall validation status
    std::vector<validation_finding> findings; ///< All findings during validation

    /**
     * @brief Check if there are any errors
     */
    [[nodiscard]] bool has_errors() const noexcept;

    /**
     * @brief Check if there are any warnings
     */
    [[nodiscard]] bool has_warnings() const noexcept;

    /**
     * @brief Get count of errors
     */
    [[nodiscard]] size_t error_count() const noexcept;

    /**
     * @brief Get count of warnings
     */
    [[nodiscard]] size_t warning_count() const noexcept;

    /**
     * @brief Get a formatted summary string
     */
    [[nodiscard]] std::string summary() const;
};

// =============================================================================
// Validation Options
// =============================================================================

/**
 * @brief Options for US IOD validation
 */
struct us_validation_options {
    /// Check Type 1 (required) attributes
    bool check_type1 = true;

    /// Check Type 2 (required, can be empty) attributes
    bool check_type2 = true;

    /// Check Type 1C/2C (conditionally required) attributes
    bool check_conditional = true;

    /// Validate pixel data consistency (rows, columns, bits)
    bool validate_pixel_data = true;

    /// Validate US Region Sequence if present
    bool validate_regions = true;

    /// Allow retired attributes
    bool allow_retired = true;

    /// Strict mode - treat warnings as errors
    bool strict_mode = false;
};

// =============================================================================
// US IOD Validator
// =============================================================================

/**
 * @brief Validator for Ultrasound Image IODs
 *
 * Validates DICOM datasets against the US Image IOD and US Multi-frame
 * Image IOD specifications. Checks required modules, attributes, and
 * value constraints.
 *
 * ## Validated Modules
 *
 * ### Mandatory Modules
 * - Patient Module (M)
 * - General Study Module (M)
 * - General Series Module (M)
 * - General Equipment Module (M)
 * - General Image Module (M)
 * - Image Pixel Module (M)
 * - US Image Module (M)
 * - SOP Common Module (M)
 *
 * ### Conditional Modules
 * - US Multi-frame Module (C) - for multi-frame images
 * - Cine Module (C) - for multi-frame images
 *
 * @example
 * @code
 * us_iod_validator validator;
 * auto result = validator.validate(dataset);
 *
 * if (!result.is_valid) {
 *     for (const auto& finding : result.findings) {
 *         std::cerr << finding.message << "\n";
 *     }
 * }
 * @endcode
 */
class us_iod_validator {
public:
    /**
     * @brief Construct validator with default options
     */
    us_iod_validator() = default;

    /**
     * @brief Construct validator with custom options
     * @param options Validation options
     */
    explicit us_iod_validator(const us_validation_options& options);

    /**
     * @brief Validate a DICOM dataset against US IOD
     *
     * @param dataset The dataset to validate
     * @return Validation result with all findings
     */
    [[nodiscard]] validation_result validate(const core::dicom_dataset& dataset) const;

    /**
     * @brief Validate a multi-frame US dataset
     *
     * Performs additional validation for multi-frame specific attributes.
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
    [[nodiscard]] const us_validation_options& options() const noexcept;

    /**
     * @brief Set validation options
     */
    void set_options(const us_validation_options& options);

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

    void validate_us_image_module(
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

    us_validation_options options_;
};

// =============================================================================
// Convenience Functions
// =============================================================================

/**
 * @brief Validate a US dataset with default options
 *
 * @param dataset The dataset to validate
 * @return Validation result
 */
[[nodiscard]] validation_result validate_us_iod(const core::dicom_dataset& dataset);

/**
 * @brief Quick check if a dataset is a valid US image
 *
 * @param dataset The dataset to check
 * @return true if the dataset passes basic US IOD validation
 */
[[nodiscard]] bool is_valid_us_dataset(const core::dicom_dataset& dataset);

}  // namespace pacs::services::validation

#endif  // PACS_SERVICES_VALIDATION_US_IOD_VALIDATOR_HPP
