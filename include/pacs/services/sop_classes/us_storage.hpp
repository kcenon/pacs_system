/**
 * @file us_storage.hpp
 * @brief Ultrasound Image Storage SOP Classes
 *
 * This file provides SOP Class definitions and utilities for Ultrasound (US)
 * image storage. Supports both current and retired SOP Classes for legacy
 * compatibility.
 *
 * @see DICOM PS3.4 Section B - Storage Service Class
 * @see DICOM PS3.3 Section A.6 - US Image IOD
 * @see DES-SVC-008 - Ultrasound Storage Implementation
 */

#ifndef PACS_SERVICES_SOP_CLASSES_US_STORAGE_HPP
#define PACS_SERVICES_SOP_CLASSES_US_STORAGE_HPP

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace pacs::services::sop_classes {

// =============================================================================
// Ultrasound Storage SOP Class UIDs
// =============================================================================

/// @name Primary US SOP Classes
/// @{

/// US Image Storage SOP Class UID (single-frame)
inline constexpr std::string_view us_image_storage_uid =
    "1.2.840.10008.5.1.4.1.1.6.1";

/// US Multi-frame Image Storage SOP Class UID (cine loops)
inline constexpr std::string_view us_multiframe_image_storage_uid =
    "1.2.840.10008.5.1.4.1.1.6.2";

/// @}

/// @name Retired US SOP Classes (Legacy Support)
/// @{

/// US Image Storage (Retired) - for legacy systems
inline constexpr std::string_view us_image_storage_retired_uid =
    "1.2.840.10008.5.1.4.1.1.6";

/// US Multi-frame Image Storage (Retired) - for legacy systems
inline constexpr std::string_view us_multiframe_image_storage_retired_uid =
    "1.2.840.10008.5.1.4.1.1.3.1";

/// @}

// =============================================================================
// US-Specific Transfer Syntaxes
// =============================================================================

/**
 * @brief Get recommended transfer syntaxes for US images
 *
 * Returns a prioritized list of transfer syntaxes suitable for
 * ultrasound image storage, considering color support and
 * compression requirements.
 *
 * @return Vector of transfer syntax UIDs in priority order
 */
[[nodiscard]] std::vector<std::string> get_us_transfer_syntaxes();

// =============================================================================
// US Photometric Interpretations
// =============================================================================

/**
 * @brief Supported photometric interpretations for US images
 */
enum class us_photometric_interpretation {
    monochrome1,      ///< Minimum pixel = white
    monochrome2,      ///< Minimum pixel = black (most common)
    palette_color,    ///< Pseudo-color via lookup table
    rgb,              ///< Full color RGB
    ybr_full,         ///< YCbCr full range
    ybr_full_422      ///< YCbCr 4:2:2 subsampled
};

/**
 * @brief Convert photometric interpretation enum to DICOM string
 * @param interp The photometric interpretation
 * @return DICOM-compliant string representation
 */
[[nodiscard]] std::string_view to_string(us_photometric_interpretation interp) noexcept;

/**
 * @brief Parse DICOM photometric interpretation string
 * @param value The DICOM string value
 * @return The corresponding enum value, or monochrome2 if unknown
 */
[[nodiscard]] us_photometric_interpretation
parse_photometric_interpretation(std::string_view value) noexcept;

/**
 * @brief Check if photometric interpretation is valid for US
 * @param value The DICOM string value
 * @return true if this is a valid US photometric interpretation
 */
[[nodiscard]] bool is_valid_us_photometric(std::string_view value) noexcept;

// =============================================================================
// US SOP Class Information
// =============================================================================

/**
 * @brief Information about an US Storage SOP Class
 */
struct us_sop_class_info {
    std::string_view uid;           ///< SOP Class UID
    std::string_view name;          ///< Human-readable name
    std::string_view description;   ///< Brief description
    bool is_retired;                ///< Whether this SOP class is retired
    bool supports_multiframe;       ///< Whether multi-frame is supported
};

/**
 * @brief Get all US Storage SOP Class UIDs
 *
 * Returns both current and retired SOP Class UIDs for comprehensive
 * ultrasound storage support.
 *
 * @param include_retired Include retired SOP classes (default: true)
 * @return Vector of US Storage SOP Class UIDs
 */
[[nodiscard]] std::vector<std::string>
get_us_storage_sop_classes(bool include_retired = true);

/**
 * @brief Get information about a specific US SOP Class
 *
 * @param uid The SOP Class UID to look up
 * @return Pointer to sop class info, or nullptr if not a US storage class
 */
[[nodiscard]] const us_sop_class_info*
get_us_sop_class_info(std::string_view uid) noexcept;

/**
 * @brief Check if a SOP Class UID is a US Storage SOP Class
 *
 * @param uid The SOP Class UID to check
 * @return true if this is any US storage SOP class (current or retired)
 */
[[nodiscard]] bool is_us_storage_sop_class(std::string_view uid) noexcept;

/**
 * @brief Check if a SOP Class UID is a multi-frame US Storage SOP Class
 *
 * @param uid The SOP Class UID to check
 * @return true if this is a multi-frame US storage SOP class
 */
[[nodiscard]] bool is_us_multiframe_sop_class(std::string_view uid) noexcept;

// =============================================================================
// US Region Information
// =============================================================================

/**
 * @brief US Region spatial format
 *
 * Defines the spatial organization of ultrasound regions
 * as specified in DICOM PS3.3 Section C.8.5.5
 */
enum class us_region_spatial_format : uint16_t {
    none = 0x0000,           ///< No geometric information
    two_d = 0x0001,          ///< 2D format (sector, linear, etc.)
    m_mode = 0x0002,         ///< M-mode (time-motion)
    spectral = 0x0003,       ///< Spectral Doppler
    wave_form = 0x0004,      ///< Physiological waveform
    graphics = 0x0005        ///< Graphics overlay
};

/**
 * @brief US Region data type
 *
 * Defines the type of data in an ultrasound region
 */
enum class us_region_data_type : uint16_t {
    tissue = 0x0001,           ///< Tissue characterization
    blood = 0x0002,            ///< Blood/flow
    color_flow = 0x0003,       ///< Color flow Doppler
    elastography = 0x0004,     ///< Tissue stiffness
    b_mode = 0x0005            ///< B-mode amplitude
};

}  // namespace pacs::services::sop_classes

#endif  // PACS_SERVICES_SOP_CLASSES_US_STORAGE_HPP
