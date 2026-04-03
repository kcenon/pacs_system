// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include "kcenon/pacs/encoding/compression/codec_factory.h"
#include "kcenon/pacs/encoding/compression/frame_deflate_codec.h"
#include "kcenon/pacs/encoding/compression/hevc_codec.h"
#include "kcenon/pacs/encoding/compression/htj2k_codec.h"
#include "kcenon/pacs/encoding/compression/jpeg_baseline_codec.h"
#include "kcenon/pacs/encoding/compression/jpeg_lossless_codec.h"
#include "kcenon/pacs/encoding/compression/jpeg2000_codec.h"
#include "kcenon/pacs/encoding/compression/jpeg_ls_codec.h"
#include "kcenon/pacs/encoding/compression/jpegxl_codec.h"
#include "kcenon/pacs/encoding/compression/rle_codec.h"

#include <array>

namespace kcenon::pacs::encoding::compression {

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

}  // namespace kcenon::pacs::encoding::compression
