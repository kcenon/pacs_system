/**
 * @file simd_photometric.hpp
 * @brief SIMD optimizations for photometric interpretation conversions
 *
 * Provides optimized pixel value transformations for DICOM photometric
 * interpretation conversions:
 * - MONOCHROME1 <-> MONOCHROME2 (pixel inversion)
 * - RGB <-> YCbCr color space conversion
 *
 * @see DICOM PS3.3 C.7.6.3.1 - Photometric Interpretation
 */

#ifndef PACS_ENCODING_SIMD_PHOTOMETRIC_HPP
#define PACS_ENCODING_SIMD_PHOTOMETRIC_HPP

#include "simd_config.hpp"
#include "simd_types.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace pacs::encoding::simd {

// Forward declarations
void invert_monochrome_8bit(const uint8_t* src, uint8_t* dst,
                             size_t pixel_count) noexcept;
void invert_monochrome_16bit(const uint16_t* src, uint16_t* dst,
                              size_t pixel_count, uint16_t max_value) noexcept;
void rgb_to_ycbcr_8bit(const uint8_t* src, uint8_t* dst,
                        size_t pixel_count) noexcept;
void ycbcr_to_rgb_8bit(const uint8_t* src, uint8_t* dst,
                        size_t pixel_count) noexcept;

namespace detail {

// ============================================================================
// Scalar fallback implementations
// ============================================================================

/**
 * @brief Scalar 8-bit monochrome inversion (MONOCHROME1 <-> MONOCHROME2)
 */
inline void invert_monochrome_8bit_scalar(const uint8_t* src, uint8_t* dst,
                                           size_t pixel_count) noexcept {
    for (size_t i = 0; i < pixel_count; ++i) {
        dst[i] = 255 - src[i];
    }
}

/**
 * @brief Scalar 16-bit monochrome inversion
 * @param max_value Maximum pixel value (e.g., 4095 for 12-bit, 65535 for 16-bit)
 */
inline void invert_monochrome_16bit_scalar(const uint16_t* src, uint16_t* dst,
                                            size_t pixel_count,
                                            uint16_t max_value) noexcept {
    for (size_t i = 0; i < pixel_count; ++i) {
        dst[i] = max_value - src[i];
    }
}

/**
 * @brief Scalar RGB to YCbCr conversion (ITU-R BT.601)
 *
 * Y  =  0.299*R + 0.587*G + 0.114*B
 * Cb = -0.169*R - 0.331*G + 0.500*B + 128
 * Cr =  0.500*R - 0.419*G - 0.081*B + 128
 */
inline void rgb_to_ycbcr_8bit_scalar(const uint8_t* src, uint8_t* dst,
                                      size_t pixel_count) noexcept {
    for (size_t i = 0; i < pixel_count; ++i) {
        const int r = src[i * 3];
        const int g = src[i * 3 + 1];
        const int b = src[i * 3 + 2];

        // Fixed-point arithmetic (16.16 format for precision)
        // Coefficients scaled by 65536
        const int y = (19595 * r + 38470 * g + 7471 * b + 32768) >> 16;
        const int cb = (-11056 * r - 21712 * g + 32768 * b + 32768) >> 16;
        const int cr = (32768 * r - 27440 * g - 5328 * b + 32768) >> 16;

        dst[i * 3] = static_cast<uint8_t>(y < 0 ? 0 : (y > 255 ? 255 : y));
        dst[i * 3 + 1] = static_cast<uint8_t>(cb + 128);
        dst[i * 3 + 2] = static_cast<uint8_t>(cr + 128);
    }
}

/**
 * @brief Scalar YCbCr to RGB conversion (ITU-R BT.601)
 *
 * R = Y + 1.402*(Cr-128)
 * G = Y - 0.344*(Cb-128) - 0.714*(Cr-128)
 * B = Y + 1.772*(Cb-128)
 */
inline void ycbcr_to_rgb_8bit_scalar(const uint8_t* src, uint8_t* dst,
                                      size_t pixel_count) noexcept {
    for (size_t i = 0; i < pixel_count; ++i) {
        const int y = src[i * 3];
        const int cb = src[i * 3 + 1] - 128;
        const int cr = src[i * 3 + 2] - 128;

        // Fixed-point arithmetic (coefficients scaled by 65536)
        const int r = y + ((91881 * cr + 32768) >> 16);
        const int g = y - ((22554 * cb + 46802 * cr + 32768) >> 16);
        const int b = y + ((116130 * cb + 32768) >> 16);

        dst[i * 3] = static_cast<uint8_t>(r < 0 ? 0 : (r > 255 ? 255 : r));
        dst[i * 3 + 1] = static_cast<uint8_t>(g < 0 ? 0 : (g > 255 ? 255 : g));
        dst[i * 3 + 2] = static_cast<uint8_t>(b < 0 ? 0 : (b > 255 ? 255 : b));
    }
}

// ============================================================================
// SSE2 implementations
// ============================================================================

#if defined(PACS_SIMD_SSE2)

/**
 * @brief SSE2 8-bit monochrome inversion
 * Processes 16 pixels per iteration using XOR with 0xFF
 */
inline void invert_monochrome_8bit_sse2(const uint8_t* src, uint8_t* dst,
                                         size_t pixel_count) noexcept {
    const __m128i all_ones = _mm_set1_epi8(static_cast<char>(0xFF));
    const size_t simd_count = (pixel_count / 16) * 16;

    size_t i = 0;
    for (; i < simd_count; i += 16) {
        __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i));
        v = _mm_xor_si128(v, all_ones);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + i), v);
    }

    // Handle remainder
    invert_monochrome_8bit_scalar(src + i, dst + i, pixel_count - i);
}

/**
 * @brief SSE2 16-bit monochrome inversion
 * Processes 8 pixels per iteration
 */
inline void invert_monochrome_16bit_sse2(const uint16_t* src, uint16_t* dst,
                                          size_t pixel_count,
                                          uint16_t max_value) noexcept {
    const __m128i max_vec = _mm_set1_epi16(static_cast<int16_t>(max_value));
    const size_t simd_count = (pixel_count / 8) * 8;

    size_t i = 0;
    for (; i < simd_count; i += 8) {
        __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i));
        v = _mm_sub_epi16(max_vec, v);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + i), v);
    }

    // Handle remainder
    invert_monochrome_16bit_scalar(src + i, dst + i, pixel_count - i, max_value);
}

#endif  // PACS_SIMD_SSE2

// ============================================================================
// AVX2 implementations
// ============================================================================

#if defined(PACS_SIMD_AVX2)

/**
 * @brief AVX2 8-bit monochrome inversion
 * Processes 32 pixels per iteration
 */
inline void invert_monochrome_8bit_avx2(const uint8_t* src, uint8_t* dst,
                                         size_t pixel_count) noexcept {
    const __m256i all_ones = _mm256_set1_epi8(static_cast<char>(0xFF));
    const size_t simd_count = (pixel_count / 32) * 32;

    size_t i = 0;
    for (; i < simd_count; i += 32) {
        __m256i v = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i));
        v = _mm256_xor_si256(v, all_ones);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i), v);
    }

    // Handle remainder with SSE2
#if defined(PACS_SIMD_SSE2)
    if (pixel_count - i >= 16) {
        invert_monochrome_8bit_sse2(src + i, dst + i, pixel_count - i);
    } else
#endif
    {
        invert_monochrome_8bit_scalar(src + i, dst + i, pixel_count - i);
    }
}

/**
 * @brief AVX2 16-bit monochrome inversion
 * Processes 16 pixels per iteration
 */
inline void invert_monochrome_16bit_avx2(const uint16_t* src, uint16_t* dst,
                                          size_t pixel_count,
                                          uint16_t max_value) noexcept {
    const __m256i max_vec = _mm256_set1_epi16(static_cast<int16_t>(max_value));
    const size_t simd_count = (pixel_count / 16) * 16;

    size_t i = 0;
    for (; i < simd_count; i += 16) {
        __m256i v = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i));
        v = _mm256_sub_epi16(max_vec, v);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i), v);
    }

    // Handle remainder with SSE2
#if defined(PACS_SIMD_SSE2)
    if (pixel_count - i >= 8) {
        invert_monochrome_16bit_sse2(src + i, dst + i, pixel_count - i, max_value);
    } else
#endif
    {
        invert_monochrome_16bit_scalar(src + i, dst + i, pixel_count - i, max_value);
    }
}

/**
 * @brief AVX2 RGB to YCbCr conversion
 *
 * Processes 8 pixels per iteration using fixed-point arithmetic.
 * Uses 16-bit integer operations for coefficient multiplication.
 */
inline void rgb_to_ycbcr_8bit_avx2(const uint8_t* src, uint8_t* dst,
                                    size_t pixel_count) noexcept {
    // Coefficients scaled by 256 (8-bit precision)
    // Y  coefficients:  77, 150, 29 (sum = 256)
    // Cb coefficients: -43, -85, 128
    // Cr coefficients: 128, -107, -21
    const __m256i coeff_y_r = _mm256_set1_epi16(77);
    const __m256i coeff_y_g = _mm256_set1_epi16(150);
    const __m256i coeff_y_b = _mm256_set1_epi16(29);

    const __m256i coeff_cb_r = _mm256_set1_epi16(-43);
    const __m256i coeff_cb_g = _mm256_set1_epi16(-85);
    const __m256i coeff_cb_b = _mm256_set1_epi16(128);

    const __m256i coeff_cr_r = _mm256_set1_epi16(128);
    const __m256i coeff_cr_g = _mm256_set1_epi16(-107);
    const __m256i coeff_cr_b = _mm256_set1_epi16(-21);

    const __m256i offset_128 = _mm256_set1_epi16(128);
    const __m256i round_add = _mm256_set1_epi16(128);  // Rounding for >>8

    const size_t simd_count = (pixel_count / 8) * 8;

    size_t i = 0;
    for (; i < simd_count; i += 8) {
        // Load 24 bytes (8 RGB pixels)
        // Note: We need to handle the scattered RGB data
        alignas(32) int16_t r[8], g[8], b[8];
        for (int j = 0; j < 8; ++j) {
            r[j] = src[(i + j) * 3];
            g[j] = src[(i + j) * 3 + 1];
            b[j] = src[(i + j) * 3 + 2];
        }

        __m256i r_vec = _mm256_load_si256(reinterpret_cast<const __m256i*>(r));
        __m256i g_vec = _mm256_load_si256(reinterpret_cast<const __m256i*>(g));
        __m256i b_vec = _mm256_load_si256(reinterpret_cast<const __m256i*>(b));

        // Calculate Y = (77*R + 150*G + 29*B + 128) >> 8
        __m256i y = _mm256_add_epi16(
            _mm256_add_epi16(_mm256_mullo_epi16(r_vec, coeff_y_r),
                             _mm256_mullo_epi16(g_vec, coeff_y_g)),
            _mm256_mullo_epi16(b_vec, coeff_y_b));
        y = _mm256_add_epi16(y, round_add);
        y = _mm256_srai_epi16(y, 8);

        // Calculate Cb = (-43*R - 85*G + 128*B + 128) >> 8 + 128
        __m256i cb = _mm256_add_epi16(
            _mm256_add_epi16(_mm256_mullo_epi16(r_vec, coeff_cb_r),
                             _mm256_mullo_epi16(g_vec, coeff_cb_g)),
            _mm256_mullo_epi16(b_vec, coeff_cb_b));
        cb = _mm256_add_epi16(cb, round_add);
        cb = _mm256_srai_epi16(cb, 8);
        cb = _mm256_add_epi16(cb, offset_128);

        // Calculate Cr = (128*R - 107*G - 21*B + 128) >> 8 + 128
        __m256i cr = _mm256_add_epi16(
            _mm256_add_epi16(_mm256_mullo_epi16(r_vec, coeff_cr_r),
                             _mm256_mullo_epi16(g_vec, coeff_cr_g)),
            _mm256_mullo_epi16(b_vec, coeff_cr_b));
        cr = _mm256_add_epi16(cr, round_add);
        cr = _mm256_srai_epi16(cr, 8);
        cr = _mm256_add_epi16(cr, offset_128);

        // Store results
        alignas(32) int16_t y_out[8], cb_out[8], cr_out[8];
        _mm256_store_si256(reinterpret_cast<__m256i*>(y_out), y);
        _mm256_store_si256(reinterpret_cast<__m256i*>(cb_out), cb);
        _mm256_store_si256(reinterpret_cast<__m256i*>(cr_out), cr);

        for (int j = 0; j < 8; ++j) {
            dst[(i + j) * 3] = static_cast<uint8_t>(
                y_out[j] < 0 ? 0 : (y_out[j] > 255 ? 255 : y_out[j]));
            dst[(i + j) * 3 + 1] = static_cast<uint8_t>(
                cb_out[j] < 0 ? 0 : (cb_out[j] > 255 ? 255 : cb_out[j]));
            dst[(i + j) * 3 + 2] = static_cast<uint8_t>(
                cr_out[j] < 0 ? 0 : (cr_out[j] > 255 ? 255 : cr_out[j]));
        }
    }

    // Handle remainder
    rgb_to_ycbcr_8bit_scalar(src + i * 3, dst + i * 3, pixel_count - i);
}

/**
 * @brief AVX2 YCbCr to RGB conversion
 */
inline void ycbcr_to_rgb_8bit_avx2(const uint8_t* src, uint8_t* dst,
                                    size_t pixel_count) noexcept {
    // Coefficients scaled by 256
    // R = Y + 1.402*Cr -> coeff = 359
    // G = Y - 0.344*Cb - 0.714*Cr -> coeffs = -88, -183
    // B = Y + 1.772*Cb -> coeff = 454
    const __m256i coeff_r_cr = _mm256_set1_epi16(359);
    const __m256i coeff_g_cb = _mm256_set1_epi16(-88);
    const __m256i coeff_g_cr = _mm256_set1_epi16(-183);
    const __m256i coeff_b_cb = _mm256_set1_epi16(454);

    const __m256i round_add = _mm256_set1_epi16(128);
    const __m256i zero = _mm256_setzero_si256();
    const __m256i max_255 = _mm256_set1_epi16(255);

    const size_t simd_count = (pixel_count / 8) * 8;

    size_t i = 0;
    for (; i < simd_count; i += 8) {
        // Load YCbCr values
        alignas(32) int16_t y[8], cb[8], cr[8];
        for (int j = 0; j < 8; ++j) {
            y[j] = src[(i + j) * 3];
            cb[j] = src[(i + j) * 3 + 1] - 128;
            cr[j] = src[(i + j) * 3 + 2] - 128;
        }

        __m256i y_vec = _mm256_load_si256(reinterpret_cast<const __m256i*>(y));
        __m256i cb_vec = _mm256_load_si256(reinterpret_cast<const __m256i*>(cb));
        __m256i cr_vec = _mm256_load_si256(reinterpret_cast<const __m256i*>(cr));

        // Calculate R = Y + (359*Cr + 128) >> 8
        __m256i r = _mm256_add_epi16(
            _mm256_srai_epi16(
                _mm256_add_epi16(_mm256_mullo_epi16(cr_vec, coeff_r_cr), round_add),
                8),
            y_vec);

        // Calculate G = Y + (-88*Cb - 183*Cr + 128) >> 8
        __m256i g = _mm256_add_epi16(
            _mm256_srai_epi16(
                _mm256_add_epi16(
                    _mm256_add_epi16(_mm256_mullo_epi16(cb_vec, coeff_g_cb),
                                     _mm256_mullo_epi16(cr_vec, coeff_g_cr)),
                    round_add),
                8),
            y_vec);

        // Calculate B = Y + (454*Cb + 128) >> 8
        __m256i b = _mm256_add_epi16(
            _mm256_srai_epi16(
                _mm256_add_epi16(_mm256_mullo_epi16(cb_vec, coeff_b_cb), round_add),
                8),
            y_vec);

        // Clamp to [0, 255]
        r = _mm256_max_epi16(_mm256_min_epi16(r, max_255), zero);
        g = _mm256_max_epi16(_mm256_min_epi16(g, max_255), zero);
        b = _mm256_max_epi16(_mm256_min_epi16(b, max_255), zero);

        // Store results
        alignas(32) int16_t r_out[8], g_out[8], b_out[8];
        _mm256_store_si256(reinterpret_cast<__m256i*>(r_out), r);
        _mm256_store_si256(reinterpret_cast<__m256i*>(g_out), g);
        _mm256_store_si256(reinterpret_cast<__m256i*>(b_out), b);

        for (int j = 0; j < 8; ++j) {
            dst[(i + j) * 3] = static_cast<uint8_t>(r_out[j]);
            dst[(i + j) * 3 + 1] = static_cast<uint8_t>(g_out[j]);
            dst[(i + j) * 3 + 2] = static_cast<uint8_t>(b_out[j]);
        }
    }

    // Handle remainder
    ycbcr_to_rgb_8bit_scalar(src + i * 3, dst + i * 3, pixel_count - i);
}

#endif  // PACS_SIMD_AVX2

// ============================================================================
// NEON implementations (ARM)
// ============================================================================

#if defined(PACS_SIMD_NEON)

/**
 * @brief NEON 8-bit monochrome inversion
 * Processes 16 pixels per iteration
 */
inline void invert_monochrome_8bit_neon(const uint8_t* src, uint8_t* dst,
                                         size_t pixel_count) noexcept {
    const uint8x16_t all_ones = vdupq_n_u8(0xFF);
    const size_t simd_count = (pixel_count / 16) * 16;

    size_t i = 0;
    for (; i < simd_count; i += 16) {
        uint8x16_t v = vld1q_u8(src + i);
        v = veorq_u8(v, all_ones);
        vst1q_u8(dst + i, v);
    }

    // Handle remainder
    invert_monochrome_8bit_scalar(src + i, dst + i, pixel_count - i);
}

/**
 * @brief NEON 16-bit monochrome inversion
 * Processes 8 pixels per iteration
 */
inline void invert_monochrome_16bit_neon(const uint16_t* src, uint16_t* dst,
                                          size_t pixel_count,
                                          uint16_t max_value) noexcept {
    const uint16x8_t max_vec = vdupq_n_u16(max_value);
    const size_t simd_count = (pixel_count / 8) * 8;

    size_t i = 0;
    for (; i < simd_count; i += 8) {
        uint16x8_t v = vld1q_u16(src + i);
        v = vsubq_u16(max_vec, v);
        vst1q_u16(dst + i, v);
    }

    // Handle remainder
    invert_monochrome_16bit_scalar(src + i, dst + i, pixel_count - i, max_value);
}

/**
 * @brief NEON RGB to YCbCr conversion
 * Uses vld3 to deinterleave RGB data efficiently
 */
inline void rgb_to_ycbcr_8bit_neon(const uint8_t* src, uint8_t* dst,
                                    size_t pixel_count) noexcept {
    // Coefficients (scaled by 256)
    const int16x8_t coeff_y_r = vdupq_n_s16(77);
    const int16x8_t coeff_y_g = vdupq_n_s16(150);
    const int16x8_t coeff_y_b = vdupq_n_s16(29);

    const int16x8_t coeff_cb_r = vdupq_n_s16(-43);
    const int16x8_t coeff_cb_g = vdupq_n_s16(-85);
    const int16x8_t coeff_cb_b = vdupq_n_s16(128);

    const int16x8_t coeff_cr_r = vdupq_n_s16(128);
    const int16x8_t coeff_cr_g = vdupq_n_s16(-107);
    const int16x8_t coeff_cr_b = vdupq_n_s16(-21);

    const int16x8_t offset_128 = vdupq_n_s16(128);

    const size_t simd_count = (pixel_count / 8) * 8;

    size_t i = 0;
    for (; i < simd_count; i += 8) {
        // Load and deinterleave 8 RGB pixels
        uint8x8x3_t rgb = vld3_u8(src + i * 3);

        // Widen to 16-bit for arithmetic
        int16x8_t r = vreinterpretq_s16_u16(vmovl_u8(rgb.val[0]));
        int16x8_t g = vreinterpretq_s16_u16(vmovl_u8(rgb.val[1]));
        int16x8_t b = vreinterpretq_s16_u16(vmovl_u8(rgb.val[2]));

        // Y = (77*R + 150*G + 29*B + 128) >> 8
        int16x8_t y = vaddq_s16(vaddq_s16(vmulq_s16(r, coeff_y_r),
                                           vmulq_s16(g, coeff_y_g)),
                                 vmulq_s16(b, coeff_y_b));
        y = vaddq_s16(y, offset_128);
        y = vshrq_n_s16(y, 8);

        // Cb = (-43*R - 85*G + 128*B + 128) >> 8 + 128
        int16x8_t cb = vaddq_s16(vaddq_s16(vmulq_s16(r, coeff_cb_r),
                                            vmulq_s16(g, coeff_cb_g)),
                                  vmulq_s16(b, coeff_cb_b));
        cb = vaddq_s16(cb, offset_128);
        cb = vshrq_n_s16(cb, 8);
        cb = vaddq_s16(cb, offset_128);

        // Cr = (128*R - 107*G - 21*B + 128) >> 8 + 128
        int16x8_t cr = vaddq_s16(vaddq_s16(vmulq_s16(r, coeff_cr_r),
                                            vmulq_s16(g, coeff_cr_g)),
                                  vmulq_s16(b, coeff_cr_b));
        cr = vaddq_s16(cr, offset_128);
        cr = vshrq_n_s16(cr, 8);
        cr = vaddq_s16(cr, offset_128);

        // Narrow back to 8-bit with saturation
        uint8x8x3_t ycbcr;
        ycbcr.val[0] = vqmovun_s16(y);
        ycbcr.val[1] = vqmovun_s16(cb);
        ycbcr.val[2] = vqmovun_s16(cr);

        // Store interleaved YCbCr
        vst3_u8(dst + i * 3, ycbcr);
    }

    // Handle remainder
    rgb_to_ycbcr_8bit_scalar(src + i * 3, dst + i * 3, pixel_count - i);
}

/**
 * @brief NEON YCbCr to RGB conversion
 */
inline void ycbcr_to_rgb_8bit_neon(const uint8_t* src, uint8_t* dst,
                                    size_t pixel_count) noexcept {
    const int16x8_t coeff_r_cr = vdupq_n_s16(359);
    const int16x8_t coeff_g_cb = vdupq_n_s16(-88);
    const int16x8_t coeff_g_cr = vdupq_n_s16(-183);
    const int16x8_t coeff_b_cb = vdupq_n_s16(454);
    const int16x8_t offset_128 = vdupq_n_s16(128);

    const size_t simd_count = (pixel_count / 8) * 8;

    size_t i = 0;
    for (; i < simd_count; i += 8) {
        // Load and deinterleave 8 YCbCr pixels
        uint8x8x3_t ycbcr = vld3_u8(src + i * 3);

        // Widen to 16-bit
        int16x8_t y = vreinterpretq_s16_u16(vmovl_u8(ycbcr.val[0]));
        int16x8_t cb = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(ycbcr.val[1])),
                                  offset_128);
        int16x8_t cr = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(ycbcr.val[2])),
                                  offset_128);

        // R = Y + (359*Cr + 128) >> 8
        int16x8_t r = vaddq_s16(y, vshrq_n_s16(vaddq_s16(vmulq_s16(cr, coeff_r_cr),
                                                          offset_128), 8));

        // G = Y + (-88*Cb - 183*Cr + 128) >> 8
        int16x8_t g = vaddq_s16(y, vshrq_n_s16(
            vaddq_s16(vaddq_s16(vmulq_s16(cb, coeff_g_cb),
                                vmulq_s16(cr, coeff_g_cr)),
                      offset_128), 8));

        // B = Y + (454*Cb + 128) >> 8
        int16x8_t bl = vaddq_s16(y, vshrq_n_s16(vaddq_s16(vmulq_s16(cb, coeff_b_cb),
                                                           offset_128), 8));

        // Narrow with saturation
        uint8x8x3_t rgb;
        rgb.val[0] = vqmovun_s16(r);
        rgb.val[1] = vqmovun_s16(g);
        rgb.val[2] = vqmovun_s16(bl);

        // Store interleaved RGB
        vst3_u8(dst + i * 3, rgb);
    }

    // Handle remainder
    ycbcr_to_rgb_8bit_scalar(src + i * 3, dst + i * 3, pixel_count - i);
}

#endif  // PACS_SIMD_NEON

}  // namespace detail

// ============================================================================
// Public API with runtime dispatch
// ============================================================================

/**
 * @brief Invert 8-bit monochrome pixels (MONOCHROME1 <-> MONOCHROME2)
 *
 * Converts between MONOCHROME1 (white = 0) and MONOCHROME2 (white = max).
 * Uses SIMD when available for better performance.
 *
 * @param src Source pixel data
 * @param dst Destination pixel data (can be same as src for in-place)
 * @param pixel_count Number of pixels to process
 */
inline void invert_monochrome_8bit(const uint8_t* src, uint8_t* dst,
                                    size_t pixel_count) noexcept {
#if defined(PACS_SIMD_AVX2)
    if (has_avx2()) {
        detail::invert_monochrome_8bit_avx2(src, dst, pixel_count);
        return;
    }
#endif
#if defined(PACS_SIMD_SSE2)
    if (has_sse2()) {
        detail::invert_monochrome_8bit_sse2(src, dst, pixel_count);
        return;
    }
#endif
#if defined(PACS_SIMD_NEON)
    if (has_neon()) {
        detail::invert_monochrome_8bit_neon(src, dst, pixel_count);
        return;
    }
#endif
    detail::invert_monochrome_8bit_scalar(src, dst, pixel_count);
}

/**
 * @brief Invert 16-bit monochrome pixels
 *
 * @param src Source pixel data
 * @param dst Destination pixel data
 * @param pixel_count Number of pixels to process
 * @param max_value Maximum pixel value (e.g., 4095 for 12-bit, 65535 for 16-bit)
 */
inline void invert_monochrome_16bit(const uint16_t* src, uint16_t* dst,
                                     size_t pixel_count,
                                     uint16_t max_value) noexcept {
#if defined(PACS_SIMD_AVX2)
    if (has_avx2()) {
        detail::invert_monochrome_16bit_avx2(src, dst, pixel_count, max_value);
        return;
    }
#endif
#if defined(PACS_SIMD_SSE2)
    if (has_sse2()) {
        detail::invert_monochrome_16bit_sse2(src, dst, pixel_count, max_value);
        return;
    }
#endif
#if defined(PACS_SIMD_NEON)
    if (has_neon()) {
        detail::invert_monochrome_16bit_neon(src, dst, pixel_count, max_value);
        return;
    }
#endif
    detail::invert_monochrome_16bit_scalar(src, dst, pixel_count, max_value);
}

/**
 * @brief Convert RGB to YCbCr color space (ITU-R BT.601)
 *
 * @param src Source RGB pixel data (interleaved RGBRGB...)
 * @param dst Destination YCbCr pixel data (interleaved YCbCrYCbCr...)
 * @param pixel_count Number of pixels to process
 */
inline void rgb_to_ycbcr_8bit(const uint8_t* src, uint8_t* dst,
                               size_t pixel_count) noexcept {
#if defined(PACS_SIMD_AVX2)
    if (has_avx2()) {
        detail::rgb_to_ycbcr_8bit_avx2(src, dst, pixel_count);
        return;
    }
#endif
#if defined(PACS_SIMD_NEON)
    if (has_neon()) {
        detail::rgb_to_ycbcr_8bit_neon(src, dst, pixel_count);
        return;
    }
#endif
    detail::rgb_to_ycbcr_8bit_scalar(src, dst, pixel_count);
}

/**
 * @brief Convert YCbCr to RGB color space (ITU-R BT.601)
 *
 * @param src Source YCbCr pixel data (interleaved YCbCrYCbCr...)
 * @param dst Destination RGB pixel data (interleaved RGBRGB...)
 * @param pixel_count Number of pixels to process
 */
inline void ycbcr_to_rgb_8bit(const uint8_t* src, uint8_t* dst,
                               size_t pixel_count) noexcept {
#if defined(PACS_SIMD_AVX2)
    if (has_avx2()) {
        detail::ycbcr_to_rgb_8bit_avx2(src, dst, pixel_count);
        return;
    }
#endif
#if defined(PACS_SIMD_NEON)
    if (has_neon()) {
        detail::ycbcr_to_rgb_8bit_neon(src, dst, pixel_count);
        return;
    }
#endif
    detail::ycbcr_to_rgb_8bit_scalar(src, dst, pixel_count);
}

}  // namespace pacs::encoding::simd

#endif  // PACS_ENCODING_SIMD_PHOTOMETRIC_HPP
