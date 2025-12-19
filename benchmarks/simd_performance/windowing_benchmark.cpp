/**
 * @file windowing_benchmark.cpp
 * @brief Performance benchmarks for SIMD window/level operations
 *
 * Measures the performance of window/level (VOI LUT) transformations:
 * - 8-bit grayscale window/level
 * - 16-bit grayscale window/level
 * - LUT-based optimization comparison
 *
 * @see Issue #313 - Apply SIMD Optimization (Image Processing)
 */

#include "simd_benchmark_common.hpp"
#include "pacs/encoding/simd/simd_windowing.hpp"

#include <iostream>
#include <vector>

using namespace pacs::benchmark::simd;
using namespace pacs::encoding::simd;

namespace {

// =============================================================================
// Benchmark: 8-bit Window/Level
// =============================================================================

void benchmark_window_level_8bit(size_t pixel_count, size_t iterations) {
    std::cout << "\n=== 8-bit Window/Level ===\n";
    std::cout << "Pixel count: " << pixel_count << " (" << format_size(pixel_count) << ")\n";

    // Generate test data
    auto src = generate_random_data(pixel_count);
    std::vector<uint8_t> dst(pixel_count);

    // Typical window/level parameters
    window_level_params params(128.0, 256.0, false);

    // Warm-up
    for (size_t i = 0; i < kWarmupIterations; ++i) {
        apply_window_level_8bit(src.data(), dst.data(), pixel_count, params);
    }

    // Benchmark
    benchmark_stats stats;
    high_resolution_timer timer;

    for (size_t i = 0; i < iterations; ++i) {
        timer.start();
        apply_window_level_8bit(src.data(), dst.data(), pixel_count, params);
        timer.stop();
        stats.record(static_cast<double>(timer.elapsed_ns().count()));
    }

    // Report results
    std::cout << "  Mean time: " << format_duration(stats.mean_ns()) << "\n";
    std::cout << "  Stddev:    " << format_duration(stats.stddev_ns()) << "\n";
    std::cout << "  Min/Max:   " << format_duration(stats.min_ns) << " / "
              << format_duration(stats.max_ns) << "\n";
    std::cout << "  Throughput: " << format_throughput(stats.throughput_bytes_per_sec(pixel_count))
              << "\n";
}

// =============================================================================
// Benchmark: 16-bit Window/Level (Unsigned)
// =============================================================================

void benchmark_window_level_16bit(size_t pixel_count, size_t iterations) {
    std::cout << "\n=== 16-bit Window/Level (Unsigned) ===\n";
    std::cout << "Pixel count: " << pixel_count << " (" << format_size(pixel_count * 2) << ")\n";

    // Generate test data (12-bit range)
    std::vector<uint16_t> src(pixel_count);
    std::vector<uint8_t> dst(pixel_count);

    std::mt19937 rng(42);
    std::uniform_int_distribution<uint16_t> dist(0, 4095);
    for (auto& pixel : src) {
        pixel = dist(rng);
    }

    // Typical CT window/level parameters
    window_level_params params(2048.0, 4096.0, false);

    // Warm-up
    for (size_t i = 0; i < kWarmupIterations; ++i) {
        apply_window_level_16bit(src.data(), dst.data(), pixel_count, params);
    }

    // Benchmark
    benchmark_stats stats;
    high_resolution_timer timer;

    for (size_t i = 0; i < iterations; ++i) {
        timer.start();
        apply_window_level_16bit(src.data(), dst.data(), pixel_count, params);
        timer.stop();
        stats.record(static_cast<double>(timer.elapsed_ns().count()));
    }

    // Report results
    std::cout << "  Mean time: " << format_duration(stats.mean_ns()) << "\n";
    std::cout << "  Stddev:    " << format_duration(stats.stddev_ns()) << "\n";
    std::cout << "  Min/Max:   " << format_duration(stats.min_ns) << " / "
              << format_duration(stats.max_ns) << "\n";
    std::cout << "  Throughput: " << format_throughput(stats.throughput_bytes_per_sec(pixel_count * 2))
              << "\n";
}

// =============================================================================
// Benchmark: 16-bit Window/Level (Signed - CT images)
// =============================================================================

void benchmark_window_level_16bit_signed(size_t pixel_count, size_t iterations) {
    std::cout << "\n=== 16-bit Window/Level (Signed - CT) ===\n";
    std::cout << "Pixel count: " << pixel_count << " (" << format_size(pixel_count * 2) << ")\n";

    // Generate test data (Hounsfield units range: -1024 to 3071)
    std::vector<int16_t> src(pixel_count);
    std::vector<uint8_t> dst(pixel_count);

    std::mt19937 rng(42);
    std::uniform_int_distribution<int16_t> dist(-1024, 3071);
    for (auto& pixel : src) {
        pixel = dist(rng);
    }

    // Typical CT soft tissue window
    window_level_params params(40.0, 400.0, false);  // Center: 40 HU, Width: 400 HU

    // Warm-up
    for (size_t i = 0; i < kWarmupIterations; ++i) {
        apply_window_level_16bit_signed(src.data(), dst.data(), pixel_count, params);
    }

    // Benchmark
    benchmark_stats stats;
    high_resolution_timer timer;

    for (size_t i = 0; i < iterations; ++i) {
        timer.start();
        apply_window_level_16bit_signed(src.data(), dst.data(), pixel_count, params);
        timer.stop();
        stats.record(static_cast<double>(timer.elapsed_ns().count()));
    }

    // Report results
    std::cout << "  Mean time: " << format_duration(stats.mean_ns()) << "\n";
    std::cout << "  Stddev:    " << format_duration(stats.stddev_ns()) << "\n";
    std::cout << "  Min/Max:   " << format_duration(stats.min_ns) << " / "
              << format_duration(stats.max_ns) << "\n";
    std::cout << "  Throughput: " << format_throughput(stats.throughput_bytes_per_sec(pixel_count * 2))
              << "\n";
}

// =============================================================================
// Benchmark: LUT vs Direct Calculation
// =============================================================================

void benchmark_lut_vs_direct_8bit(size_t pixel_count, size_t iterations) {
    std::cout << "\n=== LUT vs Direct: 8-bit Window/Level ===\n";
    std::cout << "Pixel count: " << pixel_count << "\n";

    auto src = generate_random_data(pixel_count);
    std::vector<uint8_t> dst(pixel_count);

    window_level_params params(128.0, 200.0, false);

    // Create LUT
    auto lut = window_level_lut::create_8bit(params);

    // Benchmark direct calculation
    benchmark_stats direct_stats;
    high_resolution_timer timer;

    for (size_t i = 0; i < iterations; ++i) {
        timer.start();
        apply_window_level_8bit(src.data(), dst.data(), pixel_count, params);
        timer.stop();
        direct_stats.record(static_cast<double>(timer.elapsed_ns().count()));
    }

    // Benchmark LUT
    benchmark_stats lut_stats;

    for (size_t i = 0; i < iterations; ++i) {
        timer.start();
        lut.apply_8bit(src.data(), dst.data(), pixel_count);
        timer.stop();
        lut_stats.record(static_cast<double>(timer.elapsed_ns().count()));
    }

    // Report comparison
    double speedup = calculate_speedup(direct_stats.mean_ns(), lut_stats.mean_ns());

    std::cout << "  Direct: " << format_duration(direct_stats.mean_ns())
              << " (" << format_throughput(direct_stats.throughput_bytes_per_sec(pixel_count)) << ")\n";
    std::cout << "  LUT:    " << format_duration(lut_stats.mean_ns())
              << " (" << format_throughput(lut_stats.throughput_bytes_per_sec(pixel_count)) << ")\n";
    std::cout << "  LUT Speedup: " << format_speedup(speedup) << "\n";
}

// =============================================================================
// Benchmark: Scalar vs SIMD Comparison
// =============================================================================

void benchmark_comparison_8bit(size_t pixel_count, size_t iterations) {
    std::cout << "\n=== Scalar vs SIMD: 8-bit Window/Level ===\n";
    std::cout << "Pixel count: " << pixel_count << "\n";

    auto src = generate_random_data(pixel_count);
    std::vector<uint8_t> dst(pixel_count);

    window_level_params params(128.0, 200.0, false);

    // Benchmark scalar
    benchmark_stats scalar_stats;
    high_resolution_timer timer;

    for (size_t i = 0; i < iterations; ++i) {
        timer.start();
        detail::apply_window_level_8bit_scalar(src.data(), dst.data(), pixel_count, params);
        timer.stop();
        scalar_stats.record(static_cast<double>(timer.elapsed_ns().count()));
    }

    // Benchmark SIMD
    benchmark_stats simd_stats;

    for (size_t i = 0; i < iterations; ++i) {
        timer.start();
        apply_window_level_8bit(src.data(), dst.data(), pixel_count, params);
        timer.stop();
        simd_stats.record(static_cast<double>(timer.elapsed_ns().count()));
    }

    // Report comparison
    double speedup = calculate_speedup(scalar_stats.mean_ns(), simd_stats.mean_ns());

    std::cout << "  Scalar:  " << format_duration(scalar_stats.mean_ns())
              << " (" << format_throughput(scalar_stats.throughput_bytes_per_sec(pixel_count)) << ")\n";
    std::cout << "  SIMD:    " << format_duration(simd_stats.mean_ns())
              << " (" << format_throughput(simd_stats.throughput_bytes_per_sec(pixel_count)) << ")\n";
    std::cout << "  Speedup: " << format_speedup(speedup) << "\n";
}

void benchmark_comparison_16bit_signed(size_t pixel_count, size_t iterations) {
    std::cout << "\n=== Scalar vs SIMD: 16-bit Signed Window/Level ===\n";
    std::cout << "Pixel count: " << pixel_count << "\n";

    std::vector<int16_t> src(pixel_count);
    std::vector<uint8_t> dst(pixel_count);

    std::mt19937 rng(42);
    std::uniform_int_distribution<int16_t> dist(-1024, 3071);
    for (auto& pixel : src) {
        pixel = dist(rng);
    }

    window_level_params params(40.0, 400.0, false);

    // Benchmark scalar
    benchmark_stats scalar_stats;
    high_resolution_timer timer;

    for (size_t i = 0; i < iterations; ++i) {
        timer.start();
        detail::apply_window_level_16bit_signed_scalar(src.data(), dst.data(), pixel_count, params);
        timer.stop();
        scalar_stats.record(static_cast<double>(timer.elapsed_ns().count()));
    }

    // Benchmark SIMD
    benchmark_stats simd_stats;

    for (size_t i = 0; i < iterations; ++i) {
        timer.start();
        apply_window_level_16bit_signed(src.data(), dst.data(), pixel_count, params);
        timer.stop();
        simd_stats.record(static_cast<double>(timer.elapsed_ns().count()));
    }

    // Report comparison
    double speedup = calculate_speedup(scalar_stats.mean_ns(), simd_stats.mean_ns());

    std::cout << "  Scalar:  " << format_duration(scalar_stats.mean_ns())
              << " (" << format_throughput(scalar_stats.throughput_bytes_per_sec(pixel_count * 2)) << ")\n";
    std::cout << "  SIMD:    " << format_duration(simd_stats.mean_ns())
              << " (" << format_throughput(simd_stats.throughput_bytes_per_sec(pixel_count * 2)) << ")\n";
    std::cout << "  Speedup: " << format_speedup(speedup) << "\n";
}

// =============================================================================
// Benchmark: Inversion Mode
// =============================================================================

void benchmark_inversion_mode(size_t pixel_count, size_t iterations) {
    std::cout << "\n=== Window/Level with Inversion (MONOCHROME1) ===\n";
    std::cout << "Pixel count: " << pixel_count << "\n";

    auto src = generate_random_data(pixel_count);
    std::vector<uint8_t> dst_normal(pixel_count);
    std::vector<uint8_t> dst_inverted(pixel_count);

    window_level_params params_normal(128.0, 200.0, false);
    window_level_params params_inverted(128.0, 200.0, true);

    // Benchmark normal
    benchmark_stats normal_stats;
    high_resolution_timer timer;

    for (size_t i = 0; i < iterations; ++i) {
        timer.start();
        apply_window_level_8bit(src.data(), dst_normal.data(), pixel_count, params_normal);
        timer.stop();
        normal_stats.record(static_cast<double>(timer.elapsed_ns().count()));
    }

    // Benchmark inverted
    benchmark_stats inverted_stats;

    for (size_t i = 0; i < iterations; ++i) {
        timer.start();
        apply_window_level_8bit(src.data(), dst_inverted.data(), pixel_count, params_inverted);
        timer.stop();
        inverted_stats.record(static_cast<double>(timer.elapsed_ns().count()));
    }

    // Report comparison
    std::cout << "  Normal:   " << format_duration(normal_stats.mean_ns())
              << " (" << format_throughput(normal_stats.throughput_bytes_per_sec(pixel_count)) << ")\n";
    std::cout << "  Inverted: " << format_duration(inverted_stats.mean_ns())
              << " (" << format_throughput(inverted_stats.throughput_bytes_per_sec(pixel_count)) << ")\n";
}

}  // namespace

int main() {
    std::cout << "======================================\n";
    std::cout << "  Window/Level Benchmark\n";
    std::cout << "======================================\n";
    std::cout << get_simd_features_string() << "\n";

    constexpr size_t iterations = kBenchmarkIterations;

    // Test various image sizes
    const std::vector<size_t> image_sizes = {
        256 * 256,        // 256x256 (64 KB)
        512 * 512,        // 512x512 (256 KB)
        1024 * 1024,      // 1024x1024 (1 MB)
        2048 * 2048,      // 2048x2048 (4 MB)
    };

    for (size_t pixel_count : image_sizes) {
        std::cout << "\n========================================\n";
        std::cout << "Image size: " << static_cast<size_t>(std::sqrt(pixel_count)) << "x"
                  << static_cast<size_t>(std::sqrt(pixel_count)) << "\n";
        std::cout << "========================================\n";

        benchmark_window_level_8bit(pixel_count, iterations);
        benchmark_window_level_16bit(pixel_count, iterations);
        benchmark_window_level_16bit_signed(pixel_count, iterations);
    }

    // Comparison benchmarks
    std::cout << "\n========================================\n";
    std::cout << "Optimization Comparisons (1024x1024)\n";
    std::cout << "========================================\n";

    benchmark_comparison_8bit(1024 * 1024, iterations);
    benchmark_comparison_16bit_signed(1024 * 1024, iterations);
    benchmark_lut_vs_direct_8bit(1024 * 1024, iterations);
    benchmark_inversion_mode(1024 * 1024, iterations);

    return 0;
}
