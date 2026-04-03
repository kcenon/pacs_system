// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include "pacs/encoding/compression/jpegxl_codec.hpp"
#include <pacs/core/result.hpp>

namespace kcenon::pacs::encoding::compression {

jpegxl_codec::jpegxl_codec(bool lossless,
                           bool jpeg_recompression,
                           float quality_distance)
    : lossless_(lossless),
      jpeg_recompression_(jpeg_recompression),
      quality_distance_(quality_distance) {}

jpegxl_codec::~jpegxl_codec() = default;

jpegxl_codec::jpegxl_codec(jpegxl_codec&&) noexcept = default;
jpegxl_codec& jpegxl_codec::operator=(jpegxl_codec&&) noexcept = default;

std::string_view jpegxl_codec::transfer_syntax_uid() const noexcept {
    if (lossless_) {
        return jpeg_recompression_
            ? kTransferSyntaxUIDJPEGRecompression
            : kTransferSyntaxUIDLossless;
    }
    return kTransferSyntaxUIDLossy;
}

std::string_view jpegxl_codec::name() const noexcept {
    if (lossless_) {
        return jpeg_recompression_
            ? "JPEG XL JPEG Recompression"
            : "JPEG XL (Lossless)";
    }
    return "JPEG XL (Lossy)";
}

bool jpegxl_codec::is_lossy() const noexcept {
    return !lossless_;
}

bool jpegxl_codec::can_encode(const image_params& params) const noexcept {
    if (params.width == 0 || params.height == 0) {
        return false;
    }

    if (params.samples_per_pixel != 1 && params.samples_per_pixel != 3) {
        return false;
    }

    if (params.bits_stored < 1 || params.bits_stored > 16) {
        return false;
    }

    return true;
}

bool jpegxl_codec::can_decode(const image_params& params) const noexcept {
    return can_encode(params);
}

bool jpegxl_codec::is_lossless_mode() const noexcept {
    return lossless_;
}

bool jpegxl_codec::is_jpeg_recompression_mode() const noexcept {
    return jpeg_recompression_;
}

float jpegxl_codec::quality_distance() const noexcept {
    return quality_distance_;
}

// JPEG XL library not yet integrated -- return not-available errors

codec_result jpegxl_codec::encode(
    [[maybe_unused]] std::span<const uint8_t> pixel_data,
    [[maybe_unused]] const image_params& params,
    [[maybe_unused]] const compression_options& options) const {
    return kcenon::pacs::pacs_error<compression_result>(
        kcenon::pacs::error_codes::compression_error,
        "JPEG XL codec not available: libjxl library not found at build time. "
        "Build with PACS_WITH_JPEGXL=ON to enable.");
}

codec_result jpegxl_codec::decode(
    [[maybe_unused]] std::span<const uint8_t> compressed_data,
    [[maybe_unused]] const image_params& params) const {
    return kcenon::pacs::pacs_error<compression_result>(
        kcenon::pacs::error_codes::decompression_error,
        "JPEG XL codec not available: libjxl library not found at build time. "
        "Build with PACS_WITH_JPEGXL=ON to enable.");
}

}  // namespace kcenon::pacs::encoding::compression
