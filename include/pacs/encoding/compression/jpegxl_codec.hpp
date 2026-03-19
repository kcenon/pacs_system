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
 * @file jpegxl_codec.hpp
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_ENCODING_COMPRESSION_JPEGXL_CODEC_HPP
#define PACS_ENCODING_COMPRESSION_JPEGXL_CODEC_HPP

#include "pacs/encoding/compression/compression_codec.hpp"

namespace kcenon::pacs::encoding::compression {

/**
 * @brief JPEG XL codec implementation (Supplement 232).
 *
 * Implements DICOM Transfer Syntaxes for JPEG XL:
 * - 1.2.840.10008.1.2.4.110 (JPEG XL Lossless)
 * - 1.2.840.10008.1.2.4.111 (JPEG XL JPEG Recompression -- zero generation loss)
 * - 1.2.840.10008.1.2.4.112 (JPEG XL -- any mode)
 *
 * JPEG XL provides superior compression compared to JPEG 2000 and PNG,
 * with a unique ability to losslessly recompress existing JPEG data
 * without any generation loss -- critical for legacy archive migration.
 *
 * Supported Features:
 * - 8-bit and 16-bit grayscale images
 * - 8-bit color images (RGB)
 * - Lossless mode
 * - JPEG recompression (reversible, no generation loss)
 * - Lossy mode with configurable quality
 *
 * Thread Safety:
 * - This class is NOT thread-safe
 * - Create separate instances per thread for concurrent operations
 *
 * @note Actual encode/decode operations require libjxl integration.
 *       Without libjxl, encode/decode return an error.
 *
 * @see DICOM Supplement 232 -- JPEG XL Transfer Syntaxes
 * @see ISO/IEC 18181 -- JPEG XL Image Coding System
 */
class jpegxl_codec final : public compression_codec {
public:
    /// DICOM Transfer Syntax UID for JPEG XL Lossless
    static constexpr std::string_view kTransferSyntaxUIDLossless =
        "1.2.840.10008.1.2.4.110";

    /// DICOM Transfer Syntax UID for JPEG XL JPEG Recompression
    static constexpr std::string_view kTransferSyntaxUIDJPEGRecompression =
        "1.2.840.10008.1.2.4.111";

    /// DICOM Transfer Syntax UID for JPEG XL (any mode)
    static constexpr std::string_view kTransferSyntaxUIDLossy =
        "1.2.840.10008.1.2.4.112";

    /// Default quality distance for lossy mode (1.0 = visually lossless)
    static constexpr float kDefaultQualityDistance = 1.0f;

    /**
     * @brief Constructs a JPEG XL codec instance.
     *
     * @param lossless If true, use lossless mode (UID .110).
     * @param jpeg_recompression If true, use JPEG recompression mode (UID .111).
     *                           Only meaningful when lossless is true.
     * @param quality_distance Butteraugli distance for lossy mode (0.0 = lossless,
     *                         1.0 = visually lossless). Ignored in lossless mode.
     */
    explicit jpegxl_codec(bool lossless = true,
                          bool jpeg_recompression = false,
                          float quality_distance = kDefaultQualityDistance);

    ~jpegxl_codec() override;

    // Non-copyable but movable
    jpegxl_codec(const jpegxl_codec&) = delete;
    jpegxl_codec& operator=(const jpegxl_codec&) = delete;
    jpegxl_codec(jpegxl_codec&&) noexcept;
    jpegxl_codec& operator=(jpegxl_codec&&) noexcept;

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

    [[nodiscard]] bool is_lossless_mode() const noexcept;
    [[nodiscard]] bool is_jpeg_recompression_mode() const noexcept;
    [[nodiscard]] float quality_distance() const noexcept;

    /// @}

    /// @name Compression Operations
    /// @{

    [[nodiscard]] codec_result encode(
        std::span<const uint8_t> pixel_data,
        const image_params& params,
        const compression_options& options = {}) const override;

    [[nodiscard]] codec_result decode(
        std::span<const uint8_t> compressed_data,
        const image_params& params) const override;

    /// @}

private:
    bool lossless_;
    bool jpeg_recompression_;
    float quality_distance_;
};

}  // namespace kcenon::pacs::encoding::compression

#endif  // PACS_ENCODING_COMPRESSION_JPEGXL_CODEC_HPP
