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

// Whether the codecs that need an optional library were built with it
// (cmake/targets.cmake defines these macros when CMake finds the library).
#ifdef PACS_WITH_JPEG_CODEC
constexpr bool kJpegBaselineBuilt = true;
#else
constexpr bool kJpegBaselineBuilt = false;
#endif

#ifdef PACS_WITH_JPEGLS_CODEC
constexpr bool kJpegLsBuilt = true;
#else
constexpr bool kJpegLsBuilt = false;
#endif

#ifdef PACS_WITH_JPEG2000_CODEC
constexpr bool kJpeg2000Built = true;
#else
constexpr bool kJpeg2000Built = false;
#endif

#ifdef PACS_WITH_HTJ2K_CODEC
constexpr bool kHtj2kBuilt = true;
#else
constexpr bool kHtj2kBuilt = false;
#endif

/**
 * @brief A Transfer Syntax that pacs_system has a codec for.
 */
struct codec_entry {
    std::string_view uid;
    bool available;  ///< true if this build can encode and decode with it
};

/**
 * @brief Transfer Syntaxes with an implemented codec.
 *
 * RLE and JPEG Lossless are built in; the others need an optional library.
 * hevc_codec, jpegxl_codec, and frame_deflate_codec are stubs whose encode()
 * and decode() always fail (frame_deflate_codec's zlib code needs
 * PACS_WITH_ZLIB, which no build defines), so they are not listed although
 * create() returns them.
 */
static constexpr std::array<codec_entry, 10> kImplementedCodecs = {{
    {rle_codec::kTransferSyntaxUID, true},                            // 1.2.840.10008.1.2.5
    {jpeg_baseline_codec::kTransferSyntaxUID, kJpegBaselineBuilt},    // 1.2.840.10008.1.2.4.50
    {jpeg_lossless_codec::kTransferSyntaxUID, true},                  // 1.2.840.10008.1.2.4.70
    {jpeg_ls_codec::kTransferSyntaxUIDLossless, kJpegLsBuilt},        // 1.2.840.10008.1.2.4.80
    {jpeg_ls_codec::kTransferSyntaxUIDNearLossless, kJpegLsBuilt},    // 1.2.840.10008.1.2.4.81
    {jpeg2000_codec::kTransferSyntaxUIDLossless, kJpeg2000Built},     // 1.2.840.10008.1.2.4.90
    {jpeg2000_codec::kTransferSyntaxUIDLossy, kJpeg2000Built},        // 1.2.840.10008.1.2.4.91
    {htj2k_codec::kTransferSyntaxUIDLossless, kHtj2kBuilt},           // 1.2.840.10008.1.2.4.201
    {htj2k_codec::kTransferSyntaxUIDRPCL, kHtj2kBuilt},               // 1.2.840.10008.1.2.4.202
    {htj2k_codec::kTransferSyntaxUIDLossy, kHtj2kBuilt},              // 1.2.840.10008.1.2.4.203
}};

const codec_entry* find_codec(std::string_view transfer_syntax_uid) {
    for (const auto& entry : kImplementedCodecs) {
        if (entry.uid == transfer_syntax_uid) {
            return &entry;
        }
    }
    return nullptr;
}

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
    std::vector<std::string_view> result;
    result.reserve(kImplementedCodecs.size());
    for (const auto& entry : kImplementedCodecs) {
        result.push_back(entry.uid);
    }
    return result;
}

bool codec_factory::is_supported(std::string_view transfer_syntax_uid) {
    return find_codec(transfer_syntax_uid) != nullptr;
}

bool codec_factory::is_supported(const transfer_syntax& ts) {
    return is_supported(ts.uid());
}

std::vector<std::string_view> codec_factory::available_transfer_syntaxes() {
    std::vector<std::string_view> result;
    result.reserve(kImplementedCodecs.size());
    for (const auto& entry : kImplementedCodecs) {
        if (entry.available) {
            result.push_back(entry.uid);
        }
    }
    return result;
}

bool codec_factory::is_available(std::string_view transfer_syntax_uid) {
    const auto* entry = find_codec(transfer_syntax_uid);
    return entry != nullptr && entry->available;
}

bool codec_factory::is_available(const transfer_syntax& ts) {
    return is_available(ts.uid());
}

}  // namespace kcenon::pacs::encoding::compression
