/**
 * @file xa_iod_validator.hpp
 * @brief X-Ray Angiographic Image IOD Validator
 *
 * Provides validation for X-Ray Angiographic (XA) and X-Ray Radiofluoroscopic
 * (XRF) Image Information Object Definitions as specified in DICOM PS3.3
 * Section A.14 (XA Image IOD).
 *
 * XA images have specific requirements including:
 * - Grayscale-only photometric interpretations (MONOCHROME1/2)
 * - Positioner angle information for geometry reconstruction
 * - Calibration data for quantitative measurements (QCA)
 * - Multi-frame timing information
 *
 * @see DICOM PS3.3 Section A.14 - XA Image IOD
 * @see DICOM PS3.3 Section A.53 - Enhanced XA Image IOD
 * @see DES-SVC-009 - XA Storage Implementation
 */

#ifndef PACS_SERVICES_VALIDATION_XA_IOD_VALIDATOR_HPP
#define PACS_SERVICES_VALIDATION_XA_IOD_VALIDATOR_HPP

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_tag.hpp"
#include "pacs/services/validation/us_iod_validator.hpp"  // Reuse validation types

#include <optional>
#include <string>
#include <vector>

namespace pacs::services::validation {

// =============================================================================
// XA-Specific DICOM Tags (not in core tag constants)
// =============================================================================

namespace xa_tags {

/// Number of Frames (0028,0008) - for multi-frame
inline constexpr core::dicom_tag number_of_frames{0x0028, 0x0008};

/// Frame Time (0018,1063) - time between frames in ms
inline constexpr core::dicom_tag frame_time{0x0018, 0x1063};

/// Frame Time Vector (0018,1065) - variable frame timing
inline constexpr core::dicom_tag frame_time_vector{0x0018, 0x1065};

/// Cine Rate (0018,0040) - intended display rate
inline constexpr core::dicom_tag cine_rate{0x0018, 0x0040};

/// Recommended Display Frame Rate (0008,2144)
inline constexpr core::dicom_tag recommended_display_frame_rate{0x0008, 0x2144};

/// Field of View Shape (0018,1147)
inline constexpr core::dicom_tag field_of_view_shape{0x0018, 0x1147};

/// Field of View Dimensions (0018,1149)
inline constexpr core::dicom_tag field_of_view_dimensions{0x0018, 0x1149};

/// Positioner Motion (0018,1500)
inline constexpr core::dicom_tag positioner_motion{0x0018, 0x1500};

/// Positioner Primary Angle (0018,1510) - LAO/RAO
inline constexpr core::dicom_tag positioner_primary_angle{0x0018, 0x1510};

/// Positioner Secondary Angle (0018,1511) - Cranial/Caudal
inline constexpr core::dicom_tag positioner_secondary_angle{0x0018, 0x1511};

/// Positioner Primary Angle Increment (0018,1520) - for rotational
inline constexpr core::dicom_tag positioner_primary_angle_increment{0x0018, 0x1520};

/// Positioner Secondary Angle Increment (0018,1521) - for rotational
inline constexpr core::dicom_tag positioner_secondary_angle_increment{0x0018, 0x1521};

/// Imager Pixel Spacing (0018,1164) - at detector plane
inline constexpr core::dicom_tag imager_pixel_spacing{0x0018, 0x1164};

/// Distance Source to Detector (0018,1110) - SID
inline constexpr core::dicom_tag distance_source_to_detector{0x0018, 0x1110};

/// Distance Source to Patient (0018,1111) - SOD
inline constexpr core::dicom_tag distance_source_to_patient{0x0018, 0x1111};

/// Intensifier Size (0018,1162) - image intensifier diameter
inline constexpr core::dicom_tag intensifier_size{0x0018, 0x1162};

/// Grid (0018,1166) - anti-scatter grid presence
inline constexpr core::dicom_tag grid{0x0018, 0x1166};

/// KVP (0018,0060) - X-ray tube peak kilovoltage
inline constexpr core::dicom_tag kvp{0x0018, 0x0060};

/// Exposure Time (0018,1150) - in ms
inline constexpr core::dicom_tag exposure_time{0x0018, 0x1150};

/// X-Ray Tube Current (0018,1151) - in mA
inline constexpr core::dicom_tag xray_tube_current{0x0018, 0x1151};

}  // namespace xa_tags

// =============================================================================
// XA Validation Options
// =============================================================================

/**
 * @brief Options for XA IOD validation
 */
struct xa_validation_options {
    /// Check Type 1 (required) attributes
    bool check_type1 = true;

    /// Check Type 2 (required, can be empty) attributes
    bool check_type2 = true;

    /// Check Type 1C/2C (conditionally required) attributes
    bool check_conditional = true;

    /// Validate pixel data consistency (rows, columns, bits)
    bool validate_pixel_data = true;

    /// Validate positioner angle data
    bool validate_positioner = true;

    /// Validate calibration data for QCA
    bool validate_calibration = true;

    /// Validate multi-frame timing information
    bool validate_multiframe_timing = true;

    /// Strict mode - treat warnings as errors
    bool strict_mode = false;
};

// =============================================================================
// XA IOD Validator
// =============================================================================

/**
 * @brief Validator for X-Ray Angiographic Image IODs
 *
 * Validates DICOM datasets against the XA Image IOD and Enhanced XA Image
 * IOD specifications. Checks required modules, attributes, and value
 * constraints specific to angiographic imaging.
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
 * - XA/XRF Acquisition Module (M)
 * - XA/XRF Image Module (M)
 * - SOP Common Module (M)
 *
 * ### Conditional Modules
 * - Multi-frame Module (C) - for multi-frame XA
 * - Cine Module (C) - for multi-frame XA
 * - XA Calibration Module (C) - for quantitative analysis
 *
 * @example
 * @code
 * xa_iod_validator validator;
 * auto result = validator.validate(dataset);
 *
 * if (!result.is_valid) {
 *     for (const auto& finding : result.findings) {
 *         std::cerr << finding.message << "\n";
 *     }
 * }
 * @endcode
 */
class xa_iod_validator {
public:
    /**
     * @brief Construct validator with default options
     */
    xa_iod_validator() = default;

    /**
     * @brief Construct validator with custom options
     * @param options Validation options
     */
    explicit xa_iod_validator(const xa_validation_options& options);

    /**
     * @brief Validate a DICOM dataset against XA IOD
     *
     * @param dataset The dataset to validate
     * @return Validation result with all findings
     */
    [[nodiscard]] validation_result validate(const core::dicom_dataset& dataset) const;

    /**
     * @brief Validate a multi-frame XA dataset
     *
     * Performs additional validation for multi-frame specific attributes
     * including frame timing and per-frame calibration.
     *
     * @param dataset The dataset to validate
     * @return Validation result with all findings
     */
    [[nodiscard]] validation_result
    validate_multiframe(const core::dicom_dataset& dataset) const;

    /**
     * @brief Quick check if dataset has minimum required XA attributes
     *
     * Faster than full validation - only checks Type 1 attributes.
     *
     * @param dataset The dataset to check
     * @return true if all Type 1 attributes are present
     */
    [[nodiscard]] bool quick_check(const core::dicom_dataset& dataset) const;

    /**
     * @brief Validate calibration data for quantitative analysis
     *
     * Specifically checks if the dataset has valid calibration information
     * suitable for QCA (Quantitative Coronary Analysis) measurements.
     *
     * @param dataset The dataset to check
     * @return Validation result focused on calibration attributes
     */
    [[nodiscard]] validation_result
    validate_calibration(const core::dicom_dataset& dataset) const;

    /**
     * @brief Get the validation options
     */
    [[nodiscard]] const xa_validation_options& options() const noexcept;

    /**
     * @brief Set validation options
     */
    void set_options(const xa_validation_options& options);

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

    void validate_xa_acquisition_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_xa_image_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_image_pixel_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_multiframe_module(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void validate_calibration_module(
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

    void check_xa_photometric(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void check_pixel_data_consistency(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    void check_positioner_angles(
        const core::dicom_dataset& dataset,
        std::vector<validation_finding>& findings) const;

    xa_validation_options options_;
};

// =============================================================================
// Convenience Functions
// =============================================================================

/**
 * @brief Validate an XA dataset with default options
 *
 * @param dataset The dataset to validate
 * @return Validation result
 */
[[nodiscard]] validation_result validate_xa_iod(const core::dicom_dataset& dataset);

/**
 * @brief Quick check if a dataset is a valid XA image
 *
 * @param dataset The dataset to check
 * @return true if the dataset passes basic XA IOD validation
 */
[[nodiscard]] bool is_valid_xa_dataset(const core::dicom_dataset& dataset);

/**
 * @brief Check if dataset has valid QCA calibration data
 *
 * @param dataset The dataset to check
 * @return true if the dataset has complete calibration for QCA
 */
[[nodiscard]] bool has_qca_calibration(const core::dicom_dataset& dataset);

}  // namespace pacs::services::validation

#endif  // PACS_SERVICES_VALIDATION_XA_IOD_VALIDATOR_HPP
