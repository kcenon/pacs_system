/**
 * @file simd_config.hpp
 * @brief SIMD configuration and CPU feature detection
 *
 * Provides compile-time and runtime detection of SIMD capabilities
 * for x86 (SSE2/AVX2/AVX-512) and ARM (NEON) platforms.
 *
 * @see DICOM PS3.5 - Performance optimization for pixel data processing
 */

#ifndef PACS_ENCODING_SIMD_CONFIG_HPP
#define PACS_ENCODING_SIMD_CONFIG_HPP

#include <cstdint>

// Platform detection
#if defined(_MSC_VER)
    #define PACS_COMPILER_MSVC 1
#elif defined(__clang__)
    #define PACS_COMPILER_CLANG 1
#elif defined(__GNUC__)
    #define PACS_COMPILER_GCC 1
#endif

// Architecture detection
#if defined(__x86_64__) || defined(_M_X64)
    #define PACS_ARCH_X64 1
#elif defined(__i386__) || defined(_M_IX86)
    #define PACS_ARCH_X86 1
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define PACS_ARCH_ARM64 1
#elif defined(__arm__) || defined(_M_ARM)
    #define PACS_ARCH_ARM32 1
#endif

// SIMD feature detection (compile-time)
#if defined(PACS_ARCH_X64) || defined(PACS_ARCH_X86)
    #if defined(__AVX512F__)
        #define PACS_SIMD_AVX512 1
    #endif
    #if defined(__AVX2__)
        #define PACS_SIMD_AVX2 1
    #endif
    #if defined(__AVX__)
        #define PACS_SIMD_AVX 1
    #endif
    #if defined(__SSE4_1__)
        #define PACS_SIMD_SSE41 1
    #endif
    #if defined(__SSSE3__)
        #define PACS_SIMD_SSSE3 1
    #endif
    #if defined(__SSE2__) || defined(_M_X64)
        #define PACS_SIMD_SSE2 1
    #endif
#elif defined(PACS_ARCH_ARM64) || defined(PACS_ARCH_ARM32)
    #if defined(__ARM_NEON) || defined(__ARM_NEON__)
        #define PACS_SIMD_NEON 1
    #endif
#endif

// Include appropriate intrinsics headers
#if defined(PACS_SIMD_AVX512) || defined(PACS_SIMD_AVX2) || \
    defined(PACS_SIMD_AVX) || defined(PACS_SIMD_SSE41) || \
    defined(PACS_SIMD_SSSE3) || defined(PACS_SIMD_SSE2)
    #if defined(PACS_COMPILER_MSVC)
        #include <intrin.h>
    #else
        #include <x86intrin.h>
    #endif
#elif defined(PACS_SIMD_NEON)
    #include <arm_neon.h>
#endif

namespace pacs::encoding::simd {

/**
 * @brief SIMD feature flags for runtime detection
 */
enum class simd_feature : uint32_t {
    none = 0,
    sse2 = 1 << 0,
    ssse3 = 1 << 1,
    sse41 = 1 << 2,
    avx = 1 << 3,
    avx2 = 1 << 4,
    avx512f = 1 << 5,
    neon = 1 << 6
};

/**
 * @brief Bitwise OR for simd_feature flags
 */
[[nodiscard]] constexpr simd_feature operator|(simd_feature a,
                                               simd_feature b) noexcept {
    return static_cast<simd_feature>(static_cast<uint32_t>(a) |
                                     static_cast<uint32_t>(b));
}

/**
 * @brief Bitwise AND for simd_feature flags
 */
[[nodiscard]] constexpr simd_feature operator&(simd_feature a,
                                               simd_feature b) noexcept {
    return static_cast<simd_feature>(static_cast<uint32_t>(a) &
                                     static_cast<uint32_t>(b));
}

/**
 * @brief Check if a specific feature is set
 */
[[nodiscard]] constexpr bool has_feature(simd_feature features,
                                         simd_feature check) noexcept {
    return (features & check) == check;
}

namespace detail {

#if defined(PACS_ARCH_X64) || defined(PACS_ARCH_X86)

/**
 * @brief Execute CPUID instruction (x86/x64)
 */
inline void cpuid(int info[4], int function_id) noexcept {
#if defined(PACS_COMPILER_MSVC)
    __cpuid(info, function_id);
#else
    __asm__ __volatile__(
        "cpuid"
        : "=a"(info[0]), "=b"(info[1]), "=c"(info[2]), "=d"(info[3])
        : "a"(function_id), "c"(0));
#endif
}

/**
 * @brief Execute CPUID instruction with ECX parameter
 */
inline void cpuid_ex(int info[4], int function_id, int sub_function) noexcept {
#if defined(PACS_COMPILER_MSVC)
    __cpuidex(info, function_id, sub_function);
#else
    __asm__ __volatile__(
        "cpuid"
        : "=a"(info[0]), "=b"(info[1]), "=c"(info[2]), "=d"(info[3])
        : "a"(function_id), "c"(sub_function));
#endif
}

#endif  // x86/x64

}  // namespace detail

/**
 * @brief Detect available SIMD features at runtime
 * @return Bitwise OR of available simd_feature flags
 */
[[nodiscard]] inline simd_feature detect_features() noexcept {
    simd_feature features = simd_feature::none;

#if defined(PACS_ARCH_X64) || defined(PACS_ARCH_X86)
    int info[4] = {0};

    // Check basic CPUID
    detail::cpuid(info, 0);
    int max_function = info[0];

    if (max_function >= 1) {
        detail::cpuid(info, 1);

        // EDX features
        if (info[3] & (1 << 26)) {
            features = features | simd_feature::sse2;
        }

        // ECX features
        if (info[2] & (1 << 0)) {
            features = features | simd_feature::ssse3;
        }
        if (info[2] & (1 << 19)) {
            features = features | simd_feature::sse41;
        }
        if (info[2] & (1 << 28)) {
            features = features | simd_feature::avx;
        }
    }

    if (max_function >= 7) {
        detail::cpuid_ex(info, 7, 0);

        // EBX features
        if (info[1] & (1 << 5)) {
            features = features | simd_feature::avx2;
        }
        if (info[1] & (1 << 16)) {
            features = features | simd_feature::avx512f;
        }
    }

#elif defined(PACS_ARCH_ARM64)
    // ARM64 always has NEON
    features = features | simd_feature::neon;

#elif defined(PACS_ARCH_ARM32)
    // ARM32: Check for NEON via auxiliary vector or runtime detection
    // For simplicity, assume NEON is available if compiled with NEON support
#if defined(PACS_SIMD_NEON)
    features = features | simd_feature::neon;
#endif

#endif  // Architecture

    return features;
}

/**
 * @brief Get cached SIMD features (singleton pattern)
 * @return Detected SIMD features
 */
[[nodiscard]] inline simd_feature get_features() noexcept {
    static const simd_feature features = detect_features();
    return features;
}

/**
 * @brief Check if SSE2 is available
 */
[[nodiscard]] inline bool has_sse2() noexcept {
    return has_feature(get_features(), simd_feature::sse2);
}

/**
 * @brief Check if SSSE3 is available
 */
[[nodiscard]] inline bool has_ssse3() noexcept {
    return has_feature(get_features(), simd_feature::ssse3);
}

/**
 * @brief Check if SSE4.1 is available
 */
[[nodiscard]] inline bool has_sse41() noexcept {
    return has_feature(get_features(), simd_feature::sse41);
}

/**
 * @brief Check if AVX is available
 */
[[nodiscard]] inline bool has_avx() noexcept {
    return has_feature(get_features(), simd_feature::avx);
}

/**
 * @brief Check if AVX2 is available
 */
[[nodiscard]] inline bool has_avx2() noexcept {
    return has_feature(get_features(), simd_feature::avx2);
}

/**
 * @brief Check if AVX-512F is available
 */
[[nodiscard]] inline bool has_avx512f() noexcept {
    return has_feature(get_features(), simd_feature::avx512f);
}

/**
 * @brief Check if NEON is available
 */
[[nodiscard]] inline bool has_neon() noexcept {
    return has_feature(get_features(), simd_feature::neon);
}

/**
 * @brief Get optimal vector width in bytes for current CPU
 * @return Vector width (16 for SSE/NEON, 32 for AVX2, 64 for AVX-512)
 */
[[nodiscard]] inline size_t optimal_vector_width() noexcept {
    if (has_avx512f()) {
        return 64;
    }
    if (has_avx2()) {
        return 32;
    }
    if (has_sse2() || has_neon()) {
        return 16;
    }
    return 0;  // No SIMD
}

}  // namespace pacs::encoding::simd

#endif  // PACS_ENCODING_SIMD_CONFIG_HPP
