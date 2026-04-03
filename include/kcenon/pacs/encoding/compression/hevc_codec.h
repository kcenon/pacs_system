// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file hevc_codec.h
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_ENCODING_COMPRESSION_HEVC_CODEC_HPP
#define PACS_ENCODING_COMPRESSION_HEVC_CODEC_HPP

#include "kcenon/pacs/encoding/compression/compression_codec.h"

namespace kcenon::pacs::encoding::compression {

/**
 * @brief HEVC/H.265 codec implementation for video-encoded multi-frame DICOM.
 *
 * Implements DICOM Transfer Syntaxes for HEVC:
 * - 1.2.840.10008.1.2.4.107 (HEVC/H.265 Main Profile / Level 5.1)
 * - 1.2.840.10008.1.2.4.108 (HEVC/H.265 Main 10 Profile / Level 5.1)
 *
 * HEVC provides approximately 50% bitrate reduction compared to H.264/AVC
 * for equivalent video quality. Intended for video-encoded multi-frame
 * DICOM objects such as cine MRI, ultrasound clips, and XA sequences.
 *
 * Supported Features:
 * - 8-bit grayscale and color images (Main Profile)
 * - 10-bit grayscale and color images (Main 10 Profile)
 * - Lossy compression with configurable quality
 * - Encapsulated pixel data per PS3.5 Annex A.4
 *
 * Thread Safety:
 * - This class is NOT thread-safe
 * - Create separate instances per thread for concurrent operations
 *
 * @note Actual encode/decode operations require an HEVC library (libde265,
 *       x265, or ffmpeg). Without a library, encode/decode return an error.
 *
 * @see DICOM PS3.5 -- HEVC/H.265 Transfer Syntax
 * @see ITU-T H.265 / ISO/IEC 23008-2
 */
class hevc_codec final : public compression_codec {
public:
    /// DICOM Transfer Syntax UID for HEVC/H.265 Main Profile / Level 5.1
    static constexpr std::string_view kTransferSyntaxUIDMain =
        "1.2.840.10008.1.2.4.107";

    /// DICOM Transfer Syntax UID for HEVC/H.265 Main 10 Profile / Level 5.1
    static constexpr std::string_view kTransferSyntaxUIDMain10 =
        "1.2.840.10008.1.2.4.108";

    /// Default quality for lossy encoding (CRF value, 0-51, lower = better)
    static constexpr int kDefaultQuality = 23;

    /**
     * @brief Constructs an HEVC codec instance.
     *
     * @param main10 If true, use Main 10 Profile (10-bit, UID .108).
     *               If false, use Main Profile (8-bit, UID .107).
     * @param quality Constant Rate Factor for encoding (0-51, lower = better).
     */
    explicit hevc_codec(bool main10 = false,
                        int quality = kDefaultQuality);

    ~hevc_codec() override;

    // Non-copyable but movable
    hevc_codec(const hevc_codec&) = delete;
    hevc_codec& operator=(const hevc_codec&) = delete;
    hevc_codec(hevc_codec&&) noexcept;
    hevc_codec& operator=(hevc_codec&&) noexcept;

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
     * @brief Checks if this codec is configured for Main 10 Profile.
     * @return true if Main 10 (10-bit), false if Main (8-bit)
     */
    [[nodiscard]] bool is_main10_profile() const noexcept;

    /**
     * @brief Gets the current quality setting (CRF).
     * @return Quality value (0-51)
     */
    [[nodiscard]] int quality() const noexcept;

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
    bool main10_;
    int quality_;
};

}  // namespace kcenon::pacs::encoding::compression

#endif  // PACS_ENCODING_COMPRESSION_HEVC_CODEC_HPP
