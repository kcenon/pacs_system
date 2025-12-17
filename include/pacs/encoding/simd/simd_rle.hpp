/**
 * @file simd_rle.hpp
 * @brief SIMD optimizations for RLE codec operations
 *
 * Provides optimized planar-to-interleaved and interleaved-to-planar
 * conversions for RLE segment extraction and reconstruction.
 *
 * Supported operations:
 * - 8-bit RGB: interleaved <-> planar conversion
 * - 16-bit: byte plane splitting and merging
 *
 * @see DICOM PS3.5 Annex G - RLE Lossless Compression
 */

#ifndef PACS_ENCODING_SIMD_RLE_HPP
#define PACS_ENCODING_SIMD_RLE_HPP

#include "simd_config.hpp"
#include "simd_types.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace pacs::encoding::simd {

// Forward declarations
void interleaved_to_planar_rgb8(const uint8_t* src, uint8_t* r, uint8_t* g,
                                 uint8_t* b, size_t pixel_count) noexcept;
void planar_to_interleaved_rgb8(const uint8_t* r, const uint8_t* g,
                                 const uint8_t* b, uint8_t* dst,
                                 size_t pixel_count) noexcept;
void split_16bit_to_planes(const uint8_t* src, uint8_t* high, uint8_t* low,
                           size_t pixel_count) noexcept;
void merge_planes_to_16bit(const uint8_t* high, const uint8_t* low,
                           uint8_t* dst, size_t pixel_count) noexcept;

namespace detail {

// ============================================================================
// Scalar fallback implementations
// ============================================================================

inline void interleaved_to_planar_rgb8_scalar(const uint8_t* src, uint8_t* r,
                                               uint8_t* g, uint8_t* b,
                                               size_t pixel_count) noexcept {
    for (size_t i = 0; i < pixel_count; ++i) {
        r[i] = src[i * 3];
        g[i] = src[i * 3 + 1];
        b[i] = src[i * 3 + 2];
    }
}

inline void planar_to_interleaved_rgb8_scalar(const uint8_t* r, const uint8_t* g,
                                               const uint8_t* b, uint8_t* dst,
                                               size_t pixel_count) noexcept {
    for (size_t i = 0; i < pixel_count; ++i) {
        dst[i * 3] = r[i];
        dst[i * 3 + 1] = g[i];
        dst[i * 3 + 2] = b[i];
    }
}

inline void split_16bit_to_planes_scalar(const uint8_t* src, uint8_t* high,
                                          uint8_t* low,
                                          size_t pixel_count) noexcept {
    for (size_t i = 0; i < pixel_count; ++i) {
        low[i] = src[i * 2];
        high[i] = src[i * 2 + 1];
    }
}

inline void merge_planes_to_16bit_scalar(const uint8_t* high, const uint8_t* low,
                                          uint8_t* dst,
                                          size_t pixel_count) noexcept {
    for (size_t i = 0; i < pixel_count; ++i) {
        dst[i * 2] = low[i];
        dst[i * 2 + 1] = high[i];
    }
}

// ============================================================================
// SSE2/SSSE3 implementations
// ============================================================================

#if defined(PACS_SIMD_SSSE3)

/**
 * @brief SSSE3 interleaved RGB to planar conversion
 *
 * Converts RGBRGBRGB... to separate R, G, B planes.
 * Processes 16 pixels (48 bytes) per iteration.
 */
inline void interleaved_to_planar_rgb8_ssse3(const uint8_t* src, uint8_t* r,
                                              uint8_t* g, uint8_t* b,
                                              size_t pixel_count) noexcept {
    // Shuffle masks for deinterleaving RGB
    // Input:  R0 G0 B0 R1 G1 B1 R2 G2 B2 R3 G3 B3 R4 G4 B4 R5
    // Output: R0 R1 R2 R3 R4 R5 ... (and similar for G, B)

    const __m128i shuffle_r0 =
        _mm_setr_epi8(0, 3, 6, 9, 12, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);
    const __m128i shuffle_r1 =
        _mm_setr_epi8(-1, -1, -1, -1, -1, -1, 2, 5, 8, 11, 14, -1, -1, -1, -1, -1);
    const __m128i shuffle_r2 =
        _mm_setr_epi8(-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 1, 4, 7, 10, 13);

    const __m128i shuffle_g0 =
        _mm_setr_epi8(1, 4, 7, 10, 13, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);
    const __m128i shuffle_g1 =
        _mm_setr_epi8(-1, -1, -1, -1, -1, 0, 3, 6, 9, 12, 15, -1, -1, -1, -1, -1);
    const __m128i shuffle_g2 =
        _mm_setr_epi8(-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 2, 5, 8, 11, 14);

    const __m128i shuffle_b0 =
        _mm_setr_epi8(2, 5, 8, 11, 14, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);
    const __m128i shuffle_b1 =
        _mm_setr_epi8(-1, -1, -1, -1, -1, 1, 4, 7, 10, 13, -1, -1, -1, -1, -1, -1);
    const __m128i shuffle_b2 =
        _mm_setr_epi8(-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 3, 6, 9, 12, 15);

    const size_t simd_count = (pixel_count / 16) * 16;

    size_t i = 0;
    for (; i < simd_count; i += 16) {
        // Load 48 bytes (16 RGB pixels)
        __m128i v0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i * 3));
        __m128i v1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i * 3 + 16));
        __m128i v2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i * 3 + 32));

        // Extract R channel
        __m128i r_vec = _mm_or_si128(
            _mm_or_si128(_mm_shuffle_epi8(v0, shuffle_r0),
                         _mm_shuffle_epi8(v1, shuffle_r1)),
            _mm_shuffle_epi8(v2, shuffle_r2));

        // Extract G channel
        __m128i g_vec = _mm_or_si128(
            _mm_or_si128(_mm_shuffle_epi8(v0, shuffle_g0),
                         _mm_shuffle_epi8(v1, shuffle_g1)),
            _mm_shuffle_epi8(v2, shuffle_g2));

        // Extract B channel
        __m128i b_vec = _mm_or_si128(
            _mm_or_si128(_mm_shuffle_epi8(v0, shuffle_b0),
                         _mm_shuffle_epi8(v1, shuffle_b1)),
            _mm_shuffle_epi8(v2, shuffle_b2));

        // Store results
        _mm_storeu_si128(reinterpret_cast<__m128i*>(r + i), r_vec);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(g + i), g_vec);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(b + i), b_vec);
    }

    // Handle remainder
    interleaved_to_planar_rgb8_scalar(src + i * 3, r + i, g + i, b + i,
                                       pixel_count - i);
}

/**
 * @brief SSSE3 planar to interleaved RGB conversion
 *
 * Converts separate R, G, B planes to RGBRGBRGB...
 * Processes 16 pixels (48 bytes) per iteration.
 */
inline void planar_to_interleaved_rgb8_ssse3(const uint8_t* r, const uint8_t* g,
                                              const uint8_t* b, uint8_t* dst,
                                              size_t pixel_count) noexcept {
    // Shuffle masks for interleaving
    const __m128i shuffle_r =
        _mm_setr_epi8(0, -1, -1, 1, -1, -1, 2, -1, -1, 3, -1, -1, 4, -1, -1, 5);
    const __m128i shuffle_g =
        _mm_setr_epi8(-1, 0, -1, -1, 1, -1, -1, 2, -1, -1, 3, -1, -1, 4, -1, -1);
    const __m128i shuffle_b =
        _mm_setr_epi8(-1, -1, 0, -1, -1, 1, -1, -1, 2, -1, -1, 3, -1, -1, 4, -1);

    const __m128i shuffle_r2 =
        _mm_setr_epi8(-1, -1, 6, -1, -1, 7, -1, -1, 8, -1, -1, 9, -1, -1, 10, -1);
    const __m128i shuffle_g2 =
        _mm_setr_epi8(5, -1, -1, 6, -1, -1, 7, -1, -1, 8, -1, -1, 9, -1, -1, 10);
    const __m128i shuffle_b2 =
        _mm_setr_epi8(-1, 5, -1, -1, 6, -1, -1, 7, -1, -1, 8, -1, -1, 9, -1, -1);

    const __m128i shuffle_r3 =
        _mm_setr_epi8(-1, 11, -1, -1, 12, -1, -1, 13, -1, -1, 14, -1, -1, 15, -1, -1);
    const __m128i shuffle_g3 =
        _mm_setr_epi8(-1, -1, 11, -1, -1, 12, -1, -1, 13, -1, -1, 14, -1, -1, 15, -1);
    const __m128i shuffle_b3 =
        _mm_setr_epi8(10, -1, -1, 11, -1, -1, 12, -1, -1, 13, -1, -1, 14, -1, -1, 15);

    const size_t simd_count = (pixel_count / 16) * 16;

    size_t i = 0;
    for (; i < simd_count; i += 16) {
        // Load 16 bytes from each plane
        __m128i r_vec = _mm_loadu_si128(reinterpret_cast<const __m128i*>(r + i));
        __m128i g_vec = _mm_loadu_si128(reinterpret_cast<const __m128i*>(g + i));
        __m128i b_vec = _mm_loadu_si128(reinterpret_cast<const __m128i*>(b + i));

        // First 16 bytes of output (pixels 0-5, partial 6)
        __m128i out0 = _mm_or_si128(
            _mm_or_si128(_mm_shuffle_epi8(r_vec, shuffle_r),
                         _mm_shuffle_epi8(g_vec, shuffle_g)),
            _mm_shuffle_epi8(b_vec, shuffle_b));

        // Second 16 bytes of output (partial 5, pixels 6-10, partial 11)
        __m128i out1 = _mm_or_si128(
            _mm_or_si128(_mm_shuffle_epi8(r_vec, shuffle_r2),
                         _mm_shuffle_epi8(g_vec, shuffle_g2)),
            _mm_shuffle_epi8(b_vec, shuffle_b2));

        // Third 16 bytes of output (partial 10, pixels 11-15)
        __m128i out2 = _mm_or_si128(
            _mm_or_si128(_mm_shuffle_epi8(r_vec, shuffle_r3),
                         _mm_shuffle_epi8(g_vec, shuffle_g3)),
            _mm_shuffle_epi8(b_vec, shuffle_b3));

        // Store results
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + i * 3), out0);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + i * 3 + 16), out1);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + i * 3 + 32), out2);
    }

    // Handle remainder
    planar_to_interleaved_rgb8_scalar(r + i, g + i, b + i, dst + i * 3,
                                       pixel_count - i);
}

/**
 * @brief SSSE3 16-bit to byte planes split
 *
 * Splits 16-bit little-endian data into high and low byte planes.
 * Processes 16 pixels (32 bytes) per iteration.
 */
inline void split_16bit_to_planes_ssse3(const uint8_t* src, uint8_t* high,
                                         uint8_t* low,
                                         size_t pixel_count) noexcept {
    // Shuffle mask to extract low bytes (even positions)
    const __m128i shuffle_low =
        _mm_setr_epi8(0, 2, 4, 6, 8, 10, 12, 14, -1, -1, -1, -1, -1, -1, -1, -1);
    // Shuffle mask to extract high bytes (odd positions)
    const __m128i shuffle_high =
        _mm_setr_epi8(1, 3, 5, 7, 9, 11, 13, 15, -1, -1, -1, -1, -1, -1, -1, -1);

    const size_t simd_count = (pixel_count / 16) * 16;

    size_t i = 0;
    for (; i < simd_count; i += 16) {
        // Load 32 bytes (16 x 16-bit pixels)
        __m128i v0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i * 2));
        __m128i v1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i * 2 + 16));

        // Extract low and high bytes
        __m128i low0 = _mm_shuffle_epi8(v0, shuffle_low);
        __m128i high0 = _mm_shuffle_epi8(v0, shuffle_high);
        __m128i low1 = _mm_shuffle_epi8(v1, shuffle_low);
        __m128i high1 = _mm_shuffle_epi8(v1, shuffle_high);

        // Combine into single vectors
        __m128i low_vec = _mm_or_si128(low0, _mm_slli_si128(low1, 8));
        __m128i high_vec = _mm_or_si128(high0, _mm_slli_si128(high1, 8));

        // Store results
        _mm_storeu_si128(reinterpret_cast<__m128i*>(low + i), low_vec);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(high + i), high_vec);
    }

    // Handle remainder
    split_16bit_to_planes_scalar(src + i * 2, high + i, low + i, pixel_count - i);
}

/**
 * @brief SSSE3 byte planes to 16-bit merge
 *
 * Merges high and low byte planes into 16-bit little-endian data.
 * Processes 16 pixels (32 bytes) per iteration.
 */
inline void merge_planes_to_16bit_ssse3(const uint8_t* high, const uint8_t* low,
                                         uint8_t* dst,
                                         size_t pixel_count) noexcept {
    const size_t simd_count = (pixel_count / 16) * 16;

    size_t i = 0;
    for (; i < simd_count; i += 16) {
        // Load 16 bytes from each plane
        __m128i low_vec = _mm_loadu_si128(reinterpret_cast<const __m128i*>(low + i));
        __m128i high_vec = _mm_loadu_si128(reinterpret_cast<const __m128i*>(high + i));

        // Interleave low and high bytes
        __m128i out0 = _mm_unpacklo_epi8(low_vec, high_vec);
        __m128i out1 = _mm_unpackhi_epi8(low_vec, high_vec);

        // Store results
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + i * 2), out0);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + i * 2 + 16), out1);
    }

    // Handle remainder
    merge_planes_to_16bit_scalar(high + i, low + i, dst + i * 2, pixel_count - i);
}

#endif  // PACS_SIMD_SSSE3

// ============================================================================
// AVX2 implementations
// ============================================================================

#if defined(PACS_SIMD_AVX2)

/**
 * @brief AVX2 interleaved RGB to planar conversion
 *
 * Processes 32 pixels (96 bytes) per iteration.
 */
inline void interleaved_to_planar_rgb8_avx2(const uint8_t* src, uint8_t* r,
                                             uint8_t* g, uint8_t* b,
                                             size_t pixel_count) noexcept {
    // AVX2 shuffle masks (same pattern in both 128-bit lanes)
    const __m256i shuffle_r0 = _mm256_setr_epi8(
        0, 3, 6, 9, 12, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        0, 3, 6, 9, 12, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);
    const __m256i shuffle_r1 = _mm256_setr_epi8(
        -1, -1, -1, -1, -1, -1, 2, 5, 8, 11, 14, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, 2, 5, 8, 11, 14, -1, -1, -1, -1, -1);
    const __m256i shuffle_r2 = _mm256_setr_epi8(
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 1, 4, 7, 10, 13,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 1, 4, 7, 10, 13);

    const __m256i shuffle_g0 = _mm256_setr_epi8(
        1, 4, 7, 10, 13, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        1, 4, 7, 10, 13, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);
    const __m256i shuffle_g1 = _mm256_setr_epi8(
        -1, -1, -1, -1, -1, 0, 3, 6, 9, 12, 15, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, 0, 3, 6, 9, 12, 15, -1, -1, -1, -1, -1);
    const __m256i shuffle_g2 = _mm256_setr_epi8(
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 2, 5, 8, 11, 14,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 2, 5, 8, 11, 14);

    const __m256i shuffle_b0 = _mm256_setr_epi8(
        2, 5, 8, 11, 14, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        2, 5, 8, 11, 14, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);
    const __m256i shuffle_b1 = _mm256_setr_epi8(
        -1, -1, -1, -1, -1, 1, 4, 7, 10, 13, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, 1, 4, 7, 10, 13, -1, -1, -1, -1, -1, -1);
    const __m256i shuffle_b2 = _mm256_setr_epi8(
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 3, 6, 9, 12, 15,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 3, 6, 9, 12, 15);

    const size_t simd_count = (pixel_count / 32) * 32;

    size_t i = 0;
    for (; i < simd_count; i += 32) {
        // Load 96 bytes (32 RGB pixels) as 6 x 128-bit loads
        // Then combine into 256-bit vectors
        __m128i v0_lo = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i * 3));
        __m128i v1_lo = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i * 3 + 16));
        __m128i v2_lo = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i * 3 + 32));
        __m128i v0_hi = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i * 3 + 48));
        __m128i v1_hi = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i * 3 + 64));
        __m128i v2_hi = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i * 3 + 80));

        __m256i v0 = _mm256_set_m128i(v0_hi, v0_lo);
        __m256i v1 = _mm256_set_m128i(v1_hi, v1_lo);
        __m256i v2 = _mm256_set_m128i(v2_hi, v2_lo);

        // Extract R channel
        __m256i r_vec = _mm256_or_si256(
            _mm256_or_si256(_mm256_shuffle_epi8(v0, shuffle_r0),
                            _mm256_shuffle_epi8(v1, shuffle_r1)),
            _mm256_shuffle_epi8(v2, shuffle_r2));

        // Extract G channel
        __m256i g_vec = _mm256_or_si256(
            _mm256_or_si256(_mm256_shuffle_epi8(v0, shuffle_g0),
                            _mm256_shuffle_epi8(v1, shuffle_g1)),
            _mm256_shuffle_epi8(v2, shuffle_g2));

        // Extract B channel
        __m256i b_vec = _mm256_or_si256(
            _mm256_or_si256(_mm256_shuffle_epi8(v0, shuffle_b0),
                            _mm256_shuffle_epi8(v1, shuffle_b1)),
            _mm256_shuffle_epi8(v2, shuffle_b2));

        // Permute to get correct order across lanes
        r_vec = _mm256_permute4x64_epi64(r_vec, 0xD8);  // 0, 2, 1, 3
        g_vec = _mm256_permute4x64_epi64(g_vec, 0xD8);
        b_vec = _mm256_permute4x64_epi64(b_vec, 0xD8);

        // Store results
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(r + i), r_vec);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(g + i), g_vec);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(b + i), b_vec);
    }

    // Handle remainder with SSSE3 or scalar
#if defined(PACS_SIMD_SSSE3)
    interleaved_to_planar_rgb8_ssse3(src + i * 3, r + i, g + i, b + i,
                                      pixel_count - i);
#else
    interleaved_to_planar_rgb8_scalar(src + i * 3, r + i, g + i, b + i,
                                       pixel_count - i);
#endif
}

/**
 * @brief AVX2 planar to interleaved RGB conversion
 *
 * Processes 32 pixels (96 bytes) per iteration.
 */
inline void planar_to_interleaved_rgb8_avx2(const uint8_t* r, const uint8_t* g,
                                             const uint8_t* b, uint8_t* dst,
                                             size_t pixel_count) noexcept {
    // Shuffle masks for interleaving (same pattern in both lanes)
    const __m256i shuffle_r = _mm256_setr_epi8(
        0, -1, -1, 1, -1, -1, 2, -1, -1, 3, -1, -1, 4, -1, -1, 5,
        0, -1, -1, 1, -1, -1, 2, -1, -1, 3, -1, -1, 4, -1, -1, 5);
    const __m256i shuffle_g = _mm256_setr_epi8(
        -1, 0, -1, -1, 1, -1, -1, 2, -1, -1, 3, -1, -1, 4, -1, -1,
        -1, 0, -1, -1, 1, -1, -1, 2, -1, -1, 3, -1, -1, 4, -1, -1);
    const __m256i shuffle_b = _mm256_setr_epi8(
        -1, -1, 0, -1, -1, 1, -1, -1, 2, -1, -1, 3, -1, -1, 4, -1,
        -1, -1, 0, -1, -1, 1, -1, -1, 2, -1, -1, 3, -1, -1, 4, -1);

    const __m256i shuffle_r2 = _mm256_setr_epi8(
        -1, -1, 6, -1, -1, 7, -1, -1, 8, -1, -1, 9, -1, -1, 10, -1,
        -1, -1, 6, -1, -1, 7, -1, -1, 8, -1, -1, 9, -1, -1, 10, -1);
    const __m256i shuffle_g2 = _mm256_setr_epi8(
        5, -1, -1, 6, -1, -1, 7, -1, -1, 8, -1, -1, 9, -1, -1, 10,
        5, -1, -1, 6, -1, -1, 7, -1, -1, 8, -1, -1, 9, -1, -1, 10);
    const __m256i shuffle_b2 = _mm256_setr_epi8(
        -1, 5, -1, -1, 6, -1, -1, 7, -1, -1, 8, -1, -1, 9, -1, -1,
        -1, 5, -1, -1, 6, -1, -1, 7, -1, -1, 8, -1, -1, 9, -1, -1);

    const __m256i shuffle_r3 = _mm256_setr_epi8(
        -1, 11, -1, -1, 12, -1, -1, 13, -1, -1, 14, -1, -1, 15, -1, -1,
        -1, 11, -1, -1, 12, -1, -1, 13, -1, -1, 14, -1, -1, 15, -1, -1);
    const __m256i shuffle_g3 = _mm256_setr_epi8(
        -1, -1, 11, -1, -1, 12, -1, -1, 13, -1, -1, 14, -1, -1, 15, -1,
        -1, -1, 11, -1, -1, 12, -1, -1, 13, -1, -1, 14, -1, -1, 15, -1);
    const __m256i shuffle_b3 = _mm256_setr_epi8(
        10, -1, -1, 11, -1, -1, 12, -1, -1, 13, -1, -1, 14, -1, -1, 15,
        10, -1, -1, 11, -1, -1, 12, -1, -1, 13, -1, -1, 14, -1, -1, 15);

    const size_t simd_count = (pixel_count / 32) * 32;

    size_t i = 0;
    for (; i < simd_count; i += 32) {
        // Load 32 bytes from each plane
        __m256i r_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(r + i));
        __m256i g_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(g + i));
        __m256i b_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(b + i));

        // First 32 bytes of output
        __m256i out0 = _mm256_or_si256(
            _mm256_or_si256(_mm256_shuffle_epi8(r_vec, shuffle_r),
                            _mm256_shuffle_epi8(g_vec, shuffle_g)),
            _mm256_shuffle_epi8(b_vec, shuffle_b));

        // Second 32 bytes
        __m256i out1 = _mm256_or_si256(
            _mm256_or_si256(_mm256_shuffle_epi8(r_vec, shuffle_r2),
                            _mm256_shuffle_epi8(g_vec, shuffle_g2)),
            _mm256_shuffle_epi8(b_vec, shuffle_b2));

        // Third 32 bytes
        __m256i out2 = _mm256_or_si256(
            _mm256_or_si256(_mm256_shuffle_epi8(r_vec, shuffle_r3),
                            _mm256_shuffle_epi8(g_vec, shuffle_g3)),
            _mm256_shuffle_epi8(b_vec, shuffle_b3));

        // Permute to interleave results from both lanes
        out0 = _mm256_permute4x64_epi64(out0, 0xD8);
        out1 = _mm256_permute4x64_epi64(out1, 0xD8);
        out2 = _mm256_permute4x64_epi64(out2, 0xD8);

        // Store results
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i * 3), out0);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i * 3 + 32), out1);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i * 3 + 64), out2);
    }

    // Handle remainder
#if defined(PACS_SIMD_SSSE3)
    planar_to_interleaved_rgb8_ssse3(r + i, g + i, b + i, dst + i * 3,
                                      pixel_count - i);
#else
    planar_to_interleaved_rgb8_scalar(r + i, g + i, b + i, dst + i * 3,
                                       pixel_count - i);
#endif
}

/**
 * @brief AVX2 16-bit to byte planes split
 *
 * Processes 32 pixels (64 bytes) per iteration.
 */
inline void split_16bit_to_planes_avx2(const uint8_t* src, uint8_t* high,
                                        uint8_t* low,
                                        size_t pixel_count) noexcept {
    const __m256i shuffle_low = _mm256_setr_epi8(
        0, 2, 4, 6, 8, 10, 12, 14, -1, -1, -1, -1, -1, -1, -1, -1,
        0, 2, 4, 6, 8, 10, 12, 14, -1, -1, -1, -1, -1, -1, -1, -1);
    const __m256i shuffle_high = _mm256_setr_epi8(
        1, 3, 5, 7, 9, 11, 13, 15, -1, -1, -1, -1, -1, -1, -1, -1,
        1, 3, 5, 7, 9, 11, 13, 15, -1, -1, -1, -1, -1, -1, -1, -1);

    const size_t simd_count = (pixel_count / 32) * 32;

    size_t i = 0;
    for (; i < simd_count; i += 32) {
        // Load 64 bytes (32 x 16-bit pixels)
        __m256i v0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i * 2));
        __m256i v1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i * 2 + 32));

        // Extract low and high bytes
        __m256i low0 = _mm256_shuffle_epi8(v0, shuffle_low);
        __m256i high0 = _mm256_shuffle_epi8(v0, shuffle_high);
        __m256i low1 = _mm256_shuffle_epi8(v1, shuffle_low);
        __m256i high1 = _mm256_shuffle_epi8(v1, shuffle_high);

        // Pack results from both lanes
        low0 = _mm256_permute4x64_epi64(low0, 0xD8);
        high0 = _mm256_permute4x64_epi64(high0, 0xD8);
        low1 = _mm256_permute4x64_epi64(low1, 0xD8);
        high1 = _mm256_permute4x64_epi64(high1, 0xD8);

        // Combine into final vectors
        __m256i low_vec = _mm256_permute2x128_si256(low0, low1, 0x20);
        __m256i high_vec = _mm256_permute2x128_si256(high0, high1, 0x20);

        // Store results
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(low + i), low_vec);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(high + i), high_vec);
    }

    // Handle remainder
#if defined(PACS_SIMD_SSSE3)
    split_16bit_to_planes_ssse3(src + i * 2, high + i, low + i, pixel_count - i);
#else
    split_16bit_to_planes_scalar(src + i * 2, high + i, low + i, pixel_count - i);
#endif
}

/**
 * @brief AVX2 byte planes to 16-bit merge
 *
 * Processes 32 pixels (64 bytes) per iteration.
 */
inline void merge_planes_to_16bit_avx2(const uint8_t* high, const uint8_t* low,
                                        uint8_t* dst,
                                        size_t pixel_count) noexcept {
    const size_t simd_count = (pixel_count / 32) * 32;

    size_t i = 0;
    for (; i < simd_count; i += 32) {
        // Load 32 bytes from each plane
        __m256i low_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(low + i));
        __m256i high_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(high + i));

        // Interleave low and high bytes
        __m256i out0 = _mm256_unpacklo_epi8(low_vec, high_vec);
        __m256i out1 = _mm256_unpackhi_epi8(low_vec, high_vec);

        // Permute to get correct order
        out0 = _mm256_permute4x64_epi64(out0, 0xD8);
        out1 = _mm256_permute4x64_epi64(out1, 0xD8);

        // Store results
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i * 2), out0);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i * 2 + 32), out1);
    }

    // Handle remainder
#if defined(PACS_SIMD_SSSE3)
    merge_planes_to_16bit_ssse3(high + i, low + i, dst + i * 2, pixel_count - i);
#else
    merge_planes_to_16bit_scalar(high + i, low + i, dst + i * 2, pixel_count - i);
#endif
}

#endif  // PACS_SIMD_AVX2

// ============================================================================
// ARM NEON implementations
// ============================================================================

#if defined(PACS_SIMD_NEON)

/**
 * @brief ARM NEON interleaved RGB to planar conversion
 *
 * Uses vld3q_u8 for efficient deinterleaving.
 * Processes 16 pixels (48 bytes) per iteration.
 */
inline void interleaved_to_planar_rgb8_neon(const uint8_t* src, uint8_t* r,
                                             uint8_t* g, uint8_t* b,
                                             size_t pixel_count) noexcept {
    const size_t simd_count = (pixel_count / 16) * 16;

    size_t i = 0;
    for (; i < simd_count; i += 16) {
        // vld3q_u8 automatically deinterleaves RGB data
        uint8x16x3_t rgb = vld3q_u8(src + i * 3);

        // Store each channel
        vst1q_u8(r + i, rgb.val[0]);
        vst1q_u8(g + i, rgb.val[1]);
        vst1q_u8(b + i, rgb.val[2]);
    }

    // Handle remainder
    interleaved_to_planar_rgb8_scalar(src + i * 3, r + i, g + i, b + i,
                                       pixel_count - i);
}

/**
 * @brief ARM NEON planar to interleaved RGB conversion
 *
 * Uses vst3q_u8 for efficient interleaving.
 * Processes 16 pixels (48 bytes) per iteration.
 */
inline void planar_to_interleaved_rgb8_neon(const uint8_t* r, const uint8_t* g,
                                             const uint8_t* b, uint8_t* dst,
                                             size_t pixel_count) noexcept {
    const size_t simd_count = (pixel_count / 16) * 16;

    size_t i = 0;
    for (; i < simd_count; i += 16) {
        // Load each channel
        uint8x16x3_t rgb;
        rgb.val[0] = vld1q_u8(r + i);
        rgb.val[1] = vld1q_u8(g + i);
        rgb.val[2] = vld1q_u8(b + i);

        // vst3q_u8 automatically interleaves RGB data
        vst3q_u8(dst + i * 3, rgb);
    }

    // Handle remainder
    planar_to_interleaved_rgb8_scalar(r + i, g + i, b + i, dst + i * 3,
                                       pixel_count - i);
}

/**
 * @brief ARM NEON 16-bit to byte planes split
 *
 * Uses vuzpq_u8 for efficient deinterleaving.
 * Processes 16 pixels (32 bytes) per iteration.
 */
inline void split_16bit_to_planes_neon(const uint8_t* src, uint8_t* high,
                                        uint8_t* low,
                                        size_t pixel_count) noexcept {
    const size_t simd_count = (pixel_count / 16) * 16;

    size_t i = 0;
    for (; i < simd_count; i += 16) {
        // Load 32 bytes (16 x 16-bit pixels)
        uint8x16_t v0 = vld1q_u8(src + i * 2);
        uint8x16_t v1 = vld1q_u8(src + i * 2 + 16);

        // Deinterleave to separate low and high bytes
        uint8x16x2_t deint0 = vuzpq_u8(v0, v1);

        // deint0.val[0] contains low bytes, deint0.val[1] contains high bytes
        vst1q_u8(low + i, deint0.val[0]);
        vst1q_u8(high + i, deint0.val[1]);
    }

    // Handle remainder
    split_16bit_to_planes_scalar(src + i * 2, high + i, low + i, pixel_count - i);
}

/**
 * @brief ARM NEON byte planes to 16-bit merge
 *
 * Uses vzipq_u8 for efficient interleaving.
 * Processes 16 pixels (32 bytes) per iteration.
 */
inline void merge_planes_to_16bit_neon(const uint8_t* high, const uint8_t* low,
                                        uint8_t* dst,
                                        size_t pixel_count) noexcept {
    const size_t simd_count = (pixel_count / 16) * 16;

    size_t i = 0;
    for (; i < simd_count; i += 16) {
        // Load 16 bytes from each plane
        uint8x16_t low_vec = vld1q_u8(low + i);
        uint8x16_t high_vec = vld1q_u8(high + i);

        // Interleave low and high bytes
        uint8x16x2_t interleaved = vzipq_u8(low_vec, high_vec);

        // Store results
        vst1q_u8(dst + i * 2, interleaved.val[0]);
        vst1q_u8(dst + i * 2 + 16, interleaved.val[1]);
    }

    // Handle remainder
    merge_planes_to_16bit_scalar(high + i, low + i, dst + i * 2, pixel_count - i);
}

#endif  // PACS_SIMD_NEON

}  // namespace detail

// ============================================================================
// Public API - dispatches to best available implementation
// ============================================================================

/**
 * @brief Convert interleaved RGB to planar format using best available SIMD
 *
 * @param src Source interleaved RGB data (RGBRGBRGB...)
 * @param r Destination R plane
 * @param g Destination G plane
 * @param b Destination B plane
 * @param pixel_count Number of pixels to convert
 */
inline void interleaved_to_planar_rgb8(const uint8_t* src, uint8_t* r,
                                        uint8_t* g, uint8_t* b,
                                        size_t pixel_count) noexcept {
    if (pixel_count == 0) {
        return;
    }

#if defined(PACS_SIMD_AVX2)
    if (has_avx2()) {
        detail::interleaved_to_planar_rgb8_avx2(src, r, g, b, pixel_count);
        return;
    }
#endif

#if defined(PACS_SIMD_SSSE3)
    if (has_ssse3()) {
        detail::interleaved_to_planar_rgb8_ssse3(src, r, g, b, pixel_count);
        return;
    }
#endif

#if defined(PACS_SIMD_NEON)
    detail::interleaved_to_planar_rgb8_neon(src, r, g, b, pixel_count);
    return;
#endif

    detail::interleaved_to_planar_rgb8_scalar(src, r, g, b, pixel_count);
}

/**
 * @brief Convert planar RGB to interleaved format using best available SIMD
 *
 * @param r Source R plane
 * @param g Source G plane
 * @param b Source B plane
 * @param dst Destination interleaved RGB data (RGBRGBRGB...)
 * @param pixel_count Number of pixels to convert
 */
inline void planar_to_interleaved_rgb8(const uint8_t* r, const uint8_t* g,
                                        const uint8_t* b, uint8_t* dst,
                                        size_t pixel_count) noexcept {
    if (pixel_count == 0) {
        return;
    }

#if defined(PACS_SIMD_AVX2)
    if (has_avx2()) {
        detail::planar_to_interleaved_rgb8_avx2(r, g, b, dst, pixel_count);
        return;
    }
#endif

#if defined(PACS_SIMD_SSSE3)
    if (has_ssse3()) {
        detail::planar_to_interleaved_rgb8_ssse3(r, g, b, dst, pixel_count);
        return;
    }
#endif

#if defined(PACS_SIMD_NEON)
    detail::planar_to_interleaved_rgb8_neon(r, g, b, dst, pixel_count);
    return;
#endif

    detail::planar_to_interleaved_rgb8_scalar(r, g, b, dst, pixel_count);
}

/**
 * @brief Split 16-bit data into high and low byte planes
 *
 * @param src Source 16-bit little-endian data
 * @param high Destination high byte plane
 * @param low Destination low byte plane
 * @param pixel_count Number of 16-bit values to split
 */
inline void split_16bit_to_planes(const uint8_t* src, uint8_t* high,
                                   uint8_t* low,
                                   size_t pixel_count) noexcept {
    if (pixel_count == 0) {
        return;
    }

#if defined(PACS_SIMD_AVX2)
    if (has_avx2()) {
        detail::split_16bit_to_planes_avx2(src, high, low, pixel_count);
        return;
    }
#endif

#if defined(PACS_SIMD_SSSE3)
    if (has_ssse3()) {
        detail::split_16bit_to_planes_ssse3(src, high, low, pixel_count);
        return;
    }
#endif

#if defined(PACS_SIMD_NEON)
    detail::split_16bit_to_planes_neon(src, high, low, pixel_count);
    return;
#endif

    detail::split_16bit_to_planes_scalar(src, high, low, pixel_count);
}

/**
 * @brief Merge high and low byte planes into 16-bit data
 *
 * @param high Source high byte plane
 * @param low Source low byte plane
 * @param dst Destination 16-bit little-endian data
 * @param pixel_count Number of 16-bit values to merge
 */
inline void merge_planes_to_16bit(const uint8_t* high, const uint8_t* low,
                                   uint8_t* dst,
                                   size_t pixel_count) noexcept {
    if (pixel_count == 0) {
        return;
    }

#if defined(PACS_SIMD_AVX2)
    if (has_avx2()) {
        detail::merge_planes_to_16bit_avx2(high, low, dst, pixel_count);
        return;
    }
#endif

#if defined(PACS_SIMD_SSSE3)
    if (has_ssse3()) {
        detail::merge_planes_to_16bit_ssse3(high, low, dst, pixel_count);
        return;
    }
#endif

#if defined(PACS_SIMD_NEON)
    detail::merge_planes_to_16bit_neon(high, low, dst, pixel_count);
    return;
#endif

    detail::merge_planes_to_16bit_scalar(high, low, dst, pixel_count);
}

}  // namespace pacs::encoding::simd

#endif  // PACS_ENCODING_SIMD_RLE_HPP
