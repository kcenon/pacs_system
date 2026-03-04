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
 * @file htj2k_codec.hpp
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_ENCODING_COMPRESSION_HTJ2K_CODEC_HPP
#define PACS_ENCODING_COMPRESSION_HTJ2K_CODEC_HPP

#include "pacs/encoding/compression/compression_codec.hpp"

namespace pacs::encoding::compression {

/**
 * @brief High-Throughput JPEG 2000 (HTJ2K) codec implementation.
 *
 * Implements DICOM Transfer Syntaxes defined in Supplement 235:
 * - 1.2.840.10008.1.2.4.201 (HTJ2K Lossless Only)
 * - 1.2.840.10008.1.2.4.202 (HTJ2K with RPCL Options - Lossless Only)
 * - 1.2.840.10008.1.2.4.203 (HTJ2K - Lossless or Lossy)
 *
 * HTJ2K provides 10-50x faster decoding than legacy JPEG 2000 while
 * maintaining comparable compression ratios. Uses the Part 15 (HTJ2K)
 * block coder which enables highly parallelizable decoding.
 *
 * Supported Features:
 * - 8-bit, 12-bit, 16-bit grayscale images
 * - 8-bit color images (RGB, YCbCr)
 * - Lossless mode (reversible 5/3 wavelet transform with HT block coder)
 * - Lossy mode (irreversible 9/7 wavelet transform with HT block coder)
 * - RPCL progression order for streaming/progressive decoding
 *
 * Thread Safety:
 * - This class is NOT thread-safe
 * - Create separate instances per thread for concurrent operations
 *
 * @note Actual encode/decode operations require OpenJPH library integration
 *       (see issue #785). Without OpenJPH, encode/decode return an error.
 *
 * @see DICOM Supplement 235 - HTJ2K Transfer Syntaxes
 * @see ISO/IEC 15444-15 (JPEG 2000 Part 15 - High-Throughput block coder)
 */
class htj2k_codec final : public compression_codec {
public:
    /// DICOM Transfer Syntax UID for HTJ2K Lossless Only
    static constexpr std::string_view kTransferSyntaxUIDLossless =
        "1.2.840.10008.1.2.4.201";

    /// DICOM Transfer Syntax UID for HTJ2K with RPCL Options (Lossless Only)
    static constexpr std::string_view kTransferSyntaxUIDRPCL =
        "1.2.840.10008.1.2.4.202";

    /// DICOM Transfer Syntax UID for HTJ2K (Lossy)
    static constexpr std::string_view kTransferSyntaxUIDLossy =
        "1.2.840.10008.1.2.4.203";

    /// Default compression ratio for lossy mode (20:1)
    static constexpr float kDefaultCompressionRatio = 20.0f;

    /// Default number of resolution levels
    static constexpr int kDefaultResolutionLevels = 6;

    /**
     * @brief Constructs an HTJ2K codec instance.
     *
     * @param lossless If true, use lossless mode (Transfer Syntax .201 or .202).
     *                 If false, use lossy mode (Transfer Syntax .203).
     * @param use_rpcl If true, use RPCL progression order (Transfer Syntax .202).
     *                 Only meaningful in lossless mode. Enables progressive
     *                 resolution decoding for streaming use cases.
     * @param compression_ratio Target compression ratio for lossy mode (ignored in lossless).
     *                          Higher values = smaller files but lower quality.
     * @param resolution_levels Number of DWT resolution levels (1-32, default: 6).
     */
    explicit htj2k_codec(bool lossless = true,
                          bool use_rpcl = false,
                          float compression_ratio = kDefaultCompressionRatio,
                          int resolution_levels = kDefaultResolutionLevels);

    ~htj2k_codec() override;

    // Non-copyable but movable
    htj2k_codec(const htj2k_codec&) = delete;
    htj2k_codec& operator=(const htj2k_codec&) = delete;
    htj2k_codec(htj2k_codec&&) noexcept;
    htj2k_codec& operator=(htj2k_codec&&) noexcept;

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
     * @brief Checks if RPCL progression order is enabled.
     * @return true if RPCL mode is active
     */
    [[nodiscard]] bool is_rpcl_mode() const noexcept;

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
     * @brief Compresses pixel data to HTJ2K format.
     *
     * @param pixel_data Uncompressed pixel data (8/12/16-bit, grayscale or color)
     * @param params Image parameters
     * @param options Compression options
     * @return Compressed HTJ2K codestream or error
     *
     * @note Currently returns an error indicating OpenJPH is not integrated.
     *       Full implementation will be added in issue #785.
     */
    [[nodiscard]] codec_result encode(
        std::span<const uint8_t> pixel_data,
        const image_params& params,
        const compression_options& options = {}) const override;

    /**
     * @brief Decompresses HTJ2K data.
     *
     * @param compressed_data HTJ2K compressed data
     * @param params Image parameters (width/height for validation)
     * @return Decompressed pixel data or error
     *
     * @note Currently returns an error indicating OpenJPH is not integrated.
     *       Full implementation will be added in issue #785.
     */
    [[nodiscard]] codec_result decode(
        std::span<const uint8_t> compressed_data,
        const image_params& params) const override;

    /// @}

private:
    bool lossless_;
    bool use_rpcl_;
    float compression_ratio_;
    int resolution_levels_;
};

}  // namespace pacs::encoding::compression

#endif  // PACS_ENCODING_COMPRESSION_HTJ2K_CODEC_HPP
