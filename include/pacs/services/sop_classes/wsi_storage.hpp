// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file wsi_storage.hpp
 * @brief VL Whole Slide Microscopy Image Storage SOP Class
 *
 * Provides SOP Class definitions and utilities for Whole Slide Microscopy
 * (WSI) image storage. WSI images are gigapixel-scale, tiled multi-frame
 * images used in digital pathology.
 *
 * @see DICOM PS3.3 Section A.32.8 - VL Whole Slide Microscopy Image IOD
 * @see DICOM PS3.3 Section C.8.12.4 - Whole Slide Microscopy Image Module
 * @see Issue #825 - Add WSI SOP Class registration and storage definition
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_SERVICES_SOP_CLASSES_WSI_STORAGE_HPP
#define PACS_SERVICES_SOP_CLASSES_WSI_STORAGE_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace kcenon::pacs::services::sop_classes {

// =============================================================================
// WSI Storage SOP Class UIDs
// =============================================================================

/// VL Whole Slide Microscopy Image Storage SOP Class UID
inline constexpr std::string_view wsi_image_storage_uid =
    "1.2.840.10008.5.1.4.1.1.77.1.6";

// =============================================================================
// WSI DICOM Tags (group 0x0048 — Whole Slide Microscopy)
// =============================================================================

namespace wsi_tags {

/// Imaged Volume Width (0048,0001) — physical width of scanned region in mm
inline constexpr uint32_t imaged_volume_width = 0x00480001;

/// Imaged Volume Height (0048,0002) — physical height of scanned region in mm
inline constexpr uint32_t imaged_volume_height = 0x00480002;

/// Imaged Volume Depth (0048,0003) — physical depth (Z-stack) in mm
inline constexpr uint32_t imaged_volume_depth = 0x00480003;

/// Total Pixel Matrix Columns (0048,0006) — total width across all tiles
inline constexpr uint32_t total_pixel_matrix_columns = 0x00480006;

/// Total Pixel Matrix Rows (0048,0007) — total height across all tiles
inline constexpr uint32_t total_pixel_matrix_rows = 0x00480007;

/// Total Pixel Matrix Focal Planes (0048,0008) — number of focal planes
inline constexpr uint32_t total_pixel_matrix_focal_planes = 0x00480008;

/// Image Orientation (Slide) (0048,0102)
inline constexpr uint32_t image_orientation_slide = 0x00480102;

/// Optical Path Identifier (0048,0105)
inline constexpr uint32_t optical_path_identifier = 0x00480105;

/// Optical Path Description (0048,0106)
inline constexpr uint32_t optical_path_description = 0x00480106;

/// Specimen Identifier (0040,0551)
inline constexpr uint32_t specimen_identifier = 0x00400551;

/// Container Identifier (0040,0512)
inline constexpr uint32_t container_identifier = 0x00400512;

/// Dimension Organization Type (0020,9311)
inline constexpr uint32_t dimension_organization_type = 0x00209311;

}  // namespace wsi_tags

// =============================================================================
// WSI SOP Class Utilities
// =============================================================================

/**
 * @brief Get all WSI Storage SOP Class UIDs
 * @return Vector of WSI Storage SOP Class UIDs
 */
[[nodiscard]] std::vector<std::string> get_wsi_storage_sop_classes();

/**
 * @brief Check if a SOP Class UID is a WSI Storage SOP Class
 * @param uid The SOP Class UID to check
 * @return true if this is a WSI storage SOP class
 */
[[nodiscard]] bool is_wsi_storage_sop_class(std::string_view uid) noexcept;

/**
 * @brief Check if photometric interpretation is valid for WSI
 *
 * WSI images support RGB (brightfield), YBR variants (compressed),
 * and MONOCHROME2 (fluorescence single-channel).
 *
 * @param value The photometric interpretation string
 * @return true if valid for WSI images
 */
[[nodiscard]] bool is_valid_wsi_photometric(std::string_view value) noexcept;

}  // namespace kcenon::pacs::services::sop_classes

#endif  // PACS_SERVICES_SOP_CLASSES_WSI_STORAGE_HPP
