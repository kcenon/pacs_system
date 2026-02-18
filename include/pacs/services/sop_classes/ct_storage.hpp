/**
 * @file ct_storage.hpp
 * @brief CT Image Storage SOP Classes
 *
 * Provides SOP Class definitions and utilities for Computed Tomography (CT)
 * image storage. Supports both standard CT Image Storage and Enhanced CT
 * Image Storage.
 *
 * @see DICOM PS3.4 Section B - Storage Service Class
 * @see DICOM PS3.3 Section A.3 - CT Image IOD
 * @see Issue #717 - Add CT Image IOD Validator
 */

#ifndef PACS_SERVICES_SOP_CLASSES_CT_STORAGE_HPP
#define PACS_SERVICES_SOP_CLASSES_CT_STORAGE_HPP

#include <string>
#include <string_view>
#include <vector>

namespace pacs::services::sop_classes {

// =============================================================================
// CT Storage SOP Class UIDs
// =============================================================================

/// CT Image Storage SOP Class UID
inline constexpr std::string_view ct_image_storage_uid =
    "1.2.840.10008.5.1.4.1.1.2";

/// Enhanced CT Image Storage SOP Class UID
inline constexpr std::string_view enhanced_ct_image_storage_uid =
    "1.2.840.10008.5.1.4.1.1.2.1";

// =============================================================================
// CT SOP Class Utilities
// =============================================================================

/**
 * @brief Get all CT Storage SOP Class UIDs
 * @return Vector of CT Storage SOP Class UIDs
 */
[[nodiscard]] std::vector<std::string> get_ct_storage_sop_classes();

/**
 * @brief Check if a SOP Class UID is a CT Storage SOP Class
 * @param uid The SOP Class UID to check
 * @return true if this is a CT storage SOP class
 */
[[nodiscard]] bool is_ct_storage_sop_class(std::string_view uid) noexcept;

/**
 * @brief Check if photometric interpretation is valid for CT
 *
 * CT images are always grayscale (MONOCHROME1 or MONOCHROME2).
 *
 * @param value The photometric interpretation string
 * @return true if valid for CT images
 */
[[nodiscard]] bool is_valid_ct_photometric(std::string_view value) noexcept;

}  // namespace pacs::services::sop_classes

#endif  // PACS_SERVICES_SOP_CLASSES_CT_STORAGE_HPP
