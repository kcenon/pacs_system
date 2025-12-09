#ifndef PACS_ENCODING_COMPRESSION_RLE_CODEC_HPP
#define PACS_ENCODING_COMPRESSION_RLE_CODEC_HPP

#include "pacs/encoding/compression/compression_codec.hpp"

namespace pacs::encoding::compression {

/**
 * @brief DICOM RLE Lossless codec implementation.
 *
 * Implements DICOM Transfer Syntax 1.2.840.10008.1.2.5.
 * Uses pure C++ implementation without external library dependencies.
 *
 * DICOM RLE uses a segment-based approach where each sample is encoded
 * separately, allowing for efficient lossless compression of medical images.
 *
 * Supported Features:
 * - 8-bit grayscale and color images
 * - 16-bit grayscale images (stored as two 8-bit segments)
 * - RGB color images (3 samples per pixel)
 * - Multi-frame images (via external frame-by-frame processing)
 *
 * Limitations:
 * - Maximum 15 segments (DICOM RLE specification)
 * - Maximum image size: 65535 x 65535 pixels
 * - No support for signed pixel representation in encoding
 *
 * Thread Safety:
 * - This class is NOT thread-safe
 * - Create separate instances per thread for concurrent operations
 *
 * Integration:
 * - Uses thread_system for parallel batch encoding (via thread_adapter)
 * - Logs operations via logger_system (via logger_adapter)
 * - Reports metrics to monitoring_system (via monitoring_adapter)
 *
 * @see DICOM PS3.5 Annex G - RLE Lossless Compression
 */
class rle_codec final : public compression_codec {
public:
    /// DICOM Transfer Syntax UID for RLE Lossless
    static constexpr std::string_view kTransferSyntaxUID = "1.2.840.10008.1.2.5";

    /// Maximum number of RLE segments allowed by DICOM specification
    static constexpr int kMaxSegments = 15;

    /// RLE header size (64 bytes: 16 x 4-byte offsets)
    static constexpr size_t kRLEHeaderSize = 64;

    /**
     * @brief Constructs an RLE codec instance.
     */
    rle_codec();

    ~rle_codec() override;

    // Non-copyable but movable (internal state)
    rle_codec(const rle_codec&) = delete;
    rle_codec& operator=(const rle_codec&) = delete;
    rle_codec(rle_codec&&) noexcept;
    rle_codec& operator=(rle_codec&&) noexcept;

    /// @name Codec Information
    /// @{

    [[nodiscard]] std::string_view transfer_syntax_uid() const noexcept override;
    [[nodiscard]] std::string_view name() const noexcept override;
    [[nodiscard]] bool is_lossy() const noexcept override;
    [[nodiscard]] bool can_encode(const image_params& params) const noexcept override;
    [[nodiscard]] bool can_decode(const image_params& params) const noexcept override;

    /// @}

    /// @name Compression Operations
    /// @{

    /**
     * @brief Compresses pixel data to RLE format.
     *
     * @param pixel_data Uncompressed pixel data
     * @param params Image parameters
     * @param options Compression options (lossless flag is ignored, always lossless)
     * @return Compressed RLE data or error
     *
     * The output is guaranteed to be exactly reconstructible (lossless).
     * The output includes the 64-byte RLE header followed by segment data.
     *
     * For 16-bit images, each pixel is split into high byte and low byte,
     * creating twice the number of segments as 8-bit images.
     */
    [[nodiscard]] codec_result encode(
        std::span<const uint8_t> pixel_data,
        const image_params& params,
        const compression_options& options = {}) const override;

    /**
     * @brief Decompresses RLE data.
     *
     * @param compressed_data RLE compressed data (including header)
     * @param params Image parameters (width, height, samples_per_pixel)
     * @return Decompressed pixel data or error
     *
     * Output format matches the original bit depth:
     * - 8-bit: single byte per sample
     * - 16-bit: two bytes per sample (little-endian)
     */
    [[nodiscard]] codec_result decode(
        std::span<const uint8_t> compressed_data,
        const image_params& params) const override;

    /// @}

private:
    class impl;
    std::unique_ptr<impl> impl_;
};

}  // namespace pacs::encoding::compression

#endif  // PACS_ENCODING_COMPRESSION_RLE_CODEC_HPP
