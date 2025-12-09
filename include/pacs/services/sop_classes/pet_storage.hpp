/**
 * @file pet_storage.hpp
 * @brief Positron Emission Tomography (PET) Image Storage SOP Classes
 *
 * This file provides SOP Class definitions and utilities for PET (PT)
 * image storage. Supports both current and enhanced SOP Classes.
 *
 * @see DICOM PS3.4 Section B - Storage Service Class
 * @see DICOM PS3.3 Section A.21 - PET Image IOD
 */

#ifndef PACS_SERVICES_SOP_CLASSES_PET_STORAGE_HPP
#define PACS_SERVICES_SOP_CLASSES_PET_STORAGE_HPP

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace pacs::services::sop_classes {

// =============================================================================
// PET Storage SOP Class UIDs
// =============================================================================

/// @name Primary PET SOP Classes
/// @{

/// PET Image Storage SOP Class UID
inline constexpr std::string_view pet_image_storage_uid =
    "1.2.840.10008.5.1.4.1.1.128";

/// Enhanced PET Image Storage SOP Class UID
inline constexpr std::string_view enhanced_pet_image_storage_uid =
    "1.2.840.10008.5.1.4.1.1.130";

/// Legacy Converted Enhanced PET Image Storage SOP Class UID
inline constexpr std::string_view legacy_converted_enhanced_pet_image_storage_uid =
    "1.2.840.10008.5.1.4.1.1.128.1";

/// @}

// =============================================================================
// PET-Specific Transfer Syntaxes
// =============================================================================

/**
 * @brief Get recommended transfer syntaxes for PET images
 *
 * Returns a prioritized list of transfer syntaxes suitable for
 * PET image storage, considering compression requirements and
 * quantitative accuracy.
 *
 * @return Vector of transfer syntax UIDs in priority order
 */
[[nodiscard]] std::vector<std::string> get_pet_transfer_syntaxes();

// =============================================================================
// PET Photometric Interpretations
// =============================================================================

/**
 * @brief Supported photometric interpretations for PET images
 *
 * PET images are typically grayscale, representing radiotracer
 * uptake values.
 */
enum class pet_photometric_interpretation {
    monochrome2     ///< Minimum pixel = black (standard for PET)
};

/**
 * @brief Convert photometric interpretation enum to DICOM string
 * @param interp The photometric interpretation
 * @return DICOM-compliant string representation
 */
[[nodiscard]] std::string_view to_string(pet_photometric_interpretation interp) noexcept;

/**
 * @brief Parse DICOM photometric interpretation string
 * @param value The DICOM string value
 * @return The corresponding enum value, or monochrome2 if unknown
 */
[[nodiscard]] pet_photometric_interpretation
parse_pet_photometric_interpretation(std::string_view value) noexcept;

/**
 * @brief Check if photometric interpretation is valid for PET
 * @param value The DICOM string value
 * @return true if this is a valid PET photometric interpretation
 */
[[nodiscard]] bool is_valid_pet_photometric(std::string_view value) noexcept;

// =============================================================================
// PET SOP Class Information
// =============================================================================

/**
 * @brief Information about a PET Storage SOP Class
 */
struct pet_sop_class_info {
    std::string_view uid;           ///< SOP Class UID
    std::string_view name;          ///< Human-readable name
    std::string_view description;   ///< Brief description
    bool is_retired;                ///< Whether this SOP class is retired
    bool supports_multiframe;       ///< Whether multi-frame is supported
    bool is_enhanced;               ///< Whether this is an enhanced SOP class
};

/**
 * @brief Get all PET Storage SOP Class UIDs
 *
 * Returns both current and legacy SOP Class UIDs for comprehensive
 * PET storage support.
 *
 * @param include_retired Include retired SOP classes (default: true)
 * @return Vector of PET Storage SOP Class UIDs
 */
[[nodiscard]] std::vector<std::string>
get_pet_storage_sop_classes(bool include_retired = true);

/**
 * @brief Get information about a specific PET SOP Class
 *
 * @param uid The SOP Class UID to look up
 * @return Pointer to SOP class info, or nullptr if not a PET storage class
 */
[[nodiscard]] const pet_sop_class_info*
get_pet_sop_class_info(std::string_view uid) noexcept;

/**
 * @brief Check if a SOP Class UID is a PET Storage SOP Class
 *
 * @param uid The SOP Class UID to check
 * @return true if this is any PET storage SOP class
 */
[[nodiscard]] bool is_pet_storage_sop_class(std::string_view uid) noexcept;

/**
 * @brief Check if a SOP Class UID is an Enhanced PET Storage SOP Class
 *
 * @param uid The SOP Class UID to check
 * @return true if this is an enhanced PET storage SOP class
 */
[[nodiscard]] bool is_enhanced_pet_sop_class(std::string_view uid) noexcept;

// =============================================================================
// PET Image Type Codes
// =============================================================================

/**
 * @brief PET image type indicating the nature of the image
 */
enum class pet_image_type {
    original,           ///< ORIGINAL - directly acquired
    derived,            ///< DERIVED - post-processed
    primary,            ///< PRIMARY - primary image
    secondary           ///< SECONDARY - secondary reconstruction
};

/**
 * @brief PET series type code
 */
enum class pet_series_type {
    static_image,       ///< STATIC - static acquisition
    dynamic,            ///< DYNAMIC - dynamic (time series)
    gated,              ///< GATED - cardiac/respiratory gated
    whole_body          ///< WHOLE BODY - whole body scan
};

/**
 * @brief Attenuation correction method
 */
enum class pet_attenuation_correction {
    none,               ///< No attenuation correction
    measured,           ///< Measured (transmission scan)
    calculated,         ///< Calculated from CT
    ct_based            ///< CT-based attenuation correction (PET/CT)
};

/**
 * @brief Scatter correction method
 */
enum class pet_scatter_correction {
    none,               ///< No scatter correction
    single_scatter,     ///< Single scatter simulation
    convolution,        ///< Convolution-subtraction method
    model_based         ///< Model-based scatter correction
};

/**
 * @brief PET reconstruction algorithm type
 */
enum class pet_reconstruction_type {
    fbp,                ///< Filtered Back Projection
    osem,               ///< Ordered Subset Expectation Maximization
    mlem,               ///< Maximum Likelihood Expectation Maximization
    tof_osem,           ///< Time-of-Flight OSEM
    psf_osem,           ///< Point Spread Function OSEM
    other               ///< Other algorithm
};

/**
 * @brief Convert PET reconstruction type to string
 * @param recon The reconstruction type
 * @return String representation
 */
[[nodiscard]] std::string_view to_string(pet_reconstruction_type recon) noexcept;

/**
 * @brief Parse PET reconstruction type from string
 * @param value The string value
 * @return The reconstruction type
 */
[[nodiscard]] pet_reconstruction_type
parse_pet_reconstruction_type(std::string_view value) noexcept;

// =============================================================================
// PET Units and SUV Calculation
// =============================================================================

/**
 * @brief PET units type (Units attribute 0054,1001)
 */
enum class pet_units {
    cnts,               ///< Counts
    bqml,               ///< Bq/ml (Becquerels per milliliter)
    gml,                ///< g/ml (SUVbw)
    suv_bw,             ///< Standardized Uptake Value (body weight)
    suv_lbm,            ///< SUV (lean body mass)
    suv_bsa,            ///< SUV (body surface area)
    percent_id_gram,    ///< Percent injected dose per gram
    other               ///< Other units
};

/**
 * @brief Convert PET units to DICOM string
 * @param units The units type
 * @return DICOM-compliant string representation
 */
[[nodiscard]] std::string_view to_string(pet_units units) noexcept;

/**
 * @brief Parse PET units from DICOM string
 * @param value The DICOM string value
 * @return The units type
 */
[[nodiscard]] pet_units parse_pet_units(std::string_view value) noexcept;

// =============================================================================
// PET Radiopharmaceutical Information
// =============================================================================

/**
 * @brief Common PET radiotracers
 */
enum class pet_radiotracer {
    fdg,                ///< 18F-FDG (Fluorodeoxyglucose)
    naf,                ///< 18F-NaF (Sodium Fluoride)
    flt,                ///< 18F-FLT (Fluorothymidine)
    fdopa,              ///< 18F-FDOPA
    ammonia,            ///< 13N-Ammonia
    rubidium,           ///< 82Rb (Rubidium-82)
    gallium_dotatate,   ///< 68Ga-DOTATATE
    psma,               ///< PSMA agents
    other               ///< Other tracers
};

/**
 * @brief Get radiotracer name string
 * @param tracer The radiotracer type
 * @return Human-readable name
 */
[[nodiscard]] std::string_view to_string(pet_radiotracer tracer) noexcept;

}  // namespace pacs::services::sop_classes

#endif  // PACS_SERVICES_SOP_CLASSES_PET_STORAGE_HPP
