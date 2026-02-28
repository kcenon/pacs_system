// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "pacs/encoding/compression/htj2k_codec.hpp"

namespace pacs::encoding::compression {

htj2k_codec::htj2k_codec(bool lossless,
                           bool use_rpcl,
                           float compression_ratio,
                           int resolution_levels)
    : lossless_(lossless),
      use_rpcl_(use_rpcl),
      compression_ratio_(compression_ratio),
      resolution_levels_(resolution_levels) {}

htj2k_codec::~htj2k_codec() = default;

htj2k_codec::htj2k_codec(htj2k_codec&&) noexcept = default;
htj2k_codec& htj2k_codec::operator=(htj2k_codec&&) noexcept = default;

std::string_view htj2k_codec::transfer_syntax_uid() const noexcept {
    if (lossless_) {
        return use_rpcl_ ? kTransferSyntaxUIDRPCL : kTransferSyntaxUIDLossless;
    }
    return kTransferSyntaxUIDLossy;
}

std::string_view htj2k_codec::name() const noexcept {
    if (lossless_) {
        return use_rpcl_ ? "HTJ2K with RPCL (Lossless)" : "HTJ2K (Lossless)";
    }
    return "HTJ2K (Lossy)";
}

bool htj2k_codec::is_lossy() const noexcept {
    return !lossless_;
}

bool htj2k_codec::can_encode(const image_params& params) const noexcept {
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

bool htj2k_codec::can_decode(const image_params& params) const noexcept {
    return can_encode(params);
}

bool htj2k_codec::is_lossless_mode() const noexcept {
    return lossless_;
}

bool htj2k_codec::is_rpcl_mode() const noexcept {
    return use_rpcl_;
}

float htj2k_codec::compression_ratio() const noexcept {
    return compression_ratio_;
}

int htj2k_codec::resolution_levels() const noexcept {
    return resolution_levels_;
}

codec_result htj2k_codec::encode(
    [[maybe_unused]] std::span<const uint8_t> pixel_data,
    [[maybe_unused]] const image_params& params,
    [[maybe_unused]] const compression_options& options) const {
    return pacs::pacs_error<compression_result>(
        pacs::error_codes::compression_error,
        "HTJ2K encode not yet implemented: OpenJPH library integration required (see #785)");
}

codec_result htj2k_codec::decode(
    [[maybe_unused]] std::span<const uint8_t> compressed_data,
    [[maybe_unused]] const image_params& params) const {
    return pacs::pacs_error<compression_result>(
        pacs::error_codes::decompression_error,
        "HTJ2K decode not yet implemented: OpenJPH library integration required (see #785)");
}

}  // namespace pacs::encoding::compression
