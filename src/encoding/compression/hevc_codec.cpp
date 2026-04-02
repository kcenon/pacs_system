// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include "pacs/encoding/compression/hevc_codec.hpp"
#include <pacs/core/result.hpp>

namespace kcenon::pacs::encoding::compression {

hevc_codec::hevc_codec(bool main10, int quality)
    : main10_(main10),
      quality_(quality) {}

hevc_codec::~hevc_codec() = default;

hevc_codec::hevc_codec(hevc_codec&&) noexcept = default;
hevc_codec& hevc_codec::operator=(hevc_codec&&) noexcept = default;

std::string_view hevc_codec::transfer_syntax_uid() const noexcept {
    return main10_ ? kTransferSyntaxUIDMain10 : kTransferSyntaxUIDMain;
}

std::string_view hevc_codec::name() const noexcept {
    return main10_ ? "HEVC/H.265 Main 10 Profile" : "HEVC/H.265 Main Profile";
}

bool hevc_codec::is_lossy() const noexcept {
    return true;
}

bool hevc_codec::can_encode(const image_params& params) const noexcept {
    if (params.width == 0 || params.height == 0) {
        return false;
    }

    if (params.samples_per_pixel != 1 && params.samples_per_pixel != 3) {
        return false;
    }

    if (main10_) {
        // Main 10 Profile supports up to 10-bit
        if (params.bits_stored < 1 || params.bits_stored > 10) {
            return false;
        }
    } else {
        // Main Profile supports up to 8-bit
        if (params.bits_stored < 1 || params.bits_stored > 8) {
            return false;
        }
    }

    return true;
}

bool hevc_codec::can_decode(const image_params& params) const noexcept {
    return can_encode(params);
}

bool hevc_codec::is_main10_profile() const noexcept {
    return main10_;
}

int hevc_codec::quality() const noexcept {
    return quality_;
}

// HEVC library not yet integrated -- return not-available errors

codec_result hevc_codec::encode(
    [[maybe_unused]] std::span<const uint8_t> pixel_data,
    [[maybe_unused]] const image_params& params,
    [[maybe_unused]] const compression_options& options) const {
    return kcenon::pacs::pacs_error<compression_result>(
        kcenon::pacs::error_codes::compression_error,
        "HEVC codec not available: HEVC library not found at build time. "
        "Build with PACS_WITH_HEVC=ON to enable.");
}

codec_result hevc_codec::decode(
    [[maybe_unused]] std::span<const uint8_t> compressed_data,
    [[maybe_unused]] const image_params& params) const {
    return kcenon::pacs::pacs_error<compression_result>(
        kcenon::pacs::error_codes::decompression_error,
        "HEVC codec not available: HEVC library not found at build time. "
        "Build with PACS_WITH_HEVC=ON to enable.");
}

}  // namespace kcenon::pacs::encoding::compression
