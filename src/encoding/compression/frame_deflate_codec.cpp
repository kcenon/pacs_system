// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include "pacs/encoding/compression/frame_deflate_codec.hpp"
#include <pacs/core/result.hpp>

#include <algorithm>
#include <cstring>

#ifdef PACS_WITH_ZLIB
#include <zlib.h>
#endif

namespace kcenon::pacs::encoding::compression {

frame_deflate_codec::frame_deflate_codec(int compression_level)
    : compression_level_(
          std::clamp(compression_level,
                     kMinCompressionLevel,
                     kMaxCompressionLevel)) {}

frame_deflate_codec::~frame_deflate_codec() = default;

frame_deflate_codec::frame_deflate_codec(frame_deflate_codec&&) noexcept = default;
frame_deflate_codec& frame_deflate_codec::operator=(frame_deflate_codec&&) noexcept = default;

std::string_view frame_deflate_codec::transfer_syntax_uid() const noexcept {
    return kTransferSyntaxUID;
}

std::string_view frame_deflate_codec::name() const noexcept {
    return "Frame Deflate";
}

bool frame_deflate_codec::is_lossy() const noexcept {
    return false;
}

bool frame_deflate_codec::can_encode(const image_params& params) const noexcept {
    if (params.width == 0 || params.height == 0) {
        return false;
    }

    // Frame Deflate supports grayscale only (optimized for segmentation masks)
    if (params.samples_per_pixel != 1) {
        return false;
    }

    // Support 1-bit through 16-bit
    if (params.bits_stored < 1 || params.bits_stored > 16) {
        return false;
    }

    return true;
}

bool frame_deflate_codec::can_decode(const image_params& params) const noexcept {
    return can_encode(params);
}

int frame_deflate_codec::compression_level() const noexcept {
    return compression_level_;
}

#ifdef PACS_WITH_ZLIB

codec_result frame_deflate_codec::encode(
    std::span<const uint8_t> pixel_data,
    const image_params& params,
    [[maybe_unused]] const compression_options& options) const {

    if (pixel_data.empty()) {
        return kcenon::pacs::pacs_error<compression_result>(
            kcenon::pacs::error_codes::compression_error, "Empty pixel data");
    }

    if (params.width == 0 || params.height == 0) {
        return kcenon::pacs::pacs_error<compression_result>(
            kcenon::pacs::error_codes::compression_error, "Invalid image dimensions");
    }

    // Calculate expected data size
    const size_t expected_size = params.frame_size_bytes();
    if (pixel_data.size() < expected_size) {
        return kcenon::pacs::pacs_error<compression_result>(
            kcenon::pacs::error_codes::compression_error,
            "Pixel data too small: expected " + std::to_string(expected_size)
            + " bytes, got " + std::to_string(pixel_data.size()));
    }

    // Estimate compressed output size (zlib recommends deflateBound)
    uLong source_len = static_cast<uLong>(expected_size);
    uLong bound = compressBound(source_len);
    std::vector<uint8_t> compressed(bound);

    uLong dest_len = bound;
    int ret = compress2(compressed.data(), &dest_len,
                        pixel_data.data(), source_len,
                        compression_level_);

    if (ret != Z_OK) {
        return kcenon::pacs::pacs_error<compression_result>(
            kcenon::pacs::error_codes::compression_error,
            "zlib compress2 failed with error code: " + std::to_string(ret));
    }

    compressed.resize(dest_len);

    return kcenon::pacs::ok<compression_result>(
        compression_result{std::move(compressed), params});
}

codec_result frame_deflate_codec::decode(
    std::span<const uint8_t> compressed_data,
    const image_params& params) const {

    if (compressed_data.empty()) {
        return kcenon::pacs::pacs_error<compression_result>(
            kcenon::pacs::error_codes::decompression_error, "Empty compressed data");
    }

    // Calculate expected decompressed size
    const size_t expected_size = params.frame_size_bytes();
    if (expected_size == 0) {
        return kcenon::pacs::pacs_error<compression_result>(
            kcenon::pacs::error_codes::decompression_error,
            "Cannot determine output size from image parameters");
    }

    std::vector<uint8_t> decompressed(expected_size);
    uLong dest_len = static_cast<uLong>(expected_size);
    uLong source_len = static_cast<uLong>(compressed_data.size());

    int ret = uncompress(decompressed.data(), &dest_len,
                         compressed_data.data(), source_len);

    if (ret != Z_OK) {
        return kcenon::pacs::pacs_error<compression_result>(
            kcenon::pacs::error_codes::decompression_error,
            "zlib uncompress failed with error code: " + std::to_string(ret));
    }

    if (dest_len != expected_size) {
        return kcenon::pacs::pacs_error<compression_result>(
            kcenon::pacs::error_codes::decompression_error,
            "Decompressed size mismatch: expected " + std::to_string(expected_size)
            + " bytes, got " + std::to_string(dest_len));
    }

    return kcenon::pacs::ok<compression_result>(
        compression_result{std::move(decompressed), params});
}

#else  // !PACS_WITH_ZLIB

codec_result frame_deflate_codec::encode(
    [[maybe_unused]] std::span<const uint8_t> pixel_data,
    [[maybe_unused]] const image_params& params,
    [[maybe_unused]] const compression_options& options) const {
    return kcenon::pacs::pacs_error<compression_result>(
        kcenon::pacs::error_codes::compression_error,
        "Frame Deflate codec not available: zlib library not found at build time");
}

codec_result frame_deflate_codec::decode(
    [[maybe_unused]] std::span<const uint8_t> compressed_data,
    [[maybe_unused]] const image_params& params) const {
    return kcenon::pacs::pacs_error<compression_result>(
        kcenon::pacs::error_codes::decompression_error,
        "Frame Deflate codec not available: zlib library not found at build time");
}

#endif  // PACS_WITH_ZLIB

}  // namespace kcenon::pacs::encoding::compression
