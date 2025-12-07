#ifndef PACS_ENCODING_COMPRESSION_JPEG_LOSSLESS_CODEC_HPP
#define PACS_ENCODING_COMPRESSION_JPEG_LOSSLESS_CODEC_HPP

#include "pacs/encoding/compression/compression_codec.hpp"

namespace pacs::encoding::compression {

/**
 * @brief JPEG Lossless (Process 14, Selection Value 1) codec implementation.
 *
 * Implements DICOM Transfer Syntax 1.2.840.10008.1.2.4.70.
 * Uses libjpeg-turbo for high-performance SIMD-accelerated lossless encoding/decoding.
 *
 * Supported Features:
 * - 8-bit grayscale images
 * - 12-bit grayscale images (medical imaging)
 * - 16-bit grayscale images
 * - First-order prediction (Selection Value 1: Ra = left neighbor)
 * - Huffman coding
 *
 * Limitations:
 * - Maximum image size: 65535 x 65535 pixels
 * - Grayscale only (color not supported for lossless in DICOM)
 * - Requires libjpeg-turbo 3.0+ for native lossless support
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
 * @see DICOM PS3.5 Annex A.4.2 - JPEG Lossless Image Compression
 * @see ITU-T T.81 - JPEG specification
 */
class jpeg_lossless_codec final : public compression_codec {
public:
    /// DICOM Transfer Syntax UID for JPEG Lossless (Process 14, Selection Value 1)
    static constexpr std::string_view kTransferSyntaxUID = "1.2.840.10008.1.2.4.70";

    /// Default predictor selection value (1 = Ra, left neighbor prediction)
    static constexpr int kDefaultPredictor = 1;

    /// Default point transform (0 = no scaling)
    static constexpr int kDefaultPointTransform = 0;

    /**
     * @brief Constructs a JPEG Lossless codec instance.
     *
     * @param predictor Predictor selection (1-7, default: 1)
     *   - 1: Ra (left neighbor)
     *   - 2: Rb (above neighbor)
     *   - 3: Rc (diagonal upper-left neighbor)
     *   - 4: Ra + Rb - Rc
     *   - 5: Ra + (Rb - Rc) / 2
     *   - 6: Rb + (Ra - Rc) / 2
     *   - 7: (Ra + Rb) / 2
     * @param point_transform Point transform value (0-15, default: 0)
     */
    explicit jpeg_lossless_codec(int predictor = kDefaultPredictor,
                                  int point_transform = kDefaultPointTransform);

    ~jpeg_lossless_codec() override;

    // Non-copyable but movable (internal state)
    jpeg_lossless_codec(const jpeg_lossless_codec&) = delete;
    jpeg_lossless_codec& operator=(const jpeg_lossless_codec&) = delete;
    jpeg_lossless_codec(jpeg_lossless_codec&&) noexcept;
    jpeg_lossless_codec& operator=(jpeg_lossless_codec&&) noexcept;

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
     * @brief Gets the current predictor selection value.
     * @return Predictor value (1-7)
     */
    [[nodiscard]] int predictor() const noexcept;

    /**
     * @brief Gets the current point transform value.
     * @return Point transform value (0-15)
     */
    [[nodiscard]] int point_transform() const noexcept;

    /// @}

    /// @name Compression Operations
    /// @{

    /**
     * @brief Compresses pixel data to JPEG Lossless format.
     *
     * @param pixel_data Uncompressed pixel data (8/12/16-bit)
     * @param params Image parameters
     * @param options Compression options (lossless flag is ignored, always lossless)
     * @return Compressed JPEG data or error
     *
     * The output is guaranteed to be exactly reconstructible (lossless).
     * Quality setting in options is ignored for lossless compression.
     */
    [[nodiscard]] codec_result encode(
        std::span<const uint8_t> pixel_data,
        const image_params& params,
        const compression_options& options = {}) const override;

    /**
     * @brief Decompresses JPEG Lossless data.
     *
     * @param compressed_data JPEG lossless compressed data
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

#endif  // PACS_ENCODING_COMPRESSION_JPEG_LOSSLESS_CODEC_HPP
