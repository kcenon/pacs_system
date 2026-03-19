// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @file ophthalmic_iod_validator.hpp
 * @brief Ophthalmic Photography and Tomography Image IOD Validator
 *
 * Provides validation for Ophthalmic Photography (8-bit, 16-bit, wide-field)
 * and Optical Coherence Tomography Image Information Object Definitions
 * as specified in DICOM PS3.3 Sections A.39 and A.40.
 *
 * Ophthalmic images include fundus photography, fluorescein angiography,
 * ICG angiography, and OCT B-scans. They require laterality tracking and
 * support modality-specific photometric interpretations.
 *
 * @see DICOM PS3.3 Section A.39 - Ophthalmic Photography IODs
 * @see DICOM PS3.3 Section A.40 - Ophthalmic Tomography Image IOD
 * @see Issue #830 - Add Ophthalmic IOD validator
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_SERVICES_VALIDATION_OPHTHALMIC_IOD_VALIDATOR_HPP
#define PACS_SERVICES_VALIDATION_OPHTHALMIC_IOD_VALIDATOR_HPP

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_tag.hpp"
#include "pacs/services/validation/us_iod_validator.hpp"  // Reuse validation types

#include <string>
#include <vector>

namespace kcenon::pacs::services::validation {

// =============================================================================
// Ophthalmic-Specific DICOM Tags for Validation
// =============================================================================

namespace ophthalmic_iod_tags {

/// Image Laterality (0020,0062) — R, L, or B
inline constexpr core::dicom_tag image_laterality{0x0020, 0x0062};

/// Anatomic Region Sequence (0008,2218)
inline constexpr core::dicom_tag anatomic_region_sequence{0x0008, 0x2218};

/// Acquisition Device Type Code Sequence (0022,0015)
inline constexpr core::dicom_tag acquisition_device_type_code_sequence{0x0022,
                                                                       0x0015};

/// Pupil Dilated (0022,000D)
inline constexpr core::dicom_tag pupil_dilated{0x0022, 0x000D};

/// Axial Length of the Eye (0022,0030)
inline constexpr core::dicom_tag axial_length_of_eye{0x0022, 0x0030};

/// Horizontal Field of View (0022,000C)
inline constexpr core::dicom_tag horizontal_field_of_view{0x0022, 0x000C};

/// Detector Type (0018,7004)
inline constexpr core::dicom_tag detector_type{0x0018, 0x7004};

/// Number of Frames (0028,0008)
inline constexpr core::dicom_tag number_of_frames{0x0028, 0x0008};

/// Acquisition Context Sequence (0040,0555)
inline constexpr core::dicom_tag acquisition_context_sequence{0x0040, 0x0555};

}  // namespace ophthalmic_iod_tags

// =============================================================================
// Ophthalmic Validation Options
// =============================================================================

/**
 * @brief Options for Ophthalmic IOD validation
 */
struct ophthalmic_validation_options {
    /// Check Type 1 (required) attributes
    bool check_type1 = true;

    /// Check Type 2 (required, can be empty) attributes
    bool check_type2 = true;

    /// Check Type 1C/2C (conditionally required) attributes
    bool check_conditional = true;

    /// Validate pixel data consistency (rows, columns, bits)
    bool validate_pixel_data = true;

    /// Validate ophthalmic-specific image parameters
    bool validate_ophthalmic_params = true;

    /// Validate laterality (R/L/B)
    bool validate_laterality = true;

    /// Strict mode - treat warnings as errors
    bool strict_mode = false;
};

// =============================================================================
// Ophthalmic IOD Validator
// =============================================================================

/**
 * @brief Validator for Ophthalmic Photography and Tomography Image IODs
 *
 * Validates DICOM datasets against the Ophthalmic Photography and
 * Tomography Image IOD specifications (PS3.3 Sections A.39/A.40).
 *
 * ## Validated Modules
 *
 * ### Mandatory Modules
 * - Patient Module (M)
 * - General Study Module (M)
 * - General Series Module (M) — Modality = "OP" or "OPT"
 * - Ophthalmic Photography Image Module (M)
 * - Image Pixel Module (M)
 * - SOP Common Module (M)
 * - Acquisition Context Module (M)
 *
 * ### Conditional Modules
 * - Multi-frame Module (C) — for OCT images
 *
 * @example
 * @code
 * ophthalmic_iod_validator validator;
 * auto result = validator.validate(dataset);
 *
 * if (!result.is_valid) {
 *     for (const auto& finding : result.findings) {
 *         std::cerr << finding.message << "\n";
 *     }
 * }
 * @endcode
 */
class ophthalmic_iod_validator {
public:
    /**
     * @brief Construct validator with default options
     */
    ophthalmic_iod_validator() = default;

    /**
     * @brief Construct validator with custom options
     * @param options Validation options
     */
    explicit ophthalmic_iod_validator(
        const ophthalmic_validation_options& options);

    /**
     * @brief Validate a DICOM dataset against Ophthalmic IOD
     * @param dataset The dataset to validate
     * @return Validation result with all findings
     */
    [[nodiscard]] validation_result validate(
        const core::dicom_dataset& dataset) const;

    /**
     * @brief Quick check if dataset has minimum required ophthalmic attributes
     * @param dataset The dataset to check
     * @return true if all Type 1 attributes are present
     */
    [[nodiscard]] bool quick_check(
        const core::dicom_dataset& dataset) const;

    /**
     * @brief Get the validation options
     */
    [[nodiscard]] const ophthalmic_validation_options& options() const noexcept;

    /**
     * @brief Set validation options
     */
    void set_options(const ophthalmic_validation_options& options);

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

    void validate_ophthalmic_image_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_image_pixel_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_multiframe_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_acquisition_context_module(
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

    void check_laterality(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void check_pixel_data_consistency(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    ophthalmic_validation_options options_;
};

// =============================================================================
// Convenience Functions
// =============================================================================

/**
 * @brief Validate an ophthalmic dataset with default options
 * @param dataset The dataset to validate
 * @return Validation result
 */
[[nodiscard]] validation_result validate_ophthalmic_iod(
    const core::dicom_dataset& dataset);

/**
 * @brief Quick check if a dataset is a valid ophthalmic image
 * @param dataset The dataset to check
 * @return true if the dataset passes basic ophthalmic IOD validation
 */
[[nodiscard]] bool is_valid_ophthalmic_dataset(
    const core::dicom_dataset& dataset);

}  // namespace kcenon::pacs::services::validation

#endif  // PACS_SERVICES_VALIDATION_OPHTHALMIC_IOD_VALIDATOR_HPP
