/**
 * @file simd_types.hpp
 * @brief Platform-specific SIMD type definitions and wrappers
 *
 * Provides portable type aliases and wrapper classes for SIMD operations
 * across x86 (SSE2/AVX2) and ARM (NEON) platforms.
 */

#ifndef PACS_ENCODING_SIMD_TYPES_HPP
#define PACS_ENCODING_SIMD_TYPES_HPP

#include "simd_config.hpp"

#include <cstddef>
#include <cstdint>

namespace pacs::encoding::simd {

// Memory alignment requirements
constexpr size_t SSE_ALIGNMENT = 16;
constexpr size_t AVX_ALIGNMENT = 32;
constexpr size_t AVX512_ALIGNMENT = 64;

#if defined(PACS_ARCH_X64) || defined(PACS_ARCH_X86)

// SSE2 types (128-bit)
#if defined(PACS_SIMD_SSE2)
using vec128i = __m128i;
using vec128f = __m128;
using vec128d = __m128d;
#endif

// AVX/AVX2 types (256-bit)
#if defined(PACS_SIMD_AVX2)
using vec256i = __m256i;
using vec256f = __m256;
using vec256d = __m256d;
#endif

// AVX-512 types (512-bit)
#if defined(PACS_SIMD_AVX512)
using vec512i = __m512i;
using vec512f = __m512;
using vec512d = __m512d;
#endif

#elif defined(PACS_SIMD_NEON)

// NEON types (128-bit)
using vec128i8 = int8x16_t;
using vec128u8 = uint8x16_t;
using vec128i16 = int16x8_t;
using vec128u16 = uint16x8_t;
using vec128i32 = int32x4_t;
using vec128u32 = uint32x4_t;
using vec128i64 = int64x2_t;
using vec128u64 = uint64x2_t;
using vec128f = float32x4_t;

#if defined(__aarch64__)
using vec128d = float64x2_t;
#endif

#endif  // Architecture

/**
 * @brief Portable 128-bit integer vector wrapper
 */
struct alignas(SSE_ALIGNMENT) vec128_int {
#if defined(PACS_SIMD_SSE2)
    __m128i data;

    vec128_int() noexcept : data(_mm_setzero_si128()) {}
    explicit vec128_int(__m128i v) noexcept : data(v) {}

    static vec128_int load(const void* ptr) noexcept {
        return vec128_int(_mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr)));
    }

    static vec128_int load_aligned(const void* ptr) noexcept {
        return vec128_int(_mm_load_si128(reinterpret_cast<const __m128i*>(ptr)));
    }

    void store(void* ptr) const noexcept {
        _mm_storeu_si128(reinterpret_cast<__m128i*>(ptr), data);
    }

    void store_aligned(void* ptr) const noexcept {
        _mm_store_si128(reinterpret_cast<__m128i*>(ptr), data);
    }

#elif defined(PACS_SIMD_NEON)
    uint8x16_t data;

    vec128_int() noexcept : data(vdupq_n_u8(0)) {}
    explicit vec128_int(uint8x16_t v) noexcept : data(v) {}

    static vec128_int load(const void* ptr) noexcept {
        return vec128_int(vld1q_u8(reinterpret_cast<const uint8_t*>(ptr)));
    }

    static vec128_int load_aligned(const void* ptr) noexcept {
        return load(ptr);  // NEON handles unaligned loads efficiently
    }

    void store(void* ptr) const noexcept {
        vst1q_u8(reinterpret_cast<uint8_t*>(ptr), data);
    }

    void store_aligned(void* ptr) const noexcept {
        store(ptr);
    }

#else
    // Scalar fallback
    alignas(16) uint8_t bytes[16]{};

    vec128_int() noexcept = default;

    static vec128_int load(const void* ptr) noexcept {
        vec128_int result;
        std::memcpy(result.bytes, ptr, 16);
        return result;
    }

    static vec128_int load_aligned(const void* ptr) noexcept {
        return load(ptr);
    }

    void store(void* ptr) const noexcept {
        std::memcpy(ptr, bytes, 16);
    }

    void store_aligned(void* ptr) const noexcept {
        store(ptr);
    }
#endif
};

/**
 * @brief Portable 256-bit integer vector wrapper
 */
struct alignas(AVX_ALIGNMENT) vec256_int {
#if defined(PACS_SIMD_AVX2)
    __m256i data;

    vec256_int() noexcept : data(_mm256_setzero_si256()) {}
    explicit vec256_int(__m256i v) noexcept : data(v) {}

    static vec256_int load(const void* ptr) noexcept {
        return vec256_int(
            _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr)));
    }

    static vec256_int load_aligned(const void* ptr) noexcept {
        return vec256_int(
            _mm256_load_si256(reinterpret_cast<const __m256i*>(ptr)));
    }

    void store(void* ptr) const noexcept {
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(ptr), data);
    }

    void store_aligned(void* ptr) const noexcept {
        _mm256_store_si256(reinterpret_cast<__m256i*>(ptr), data);
    }

#else
    // Fallback using two 128-bit vectors
    vec128_int low;
    vec128_int high;

    vec256_int() noexcept = default;
    vec256_int(vec128_int l, vec128_int h) noexcept : low(l), high(h) {}

    static vec256_int load(const void* ptr) noexcept {
        const auto* p = reinterpret_cast<const uint8_t*>(ptr);
        return vec256_int(vec128_int::load(p), vec128_int::load(p + 16));
    }

    static vec256_int load_aligned(const void* ptr) noexcept {
        const auto* p = reinterpret_cast<const uint8_t*>(ptr);
        return vec256_int(vec128_int::load_aligned(p),
                          vec128_int::load_aligned(p + 16));
    }

    void store(void* ptr) const noexcept {
        auto* p = reinterpret_cast<uint8_t*>(ptr);
        low.store(p);
        high.store(p + 16);
    }

    void store_aligned(void* ptr) const noexcept {
        auto* p = reinterpret_cast<uint8_t*>(ptr);
        low.store_aligned(p);
        high.store_aligned(p + 16);
    }
#endif
};

/**
 * @brief Check if a pointer is aligned to the specified boundary
 */
template <size_t Alignment>
[[nodiscard]] constexpr bool is_aligned(const void* ptr) noexcept {
    return (reinterpret_cast<uintptr_t>(ptr) % Alignment) == 0;
}

/**
 * @brief Get the aligned portion start offset
 */
template <size_t Alignment>
[[nodiscard]] constexpr size_t align_offset(const void* ptr) noexcept {
    const auto addr = reinterpret_cast<uintptr_t>(ptr);
    const auto remainder = addr % Alignment;
    return remainder == 0 ? 0 : Alignment - remainder;
}

}  // namespace pacs::encoding::simd

#endif  // PACS_ENCODING_SIMD_TYPES_HPP
