/**
 * @file dx_storage.hpp
 * @brief Digital X-Ray (DX) Image Storage SOP Classes
 *
 * This file provides SOP Class definitions and utilities for Digital X-Ray (DX)
 * image storage. Supports both For Presentation and For Processing image types
 * as defined in DICOM PS3.4.
 *
 * Digital X-Ray covers general radiography imaging using digital detectors
 * (DR - Digital Radiography) as opposed to computed radiography (CR) which
 * uses phosphor plates.
 *
 * @see DICOM PS3.4 Section B - Storage Service Class
 * @see DICOM PS3.3 Section A.26 - DX Image IOD
 * @see DES-SVC-009 - Digital X-Ray Storage Implementation
 */

#ifndef PACS_SERVICES_SOP_CLASSES_DX_STORAGE_HPP
#define PACS_SERVICES_SOP_CLASSES_DX_STORAGE_HPP

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace pacs::services::sop_classes {

// =============================================================================
// Digital X-Ray Storage SOP Class UIDs
// =============================================================================

/// @name Primary DX SOP Classes
/// @{

/// Digital X-Ray Image Storage - For Presentation SOP Class UID
/// Used for images ready for display and clinical review
inline constexpr std::string_view dx_image_storage_for_presentation_uid =
    "1.2.840.10008.5.1.4.1.1.1.1";

/// Digital X-Ray Image Storage - For Processing SOP Class UID
/// Used for raw detector data requiring additional processing
inline constexpr std::string_view dx_image_storage_for_processing_uid =
    "1.2.840.10008.5.1.4.1.1.1.1.1";

/// @}

/// @name Digital Mammography X-Ray SOP Classes
/// @{

/// Digital Mammography X-Ray Image Storage - For Presentation
inline constexpr std::string_view mammography_image_storage_for_presentation_uid =
    "1.2.840.10008.5.1.4.1.1.1.2";

/// Digital Mammography X-Ray Image Storage - For Processing
inline constexpr std::string_view mammography_image_storage_for_processing_uid =
    "1.2.840.10008.5.1.4.1.1.1.2.1";

/// @}

/// @name Digital Intra-Oral X-Ray SOP Classes
/// @{

/// Digital Intra-Oral X-Ray Image Storage - For Presentation
inline constexpr std::string_view intraoral_image_storage_for_presentation_uid =
    "1.2.840.10008.5.1.4.1.1.1.3";

/// Digital Intra-Oral X-Ray Image Storage - For Processing
inline constexpr std::string_view intraoral_image_storage_for_processing_uid =
    "1.2.840.10008.5.1.4.1.1.1.3.1";

/// @}

// =============================================================================
// DX-Specific Transfer Syntaxes
// =============================================================================

/**
 * @brief Get recommended transfer syntaxes for DX images
 *
 * Returns a prioritized list of transfer syntaxes suitable for
 * digital X-ray image storage. DX images are typically grayscale
 * with high bit depth (12-16 bits).
 *
 * @return Vector of transfer syntax UIDs in priority order
 */
[[nodiscard]] std::vector<std::string> get_dx_transfer_syntaxes();

// =============================================================================
// DX Photometric Interpretations
// =============================================================================

/**
 * @brief Supported photometric interpretations for DX images
 *
 * DX images are always grayscale. MONOCHROME1 means high values are dark
 * (as on film), while MONOCHROME2 means high values are bright.
 */
enum class dx_photometric_interpretation {
    monochrome1,    ///< Minimum pixel value = white (inverted)
    monochrome2     ///< Minimum pixel value = black (standard)
};

/**
 * @brief Convert photometric interpretation enum to DICOM string
 * @param interp The photometric interpretation
 * @return DICOM-compliant string representation
 */
[[nodiscard]] std::string_view to_string(dx_photometric_interpretation interp) noexcept;

/**
 * @brief Parse DICOM photometric interpretation string for DX
 * @param value The DICOM string value
 * @return The corresponding enum value, or monochrome2 if unknown
 */
[[nodiscard]] dx_photometric_interpretation
parse_dx_photometric_interpretation(std::string_view value) noexcept;

/**
 * @brief Check if photometric interpretation is valid for DX
 * @param value The DICOM string value
 * @return true if this is a valid DX photometric interpretation
 */
[[nodiscard]] bool is_valid_dx_photometric(std::string_view value) noexcept;

// =============================================================================
// DX Image Types
// =============================================================================

/**
 * @brief DX image purpose classification
 */
enum class dx_image_type {
    for_presentation,   ///< Ready for display and diagnosis
    for_processing      ///< Raw data requiring further processing
};

/**
 * @brief Convert DX image type to string
 * @param type The image type
 * @return String representation
 */
[[nodiscard]] std::string_view to_string(dx_image_type type) noexcept;

// =============================================================================
// DX View Position
// =============================================================================

/**
 * @brief Common radiographic view positions for DX images
 *
 * View position indicates the direction of the X-ray beam relative
 * to the patient and detector.
 */
enum class dx_view_position {
    ap,         ///< Anterior-Posterior
    pa,         ///< Posterior-Anterior
    lateral,    ///< Lateral (left or right)
    oblique,    ///< Oblique projection
    other       ///< Other/unspecified
};

/**
 * @brief Convert view position enum to DICOM string
 * @param position The view position
 * @return DICOM-compliant string representation
 */
[[nodiscard]] std::string_view to_string(dx_view_position position) noexcept;

/**
 * @brief Parse DICOM view position string
 * @param value The DICOM string value
 * @return The corresponding enum value, or other if unknown
 */
[[nodiscard]] dx_view_position parse_view_position(std::string_view value) noexcept;

// =============================================================================
// DX Detector Type
// =============================================================================

/**
 * @brief DX detector technology types
 *
 * Indicates the type of digital detector used for image acquisition.
 */
enum class dx_detector_type {
    direct,     ///< Direct conversion (a-Se based)
    indirect,   ///< Indirect conversion (scintillator + photodiode)
    storage,    ///< Storage phosphor (CR-like)
    film        ///< Film digitizer (rare for DX)
};

/**
 * @brief Convert detector type enum to DICOM string
 * @param type The detector type
 * @return DICOM-compliant string representation
 */
[[nodiscard]] std::string_view to_string(dx_detector_type type) noexcept;

/**
 * @brief Parse DICOM detector type string
 * @param value The DICOM string value
 * @return The corresponding enum value
 */
[[nodiscard]] dx_detector_type parse_detector_type(std::string_view value) noexcept;

// =============================================================================
// DX SOP Class Information
// =============================================================================

/**
 * @brief Information about a DX Storage SOP Class
 */
struct dx_sop_class_info {
    std::string_view uid;           ///< SOP Class UID
    std::string_view name;          ///< Human-readable name
    std::string_view description;   ///< Brief description
    dx_image_type image_type;       ///< For Presentation or For Processing
    bool is_mammography;            ///< True if this is a mammography class
    bool is_intraoral;              ///< True if this is an intra-oral class
};

/**
 * @brief Get all DX Storage SOP Class UIDs
 *
 * Returns all DX-related SOP Class UIDs including general radiography,
 * mammography, and intra-oral imaging.
 *
 * @param include_mammography Include mammography SOP classes (default: true)
 * @param include_intraoral Include intra-oral SOP classes (default: true)
 * @return Vector of DX Storage SOP Class UIDs
 */
[[nodiscard]] std::vector<std::string>
get_dx_storage_sop_classes(bool include_mammography = true,
                           bool include_intraoral = true);

/**
 * @brief Get information about a specific DX SOP Class
 *
 * @param uid The SOP Class UID to look up
 * @return Pointer to sop class info, or nullptr if not a DX storage class
 */
[[nodiscard]] const dx_sop_class_info*
get_dx_sop_class_info(std::string_view uid) noexcept;

/**
 * @brief Check if a SOP Class UID is a DX Storage SOP Class
 *
 * Includes general DX, mammography, and intra-oral X-ray classes.
 *
 * @param uid The SOP Class UID to check
 * @return true if this is any DX-related storage SOP class
 */
[[nodiscard]] bool is_dx_storage_sop_class(std::string_view uid) noexcept;

/**
 * @brief Check if a SOP Class UID is a For Processing SOP Class
 *
 * For Processing images contain raw detector data and typically
 * require additional image processing before display.
 *
 * @param uid The SOP Class UID to check
 * @return true if this is a For Processing DX storage SOP class
 */
[[nodiscard]] bool is_dx_for_processing_sop_class(std::string_view uid) noexcept;

/**
 * @brief Check if a SOP Class UID is a For Presentation SOP Class
 *
 * For Presentation images are ready for display and clinical review.
 *
 * @param uid The SOP Class UID to check
 * @return true if this is a For Presentation DX storage SOP class
 */
[[nodiscard]] bool is_dx_for_presentation_sop_class(std::string_view uid) noexcept;

/**
 * @brief Check if a SOP Class UID is a mammography SOP Class
 *
 * @param uid The SOP Class UID to check
 * @return true if this is a mammography storage SOP class
 */
[[nodiscard]] bool is_mammography_sop_class(std::string_view uid) noexcept;

// =============================================================================
// DX Body Part Information
// =============================================================================

/**
 * @brief Common body parts for DX imaging
 *
 * These correspond to standard DICOM Body Part Examined (0018,0015) values.
 */
enum class dx_body_part {
    chest,          ///< CHEST
    abdomen,        ///< ABDOMEN
    pelvis,         ///< PELVIS
    spine,          ///< SPINE
    skull,          ///< SKULL
    hand,           ///< HAND
    foot,           ///< FOOT
    knee,           ///< KNEE
    elbow,          ///< ELBOW
    shoulder,       ///< SHOULDER
    hip,            ///< HIP
    wrist,          ///< WRIST
    ankle,          ///< ANKLE
    extremity,      ///< EXTREMITY (general)
    breast,         ///< BREAST (for mammography)
    other           ///< Other/unspecified
};

/**
 * @brief Convert body part enum to DICOM string
 * @param part The body part
 * @return DICOM-compliant string representation
 */
[[nodiscard]] std::string_view to_string(dx_body_part part) noexcept;

/**
 * @brief Parse DICOM body part examined string
 * @param value The DICOM string value
 * @return The corresponding enum value, or other if unknown
 */
[[nodiscard]] dx_body_part parse_body_part(std::string_view value) noexcept;

}  // namespace pacs::services::sop_classes

#endif  // PACS_SERVICES_SOP_CLASSES_DX_STORAGE_HPP
