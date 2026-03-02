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
 * @file wsi_iod_validator.hpp
 * @brief VL Whole Slide Microscopy Image IOD Validator
 *
 * Provides validation for VL Whole Slide Microscopy Image Information
 * Object Definitions as specified in DICOM PS3.3 Section A.32.8.
 *
 * WSI images are gigapixel-scale, tiled multi-frame images used in
 * digital pathology. They have unique structural requirements including
 * tile dimensions, optical paths, and specimen identification.
 *
 * @see DICOM PS3.3 Section A.32.8 - VL Whole Slide Microscopy Image IOD
 * @see DICOM PS3.3 Section C.8.12.4 - Whole Slide Microscopy Image Module
 * @see Issue #826 - Add WSI IOD validator
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_SERVICES_VALIDATION_WSI_IOD_VALIDATOR_HPP
#define PACS_SERVICES_VALIDATION_WSI_IOD_VALIDATOR_HPP

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_tag.hpp"
#include "pacs/services/validation/us_iod_validator.hpp"  // Reuse validation types

#include <string>
#include <vector>

namespace pacs::services::validation {

// =============================================================================
// WSI-Specific DICOM Tags for Validation
// =============================================================================

namespace wsi_iod_tags {

/// Total Pixel Matrix Columns (0048,0006)
inline constexpr core::dicom_tag total_pixel_matrix_columns{0x0048, 0x0006};

/// Total Pixel Matrix Rows (0048,0007)
inline constexpr core::dicom_tag total_pixel_matrix_rows{0x0048, 0x0007};

/// Total Pixel Matrix Focal Planes (0048,0008)
inline constexpr core::dicom_tag total_pixel_matrix_focal_planes{0x0048, 0x0008};

/// Image Orientation (Slide) (0048,0102)
inline constexpr core::dicom_tag image_orientation_slide{0x0048, 0x0102};

/// Dimension Organization Type (0020,9311)
inline constexpr core::dicom_tag dimension_organization_type{0x0020, 0x9311};

/// Number of Frames (0028,0008)
inline constexpr core::dicom_tag number_of_frames{0x0028, 0x0008};

/// Imaged Volume Width (0048,0001) — physical width in mm
inline constexpr core::dicom_tag imaged_volume_width{0x0048, 0x0001};

/// Imaged Volume Height (0048,0002) — physical height in mm
inline constexpr core::dicom_tag imaged_volume_height{0x0048, 0x0002};

/// Optical Path Identifier (0048,0105)
inline constexpr core::dicom_tag optical_path_identifier{0x0048, 0x0105};

/// Optical Path Description (0048,0106)
inline constexpr core::dicom_tag optical_path_description{0x0048, 0x0106};

/// Specimen Identifier (0040,0551)
inline constexpr core::dicom_tag specimen_identifier{0x0040, 0x0551};

/// Container Identifier (0040,0512)
inline constexpr core::dicom_tag container_identifier{0x0040, 0x0512};

}  // namespace wsi_iod_tags

// =============================================================================
// WSI Validation Options
// =============================================================================

/**
 * @brief Options for WSI IOD validation
 */
struct wsi_validation_options {
    /// Check Type 1 (required) attributes
    bool check_type1 = true;

    /// Check Type 2 (required, can be empty) attributes
    bool check_type2 = true;

    /// Check Type 1C/2C (conditionally required) attributes
    bool check_conditional = true;

    /// Validate pixel data consistency (rows, columns, bits)
    bool validate_pixel_data = true;

    /// Validate WSI-specific image parameters
    bool validate_wsi_params = true;

    /// Validate optical path information
    bool validate_optical_path = true;

    /// Strict mode - treat warnings as errors
    bool strict_mode = false;
};

// =============================================================================
// WSI IOD Validator
// =============================================================================

/**
 * @brief Validator for VL Whole Slide Microscopy Image IODs
 *
 * Validates DICOM datasets against the VL Whole Slide Microscopy Image
 * IOD specification (PS3.3 Section A.32.8).
 *
 * ## Validated Modules (PS3.3 Table A.32.8-1)
 *
 * ### Mandatory Modules
 * - Patient Module (M)
 * - General Study Module (M)
 * - General Series Module (M) — Modality = "SM"
 * - Whole Slide Microscopy Image Module (M)
 * - Optical Path Module (M)
 * - Multi-frame Dimension Module (M)
 * - Image Pixel Module (M)
 * - SOP Common Module (M)
 *
 * ### User Optional Modules
 * - Specimen Module (U) — checked at info level
 *
 * @example
 * @code
 * wsi_iod_validator validator;
 * auto result = validator.validate(dataset);
 *
 * if (!result.is_valid) {
 *     for (const auto& finding : result.findings) {
 *         std::cerr << finding.message << "\n";
 *     }
 * }
 * @endcode
 */
class wsi_iod_validator {
public:
    /**
     * @brief Construct validator with default options
     */
    wsi_iod_validator() = default;

    /**
     * @brief Construct validator with custom options
     * @param options Validation options
     */
    explicit wsi_iod_validator(const wsi_validation_options& options);

    /**
     * @brief Validate a DICOM dataset against WSI IOD
     * @param dataset The dataset to validate
     * @return Validation result with all findings
     */
    [[nodiscard]] validation_result validate(
        const core::dicom_dataset& dataset) const;

    /**
     * @brief Quick check if dataset has minimum required WSI attributes
     * @param dataset The dataset to check
     * @return true if all Type 1 attributes are present
     */
    [[nodiscard]] bool quick_check(
        const core::dicom_dataset& dataset) const;

    /**
     * @brief Get the validation options
     */
    [[nodiscard]] const wsi_validation_options& options() const noexcept;

    /**
     * @brief Set validation options
     */
    void set_options(const wsi_validation_options& options);

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

    void validate_wsi_image_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_optical_path_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_multiframe_dimension_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_image_pixel_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_specimen_module(
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

    wsi_validation_options options_;
};

// =============================================================================
// Convenience Functions
// =============================================================================

/**
 * @brief Validate a WSI dataset with default options
 * @param dataset The dataset to validate
 * @return Validation result
 */
[[nodiscard]] validation_result validate_wsi_iod(
    const core::dicom_dataset& dataset);

/**
 * @brief Quick check if a dataset is a valid WSI image
 * @param dataset The dataset to check
 * @return true if the dataset passes basic WSI IOD validation
 */
[[nodiscard]] bool is_valid_wsi_dataset(const core::dicom_dataset& dataset);

}  // namespace pacs::services::validation

#endif  // PACS_SERVICES_VALIDATION_WSI_IOD_VALIDATOR_HPP
