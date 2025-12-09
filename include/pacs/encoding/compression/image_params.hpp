#ifndef PACS_ENCODING_COMPRESSION_IMAGE_PARAMS_HPP
#define PACS_ENCODING_COMPRESSION_IMAGE_PARAMS_HPP

#include <cstdint>
#include <string>

namespace pacs::encoding::compression {

/**
 * @brief Photometric interpretation of pixel data.
 *
 * Defines how pixel values should be interpreted for display.
 * @see DICOM PS3.3 Section C.7.6.3.1.2
 */
enum class photometric_interpretation {
    monochrome1,     ///< Minimum pixel value displayed as white
    monochrome2,     ///< Minimum pixel value displayed as black
    rgb,             ///< Red, Green, Blue color model
    ycbcr_full,      ///< YCbCr full range (JPEG standard)
    ycbcr_full_422,  ///< YCbCr 4:2:2 subsampling
    palette_color,   ///< Palette color lookup table
    unknown          ///< Unknown or unsupported interpretation
};

/**
 * @brief Converts photometric interpretation to DICOM string value.
 * @param pi The photometric interpretation enum value
 * @return DICOM-compliant string representation
 */
[[nodiscard]] inline std::string to_string(photometric_interpretation pi) {
    switch (pi) {
        case photometric_interpretation::monochrome1:
            return "MONOCHROME1";
        case photometric_interpretation::monochrome2:
            return "MONOCHROME2";
        case photometric_interpretation::rgb:
            return "RGB";
        case photometric_interpretation::ycbcr_full:
            return "YBR_FULL";
        case photometric_interpretation::ycbcr_full_422:
            return "YBR_FULL_422";
        case photometric_interpretation::palette_color:
            return "PALETTE COLOR";
        default:
            return "UNKNOWN";
    }
}

/**
 * @brief Parses a DICOM photometric interpretation string.
 * @param str The DICOM string value
 * @return The corresponding enum value
 */
[[nodiscard]] inline photometric_interpretation parse_photometric_interpretation(
    const std::string& str) {
    if (str == "MONOCHROME1") return photometric_interpretation::monochrome1;
    if (str == "MONOCHROME2") return photometric_interpretation::monochrome2;
    if (str == "RGB") return photometric_interpretation::rgb;
    if (str == "YBR_FULL") return photometric_interpretation::ycbcr_full;
    if (str == "YBR_FULL_422") return photometric_interpretation::ycbcr_full_422;
    if (str == "PALETTE COLOR") return photometric_interpretation::palette_color;
    return photometric_interpretation::unknown;
}

/**
 * @brief Parameters describing image pixel data.
 *
 * Contains all DICOM attributes needed for image compression/decompression.
 * Maps directly to DICOM tags in Image Pixel Module (C.7.6.3).
 */
struct image_params {
    /// Image width in pixels (Columns - 0028,0011)
    uint16_t width{0};

    /// Image height in pixels (Rows - 0028,0010)
    uint16_t height{0};

    /// Bits allocated per pixel sample (0028,0100)
    /// Valid values: 8, 16
    uint16_t bits_allocated{0};

    /// Bits stored per pixel sample (0028,0101)
    /// Must be <= bits_allocated
    uint16_t bits_stored{0};

    /// High bit position (0028,0102)
    /// Typically bits_stored - 1
    uint16_t high_bit{0};

    /// Number of samples per pixel (0028,0002)
    /// 1 for grayscale, 3 for color
    uint16_t samples_per_pixel{1};

    /// Planar configuration (0028,0006)
    /// 0 = interleaved (R1G1B1R2G2B2...), 1 = separate planes (RRR...GGG...BBB...)
    uint16_t planar_configuration{0};

    /// Pixel representation (0028,0103)
    /// 0 = unsigned, 1 = signed
    uint16_t pixel_representation{0};

    /// Photometric interpretation (0028,0004)
    photometric_interpretation photometric{photometric_interpretation::monochrome2};

    /// Number of frames in multi-frame image (0028,0008)
    uint32_t number_of_frames{1};

    /**
     * @brief Calculates the size of uncompressed pixel data in bytes.
     * @return Size in bytes for a single frame
     */
    [[nodiscard]] size_t frame_size_bytes() const noexcept {
        size_t bits_per_pixel = static_cast<size_t>(bits_allocated) * samples_per_pixel;
        size_t total_bits = static_cast<size_t>(width) * height * bits_per_pixel;
        return (total_bits + 7) / 8;  // Round up to byte boundary
    }

    /**
     * @brief Checks if the image is grayscale (single sample per pixel).
     * @return true if grayscale
     */
    [[nodiscard]] bool is_grayscale() const noexcept {
        return samples_per_pixel == 1;
    }

    /**
     * @brief Checks if the image is color (multiple samples per pixel).
     * @return true if color
     */
    [[nodiscard]] bool is_color() const noexcept {
        return samples_per_pixel > 1;
    }

    /**
     * @brief Checks if pixel values are signed integers.
     * @return true if signed
     */
    [[nodiscard]] bool is_signed() const noexcept {
        return pixel_representation == 1;
    }

    /**
     * @brief Validates image parameters for JPEG Baseline compression.
     * @return true if parameters are valid for JPEG Baseline
     *
     * JPEG Baseline requirements:
     * - 8-bit samples only
     * - 1 (grayscale) or 3 (color) samples per pixel
     */
    [[nodiscard]] bool valid_for_jpeg_baseline() const noexcept {
        if (bits_allocated != 8 || bits_stored != 8) return false;
        if (samples_per_pixel != 1 && samples_per_pixel != 3) return false;
        return true;
    }

    /**
     * @brief Validates image parameters for JPEG Lossless compression.
     * @return true if parameters are valid for JPEG Lossless
     *
     * JPEG Lossless requirements:
     * - 2-16 bit precision
     * - bits_allocated must be 8 or 16
     * - Grayscale only (samples_per_pixel = 1)
     */
    [[nodiscard]] bool valid_for_jpeg_lossless() const noexcept {
        if (bits_stored < 2 || bits_stored > 16) return false;
        if (bits_allocated != 8 && bits_allocated != 16) return false;
        if (samples_per_pixel != 1) return false;
        return true;
    }

    /**
     * @brief Validates image parameters for JPEG 2000 compression.
     * @return true if parameters are valid for JPEG 2000
     *
     * JPEG 2000 requirements:
     * - 1-16 bit precision (per component)
     * - bits_allocated must be 8 or 16
     * - 1 (grayscale) or 3 (color) samples per pixel
     * - Valid dimensions (non-zero)
     */
    [[nodiscard]] bool valid_for_jpeg2000() const noexcept {
        if (bits_stored < 1 || bits_stored > 16) return false;
        if (bits_allocated != 8 && bits_allocated != 16) return false;
        if (samples_per_pixel != 1 && samples_per_pixel != 3) return false;
        if (width == 0 || height == 0) return false;
        return true;
    }

    /**
     * @brief Validates image parameters for JPEG-LS compression.
     * @return true if parameters are valid for JPEG-LS
     *
     * JPEG-LS requirements:
     * - 2-16 bit precision (per component)
     * - bits_allocated must be 8 or 16
     * - 1 (grayscale) or 3 (color) samples per pixel
     * - Valid dimensions (non-zero, max 65535x65535)
     */
    [[nodiscard]] bool valid_for_jpeg_ls() const noexcept {
        if (bits_stored < 2 || bits_stored > 16) return false;
        if (bits_allocated != 8 && bits_allocated != 16) return false;
        if (samples_per_pixel != 1 && samples_per_pixel != 3) return false;
        if (width == 0 || height == 0) return false;
        if (width > 65535 || height > 65535) return false;
        return true;
    }

    /**
     * @brief Validates image parameters for RLE Lossless compression.
     * @return true if parameters are valid for RLE
     *
     * RLE Lossless requirements:
     * - bits_allocated must be 8 or 16
     * - 1-3 samples per pixel (grayscale or RGB)
     * - Valid dimensions (non-zero, max 65535x65535)
     * - Maximum 15 segments (samples_per_pixel * bytes_per_sample)
     */
    [[nodiscard]] bool valid_for_rle() const noexcept {
        if (bits_allocated != 8 && bits_allocated != 16) return false;
        if (samples_per_pixel < 1 || samples_per_pixel > 3) return false;
        if (width == 0 || height == 0) return false;
        if (width > 65535 || height > 65535) return false;
        // Check segment count limit (max 15 segments)
        int bytes_per_sample = (bits_allocated + 7) / 8;
        int num_segments = samples_per_pixel * bytes_per_sample;
        if (num_segments > 15) return false;
        return true;
    }
};

}  // namespace pacs::encoding::compression

#endif  // PACS_ENCODING_COMPRESSION_IMAGE_PARAMS_HPP
