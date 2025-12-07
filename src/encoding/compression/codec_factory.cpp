#include "pacs/encoding/compression/codec_factory.hpp"
#include "pacs/encoding/compression/jpeg_baseline_codec.hpp"

#include <array>

namespace pacs::encoding::compression {

namespace {

/**
 * @brief List of supported Transfer Syntax UIDs for compression codecs.
 *
 * As more codecs are implemented (JPEG Lossless, JPEG 2000, etc.),
 * they should be added to this list.
 */
static constexpr std::array<std::string_view, 1> kSupportedTransferSyntaxes = {{
    jpeg_baseline_codec::kTransferSyntaxUID,  // 1.2.840.10008.1.2.4.50
}};

}  // namespace

std::unique_ptr<compression_codec> codec_factory::create(
    std::string_view transfer_syntax_uid) {

    // JPEG Baseline (Process 1)
    if (transfer_syntax_uid == jpeg_baseline_codec::kTransferSyntaxUID) {
        return std::make_unique<jpeg_baseline_codec>();
    }

    // Future codecs will be added here:
    // - JPEG Lossless (1.2.840.10008.1.2.4.70)
    // - JPEG 2000 Lossless (1.2.840.10008.1.2.4.90)
    // - JPEG 2000 Lossy (1.2.840.10008.1.2.4.91)
    // - JPEG-LS Lossless (1.2.840.10008.1.2.4.80)
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
