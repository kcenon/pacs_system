/**
 * @file itk_adapter.hpp
 * @brief ITK/VTK integration adapter for dicom_viewer
 *
 * This file provides adapter functions for converting pacs_system DICOM data
 * structures to ITK image types, enabling dicom_viewer to use pacs_system
 * for DICOM parsing while leveraging ITK/VTK for image processing.
 *
 * @see Issue #463 - ITK/VTK Integration Adapter for dicom_viewer
 * @see DICOM PS3.3 - Information Object Definitions (Image Pixel Module)
 */

#pragma once

#include <pacs/core/dicom_dataset.hpp>
#include <pacs/core/dicom_file.hpp>
#include <pacs/core/dicom_tag_constants.hpp>
#include <pacs/core/result.hpp>

#ifdef PACS_WITH_ITK

#include <itkImage.h>
#include <itkImportImageFilter.h>
#include <itkRGBPixel.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pacs::integration::itk {

// ============================================================================
// Type Aliases
// ============================================================================

/// CT image type (signed 16-bit, 3D)
using ct_image_type = ::itk::Image<int16_t, 3>;

/// MR image type (unsigned 16-bit, 3D)
using mr_image_type = ::itk::Image<uint16_t, 3>;

/// Grayscale 2D image type
using grayscale_2d_type = ::itk::Image<uint16_t, 2>;

/// Grayscale 3D image type
using grayscale_3d_type = ::itk::Image<uint16_t, 3>;

/// Signed grayscale 3D image type
using signed_grayscale_3d_type = ::itk::Image<int16_t, 3>;

/// RGB pixel type
using rgb_pixel_type = ::itk::RGBPixel<uint8_t>;

/// RGB 2D image type
using rgb_2d_type = ::itk::Image<rgb_pixel_type, 2>;

// ============================================================================
// Image Metadata
// ============================================================================

/**
 * @brief Image metadata extracted from DICOM dataset
 *
 * Contains spatial information required for ITK image construction:
 * - Origin from Image Position Patient (0020,0032)
 * - Spacing from Pixel Spacing (0028,0030) and Slice Thickness (0018,0050)
 * - Orientation from Image Orientation Patient (0020,0037)
 * - Dimensions from Columns (0028,0011), Rows (0028,0010), and frame count
 */
struct image_metadata {
    /// Image origin in patient coordinates (mm)
    std::array<double, 3> origin{0.0, 0.0, 0.0};

    /// Pixel spacing (row, column, slice) in mm
    std::array<double, 3> spacing{1.0, 1.0, 1.0};

    /// Image orientation cosines (row direction x3, column direction x3)
    std::array<double, 6> orientation{1.0, 0.0, 0.0, 0.0, 1.0, 0.0};

    /// Image dimensions (columns, rows, slices)
    std::array<size_t, 3> dimensions{0, 0, 1};

    /// Rescale slope for Hounsfield Unit conversion (default: 1.0)
    double rescale_slope{1.0};

    /// Rescale intercept for Hounsfield Unit conversion (default: 0.0)
    double rescale_intercept{0.0};

    /// Bits allocated per pixel
    uint16_t bits_allocated{16};

    /// Bits stored per pixel
    uint16_t bits_stored{16};

    /// High bit position
    uint16_t high_bit{15};

    /// Pixel representation (0 = unsigned, 1 = signed)
    uint16_t pixel_representation{0};

    /// Samples per pixel (1 = grayscale, 3 = RGB)
    uint16_t samples_per_pixel{1};

    /// Photometric interpretation
    std::string photometric_interpretation{"MONOCHROME2"};
};

/**
 * @brief Slice information for series sorting
 *
 * Used to sort DICOM files within a series for correct volume assembly.
 */
struct slice_info {
    /// File path
    std::filesystem::path file_path;

    /// Z position from Image Position Patient
    double z_position{0.0};

    /// Instance number from Instance Number (0020,0013)
    int32_t instance_number{0};

    /// Slice location from Slice Location (0020,1041)
    double slice_location{0.0};
};

// ============================================================================
// Metadata Extraction
// ============================================================================

/**
 * @brief Extract image metadata from a DICOM dataset
 *
 * Extracts all spatial and pixel information required for ITK image
 * construction from a DICOM dataset.
 *
 * @param dataset The DICOM dataset to extract metadata from
 * @return Image metadata structure
 *
 * @note Missing tags use sensible defaults (1.0 for spacing, 0.0 for origin)
 */
[[nodiscard]] auto extract_metadata(const core::dicom_dataset& dataset)
    -> image_metadata;

/**
 * @brief Sort DICOM files for volume assembly
 *
 * Sorts DICOM files by their spatial position to ensure correct
 * slice ordering in the resulting 3D volume.
 *
 * Sorting priority:
 * 1. Image Position Patient Z coordinate
 * 2. Slice Location (fallback)
 * 3. Instance Number (final fallback)
 *
 * @param files List of DICOM file paths
 * @return Sorted slice information
 *
 * @throws std::runtime_error if files cannot be read
 */
[[nodiscard]] auto sort_slices(const std::vector<std::filesystem::path>& files)
    -> std::vector<slice_info>;

// ============================================================================
// Single Dataset Conversion
// ============================================================================

/**
 * @brief Convert a DICOM dataset to an ITK image
 *
 * Creates an ITK image from a single DICOM dataset. The pixel type
 * and dimension must be specified as template parameters.
 *
 * @tparam TPixel Pixel type (e.g., int16_t, uint16_t, uint8_t)
 * @tparam VDimension Image dimension (2 or 3)
 * @param dataset DICOM dataset containing pixel data
 * @return Result containing ITK image smart pointer or error
 *
 * @example
 * @code
 * auto result = dicom_file::open("ct_slice.dcm");
 * if (result.is_ok()) {
 *     auto image = dataset_to_image<int16_t, 2>(result->dataset());
 *     if (image.is_ok()) {
 *         // Use ITK image...
 *     }
 * }
 * @endcode
 *
 * @note Pixel data is copied to ITK-managed buffer
 * @see DICOM PS3.3 C.7.6.3 - Image Pixel Module
 */
template <typename TPixel, unsigned int VDimension>
[[nodiscard]] auto dataset_to_image(const core::dicom_dataset& dataset)
    -> pacs::Result<typename ::itk::Image<TPixel, VDimension>::Pointer>;

// ============================================================================
// Series Conversion
// ============================================================================

/**
 * @brief Convert a DICOM series to a 3D ITK image
 *
 * Loads multiple DICOM files from a series and assembles them into
 * a single 3D ITK volume. Files are automatically sorted by spatial
 * position.
 *
 * @tparam TPixel Pixel type (e.g., int16_t for CT, uint16_t for MR)
 * @param files List of DICOM file paths in the series
 * @return Result containing 3D ITK image smart pointer or error
 *
 * @example
 * @code
 * std::vector<std::filesystem::path> series_files = scan_directory("ct_series/");
 * auto volume = series_to_image<int16_t>(series_files);
 * if (volume.is_ok()) {
 *     auto size = volume.value()->GetLargestPossibleRegion().GetSize();
 *     std::cout << "Volume: " << size[0] << "x" << size[1] << "x" << size[2] << "\n";
 * }
 * @endcode
 *
 * @note Slice spacing is calculated from position differences
 * @note All files must have compatible dimensions and pixel format
 */
template <typename TPixel>
[[nodiscard]] auto series_to_image(
    const std::vector<std::filesystem::path>& files)
    -> pacs::Result<typename ::itk::Image<TPixel, 3>::Pointer>;

// ============================================================================
// Pixel Data Utilities
// ============================================================================

/**
 * @brief Get raw pixel data from a DICOM dataset
 *
 * Extracts the pixel data element (7FE0,0010) as raw bytes.
 * Handles both uncompressed and decompressed pixel data.
 *
 * @param dataset DICOM dataset containing pixel data
 * @return Result containing pixel data bytes or error
 */
[[nodiscard]] auto get_pixel_data(const core::dicom_dataset& dataset)
    -> pacs::Result<std::vector<uint8_t>>;

/**
 * @brief Check if pixel data is signed
 *
 * @param dataset DICOM dataset to check
 * @return true if Pixel Representation (0028,0103) indicates signed data
 */
[[nodiscard]] auto is_signed_pixel_data(const core::dicom_dataset& dataset)
    -> bool;

/**
 * @brief Check if image is a multi-frame image
 *
 * @param dataset DICOM dataset to check
 * @return true if Number of Frames (0028,0008) > 1
 */
[[nodiscard]] auto is_multi_frame(const core::dicom_dataset& dataset) -> bool;

/**
 * @brief Get the number of frames in the image
 *
 * @param dataset DICOM dataset to check
 * @return Number of frames (1 for single-frame images)
 */
[[nodiscard]] auto get_frame_count(const core::dicom_dataset& dataset)
    -> uint32_t;

// ============================================================================
// Hounsfield Unit Conversion
// ============================================================================

/**
 * @brief Apply Hounsfield Unit (HU) conversion to CT image data
 *
 * Converts stored pixel values to Hounsfield Units using the formula:
 *   HU = pixel_value * RescaleSlope + RescaleIntercept
 *
 * @param pixel_data Raw pixel data buffer
 * @param pixel_count Number of pixels
 * @param slope Rescale Slope (0028,1053)
 * @param intercept Rescale Intercept (0028,1052)
 *
 * @note Modifies pixel_data in place
 * @note Only applies to 16-bit signed data (CT images)
 */
void apply_hounsfield_conversion(std::span<int16_t> pixel_data,
                                  double slope,
                                  double intercept);

// ============================================================================
// Convenience Functions
// ============================================================================

/**
 * @brief Load a CT series as a 3D volume
 *
 * Convenience function that loads a CT series and returns a properly
 * typed CT image (signed 16-bit, 3D).
 *
 * @param directory Directory containing DICOM files
 * @return Result containing CT volume or error
 */
[[nodiscard]] auto load_ct_series(const std::filesystem::path& directory)
    -> pacs::Result<ct_image_type::Pointer>;

/**
 * @brief Load an MR series as a 3D volume
 *
 * Convenience function that loads an MR series and returns a properly
 * typed MR image (unsigned 16-bit, 3D).
 *
 * @param directory Directory containing DICOM files
 * @return Result containing MR volume or error
 */
[[nodiscard]] auto load_mr_series(const std::filesystem::path& directory)
    -> pacs::Result<mr_image_type::Pointer>;

/**
 * @brief Scan a directory for DICOM files
 *
 * Recursively scans a directory and returns paths to all valid DICOM files.
 *
 * @param directory Directory to scan
 * @return List of DICOM file paths
 */
[[nodiscard]] auto scan_dicom_directory(const std::filesystem::path& directory)
    -> std::vector<std::filesystem::path>;

/**
 * @brief Group DICOM files by Series Instance UID
 *
 * Organizes DICOM files into groups based on their Series Instance UID.
 *
 * @param files List of DICOM file paths
 * @return Map of Series Instance UID to file paths
 */
[[nodiscard]] auto group_by_series(
    const std::vector<std::filesystem::path>& files)
    -> std::map<std::string, std::vector<std::filesystem::path>>;

}  // namespace pacs::integration::itk

#endif  // PACS_WITH_ITK
