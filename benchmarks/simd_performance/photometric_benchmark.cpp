/**
 * @file photometric_benchmark.cpp
 * @brief Performance benchmarks for SIMD photometric conversions
 *
 * Measures the performance of photometric interpretation conversions:
 * - MONOCHROME1 <-> MONOCHROME2 inversion
 * - RGB <-> YCbCr color space conversion
 *
 * @see Issue #313 - Apply SIMD Optimization (Image Processing)
 */

#include "simd_benchmark_common.hpp"
#include "pacs/encoding/simd/simd_photometric.hpp"

#include <iostream>
#include <vector>

using namespace pacs::benchmark::simd;
using namespace pacs::encoding::simd;

namespace {

// =============================================================================
// Benchmark: 8-bit Monochrome Inversion
// =============================================================================

void benchmark_invert_monochrome_8bit(size_t pixel_count, size_t iterations) {
    std::cout << "\n=== 8-bit Monochrome Inversion ===\n";
    std::cout << "Pixel count: " << pixel_count << " (" << format_size(pixel_count) << ")\n";

    // Generate test data
    auto src = generate_random_data(pixel_count);
    std::vector<uint8_t> dst(pixel_count);

    // Warm-up
    for (size_t i = 0; i < kWarmupIterations; ++i) {
        invert_monochrome_8bit(src.data(), dst.data(), pixel_count);
    }

    // Benchmark
    benchmark_stats stats;
    high_resolution_timer timer;

    for (size_t i = 0; i < iterations; ++i) {
        timer.start();
        invert_monochrome_8bit(src.data(), dst.data(), pixel_count);
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
// Benchmark: 16-bit Monochrome Inversion
// =============================================================================

void benchmark_invert_monochrome_16bit(size_t pixel_count, size_t iterations) {
    std::cout << "\n=== 16-bit Monochrome Inversion ===\n";
    std::cout << "Pixel count: " << pixel_count << " (" << format_size(pixel_count * 2) << ")\n";

    // Generate test data
    std::vector<uint16_t> src(pixel_count);
    std::vector<uint16_t> dst(pixel_count);

    std::mt19937 rng(42);
    std::uniform_int_distribution<uint16_t> dist(0, 4095);  // 12-bit range
    for (auto& pixel : src) {
        pixel = dist(rng);
    }

    const uint16_t max_value = 4095;  // 12-bit

    // Warm-up
    for (size_t i = 0; i < kWarmupIterations; ++i) {
        invert_monochrome_16bit(src.data(), dst.data(), pixel_count, max_value);
    }

    // Benchmark
    benchmark_stats stats;
    high_resolution_timer timer;

    for (size_t i = 0; i < iterations; ++i) {
        timer.start();
        invert_monochrome_16bit(src.data(), dst.data(), pixel_count, max_value);
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
// Benchmark: RGB to YCbCr Conversion
// =============================================================================

void benchmark_rgb_to_ycbcr(size_t pixel_count, size_t iterations) {
    std::cout << "\n=== RGB to YCbCr Conversion ===\n";
    std::cout << "Pixel count: " << pixel_count << " (" << format_size(pixel_count * 3) << ")\n";

    // Generate test data
    auto src = generate_rgb_data(pixel_count);
    std::vector<uint8_t> dst(pixel_count * 3);

    // Warm-up
    for (size_t i = 0; i < kWarmupIterations; ++i) {
        rgb_to_ycbcr_8bit(src.data(), dst.data(), pixel_count);
    }

    // Benchmark
    benchmark_stats stats;
    high_resolution_timer timer;

    for (size_t i = 0; i < iterations; ++i) {
        timer.start();
        rgb_to_ycbcr_8bit(src.data(), dst.data(), pixel_count);
        timer.stop();
        stats.record(static_cast<double>(timer.elapsed_ns().count()));
    }

    // Report results
    std::cout << "  Mean time: " << format_duration(stats.mean_ns()) << "\n";
    std::cout << "  Stddev:    " << format_duration(stats.stddev_ns()) << "\n";
    std::cout << "  Min/Max:   " << format_duration(stats.min_ns) << " / "
              << format_duration(stats.max_ns) << "\n";
    std::cout << "  Throughput: " << format_throughput(stats.throughput_bytes_per_sec(pixel_count * 3))
              << "\n";
}

// =============================================================================
// Benchmark: YCbCr to RGB Conversion
// =============================================================================

void benchmark_ycbcr_to_rgb(size_t pixel_count, size_t iterations) {
    std::cout << "\n=== YCbCr to RGB Conversion ===\n";
    std::cout << "Pixel count: " << pixel_count << " (" << format_size(pixel_count * 3) << ")\n";

    // Generate test data (use RGB and convert to YCbCr first)
    auto rgb_src = generate_rgb_data(pixel_count);
    std::vector<uint8_t> ycbcr(pixel_count * 3);
    std::vector<uint8_t> dst(pixel_count * 3);

    rgb_to_ycbcr_8bit(rgb_src.data(), ycbcr.data(), pixel_count);

    // Warm-up
    for (size_t i = 0; i < kWarmupIterations; ++i) {
        ycbcr_to_rgb_8bit(ycbcr.data(), dst.data(), pixel_count);
    }

    // Benchmark
    benchmark_stats stats;
    high_resolution_timer timer;

    for (size_t i = 0; i < iterations; ++i) {
        timer.start();
        ycbcr_to_rgb_8bit(ycbcr.data(), dst.data(), pixel_count);
        timer.stop();
        stats.record(static_cast<double>(timer.elapsed_ns().count()));
    }

    // Report results
    std::cout << "  Mean time: " << format_duration(stats.mean_ns()) << "\n";
    std::cout << "  Stddev:    " << format_duration(stats.stddev_ns()) << "\n";
    std::cout << "  Min/Max:   " << format_duration(stats.min_ns) << " / "
              << format_duration(stats.max_ns) << "\n";
    std::cout << "  Throughput: " << format_throughput(stats.throughput_bytes_per_sec(pixel_count * 3))
              << "\n";
}

// =============================================================================
// Benchmark: Scalar vs SIMD Comparison
// =============================================================================

void benchmark_comparison_8bit_inversion(size_t pixel_count, size_t iterations) {
    std::cout << "\n=== Scalar vs SIMD Comparison: 8-bit Inversion ===\n";
    std::cout << "Pixel count: " << pixel_count << "\n";

    auto src = generate_random_data(pixel_count);
    std::vector<uint8_t> dst(pixel_count);

    // Benchmark scalar
    benchmark_stats scalar_stats;
    high_resolution_timer timer;

    for (size_t i = 0; i < iterations; ++i) {
        timer.start();
        detail::invert_monochrome_8bit_scalar(src.data(), dst.data(), pixel_count);
        timer.stop();
        scalar_stats.record(static_cast<double>(timer.elapsed_ns().count()));
    }

    // Benchmark SIMD (via dispatch)
    benchmark_stats simd_stats;

    for (size_t i = 0; i < iterations; ++i) {
        timer.start();
        invert_monochrome_8bit(src.data(), dst.data(), pixel_count);
        timer.stop();
        simd_stats.record(static_cast<double>(timer.elapsed_ns().count()));
    }

    // Report comparison
    double speedup = calculate_speedup(scalar_stats.mean_ns(), simd_stats.mean_ns());

    std::cout << "  Scalar:   " << format_duration(scalar_stats.mean_ns())
              << " (" << format_throughput(scalar_stats.throughput_bytes_per_sec(pixel_count)) << ")\n";
    std::cout << "  SIMD:     " << format_duration(simd_stats.mean_ns())
              << " (" << format_throughput(simd_stats.throughput_bytes_per_sec(pixel_count)) << ")\n";
    std::cout << "  Speedup:  " << format_speedup(speedup) << "\n";
}

void benchmark_comparison_rgb_ycbcr(size_t pixel_count, size_t iterations) {
    std::cout << "\n=== Scalar vs SIMD Comparison: RGB to YCbCr ===\n";
    std::cout << "Pixel count: " << pixel_count << "\n";

    auto src = generate_rgb_data(pixel_count);
    std::vector<uint8_t> dst(pixel_count * 3);

    // Benchmark scalar
    benchmark_stats scalar_stats;
    high_resolution_timer timer;

    for (size_t i = 0; i < iterations; ++i) {
        timer.start();
        detail::rgb_to_ycbcr_8bit_scalar(src.data(), dst.data(), pixel_count);
        timer.stop();
        scalar_stats.record(static_cast<double>(timer.elapsed_ns().count()));
    }

    // Benchmark SIMD
    benchmark_stats simd_stats;

    for (size_t i = 0; i < iterations; ++i) {
        timer.start();
        rgb_to_ycbcr_8bit(src.data(), dst.data(), pixel_count);
        timer.stop();
        simd_stats.record(static_cast<double>(timer.elapsed_ns().count()));
    }

    // Report comparison
    double speedup = calculate_speedup(scalar_stats.mean_ns(), simd_stats.mean_ns());

    std::cout << "  Scalar:   " << format_duration(scalar_stats.mean_ns())
              << " (" << format_throughput(scalar_stats.throughput_bytes_per_sec(pixel_count * 3)) << ")\n";
    std::cout << "  SIMD:     " << format_duration(simd_stats.mean_ns())
              << " (" << format_throughput(simd_stats.throughput_bytes_per_sec(pixel_count * 3)) << ")\n";
    std::cout << "  Speedup:  " << format_speedup(speedup) << "\n";
}

}  // namespace

int main() {
    std::cout << "======================================\n";
    std::cout << "  Photometric Conversion Benchmark\n";
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

        benchmark_invert_monochrome_8bit(pixel_count, iterations);
        benchmark_invert_monochrome_16bit(pixel_count, iterations);
        benchmark_rgb_to_ycbcr(pixel_count, iterations);
        benchmark_ycbcr_to_rgb(pixel_count, iterations);
    }

    // Comparison benchmarks
    std::cout << "\n========================================\n";
    std::cout << "Scalar vs SIMD Comparison (1024x1024)\n";
    std::cout << "========================================\n";

    benchmark_comparison_8bit_inversion(1024 * 1024, iterations);
    benchmark_comparison_rgb_ycbcr(1024 * 1024, iterations);

    return 0;
}
