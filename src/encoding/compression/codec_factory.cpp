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

#include "pacs/encoding/compression/codec_factory.hpp"
#include "pacs/encoding/compression/frame_deflate_codec.hpp"
#include "pacs/encoding/compression/hevc_codec.hpp"
#include "pacs/encoding/compression/htj2k_codec.hpp"
#include "pacs/encoding/compression/jpeg_baseline_codec.hpp"
#include "pacs/encoding/compression/jpeg_lossless_codec.hpp"
#include "pacs/encoding/compression/jpeg2000_codec.hpp"
#include "pacs/encoding/compression/jpeg_ls_codec.hpp"
#include "pacs/encoding/compression/jpegxl_codec.hpp"
#include "pacs/encoding/compression/rle_codec.hpp"

#include <array>

namespace pacs::encoding::compression {

namespace {

/**
 * @brief List of supported Transfer Syntax UIDs for compression codecs.
 *
 * As more codecs are implemented (RLE, etc.),
 * they should be added to this list.
 */
static constexpr std::array<std::string_view, 16> kSupportedTransferSyntaxes = {{
    rle_codec::kTransferSyntaxUID,                   // 1.2.840.10008.1.2.5
    jpeg_baseline_codec::kTransferSyntaxUID,         // 1.2.840.10008.1.2.4.50
    jpeg_lossless_codec::kTransferSyntaxUID,         // 1.2.840.10008.1.2.4.70
    jpeg_ls_codec::kTransferSyntaxUIDLossless,       // 1.2.840.10008.1.2.4.80
    jpeg_ls_codec::kTransferSyntaxUIDNearLossless,   // 1.2.840.10008.1.2.4.81
    jpeg2000_codec::kTransferSyntaxUIDLossless,      // 1.2.840.10008.1.2.4.90
    jpeg2000_codec::kTransferSyntaxUIDLossy,         // 1.2.840.10008.1.2.4.91
    hevc_codec::kTransferSyntaxUIDMain,              // 1.2.840.10008.1.2.4.107
    hevc_codec::kTransferSyntaxUIDMain10,            // 1.2.840.10008.1.2.4.108
    jpegxl_codec::kTransferSyntaxUIDLossless,        // 1.2.840.10008.1.2.4.110
    jpegxl_codec::kTransferSyntaxUIDJPEGRecompression, // 1.2.840.10008.1.2.4.111
    jpegxl_codec::kTransferSyntaxUIDLossy,           // 1.2.840.10008.1.2.4.112
    htj2k_codec::kTransferSyntaxUIDLossless,         // 1.2.840.10008.1.2.4.201
    htj2k_codec::kTransferSyntaxUIDRPCL,             // 1.2.840.10008.1.2.4.202
    htj2k_codec::kTransferSyntaxUIDLossy,            // 1.2.840.10008.1.2.4.203
    frame_deflate_codec::kTransferSyntaxUID,          // 1.2.840.10008.1.2.11
}};

}  // namespace

std::unique_ptr<compression_codec> codec_factory::create(
    std::string_view transfer_syntax_uid) {

    // RLE Lossless (1.2.840.10008.1.2.5)
    if (transfer_syntax_uid == rle_codec::kTransferSyntaxUID) {
        return std::make_unique<rle_codec>();
    }

    // JPEG Baseline (Process 1)
    if (transfer_syntax_uid == jpeg_baseline_codec::kTransferSyntaxUID) {
        return std::make_unique<jpeg_baseline_codec>();
    }

    // JPEG Lossless (Process 14, Selection Value 1)
    if (transfer_syntax_uid == jpeg_lossless_codec::kTransferSyntaxUID) {
        return std::make_unique<jpeg_lossless_codec>();
    }

    // JPEG-LS Lossless (1.2.840.10008.1.2.4.80)
    if (transfer_syntax_uid == jpeg_ls_codec::kTransferSyntaxUIDLossless) {
        return std::make_unique<jpeg_ls_codec>(true);  // lossless = true
    }

    // JPEG-LS Near-Lossless (1.2.840.10008.1.2.4.81)
    if (transfer_syntax_uid == jpeg_ls_codec::kTransferSyntaxUIDNearLossless) {
        return std::make_unique<jpeg_ls_codec>(false);  // lossless = false
    }

    // JPEG 2000 Lossless Only (1.2.840.10008.1.2.4.90)
    if (transfer_syntax_uid == jpeg2000_codec::kTransferSyntaxUIDLossless) {
        return std::make_unique<jpeg2000_codec>(true);  // lossless = true
    }

    // JPEG 2000 (1.2.840.10008.1.2.4.91) - can be lossy or lossless
    if (transfer_syntax_uid == jpeg2000_codec::kTransferSyntaxUIDLossy) {
        return std::make_unique<jpeg2000_codec>(false);  // lossless = false (default lossy)
    }

    // HEVC/H.265 Main Profile (1.2.840.10008.1.2.4.107)
    if (transfer_syntax_uid == hevc_codec::kTransferSyntaxUIDMain) {
        return std::make_unique<hevc_codec>(false);  // main10 = false
    }

    // HEVC/H.265 Main 10 Profile (1.2.840.10008.1.2.4.108)
    if (transfer_syntax_uid == hevc_codec::kTransferSyntaxUIDMain10) {
        return std::make_unique<hevc_codec>(true);  // main10 = true
    }

    // JPEG XL Lossless (1.2.840.10008.1.2.4.110)
    if (transfer_syntax_uid == jpegxl_codec::kTransferSyntaxUIDLossless) {
        return std::make_unique<jpegxl_codec>(true, false);
    }

    // JPEG XL JPEG Recompression (1.2.840.10008.1.2.4.111)
    if (transfer_syntax_uid == jpegxl_codec::kTransferSyntaxUIDJPEGRecompression) {
        return std::make_unique<jpegxl_codec>(true, true);
    }

    // JPEG XL (1.2.840.10008.1.2.4.112) - lossy
    if (transfer_syntax_uid == jpegxl_codec::kTransferSyntaxUIDLossy) {
        return std::make_unique<jpegxl_codec>(false);
    }

    // HTJ2K Lossless Only (1.2.840.10008.1.2.4.201)
    if (transfer_syntax_uid == htj2k_codec::kTransferSyntaxUIDLossless) {
        return std::make_unique<htj2k_codec>(true, false);
    }

    // HTJ2K with RPCL Options (1.2.840.10008.1.2.4.202)
    if (transfer_syntax_uid == htj2k_codec::kTransferSyntaxUIDRPCL) {
        return std::make_unique<htj2k_codec>(true, true);
    }

    // HTJ2K (1.2.840.10008.1.2.4.203) - lossy
    if (transfer_syntax_uid == htj2k_codec::kTransferSyntaxUIDLossy) {
        return std::make_unique<htj2k_codec>(false);
    }

    // Frame Deflate (1.2.840.10008.1.2.11)
    if (transfer_syntax_uid == frame_deflate_codec::kTransferSyntaxUID) {
        return std::make_unique<frame_deflate_codec>();
    }

    return nullptr;
}

std::unique_ptr<compression_codec> codec_factory::create(
    const transfer_syntax& ts) {
    return create(ts.uid());
}

std::vector<std::string_view> codec_factory::supported_transfer_syntaxes() {
    return {kSupportedTransferSyntaxes.begin(), kSupportedTransferSyntaxes.end()};
}

bool codec_factory::is_supported(std::string_view transfer_syntax_uid) {
    for (const auto& uid : kSupportedTransferSyntaxes) {
        if (uid == transfer_syntax_uid) {
            return true;
        }
    }
    return false;
}

bool codec_factory::is_supported(const transfer_syntax& ts) {
    return is_supported(ts.uid());
}

}  // namespace pacs::encoding::compression
