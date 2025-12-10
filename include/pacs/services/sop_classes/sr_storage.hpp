/**
 * @file sr_storage.hpp
 * @brief Structured Report (SR) Storage SOP Classes
 *
 * This file provides SOP Class definitions and utilities for Structured Report
 * object storage. Supports various SR document types including Basic Text SR,
 * Enhanced SR, Comprehensive SR, and specialized SR documents for AI/CAD results.
 *
 * @see DICOM PS3.4 Section B - Storage Service Class
 * @see DICOM PS3.3 Section A.35 - SR Document IODs
 */

#ifndef PACS_SERVICES_SOP_CLASSES_SR_STORAGE_HPP
#define PACS_SERVICES_SOP_CLASSES_SR_STORAGE_HPP

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace pacs::services::sop_classes {

// =============================================================================
// Structured Report Storage SOP Class UIDs
// =============================================================================

/// @name Basic SR SOP Classes
/// @{

/// Basic Text SR Storage SOP Class UID
inline constexpr std::string_view basic_text_sr_storage_uid =
    "1.2.840.10008.5.1.4.1.1.88.11";

/// Enhanced SR Storage SOP Class UID
inline constexpr std::string_view enhanced_sr_storage_uid =
    "1.2.840.10008.5.1.4.1.1.88.22";

/// Comprehensive SR Storage SOP Class UID
inline constexpr std::string_view comprehensive_sr_storage_uid =
    "1.2.840.10008.5.1.4.1.1.88.33";

/// Comprehensive 3D SR Storage SOP Class UID
inline constexpr std::string_view comprehensive_3d_sr_storage_uid =
    "1.2.840.10008.5.1.4.1.1.88.34";

/// Extensible SR Storage SOP Class UID
inline constexpr std::string_view extensible_sr_storage_uid =
    "1.2.840.10008.5.1.4.1.1.88.35";

/// @}

/// @name Specialized SR SOP Classes
/// @{

/// Mammography CAD SR Storage SOP Class UID
inline constexpr std::string_view mammography_cad_sr_storage_uid =
    "1.2.840.10008.5.1.4.1.1.88.50";

/// Chest CAD SR Storage SOP Class UID
inline constexpr std::string_view chest_cad_sr_storage_uid =
    "1.2.840.10008.5.1.4.1.1.88.65";

/// Colon CAD SR Storage SOP Class UID
inline constexpr std::string_view colon_cad_sr_storage_uid =
    "1.2.840.10008.5.1.4.1.1.88.69";

/// X-Ray Radiation Dose SR Storage SOP Class UID
inline constexpr std::string_view xray_radiation_dose_sr_storage_uid =
    "1.2.840.10008.5.1.4.1.1.88.67";

/// Radiopharmaceutical Radiation Dose SR Storage SOP Class UID
inline constexpr std::string_view radiopharmaceutical_radiation_dose_sr_storage_uid =
    "1.2.840.10008.5.1.4.1.1.88.68";

/// Acquisition Context SR Storage SOP Class UID
inline constexpr std::string_view acquisition_context_sr_storage_uid =
    "1.2.840.10008.5.1.4.1.1.88.71";

/// Simplified Adult Echo SR Storage SOP Class UID
inline constexpr std::string_view simplified_adult_echo_sr_storage_uid =
    "1.2.840.10008.5.1.4.1.1.88.72";

/// Patient Radiation Dose SR Storage SOP Class UID
inline constexpr std::string_view patient_radiation_dose_sr_storage_uid =
    "1.2.840.10008.5.1.4.1.1.88.73";

/// Planned Imaging Agent Administration SR Storage SOP Class UID
inline constexpr std::string_view planned_imaging_agent_admin_sr_storage_uid =
    "1.2.840.10008.5.1.4.1.1.88.74";

/// Performed Imaging Agent Administration SR Storage SOP Class UID
inline constexpr std::string_view performed_imaging_agent_admin_sr_storage_uid =
    "1.2.840.10008.5.1.4.1.1.88.75";

/// Enhanced X-Ray Radiation Dose SR Storage SOP Class UID
inline constexpr std::string_view enhanced_xray_radiation_dose_sr_storage_uid =
    "1.2.840.10008.5.1.4.1.1.88.76";

/// @}

/// @name Key Object Selection Document
/// @{

/// Key Object Selection Document Storage SOP Class UID
inline constexpr std::string_view key_object_selection_document_storage_uid =
    "1.2.840.10008.5.1.4.1.1.88.59";

/// @}

// =============================================================================
// SR-Specific Transfer Syntaxes
// =============================================================================

/**
 * @brief Get recommended transfer syntaxes for SR objects
 *
 * Returns a prioritized list of transfer syntaxes suitable for
 * structured report storage. SR objects are typically small and
 * don't benefit from compression.
 *
 * @return Vector of transfer syntax UIDs in priority order
 */
[[nodiscard]] std::vector<std::string> get_sr_transfer_syntaxes();

// =============================================================================
// SR Document Type
// =============================================================================

/**
 * @brief SR Document type classification
 *
 * Categorizes SR documents by their primary purpose.
 */
enum class sr_document_type {
    basic_text,             ///< Basic Text SR - Simple text reports
    enhanced,               ///< Enhanced SR - References to images/waveforms
    comprehensive,          ///< Comprehensive SR - Complex with spatial coords
    comprehensive_3d,       ///< Comprehensive 3D SR - 3D spatial coordinates
    extensible,             ///< Extensible SR - Template-based
    key_object_selection,   ///< Key Object Selection - Image selection
    cad,                    ///< CAD SR - Computer-aided detection results
    dose_report,            ///< Dose Report - Radiation dose information
    procedure_log,          ///< Procedure Log - Procedure documentation
    other                   ///< Other specialized SR types
};

/**
 * @brief Get SR document type for a SOP Class UID
 * @param uid The SOP Class UID
 * @return The SR document type
 */
[[nodiscard]] sr_document_type get_sr_document_type(std::string_view uid) noexcept;

/**
 * @brief Get human-readable name for SR document type
 * @param type The SR document type
 * @return Human-readable type name
 */
[[nodiscard]] std::string_view to_string(sr_document_type type) noexcept;

// =============================================================================
// SR Value Types
// =============================================================================

/**
 * @brief SR Content Item Value Type (0040,A040)
 *
 * Defines the type of value contained in a content item.
 */
enum class sr_value_type {
    text,           ///< TEXT - Free text
    code,           ///< CODE - Coded entry
    num,            ///< NUM - Numeric measurement
    datetime,       ///< DATETIME - Date/time value
    date,           ///< DATE - Date value
    time,           ///< TIME - Time value
    uidref,         ///< UIDREF - UID reference
    pname,          ///< PNAME - Person name
    composite,      ///< COMPOSITE - Reference to composite object
    image,          ///< IMAGE - Reference to image
    waveform,       ///< WAVEFORM - Reference to waveform
    scoord,         ///< SCOORD - Spatial coordinates (2D)
    scoord3d,       ///< SCOORD3D - Spatial coordinates (3D)
    tcoord,         ///< TCOORD - Temporal coordinates
    container,      ///< CONTAINER - Container for other items
    table,          ///< TABLE - Tabular data (Extensible SR)
    unknown         ///< Unknown value type
};

/**
 * @brief Convert SR value type to DICOM string
 * @param type The SR value type
 * @return DICOM-compliant string representation
 */
[[nodiscard]] std::string_view to_string(sr_value_type type) noexcept;

/**
 * @brief Parse SR value type from DICOM string
 * @param value The DICOM string value
 * @return The SR value type
 */
[[nodiscard]] sr_value_type parse_sr_value_type(std::string_view value) noexcept;

/**
 * @brief Check if SR value type string is valid
 * @param value The DICOM string value
 * @return true if this is a valid SR value type
 */
[[nodiscard]] bool is_valid_sr_value_type(std::string_view value) noexcept;

// =============================================================================
// SR Relationship Types
// =============================================================================

/**
 * @brief SR Content Item Relationship Type (0040,A010)
 *
 * Defines the relationship between content items in the SR tree.
 */
enum class sr_relationship_type {
    contains,           ///< CONTAINS - Parent contains child
    has_obs_context,    ///< HAS OBS CONTEXT - Observation context
    has_acq_context,    ///< HAS ACQ CONTEXT - Acquisition context
    has_concept_mod,    ///< HAS CONCEPT MOD - Concept modifier
    has_properties,     ///< HAS PROPERTIES - Property value
    inferred_from,      ///< INFERRED FROM - Inference source
    selected_from,      ///< SELECTED FROM - Selection source
    unknown             ///< Unknown relationship
};

/**
 * @brief Convert SR relationship type to DICOM string
 * @param type The SR relationship type
 * @return DICOM-compliant string representation
 */
[[nodiscard]] std::string_view to_string(sr_relationship_type type) noexcept;

/**
 * @brief Parse SR relationship type from DICOM string
 * @param value The DICOM string value
 * @return The SR relationship type
 */
[[nodiscard]] sr_relationship_type parse_sr_relationship_type(std::string_view value) noexcept;

// =============================================================================
// SR Completion and Verification
// =============================================================================

/**
 * @brief SR Completion Flag (0040,A491)
 */
enum class sr_completion_flag {
    partial,    ///< PARTIAL - Document is not complete
    complete    ///< COMPLETE - Document is complete
};

/**
 * @brief Convert SR completion flag to DICOM string
 * @param flag The completion flag
 * @return DICOM-compliant string representation
 */
[[nodiscard]] std::string_view to_string(sr_completion_flag flag) noexcept;

/**
 * @brief Parse SR completion flag from DICOM string
 * @param value The DICOM string value
 * @return The completion flag
 */
[[nodiscard]] sr_completion_flag parse_sr_completion_flag(std::string_view value) noexcept;

/**
 * @brief SR Verification Flag (0040,A493)
 */
enum class sr_verification_flag {
    unverified,     ///< UNVERIFIED - Not verified
    verified        ///< VERIFIED - Verified by authorized person
};

/**
 * @brief Convert SR verification flag to DICOM string
 * @param flag The verification flag
 * @return DICOM-compliant string representation
 */
[[nodiscard]] std::string_view to_string(sr_verification_flag flag) noexcept;

/**
 * @brief Parse SR verification flag from DICOM string
 * @param value The DICOM string value
 * @return The verification flag
 */
[[nodiscard]] sr_verification_flag parse_sr_verification_flag(std::string_view value) noexcept;

// =============================================================================
// SR SOP Class Information
// =============================================================================

/**
 * @brief Information about an SR Storage SOP Class
 */
struct sr_sop_class_info {
    std::string_view uid;               ///< SOP Class UID
    std::string_view name;              ///< Human-readable name
    std::string_view description;       ///< Brief description
    sr_document_type document_type;     ///< Document type classification
    bool is_retired;                    ///< Whether this SOP class is retired
    bool supports_spatial_coords;       ///< Can contain SCOORD/SCOORD3D
    bool supports_waveform_ref;         ///< Can reference waveforms
};

/**
 * @brief Get all SR Storage SOP Class UIDs
 *
 * @param include_cad Include CAD-specific SR classes (default: true)
 * @param include_dose Include dose report SR classes (default: true)
 * @return Vector of SR Storage SOP Class UIDs
 */
[[nodiscard]] std::vector<std::string>
get_sr_storage_sop_classes(bool include_cad = true, bool include_dose = true);

/**
 * @brief Get information about a specific SR SOP Class
 *
 * @param uid The SOP Class UID to look up
 * @return Pointer to SOP class info, or nullptr if not an SR storage class
 */
[[nodiscard]] const sr_sop_class_info*
get_sr_sop_class_info(std::string_view uid) noexcept;

/**
 * @brief Check if a SOP Class UID is an SR Storage SOP Class
 *
 * @param uid The SOP Class UID to check
 * @return true if this is any SR storage SOP class
 */
[[nodiscard]] bool is_sr_storage_sop_class(std::string_view uid) noexcept;

/**
 * @brief Check if a SOP Class UID is a CAD SR Storage SOP Class
 *
 * @param uid The SOP Class UID to check
 * @return true if this is a CAD SR storage SOP class
 */
[[nodiscard]] bool is_cad_sr_storage_sop_class(std::string_view uid) noexcept;

/**
 * @brief Check if a SOP Class UID is a Dose Report SR Storage SOP Class
 *
 * @param uid The SOP Class UID to check
 * @return true if this is a dose report SR storage SOP class
 */
[[nodiscard]] bool is_dose_sr_storage_sop_class(std::string_view uid) noexcept;

/**
 * @brief Check if an SR SOP Class supports spatial coordinates
 *
 * @param uid The SOP Class UID to check
 * @return true if the SR can contain SCOORD or SCOORD3D content items
 */
[[nodiscard]] bool sr_supports_spatial_coords(std::string_view uid) noexcept;

// =============================================================================
// SR Template Identification
// =============================================================================

/**
 * @brief Common SR Template IDs (TID)
 *
 * Well-known template identifiers for SR document structure.
 */
namespace sr_template {

/// Basic Diagnostic Imaging Report (TID 2000)
inline constexpr std::string_view basic_diagnostic_imaging_report = "2000";

/// Mammography CAD Report (TID 4000)
inline constexpr std::string_view mammography_cad_report = "4000";

/// Chest CAD Report (TID 4100)
inline constexpr std::string_view chest_cad_report = "4100";

/// Colon CAD Report (TID 4200)
inline constexpr std::string_view colon_cad_report = "4200";

/// X-Ray Radiation Dose Report (TID 10001)
inline constexpr std::string_view xray_radiation_dose_report = "10001";

/// CT Radiation Dose Report (TID 10011)
inline constexpr std::string_view ct_radiation_dose_report = "10011";

/// Projection X-Ray Radiation Dose Report (TID 10020)
inline constexpr std::string_view projection_xray_dose_report = "10020";

/// Key Object Selection (TID 2010)
inline constexpr std::string_view key_object_selection = "2010";

/// AI Results Report (TID 1500 - measurement group)
inline constexpr std::string_view measurement_report = "1500";

}  // namespace sr_template

/**
 * @brief Get recommended template ID for an SR SOP Class
 * @param uid The SR SOP Class UID
 * @return Template ID string, or empty if no specific template recommended
 */
[[nodiscard]] std::string_view get_recommended_sr_template(std::string_view uid) noexcept;

}  // namespace pacs::services::sop_classes

#endif  // PACS_SERVICES_SOP_CLASSES_SR_STORAGE_HPP
