/**
 * @file itk_adapter.cpp
 * @brief ITK/VTK integration adapter implementation
 *
 * Implementation of adapter functions for converting pacs_system DICOM data
 * structures to ITK image types.
 *
 * @see Issue #463 - ITK/VTK Integration Adapter for dicom_viewer
 */

#include <pacs/integration/itk_adapter.hpp>

#ifdef PACS_WITH_ITK

#include <pacs/core/dicom_element.hpp>
#include <pacs/core/dicom_file.hpp>
#include <pacs/core/dicom_tag_constants.hpp>

#include <itkImageRegionIterator.h>

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>
#include <sstream>
#include <stdexcept>

namespace pacs::integration::itk {

namespace {

/**
 * @brief Parse decimal string (DS) values from DICOM
 *
 * DS VR contains one or more decimal values separated by backslash.
 *
 * @param ds_string The decimal string to parse
 * @return Vector of parsed double values
 */
[[nodiscard]] auto parse_ds_values(std::string_view ds_string)
    -> std::vector<double> {
    std::vector<double> values;

    if (ds_string.empty()) {
        return values;
    }

    std::string str(ds_string);
    std::istringstream stream(str);
    std::string token;

    while (std::getline(stream, token, '\\')) {
        if (!token.empty()) {
            try {
                // Trim whitespace
                size_t start = token.find_first_not_of(" \t");
                size_t end = token.find_last_not_of(" \t");
                if (start != std::string::npos && end != std::string::npos) {
                    token = token.substr(start, end - start + 1);
                }
                values.push_back(std::stod(token));
            } catch (const std::exception&) {
                // Skip invalid values
            }
        }
    }

    return values;
}

/**
 * @brief Calculate slice spacing from sorted slice positions
 *
 * @param slices Sorted slice information
 * @return Calculated slice spacing in mm
 */
[[nodiscard]] auto calculate_slice_spacing(const std::vector<slice_info>& slices)
    -> double {
    if (slices.size() < 2) {
        return 1.0;  // Default for single slice
    }

    // Calculate average spacing from adjacent slices
    double total_spacing = 0.0;
    size_t count = 0;

    for (size_t i = 1; i < slices.size(); ++i) {
        double spacing = std::abs(slices[i].z_position - slices[i - 1].z_position);
        if (spacing > 0.001) {  // Skip near-zero spacings
            total_spacing += spacing;
            ++count;
        }
    }

    return (count > 0) ? (total_spacing / static_cast<double>(count)) : 1.0;
}

}  // namespace

// ============================================================================
// Metadata Extraction
// ============================================================================

auto extract_metadata(const core::dicom_dataset& dataset) -> image_metadata {
    image_metadata meta{};

    // Dimensions
    meta.dimensions[0] =
        dataset.get_numeric<uint16_t>(core::tags::columns).value_or(0);
    meta.dimensions[1] =
        dataset.get_numeric<uint16_t>(core::tags::rows).value_or(0);
    meta.dimensions[2] = 1;  // Single slice; caller updates for series

    // Pixel Spacing (row spacing, column spacing)
    auto pixel_spacing =
        dataset.get_string(core::tags::pixel_spacing);
    if (!pixel_spacing.empty()) {
        auto values = parse_ds_values(pixel_spacing);
        if (values.size() >= 2) {
            meta.spacing[0] = values[1];  // Column spacing (X)
            meta.spacing[1] = values[0];  // Row spacing (Y)
        }
    }

    // Slice Thickness
    auto slice_thickness =
        dataset.get_string(core::tags::slice_location);
    // Use Slice Thickness tag (0018,0050) if available
    // Note: slice_location is a different tag, we need to check for thickness
    meta.spacing[2] = 1.0;  // Will be calculated from series positions

    // Image Position Patient (origin)
    auto position =
        dataset.get_string(core::tags::image_position_patient);
    if (!position.empty()) {
        auto values = parse_ds_values(position);
        if (values.size() >= 3) {
            meta.origin[0] = values[0];
            meta.origin[1] = values[1];
            meta.origin[2] = values[2];
        }
    }

    // Image Orientation Patient (direction cosines)
    auto orientation =
        dataset.get_string(core::tags::image_orientation_patient);
    if (!orientation.empty()) {
        auto values = parse_ds_values(orientation);
        if (values.size() >= 6) {
            std::copy_n(values.begin(), 6, meta.orientation.begin());
        }
    }

    // Rescale parameters for HU conversion
    auto rescale_slope = dataset.get_string(core::tags::rescale_slope);
    if (!rescale_slope.empty()) {
        auto values = parse_ds_values(rescale_slope);
        if (!values.empty()) {
            meta.rescale_slope = values[0];
        }
    }

    auto rescale_intercept =
        dataset.get_string(core::tags::rescale_intercept);
    if (!rescale_intercept.empty()) {
        auto values = parse_ds_values(rescale_intercept);
        if (!values.empty()) {
            meta.rescale_intercept = values[0];
        }
    }

    // Pixel format information
    meta.bits_allocated =
        dataset.get_numeric<uint16_t>(core::tags::bits_allocated).value_or(16);
    meta.bits_stored =
        dataset.get_numeric<uint16_t>(core::tags::bits_stored)
            .value_or(meta.bits_allocated);
    meta.high_bit =
        dataset.get_numeric<uint16_t>(core::tags::high_bit)
            .value_or(meta.bits_stored - 1);
    meta.pixel_representation =
        dataset.get_numeric<uint16_t>(core::tags::pixel_representation)
            .value_or(0);
    meta.samples_per_pixel =
        dataset.get_numeric<uint16_t>(core::tags::samples_per_pixel).value_or(1);
    meta.photometric_interpretation =
        dataset.get_string(core::tags::photometric_interpretation, "MONOCHROME2");

    return meta;
}

auto sort_slices(const std::vector<std::filesystem::path>& files)
    -> std::vector<slice_info> {
    std::vector<slice_info> slices;
    slices.reserve(files.size());

    for (const auto& file : files) {
        auto result = core::dicom_file::open(file);
        if (!result.is_ok()) {
            continue;  // Skip unreadable files
        }

        const auto& dataset = result.value().dataset();

        slice_info info;
        info.file_path = file;

        // Try Image Position Patient Z coordinate first
        auto position =
            dataset.get_string(core::tags::image_position_patient);
        if (!position.empty()) {
            auto values = parse_ds_values(position);
            if (values.size() >= 3) {
                info.z_position = values[2];
            }
        }

        // Fallback to Slice Location
        auto slice_location = dataset.get_string(core::tags::slice_location);
        if (!slice_location.empty()) {
            auto values = parse_ds_values(slice_location);
            if (!values.empty()) {
                info.slice_location = values[0];
                if (position.empty()) {
                    info.z_position = info.slice_location;
                }
            }
        }

        // Instance Number for secondary sorting
        info.instance_number =
            dataset.get_numeric<int32_t>(core::tags::instance_number)
                .value_or(0);

        slices.push_back(info);
    }

    // Sort by Z position, then by instance number
    std::sort(slices.begin(), slices.end(),
              [](const slice_info& a, const slice_info& b) {
                  constexpr double kPositionTolerance = 0.001;
                  if (std::abs(a.z_position - b.z_position) > kPositionTolerance) {
                      return a.z_position < b.z_position;
                  }
                  return a.instance_number < b.instance_number;
              });

    return slices;
}

// ============================================================================
// Single Dataset Conversion
// ============================================================================

template <typename TPixel, unsigned int VDimension>
auto dataset_to_image(const core::dicom_dataset& dataset)
    -> pacs::Result<typename ::itk::Image<TPixel, VDimension>::Pointer> {
    using ImageType = ::itk::Image<TPixel, VDimension>;
    using ImportFilterType = ::itk::ImportImageFilter<TPixel, VDimension>;

    // Extract metadata
    auto meta = extract_metadata(dataset);

    if (meta.dimensions[0] == 0 || meta.dimensions[1] == 0) {
        return pacs::make_error(-800, "Invalid image dimensions");
    }

    // Get pixel data
    auto pixel_result = get_pixel_data(dataset);
    if (!pixel_result.is_ok()) {
        return pacs::make_error(pixel_result.error().code(),
                                pixel_result.error().message());
    }

    const auto& pixel_data = pixel_result.value();
    if (pixel_data.empty()) {
        return pacs::make_error(-801, "Empty pixel data");
    }

    // Create import filter
    auto import_filter = ImportFilterType::New();

    // Set region
    typename ImageType::SizeType size;
    typename ImageType::IndexType start;
    start.Fill(0);

    for (unsigned int i = 0; i < VDimension; ++i) {
        size[i] = (i < 3) ? meta.dimensions[i] : 1;
    }

    typename ImageType::RegionType region;
    region.SetSize(size);
    region.SetIndex(start);
    import_filter->SetRegion(region);

    // Set spacing
    typename ImageType::SpacingType spacing;
    for (unsigned int i = 0; i < VDimension; ++i) {
        spacing[i] = (i < 3) ? meta.spacing[i] : 1.0;
    }
    import_filter->SetSpacing(spacing);

    // Set origin
    typename ImageType::PointType origin;
    for (unsigned int i = 0; i < VDimension; ++i) {
        origin[i] = (i < 3) ? meta.origin[i] : 0.0;
    }
    import_filter->SetOrigin(origin);

    // Set direction from orientation cosines
    if constexpr (VDimension >= 2) {
        typename ImageType::DirectionType direction;
        direction.SetIdentity();

        // Row direction
        direction[0][0] = meta.orientation[0];
        direction[1][0] = meta.orientation[1];
        if constexpr (VDimension >= 3) {
            direction[2][0] = meta.orientation[2];
        }

        // Column direction
        direction[0][1] = meta.orientation[3];
        direction[1][1] = meta.orientation[4];
        if constexpr (VDimension >= 3) {
            direction[2][1] = meta.orientation[5];

            // Calculate slice direction as cross product
            direction[0][2] = meta.orientation[1] * meta.orientation[5] -
                              meta.orientation[2] * meta.orientation[4];
            direction[1][2] = meta.orientation[2] * meta.orientation[3] -
                              meta.orientation[0] * meta.orientation[5];
            direction[2][2] = meta.orientation[0] * meta.orientation[4] -
                              meta.orientation[1] * meta.orientation[3];
        }

        import_filter->SetDirection(direction);
    }

    // Calculate expected pixel count
    size_t num_pixels = 1;
    for (unsigned int i = 0; i < VDimension; ++i) {
        num_pixels *= size[i];
    }

    // Allocate buffer and copy pixel data
    // ITK takes ownership of the buffer (last parameter = true)
    auto* buffer = new TPixel[num_pixels];

    const size_t expected_bytes = num_pixels * sizeof(TPixel);
    const size_t copy_bytes = std::min(pixel_data.size(), expected_bytes);
    std::memcpy(buffer, pixel_data.data(), copy_bytes);

    // Zero-fill any remaining buffer space
    if (copy_bytes < expected_bytes) {
        std::memset(reinterpret_cast<uint8_t*>(buffer) + copy_bytes, 0,
                    expected_bytes - copy_bytes);
    }

    import_filter->SetImportPointer(buffer, num_pixels, true);

    try {
        import_filter->Update();
    } catch (const ::itk::ExceptionObject& e) {
        return pacs::make_error(-802,
                                std::string("ITK import failed: ") + e.what());
    }

    return import_filter->GetOutput();
}

// ============================================================================
// Series Conversion
// ============================================================================

template <typename TPixel>
auto series_to_image(const std::vector<std::filesystem::path>& files)
    -> pacs::Result<typename ::itk::Image<TPixel, 3>::Pointer> {
    using ImageType = ::itk::Image<TPixel, 3>;

    if (files.empty()) {
        return pacs::make_error(-803, "No files provided for series");
    }

    // Sort slices
    auto sorted = sort_slices(files);
    if (sorted.empty()) {
        return pacs::make_error(-804, "No readable DICOM files in series");
    }

    // Read first file for metadata
    auto first_result = core::dicom_file::open(sorted[0].file_path);
    if (!first_result.is_ok()) {
        return pacs::make_error(-805, "Failed to read first DICOM file");
    }

    auto meta = extract_metadata(first_result.value().dataset());
    meta.dimensions[2] = sorted.size();

    // Calculate slice spacing from positions
    meta.spacing[2] = calculate_slice_spacing(sorted);

    // Allocate output image
    auto image = ImageType::New();

    typename ImageType::SizeType size;
    size[0] = meta.dimensions[0];
    size[1] = meta.dimensions[1];
    size[2] = meta.dimensions[2];

    typename ImageType::RegionType region;
    region.SetSize(size);
    image->SetRegions(region);

    typename ImageType::SpacingType spacing;
    spacing[0] = meta.spacing[0];
    spacing[1] = meta.spacing[1];
    spacing[2] = meta.spacing[2];
    image->SetSpacing(spacing);

    typename ImageType::PointType origin;
    origin[0] = meta.origin[0];
    origin[1] = meta.origin[1];
    origin[2] = sorted[0].z_position;
    image->SetOrigin(origin);

    // Set direction from orientation cosines
    typename ImageType::DirectionType direction;
    direction.SetIdentity();
    direction[0][0] = meta.orientation[0];
    direction[1][0] = meta.orientation[1];
    direction[2][0] = meta.orientation[2];
    direction[0][1] = meta.orientation[3];
    direction[1][1] = meta.orientation[4];
    direction[2][1] = meta.orientation[5];
    direction[0][2] = meta.orientation[1] * meta.orientation[5] -
                      meta.orientation[2] * meta.orientation[4];
    direction[1][2] = meta.orientation[2] * meta.orientation[3] -
                      meta.orientation[0] * meta.orientation[5];
    direction[2][2] = meta.orientation[0] * meta.orientation[4] -
                      meta.orientation[1] * meta.orientation[3];
    image->SetDirection(direction);

    try {
        image->Allocate();
    } catch (const ::itk::ExceptionObject& e) {
        return pacs::make_error(-806,
                                std::string("Failed to allocate volume: ") + e.what());
    }

    // Read each slice
    const size_t slice_size = size[0] * size[1];
    TPixel* buffer = image->GetBufferPointer();

    for (size_t z = 0; z < sorted.size(); ++z) {
        auto file_result = core::dicom_file::open(sorted[z].file_path);
        if (!file_result.is_ok()) {
            return pacs::make_error(
                -807, "Failed to read DICOM file: " + sorted[z].file_path.string());
        }

        auto pixel_result = get_pixel_data(file_result.value().dataset());
        if (!pixel_result.is_ok()) {
            return pacs::make_error(-808, "Failed to get pixel data from: " +
                                              sorted[z].file_path.string());
        }

        const auto& pixel_data = pixel_result.value();
        const size_t copy_bytes =
            std::min(pixel_data.size(), slice_size * sizeof(TPixel));
        std::memcpy(buffer + z * slice_size, pixel_data.data(), copy_bytes);
    }

    return image;
}

// ============================================================================
// Pixel Data Utilities
// ============================================================================

auto get_pixel_data(const core::dicom_dataset& dataset)
    -> pacs::Result<std::vector<uint8_t>> {
    const auto* pixel_element = dataset.get(core::tags::pixel_data);
    if (pixel_element == nullptr) {
        return pacs::make_error(-810, "Pixel data element not found");
    }

    auto raw_data = pixel_element->raw_data();
    if (raw_data.empty()) {
        return pacs::make_error(-811, "Pixel data is empty");
    }

    // Copy to vector
    std::vector<uint8_t> result(raw_data.size());
    std::memcpy(result.data(), raw_data.data(), raw_data.size());

    return result;
}

auto is_signed_pixel_data(const core::dicom_dataset& dataset) -> bool {
    return dataset.get_numeric<uint16_t>(core::tags::pixel_representation)
               .value_or(0) == 1;
}

auto is_multi_frame(const core::dicom_dataset& dataset) -> bool {
    return get_frame_count(dataset) > 1;
}

auto get_frame_count(const core::dicom_dataset& dataset) -> uint32_t {
    // Number of Frames tag (0028,0008)
    constexpr core::dicom_tag number_of_frames{0x0028, 0x0008};

    auto frames_str = dataset.get_string(number_of_frames);
    if (frames_str.empty()) {
        return 1;
    }

    try {
        return static_cast<uint32_t>(std::stoul(frames_str));
    } catch (const std::exception&) {
        return 1;
    }
}

// ============================================================================
// Hounsfield Unit Conversion
// ============================================================================

void apply_hounsfield_conversion(std::span<int16_t> pixel_data,
                                  double slope,
                                  double intercept) {
    for (auto& pixel : pixel_data) {
        double hu_value = static_cast<double>(pixel) * slope + intercept;
        // Clamp to int16_t range
        hu_value = std::clamp(hu_value, static_cast<double>(INT16_MIN),
                              static_cast<double>(INT16_MAX));
        pixel = static_cast<int16_t>(std::round(hu_value));
    }
}

// ============================================================================
// Convenience Functions
// ============================================================================

auto load_ct_series(const std::filesystem::path& directory)
    -> pacs::Result<ct_image_type::Pointer> {
    auto files = scan_dicom_directory(directory);
    if (files.empty()) {
        return pacs::make_error(-820, "No DICOM files found in directory");
    }

    return series_to_image<int16_t>(files);
}

auto load_mr_series(const std::filesystem::path& directory)
    -> pacs::Result<mr_image_type::Pointer> {
    auto files = scan_dicom_directory(directory);
    if (files.empty()) {
        return pacs::make_error(-821, "No DICOM files found in directory");
    }

    return series_to_image<uint16_t>(files);
}

auto scan_dicom_directory(const std::filesystem::path& directory)
    -> std::vector<std::filesystem::path> {
    std::vector<std::filesystem::path> files;

    if (!std::filesystem::exists(directory) ||
        !std::filesystem::is_directory(directory)) {
        return files;
    }

    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(directory)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        // Quick DICOM file validation
        auto result = core::dicom_file::open(entry.path());
        if (result.is_ok()) {
            files.push_back(entry.path());
        }
    }

    return files;
}

auto group_by_series(const std::vector<std::filesystem::path>& files)
    -> std::map<std::string, std::vector<std::filesystem::path>> {
    std::map<std::string, std::vector<std::filesystem::path>> series_map;

    for (const auto& file : files) {
        auto result = core::dicom_file::open(file);
        if (!result.is_ok()) {
            continue;
        }

        auto series_uid =
            result.value().dataset().get_string(core::tags::series_instance_uid,
                                                 "unknown");
        series_map[series_uid].push_back(file);
    }

    return series_map;
}

// ============================================================================
// Explicit Template Instantiations
// ============================================================================

// 2D images
template auto dataset_to_image<uint8_t, 2>(const core::dicom_dataset&)
    -> pacs::Result<::itk::Image<uint8_t, 2>::Pointer>;
template auto dataset_to_image<uint16_t, 2>(const core::dicom_dataset&)
    -> pacs::Result<::itk::Image<uint16_t, 2>::Pointer>;
template auto dataset_to_image<int16_t, 2>(const core::dicom_dataset&)
    -> pacs::Result<::itk::Image<int16_t, 2>::Pointer>;

// 3D images
template auto dataset_to_image<uint8_t, 3>(const core::dicom_dataset&)
    -> pacs::Result<::itk::Image<uint8_t, 3>::Pointer>;
template auto dataset_to_image<uint16_t, 3>(const core::dicom_dataset&)
    -> pacs::Result<::itk::Image<uint16_t, 3>::Pointer>;
template auto dataset_to_image<int16_t, 3>(const core::dicom_dataset&)
    -> pacs::Result<::itk::Image<int16_t, 3>::Pointer>;

// Series to 3D volume
template auto series_to_image<uint8_t>(const std::vector<std::filesystem::path>&)
    -> pacs::Result<::itk::Image<uint8_t, 3>::Pointer>;
template auto series_to_image<uint16_t>(const std::vector<std::filesystem::path>&)
    -> pacs::Result<::itk::Image<uint16_t, 3>::Pointer>;
template auto series_to_image<int16_t>(const std::vector<std::filesystem::path>&)
    -> pacs::Result<::itk::Image<int16_t, 3>::Pointer>;

}  // namespace pacs::integration::itk

#endif  // PACS_WITH_ITK
