/**
 * @file nm_storage.hpp
 * @brief Nuclear Medicine (NM) Image Storage SOP Classes
 *
 * This file provides SOP Class definitions and utilities for Nuclear Medicine (NM)
 * image storage including planar, SPECT, and gated acquisitions.
 *
 * @see DICOM PS3.4 Section B - Storage Service Class
 * @see DICOM PS3.3 Section A.5 - NM Image IOD
 */

#ifndef PACS_SERVICES_SOP_CLASSES_NM_STORAGE_HPP
#define PACS_SERVICES_SOP_CLASSES_NM_STORAGE_HPP

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace pacs::services::sop_classes {

// =============================================================================
// NM Storage SOP Class UIDs
// =============================================================================

/// @name Primary NM SOP Classes
/// @{

/// Nuclear Medicine Image Storage SOP Class UID
inline constexpr std::string_view nm_image_storage_uid =
    "1.2.840.10008.5.1.4.1.1.20";

/// @}

/// @name Retired NM SOP Classes (Legacy Support)
/// @{

/// Nuclear Medicine Image Storage (Retired) - for legacy systems
inline constexpr std::string_view nm_image_storage_retired_uid =
    "1.2.840.10008.5.1.4.1.1.5";

/// @}

// =============================================================================
// NM-Specific Transfer Syntaxes
// =============================================================================

/**
 * @brief Get recommended transfer syntaxes for NM images
 *
 * Returns a prioritized list of transfer syntaxes suitable for
 * nuclear medicine image storage, considering multi-frame support
 * and quantitative accuracy requirements.
 *
 * @return Vector of transfer syntax UIDs in priority order
 */
[[nodiscard]] std::vector<std::string> get_nm_transfer_syntaxes();

// =============================================================================
// NM Photometric Interpretations
// =============================================================================

/**
 * @brief Supported photometric interpretations for NM images
 *
 * NM images are typically grayscale, representing count or
 * activity values.
 */
enum class nm_photometric_interpretation {
    monochrome2,        ///< Minimum pixel = black (standard)
    palette_color       ///< Pseudo-color via lookup table (for display)
};

/**
 * @brief Convert photometric interpretation enum to DICOM string
 * @param interp The photometric interpretation
 * @return DICOM-compliant string representation
 */
[[nodiscard]] std::string_view to_string(nm_photometric_interpretation interp) noexcept;

/**
 * @brief Parse DICOM photometric interpretation string
 * @param value The DICOM string value
 * @return The corresponding enum value, or monochrome2 if unknown
 */
[[nodiscard]] nm_photometric_interpretation
parse_nm_photometric_interpretation(std::string_view value) noexcept;

/**
 * @brief Check if photometric interpretation is valid for NM
 * @param value The DICOM string value
 * @return true if this is a valid NM photometric interpretation
 */
[[nodiscard]] bool is_valid_nm_photometric(std::string_view value) noexcept;

// =============================================================================
// NM SOP Class Information
// =============================================================================

/**
 * @brief Information about a NM Storage SOP Class
 */
struct nm_sop_class_info {
    std::string_view uid;           ///< SOP Class UID
    std::string_view name;          ///< Human-readable name
    std::string_view description;   ///< Brief description
    bool is_retired;                ///< Whether this SOP class is retired
    bool supports_multiframe;       ///< Whether multi-frame is supported
};

/**
 * @brief Get all NM Storage SOP Class UIDs
 *
 * Returns both current and retired SOP Class UIDs for comprehensive
 * nuclear medicine storage support.
 *
 * @param include_retired Include retired SOP classes (default: true)
 * @return Vector of NM Storage SOP Class UIDs
 */
[[nodiscard]] std::vector<std::string>
get_nm_storage_sop_classes(bool include_retired = true);

/**
 * @brief Get information about a specific NM SOP Class
 *
 * @param uid The SOP Class UID to look up
 * @return Pointer to SOP class info, or nullptr if not a NM storage class
 */
[[nodiscard]] const nm_sop_class_info*
get_nm_sop_class_info(std::string_view uid) noexcept;

/**
 * @brief Check if a SOP Class UID is a NM Storage SOP Class
 *
 * @param uid The SOP Class UID to check
 * @return true if this is any NM storage SOP class (current or retired)
 */
[[nodiscard]] bool is_nm_storage_sop_class(std::string_view uid) noexcept;

/**
 * @brief Check if a SOP Class UID supports multi-frame
 *
 * @param uid The SOP Class UID to check
 * @return true if this SOP class supports multi-frame images
 */
[[nodiscard]] bool is_nm_multiframe_sop_class(std::string_view uid) noexcept;

// =============================================================================
// NM Image Type Codes
// =============================================================================

/**
 * @brief NM image type (Type of Data)
 */
enum class nm_type_of_data {
    static_image,       ///< STATIC - static planar image
    dynamic,            ///< DYNAMIC - dynamic study (time series)
    gated,              ///< GATED - cardiac gated acquisition
    whole_body,         ///< WHOLE BODY - whole body scan
    recon_tomo,         ///< RECON TOMO - reconstructed SPECT
    recon_gated_tomo,   ///< RECON GATED TOMO - reconstructed gated SPECT
    tomo,               ///< TOMO - SPECT raw projection data
    gated_tomo          ///< GATED TOMO - gated SPECT projections
};

/**
 * @brief Convert NM type of data to DICOM string
 * @param type The type of data
 * @return DICOM-compliant string representation
 */
[[nodiscard]] std::string_view to_string(nm_type_of_data type) noexcept;

/**
 * @brief Parse NM type of data from DICOM string
 * @param value The DICOM string value
 * @return The type of data
 */
[[nodiscard]] nm_type_of_data parse_nm_type_of_data(std::string_view value) noexcept;

// =============================================================================
// NM Detector Information
// =============================================================================

/**
 * @brief NM detector geometry type
 */
enum class nm_detector_geometry {
    planar,             ///< Planar detector (2D)
    ring,               ///< Ring detector (PET-like)
    partial_ring,       ///< Partial ring
    curved,             ///< Curved detector
    cylindrical         ///< Cylindrical detector
};

/**
 * @brief NM collimator type
 */
enum class nm_collimator_type {
    parallel,           ///< PARA - Parallel hole
    fan_beam,           ///< FANB - Fan beam
    cone_beam,          ///< CONE - Cone beam
    pinhole,            ///< PINH - Pinhole
    diverging,          ///< DIVG - Diverging
    converging,         ///< CVGB - Converging
    none                ///< NONE - No collimator
};

/**
 * @brief Convert collimator type to DICOM string
 * @param collimator The collimator type
 * @return DICOM code string
 */
[[nodiscard]] std::string_view to_string(nm_collimator_type collimator) noexcept;

/**
 * @brief Parse collimator type from DICOM string
 * @param value The DICOM string value
 * @return The collimator type
 */
[[nodiscard]] nm_collimator_type parse_nm_collimator_type(std::string_view value) noexcept;

// =============================================================================
// NM Energy Window Information
// =============================================================================

/**
 * @brief Energy window information for NM acquisition
 */
struct nm_energy_window_info {
    double lower_limit;     ///< Lower energy limit (keV)
    double upper_limit;     ///< Upper energy limit (keV)
    std::string name;       ///< Window name (e.g., "Tc-99m", "I-131")
};

/**
 * @brief Common radioisotopes used in NM imaging
 */
enum class nm_radioisotope {
    tc99m,              ///< Technetium-99m (140 keV)
    i131,               ///< Iodine-131 (364 keV)
    i123,               ///< Iodine-123 (159 keV)
    tl201,              ///< Thallium-201 (71, 167 keV)
    ga67,               ///< Gallium-67 (93, 185, 300 keV)
    in111,              ///< Indium-111 (171, 245 keV)
    f18,                ///< Fluorine-18 (511 keV - for PET)
    other               ///< Other radioisotope
};

/**
 * @brief Get radioisotope name string
 * @param isotope The radioisotope type
 * @return Human-readable name
 */
[[nodiscard]] std::string_view to_string(nm_radioisotope isotope) noexcept;

/**
 * @brief Get primary photopeak energy for radioisotope
 * @param isotope The radioisotope type
 * @return Primary photopeak energy in keV
 */
[[nodiscard]] double get_primary_energy_kev(nm_radioisotope isotope) noexcept;

// =============================================================================
// NM Acquisition Information
// =============================================================================

/**
 * @brief NM rotation direction for SPECT
 */
enum class nm_rotation_direction {
    cw,                 ///< CW - Clockwise
    cc                  ///< CC - Counter-clockwise
};

/**
 * @brief NM scan arc for SPECT
 */
enum class nm_scan_arc {
    arc_180,            ///< 180 degree arc
    arc_360             ///< 360 degree arc (full rotation)
};

/**
 * @brief Patient orientation for whole body scan
 */
enum class nm_whole_body_technique {
    single_pass,        ///< 1PASS - Single pass
    multi_pass,         ///< 2PASS - Multiple pass (anterior/posterior)
    stepping            ///< STEP - Stepping acquisition
};

/**
 * @brief Convert whole body technique to string
 * @param technique The whole body technique
 * @return DICOM code string
 */
[[nodiscard]] std::string_view to_string(nm_whole_body_technique technique) noexcept;

}  // namespace pacs::services::sop_classes

#endif  // PACS_SERVICES_SOP_CLASSES_NM_STORAGE_HPP
