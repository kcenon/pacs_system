#ifndef PACS_ENCODING_COMPRESSION_JPEG2000_CODEC_HPP
#define PACS_ENCODING_COMPRESSION_JPEG2000_CODEC_HPP

#include "pacs/encoding/compression/compression_codec.hpp"

namespace pacs::encoding::compression {

/**
 * @brief JPEG 2000 codec implementation supporting both lossless and lossy modes.
 *
 * Implements DICOM Transfer Syntaxes:
 * - 1.2.840.10008.1.2.4.90 (JPEG 2000 Image Compression - Lossless Only)
 * - 1.2.840.10008.1.2.4.91 (JPEG 2000 Image Compression)
 *
 * Uses OpenJPEG library for high-performance wavelet-based compression/decompression.
 *
 * Supported Features:
 * - 8-bit, 12-bit, 16-bit grayscale images
 * - 8-bit color images (RGB, YCbCr)
 * - Lossless mode (reversible 5/3 wavelet transform)
 * - Lossy mode (irreversible 9/7 wavelet transform)
 * - Configurable compression ratio for lossy mode
 * - Progressive decoding support
 *
 * Limitations:
 * - Maximum image size: 2^32-1 x 2^32-1 pixels (practical limit: memory)
 * - Requires OpenJPEG 2.4+ for full feature support
 *
 * Thread Safety:
 * - This class is NOT thread-safe
 * - Create separate instances per thread for concurrent operations
 *
 * Integration:
 * - Uses thread_system for parallel tile encoding (via thread_adapter)
 * - Logs operations via logger_system (via logger_adapter)
 * - Reports metrics to monitoring_system (via monitoring_adapter)
 *
 * @see DICOM PS3.5 Annex A.4.4 - JPEG 2000 Image Compression
 * @see ISO/IEC 15444-1 (JPEG 2000 Part 1)
 */
class jpeg2000_codec final : public compression_codec {
public:
    /// DICOM Transfer Syntax UID for JPEG 2000 Lossless Only
    static constexpr std::string_view kTransferSyntaxUIDLossless =
        "1.2.840.10008.1.2.4.90";

    /// DICOM Transfer Syntax UID for JPEG 2000 (Lossy or Lossless)
    static constexpr std::string_view kTransferSyntaxUIDLossy =
        "1.2.840.10008.1.2.4.91";

    /// Default compression ratio for lossy mode (20:1)
    static constexpr float kDefaultCompressionRatio = 20.0f;

    /// Default number of resolution levels
    static constexpr int kDefaultResolutionLevels = 6;

    /**
     * @brief Constructs a JPEG 2000 codec instance.
     *
     * @param lossless If true, use lossless mode (Transfer Syntax 1.2.840.10008.1.2.4.90).
     *                 If false, use lossy mode (Transfer Syntax 1.2.840.10008.1.2.4.91).
     * @param compression_ratio Target compression ratio for lossy mode (ignored in lossless).
     *                          Higher values = smaller files but lower quality.
     *                          Typical range: 10-50 for medical imaging.
     * @param resolution_levels Number of DWT resolution levels (1-32, default: 6).
     *                          More levels enable progressive decoding at multiple resolutions.
     */
    explicit jpeg2000_codec(bool lossless = true,
                            float compression_ratio = kDefaultCompressionRatio,
                            int resolution_levels = kDefaultResolutionLevels);

    ~jpeg2000_codec() override;

    // Non-copyable but movable (internal state)
    jpeg2000_codec(const jpeg2000_codec&) = delete;
    jpeg2000_codec& operator=(const jpeg2000_codec&) = delete;
    jpeg2000_codec(jpeg2000_codec&&) noexcept;
    jpeg2000_codec& operator=(jpeg2000_codec&&) noexcept;

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
     * @return true if lossless, false if lossy
     */
    [[nodiscard]] bool is_lossless_mode() const noexcept;

    /**
     * @brief Gets the current compression ratio setting.
     * @return Compression ratio (only meaningful for lossy mode)
     */
    [[nodiscard]] float compression_ratio() const noexcept;

    /**
     * @brief Gets the number of DWT resolution levels.
     * @return Resolution levels (1-32)
     */
    [[nodiscard]] int resolution_levels() const noexcept;

    /// @}

    /// @name Compression Operations
    /// @{

    /**
     * @brief Compresses pixel data to JPEG 2000 format.
     *
     * @param pixel_data Uncompressed pixel data (8/12/16-bit, grayscale or color)
     * @param params Image parameters
     * @param options Compression options:
     *                - quality: 1-100 for lossy mode (maps to compression ratio)
     *                - lossless: Override codec's lossless setting
     * @return Compressed J2K codestream or error
     *
     * For lossless mode:
     * - Uses reversible 5/3 integer wavelet transform
     * - Output is exactly reconstructible
     *
     * For lossy mode:
     * - Uses irreversible 9/7 floating-point wavelet transform
     * - Compression ratio is determined by quality setting or constructor parameter
     */
    [[nodiscard]] codec_result encode(
        std::span<const uint8_t> pixel_data,
        const image_params& params,
        const compression_options& options = {}) const override;

    /**
     * @brief Decompresses JPEG 2000 data.
     *
     * @param compressed_data JPEG 2000 compressed data (J2K or JP2 format)
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

#endif  // PACS_ENCODING_COMPRESSION_JPEG2000_CODEC_HPP
