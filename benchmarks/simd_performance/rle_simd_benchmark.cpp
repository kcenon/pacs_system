/**
 * @file rle_simd_benchmark.cpp
 * @brief Benchmarks for SIMD operations used in RLE codec
 *
 * Measures performance of RGB planar conversion and 16-bit plane
 * splitting/merging operations used in DICOM RLE compression.
 *
 * @see Issue #332 - Add SIMD optimization benchmarks
 * @see Issue #313 - Apply SIMD Optimization (Image Processing)
 */

#include "simd_benchmark_common.hpp"

#include "pacs/encoding/simd/simd_config.hpp"
#include "pacs/encoding/simd/simd_rle.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

#include <iostream>
#include <vector>

namespace {

using namespace pacs::benchmark::simd;
using namespace pacs::encoding::simd;

// =============================================================================
// Image size constants (in pixels)
// =============================================================================

// Small: 64x64 (typical thumbnail)
constexpr size_t kSmallPixels = 64 * 64;

// Medium: 512x512 (typical CT slice)
constexpr size_t kMediumPixels = 512 * 512;

// Large: 2048x2048 (high-resolution medical image)
constexpr size_t kLargePixels = 2048 * 2048;

// =============================================================================
// Scalar implementations for comparison
// =============================================================================

void interleaved_to_planar_rgb8_scalar(const uint8_t* src, uint8_t* r,
                                        uint8_t* g, uint8_t* b,
                                        size_t pixel_count) noexcept {
    for (size_t i = 0; i < pixel_count; ++i) {
        r[i] = src[i * 3];
        g[i] = src[i * 3 + 1];
        b[i] = src[i * 3 + 2];
    }
}

void planar_to_interleaved_rgb8_scalar(const uint8_t* r, const uint8_t* g,
                                        const uint8_t* b, uint8_t* dst,
                                        size_t pixel_count) noexcept {
    for (size_t i = 0; i < pixel_count; ++i) {
        dst[i * 3] = r[i];
        dst[i * 3 + 1] = g[i];
        dst[i * 3 + 2] = b[i];
    }
}

void split_16bit_to_planes_scalar(const uint8_t* src, uint8_t* high,
                                   uint8_t* low, size_t pixel_count) noexcept {
    for (size_t i = 0; i < pixel_count; ++i) {
        low[i] = src[i * 2];
        high[i] = src[i * 2 + 1];
    }
}

void merge_planes_to_16bit_scalar(const uint8_t* high, const uint8_t* low,
                                   uint8_t* dst, size_t pixel_count) noexcept {
    for (size_t i = 0; i < pixel_count; ++i) {
        dst[i * 2] = low[i];
        dst[i * 2 + 1] = high[i];
    }
}

// =============================================================================
// Benchmark helper structures
// =============================================================================

struct rgb_benchmark_result {
    double scalar_ns;
    double simd_ns;
    double speedup;
    size_t pixel_count;
};

struct plane_benchmark_result {
    double scalar_ns;
    double simd_ns;
    double speedup;
    size_t pixel_count;
};

// =============================================================================
// RGB conversion benchmarks
// =============================================================================

rgb_benchmark_result run_interleaved_to_planar_benchmark(
    size_t pixel_count,
    size_t warmup_iterations = kWarmupIterations,
    size_t benchmark_iterations = kBenchmarkIterations) {

    auto rgb_data = generate_rgb_data(pixel_count);
    std::vector<uint8_t> r_scalar(pixel_count), g_scalar(pixel_count), b_scalar(pixel_count);
    std::vector<uint8_t> r_simd(pixel_count), g_simd(pixel_count), b_simd(pixel_count);

    high_resolution_timer timer;
    benchmark_stats scalar_stats;
    benchmark_stats simd_stats;

    // Warm-up scalar
    for (size_t i = 0; i < warmup_iterations; ++i) {
        interleaved_to_planar_rgb8_scalar(rgb_data.data(),
                                          r_scalar.data(), g_scalar.data(), b_scalar.data(),
                                          pixel_count);
    }

    // Benchmark scalar
    for (size_t i = 0; i < benchmark_iterations; ++i) {
        timer.start();
        interleaved_to_planar_rgb8_scalar(rgb_data.data(),
                                          r_scalar.data(), g_scalar.data(), b_scalar.data(),
                                          pixel_count);
        timer.stop();
        scalar_stats.record(static_cast<double>(timer.elapsed_ns().count()));
    }

    // Warm-up SIMD
    for (size_t i = 0; i < warmup_iterations; ++i) {
        interleaved_to_planar_rgb8(rgb_data.data(),
                                   r_simd.data(), g_simd.data(), b_simd.data(),
                                   pixel_count);
    }

    // Benchmark SIMD
    for (size_t i = 0; i < benchmark_iterations; ++i) {
        timer.start();
        interleaved_to_planar_rgb8(rgb_data.data(),
                                   r_simd.data(), g_simd.data(), b_simd.data(),
                                   pixel_count);
        timer.stop();
        simd_stats.record(static_cast<double>(timer.elapsed_ns().count()));
    }

    // Verify results match
    REQUIRE(r_scalar == r_simd);
    REQUIRE(g_scalar == g_simd);
    REQUIRE(b_scalar == b_simd);

    return {
        scalar_stats.mean_ns(),
        simd_stats.mean_ns(),
        calculate_speedup(scalar_stats.mean_ns(), simd_stats.mean_ns()),
        pixel_count
    };
}

rgb_benchmark_result run_planar_to_interleaved_benchmark(
    size_t pixel_count,
    size_t warmup_iterations = kWarmupIterations,
    size_t benchmark_iterations = kBenchmarkIterations) {

    // Generate separate planes
    auto r_data = generate_random_data(pixel_count);
    auto g_data = generate_random_data(pixel_count, 43);
    auto b_data = generate_random_data(pixel_count, 44);

    std::vector<uint8_t> result_scalar(pixel_count * 3);
    std::vector<uint8_t> result_simd(pixel_count * 3);

    high_resolution_timer timer;
    benchmark_stats scalar_stats;
    benchmark_stats simd_stats;

    // Warm-up scalar
    for (size_t i = 0; i < warmup_iterations; ++i) {
        planar_to_interleaved_rgb8_scalar(r_data.data(), g_data.data(), b_data.data(),
                                          result_scalar.data(), pixel_count);
    }

    // Benchmark scalar
    for (size_t i = 0; i < benchmark_iterations; ++i) {
        timer.start();
        planar_to_interleaved_rgb8_scalar(r_data.data(), g_data.data(), b_data.data(),
                                          result_scalar.data(), pixel_count);
        timer.stop();
        scalar_stats.record(static_cast<double>(timer.elapsed_ns().count()));
    }

    // Warm-up SIMD
    for (size_t i = 0; i < warmup_iterations; ++i) {
        planar_to_interleaved_rgb8(r_data.data(), g_data.data(), b_data.data(),
                                   result_simd.data(), pixel_count);
    }

    // Benchmark SIMD
    for (size_t i = 0; i < benchmark_iterations; ++i) {
        timer.start();
        planar_to_interleaved_rgb8(r_data.data(), g_data.data(), b_data.data(),
                                   result_simd.data(), pixel_count);
        timer.stop();
        simd_stats.record(static_cast<double>(timer.elapsed_ns().count()));
    }

    // Verify results match
    REQUIRE(result_scalar == result_simd);

    return {
        scalar_stats.mean_ns(),
        simd_stats.mean_ns(),
        calculate_speedup(scalar_stats.mean_ns(), simd_stats.mean_ns()),
        pixel_count
    };
}

// =============================================================================
// 16-bit plane benchmarks
// =============================================================================

plane_benchmark_result run_split_16bit_benchmark(
    size_t pixel_count,
    size_t warmup_iterations = kWarmupIterations,
    size_t benchmark_iterations = kBenchmarkIterations) {

    auto data_16bit = generate_16bit_data(pixel_count);
    std::vector<uint8_t> high_scalar(pixel_count), low_scalar(pixel_count);
    std::vector<uint8_t> high_simd(pixel_count), low_simd(pixel_count);

    high_resolution_timer timer;
    benchmark_stats scalar_stats;
    benchmark_stats simd_stats;

    // Warm-up scalar
    for (size_t i = 0; i < warmup_iterations; ++i) {
        split_16bit_to_planes_scalar(data_16bit.data(),
                                     high_scalar.data(), low_scalar.data(),
                                     pixel_count);
    }

    // Benchmark scalar
    for (size_t i = 0; i < benchmark_iterations; ++i) {
        timer.start();
        split_16bit_to_planes_scalar(data_16bit.data(),
                                     high_scalar.data(), low_scalar.data(),
                                     pixel_count);
        timer.stop();
        scalar_stats.record(static_cast<double>(timer.elapsed_ns().count()));
    }

    // Warm-up SIMD
    for (size_t i = 0; i < warmup_iterations; ++i) {
        split_16bit_to_planes(data_16bit.data(),
                              high_simd.data(), low_simd.data(),
                              pixel_count);
    }

    // Benchmark SIMD
    for (size_t i = 0; i < benchmark_iterations; ++i) {
        timer.start();
        split_16bit_to_planes(data_16bit.data(),
                              high_simd.data(), low_simd.data(),
                              pixel_count);
        timer.stop();
        simd_stats.record(static_cast<double>(timer.elapsed_ns().count()));
    }

    // Verify results match
    REQUIRE(high_scalar == high_simd);
    REQUIRE(low_scalar == low_simd);

    return {
        scalar_stats.mean_ns(),
        simd_stats.mean_ns(),
        calculate_speedup(scalar_stats.mean_ns(), simd_stats.mean_ns()),
        pixel_count
    };
}

plane_benchmark_result run_merge_16bit_benchmark(
    size_t pixel_count,
    size_t warmup_iterations = kWarmupIterations,
    size_t benchmark_iterations = kBenchmarkIterations) {

    auto high_data = generate_random_data(pixel_count);
    auto low_data = generate_random_data(pixel_count, 43);

    std::vector<uint8_t> result_scalar(pixel_count * 2);
    std::vector<uint8_t> result_simd(pixel_count * 2);

    high_resolution_timer timer;
    benchmark_stats scalar_stats;
    benchmark_stats simd_stats;

    // Warm-up scalar
    for (size_t i = 0; i < warmup_iterations; ++i) {
        merge_planes_to_16bit_scalar(high_data.data(), low_data.data(),
                                     result_scalar.data(), pixel_count);
    }

    // Benchmark scalar
    for (size_t i = 0; i < benchmark_iterations; ++i) {
        timer.start();
        merge_planes_to_16bit_scalar(high_data.data(), low_data.data(),
                                     result_scalar.data(), pixel_count);
        timer.stop();
        scalar_stats.record(static_cast<double>(timer.elapsed_ns().count()));
    }

    // Warm-up SIMD
    for (size_t i = 0; i < warmup_iterations; ++i) {
        merge_planes_to_16bit(high_data.data(), low_data.data(),
                              result_simd.data(), pixel_count);
    }

    // Benchmark SIMD
    for (size_t i = 0; i < benchmark_iterations; ++i) {
        timer.start();
        merge_planes_to_16bit(high_data.data(), low_data.data(),
                              result_simd.data(), pixel_count);
        timer.stop();
        simd_stats.record(static_cast<double>(timer.elapsed_ns().count()));
    }

    // Verify results match
    REQUIRE(result_scalar == result_simd);

    return {
        scalar_stats.mean_ns(),
        simd_stats.mean_ns(),
        calculate_speedup(scalar_stats.mean_ns(), simd_stats.mean_ns()),
        pixel_count
    };
}

void print_rgb_result(const std::string& name, const rgb_benchmark_result& result) {
    size_t bytes = result.pixel_count * 3;
    std::cout << "\n=== " << name << " (" << result.pixel_count << " pixels, "
              << format_size(bytes) << ") ===" << std::endl;
    std::cout << "  Scalar:  " << format_duration(result.scalar_ns)
              << " (" << format_throughput(bytes / (result.scalar_ns / 1e9)) << ")"
              << std::endl;
    std::cout << "  SIMD:    " << format_duration(result.simd_ns)
              << " (" << format_throughput(bytes / (result.simd_ns / 1e9)) << ")"
              << std::endl;
    std::cout << "  Speedup: " << format_speedup(result.speedup) << std::endl;
}

void print_plane_result(const std::string& name, const plane_benchmark_result& result) {
    size_t bytes = result.pixel_count * 2;
    std::cout << "\n=== " << name << " (" << result.pixel_count << " pixels, "
              << format_size(bytes) << ") ===" << std::endl;
    std::cout << "  Scalar:  " << format_duration(result.scalar_ns)
              << " (" << format_throughput(bytes / (result.scalar_ns / 1e9)) << ")"
              << std::endl;
    std::cout << "  SIMD:    " << format_duration(result.simd_ns)
              << " (" << format_throughput(bytes / (result.simd_ns / 1e9)) << ")"
              << std::endl;
    std::cout << "  Speedup: " << format_speedup(result.speedup) << std::endl;
}

}  // namespace

// =============================================================================
// Test Cases: RGB Interleaved to Planar
// =============================================================================

TEST_CASE("RGB interleaved to planar benchmarks", "[benchmark][simd][rle][rgb][i2p]") {
    std::cout << "\n" << get_simd_features_string() << std::endl;

    SECTION("Small image (64x64 = 4096 pixels)") {
        auto result = run_interleaved_to_planar_benchmark(kSmallPixels);
        print_rgb_result("Interleaved->Planar RGB", result);
        CHECK(result.speedup > 0.0);
    }

    SECTION("Medium image (512x512 = 262144 pixels)") {
        auto result = run_interleaved_to_planar_benchmark(kMediumPixels);
        print_rgb_result("Interleaved->Planar RGB", result);
        CHECK(result.speedup > 0.0);
    }

    SECTION("Large image (2048x2048 = 4194304 pixels)") {
        auto result = run_interleaved_to_planar_benchmark(kLargePixels);
        print_rgb_result("Interleaved->Planar RGB", result);
        CHECK(result.speedup > 0.0);
    }
}

TEST_CASE("RGB interleaved to planar - Catch2 BENCHMARK", "[benchmark][simd][rle][rgb][i2p][catch2]") {
    auto small_rgb = generate_rgb_data(kSmallPixels);
    auto medium_rgb = generate_rgb_data(kMediumPixels);
    auto large_rgb = generate_rgb_data(kLargePixels);

    std::vector<uint8_t> r_small(kSmallPixels), g_small(kSmallPixels), b_small(kSmallPixels);
    std::vector<uint8_t> r_medium(kMediumPixels), g_medium(kMediumPixels), b_medium(kMediumPixels);
    std::vector<uint8_t> r_large(kLargePixels), g_large(kLargePixels), b_large(kLargePixels);

    BENCHMARK("scalar_i2p_64x64") {
        interleaved_to_planar_rgb8_scalar(small_rgb.data(),
                                          r_small.data(), g_small.data(), b_small.data(),
                                          kSmallPixels);
        return r_small[0];
    };

    BENCHMARK("simd_i2p_64x64") {
        interleaved_to_planar_rgb8(small_rgb.data(),
                                   r_small.data(), g_small.data(), b_small.data(),
                                   kSmallPixels);
        return r_small[0];
    };

    BENCHMARK("scalar_i2p_512x512") {
        interleaved_to_planar_rgb8_scalar(medium_rgb.data(),
                                          r_medium.data(), g_medium.data(), b_medium.data(),
                                          kMediumPixels);
        return r_medium[0];
    };

    BENCHMARK("simd_i2p_512x512") {
        interleaved_to_planar_rgb8(medium_rgb.data(),
                                   r_medium.data(), g_medium.data(), b_medium.data(),
                                   kMediumPixels);
        return r_medium[0];
    };

    BENCHMARK("scalar_i2p_2048x2048") {
        interleaved_to_planar_rgb8_scalar(large_rgb.data(),
                                          r_large.data(), g_large.data(), b_large.data(),
                                          kLargePixels);
        return r_large[0];
    };

    BENCHMARK("simd_i2p_2048x2048") {
        interleaved_to_planar_rgb8(large_rgb.data(),
                                   r_large.data(), g_large.data(), b_large.data(),
                                   kLargePixels);
        return r_large[0];
    };
}

// =============================================================================
// Test Cases: RGB Planar to Interleaved
// =============================================================================

TEST_CASE("RGB planar to interleaved benchmarks", "[benchmark][simd][rle][rgb][p2i]") {
    std::cout << "\n" << get_simd_features_string() << std::endl;

    SECTION("Small image (64x64 = 4096 pixels)") {
        auto result = run_planar_to_interleaved_benchmark(kSmallPixels);
        print_rgb_result("Planar->Interleaved RGB", result);
        CHECK(result.speedup > 0.0);
    }

    SECTION("Medium image (512x512 = 262144 pixels)") {
        auto result = run_planar_to_interleaved_benchmark(kMediumPixels);
        print_rgb_result("Planar->Interleaved RGB", result);
        CHECK(result.speedup > 0.0);
    }

    SECTION("Large image (2048x2048 = 4194304 pixels)") {
        auto result = run_planar_to_interleaved_benchmark(kLargePixels);
        print_rgb_result("Planar->Interleaved RGB", result);
        CHECK(result.speedup > 0.0);
    }
}

TEST_CASE("RGB planar to interleaved - Catch2 BENCHMARK", "[benchmark][simd][rle][rgb][p2i][catch2]") {
    auto r_small = generate_random_data(kSmallPixels);
    auto g_small = generate_random_data(kSmallPixels, 43);
    auto b_small = generate_random_data(kSmallPixels, 44);

    auto r_medium = generate_random_data(kMediumPixels);
    auto g_medium = generate_random_data(kMediumPixels, 43);
    auto b_medium = generate_random_data(kMediumPixels, 44);

    auto r_large = generate_random_data(kLargePixels);
    auto g_large = generate_random_data(kLargePixels, 43);
    auto b_large = generate_random_data(kLargePixels, 44);

    std::vector<uint8_t> result_small(kSmallPixels * 3);
    std::vector<uint8_t> result_medium(kMediumPixels * 3);
    std::vector<uint8_t> result_large(kLargePixels * 3);

    BENCHMARK("scalar_p2i_64x64") {
        planar_to_interleaved_rgb8_scalar(r_small.data(), g_small.data(), b_small.data(),
                                          result_small.data(), kSmallPixels);
        return result_small[0];
    };

    BENCHMARK("simd_p2i_64x64") {
        planar_to_interleaved_rgb8(r_small.data(), g_small.data(), b_small.data(),
                                   result_small.data(), kSmallPixels);
        return result_small[0];
    };

    BENCHMARK("scalar_p2i_512x512") {
        planar_to_interleaved_rgb8_scalar(r_medium.data(), g_medium.data(), b_medium.data(),
                                          result_medium.data(), kMediumPixels);
        return result_medium[0];
    };

    BENCHMARK("simd_p2i_512x512") {
        planar_to_interleaved_rgb8(r_medium.data(), g_medium.data(), b_medium.data(),
                                   result_medium.data(), kMediumPixels);
        return result_medium[0];
    };

    BENCHMARK("scalar_p2i_2048x2048") {
        planar_to_interleaved_rgb8_scalar(r_large.data(), g_large.data(), b_large.data(),
                                          result_large.data(), kLargePixels);
        return result_large[0];
    };

    BENCHMARK("simd_p2i_2048x2048") {
        planar_to_interleaved_rgb8(r_large.data(), g_large.data(), b_large.data(),
                                   result_large.data(), kLargePixels);
        return result_large[0];
    };
}

// =============================================================================
// Test Cases: 16-bit Plane Split
// =============================================================================

TEST_CASE("16-bit plane split benchmarks", "[benchmark][simd][rle][16bit][split]") {
    std::cout << "\n" << get_simd_features_string() << std::endl;

    SECTION("Small image (64x64 = 4096 pixels)") {
        auto result = run_split_16bit_benchmark(kSmallPixels);
        print_plane_result("16-bit plane split", result);
        CHECK(result.speedup > 0.0);
    }

    SECTION("Medium image (512x512 = 262144 pixels)") {
        auto result = run_split_16bit_benchmark(kMediumPixels);
        print_plane_result("16-bit plane split", result);
        CHECK(result.speedup > 0.0);
    }

    SECTION("Large image (2048x2048 = 4194304 pixels)") {
        auto result = run_split_16bit_benchmark(kLargePixels);
        print_plane_result("16-bit plane split", result);
        CHECK(result.speedup > 0.0);
    }
}

TEST_CASE("16-bit plane split - Catch2 BENCHMARK", "[benchmark][simd][rle][16bit][split][catch2]") {
    auto small_16bit = generate_16bit_data(kSmallPixels);
    auto medium_16bit = generate_16bit_data(kMediumPixels);
    auto large_16bit = generate_16bit_data(kLargePixels);

    std::vector<uint8_t> high_small(kSmallPixels), low_small(kSmallPixels);
    std::vector<uint8_t> high_medium(kMediumPixels), low_medium(kMediumPixels);
    std::vector<uint8_t> high_large(kLargePixels), low_large(kLargePixels);

    BENCHMARK("scalar_split_64x64") {
        split_16bit_to_planes_scalar(small_16bit.data(),
                                     high_small.data(), low_small.data(),
                                     kSmallPixels);
        return high_small[0];
    };

    BENCHMARK("simd_split_64x64") {
        split_16bit_to_planes(small_16bit.data(),
                              high_small.data(), low_small.data(),
                              kSmallPixels);
        return high_small[0];
    };

    BENCHMARK("scalar_split_512x512") {
        split_16bit_to_planes_scalar(medium_16bit.data(),
                                     high_medium.data(), low_medium.data(),
                                     kMediumPixels);
        return high_medium[0];
    };

    BENCHMARK("simd_split_512x512") {
        split_16bit_to_planes(medium_16bit.data(),
                              high_medium.data(), low_medium.data(),
                              kMediumPixels);
        return high_medium[0];
    };

    BENCHMARK("scalar_split_2048x2048") {
        split_16bit_to_planes_scalar(large_16bit.data(),
                                     high_large.data(), low_large.data(),
                                     kLargePixels);
        return high_large[0];
    };

    BENCHMARK("simd_split_2048x2048") {
        split_16bit_to_planes(large_16bit.data(),
                              high_large.data(), low_large.data(),
                              kLargePixels);
        return high_large[0];
    };
}

// =============================================================================
// Test Cases: 16-bit Plane Merge
// =============================================================================

TEST_CASE("16-bit plane merge benchmarks", "[benchmark][simd][rle][16bit][merge]") {
    std::cout << "\n" << get_simd_features_string() << std::endl;

    SECTION("Small image (64x64 = 4096 pixels)") {
        auto result = run_merge_16bit_benchmark(kSmallPixels);
        print_plane_result("16-bit plane merge", result);
        CHECK(result.speedup > 0.0);
    }

    SECTION("Medium image (512x512 = 262144 pixels)") {
        auto result = run_merge_16bit_benchmark(kMediumPixels);
        print_plane_result("16-bit plane merge", result);
        CHECK(result.speedup > 0.0);
    }

    SECTION("Large image (2048x2048 = 4194304 pixels)") {
        auto result = run_merge_16bit_benchmark(kLargePixels);
        print_plane_result("16-bit plane merge", result);
        CHECK(result.speedup > 0.0);
    }
}

TEST_CASE("16-bit plane merge - Catch2 BENCHMARK", "[benchmark][simd][rle][16bit][merge][catch2]") {
    auto high_small = generate_random_data(kSmallPixels);
    auto low_small = generate_random_data(kSmallPixels, 43);

    auto high_medium = generate_random_data(kMediumPixels);
    auto low_medium = generate_random_data(kMediumPixels, 43);

    auto high_large = generate_random_data(kLargePixels);
    auto low_large = generate_random_data(kLargePixels, 43);

    std::vector<uint8_t> result_small(kSmallPixels * 2);
    std::vector<uint8_t> result_medium(kMediumPixels * 2);
    std::vector<uint8_t> result_large(kLargePixels * 2);

    BENCHMARK("scalar_merge_64x64") {
        merge_planes_to_16bit_scalar(high_small.data(), low_small.data(),
                                     result_small.data(), kSmallPixels);
        return result_small[0];
    };

    BENCHMARK("simd_merge_64x64") {
        merge_planes_to_16bit(high_small.data(), low_small.data(),
                              result_small.data(), kSmallPixels);
        return result_small[0];
    };

    BENCHMARK("scalar_merge_512x512") {
        merge_planes_to_16bit_scalar(high_medium.data(), low_medium.data(),
                                     result_medium.data(), kMediumPixels);
        return result_medium[0];
    };

    BENCHMARK("simd_merge_512x512") {
        merge_planes_to_16bit(high_medium.data(), low_medium.data(),
                              result_medium.data(), kMediumPixels);
        return result_medium[0];
    };

    BENCHMARK("scalar_merge_2048x2048") {
        merge_planes_to_16bit_scalar(high_large.data(), low_large.data(),
                                     result_large.data(), kLargePixels);
        return result_large[0];
    };

    BENCHMARK("simd_merge_2048x2048") {
        merge_planes_to_16bit(high_large.data(), low_large.data(),
                              result_large.data(), kLargePixels);
        return result_large[0];
    };
}

// =============================================================================
// Summary test case
// =============================================================================

TEST_CASE("RLE SIMD benchmark summary", "[benchmark][simd][rle][summary]") {
    std::cout << "\n========================================" << std::endl;
    std::cout << "RLE SIMD BENCHMARK SUMMARY" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << get_simd_features_string() << std::endl;

    // Run all benchmarks
    auto i2p_small = run_interleaved_to_planar_benchmark(kSmallPixels);
    auto i2p_medium = run_interleaved_to_planar_benchmark(kMediumPixels);
    auto i2p_large = run_interleaved_to_planar_benchmark(kLargePixels);

    auto p2i_small = run_planar_to_interleaved_benchmark(kSmallPixels);
    auto p2i_medium = run_planar_to_interleaved_benchmark(kMediumPixels);
    auto p2i_large = run_planar_to_interleaved_benchmark(kLargePixels);

    auto split_small = run_split_16bit_benchmark(kSmallPixels);
    auto split_medium = run_split_16bit_benchmark(kMediumPixels);
    auto split_large = run_split_16bit_benchmark(kLargePixels);

    auto merge_small = run_merge_16bit_benchmark(kSmallPixels);
    auto merge_medium = run_merge_16bit_benchmark(kMediumPixels);
    auto merge_large = run_merge_16bit_benchmark(kLargePixels);

    std::cout << "\n+------------------------+------------+----------+" << std::endl;
    std::cout << "| Operation              | Image Size | Speedup  |" << std::endl;
    std::cout << "+------------------------+------------+----------+" << std::endl;

    auto print_row = [](const std::string& op, const std::string& size, double speedup) {
        std::cout << "| " << std::left << std::setw(22) << op
                  << " | " << std::setw(10) << size
                  << " | " << std::setw(8) << format_speedup(speedup)
                  << " |" << std::endl;
    };

    print_row("RGB Interleaved->Planar", "64x64", i2p_small.speedup);
    print_row("RGB Interleaved->Planar", "512x512", i2p_medium.speedup);
    print_row("RGB Interleaved->Planar", "2048x2048", i2p_large.speedup);
    print_row("RGB Planar->Interleaved", "64x64", p2i_small.speedup);
    print_row("RGB Planar->Interleaved", "512x512", p2i_medium.speedup);
    print_row("RGB Planar->Interleaved", "2048x2048", p2i_large.speedup);
    print_row("16-bit Plane Split", "64x64", split_small.speedup);
    print_row("16-bit Plane Split", "512x512", split_medium.speedup);
    print_row("16-bit Plane Split", "2048x2048", split_large.speedup);
    print_row("16-bit Plane Merge", "64x64", merge_small.speedup);
    print_row("16-bit Plane Merge", "512x512", merge_medium.speedup);
    print_row("16-bit Plane Merge", "2048x2048", merge_large.speedup);

    std::cout << "+------------------------+------------+----------+" << std::endl;

    // Verify results are valid (speedup can be < 1.0 on platforms where
    // compiler auto-vectorization is already applied to scalar code)
    CHECK(i2p_small.speedup > 0.0);
    CHECK(i2p_medium.speedup > 0.0);
    CHECK(i2p_large.speedup > 0.0);
    CHECK(p2i_small.speedup > 0.0);
    CHECK(p2i_medium.speedup > 0.0);
    CHECK(p2i_large.speedup > 0.0);
    CHECK(split_small.speedup > 0.0);
    CHECK(split_medium.speedup > 0.0);
    CHECK(split_large.speedup > 0.0);
    CHECK(merge_small.speedup > 0.0);
    CHECK(merge_medium.speedup > 0.0);
    CHECK(merge_large.speedup > 0.0);
}
