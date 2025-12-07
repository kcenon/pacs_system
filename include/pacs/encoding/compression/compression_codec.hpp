#ifndef PACS_ENCODING_COMPRESSION_COMPRESSION_CODEC_HPP
#define PACS_ENCODING_COMPRESSION_COMPRESSION_CODEC_HPP

#include "pacs/encoding/compression/image_params.hpp"
#include "pacs/encoding/transfer_syntax.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#ifdef PACS_WITH_COMMON_SYSTEM
#include <kcenon/common/patterns/result.h>
#endif

namespace pacs::encoding::compression {

/**
 * @brief Compression quality settings for lossy codecs.
 *
 * Quality is codec-specific:
 * - JPEG: 1-100, higher is better quality (larger file)
 * - JPEG 2000: Compression ratio or rate-distortion metric
 */
struct compression_options {
    /// Quality setting (1-100 for JPEG)
    int quality{75};

    /// Enable lossless mode if supported by codec
    bool lossless{false};

    /// Enable progressive encoding (JPEG only)
    bool progressive{false};

    /// Chroma subsampling for color images
    /// 0 = 4:4:4 (no subsampling)
    /// 1 = 4:2:2 (horizontal subsampling)
    /// 2 = 4:2:0 (horizontal and vertical subsampling, default for JPEG)
    int chroma_subsampling{2};
};

/**
 * @brief Result of a codec operation.
 *
 * Contains either the processed data or an error description.
 */
struct codec_result {
    /// Processed pixel data
    std::vector<uint8_t> data;

    /// True if operation succeeded
    bool success{false};

    /// Error message if operation failed
    std::string error_message;

    /// Output image parameters (may differ from input for decompression)
    image_params output_params;

    /**
     * @brief Creates a successful result with data.
     * @param d The processed data
     * @param params Output image parameters
     * @return A successful codec_result
     */
    [[nodiscard]] static codec_result ok(std::vector<uint8_t> d,
                                          const image_params& params) {
        return {std::move(d), true, "", params};
    }

    /**
     * @brief Creates a failed result with error message.
     * @param msg The error description
     * @return A failed codec_result
     */
    [[nodiscard]] static codec_result error(std::string msg) {
        return {{}, false, std::move(msg), {}};
    }
};

/**
 * @brief Abstract base class for image compression codecs.
 *
 * Provides a unified interface for DICOM image compression and decompression.
 * Implementations wrap external libraries (libjpeg-turbo, OpenJPEG, etc.).
 *
 * Thread Safety:
 * - Codec instances are NOT thread-safe by themselves
 * - Use separate instances per thread or external synchronization
 * - Factory methods are thread-safe
 *
 * @see DICOM PS3.5 Section 8.2 - Native and Encapsulated Pixel Data
 */
class compression_codec {
public:
    virtual ~compression_codec() = default;

    /// @name Codec Information
    /// @{

    /**
     * @brief Returns the Transfer Syntax UID supported by this codec.
     * @return The DICOM Transfer Syntax UID
     */
    [[nodiscard]] virtual std::string_view transfer_syntax_uid() const noexcept = 0;

    /**
     * @brief Returns a human-readable name for the codec.
     * @return The codec name (e.g., "JPEG Baseline")
     */
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;

    /**
     * @brief Checks if this codec produces lossy compression.
     * @return true if lossy, false if lossless
     */
    [[nodiscard]] virtual bool is_lossy() const noexcept = 0;

    /**
     * @brief Checks if this codec supports the given image parameters.
     * @param params The image parameters to check
     * @return true if the codec can handle these parameters
     */
    [[nodiscard]] virtual bool can_encode(const image_params& params) const noexcept = 0;

    /**
     * @brief Checks if this codec can decode data with given parameters.
     * @param params The image parameters
     * @return true if decoding is supported
     */
    [[nodiscard]] virtual bool can_decode(const image_params& params) const noexcept = 0;

    /// @}

    /// @name Compression Operations
    /// @{

    /**
     * @brief Compresses uncompressed pixel data.
     *
     * @param pixel_data The raw, uncompressed pixel data
     * @param params Image parameters describing the pixel data
     * @param options Compression settings
     * @return codec_result containing compressed data or error
     *
     * The input pixel_data must match the format specified by params:
     * - For planar_configuration=0: interleaved (RGBRGB...)
     * - For planar_configuration=1: separate planes (RRR...GGG...BBB...)
     *
     * @note This is a potentially expensive operation. Consider using
     * thread_adapter for batch processing.
     */
    [[nodiscard]] virtual codec_result encode(
        std::span<const uint8_t> pixel_data,
        const image_params& params,
        const compression_options& options = {}) const = 0;

    /**
     * @brief Decompresses compressed pixel data.
     *
     * @param compressed_data The compressed pixel data (single frame)
     * @param params Image parameters (width, height, samples_per_pixel)
     * @return codec_result containing decompressed data or error
     *
     * The output pixel data is always in interleaved format (planar_configuration=0).
     */
    [[nodiscard]] virtual codec_result decode(
        std::span<const uint8_t> compressed_data,
        const image_params& params) const = 0;

    /// @}

protected:
    compression_codec() = default;
    compression_codec(const compression_codec&) = default;
    compression_codec& operator=(const compression_codec&) = default;
    compression_codec(compression_codec&&) = default;
    compression_codec& operator=(compression_codec&&) = default;
};

}  // namespace pacs::encoding::compression

#endif  // PACS_ENCODING_COMPRESSION_COMPRESSION_CODEC_HPP
