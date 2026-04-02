// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file waveform_ps_iod_validator.hpp
 * @brief Waveform Presentation State IOD Validator
 *
 * Provides validation for Waveform Presentation State and Waveform Annotation
 * Information Object Definitions.
 *
 * @see DICOM PS3.3 -- Waveform Presentation State IOD
 * @see DICOM PS3.3 -- Waveform Annotation IOD
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_SERVICES_VALIDATION_WAVEFORM_PS_IOD_VALIDATOR_HPP
#define PACS_SERVICES_VALIDATION_WAVEFORM_PS_IOD_VALIDATOR_HPP

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_tag.hpp"
#include "pacs/services/validation/us_iod_validator.hpp"  // For validation_result types

#include <string>
#include <vector>

namespace kcenon::pacs::services::validation {

// =============================================================================
// Waveform PS Validation Options
// =============================================================================

/**
 * @brief Options for Waveform Presentation State / Annotation IOD validation
 */
struct waveform_ps_validation_options {
    /// Check Type 1 (required) attributes
    bool check_type1 = true;

    /// Check Type 2 (required, can be empty) attributes
    bool check_type2 = true;

    /// Check Type 1C/2C (conditionally required) attributes
    bool check_conditional = true;

    /// Validate referenced waveform sequences
    bool validate_references = true;

    /// Validate presentation state display configuration
    bool validate_display_config = true;

    /// Validate annotation sequences (for Waveform Annotation)
    bool validate_annotations = true;

    /// Strict mode: treat warnings as errors
    bool strict_mode = false;
};

// =============================================================================
// Waveform PS IOD Validator
// =============================================================================

/**
 * @brief Validates Waveform Presentation State and Annotation IODs.
 *
 * Supports validation of:
 * - Waveform Presentation State Storage (1.2.840.10008.5.1.4.1.1.11.11)
 * - Waveform Annotation Storage (1.2.840.10008.5.1.4.1.1.11.12)
 *
 * Validated Modules:
 * - Patient Module (M)
 * - General Study Module (M)
 * - General Series Module (M)
 * - General Equipment Module (M)
 * - Waveform Presentation State Module (M) [for PS]
 * - Waveform Annotation Module (M) [for Annotation]
 * - SOP Common Module (M)
 */
class waveform_ps_iod_validator {
public:
    waveform_ps_iod_validator() = default;
    explicit waveform_ps_iod_validator(const waveform_ps_validation_options& options);

    /**
     * @brief Validate a complete Waveform Presentation State or Annotation dataset.
     * @param dataset The DICOM dataset to validate
     * @return Validation result with findings
     */
    [[nodiscard]] validation_result validate(const core::dicom_dataset& dataset) const;

    /**
     * @brief Validate referenced waveform sequences.
     * @param dataset The DICOM dataset to validate
     * @return Validation result with reference-specific findings
     */
    [[nodiscard]] validation_result validate_references(const core::dicom_dataset& dataset) const;

    /**
     * @brief Quick validation of essential Type 1 attributes only.
     * @param dataset The DICOM dataset to validate
     * @return true if essential attributes are present and valid
     */
    [[nodiscard]] bool quick_check(const core::dicom_dataset& dataset) const;

    /**
     * @brief Get current validation options.
     * @return Reference to validation options
     */
    [[nodiscard]] const waveform_ps_validation_options& options() const noexcept;

    /**
     * @brief Set validation options.
     * @param options New validation options
     */
    void set_options(const waveform_ps_validation_options& options);

private:
    waveform_ps_validation_options options_;

    void validate_patient_module(
        const core::dicom_dataset& dataset,
        validation_result& result) const;

    void validate_general_study_module(
        const core::dicom_dataset& dataset,
        validation_result& result) const;

    void validate_general_series_module(
        const core::dicom_dataset& dataset,
        validation_result& result) const;

    void validate_general_equipment_module(
        const core::dicom_dataset& dataset,
        validation_result& result) const;

    void validate_sop_common_module(
        const core::dicom_dataset& dataset,
        validation_result& result) const;

    void validate_waveform_ps_module(
        const core::dicom_dataset& dataset,
        validation_result& result) const;

    void validate_waveform_annotation_module(
        const core::dicom_dataset& dataset,
        validation_result& result) const;

    void check_type1_attribute(
        const core::dicom_dataset& dataset,
        const core::dicom_tag& tag,
        const std::string& description,
        validation_result& result) const;

    void check_type2_attribute(
        const core::dicom_dataset& dataset,
        const core::dicom_tag& tag,
        const std::string& description,
        validation_result& result) const;
};

}  // namespace kcenon::pacs::services::validation

#endif  // PACS_SERVICES_VALIDATION_WAVEFORM_PS_IOD_VALIDATOR_HPP
