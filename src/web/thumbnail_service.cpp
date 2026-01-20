/**
 * @file thumbnail_service.cpp
 * @brief Thumbnail generation service implementation
 *
 * @see Issue #543 - Implement Thumbnail API for DICOM Viewer
 * @copyright Copyright (c) 2025
 * @license MIT
 */

#include "pacs/web/thumbnail_service.hpp"

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_element.hpp"
#include "pacs/core/dicom_file.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/storage/index_database.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>

#ifdef PACS_JPEG_FOUND
#include <jpeglib.h>
#endif

#ifdef PACS_PNG_FOUND
#include <png.h>
#endif

namespace pacs::web {

// =============================================================================
// Construction / Destruction
// =============================================================================

thumbnail_service::thumbnail_service(
    std::shared_ptr<storage::index_database> database)
    : database_(std::move(database)) {}

thumbnail_service::~thumbnail_service() = default;

// =============================================================================
// Thumbnail Generation
// =============================================================================

thumbnail_result thumbnail_service::get_thumbnail(
    std::string_view sop_instance_uid, const thumbnail_params& params) {
    // Validate parameters
    if (params.size != 64 && params.size != 128 && params.size != 256 &&
        params.size != 512) {
        return thumbnail_result::error("Invalid size: must be 64, 128, 256, or 512");
    }

    if (params.format != "jpeg" && params.format != "png") {
        return thumbnail_result::error("Invalid format: must be jpeg or png");
    }

    if (params.quality < 1 || params.quality > 100) {
        return thumbnail_result::error("Invalid quality: must be 1-100");
    }

    // Check database is available
    if (database_ == nullptr) {
        return thumbnail_result::error("Database not configured");
    }

    // Build cache key
    cache_key key{
        std::string(sop_instance_uid), params.size, params.format,
        params.quality, params.frame};

    // Check cache first (read lock)
    {
        std::shared_lock lock(cache_mutex_);
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            // Update last accessed time
            const_cast<thumbnail_cache_entry&>(it->second).last_accessed =
                std::chrono::system_clock::now();
            return thumbnail_result::ok(it->second);
        }
    }

    // Find instance in database
    auto instance = database_->find_instance(sop_instance_uid);
    if (!instance) {
        return thumbnail_result::error("Instance not found");
    }

    // Check if file exists
    if (!std::filesystem::exists(instance->file_path)) {
        return thumbnail_result::error("DICOM file not found");
    }

    // Generate thumbnail
    auto thumbnail_data = generate_thumbnail(instance->file_path, params);
    if (thumbnail_data.empty()) {
        return thumbnail_result::error("Failed to generate thumbnail");
    }

    // Create cache entry
    thumbnail_cache_entry entry;
    entry.data = std::move(thumbnail_data);
    entry.content_type = get_content_type(params.format);
    entry.created_at = std::chrono::system_clock::now();
    entry.last_accessed = entry.created_at;

    // Store in cache (write lock)
    {
        std::unique_lock lock(cache_mutex_);

        // Check if we need to evict entries
        while (current_cache_size_ + entry.data.size() > max_cache_size_ &&
               !cache_.empty()) {
            evict_lru();
        }

        current_cache_size_ += entry.data.size();
        cache_[key] = entry;
    }

    return thumbnail_result::ok(std::move(entry));
}

thumbnail_result thumbnail_service::get_series_thumbnail(
    std::string_view series_uid, const thumbnail_params& params) {
    auto sop_uid = select_representative_instance(series_uid);
    if (!sop_uid) {
        return thumbnail_result::error("No instances found in series");
    }

    return get_thumbnail(*sop_uid, params);
}

thumbnail_result thumbnail_service::get_study_thumbnail(
    std::string_view study_uid, const thumbnail_params& params) {
    auto series_uid = select_representative_series(study_uid);
    if (!series_uid) {
        return thumbnail_result::error("No series found in study");
    }

    return get_series_thumbnail(*series_uid, params);
}

// =============================================================================
// Cache Management
// =============================================================================

void thumbnail_service::clear_cache() {
    std::unique_lock lock(cache_mutex_);
    cache_.clear();
    current_cache_size_ = 0;
}

void thumbnail_service::clear_cache(std::string_view sop_instance_uid) {
    std::unique_lock lock(cache_mutex_);

    // Remove all entries for this instance (any size/format/quality)
    for (auto it = cache_.begin(); it != cache_.end();) {
        if (it->first.uid == sop_instance_uid) {
            current_cache_size_ -= it->second.data.size();
            it = cache_.erase(it);
        } else {
            ++it;
        }
    }
}

size_t thumbnail_service::cache_size() const {
    std::shared_lock lock(cache_mutex_);
    return current_cache_size_;
}

size_t thumbnail_service::cache_entry_count() const {
    std::shared_lock lock(cache_mutex_);
    return cache_.size();
}

void thumbnail_service::set_max_cache_size(size_t max_bytes) {
    std::unique_lock lock(cache_mutex_);
    max_cache_size_ = max_bytes;

    // Evict if over limit
    while (current_cache_size_ > max_cache_size_ && !cache_.empty()) {
        evict_lru();
    }
}

size_t thumbnail_service::max_cache_size() const {
    std::shared_lock lock(cache_mutex_);
    return max_cache_size_;
}

// =============================================================================
// Internal Methods
// =============================================================================

std::vector<uint8_t> thumbnail_service::generate_thumbnail(
    std::string_view file_path, const thumbnail_params& params) {
    using namespace pacs::core;

    // Open DICOM file
    auto result = dicom_file::open(std::filesystem::path(file_path));
    if (result.is_err()) {
        return {};
    }

    auto& file = result.value();
    const auto& dataset = file.dataset();

    // Get image dimensions
    auto rows = dataset.get_numeric<uint16_t>(tags::rows);
    auto columns = dataset.get_numeric<uint16_t>(tags::columns);

    if (!rows || !columns || *rows == 0 || *columns == 0) {
        return {};
    }

    // Get bits information
    auto bits_allocated =
        dataset.get_numeric<uint16_t>(dicom_tag{0x0028, 0x0100});
    auto bits_stored = dataset.get_numeric<uint16_t>(dicom_tag{0x0028, 0x0101});
    auto samples_per_pixel = dataset.get_numeric<uint16_t>(tags::samples_per_pixel);
    auto pixel_representation =
        dataset.get_numeric<uint16_t>(dicom_tag{0x0028, 0x0103});

    if (!bits_allocated || !bits_stored) {
        return {};
    }

    uint16_t spp = samples_per_pixel.value_or(1);
    uint16_t repr = pixel_representation.value_or(0);

    // Get photometric interpretation
    auto photometric = dataset.get_string(tags::photometric_interpretation);

    // Get pixel data
    auto* pixel_element = dataset.get(tags::pixel_data);
    if (pixel_element == nullptr) {
        return {};
    }

    auto raw_data = pixel_element->raw_data();
    if (raw_data.empty()) {
        return {};
    }

    // Calculate frame offset for multi-frame images
    size_t frame_size = static_cast<size_t>(*rows) * (*columns) * spp *
                        ((*bits_allocated + 7) / 8);

    uint32_t frame_index = params.frame > 0 ? params.frame - 1 : 0;
    size_t frame_offset = frame_index * frame_size;

    if (frame_offset + frame_size > raw_data.size()) {
        // Fall back to first frame
        frame_offset = 0;
    }

    // Convert to 8-bit grayscale or RGB
    std::vector<uint8_t> pixels;
    size_t num_pixels = static_cast<size_t>(*rows) * (*columns) * spp;

    if (*bits_allocated == 16) {
        // 16-bit to 8-bit conversion with auto window/level
        int min_val = 65535;
        int max_val = -65536;

        // Find min/max for windowing
        for (size_t i = 0; i < num_pixels; ++i) {
            size_t idx = frame_offset + i * 2;
            if (idx + 1 >= raw_data.size()) break;

            int16_t pixel;
            if (repr == 0) {
                pixel = static_cast<int16_t>(raw_data[idx] |
                                             (raw_data[idx + 1] << 8));
            } else {
                pixel = static_cast<int16_t>(raw_data[idx] |
                                             (raw_data[idx + 1] << 8));
            }
            min_val = std::min(min_val, static_cast<int>(pixel));
            max_val = std::max(max_val, static_cast<int>(pixel));
        }

        double window_width = static_cast<double>(max_val - min_val);
        double window_center = static_cast<double>(min_val + max_val) / 2.0;
        if (window_width < 1) window_width = 1;

        pixels.resize(num_pixels);
        for (size_t i = 0; i < num_pixels; ++i) {
            size_t idx = frame_offset + i * 2;
            if (idx + 1 >= raw_data.size()) break;

            int16_t pixel = static_cast<int16_t>(raw_data[idx] |
                                                 (raw_data[idx + 1] << 8));

            double lower = window_center - window_width / 2.0;
            double upper = window_center + window_width / 2.0;

            if (pixel <= lower) {
                pixels[i] = 0;
            } else if (pixel >= upper) {
                pixels[i] = 255;
            } else {
                pixels[i] = static_cast<uint8_t>(
                    ((pixel - lower) / window_width) * 255.0);
            }
        }
    } else {
        // 8-bit: direct copy
        pixels.resize(num_pixels);
        for (size_t i = 0; i < num_pixels; ++i) {
            size_t idx = frame_offset + i;
            if (idx < raw_data.size()) {
                pixels[i] = raw_data[idx];
            }
        }
    }

    // Handle MONOCHROME1 inversion
    if (photometric == "MONOCHROME1") {
        for (auto& p : pixels) {
            p = static_cast<uint8_t>(255 - p);
        }
    }

    // Resize to target size
    uint16_t src_width = *columns;
    uint16_t src_height = *rows;
    uint16_t dst_size = params.size;

    // Calculate destination dimensions maintaining aspect ratio
    uint16_t dst_width, dst_height;
    if (src_width > src_height) {
        dst_width = dst_size;
        dst_height =
            static_cast<uint16_t>(static_cast<float>(src_height) / src_width * dst_size);
    } else {
        dst_height = dst_size;
        dst_width =
            static_cast<uint16_t>(static_cast<float>(src_width) / src_height * dst_size);
    }

    if (dst_width == 0) dst_width = 1;
    if (dst_height == 0) dst_height = 1;

    // Simple bilinear resize
    std::vector<uint8_t> resized(static_cast<size_t>(dst_width) * dst_height * spp);

    float x_ratio = static_cast<float>(src_width) / dst_width;
    float y_ratio = static_cast<float>(src_height) / dst_height;

    for (uint16_t y = 0; y < dst_height; ++y) {
        for (uint16_t x = 0; x < dst_width; ++x) {
            float src_x = x * x_ratio;
            float src_y = y * y_ratio;

            int x0 = static_cast<int>(src_x);
            int y0 = static_cast<int>(src_y);
            int x1 = std::min(x0 + 1, static_cast<int>(src_width - 1));
            int y1 = std::min(y0 + 1, static_cast<int>(src_height - 1));

            float x_diff = src_x - x0;
            float y_diff = src_y - y0;

            for (uint16_t c = 0; c < spp; ++c) {
                size_t idx00 = (static_cast<size_t>(y0) * src_width + x0) * spp + c;
                size_t idx01 = (static_cast<size_t>(y0) * src_width + x1) * spp + c;
                size_t idx10 = (static_cast<size_t>(y1) * src_width + x0) * spp + c;
                size_t idx11 = (static_cast<size_t>(y1) * src_width + x1) * spp + c;

                float v00 = idx00 < pixels.size() ? pixels[idx00] : 0;
                float v01 = idx01 < pixels.size() ? pixels[idx01] : 0;
                float v10 = idx10 < pixels.size() ? pixels[idx10] : 0;
                float v11 = idx11 < pixels.size() ? pixels[idx11] : 0;

                float value = v00 * (1 - x_diff) * (1 - y_diff) +
                              v01 * x_diff * (1 - y_diff) +
                              v10 * (1 - x_diff) * y_diff +
                              v11 * x_diff * y_diff;

                size_t dst_idx =
                    (static_cast<size_t>(y) * dst_width + x) * spp + c;
                resized[dst_idx] = static_cast<uint8_t>(std::clamp(value, 0.0f, 255.0f));
            }
        }
    }

    // Encode to output format
    std::vector<uint8_t> output;

    if (params.format == "jpeg") {
#ifdef PACS_JPEG_FOUND
        // JPEG encoding
        struct jpeg_compress_struct cinfo {};
        struct jpeg_error_mgr jerr {};

        cinfo.err = jpeg_std_error(&jerr);
        jpeg_create_compress(&cinfo);

        // Memory destination
        unsigned char* outbuffer = nullptr;
        unsigned long outsize = 0;
        jpeg_mem_dest(&cinfo, &outbuffer, &outsize);

        cinfo.image_width = dst_width;
        cinfo.image_height = dst_height;
        cinfo.input_components = static_cast<int>(spp);
        cinfo.in_color_space = (spp == 1) ? JCS_GRAYSCALE : JCS_RGB;

        jpeg_set_defaults(&cinfo);
        jpeg_set_quality(&cinfo, params.quality, TRUE);
        jpeg_start_compress(&cinfo, TRUE);

        JSAMPROW row_pointer[1];
        int row_stride = dst_width * spp;

        while (cinfo.next_scanline < cinfo.image_height) {
            row_pointer[0] = &resized[cinfo.next_scanline * static_cast<size_t>(row_stride)];
            jpeg_write_scanlines(&cinfo, row_pointer, 1);
        }

        jpeg_finish_compress(&cinfo);
        jpeg_destroy_compress(&cinfo);

        if (outbuffer != nullptr && outsize > 0) {
            output.assign(outbuffer, outbuffer + outsize);
            free(outbuffer);
        }
#else
        // Fallback: return empty if JPEG not available
        return {};
#endif
    } else if (params.format == "png") {
#ifdef PACS_PNG_FOUND
        // PNG encoding using libpng memory I/O
        struct png_mem_buffer {
            std::vector<uint8_t> data;
        };

        auto write_callback = [](png_structp png_ptr, png_bytep data,
                                 png_size_t length) {
            auto* buffer =
                static_cast<png_mem_buffer*>(png_get_io_ptr(png_ptr));
            buffer->data.insert(buffer->data.end(), data, data + length);
        };

        auto flush_callback = [](png_structp /*png_ptr*/) {
            // No-op for memory buffer
        };

        png_mem_buffer buffer;

        png_structp png_ptr = png_create_write_struct(
            PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
        if (png_ptr == nullptr) {
            return {};
        }

        png_infop info_ptr = png_create_info_struct(png_ptr);
        if (info_ptr == nullptr) {
            png_destroy_write_struct(&png_ptr, nullptr);
            return {};
        }

        if (setjmp(png_jmpbuf(png_ptr))) {
            png_destroy_write_struct(&png_ptr, &info_ptr);
            return {};
        }

        png_set_write_fn(png_ptr, &buffer, write_callback, flush_callback);

        int color_type = (spp == 1) ? PNG_COLOR_TYPE_GRAY : PNG_COLOR_TYPE_RGB;
        png_set_IHDR(png_ptr, info_ptr, dst_width, dst_height, 8, color_type,
                     PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                     PNG_FILTER_TYPE_DEFAULT);

        png_write_info(png_ptr, info_ptr);

        int row_stride = dst_width * spp;
        for (uint16_t y = 0; y < dst_height; ++y) {
            png_bytep row = &resized[static_cast<size_t>(y) * row_stride];
            png_write_row(png_ptr, row);
        }

        png_write_end(png_ptr, nullptr);
        png_destroy_write_struct(&png_ptr, &info_ptr);

        output = std::move(buffer.data);
#else
        // Fallback: return empty if PNG not available
        return {};
#endif
    }

    return output;
}

std::optional<std::string> thumbnail_service::select_representative_instance(
    std::string_view series_uid) {
    auto instances_result = database_->list_instances(series_uid);
    if (instances_result.is_err() || instances_result.value().empty()) {
        return std::nullopt;
    }

    const auto& instances = instances_result.value();

    // Sort by instance number and select middle instance
    std::vector<std::pair<int, std::string>> sorted;
    for (const auto& inst : instances) {
        int num = inst.instance_number.value_or(1);
        sorted.emplace_back(num, inst.sop_uid);
    }

    std::sort(sorted.begin(), sorted.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    // Select middle instance
    size_t middle = sorted.size() / 2;
    return sorted[middle].second;
}

std::optional<std::string> thumbnail_service::select_representative_series(
    std::string_view study_uid) {
    auto series_result = database_->list_series(study_uid);
    if (series_result.is_err() || series_result.value().empty()) {
        return std::nullopt;
    }

    const auto& series_list = series_result.value();

    // Prefer series with images (CT, MR, CR, DX, etc.)
    // Use first series with most instances as representative
    const pacs::storage::series_record* best = nullptr;
    for (const auto& s : series_list) {
        // Skip structured reports, KOS, etc.
        if (s.modality == "SR" || s.modality == "KO" || s.modality == "PR") {
            continue;
        }

        if (best == nullptr || s.num_instances > best->num_instances) {
            best = &s;
        }
    }

    if (best == nullptr && !series_list.empty()) {
        // Fall back to first series
        best = &series_list[0];
    }

    return best ? std::make_optional(best->series_uid) : std::nullopt;
}

void thumbnail_service::evict_lru() {
    // Find least recently used entry
    auto lru_it = cache_.end();
    auto oldest_time = std::chrono::system_clock::time_point::max();

    for (auto it = cache_.begin(); it != cache_.end(); ++it) {
        if (it->second.last_accessed < oldest_time) {
            oldest_time = it->second.last_accessed;
            lru_it = it;
        }
    }

    if (lru_it != cache_.end()) {
        current_cache_size_ -= lru_it->second.data.size();
        cache_.erase(lru_it);
    }
}

std::string thumbnail_service::get_content_type(std::string_view format) {
    if (format == "jpeg") {
        return "image/jpeg";
    } else if (format == "png") {
        return "image/png";
    }
    return "application/octet-stream";
}

}  // namespace pacs::web
