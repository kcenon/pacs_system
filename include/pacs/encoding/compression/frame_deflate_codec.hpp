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

/**
 * @file frame_deflate_codec.hpp
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_ENCODING_COMPRESSION_FRAME_DEFLATE_CODEC_HPP
#define PACS_ENCODING_COMPRESSION_FRAME_DEFLATE_CODEC_HPP

#include "pacs/encoding/compression/compression_codec.hpp"

namespace pacs::encoding::compression {

/**
 * @brief Frame-level Deflate codec implementation (Supplement 244).
 *
 * Implements DICOM Transfer Syntax 1.2.840.10008.1.2.11 defined in
 * Supplement 244. Applies zlib deflate compression on a per-frame basis,
 * specifically optimized for single-bit binary segmentation masks generated
 * by AI algorithms.
 *
 * Key Properties:
 * - Always lossless (deflate is a lossless compression algorithm)
 * - Per-frame compression enables random access to individual frames
 * - Highest compression ratio for single-bit binary data compared to RLE
 * - Encapsulated pixel data format (like JPEG, RLE, etc.)
 *
 * Supported Features:
 * - 1-bit binary segmentation masks (optimal use case)
 * - 8-bit grayscale images
 * - 16-bit grayscale images
 * - Single-sample (grayscale) images
 * - Multi-frame images (via external frame-by-frame processing)
 *
 * Limitations:
 * - Color images (samples_per_pixel > 1) are not supported
 * - Maximum image size: 65535 x 65535 pixels
 *
 * Thread Safety:
 * - This class is NOT thread-safe
 * - Create separate instances per thread for concurrent operations
 *
 * @note Actual encode/decode operations require zlib library integration.
 *       Without zlib, encode/decode return an error.
 *
 * @see DICOM Supplement 244 -- Frame Deflate Transfer Syntax
 * @see RFC 1950 (zlib), RFC 1951 (DEFLATE)
 */
class frame_deflate_codec final : public compression_codec {
public:
    /// DICOM Transfer Syntax UID for Frame Deflate
    static constexpr std::string_view kTransferSyntaxUID =
        "1.2.840.10008.1.2.11";

    /// Default zlib compression level (6 = balanced speed/ratio)
    static constexpr int kDefaultCompressionLevel = 6;

    /// Minimum zlib compression level (fastest)
    static constexpr int kMinCompressionLevel = 1;

    /// Maximum zlib compression level (best compression)
    static constexpr int kMaxCompressionLevel = 9;

    /**
     * @brief Constructs a Frame Deflate codec instance.
     *
     * @param compression_level zlib compression level (1-9).
     *        1 = fastest, 9 = best compression, 6 = default balanced.
     */
    explicit frame_deflate_codec(
        int compression_level = kDefaultCompressionLevel);

    ~frame_deflate_codec() override;

    // Non-copyable but movable
    frame_deflate_codec(const frame_deflate_codec&) = delete;
    frame_deflate_codec& operator=(const frame_deflate_codec&) = delete;
    frame_deflate_codec(frame_deflate_codec&&) noexcept;
    frame_deflate_codec& operator=(frame_deflate_codec&&) noexcept;

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
     * @brief Gets the current zlib compression level.
     * @return Compression level (1-9)
     */
    [[nodiscard]] int compression_level() const noexcept;

    /// @}

    /// @name Compression Operations
    /// @{

    /**
     * @brief Compresses pixel data using zlib deflate.
     *
     * @param pixel_data Uncompressed pixel data (single frame)
     * @param params Image parameters describing the pixel data
     * @param options Compression options (lossless flag is ignored, always lossless)
     * @return Compressed deflate data or error
     *
     * For binary segmentation masks (1-bit), achieves very high compression
     * ratios due to the repetitive nature of single-bit data.
     */
    [[nodiscard]] codec_result encode(
        std::span<const uint8_t> pixel_data,
        const image_params& params,
        const compression_options& options = {}) const override;

    /**
     * @brief Decompresses deflate-compressed pixel data.
     *
     * @param compressed_data Deflate compressed data (single frame)
     * @param params Image parameters (width, height, samples_per_pixel)
     * @return Decompressed pixel data or error
     */
    [[nodiscard]] codec_result decode(
        std::span<const uint8_t> compressed_data,
        const image_params& params) const override;

    /// @}

private:
    int compression_level_;
};

}  // namespace pacs::encoding::compression

#endif  // PACS_ENCODING_COMPRESSION_FRAME_DEFLATE_CODEC_HPP
