#include "pacs/encoding/compression/codec_factory.hpp"
#include "pacs/encoding/compression/jpeg_baseline_codec.hpp"
#include "pacs/encoding/compression/jpeg_lossless_codec.hpp"
#include "pacs/encoding/compression/jpeg2000_codec.hpp"
#include "pacs/encoding/compression/jpeg_ls_codec.hpp"

#include <array>

namespace pacs::encoding::compression {

namespace {

/**
 * @brief List of supported Transfer Syntax UIDs for compression codecs.
 *
 * As more codecs are implemented (RLE, etc.),
 * they should be added to this list.
 */
static constexpr std::array<std::string_view, 6> kSupportedTransferSyntaxes = {{
    jpeg_baseline_codec::kTransferSyntaxUID,         // 1.2.840.10008.1.2.4.50
    jpeg_lossless_codec::kTransferSyntaxUID,         // 1.2.840.10008.1.2.4.70
    jpeg_ls_codec::kTransferSyntaxUIDLossless,       // 1.2.840.10008.1.2.4.80
    jpeg_ls_codec::kTransferSyntaxUIDNearLossless,   // 1.2.840.10008.1.2.4.81
    jpeg2000_codec::kTransferSyntaxUIDLossless,      // 1.2.840.10008.1.2.4.90
    jpeg2000_codec::kTransferSyntaxUIDLossy,         // 1.2.840.10008.1.2.4.91
}};

}  // namespace

std::unique_ptr<compression_codec> codec_factory::create(
    std::string_view transfer_syntax_uid) {

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

    // Future codecs will be added here:
    // - RLE Lossless (1.2.840.10008.1.2.5)

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
