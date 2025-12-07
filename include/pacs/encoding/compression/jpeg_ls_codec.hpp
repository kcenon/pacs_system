#ifndef PACS_ENCODING_COMPRESSION_JPEG_LS_CODEC_HPP
#define PACS_ENCODING_COMPRESSION_JPEG_LS_CODEC_HPP

#include "pacs/encoding/compression/compression_codec.hpp"

namespace pacs::encoding::compression {

/**
 * @brief JPEG-LS codec implementation supporting both lossless and near-lossless modes.
 *
 * Implements DICOM Transfer Syntaxes:
 * - 1.2.840.10008.1.2.4.80 (JPEG-LS Lossless Image Compression)
 * - 1.2.840.10008.1.2.4.81 (JPEG-LS Lossy (Near-Lossless) Image Compression)
 *
 * Uses CharLS library for high-performance JPEG-LS encoding/decoding.
 *
 * Supported Features:
 * - 8-bit, 12-bit, 16-bit grayscale images
 * - 8-bit color images (RGB, interleaved)
 * - Lossless mode (NEAR=0)
 * - Near-lossless mode with configurable NEAR parameter
 * - Line interleaved and sample interleaved modes for color
 *
 * Limitations:
 * - Maximum image size: 65535 x 65535 pixels
 * - Requires CharLS 2.0+ for full feature support
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
 * @see DICOM PS3.5 Annex A.4.3 - JPEG-LS Image Compression
 * @see ISO/IEC 14495-1 (JPEG-LS standard)
 */
class jpeg_ls_codec final : public compression_codec {
public:
    /// DICOM Transfer Syntax UID for JPEG-LS Lossless
    static constexpr std::string_view kTransferSyntaxUIDLossless =
        "1.2.840.10008.1.2.4.80";

    /// DICOM Transfer Syntax UID for JPEG-LS Near-Lossless (Lossy)
    static constexpr std::string_view kTransferSyntaxUIDNearLossless =
        "1.2.840.10008.1.2.4.81";

    /// Sentinel value indicating "auto-determine NEAR based on mode"
    static constexpr int kAutoNearValue = -1;

    /// NEAR parameter for lossless mode
    static constexpr int kLosslessNearValue = 0;

    /// Default NEAR parameter for near-lossless mode (visually lossless quality)
    static constexpr int kDefaultNearLosslessValue = 2;

    /// Maximum NEAR parameter value (higher = more compression, more loss)
    static constexpr int kMaxNearValue = 255;

    /**
     * @brief Constructs a JPEG-LS codec instance.
     *
     * @param lossless If true, use lossless mode (Transfer Syntax 1.2.840.10008.1.2.4.80).
     *                 If false, use near-lossless mode (Transfer Syntax 1.2.840.10008.1.2.4.81).
     * @param near_value NEAR parameter controlling the maximum absolute error per pixel.
     *                   -1 = auto (0 for lossless, kDefaultNearLosslessValue for near-lossless)
     *                   0 = lossless (overrides lossless flag to true)
     *                   1-255 = near-lossless (bounded error, higher = more compression)
     *                   Typical medical imaging: 1-3 for visually lossless quality.
     */
    explicit jpeg_ls_codec(bool lossless = true, int near_value = kAutoNearValue);

    ~jpeg_ls_codec() override;

    // Non-copyable but movable (internal state)
    jpeg_ls_codec(const jpeg_ls_codec&) = delete;
    jpeg_ls_codec& operator=(const jpeg_ls_codec&) = delete;
    jpeg_ls_codec(jpeg_ls_codec&&) noexcept;
    jpeg_ls_codec& operator=(jpeg_ls_codec&&) noexcept;

    /// @name Codec Information
    /// @{

    [[nodiscard]] std::string_view transfer_syntax_uid() const noexcept override;
    [[nodiscard]] std::string_view name() const noexcept override;
    [[nodiscard]] bool is_lossy() const noexcept override;
    [[nodiscard]] bool can_encode(const image_params& params) const noexcept override;
    [[nodiscard]] bool can_decode(const image_params& params) const noexcept override;

    /// @}

    /// @name Configuration
    /// @{

    /**
     * @brief Checks if this codec is configured for lossless mode.
     * @return true if lossless, false if near-lossless
     */
    [[nodiscard]] bool is_lossless_mode() const noexcept;

    /**
     * @brief Gets the NEAR parameter value.
     * @return NEAR value (0 for lossless, 1-255 for near-lossless)
     */
    [[nodiscard]] int near_value() const noexcept;

    /// @}

    /// @name Compression Operations
    /// @{

    /**
     * @brief Compresses pixel data to JPEG-LS format.
     *
     * @param pixel_data Uncompressed pixel data (8/12/16-bit, grayscale or color)
     * @param params Image parameters
     * @param options Compression options:
     *                - quality: 1-100 for near-lossless mode (maps to NEAR parameter)
     *                - lossless: Override codec's lossless setting
     * @return Compressed JPEG-LS data or error
     *
     * For lossless mode (NEAR=0):
     * - Output is exactly reconstructible
     * - Typical compression ratio: 2:1 to 4:1 for medical images
     *
     * For near-lossless mode (NEAR>0):
     * - Maximum pixel error is bounded by NEAR value
     * - Higher NEAR = smaller file but more artifact potential
     * - NEAR=1-3 typically provides visually lossless quality
     */
    [[nodiscard]] codec_result encode(
        std::span<const uint8_t> pixel_data,
        const image_params& params,
        const compression_options& options = {}) const override;

    /**
     * @brief Decompresses JPEG-LS data.
     *
     * @param compressed_data JPEG-LS compressed data
     * @param params Image parameters (width/height for validation)
     * @return Decompressed pixel data or error
     *
     * Output format matches the original bit depth:
     * - 8-bit: single byte per sample
     * - 12/16-bit: two bytes per sample (little-endian)
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

#endif  // PACS_ENCODING_COMPRESSION_JPEG_LS_CODEC_HPP
