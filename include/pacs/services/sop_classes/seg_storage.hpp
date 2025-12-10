/**
 * @file seg_storage.hpp
 * @brief Segmentation (SEG) Storage SOP Classes
 *
 * This file provides SOP Class definitions and utilities for Segmentation
 * object storage. Supports DICOM Segmentation objects for AI/CAD outputs
 * and clinical segmentation results.
 *
 * @see DICOM PS3.4 Section B - Storage Service Class
 * @see DICOM PS3.3 Section A.51 - Segmentation IOD
 */

#ifndef PACS_SERVICES_SOP_CLASSES_SEG_STORAGE_HPP
#define PACS_SERVICES_SOP_CLASSES_SEG_STORAGE_HPP

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace pacs::services::sop_classes {

// =============================================================================
// Segmentation Storage SOP Class UIDs
// =============================================================================

/// @name Primary SEG SOP Classes
/// @{

/// Segmentation Storage SOP Class UID
inline constexpr std::string_view segmentation_storage_uid =
    "1.2.840.10008.5.1.4.1.1.66.4";

/// Surface Segmentation Storage SOP Class UID
inline constexpr std::string_view surface_segmentation_storage_uid =
    "1.2.840.10008.5.1.4.1.1.66.5";

/// @}

// =============================================================================
// SEG-Specific Transfer Syntaxes
// =============================================================================

/**
 * @brief Get recommended transfer syntaxes for SEG objects
 *
 * Returns a prioritized list of transfer syntaxes suitable for
 * segmentation object storage. Binary segmentations benefit from
 * lossless compression.
 *
 * @return Vector of transfer syntax UIDs in priority order
 */
[[nodiscard]] std::vector<std::string> get_seg_transfer_syntaxes();

// =============================================================================
// Segmentation Type
// =============================================================================

/**
 * @brief Segmentation type (0062,0001)
 *
 * Defines whether the segmentation is binary or fractional.
 */
enum class segmentation_type {
    binary,         ///< BINARY - Binary segmentation (0 or 1)
    fractional      ///< FRACTIONAL - Fractional/probabilistic segmentation
};

/**
 * @brief Convert segmentation type to DICOM string
 * @param type The segmentation type
 * @return DICOM-compliant string representation
 */
[[nodiscard]] std::string_view to_string(segmentation_type type) noexcept;

/**
 * @brief Parse segmentation type from DICOM string
 * @param value The DICOM string value
 * @return The segmentation type
 */
[[nodiscard]] segmentation_type parse_segmentation_type(std::string_view value) noexcept;

/**
 * @brief Check if segmentation type string is valid
 * @param value The DICOM string value
 * @return true if this is a valid segmentation type
 */
[[nodiscard]] bool is_valid_segmentation_type(std::string_view value) noexcept;

// =============================================================================
// Segmentation Fractional Type
// =============================================================================

/**
 * @brief Segmentation fractional type (0062,0010)
 *
 * Defines the meaning of fractional values when Segmentation Type is FRACTIONAL.
 */
enum class segmentation_fractional_type {
    probability,    ///< PROBABILITY - Values represent probability (0.0-1.0)
    occupancy       ///< OCCUPANCY - Values represent fractional occupancy
};

/**
 * @brief Convert segmentation fractional type to DICOM string
 * @param type The segmentation fractional type
 * @return DICOM-compliant string representation
 */
[[nodiscard]] std::string_view to_string(segmentation_fractional_type type) noexcept;

/**
 * @brief Parse segmentation fractional type from DICOM string
 * @param value The DICOM string value
 * @return The segmentation fractional type
 */
[[nodiscard]] segmentation_fractional_type
parse_segmentation_fractional_type(std::string_view value) noexcept;

// =============================================================================
// Segment Algorithm Type
// =============================================================================

/**
 * @brief Segment algorithm type (0062,0008)
 *
 * Defines how the segment was created.
 */
enum class segment_algorithm_type {
    automatic,      ///< AUTOMATIC - Fully automated segmentation
    semiautomatic,  ///< SEMIAUTOMATIC - Semi-automated with user input
    manual          ///< MANUAL - Manual segmentation
};

/**
 * @brief Convert segment algorithm type to DICOM string
 * @param type The segment algorithm type
 * @return DICOM-compliant string representation
 */
[[nodiscard]] std::string_view to_string(segment_algorithm_type type) noexcept;

/**
 * @brief Parse segment algorithm type from DICOM string
 * @param value The DICOM string value
 * @return The segment algorithm type
 */
[[nodiscard]] segment_algorithm_type
parse_segment_algorithm_type(std::string_view value) noexcept;

/**
 * @brief Check if segment algorithm type string is valid
 * @param value The DICOM string value
 * @return true if this is a valid segment algorithm type
 */
[[nodiscard]] bool is_valid_segment_algorithm_type(std::string_view value) noexcept;

// =============================================================================
// Recommended Display CIELab Value
// =============================================================================

/**
 * @brief Standard segment colors for common anatomical structures
 *
 * CIELab values as defined in DICOM PS3.3 Table C.8.20-3
 */
struct segment_color {
    uint16_t l;     ///< L* component (0-65535, maps to 0-100)
    uint16_t a;     ///< a* component (0-65535, maps to -128 to 127)
    uint16_t b;     ///< b* component (0-65535, maps to -128 to 127)
};

/**
 * @brief Get recommended color for common segment types
 * @param segment_label The segment label (e.g., "Liver", "Tumor")
 * @return Recommended CIELab color, or default gray if unknown
 */
[[nodiscard]] segment_color get_recommended_segment_color(std::string_view segment_label) noexcept;

// =============================================================================
// SEG SOP Class Information
// =============================================================================

/**
 * @brief Information about a SEG Storage SOP Class
 */
struct seg_sop_class_info {
    std::string_view uid;           ///< SOP Class UID
    std::string_view name;          ///< Human-readable name
    std::string_view description;   ///< Brief description
    bool is_retired;                ///< Whether this SOP class is retired
    bool is_surface;                ///< Whether this is surface segmentation
};

/**
 * @brief Get all SEG Storage SOP Class UIDs
 *
 * @param include_surface Include surface segmentation (default: true)
 * @return Vector of SEG Storage SOP Class UIDs
 */
[[nodiscard]] std::vector<std::string>
get_seg_storage_sop_classes(bool include_surface = true);

/**
 * @brief Get information about a specific SEG SOP Class
 *
 * @param uid The SOP Class UID to look up
 * @return Pointer to SOP class info, or nullptr if not a SEG storage class
 */
[[nodiscard]] const seg_sop_class_info*
get_seg_sop_class_info(std::string_view uid) noexcept;

/**
 * @brief Check if a SOP Class UID is a SEG Storage SOP Class
 *
 * @param uid The SOP Class UID to check
 * @return true if this is any SEG storage SOP class
 */
[[nodiscard]] bool is_seg_storage_sop_class(std::string_view uid) noexcept;

/**
 * @brief Check if a SOP Class UID is Surface Segmentation
 *
 * @param uid The SOP Class UID to check
 * @return true if this is Surface Segmentation SOP class
 */
[[nodiscard]] bool is_surface_segmentation_sop_class(std::string_view uid) noexcept;

// =============================================================================
// Segment Category and Type Codes
// =============================================================================

/**
 * @brief Common anatomical property categories (CID 7150)
 *
 * Predefined category codes for segment categorization
 */
enum class segment_category {
    tissue,                 ///< Tissue type (organ, muscle, etc.)
    anatomical_structure,   ///< Named anatomical structure
    physical_object,        ///< Physical object (implant, etc.)
    morphologically_abnormal, ///< Tumor, lesion, etc.
    function,               ///< Functional region
    spatial,                ///< Spatial reference
    body_substance          ///< Body fluid, substance
};

/**
 * @brief Get SNOMED CT code for segment category
 * @param category The segment category
 * @return SNOMED CT code string
 */
[[nodiscard]] std::string_view get_segment_category_code(segment_category category) noexcept;

/**
 * @brief Get code meaning for segment category
 * @param category The segment category
 * @return Human-readable category name
 */
[[nodiscard]] std::string_view get_segment_category_meaning(segment_category category) noexcept;

}  // namespace pacs::services::sop_classes

#endif  // PACS_SERVICES_SOP_CLASSES_SEG_STORAGE_HPP
