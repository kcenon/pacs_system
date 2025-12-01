/**
 * @file xa_storage.hpp
 * @brief X-Ray Angiographic (XA) Image Storage SOP Classes
 *
 * This file provides SOP Class definitions and utilities for X-Ray Angiographic
 * (XA) and X-Ray Radiofluoroscopic (XRF) image storage. Supports standard,
 * enhanced, and 3D angiographic SOP Classes.
 *
 * XA imaging is essential for interventional procedures including:
 * - Cardiac catheterization (coronary angiography)
 * - Peripheral vascular interventions
 * - Neurointerventional procedures
 * - Electrophysiology studies
 * - Fluoroscopy-guided procedures
 *
 * @see DICOM PS3.4 Section B - Storage Service Class
 * @see DICOM PS3.3 Section A.14 - XA Image IOD
 * @see DICOM PS3.3 Section A.53 - Enhanced XA Image IOD
 * @see DES-SVC-009 - XA Storage Implementation
 */

#ifndef PACS_SERVICES_SOP_CLASSES_XA_STORAGE_HPP
#define PACS_SERVICES_SOP_CLASSES_XA_STORAGE_HPP

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace pacs::services::sop_classes {

// =============================================================================
// X-Ray Angiographic Storage SOP Class UIDs
// =============================================================================

/// @name Primary XA/XRF SOP Classes
/// @{

/// XA Image Storage SOP Class UID (single/multi-frame)
inline constexpr std::string_view xa_image_storage_uid =
    "1.2.840.10008.5.1.4.1.1.12.1";

/// Enhanced XA Image Storage SOP Class UID (enhanced IOD)
inline constexpr std::string_view enhanced_xa_image_storage_uid =
    "1.2.840.10008.5.1.4.1.1.12.1.1";

/// XRF Image Storage SOP Class UID (X-Ray Radiofluoroscopic)
inline constexpr std::string_view xrf_image_storage_uid =
    "1.2.840.10008.5.1.4.1.1.12.2";

/// @}

/// @name 3D Angiographic SOP Classes
/// @{

/// X-Ray 3D Angiographic Image Storage SOP Class UID (3D rotational)
inline constexpr std::string_view xray_3d_angiographic_image_storage_uid =
    "1.2.840.10008.5.1.4.1.1.13.1.1";

/// X-Ray 3D Craniofacial Image Storage SOP Class UID
inline constexpr std::string_view xray_3d_craniofacial_image_storage_uid =
    "1.2.840.10008.5.1.4.1.1.13.1.2";

/// @}

// =============================================================================
// XA-Specific Transfer Syntaxes
// =============================================================================

/**
 * @brief Get recommended transfer syntaxes for XA images
 *
 * Returns a prioritized list of transfer syntaxes suitable for
 * X-Ray angiographic image storage. XA images are typically
 * grayscale with high bit depth (8-16 bits).
 *
 * @return Vector of transfer syntax UIDs in priority order
 */
[[nodiscard]] std::vector<std::string> get_xa_transfer_syntaxes();

// =============================================================================
// XA Photometric Interpretations
// =============================================================================

/**
 * @brief Supported photometric interpretations for XA/XRF images
 *
 * XA images are grayscale-only. MONOCHROME1 is traditional for XA
 * (minimum pixel = white, like a lightbox view), while MONOCHROME2
 * is also supported.
 */
enum class xa_photometric_interpretation {
    monochrome1,    ///< Minimum pixel = white (traditional X-ray view)
    monochrome2     ///< Minimum pixel = black
};

/**
 * @brief Convert photometric interpretation enum to DICOM string
 * @param interp The photometric interpretation
 * @return DICOM-compliant string representation
 */
[[nodiscard]] std::string_view to_string(xa_photometric_interpretation interp) noexcept;

/**
 * @brief Parse DICOM photometric interpretation string for XA
 * @param value The DICOM string value
 * @return The corresponding enum value, or monochrome2 if unknown
 */
[[nodiscard]] xa_photometric_interpretation
parse_xa_photometric_interpretation(std::string_view value) noexcept;

/**
 * @brief Check if photometric interpretation is valid for XA
 * @param value The DICOM string value
 * @return true if this is a valid XA photometric interpretation
 */
[[nodiscard]] bool is_valid_xa_photometric(std::string_view value) noexcept;

// =============================================================================
// XA SOP Class Information
// =============================================================================

/**
 * @brief Information about an XA/XRF Storage SOP Class
 */
struct xa_sop_class_info {
    std::string_view uid;           ///< SOP Class UID
    std::string_view name;          ///< Human-readable name
    std::string_view description;   ///< Brief description
    bool is_enhanced;               ///< Whether this is an enhanced IOD
    bool is_3d;                     ///< Whether this is a 3D acquisition
    bool supports_multiframe;       ///< Whether multi-frame is supported
};

/**
 * @brief Get all XA/XRF Storage SOP Class UIDs
 *
 * Returns all supported X-Ray angiographic SOP Class UIDs including
 * standard, enhanced, and 3D variants.
 *
 * @param include_3d Include 3D angiographic SOP classes (default: true)
 * @return Vector of XA Storage SOP Class UIDs
 */
[[nodiscard]] std::vector<std::string>
get_xa_storage_sop_classes(bool include_3d = true);

/**
 * @brief Get information about a specific XA SOP Class
 *
 * @param uid The SOP Class UID to look up
 * @return Pointer to sop class info, or nullptr if not an XA storage class
 */
[[nodiscard]] const xa_sop_class_info*
get_xa_sop_class_info(std::string_view uid) noexcept;

/**
 * @brief Check if a SOP Class UID is an XA/XRF Storage SOP Class
 *
 * @param uid The SOP Class UID to check
 * @return true if this is any XA/XRF storage SOP class
 */
[[nodiscard]] bool is_xa_storage_sop_class(std::string_view uid) noexcept;

/**
 * @brief Check if a SOP Class UID is a multi-frame XA Storage SOP Class
 *
 * @param uid The SOP Class UID to check
 * @return true if this is a multi-frame XA storage SOP class
 */
[[nodiscard]] bool is_xa_multiframe_sop_class(std::string_view uid) noexcept;

/**
 * @brief Check if a SOP Class UID is an enhanced XA SOP Class
 *
 * @param uid The SOP Class UID to check
 * @return true if this is an enhanced XA SOP class
 */
[[nodiscard]] bool is_enhanced_xa_sop_class(std::string_view uid) noexcept;

/**
 * @brief Check if a SOP Class UID is a 3D XA SOP Class
 *
 * @param uid The SOP Class UID to check
 * @return true if this is a 3D XA storage SOP class
 */
[[nodiscard]] bool is_xa_3d_sop_class(std::string_view uid) noexcept;

// =============================================================================
// XA Positioner Information
// =============================================================================

/**
 * @brief Positioner motion type
 *
 * Describes the type of positioner movement during acquisition.
 */
enum class xa_positioner_motion {
    stationary,     ///< No movement during acquisition
    dynamic         ///< Positioner moves during acquisition (e.g., rotational)
};

/**
 * @brief Convert positioner motion enum to DICOM string
 * @param motion The positioner motion type
 * @return DICOM-compliant string representation
 */
[[nodiscard]] std::string_view to_string(xa_positioner_motion motion) noexcept;

/**
 * @brief Positioner angle information
 *
 * Stores the primary and secondary angles of the X-ray positioner,
 * which are essential for proper reconstruction and QCA analysis.
 */
struct xa_positioner_angles {
    double primary_angle;       ///< LAO/RAO angle in degrees
    double secondary_angle;     ///< Cranial/Caudal angle in degrees

    /**
     * @brief Check if angles are within valid range
     * @return true if both angles are within typical clinical range
     */
    [[nodiscard]] bool is_valid() const noexcept;
};

// =============================================================================
// XA Frame Information
// =============================================================================

/**
 * @brief Common XA frame rates in frames per second
 *
 * XA acquisitions typically use specific standardized frame rates
 * for cardiac and vascular imaging.
 */
enum class xa_frame_rate : uint16_t {
    fps_7_5 = 8,      ///< 7.5 fps (low dose)
    fps_15 = 15,      ///< 15 fps (standard)
    fps_30 = 30       ///< 30 fps (high temporal resolution)
};

/**
 * @brief Get typical cine rate for cardiac XA
 * @return Default cine rate in frames per second
 */
[[nodiscard]] constexpr uint16_t get_default_xa_cine_rate() noexcept {
    return 15;
}

/**
 * @brief Get maximum recommended frame count for XA acquisitions
 *
 * Large XA acquisitions can have many frames. This returns a
 * reasonable upper limit for memory pre-allocation.
 *
 * @return Maximum expected frame count
 */
[[nodiscard]] constexpr size_t get_max_xa_frame_count() noexcept {
    return 2000;  // Typical max for long cardiac runs
}

// =============================================================================
// XA Calibration Information
// =============================================================================

/**
 * @brief XA calibration data for quantitative analysis
 *
 * Contains pixel spacing and geometry calibration data essential
 * for Quantitative Coronary Analysis (QCA) and other measurements.
 */
struct xa_calibration_data {
    double imager_pixel_spacing[2];     ///< Pixel spacing at detector (mm)
    double distance_source_to_detector; ///< SID in mm
    double distance_source_to_patient;  ///< SOD in mm

    /**
     * @brief Calculate magnification factor
     * @return SID/SOD ratio, or 0 if distances are invalid
     */
    [[nodiscard]] double magnification_factor() const noexcept;

    /**
     * @brief Calculate pixel spacing at isocenter
     * @return Calibrated pixel spacing at isocenter in mm
     */
    [[nodiscard]] double isocenter_pixel_spacing() const noexcept;

    /**
     * @brief Check if calibration data is valid for measurements
     * @return true if all required values are present and valid
     */
    [[nodiscard]] bool is_valid() const noexcept;
};

}  // namespace pacs::services::sop_classes

#endif  // PACS_SERVICES_SOP_CLASSES_XA_STORAGE_HPP
