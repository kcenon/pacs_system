/**
 * @file rt_iod_validator.hpp
 * @brief Radiation Therapy (RT) IOD Validators
 *
 * Provides validation for Radiation Therapy Information Object Definitions
 * including RT Plan, RT Dose, and RT Structure Set as specified in DICOM PS3.3.
 *
 * @see DICOM PS3.3 Section A.19 - RT Plan IOD
 * @see DICOM PS3.3 Section A.18 - RT Dose IOD
 * @see DICOM PS3.3 Section A.19 - RT Structure Set IOD
 * @see DICOM PS3.3 Section C.8.8 - RT Modules
 */

#ifndef PACS_SERVICES_VALIDATION_RT_IOD_VALIDATOR_HPP
#define PACS_SERVICES_VALIDATION_RT_IOD_VALIDATOR_HPP

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_tag.hpp"
#include "pacs/services/validation/us_iod_validator.hpp"  // For shared validation types

#include <optional>
#include <string>
#include <vector>

namespace pacs::services::validation {

// =============================================================================
// RT Validation Options
// =============================================================================

/**
 * @brief Options for RT IOD validation
 */
struct rt_validation_options {
    /// Check Type 1 (required) attributes
    bool check_type1 = true;

    /// Check Type 2 (required, can be empty) attributes
    bool check_type2 = true;

    /// Check Type 1C/2C (conditionally required) attributes
    bool check_conditional = true;

    /// Validate RT Plan specific attributes (beams, fractions)
    bool validate_rt_plan = true;

    /// Validate RT Dose specific attributes (dose grid, units)
    bool validate_rt_dose = true;

    /// Validate RT Structure Set specific attributes (ROIs, contours)
    bool validate_rt_structure_set = true;

    /// Validate pixel data consistency (for RT Dose and RT Image)
    bool validate_pixel_data = true;

    /// Validate referenced objects (plans, images)
    bool validate_references = true;

    /// Allow retired attributes
    bool allow_retired = true;

    /// Strict mode - treat warnings as errors
    bool strict_mode = false;
};

// =============================================================================
// RT Plan IOD Validator
// =============================================================================

/**
 * @brief Validator for RT Plan IODs
 *
 * Validates DICOM datasets against the RT Plan IOD specification.
 * Checks required modules, attributes, and RT Plan specific constraints.
 *
 * ## Validated Modules
 *
 * ### Mandatory Modules
 * - Patient Module (M)
 * - General Study Module (M)
 * - RT Series Module (M)
 * - Frame of Reference Module (M)
 * - General Equipment Module (M)
 * - RT General Plan Module (M)
 * - RT Prescription Module (U)
 * - RT Tolerance Tables Module (U)
 * - RT Patient Setup Module (U)
 * - RT Fraction Scheme Module (C)
 * - RT Beams Module (C)
 * - RT Brachy Application Setups Module (C)
 * - Approval Module (U)
 * - SOP Common Module (M)
 */
class rt_plan_iod_validator {
public:
    rt_plan_iod_validator() = default;
    explicit rt_plan_iod_validator(const rt_validation_options& options);

    [[nodiscard]] validation_result validate(const core::dicom_dataset& dataset) const;
    [[nodiscard]] bool quick_check(const core::dicom_dataset& dataset) const;

    [[nodiscard]] const rt_validation_options& options() const noexcept;
    void set_options(const rt_validation_options& options);

private:
    void validate_patient_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_general_study_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_rt_series_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_frame_of_reference_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_rt_general_plan_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_rt_fraction_scheme_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_rt_beams_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_sop_common_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

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

    rt_validation_options options_;
};

// =============================================================================
// RT Dose IOD Validator
// =============================================================================

/**
 * @brief Validator for RT Dose IODs
 *
 * Validates DICOM datasets against the RT Dose IOD specification.
 * Checks dose grid attributes, units, and summation type.
 *
 * ## Validated Modules
 *
 * ### Mandatory Modules
 * - Patient Module (M)
 * - General Study Module (M)
 * - RT Series Module (M)
 * - Frame of Reference Module (M)
 * - General Equipment Module (M)
 * - General Image Module (M)
 * - Image Plane Module (C)
 * - Image Pixel Module (C)
 * - Multi-frame Module (C)
 * - RT Dose Module (M)
 * - RT DVH Module (U)
 * - SOP Common Module (M)
 */
class rt_dose_iod_validator {
public:
    rt_dose_iod_validator() = default;
    explicit rt_dose_iod_validator(const rt_validation_options& options);

    [[nodiscard]] validation_result validate(const core::dicom_dataset& dataset) const;
    [[nodiscard]] bool quick_check(const core::dicom_dataset& dataset) const;

    [[nodiscard]] const rt_validation_options& options() const noexcept;
    void set_options(const rt_validation_options& options);

private:
    void validate_patient_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_general_study_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_rt_series_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_frame_of_reference_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_rt_dose_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_image_pixel_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_sop_common_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

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

    void check_dose_data_consistency(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    rt_validation_options options_;
};

// =============================================================================
// RT Structure Set IOD Validator
// =============================================================================

/**
 * @brief Validator for RT Structure Set IODs
 *
 * Validates DICOM datasets against the RT Structure Set IOD specification.
 * Checks ROI definitions, contours, and referenced frame of reference.
 *
 * ## Validated Modules
 *
 * ### Mandatory Modules
 * - Patient Module (M)
 * - General Study Module (M)
 * - RT Series Module (M)
 * - General Equipment Module (M)
 * - Structure Set Module (M)
 * - ROI Contour Module (M)
 * - RT ROI Observations Module (M)
 * - Approval Module (U)
 * - SOP Common Module (M)
 */
class rt_structure_set_iod_validator {
public:
    rt_structure_set_iod_validator() = default;
    explicit rt_structure_set_iod_validator(const rt_validation_options& options);

    [[nodiscard]] validation_result validate(const core::dicom_dataset& dataset) const;
    [[nodiscard]] bool quick_check(const core::dicom_dataset& dataset) const;

    [[nodiscard]] const rt_validation_options& options() const noexcept;
    void set_options(const rt_validation_options& options);

private:
    void validate_patient_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_general_study_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_rt_series_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_structure_set_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_roi_contour_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_rt_roi_observations_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_sop_common_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

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

    void check_roi_consistency(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    rt_validation_options options_;
};

// =============================================================================
// Unified RT IOD Validator
// =============================================================================

/**
 * @brief Unified validator for all RT IODs
 *
 * Automatically detects the RT object type and applies appropriate validation.
 * Supports RT Plan, RT Dose, RT Structure Set, RT Image, and treatment records.
 */
class rt_iod_validator {
public:
    rt_iod_validator() = default;
    explicit rt_iod_validator(const rt_validation_options& options);

    /**
     * @brief Validate an RT dataset (auto-detects type)
     * @param dataset The dataset to validate
     * @return Validation result with all findings
     */
    [[nodiscard]] validation_result validate(const core::dicom_dataset& dataset) const;

    /**
     * @brief Quick check if dataset has minimum required RT attributes
     * @param dataset The dataset to check
     * @return true if all Type 1 attributes are present
     */
    [[nodiscard]] bool quick_check(const core::dicom_dataset& dataset) const;

    [[nodiscard]] const rt_validation_options& options() const noexcept;
    void set_options(const rt_validation_options& options);

private:
    rt_validation_options options_;
};

// =============================================================================
// Convenience Functions
// =============================================================================

/**
 * @brief Validate an RT Plan dataset with default options
 */
[[nodiscard]] validation_result validate_rt_plan_iod(const core::dicom_dataset& dataset);

/**
 * @brief Validate an RT Dose dataset with default options
 */
[[nodiscard]] validation_result validate_rt_dose_iod(const core::dicom_dataset& dataset);

/**
 * @brief Validate an RT Structure Set dataset with default options
 */
[[nodiscard]] validation_result validate_rt_structure_set_iod(const core::dicom_dataset& dataset);

/**
 * @brief Validate any RT dataset (auto-detects type) with default options
 */
[[nodiscard]] validation_result validate_rt_iod(const core::dicom_dataset& dataset);

/**
 * @brief Quick check if a dataset is a valid RT Plan
 */
[[nodiscard]] bool is_valid_rt_plan_dataset(const core::dicom_dataset& dataset);

/**
 * @brief Quick check if a dataset is a valid RT Dose
 */
[[nodiscard]] bool is_valid_rt_dose_dataset(const core::dicom_dataset& dataset);

/**
 * @brief Quick check if a dataset is a valid RT Structure Set
 */
[[nodiscard]] bool is_valid_rt_structure_set_dataset(const core::dicom_dataset& dataset);

/**
 * @brief Quick check if a dataset is a valid RT object (any type)
 */
[[nodiscard]] bool is_valid_rt_dataset(const core::dicom_dataset& dataset);

}  // namespace pacs::services::validation

#endif  // PACS_SERVICES_VALIDATION_RT_IOD_VALIDATOR_HPP
