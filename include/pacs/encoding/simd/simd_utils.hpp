/**
 * @file simd_utils.hpp
 * @brief Common SIMD utility functions
 *
 * Provides portable SIMD operations for common tasks like byte swapping,
 * data conversion, and bulk memory operations.
 */

#ifndef PACS_ENCODING_SIMD_UTILS_HPP
#define PACS_ENCODING_SIMD_UTILS_HPP

#include "simd_config.hpp"
#include "simd_types.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace pacs::encoding::simd {

// Forward declarations for byte swap functions
void swap_bytes_16_simd(const uint8_t* src, uint8_t* dst,
                        size_t count) noexcept;
void swap_bytes_32_simd(const uint8_t* src, uint8_t* dst,
                        size_t count) noexcept;
void swap_bytes_64_simd(const uint8_t* src, uint8_t* dst,
                        size_t count) noexcept;

namespace detail {

// Scalar fallback implementations
inline void swap_bytes_16_scalar(const uint8_t* src, uint8_t* dst,
                                 size_t byte_count) noexcept {
    for (size_t i = 0; i + 1 < byte_count; i += 2) {
        dst[i] = src[i + 1];
        dst[i + 1] = src[i];
    }
}

inline void swap_bytes_32_scalar(const uint8_t* src, uint8_t* dst,
                                 size_t byte_count) noexcept {
    for (size_t i = 0; i + 3 < byte_count; i += 4) {
        dst[i] = src[i + 3];
        dst[i + 1] = src[i + 2];
        dst[i + 2] = src[i + 1];
        dst[i + 3] = src[i];
    }
}

inline void swap_bytes_64_scalar(const uint8_t* src, uint8_t* dst,
                                 size_t byte_count) noexcept {
    for (size_t i = 0; i + 7 < byte_count; i += 8) {
        dst[i] = src[i + 7];
        dst[i + 1] = src[i + 6];
        dst[i + 2] = src[i + 5];
        dst[i + 3] = src[i + 4];
        dst[i + 4] = src[i + 3];
        dst[i + 5] = src[i + 2];
        dst[i + 6] = src[i + 1];
        dst[i + 7] = src[i];
    }
}

#if defined(PACS_SIMD_SSSE3)

// SSSE3 shuffle masks for byte swapping
inline __m128i get_swap16_mask() noexcept {
    // Swap adjacent bytes: [0,1,2,3,...] -> [1,0,3,2,...]
    return _mm_setr_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14);
}

inline __m128i get_swap32_mask() noexcept {
    // Reverse 4-byte groups: [0,1,2,3,...] -> [3,2,1,0,7,6,5,4,...]
    return _mm_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12);
}

inline __m128i get_swap64_mask() noexcept {
    // Reverse 8-byte groups
    return _mm_setr_epi8(7, 6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11, 10, 9, 8);
}

inline void swap_bytes_16_ssse3(const uint8_t* src, uint8_t* dst,
                                size_t byte_count) noexcept {
    const __m128i mask = get_swap16_mask();
    const size_t simd_count = (byte_count / 16) * 16;

    size_t i = 0;
    for (; i < simd_count; i += 16) {
        __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i));
        v = _mm_shuffle_epi8(v, mask);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + i), v);
    }

    // Handle remainder
    swap_bytes_16_scalar(src + i, dst + i, byte_count - i);
}

inline void swap_bytes_32_ssse3(const uint8_t* src, uint8_t* dst,
                                size_t byte_count) noexcept {
    const __m128i mask = get_swap32_mask();
    const size_t simd_count = (byte_count / 16) * 16;

    size_t i = 0;
    for (; i < simd_count; i += 16) {
        __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i));
        v = _mm_shuffle_epi8(v, mask);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + i), v);
    }

    // Handle remainder
    swap_bytes_32_scalar(src + i, dst + i, byte_count - i);
}

inline void swap_bytes_64_ssse3(const uint8_t* src, uint8_t* dst,
                                size_t byte_count) noexcept {
    const __m128i mask = get_swap64_mask();
    const size_t simd_count = (byte_count / 16) * 16;

    size_t i = 0;
    for (; i < simd_count; i += 16) {
        __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + i));
        v = _mm_shuffle_epi8(v, mask);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + i), v);
    }

    // Handle remainder
    swap_bytes_64_scalar(src + i, dst + i, byte_count - i);
}

#endif  // PACS_SIMD_SSSE3

#if defined(PACS_SIMD_AVX2)

inline __m256i get_swap16_mask_256() noexcept {
    return _mm256_setr_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14,
                           1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14);
}

inline __m256i get_swap32_mask_256() noexcept {
    return _mm256_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12,
                           3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12);
}

inline __m256i get_swap64_mask_256() noexcept {
    return _mm256_setr_epi8(7, 6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11, 10, 9, 8,
                           7, 6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11, 10, 9, 8);
}

inline void swap_bytes_16_avx2(const uint8_t* src, uint8_t* dst,
                               size_t byte_count) noexcept {
    const __m256i mask = get_swap16_mask_256();
    const size_t simd_count = (byte_count / 32) * 32;

    size_t i = 0;
    for (; i < simd_count; i += 32) {
        __m256i v =
            _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i));
        v = _mm256_shuffle_epi8(v, mask);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i), v);
    }

    // Handle remainder with SSSE3 or scalar
#if defined(PACS_SIMD_SSSE3)
    swap_bytes_16_ssse3(src + i, dst + i, byte_count - i);
#else
    swap_bytes_16_scalar(src + i, dst + i, byte_count - i);
#endif
}

inline void swap_bytes_32_avx2(const uint8_t* src, uint8_t* dst,
                               size_t byte_count) noexcept {
    const __m256i mask = get_swap32_mask_256();
    const size_t simd_count = (byte_count / 32) * 32;

    size_t i = 0;
    for (; i < simd_count; i += 32) {
        __m256i v =
            _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i));
        v = _mm256_shuffle_epi8(v, mask);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i), v);
    }

#if defined(PACS_SIMD_SSSE3)
    swap_bytes_32_ssse3(src + i, dst + i, byte_count - i);
#else
    swap_bytes_32_scalar(src + i, dst + i, byte_count - i);
#endif
}

inline void swap_bytes_64_avx2(const uint8_t* src, uint8_t* dst,
                               size_t byte_count) noexcept {
    const __m256i mask = get_swap64_mask_256();
    const size_t simd_count = (byte_count / 32) * 32;

    size_t i = 0;
    for (; i < simd_count; i += 32) {
        __m256i v =
            _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + i));
        v = _mm256_shuffle_epi8(v, mask);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i), v);
    }

#if defined(PACS_SIMD_SSSE3)
    swap_bytes_64_ssse3(src + i, dst + i, byte_count - i);
#else
    swap_bytes_64_scalar(src + i, dst + i, byte_count - i);
#endif
}

#endif  // PACS_SIMD_AVX2

#if defined(PACS_SIMD_NEON)

inline void swap_bytes_16_neon(const uint8_t* src, uint8_t* dst,
                               size_t byte_count) noexcept {
    const size_t simd_count = (byte_count / 16) * 16;

    size_t i = 0;
    for (; i < simd_count; i += 16) {
        uint8x16_t v = vld1q_u8(src + i);
        // vrev16q_u8 reverses bytes within 16-bit elements
        v = vrev16q_u8(v);
        vst1q_u8(dst + i, v);
    }

    // Handle remainder
    swap_bytes_16_scalar(src + i, dst + i, byte_count - i);
}

inline void swap_bytes_32_neon(const uint8_t* src, uint8_t* dst,
                               size_t byte_count) noexcept {
    const size_t simd_count = (byte_count / 16) * 16;

    size_t i = 0;
    for (; i < simd_count; i += 16) {
        uint8x16_t v = vld1q_u8(src + i);
        // vrev32q_u8 reverses bytes within 32-bit elements
        v = vrev32q_u8(v);
        vst1q_u8(dst + i, v);
    }

    // Handle remainder
    swap_bytes_32_scalar(src + i, dst + i, byte_count - i);
}

inline void swap_bytes_64_neon(const uint8_t* src, uint8_t* dst,
                               size_t byte_count) noexcept {
    const size_t simd_count = (byte_count / 16) * 16;

    size_t i = 0;
    for (; i < simd_count; i += 16) {
        uint8x16_t v = vld1q_u8(src + i);
        // vrev64q_u8 reverses bytes within 64-bit elements
        v = vrev64q_u8(v);
        vst1q_u8(dst + i, v);
    }

    // Handle remainder
    swap_bytes_64_scalar(src + i, dst + i, byte_count - i);
}

#endif  // PACS_SIMD_NEON

}  // namespace detail

/**
 * @brief Swap bytes in 16-bit words using best available SIMD
 * @param src Source data pointer
 * @param dst Destination data pointer
 * @param byte_count Number of bytes (should be even)
 */
inline void swap_bytes_16_simd(const uint8_t* src, uint8_t* dst,
                               size_t byte_count) noexcept {
    if (byte_count < 2) {
        return;
    }

#if defined(PACS_SIMD_AVX2)
    if (has_avx2()) {
        detail::swap_bytes_16_avx2(src, dst, byte_count);
        return;
    }
#endif

#if defined(PACS_SIMD_SSSE3)
    if (has_ssse3()) {
        detail::swap_bytes_16_ssse3(src, dst, byte_count);
        return;
    }
#endif

#if defined(PACS_SIMD_NEON)
    detail::swap_bytes_16_neon(src, dst, byte_count);
    return;
#endif

    detail::swap_bytes_16_scalar(src, dst, byte_count);
}

/**
 * @brief Swap bytes in 32-bit words using best available SIMD
 * @param src Source data pointer
 * @param dst Destination data pointer
 * @param byte_count Number of bytes (should be multiple of 4)
 */
inline void swap_bytes_32_simd(const uint8_t* src, uint8_t* dst,
                               size_t byte_count) noexcept {
    if (byte_count < 4) {
        return;
    }

#if defined(PACS_SIMD_AVX2)
    if (has_avx2()) {
        detail::swap_bytes_32_avx2(src, dst, byte_count);
        return;
    }
#endif

#if defined(PACS_SIMD_SSSE3)
    if (has_ssse3()) {
        detail::swap_bytes_32_ssse3(src, dst, byte_count);
        return;
    }
#endif

#if defined(PACS_SIMD_NEON)
    detail::swap_bytes_32_neon(src, dst, byte_count);
    return;
#endif

    detail::swap_bytes_32_scalar(src, dst, byte_count);
}

/**
 * @brief Swap bytes in 64-bit words using best available SIMD
 * @param src Source data pointer
 * @param dst Destination data pointer
 * @param byte_count Number of bytes (should be multiple of 8)
 */
inline void swap_bytes_64_simd(const uint8_t* src, uint8_t* dst,
                               size_t byte_count) noexcept {
    if (byte_count < 8) {
        return;
    }

#if defined(PACS_SIMD_AVX2)
    if (has_avx2()) {
        detail::swap_bytes_64_avx2(src, dst, byte_count);
        return;
    }
#endif

#if defined(PACS_SIMD_SSSE3)
    if (has_ssse3()) {
        detail::swap_bytes_64_ssse3(src, dst, byte_count);
        return;
    }
#endif

#if defined(PACS_SIMD_NEON)
    detail::swap_bytes_64_neon(src, dst, byte_count);
    return;
#endif

    detail::swap_bytes_64_scalar(src, dst, byte_count);
}

}  // namespace pacs::encoding::simd

#endif  // PACS_ENCODING_SIMD_UTILS_HPP
