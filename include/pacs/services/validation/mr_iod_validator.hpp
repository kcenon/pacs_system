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
 * @file mr_iod_validator.hpp
 * @brief MR Image IOD Validator
 *
 * Provides validation for MR Image Information Object Definitions
 * as specified in DICOM PS3.3 Section A.4 (MR Image IOD).
 *
 * MR images include Frame of Reference for spatial registration and
 * MR-specific acquisition parameters (MagneticFieldStrength, EchoTime,
 * RepetitionTime, ScanningSequence, etc.).
 *
 * @see DICOM PS3.3 Section A.4 - MR Image IOD
 * @see DICOM PS3.3 Table A.4-1 - MR Image IOD Modules
 * @see Issue #718 - Add MR Image IOD Validator
 */

#ifndef PACS_SERVICES_VALIDATION_MR_IOD_VALIDATOR_HPP
#define PACS_SERVICES_VALIDATION_MR_IOD_VALIDATOR_HPP

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_tag.hpp"
#include "pacs/services/validation/us_iod_validator.hpp"  // Reuse validation types

#include <string>
#include <vector>

namespace pacs::services::validation {

// =============================================================================
// MR-Specific DICOM Tags (not in core tag constants)
// =============================================================================

namespace mr_tags {

/// Scanning Sequence (0018,0020) - Type 1
/// Identifies the type of MR data acquisition sequence: SE, IR, GR, EP, RM
inline constexpr core::dicom_tag scanning_sequence{0x0018, 0x0020};

/// Sequence Variant (0018,0021) - Type 1
/// Variant of the scanning sequence: SK, MTC, SS, TRSS, SP, MP, OSP, NONE
inline constexpr core::dicom_tag sequence_variant{0x0018, 0x0021};

/// Scan Options (0018,0022) - Type 2
/// Additional scan parameters: PER, RG, CG, PPG, FC, PFF, PFP, SP, FS
inline constexpr core::dicom_tag scan_options{0x0018, 0x0022};

/// MR Acquisition Type (0018,0023) - Type 2
/// Identifies the spatial data encoding scheme: 2D or 3D
inline constexpr core::dicom_tag mr_acquisition_type{0x0018, 0x0023};

/// Repetition Time (0018,0080) - Type 2C
/// Time in ms between successive pulse sequences
inline constexpr core::dicom_tag repetition_time{0x0018, 0x0080};

/// Echo Time (0018,0081) - Type 2
/// Time in ms between the middle of the excitation pulse and peak of echo
inline constexpr core::dicom_tag echo_time{0x0018, 0x0081};

/// Echo Train Length (0018,0091) - Type 2
/// Number of echoes in a multi-echo acquisition
inline constexpr core::dicom_tag echo_train_length{0x0018, 0x0091};

/// Inversion Time (0018,0082) - Type 2C
/// Time in ms between the inversion and excitation pulses
inline constexpr core::dicom_tag inversion_time{0x0018, 0x0082};

/// Magnetic Field Strength (0018,0087) - Type 2
/// Nominal field strength of MR magnet in Tesla
inline constexpr core::dicom_tag magnetic_field_strength{0x0018, 0x0087};

/// Spacing Between Slices (0018,0088) - Type 2
/// Distance between adjacent slices in mm
inline constexpr core::dicom_tag spacing_between_slices{0x0018, 0x0088};

/// Number of Phase Encoding Steps (0018,0089) - Type 2
inline constexpr core::dicom_tag number_of_phase_encoding_steps{0x0018, 0x0089};

/// Percent Sampling (0018,0093) - Type 2
inline constexpr core::dicom_tag percent_sampling{0x0018, 0x0093};

/// Percent Phase Field of View (0018,0094) - Type 2
inline constexpr core::dicom_tag percent_phase_fov{0x0018, 0x0094};

/// Pixel Bandwidth (0018,0095) - Type 2
inline constexpr core::dicom_tag pixel_bandwidth{0x0018, 0x0095};

/// Flip Angle (0018,1314) - Type 2
/// Steady state angle in degrees
inline constexpr core::dicom_tag flip_angle{0x0018, 0x1314};

/// SAR (Specific Absorption Rate) (0018,1316) - Type 2
inline constexpr core::dicom_tag sar{0x0018, 0x1316};

/// Slice Thickness (0018,0050) - Type 2
inline constexpr core::dicom_tag slice_thickness{0x0018, 0x0050};

/// Image Position (Patient) (0020,0032)
inline constexpr core::dicom_tag image_position_patient{0x0020, 0x0032};

/// Image Orientation (Patient) (0020,0037)
inline constexpr core::dicom_tag image_orientation_patient{0x0020, 0x0037};

}  // namespace mr_tags

// =============================================================================
// MR Validation Options
// =============================================================================

/**
 * @brief Options for MR IOD validation
 */
struct mr_validation_options {
    /// Check Type 1 (required) attributes
    bool check_type1 = true;

    /// Check Type 2 (required, can be empty) attributes
    bool check_type2 = true;

    /// Check Type 1C/2C (conditionally required) attributes
    bool check_conditional = true;

    /// Validate pixel data consistency (rows, columns, bits)
    bool validate_pixel_data = true;

    /// Validate MR-specific acquisition parameters
    bool validate_mr_params = true;

    /// Strict mode - treat warnings as errors
    bool strict_mode = false;
};

// =============================================================================
// MR IOD Validator
// =============================================================================

/**
 * @brief Validator for MR Image IODs
 *
 * Validates DICOM datasets against the MR Image IOD specification
 * (PS3.3 Section A.4).
 *
 * ## Validated Modules (PS3.3 Table A.4-1)
 *
 * ### Mandatory Modules
 * - Patient Module (M)
 * - General Study Module (M)
 * - General Series Module (M)
 * - Frame of Reference Module (M)
 * - General Equipment Module (M)
 * - General Image Module (M)
 * - Image Pixel Module (M)
 * - MR Image Module (M)
 * - SOP Common Module (M)
 *
 * @example
 * @code
 * mr_iod_validator validator;
 * auto result = validator.validate(dataset);
 *
 * if (!result.is_valid) {
 *     for (const auto& finding : result.findings) {
 *         std::cerr << finding.message << "\n";
 *     }
 * }
 * @endcode
 */
class mr_iod_validator {
public:
    /**
     * @brief Construct validator with default options
     */
    mr_iod_validator() = default;

    /**
     * @brief Construct validator with custom options
     * @param options Validation options
     */
    explicit mr_iod_validator(const mr_validation_options& options);

    /**
     * @brief Validate a DICOM dataset against MR IOD
     * @param dataset The dataset to validate
     * @return Validation result with all findings
     */
    [[nodiscard]] validation_result validate(
        const core::dicom_dataset& dataset) const;

    /**
     * @brief Quick check if dataset has minimum required MR attributes
     * @param dataset The dataset to check
     * @return true if all Type 1 attributes are present
     */
    [[nodiscard]] bool quick_check(
        const core::dicom_dataset& dataset) const;

    /**
     * @brief Get the validation options
     */
    [[nodiscard]] const mr_validation_options& options() const noexcept;

    /**
     * @brief Set validation options
     */
    void set_options(const mr_validation_options& options);

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

    void validate_mr_image_module(
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

    mr_validation_options options_;
};

// =============================================================================
// Convenience Functions
// =============================================================================

/**
 * @brief Validate an MR dataset with default options
 * @param dataset The dataset to validate
 * @return Validation result
 */
[[nodiscard]] validation_result validate_mr_iod(
    const core::dicom_dataset& dataset);

/**
 * @brief Quick check if a dataset is a valid MR image
 * @param dataset The dataset to check
 * @return true if the dataset passes basic MR IOD validation
 */
[[nodiscard]] bool is_valid_mr_dataset(const core::dicom_dataset& dataset);

}  // namespace pacs::services::validation

#endif  // PACS_SERVICES_VALIDATION_MR_IOD_VALIDATOR_HPP
