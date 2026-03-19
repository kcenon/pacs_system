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
 * @file ct_iod_validator.hpp
 * @brief CT Image IOD Validator
 *
 * Provides validation for CT Image Information Object Definitions
 * as specified in DICOM PS3.3 Section A.3 (CT Image IOD).
 *
 * CT images include Frame of Reference for spatial registration and
 * CT-specific acquisition parameters (KVP, SliceThickness, etc.).
 *
 * @see DICOM PS3.3 Section A.3 - CT Image IOD
 * @see DICOM PS3.3 Table A.3-1 - CT Image IOD Modules
 * @see Issue #717 - Add CT Image IOD Validator
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_SERVICES_VALIDATION_CT_IOD_VALIDATOR_HPP
#define PACS_SERVICES_VALIDATION_CT_IOD_VALIDATOR_HPP

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_tag.hpp"
#include "pacs/services/validation/us_iod_validator.hpp"  // Reuse validation types

#include <string>
#include <vector>

namespace kcenon::pacs::services::validation {

// =============================================================================
// CT-Specific DICOM Tags (not in core tag constants)
// =============================================================================

namespace ct_tags {

/// KVP (0018,0060) - Peak kilo voltage output of the X-Ray generator
inline constexpr core::dicom_tag kvp{0x0018, 0x0060};

/// Slice Thickness (0018,0050) - Nominal slice thickness in mm
inline constexpr core::dicom_tag slice_thickness{0x0018, 0x0050};

/// Gantry/Detector Tilt (0018,1120) - Gantry tilt in degrees
inline constexpr core::dicom_tag gantry_detector_tilt{0x0018, 0x1120};

/// Table Height (0018,1130) - Table height in mm
inline constexpr core::dicom_tag table_height{0x0018, 0x1130};

/// Rotation Direction (0018,1140) - Direction of rotation (CW/CC)
inline constexpr core::dicom_tag rotation_direction{0x0018, 0x1140};

/// Exposure Time (0018,1150) - Duration of X-Ray exposure in ms
inline constexpr core::dicom_tag exposure_time{0x0018, 0x1150};

/// X-Ray Tube Current (0018,1151) - in mA
inline constexpr core::dicom_tag xray_tube_current{0x0018, 0x1151};

/// Exposure (0018,1152) - in mAs (milliampere seconds)
inline constexpr core::dicom_tag exposure{0x0018, 0x1152};

/// Filter Type (0018,1160) - Type of filter inserted into X-Ray beam
inline constexpr core::dicom_tag filter_type{0x0018, 0x1160};

/// Generator Power (0018,1170) - Power in kW applied to generator
inline constexpr core::dicom_tag generator_power{0x0018, 0x1170};

/// Focal Spots (0018,1190) - Size of the focal spot in mm
inline constexpr core::dicom_tag focal_spots{0x0018, 0x1190};

/// Convolution Kernel (0018,1210) - Reconstruction algorithm
inline constexpr core::dicom_tag convolution_kernel{0x0018, 0x1210};

/// Reconstruction Diameter (0018,1100) - Diameter of reconstruction in mm
inline constexpr core::dicom_tag reconstruction_diameter{0x0018, 0x1100};

/// Data Collection Diameter (0018,0090) - in mm
inline constexpr core::dicom_tag data_collection_diameter{0x0018, 0x0090};

/// Patient Position (0018,5100)
inline constexpr core::dicom_tag patient_position{0x0018, 0x5100};

/// Image Position (Patient) (0020,0032)
inline constexpr core::dicom_tag image_position_patient{0x0020, 0x0032};

/// Image Orientation (Patient) (0020,0037)
inline constexpr core::dicom_tag image_orientation_patient{0x0020, 0x0037};

/// Slice Location (0020,1041)
inline constexpr core::dicom_tag slice_location{0x0020, 0x1041};

}  // namespace ct_tags

// =============================================================================
// CT Validation Options
// =============================================================================

/**
 * @brief Options for CT IOD validation
 */
struct ct_validation_options {
    /// Check Type 1 (required) attributes
    bool check_type1 = true;

    /// Check Type 2 (required, can be empty) attributes
    bool check_type2 = true;

    /// Check Type 1C/2C (conditionally required) attributes
    bool check_conditional = true;

    /// Validate pixel data consistency (rows, columns, bits)
    bool validate_pixel_data = true;

    /// Validate CT-specific acquisition parameters
    bool validate_ct_params = true;

    /// Strict mode - treat warnings as errors
    bool strict_mode = false;
};

// =============================================================================
// CT IOD Validator
// =============================================================================

/**
 * @brief Validator for CT Image IODs
 *
 * Validates DICOM datasets against the CT Image IOD specification
 * (PS3.3 Section A.3).
 *
 * ## Validated Modules (PS3.3 Table A.3-1)
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
 * @example
 * @code
 * ct_iod_validator validator;
 * auto result = validator.validate(dataset);
 *
 * if (!result.is_valid) {
 *     for (const auto& finding : result.findings) {
 *         std::cerr << finding.message << "\n";
 *     }
 * }
 * @endcode
 */
class ct_iod_validator {
public:
    /**
     * @brief Construct validator with default options
     */
    ct_iod_validator() = default;

    /**
     * @brief Construct validator with custom options
     * @param options Validation options
     */
    explicit ct_iod_validator(const ct_validation_options& options);

    /**
     * @brief Validate a DICOM dataset against CT IOD
     * @param dataset The dataset to validate
     * @return Validation result with all findings
     */
    [[nodiscard]] validation_result validate(
        const core::dicom_dataset& dataset) const;

    /**
     * @brief Quick check if dataset has minimum required CT attributes
     * @param dataset The dataset to check
     * @return true if all Type 1 attributes are present
     */
    [[nodiscard]] bool quick_check(
        const core::dicom_dataset& dataset) const;

    /**
     * @brief Get the validation options
     */
    [[nodiscard]] const ct_validation_options& options() const noexcept;

    /**
     * @brief Set validation options
     */
    void set_options(const ct_validation_options& options);

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

    ct_validation_options options_;
};

// =============================================================================
// Convenience Functions
// =============================================================================

/**
 * @brief Validate a CT dataset with default options
 * @param dataset The dataset to validate
 * @return Validation result
 */
[[nodiscard]] validation_result validate_ct_iod(
    const core::dicom_dataset& dataset);

/**
 * @brief Quick check if a dataset is a valid CT image
 * @param dataset The dataset to check
 * @return true if the dataset passes basic CT IOD validation
 */
[[nodiscard]] bool is_valid_ct_dataset(const core::dicom_dataset& dataset);

}  // namespace kcenon::pacs::services::validation

#endif  // PACS_SERVICES_VALIDATION_CT_IOD_VALIDATOR_HPP
