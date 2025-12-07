/**
 * @file mg_storage.hpp
 * @brief Digital Mammography X-Ray Image Storage SOP Classes
 *
 * This file provides SOP Class definitions and utilities specific to
 * Digital Mammography X-Ray image storage. Mammography has unique requirements
 * for breast cancer screening and diagnostic imaging workflows.
 *
 * Mammography imaging is characterized by:
 * - Breast laterality specification (left/right/bilateral)
 * - Specialized view positions (CC, MLO, etc.)
 * - Compression force documentation
 * - High spatial resolution requirements
 *
 * @see DICOM PS3.4 Section B - Storage Service Class
 * @see DICOM PS3.3 Section A.26.2 - Digital Mammography X-Ray Image IOD
 * @see ACR BI-RADS Mammography Atlas
 */

#ifndef PACS_SERVICES_SOP_CLASSES_MG_STORAGE_HPP
#define PACS_SERVICES_SOP_CLASSES_MG_STORAGE_HPP

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pacs::services::sop_classes {

// =============================================================================
// Digital Mammography Storage SOP Class UIDs (re-exported from dx_storage.hpp)
// =============================================================================

/// @name Digital Mammography X-Ray SOP Classes
/// @{

/// Digital Mammography X-Ray Image Storage - For Presentation SOP Class UID
inline constexpr std::string_view mg_image_storage_for_presentation_uid =
    "1.2.840.10008.5.1.4.1.1.1.2";

/// Digital Mammography X-Ray Image Storage - For Processing SOP Class UID
inline constexpr std::string_view mg_image_storage_for_processing_uid =
    "1.2.840.10008.5.1.4.1.1.1.2.1";

/// Breast Tomosynthesis Image Storage SOP Class UID
inline constexpr std::string_view breast_tomosynthesis_image_storage_uid =
    "1.2.840.10008.5.1.4.1.1.13.1.3";

/// Breast Projection X-Ray Image Storage - For Presentation SOP Class UID
inline constexpr std::string_view breast_projection_image_storage_for_presentation_uid =
    "1.2.840.10008.5.1.4.1.1.13.1.4";

/// Breast Projection X-Ray Image Storage - For Processing SOP Class UID
inline constexpr std::string_view breast_projection_image_storage_for_processing_uid =
    "1.2.840.10008.5.1.4.1.1.13.1.5";

/// @}

// =============================================================================
// Breast Laterality
// =============================================================================

/**
 * @brief Breast laterality enumeration
 *
 * Specifies which breast is being imaged. This is a critical attribute for
 * mammography workflow and helps prevent laterality errors in clinical practice.
 *
 * @see DICOM Tag (0020,0060) - Laterality
 * @see DICOM Tag (0020,0062) - Image Laterality
 */
enum class breast_laterality {
    left,       ///< Left breast (DICOM value: "L")
    right,      ///< Right breast (DICOM value: "R")
    bilateral,  ///< Both breasts (DICOM value: "B") - used for comparison views
    unknown     ///< Unknown or unspecified laterality
};

/**
 * @brief Convert breast laterality enum to DICOM string
 * @param laterality The breast laterality
 * @return DICOM-compliant string representation ("L", "R", "B", or empty)
 */
[[nodiscard]] std::string_view to_string(breast_laterality laterality) noexcept;

/**
 * @brief Parse DICOM laterality string to enum
 * @param value The DICOM string value
 * @return The corresponding enum value, or unknown if not recognized
 */
[[nodiscard]] breast_laterality parse_breast_laterality(std::string_view value) noexcept;

/**
 * @brief Check if a laterality value is valid for mammography
 * @param value The DICOM string value
 * @return true if this is a valid mammography laterality value
 */
[[nodiscard]] bool is_valid_breast_laterality(std::string_view value) noexcept;

// =============================================================================
// Mammography View Position
// =============================================================================

/**
 * @brief Mammography-specific view positions
 *
 * Standard mammography views as defined by the ACR (American College of
 * Radiology) and used in DICOM. View position is essential for proper
 * image interpretation and comparison studies.
 *
 * @see DICOM Tag (0018,5101) - View Position
 * @see ACR BI-RADS Mammography Atlas
 */
enum class mg_view_position {
    // Standard screening views
    cc,         ///< Craniocaudal - standard superior-inferior view
    mlo,        ///< Mediolateral Oblique - angled lateral view (most common)

    // Additional diagnostic views
    ml,         ///< Mediolateral - true lateral (medial to lateral)
    lm,         ///< Lateromedial - true lateral (lateral to medial)
    xccl,       ///< Exaggerated CC Laterally - for lateral breast tissue
    xccm,       ///< Exaggerated CC Medially - for medial breast tissue
    fb,         ///< From Below - inferior to superior view
    sio,        ///< Superolateral to Inferomedial Oblique
    iso,        ///< Inferomedial to Superolateral Oblique
    cv,         ///< Cleavage View - for medial breast tissue
    at,         ///< Axillary Tail - for axillary extension

    // Spot/magnification views
    spot,       ///< Spot compression view
    mag,        ///< Magnification view
    spot_mag,   ///< Spot compression with magnification
    rl,         ///< Rolled Lateral
    rm,         ///< Rolled Medial
    rs,         ///< Rolled Superior
    ri,         ///< Rolled Inferior

    // Specialized views
    tangen,     ///< Tangential view
    implant,    ///< Implant displaced view (Eklund technique)
    id,         ///< Implant Displaced (alternate code)

    // Other
    other       ///< Other or unspecified view
};

/**
 * @brief Convert mammography view position enum to DICOM string
 * @param position The view position
 * @return DICOM-compliant string representation
 */
[[nodiscard]] std::string_view to_string(mg_view_position position) noexcept;

/**
 * @brief Parse DICOM view position string to mammography view enum
 * @param value The DICOM string value
 * @return The corresponding enum value, or other if not recognized
 */
[[nodiscard]] mg_view_position parse_mg_view_position(std::string_view value) noexcept;

/**
 * @brief Check if a view position is a standard screening view
 *
 * Standard screening mammography typically includes CC and MLO views
 * of each breast.
 *
 * @param position The view position to check
 * @return true if this is a standard screening view
 */
[[nodiscard]] bool is_screening_view(mg_view_position position) noexcept;

/**
 * @brief Check if a view position requires magnification
 * @param position The view position to check
 * @return true if this view typically involves magnification
 */
[[nodiscard]] bool is_magnification_view(mg_view_position position) noexcept;

/**
 * @brief Check if a view position involves spot compression
 * @param position The view position to check
 * @return true if this view involves spot compression
 */
[[nodiscard]] bool is_spot_compression_view(mg_view_position position) noexcept;

/**
 * @brief Get all valid mammography view position strings
 * @return Vector of valid DICOM view position codes
 */
[[nodiscard]] std::vector<std::string_view> get_valid_mg_view_positions() noexcept;

// =============================================================================
// Mammography Image Acquisition Parameters
// =============================================================================

/**
 * @brief Mammography acquisition parameters structure
 *
 * Contains key acquisition parameters specific to mammography imaging.
 * These parameters are important for quality control and image interpretation.
 */
struct mg_acquisition_params {
    /// Compression force in Newtons (0018,11A2)
    std::optional<double> compression_force_n;

    /// Compressed breast thickness in mm (0018,11A0)
    std::optional<double> compressed_breast_thickness_mm;

    /// Body part thickness in mm (0018,11A0) - deprecated, use compressed_breast_thickness
    std::optional<double> body_part_thickness_mm;

    /// Relative X-ray exposure (0018,1405)
    std::optional<double> relative_x_ray_exposure;

    /// Entrance dose in dGy (0040,0302)
    std::optional<double> entrance_dose_dgy;

    /// Entrance dose derivation (0040,0303)
    std::optional<std::string> entrance_dose_derivation;

    /// Organ dose in dGy (0040,0316)
    std::optional<double> organ_dose_dgy;

    /// Half value layer in mm Al (0040,0314)
    std::optional<double> half_value_layer_mm;

    /// KVP - X-ray tube peak kilovoltage (0018,0060)
    std::optional<double> kvp;

    /// Exposure time in ms (0018,1150)
    std::optional<double> exposure_time_ms;

    /// X-ray tube current in mA (0018,1151)
    std::optional<double> tube_current_ma;

    /// Exposure in mAs (0018,1153)
    std::optional<double> exposure_mas;

    /// Anode target material (0018,1191)
    std::optional<std::string> anode_target_material;

    /// Filter material (0018,7050)
    std::optional<std::string> filter_material;

    /// Filter thickness in mm (0018,7052)
    std::optional<double> filter_thickness_mm;

    /// Focal spot size in mm (0018,1190)
    std::optional<double> focal_spot_mm;

    /// Breast implant present (0028,1300)
    std::optional<bool> breast_implant_present;

    /// Partial view flag (0028,1350)
    std::optional<bool> partial_view;

    /// Partial view description (0028,1351)
    std::optional<std::string> partial_view_description;
};

/**
 * @brief Validate compression force value
 *
 * Typical compression force for mammography is between 50-200 Newtons.
 * Values outside this range may indicate measurement errors or non-standard
 * technique.
 *
 * @param force_n Compression force in Newtons
 * @return true if the force is within typical mammography range
 */
[[nodiscard]] bool is_valid_compression_force(double force_n) noexcept;

/**
 * @brief Get typical compression force range
 * @return Pair of (minimum, maximum) typical compression force in Newtons
 */
[[nodiscard]] std::pair<double, double> get_typical_compression_force_range() noexcept;

/**
 * @brief Validate compressed breast thickness
 *
 * @param thickness_mm Compressed breast thickness in mm
 * @return true if the thickness is within reasonable range
 */
[[nodiscard]] bool is_valid_compressed_breast_thickness(double thickness_mm) noexcept;

// =============================================================================
// Mammography Image Type
// =============================================================================

/**
 * @brief Mammography image purpose classification
 *
 * Mammography images can be either for presentation (display-ready with
 * applied processing) or for processing (raw data requiring additional
 * image processing).
 */
enum class mg_image_type {
    for_presentation,   ///< Ready for display and diagnosis
    for_processing      ///< Raw data requiring further processing
};

/**
 * @brief Convert mammography image type to string
 * @param type The image type
 * @return String representation
 */
[[nodiscard]] std::string_view to_string(mg_image_type type) noexcept;

// =============================================================================
// Mammography CAD Integration
// =============================================================================

/**
 * @brief CAD (Computer-Aided Detection) processing status
 *
 * Indicates whether and how CAD analysis has been performed on the image.
 */
enum class cad_processing_status {
    not_processed,          ///< CAD has not been run on this image
    processed_no_findings,  ///< CAD completed with no findings
    processed_findings,     ///< CAD completed with findings
    processing_failed,      ///< CAD processing failed
    pending                 ///< CAD processing is pending
};

/**
 * @brief Convert CAD processing status to display string
 * @param status The CAD status
 * @return Human-readable string representation
 */
[[nodiscard]] std::string_view to_string(cad_processing_status status) noexcept;

// =============================================================================
// Mammography SOP Class Information
// =============================================================================

/**
 * @brief Information about a Mammography Storage SOP Class
 */
struct mg_sop_class_info {
    std::string_view uid;           ///< SOP Class UID
    std::string_view name;          ///< Human-readable name
    std::string_view description;   ///< Brief description
    mg_image_type image_type;       ///< For Presentation or For Processing
    bool is_tomosynthesis;          ///< True if this is a tomosynthesis class
    bool supports_multiframe;       ///< Multi-frame support
};

/**
 * @brief Get all Mammography Storage SOP Class UIDs
 *
 * Returns all mammography-related SOP Class UIDs including standard 2D
 * mammography and breast tomosynthesis.
 *
 * @param include_tomosynthesis Include breast tomosynthesis SOP classes (default: true)
 * @return Vector of Mammography Storage SOP Class UIDs
 */
[[nodiscard]] std::vector<std::string>
get_mg_storage_sop_classes(bool include_tomosynthesis = true);

/**
 * @brief Get information about a specific Mammography SOP Class
 *
 * @param uid The SOP Class UID to look up
 * @return Pointer to sop class info, or nullptr if not a mammography storage class
 */
[[nodiscard]] const mg_sop_class_info*
get_mg_sop_class_info(std::string_view uid) noexcept;

/**
 * @brief Check if a SOP Class UID is a Mammography Storage SOP Class
 *
 * Includes standard mammography and breast tomosynthesis classes.
 *
 * @param uid The SOP Class UID to check
 * @return true if this is any mammography-related storage SOP class
 */
[[nodiscard]] bool is_mg_storage_sop_class(std::string_view uid) noexcept;

/**
 * @brief Check if a SOP Class UID is a breast tomosynthesis SOP Class
 *
 * @param uid The SOP Class UID to check
 * @return true if this is a breast tomosynthesis storage SOP class
 */
[[nodiscard]] bool is_breast_tomosynthesis_sop_class(std::string_view uid) noexcept;

/**
 * @brief Check if a SOP Class UID is a For Processing mammography SOP Class
 *
 * @param uid The SOP Class UID to check
 * @return true if this is a For Processing mammography storage SOP class
 */
[[nodiscard]] bool is_mg_for_processing_sop_class(std::string_view uid) noexcept;

/**
 * @brief Check if a SOP Class UID is a For Presentation mammography SOP Class
 *
 * @param uid The SOP Class UID to check
 * @return true if this is a For Presentation mammography storage SOP class
 */
[[nodiscard]] bool is_mg_for_presentation_sop_class(std::string_view uid) noexcept;

// =============================================================================
// Transfer Syntax Recommendations
// =============================================================================

/**
 * @brief Get recommended transfer syntaxes for mammography images
 *
 * Returns a prioritized list of transfer syntaxes suitable for
 * mammography image storage. Mammography images typically have very
 * high spatial resolution and benefit from specific compression approaches.
 *
 * @return Vector of transfer syntax UIDs in priority order
 */
[[nodiscard]] std::vector<std::string> get_mg_transfer_syntaxes();

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * @brief Check if laterality and view position are consistent
 *
 * Validates that the laterality and view position make clinical sense
 * (e.g., bilateral laterality shouldn't be used with single-breast views).
 *
 * @param laterality The breast laterality
 * @param view The view position
 * @return true if the combination is clinically valid
 */
[[nodiscard]] bool is_valid_laterality_view_combination(
    breast_laterality laterality,
    mg_view_position view) noexcept;

/**
 * @brief Get standard four-view screening exam views
 *
 * Returns the standard four views for screening mammography:
 * - Right CC, Left CC, Right MLO, Left MLO
 *
 * @return Vector of (laterality, view) pairs
 */
[[nodiscard]] std::vector<std::pair<breast_laterality, mg_view_position>>
get_standard_screening_views() noexcept;

/**
 * @brief Create DICOM-compliant image type value for mammography
 *
 * Constructs the Image Type (0008,0008) value for mammography images
 * according to DICOM specifications.
 *
 * @param is_original true if ORIGINAL, false if DERIVED
 * @param is_primary true if PRIMARY, false if SECONDARY
 * @param type For Presentation or For Processing
 * @return Properly formatted Image Type value
 */
[[nodiscard]] std::string create_mg_image_type(
    bool is_original,
    bool is_primary,
    mg_image_type type);

}  // namespace pacs::services::sop_classes

#endif  // PACS_SERVICES_SOP_CLASSES_MG_STORAGE_HPP
