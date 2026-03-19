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
 * @file jpeg_baseline_codec.hpp
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_ENCODING_COMPRESSION_JPEG_BASELINE_CODEC_HPP
#define PACS_ENCODING_COMPRESSION_JPEG_BASELINE_CODEC_HPP

#include "pacs/encoding/compression/compression_codec.hpp"

namespace kcenon::pacs::encoding::compression {

/**
 * @brief JPEG Baseline (Process 1) codec implementation.
 *
 * Implements DICOM Transfer Syntax 1.2.840.10008.1.2.4.50.
 * Uses libjpeg-turbo for high-performance SIMD-accelerated encoding/decoding.
 *
 * Supported Features:
 * - 8-bit grayscale images
 * - 8-bit RGB/YCbCr color images
 * - Quality settings from 1-100
 * - Chroma subsampling (4:4:4, 4:2:2, 4:2:0)
 *
 * Limitations:
 * - Maximum image size: 65535 x 65535 pixels
 * - Only 8-bit depth (JPEG Baseline limitation)
 * - Lossy compression only
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
 * @see DICOM PS3.5 Annex A.4.1 - JPEG Image Compression
 * @see ITU-T T.81 - JPEG specification
 */
class jpeg_baseline_codec final : public compression_codec {
public:
    /// DICOM Transfer Syntax UID for JPEG Baseline
    static constexpr std::string_view kTransferSyntaxUID = "1.2.840.10008.1.2.4.50";

    /**
     * @brief Constructs a JPEG Baseline codec instance.
     */
    jpeg_baseline_codec();

    ~jpeg_baseline_codec() override;

    // Non-copyable but movable (internal state)
    jpeg_baseline_codec(const jpeg_baseline_codec&) = delete;
    jpeg_baseline_codec& operator=(const jpeg_baseline_codec&) = delete;
    jpeg_baseline_codec(jpeg_baseline_codec&&) noexcept;
    jpeg_baseline_codec& operator=(jpeg_baseline_codec&&) noexcept;

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
     * @brief Compresses pixel data to JPEG Baseline format.
     *
     * @param pixel_data Uncompressed pixel data (8-bit)
     * @param params Image parameters
     * @param options Compression options (quality, subsampling)
     * @return Compressed JPEG data or error
     *
     * Quality mapping:
     * - 100: Highest quality, largest file size
     * - 75: Good balance (default)
     * - 50: Medium quality
     * - 25: Lower quality, smaller file
     */
    [[nodiscard]] codec_result encode(
        std::span<const uint8_t> pixel_data,
        const image_params& params,
        const compression_options& options = {}) const override;

    /**
     * @brief Decompresses JPEG Baseline data.
     *
     * @param compressed_data JPEG compressed data
     * @param params Image parameters (width/height for validation)
     * @return Decompressed pixel data or error
     *
     * Output format:
     * - Grayscale: 8-bit samples
     * - Color: Interleaved RGB (converted from YCbCr)
     */
    [[nodiscard]] codec_result decode(
        std::span<const uint8_t> compressed_data,
        const image_params& params) const override;

    /// @}

private:
    class impl;
    std::unique_ptr<impl> impl_;
};

}  // namespace kcenon::pacs::encoding::compression

#endif  // PACS_ENCODING_COMPRESSION_JPEG_BASELINE_CODEC_HPP
