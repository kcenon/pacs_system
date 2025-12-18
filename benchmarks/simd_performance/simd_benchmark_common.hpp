/**
 * @file simd_benchmark_common.hpp
 * @brief Common utilities for SIMD performance benchmarks
 *
 * Provides data generators, timing utilities, and statistics accumulators
 * specifically designed for measuring SIMD optimization performance.
 *
 * @see Issue #332 - Add SIMD optimization benchmarks
 */

#ifndef PACS_BENCHMARKS_SIMD_PERFORMANCE_SIMD_BENCHMARK_COMMON_HPP
#define PACS_BENCHMARKS_SIMD_PERFORMANCE_SIMD_BENCHMARK_COMMON_HPP

#include "pacs/encoding/simd/simd_config.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace pacs::benchmark::simd {

// =============================================================================
// Constants
// =============================================================================

/// Data sizes for benchmarks
constexpr size_t kSmallSize = 1024;              // 1 KB
constexpr size_t kMediumSize = 1024 * 1024;      // 1 MB
constexpr size_t kLargeSize = 16 * 1024 * 1024;  // 16 MB

/// Default number of iterations for warm-up
constexpr size_t kWarmupIterations = 3;

/// Default number of iterations for measurement
constexpr size_t kBenchmarkIterations = 10;

// =============================================================================
// Timing Utilities
// =============================================================================

/**
 * @brief High-resolution timer for precise measurements
 */
class high_resolution_timer {
public:
    void start() noexcept {
        start_time_ = std::chrono::high_resolution_clock::now();
    }

    void stop() noexcept {
        end_time_ = std::chrono::high_resolution_clock::now();
    }

    [[nodiscard]] std::chrono::nanoseconds elapsed_ns() const noexcept {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            end_time_ - start_time_);
    }

    [[nodiscard]] std::chrono::microseconds elapsed_us() const noexcept {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            end_time_ - start_time_);
    }

    [[nodiscard]] double elapsed_ms() const noexcept {
        return std::chrono::duration<double, std::milli>(
            end_time_ - start_time_).count();
    }

    [[nodiscard]] double elapsed_seconds() const noexcept {
        return std::chrono::duration<double>(end_time_ - start_time_).count();
    }

private:
    std::chrono::high_resolution_clock::time_point start_time_;
    std::chrono::high_resolution_clock::time_point end_time_;
};

/**
 * @brief Statistics accumulator for benchmark results
 */
struct benchmark_stats {
    size_t count{0};
    double sum_ns{0.0};
    double sum_squared_ns{0.0};
    double min_ns{std::numeric_limits<double>::max()};
    double max_ns{0.0};

    void record(double duration_ns) noexcept {
        ++count;
        sum_ns += duration_ns;
        sum_squared_ns += duration_ns * duration_ns;
        min_ns = std::min(min_ns, duration_ns);
        max_ns = std::max(max_ns, duration_ns);
    }

    [[nodiscard]] double mean_ns() const noexcept {
        return count > 0 ? sum_ns / static_cast<double>(count) : 0.0;
    }

    [[nodiscard]] double mean_us() const noexcept {
        return mean_ns() / 1000.0;
    }

    [[nodiscard]] double mean_ms() const noexcept {
        return mean_ns() / 1000000.0;
    }

    [[nodiscard]] double stddev_ns() const noexcept {
        if (count < 2) return 0.0;
        double mean = mean_ns();
        double variance = (sum_squared_ns / static_cast<double>(count)) - (mean * mean);
        return std::sqrt(std::max(0.0, variance));
    }

    /**
     * @brief Calculate throughput in bytes per second
     * @param bytes Number of bytes processed per iteration
     */
    [[nodiscard]] double throughput_bytes_per_sec(size_t bytes) const noexcept {
        double mean_sec = mean_ns() / 1e9;
        return mean_sec > 0.0 ? static_cast<double>(bytes) / mean_sec : 0.0;
    }

    /**
     * @brief Calculate throughput in MB/s
     * @param bytes Number of bytes processed per iteration
     */
    [[nodiscard]] double throughput_mb_per_sec(size_t bytes) const noexcept {
        return throughput_bytes_per_sec(bytes) / (1024.0 * 1024.0);
    }

    /**
     * @brief Calculate throughput in GB/s
     * @param bytes Number of bytes processed per iteration
     */
    [[nodiscard]] double throughput_gb_per_sec(size_t bytes) const noexcept {
        return throughput_bytes_per_sec(bytes) / (1024.0 * 1024.0 * 1024.0);
    }
};

// =============================================================================
// Data Generators
// =============================================================================

/**
 * @brief Generate random byte data for benchmarking
 * @param size Number of bytes to generate
 * @param seed Random seed for reproducibility
 * @return Vector of random bytes
 */
inline std::vector<uint8_t> generate_random_data(size_t size, uint32_t seed = 42) {
    std::vector<uint8_t> data(size);
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> dist(0, 255);

    for (auto& byte : data) {
        byte = static_cast<uint8_t>(dist(rng));
    }

    return data;
}

/**
 * @brief Generate gradient data (good for testing compression)
 * @param size Number of bytes to generate
 * @return Vector of gradient bytes
 */
inline std::vector<uint8_t> generate_gradient_data(size_t size) {
    std::vector<uint8_t> data(size);
    for (size_t i = 0; i < size; ++i) {
        data[i] = static_cast<uint8_t>(i % 256);
    }
    return data;
}

/**
 * @brief Generate RGB interleaved data
 * @param pixel_count Number of pixels (each pixel is 3 bytes)
 * @param seed Random seed for reproducibility
 * @return Vector of interleaved RGB data
 */
inline std::vector<uint8_t> generate_rgb_data(size_t pixel_count, uint32_t seed = 42) {
    std::vector<uint8_t> data(pixel_count * 3);
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> dist(0, 255);

    for (auto& byte : data) {
        byte = static_cast<uint8_t>(dist(rng));
    }

    return data;
}

/**
 * @brief Generate 16-bit pixel data
 * @param pixel_count Number of 16-bit pixels
 * @param seed Random seed for reproducibility
 * @return Vector of 16-bit data as bytes (little-endian)
 */
inline std::vector<uint8_t> generate_16bit_data(size_t pixel_count, uint32_t seed = 42) {
    std::vector<uint8_t> data(pixel_count * 2);
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> dist(0, 65535);

    for (size_t i = 0; i < pixel_count; ++i) {
        uint16_t value = static_cast<uint16_t>(dist(rng));
        data[i * 2] = static_cast<uint8_t>(value & 0xFF);
        data[i * 2 + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    }

    return data;
}

// =============================================================================
// Result Formatting
// =============================================================================

/**
 * @brief Format data size in human-readable form
 * @param bytes Number of bytes
 * @return Formatted string (e.g., "1.5 MB")
 */
inline std::string format_size(size_t bytes) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);

    if (bytes >= 1024 * 1024 * 1024) {
        oss << static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0) << " GB";
    } else if (bytes >= 1024 * 1024) {
        oss << static_cast<double>(bytes) / (1024.0 * 1024.0) << " MB";
    } else if (bytes >= 1024) {
        oss << static_cast<double>(bytes) / 1024.0 << " KB";
    } else {
        oss << bytes << " B";
    }

    return oss.str();
}

/**
 * @brief Format throughput in human-readable form
 * @param bytes_per_sec Throughput in bytes per second
 * @return Formatted string (e.g., "2.5 GB/s")
 */
inline std::string format_throughput(double bytes_per_sec) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);

    if (bytes_per_sec >= 1024.0 * 1024.0 * 1024.0) {
        oss << bytes_per_sec / (1024.0 * 1024.0 * 1024.0) << " GB/s";
    } else if (bytes_per_sec >= 1024.0 * 1024.0) {
        oss << bytes_per_sec / (1024.0 * 1024.0) << " MB/s";
    } else if (bytes_per_sec >= 1024.0) {
        oss << bytes_per_sec / 1024.0 << " KB/s";
    } else {
        oss << bytes_per_sec << " B/s";
    }

    return oss.str();
}

/**
 * @brief Format time duration in human-readable form
 * @param ns Duration in nanoseconds
 * @return Formatted string (e.g., "1.5 ms")
 */
inline std::string format_duration(double ns) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);

    if (ns >= 1e9) {
        oss << ns / 1e9 << " s";
    } else if (ns >= 1e6) {
        oss << ns / 1e6 << " ms";
    } else if (ns >= 1e3) {
        oss << ns / 1e3 << " us";
    } else {
        oss << ns << " ns";
    }

    return oss.str();
}

/**
 * @brief Calculate speedup ratio
 * @param baseline_ns Baseline time in nanoseconds
 * @param optimized_ns Optimized time in nanoseconds
 * @return Speedup ratio (e.g., 2.5x means 2.5 times faster)
 */
inline double calculate_speedup(double baseline_ns, double optimized_ns) {
    if (optimized_ns <= 0.0) return 0.0;
    return baseline_ns / optimized_ns;
}

/**
 * @brief Format speedup ratio
 * @param speedup Speedup ratio
 * @return Formatted string (e.g., "2.5x faster")
 */
inline std::string format_speedup(double speedup) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << speedup << "x";
    return oss.str();
}

// =============================================================================
// SIMD Feature Detection
// =============================================================================

/**
 * @brief Get string representation of available SIMD features
 * @return String describing available SIMD instruction sets
 */
inline std::string get_simd_features_string() {
    std::ostringstream oss;
    oss << "SIMD Features: ";

#if defined(PACS_SIMD_AVX2)
    if (encoding::simd::has_avx2()) {
        oss << "AVX2 ";
    }
#endif

#if defined(PACS_SIMD_SSSE3)
    if (encoding::simd::has_ssse3()) {
        oss << "SSSE3 ";
    }
#endif

#if defined(PACS_SIMD_SSE2)
    if (encoding::simd::has_sse2()) {
        oss << "SSE2 ";
    }
#endif

#if defined(PACS_SIMD_NEON)
    oss << "NEON ";
#endif

    return oss.str();
}

}  // namespace pacs::benchmark::simd

#endif  // PACS_BENCHMARKS_SIMD_PERFORMANCE_SIMD_BENCHMARK_COMMON_HPP
