/**
 * @file simd_windowing.hpp
 * @brief SIMD optimizations for window/level (VOI LUT) application
 *
 * Provides optimized window/level transformations for DICOM image display.
 * Window/level (also known as contrast/brightness) maps input pixel values
 * to display values based on window center and width parameters.
 *
 * Transformation formula:
 *   output = clamp((input - (center - width/2)) * 255 / width, 0, 255)
 *
 * @see DICOM PS3.3 C.11.2 - VOI LUT Module
 */

#ifndef PACS_ENCODING_SIMD_WINDOWING_HPP
#define PACS_ENCODING_SIMD_WINDOWING_HPP

#include "simd_config.hpp"
#include "simd_types.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace pacs::encoding::simd {

/**
 * @brief Window/Level parameters
 */
struct window_level_params {
    double center;     ///< Window center (level)
    double width;      ///< Window width
    bool invert;       ///< Invert output (for MONOCHROME1)

    constexpr window_level_params(double c = 128.0, double w = 256.0,
                                   bool inv = false) noexcept
        : center(c), width(w), invert(inv) {}
};

// Forward declarations
void apply_window_level_8bit(const uint8_t* src, uint8_t* dst,
                              size_t pixel_count,
                              const window_level_params& params) noexcept;
void apply_window_level_16bit(const uint16_t* src, uint8_t* dst,
                               size_t pixel_count,
                               const window_level_params& params) noexcept;
void apply_window_level_16bit_signed(const int16_t* src, uint8_t* dst,
                                      size_t pixel_count,
                                      const window_level_params& params) noexcept;

/**
 * @brief Precomputed LUT for fast repeated window/level application
 */
class window_level_lut {
public:
    /**
     * @brief Construct LUT for 8-bit input
     */
    static window_level_lut create_8bit(const window_level_params& params) {
        window_level_lut lut;
        lut.lut_8bit_.resize(256);

        const double min_val = params.center - params.width / 2.0;
        const double scale = 255.0 / params.width;

        for (int i = 0; i < 256; ++i) {
            double val = (i - min_val) * scale;
            val = std::clamp(val, 0.0, 255.0);
            if (params.invert) {
                val = 255.0 - val;
            }
            lut.lut_8bit_[i] = static_cast<uint8_t>(std::round(val));
        }

        return lut;
    }

    /**
     * @brief Construct LUT for 12-bit input
     */
    static window_level_lut create_12bit(const window_level_params& params) {
        window_level_lut lut;
        lut.lut_16bit_.resize(4096);

        const double min_val = params.center - params.width / 2.0;
        const double scale = 255.0 / params.width;

        for (int i = 0; i < 4096; ++i) {
            double val = (i - min_val) * scale;
            val = std::clamp(val, 0.0, 255.0);
            if (params.invert) {
                val = 255.0 - val;
            }
            lut.lut_16bit_[i] = static_cast<uint8_t>(std::round(val));
        }

        return lut;
    }

    /**
     * @brief Construct LUT for 16-bit input
     */
    static window_level_lut create_16bit(const window_level_params& params) {
        window_level_lut lut;
        lut.lut_16bit_.resize(65536);

        const double min_val = params.center - params.width / 2.0;
        const double scale = 255.0 / params.width;

        for (int i = 0; i < 65536; ++i) {
            double val = (i - min_val) * scale;
            val = std::clamp(val, 0.0, 255.0);
            if (params.invert) {
                val = 255.0 - val;
            }
            lut.lut_16bit_[i] = static_cast<uint8_t>(std::round(val));
        }

        return lut;
    }

    /**
     * @brief Apply LUT to 8-bit data
     */
    void apply_8bit(const uint8_t* src, uint8_t* dst,
                    size_t pixel_count) const noexcept {
        for (size_t i = 0; i < pixel_count; ++i) {
            dst[i] = lut_8bit_[src[i]];
        }
    }

    /**
     * @brief Apply LUT to 16-bit data (uses clamping for out-of-range values)
     */
    void apply_16bit(const uint16_t* src, uint8_t* dst,
                     size_t pixel_count) const noexcept {
        const size_t lut_size = lut_16bit_.size();
        for (size_t i = 0; i < pixel_count; ++i) {
            const uint16_t val = src[i];
            if (val < lut_size) {
                dst[i] = lut_16bit_[val];
            } else {
                dst[i] = lut_16bit_[lut_size - 1];
            }
        }
    }

    [[nodiscard]] bool is_valid_8bit() const noexcept {
        return !lut_8bit_.empty();
    }

    [[nodiscard]] bool is_valid_16bit() const noexcept {
        return !lut_16bit_.empty();
    }

private:
    std::vector<uint8_t> lut_8bit_;
    std::vector<uint8_t> lut_16bit_;
};

namespace detail {

// ============================================================================
// Scalar fallback implementations
// ============================================================================

/**
 * @brief Scalar 8-bit window/level application
 */
inline void apply_window_level_8bit_scalar(const uint8_t* src, uint8_t* dst,
                                            size_t pixel_count,
                                            const window_level_params& params) noexcept {
    const double min_val = params.center - params.width / 2.0;
    const double scale = 255.0 / params.width;

    for (size_t i = 0; i < pixel_count; ++i) {
        double val = (src[i] - min_val) * scale;
        val = std::clamp(val, 0.0, 255.0);
        if (params.invert) {
            val = 255.0 - val;
        }
        dst[i] = static_cast<uint8_t>(val);
    }
}

/**
 * @brief Scalar 16-bit window/level application
 */
inline void apply_window_level_16bit_scalar(const uint16_t* src, uint8_t* dst,
                                             size_t pixel_count,
                                             const window_level_params& params) noexcept {
    const double min_val = params.center - params.width / 2.0;
    const double scale = 255.0 / params.width;

    for (size_t i = 0; i < pixel_count; ++i) {
        double val = (src[i] - min_val) * scale;
        val = std::clamp(val, 0.0, 255.0);
        if (params.invert) {
            val = 255.0 - val;
        }
        dst[i] = static_cast<uint8_t>(val);
    }
}

/**
 * @brief Scalar signed 16-bit window/level application
 */
inline void apply_window_level_16bit_signed_scalar(
    const int16_t* src, uint8_t* dst, size_t pixel_count,
    const window_level_params& params) noexcept {
    const double min_val = params.center - params.width / 2.0;
    const double scale = 255.0 / params.width;

    for (size_t i = 0; i < pixel_count; ++i) {
        double val = (src[i] - min_val) * scale;
        val = std::clamp(val, 0.0, 255.0);
        if (params.invert) {
            val = 255.0 - val;
        }
        dst[i] = static_cast<uint8_t>(val);
    }
}

// ============================================================================
// SSE2 implementations
// ============================================================================

#if defined(PACS_SIMD_SSE2)

/**
 * @brief SSE2 8-bit window/level using fixed-point arithmetic
 *
 * Uses 16-bit fixed-point multiplication for efficiency.
 * Processes 16 pixels per iteration.
 */
inline void apply_window_level_8bit_sse2(const uint8_t* src, uint8_t* dst,
                                          size_t pixel_count,
                                          const window_level_params& params) noexcept {
    // Precompute fixed-point parameters (16.16 format)
    const int32_t min_val_fp =
        static_cast<int32_t>((params.center - params.width / 2.0) * 256);
    const int32_t scale_fp =
        static_cast<int32_t>((255.0 / params.width) * 256);

    const __m128i min_vec = _mm_set1_epi16(static_cast<int16_t>(min_val_fp >> 8));
    const __m128i scale_vec = _mm_set1_epi16(static_cast<int16_t>(scale_fp));
    const __m128i zero = _mm_setzero_si128();
    const __m128i max_255 = _mm_set1_epi16(255);
    const __m128i all_ones = _mm_set1_epi8(static_cast<char>(0xFF));

    const size_t simd_count = (pixel_count / 16) * 16;

    size_t i = 0;
    for (; i < simd_count; i += 16) {
        // Load 16 pixels
        __m128i pixels = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i));

        // Unpack to 16-bit (low 8 pixels)
        __m128i pixels_lo = _mm_unpacklo_epi8(pixels, zero);
        // Unpack to 16-bit (high 8 pixels)
        __m128i pixels_hi = _mm_unpackhi_epi8(pixels, zero);

        // Subtract minimum value
        pixels_lo = _mm_sub_epi16(pixels_lo, min_vec);
        pixels_hi = _mm_sub_epi16(pixels_hi, min_vec);

        // Multiply by scale (fixed-point, keep high 16 bits)
        pixels_lo = _mm_mulhi_epi16(pixels_lo, scale_vec);
        pixels_hi = _mm_mulhi_epi16(pixels_hi, scale_vec);

        // Clamp to [0, 255]
        pixels_lo = _mm_max_epi16(_mm_min_epi16(pixels_lo, max_255), zero);
        pixels_hi = _mm_max_epi16(_mm_min_epi16(pixels_hi, max_255), zero);

        // Pack back to 8-bit
        __m128i result = _mm_packus_epi16(pixels_lo, pixels_hi);

        // Apply inversion if needed
        if (params.invert) {
            result = _mm_xor_si128(result, all_ones);
        }

        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + i), result);
    }

    // Handle remainder
    apply_window_level_8bit_scalar(src + i, dst + i, pixel_count - i, params);
}

/**
 * @brief SSE2 16-bit window/level
 * Processes 8 pixels per iteration
 */
inline void apply_window_level_16bit_sse2(const uint16_t* src, uint8_t* dst,
                                           size_t pixel_count,
                                           const window_level_params& params) noexcept {
    // For 16-bit input, we need 32-bit arithmetic
    const float min_val = static_cast<float>(params.center - params.width / 2.0);
    const float scale = 255.0f / static_cast<float>(params.width);

    const __m128 min_vec = _mm_set1_ps(min_val);
    const __m128 scale_vec = _mm_set1_ps(scale);
    const __m128 zero_f = _mm_setzero_ps();
    const __m128 max_255_f = _mm_set1_ps(255.0f);
    const __m128i all_ones = _mm_set1_epi8(static_cast<char>(0xFF));

    const size_t simd_count = (pixel_count / 8) * 8;

    size_t i = 0;
    for (; i < simd_count; i += 8) {
        // Load 8 16-bit pixels
        __m128i pixels = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i));

        // Convert to float (need to split into two 4-element vectors)
        __m128i lo = _mm_unpacklo_epi16(pixels, _mm_setzero_si128());
        __m128i hi = _mm_unpackhi_epi16(pixels, _mm_setzero_si128());

        __m128 lo_f = _mm_cvtepi32_ps(lo);
        __m128 hi_f = _mm_cvtepi32_ps(hi);

        // Apply window/level: (pixel - min) * scale
        lo_f = _mm_mul_ps(_mm_sub_ps(lo_f, min_vec), scale_vec);
        hi_f = _mm_mul_ps(_mm_sub_ps(hi_f, min_vec), scale_vec);

        // Clamp to [0, 255]
        lo_f = _mm_max_ps(_mm_min_ps(lo_f, max_255_f), zero_f);
        hi_f = _mm_max_ps(_mm_min_ps(hi_f, max_255_f), zero_f);

        // Convert back to integer
        __m128i lo_i = _mm_cvtps_epi32(lo_f);
        __m128i hi_i = _mm_cvtps_epi32(hi_f);

        // Pack to 16-bit then 8-bit
        __m128i packed16 = _mm_packs_epi32(lo_i, hi_i);
        __m128i packed8 = _mm_packus_epi16(packed16, packed16);

        // Apply inversion if needed
        if (params.invert) {
            packed8 = _mm_xor_si128(packed8, all_ones);
        }

        // Store 8 bytes
        _mm_storel_epi64(reinterpret_cast<__m128i*>(dst + i), packed8);
    }

    // Handle remainder
    apply_window_level_16bit_scalar(src + i, dst + i, pixel_count - i, params);
}

/**
 * @brief SSE2 signed 16-bit window/level
 */
inline void apply_window_level_16bit_signed_sse2(
    const int16_t* src, uint8_t* dst, size_t pixel_count,
    const window_level_params& params) noexcept {
    const float min_val = static_cast<float>(params.center - params.width / 2.0);
    const float scale = 255.0f / static_cast<float>(params.width);

    const __m128 min_vec = _mm_set1_ps(min_val);
    const __m128 scale_vec = _mm_set1_ps(scale);
    const __m128 zero_f = _mm_setzero_ps();
    const __m128 max_255_f = _mm_set1_ps(255.0f);
    const __m128i all_ones = _mm_set1_epi8(static_cast<char>(0xFF));

    const size_t simd_count = (pixel_count / 8) * 8;

    size_t i = 0;
    for (; i < simd_count; i += 8) {
        __m128i pixels = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i));

        // Sign-extend to 32-bit
        __m128i lo = _mm_srai_epi32(_mm_unpacklo_epi16(pixels, pixels), 16);
        __m128i hi = _mm_srai_epi32(_mm_unpackhi_epi16(pixels, pixels), 16);

        __m128 lo_f = _mm_cvtepi32_ps(lo);
        __m128 hi_f = _mm_cvtepi32_ps(hi);

        lo_f = _mm_mul_ps(_mm_sub_ps(lo_f, min_vec), scale_vec);
        hi_f = _mm_mul_ps(_mm_sub_ps(hi_f, min_vec), scale_vec);

        lo_f = _mm_max_ps(_mm_min_ps(lo_f, max_255_f), zero_f);
        hi_f = _mm_max_ps(_mm_min_ps(hi_f, max_255_f), zero_f);

        __m128i lo_i = _mm_cvtps_epi32(lo_f);
        __m128i hi_i = _mm_cvtps_epi32(hi_f);

        __m128i packed16 = _mm_packs_epi32(lo_i, hi_i);
        __m128i packed8 = _mm_packus_epi16(packed16, packed16);

        if (params.invert) {
            packed8 = _mm_xor_si128(packed8, all_ones);
        }

        _mm_storel_epi64(reinterpret_cast<__m128i*>(dst + i), packed8);
    }

    apply_window_level_16bit_signed_scalar(src + i, dst + i, pixel_count - i, params);
}

#endif  // PACS_SIMD_SSE2

// ============================================================================
// AVX2 implementations
// ============================================================================

#if defined(PACS_SIMD_AVX2)

/**
 * @brief AVX2 8-bit window/level
 * Processes 32 pixels per iteration
 */
inline void apply_window_level_8bit_avx2(const uint8_t* src, uint8_t* dst,
                                          size_t pixel_count,
                                          const window_level_params& params) noexcept {
    const int32_t min_val_fp =
        static_cast<int32_t>((params.center - params.width / 2.0) * 256);
    const int32_t scale_fp =
        static_cast<int32_t>((255.0 / params.width) * 256);

    const __m256i min_vec = _mm256_set1_epi16(static_cast<int16_t>(min_val_fp >> 8));
    const __m256i scale_vec = _mm256_set1_epi16(static_cast<int16_t>(scale_fp));
    const __m256i zero = _mm256_setzero_si256();
    const __m256i max_255 = _mm256_set1_epi16(255);
    const __m256i all_ones = _mm256_set1_epi8(static_cast<char>(0xFF));

    const size_t simd_count = (pixel_count / 32) * 32;

    size_t i = 0;
    for (; i < simd_count; i += 32) {
        __m256i pixels = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i));

        // Unpack to 16-bit
        __m256i pixels_lo = _mm256_unpacklo_epi8(pixels, zero);
        __m256i pixels_hi = _mm256_unpackhi_epi8(pixels, zero);

        // Subtract minimum and multiply by scale
        pixels_lo = _mm256_sub_epi16(pixels_lo, min_vec);
        pixels_hi = _mm256_sub_epi16(pixels_hi, min_vec);

        pixels_lo = _mm256_mulhi_epi16(pixels_lo, scale_vec);
        pixels_hi = _mm256_mulhi_epi16(pixels_hi, scale_vec);

        // Clamp
        pixels_lo = _mm256_max_epi16(_mm256_min_epi16(pixels_lo, max_255), zero);
        pixels_hi = _mm256_max_epi16(_mm256_min_epi16(pixels_hi, max_255), zero);

        // Pack back to 8-bit
        __m256i result = _mm256_packus_epi16(pixels_lo, pixels_hi);

        // Fix lane crossing issue with packus
        result = _mm256_permute4x64_epi64(result, 0xD8);

        if (params.invert) {
            result = _mm256_xor_si256(result, all_ones);
        }

        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i), result);
    }

    // Handle remainder with SSE2
#if defined(PACS_SIMD_SSE2)
    if (pixel_count - i >= 16) {
        apply_window_level_8bit_sse2(src + i, dst + i, pixel_count - i, params);
    } else
#endif
    {
        apply_window_level_8bit_scalar(src + i, dst + i, pixel_count - i, params);
    }
}

/**
 * @brief AVX2 16-bit window/level
 * Processes 16 pixels per iteration
 */
inline void apply_window_level_16bit_avx2(const uint16_t* src, uint8_t* dst,
                                           size_t pixel_count,
                                           const window_level_params& params) noexcept {
    const float min_val = static_cast<float>(params.center - params.width / 2.0);
    const float scale = 255.0f / static_cast<float>(params.width);

    const __m256 min_vec = _mm256_set1_ps(min_val);
    const __m256 scale_vec = _mm256_set1_ps(scale);
    const __m256 zero_f = _mm256_setzero_ps();
    const __m256 max_255_f = _mm256_set1_ps(255.0f);
    const __m128i all_ones_128 = _mm_set1_epi8(static_cast<char>(0xFF));

    const size_t simd_count = (pixel_count / 16) * 16;

    size_t i = 0;
    for (; i < simd_count; i += 16) {
        // Load 16 16-bit pixels
        __m256i pixels = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i));

        // Convert to 32-bit and float
        __m128i lo_128 = _mm256_castsi256_si128(pixels);
        __m128i hi_128 = _mm256_extracti128_si256(pixels, 1);

        __m256i lo_32 = _mm256_cvtepu16_epi32(lo_128);
        __m256i hi_32 = _mm256_cvtepu16_epi32(hi_128);

        __m256 lo_f = _mm256_cvtepi32_ps(lo_32);
        __m256 hi_f = _mm256_cvtepi32_ps(hi_32);

        // Apply transformation
        lo_f = _mm256_mul_ps(_mm256_sub_ps(lo_f, min_vec), scale_vec);
        hi_f = _mm256_mul_ps(_mm256_sub_ps(hi_f, min_vec), scale_vec);

        // Clamp
        lo_f = _mm256_max_ps(_mm256_min_ps(lo_f, max_255_f), zero_f);
        hi_f = _mm256_max_ps(_mm256_min_ps(hi_f, max_255_f), zero_f);

        // Convert back to integer
        __m256i lo_i = _mm256_cvtps_epi32(lo_f);
        __m256i hi_i = _mm256_cvtps_epi32(hi_f);

        // Pack to 16-bit
        __m256i packed16 = _mm256_packs_epi32(lo_i, hi_i);
        packed16 = _mm256_permute4x64_epi64(packed16, 0xD8);

        // Pack to 8-bit
        __m128i lo_16 = _mm256_castsi256_si128(packed16);
        __m128i hi_16 = _mm256_extracti128_si256(packed16, 1);
        __m128i packed8 = _mm_packus_epi16(lo_16, hi_16);

        if (params.invert) {
            packed8 = _mm_xor_si128(packed8, all_ones_128);
        }

        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + i), packed8);
    }

    // Handle remainder
#if defined(PACS_SIMD_SSE2)
    if (pixel_count - i >= 8) {
        apply_window_level_16bit_sse2(src + i, dst + i, pixel_count - i, params);
    } else
#endif
    {
        apply_window_level_16bit_scalar(src + i, dst + i, pixel_count - i, params);
    }
}

/**
 * @brief AVX2 signed 16-bit window/level
 */
inline void apply_window_level_16bit_signed_avx2(
    const int16_t* src, uint8_t* dst, size_t pixel_count,
    const window_level_params& params) noexcept {
    const float min_val = static_cast<float>(params.center - params.width / 2.0);
    const float scale = 255.0f / static_cast<float>(params.width);

    const __m256 min_vec = _mm256_set1_ps(min_val);
    const __m256 scale_vec = _mm256_set1_ps(scale);
    const __m256 zero_f = _mm256_setzero_ps();
    const __m256 max_255_f = _mm256_set1_ps(255.0f);
    const __m128i all_ones_128 = _mm_set1_epi8(static_cast<char>(0xFF));

    const size_t simd_count = (pixel_count / 16) * 16;

    size_t i = 0;
    for (; i < simd_count; i += 16) {
        __m256i pixels = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i));

        // Sign-extend to 32-bit
        __m128i lo_128 = _mm256_castsi256_si128(pixels);
        __m128i hi_128 = _mm256_extracti128_si256(pixels, 1);

        __m256i lo_32 = _mm256_cvtepi16_epi32(lo_128);
        __m256i hi_32 = _mm256_cvtepi16_epi32(hi_128);

        __m256 lo_f = _mm256_cvtepi32_ps(lo_32);
        __m256 hi_f = _mm256_cvtepi32_ps(hi_32);

        lo_f = _mm256_mul_ps(_mm256_sub_ps(lo_f, min_vec), scale_vec);
        hi_f = _mm256_mul_ps(_mm256_sub_ps(hi_f, min_vec), scale_vec);

        lo_f = _mm256_max_ps(_mm256_min_ps(lo_f, max_255_f), zero_f);
        hi_f = _mm256_max_ps(_mm256_min_ps(hi_f, max_255_f), zero_f);

        __m256i lo_i = _mm256_cvtps_epi32(lo_f);
        __m256i hi_i = _mm256_cvtps_epi32(hi_f);

        __m256i packed16 = _mm256_packs_epi32(lo_i, hi_i);
        packed16 = _mm256_permute4x64_epi64(packed16, 0xD8);

        __m128i lo_16 = _mm256_castsi256_si128(packed16);
        __m128i hi_16 = _mm256_extracti128_si256(packed16, 1);
        __m128i packed8 = _mm_packus_epi16(lo_16, hi_16);

        if (params.invert) {
            packed8 = _mm_xor_si128(packed8, all_ones_128);
        }

        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + i), packed8);
    }

#if defined(PACS_SIMD_SSE2)
    if (pixel_count - i >= 8) {
        apply_window_level_16bit_signed_sse2(src + i, dst + i, pixel_count - i, params);
    } else
#endif
    {
        apply_window_level_16bit_signed_scalar(src + i, dst + i, pixel_count - i, params);
    }
}

#endif  // PACS_SIMD_AVX2

// ============================================================================
// NEON implementations (ARM)
// ============================================================================

#if defined(PACS_SIMD_NEON)

/**
 * @brief NEON 8-bit window/level
 * Processes 16 pixels per iteration
 */
inline void apply_window_level_8bit_neon(const uint8_t* src, uint8_t* dst,
                                          size_t pixel_count,
                                          const window_level_params& params) noexcept {
    const float min_val = static_cast<float>(params.center - params.width / 2.0);
    const float scale = 255.0f / static_cast<float>(params.width);

    const float32x4_t min_vec = vdupq_n_f32(min_val);
    const float32x4_t scale_vec = vdupq_n_f32(scale);
    const float32x4_t zero_f = vdupq_n_f32(0.0f);
    const float32x4_t max_255_f = vdupq_n_f32(255.0f);
    const uint8x16_t all_ones = vdupq_n_u8(0xFF);

    const size_t simd_count = (pixel_count / 16) * 16;

    size_t i = 0;
    for (; i < simd_count; i += 16) {
        uint8x16_t pixels = vld1q_u8(src + i);

        // Process in 4-element chunks (NEON doesn't have 8-wide float)
        uint8x8_t lo8 = vget_low_u8(pixels);
        uint8x8_t hi8 = vget_high_u8(pixels);

        uint16x8_t lo16 = vmovl_u8(lo8);
        uint16x8_t hi16 = vmovl_u8(hi8);

        // Process 4 groups of 4 pixels each
        uint32x4_t p0 = vmovl_u16(vget_low_u16(lo16));
        uint32x4_t p1 = vmovl_u16(vget_high_u16(lo16));
        uint32x4_t p2 = vmovl_u16(vget_low_u16(hi16));
        uint32x4_t p3 = vmovl_u16(vget_high_u16(hi16));

        float32x4_t f0 = vcvtq_f32_u32(p0);
        float32x4_t f1 = vcvtq_f32_u32(p1);
        float32x4_t f2 = vcvtq_f32_u32(p2);
        float32x4_t f3 = vcvtq_f32_u32(p3);

        // Apply transformation
        f0 = vmulq_f32(vsubq_f32(f0, min_vec), scale_vec);
        f1 = vmulq_f32(vsubq_f32(f1, min_vec), scale_vec);
        f2 = vmulq_f32(vsubq_f32(f2, min_vec), scale_vec);
        f3 = vmulq_f32(vsubq_f32(f3, min_vec), scale_vec);

        // Clamp
        f0 = vmaxq_f32(vminq_f32(f0, max_255_f), zero_f);
        f1 = vmaxq_f32(vminq_f32(f1, max_255_f), zero_f);
        f2 = vmaxq_f32(vminq_f32(f2, max_255_f), zero_f);
        f3 = vmaxq_f32(vminq_f32(f3, max_255_f), zero_f);

        // Convert back to integer
        uint32x4_t i0 = vcvtq_u32_f32(f0);
        uint32x4_t i1 = vcvtq_u32_f32(f1);
        uint32x4_t i2 = vcvtq_u32_f32(f2);
        uint32x4_t i3 = vcvtq_u32_f32(f3);

        // Narrow to 16-bit then 8-bit
        uint16x4_t n0 = vmovn_u32(i0);
        uint16x4_t n1 = vmovn_u32(i1);
        uint16x4_t n2 = vmovn_u32(i2);
        uint16x4_t n3 = vmovn_u32(i3);

        uint16x8_t n_lo = vcombine_u16(n0, n1);
        uint16x8_t n_hi = vcombine_u16(n2, n3);

        uint8x8_t r_lo = vmovn_u16(n_lo);
        uint8x8_t r_hi = vmovn_u16(n_hi);

        uint8x16_t result = vcombine_u8(r_lo, r_hi);

        if (params.invert) {
            result = veorq_u8(result, all_ones);
        }

        vst1q_u8(dst + i, result);
    }

    apply_window_level_8bit_scalar(src + i, dst + i, pixel_count - i, params);
}

/**
 * @brief NEON 16-bit window/level
 */
inline void apply_window_level_16bit_neon(const uint16_t* src, uint8_t* dst,
                                           size_t pixel_count,
                                           const window_level_params& params) noexcept {
    const float min_val = static_cast<float>(params.center - params.width / 2.0);
    const float scale = 255.0f / static_cast<float>(params.width);

    const float32x4_t min_vec = vdupq_n_f32(min_val);
    const float32x4_t scale_vec = vdupq_n_f32(scale);
    const float32x4_t zero_f = vdupq_n_f32(0.0f);
    const float32x4_t max_255_f = vdupq_n_f32(255.0f);
    const uint8x8_t all_ones = vdup_n_u8(0xFF);

    const size_t simd_count = (pixel_count / 8) * 8;

    size_t i = 0;
    for (; i < simd_count; i += 8) {
        uint16x8_t pixels = vld1q_u16(src + i);

        uint32x4_t lo32 = vmovl_u16(vget_low_u16(pixels));
        uint32x4_t hi32 = vmovl_u16(vget_high_u16(pixels));

        float32x4_t lo_f = vcvtq_f32_u32(lo32);
        float32x4_t hi_f = vcvtq_f32_u32(hi32);

        lo_f = vmulq_f32(vsubq_f32(lo_f, min_vec), scale_vec);
        hi_f = vmulq_f32(vsubq_f32(hi_f, min_vec), scale_vec);

        lo_f = vmaxq_f32(vminq_f32(lo_f, max_255_f), zero_f);
        hi_f = vmaxq_f32(vminq_f32(hi_f, max_255_f), zero_f);

        uint32x4_t lo_i = vcvtq_u32_f32(lo_f);
        uint32x4_t hi_i = vcvtq_u32_f32(hi_f);

        uint16x4_t lo16 = vmovn_u32(lo_i);
        uint16x4_t hi16 = vmovn_u32(hi_i);

        uint16x8_t packed16 = vcombine_u16(lo16, hi16);
        uint8x8_t packed8 = vmovn_u16(packed16);

        if (params.invert) {
            packed8 = veor_u8(packed8, all_ones);
        }

        vst1_u8(dst + i, packed8);
    }

    apply_window_level_16bit_scalar(src + i, dst + i, pixel_count - i, params);
}

/**
 * @brief NEON signed 16-bit window/level
 */
inline void apply_window_level_16bit_signed_neon(
    const int16_t* src, uint8_t* dst, size_t pixel_count,
    const window_level_params& params) noexcept {
    const float min_val = static_cast<float>(params.center - params.width / 2.0);
    const float scale = 255.0f / static_cast<float>(params.width);

    const float32x4_t min_vec = vdupq_n_f32(min_val);
    const float32x4_t scale_vec = vdupq_n_f32(scale);
    const float32x4_t zero_f = vdupq_n_f32(0.0f);
    const float32x4_t max_255_f = vdupq_n_f32(255.0f);
    const uint8x8_t all_ones = vdup_n_u8(0xFF);

    const size_t simd_count = (pixel_count / 8) * 8;

    size_t i = 0;
    for (; i < simd_count; i += 8) {
        int16x8_t pixels = vld1q_s16(src + i);

        int32x4_t lo32 = vmovl_s16(vget_low_s16(pixels));
        int32x4_t hi32 = vmovl_s16(vget_high_s16(pixels));

        float32x4_t lo_f = vcvtq_f32_s32(lo32);
        float32x4_t hi_f = vcvtq_f32_s32(hi32);

        lo_f = vmulq_f32(vsubq_f32(lo_f, min_vec), scale_vec);
        hi_f = vmulq_f32(vsubq_f32(hi_f, min_vec), scale_vec);

        lo_f = vmaxq_f32(vminq_f32(lo_f, max_255_f), zero_f);
        hi_f = vmaxq_f32(vminq_f32(hi_f, max_255_f), zero_f);

        uint32x4_t lo_i = vcvtq_u32_f32(lo_f);
        uint32x4_t hi_i = vcvtq_u32_f32(hi_f);

        uint16x4_t lo16 = vmovn_u32(lo_i);
        uint16x4_t hi16 = vmovn_u32(hi_i);

        uint16x8_t packed16 = vcombine_u16(lo16, hi16);
        uint8x8_t packed8 = vmovn_u16(packed16);

        if (params.invert) {
            packed8 = veor_u8(packed8, all_ones);
        }

        vst1_u8(dst + i, packed8);
    }

    apply_window_level_16bit_signed_scalar(src + i, dst + i, pixel_count - i, params);
}

#endif  // PACS_SIMD_NEON

}  // namespace detail

// ============================================================================
// Public API with runtime dispatch
// ============================================================================

/**
 * @brief Apply window/level transformation to 8-bit grayscale data
 *
 * Maps input pixel values to output values based on window center and width.
 * Uses SIMD when available for better performance.
 *
 * @param src Source pixel data (8-bit grayscale)
 * @param dst Destination pixel data (8-bit display values)
 * @param pixel_count Number of pixels to process
 * @param params Window/level parameters
 */
inline void apply_window_level_8bit(const uint8_t* src, uint8_t* dst,
                                     size_t pixel_count,
                                     const window_level_params& params) noexcept {
#if defined(PACS_SIMD_AVX2)
    if (has_avx2()) {
        detail::apply_window_level_8bit_avx2(src, dst, pixel_count, params);
        return;
    }
#endif
#if defined(PACS_SIMD_SSE2)
    if (has_sse2()) {
        detail::apply_window_level_8bit_sse2(src, dst, pixel_count, params);
        return;
    }
#endif
#if defined(PACS_SIMD_NEON)
    if (has_neon()) {
        detail::apply_window_level_8bit_neon(src, dst, pixel_count, params);
        return;
    }
#endif
    detail::apply_window_level_8bit_scalar(src, dst, pixel_count, params);
}

/**
 * @brief Apply window/level transformation to 16-bit unsigned grayscale data
 *
 * @param src Source pixel data (16-bit unsigned grayscale)
 * @param dst Destination pixel data (8-bit display values)
 * @param pixel_count Number of pixels to process
 * @param params Window/level parameters
 */
inline void apply_window_level_16bit(const uint16_t* src, uint8_t* dst,
                                      size_t pixel_count,
                                      const window_level_params& params) noexcept {
#if defined(PACS_SIMD_AVX2)
    if (has_avx2()) {
        detail::apply_window_level_16bit_avx2(src, dst, pixel_count, params);
        return;
    }
#endif
#if defined(PACS_SIMD_SSE2)
    if (has_sse2()) {
        detail::apply_window_level_16bit_sse2(src, dst, pixel_count, params);
        return;
    }
#endif
#if defined(PACS_SIMD_NEON)
    if (has_neon()) {
        detail::apply_window_level_16bit_neon(src, dst, pixel_count, params);
        return;
    }
#endif
    detail::apply_window_level_16bit_scalar(src, dst, pixel_count, params);
}

/**
 * @brief Apply window/level transformation to 16-bit signed grayscale data
 *
 * Handles signed pixel data commonly found in CT images.
 *
 * @param src Source pixel data (16-bit signed grayscale)
 * @param dst Destination pixel data (8-bit display values)
 * @param pixel_count Number of pixels to process
 * @param params Window/level parameters
 */
inline void apply_window_level_16bit_signed(const int16_t* src, uint8_t* dst,
                                             size_t pixel_count,
                                             const window_level_params& params) noexcept {
#if defined(PACS_SIMD_AVX2)
    if (has_avx2()) {
        detail::apply_window_level_16bit_signed_avx2(src, dst, pixel_count, params);
        return;
    }
#endif
#if defined(PACS_SIMD_SSE2)
    if (has_sse2()) {
        detail::apply_window_level_16bit_signed_sse2(src, dst, pixel_count, params);
        return;
    }
#endif
#if defined(PACS_SIMD_NEON)
    if (has_neon()) {
        detail::apply_window_level_16bit_signed_neon(src, dst, pixel_count, params);
        return;
    }
#endif
    detail::apply_window_level_16bit_signed_scalar(src, dst, pixel_count, params);
}

}  // namespace pacs::encoding::simd

#endif  // PACS_ENCODING_SIMD_WINDOWING_HPP
