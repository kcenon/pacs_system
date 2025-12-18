/**
 * @file byte_swap_benchmark.cpp
 * @brief Benchmarks for SIMD byte swap operations
 *
 * Measures performance of byte swapping operations used for DICOM
 * endian conversion (OW, OL, OD VR types).
 *
 * @see Issue #332 - Add SIMD optimization benchmarks
 * @see Issue #313 - Apply SIMD Optimization (Image Processing)
 */

#include "simd_benchmark_common.hpp"

#include "pacs/encoding/simd/simd_config.hpp"
#include "pacs/encoding/simd/simd_utils.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

#include <cstring>
#include <iostream>
#include <vector>

namespace {

using namespace pacs::benchmark::simd;
using namespace pacs::encoding::simd;

// =============================================================================
// Scalar implementations for comparison
// =============================================================================

void swap_bytes_16_scalar(const uint8_t* src, uint8_t* dst,
                          size_t byte_count) noexcept {
    for (size_t i = 0; i + 1 < byte_count; i += 2) {
        dst[i] = src[i + 1];
        dst[i + 1] = src[i];
    }
}

void swap_bytes_32_scalar(const uint8_t* src, uint8_t* dst,
                          size_t byte_count) noexcept {
    for (size_t i = 0; i + 3 < byte_count; i += 4) {
        dst[i] = src[i + 3];
        dst[i + 1] = src[i + 2];
        dst[i + 2] = src[i + 1];
        dst[i + 3] = src[i];
    }
}

void swap_bytes_64_scalar(const uint8_t* src, uint8_t* dst,
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

// =============================================================================
// Benchmark helper functions
// =============================================================================

struct benchmark_result {
    double scalar_ns;
    double simd_ns;
    double speedup;
    size_t data_size;
};

template<typename ScalarFn, typename SimdFn>
benchmark_result run_byte_swap_benchmark(
    ScalarFn scalar_fn,
    SimdFn simd_fn,
    size_t data_size,
    size_t warmup_iterations = kWarmupIterations,
    size_t benchmark_iterations = kBenchmarkIterations) {

    auto data = generate_random_data(data_size);
    std::vector<uint8_t> result_scalar(data_size);
    std::vector<uint8_t> result_simd(data_size);

    high_resolution_timer timer;
    benchmark_stats scalar_stats;
    benchmark_stats simd_stats;

    // Warm-up scalar
    for (size_t i = 0; i < warmup_iterations; ++i) {
        scalar_fn(data.data(), result_scalar.data(), data_size);
    }

    // Benchmark scalar
    for (size_t i = 0; i < benchmark_iterations; ++i) {
        timer.start();
        scalar_fn(data.data(), result_scalar.data(), data_size);
        timer.stop();
        scalar_stats.record(static_cast<double>(timer.elapsed_ns().count()));
    }

    // Warm-up SIMD
    for (size_t i = 0; i < warmup_iterations; ++i) {
        simd_fn(data.data(), result_simd.data(), data_size);
    }

    // Benchmark SIMD
    for (size_t i = 0; i < benchmark_iterations; ++i) {
        timer.start();
        simd_fn(data.data(), result_simd.data(), data_size);
        timer.stop();
        simd_stats.record(static_cast<double>(timer.elapsed_ns().count()));
    }

    // Verify results match
    REQUIRE(result_scalar == result_simd);

    return {
        scalar_stats.mean_ns(),
        simd_stats.mean_ns(),
        calculate_speedup(scalar_stats.mean_ns(), simd_stats.mean_ns()),
        data_size
    };
}

void print_benchmark_result(const std::string& name, const benchmark_result& result) {
    std::cout << "\n=== " << name << " (" << format_size(result.data_size) << ") ===" << std::endl;
    std::cout << "  Scalar:  " << format_duration(result.scalar_ns)
              << " (" << format_throughput(result.data_size / (result.scalar_ns / 1e9)) << ")"
              << std::endl;
    std::cout << "  SIMD:    " << format_duration(result.simd_ns)
              << " (" << format_throughput(result.data_size / (result.simd_ns / 1e9)) << ")"
              << std::endl;
    std::cout << "  Speedup: " << format_speedup(result.speedup) << std::endl;
}

}  // namespace

// =============================================================================
// Test Cases
// =============================================================================

TEST_CASE("SIMD feature detection", "[benchmark][simd][info]") {
    std::cout << "\n" << get_simd_features_string() << std::endl;

    SECTION("Print CPU features") {
#if defined(PACS_SIMD_AVX2)
        INFO("AVX2: " << (has_avx2() ? "available" : "not available"));
#endif
#if defined(PACS_SIMD_SSSE3)
        INFO("SSSE3: " << (has_ssse3() ? "available" : "not available"));
#endif
#if defined(PACS_SIMD_SSE2)
        INFO("SSE2: " << (has_sse2() ? "available" : "not available"));
#endif
#if defined(PACS_SIMD_NEON)
        INFO("NEON: available (compile-time)");
#endif
        SUCCEED();
    }
}

// =============================================================================
// 16-bit byte swap benchmarks (OW - Other Word)
// =============================================================================

TEST_CASE("Byte swap 16-bit (OW) benchmarks", "[benchmark][simd][byte_swap][16bit]") {
    std::cout << "\n" << get_simd_features_string() << std::endl;

    SECTION("Small data (1 KB)") {
        auto result = run_byte_swap_benchmark(
            swap_bytes_16_scalar,
            swap_bytes_16_simd,
            kSmallSize
        );
        print_benchmark_result("16-bit byte swap", result);
        CHECK(result.speedup > 0.0);
    }

    SECTION("Medium data (1 MB)") {
        auto result = run_byte_swap_benchmark(
            swap_bytes_16_scalar,
            swap_bytes_16_simd,
            kMediumSize
        );
        print_benchmark_result("16-bit byte swap", result);
        CHECK(result.speedup > 0.0);
    }

    SECTION("Large data (16 MB)") {
        auto result = run_byte_swap_benchmark(
            swap_bytes_16_scalar,
            swap_bytes_16_simd,
            kLargeSize
        );
        print_benchmark_result("16-bit byte swap", result);
        CHECK(result.speedup > 0.0);
    }
}

// Catch2 BENCHMARK macros for CI integration
TEST_CASE("16-bit byte swap - Catch2 BENCHMARK", "[benchmark][simd][byte_swap][16bit][catch2]") {
    auto small_data = generate_random_data(kSmallSize);
    auto medium_data = generate_random_data(kMediumSize);
    auto large_data = generate_random_data(kLargeSize);

    std::vector<uint8_t> result_small(kSmallSize);
    std::vector<uint8_t> result_medium(kMediumSize);
    std::vector<uint8_t> result_large(kLargeSize);

    BENCHMARK("scalar_16bit_1KB") {
        swap_bytes_16_scalar(small_data.data(), result_small.data(), kSmallSize);
        return result_small[0];
    };

    BENCHMARK("simd_16bit_1KB") {
        swap_bytes_16_simd(small_data.data(), result_small.data(), kSmallSize);
        return result_small[0];
    };

    BENCHMARK("scalar_16bit_1MB") {
        swap_bytes_16_scalar(medium_data.data(), result_medium.data(), kMediumSize);
        return result_medium[0];
    };

    BENCHMARK("simd_16bit_1MB") {
        swap_bytes_16_simd(medium_data.data(), result_medium.data(), kMediumSize);
        return result_medium[0];
    };

    BENCHMARK("scalar_16bit_16MB") {
        swap_bytes_16_scalar(large_data.data(), result_large.data(), kLargeSize);
        return result_large[0];
    };

    BENCHMARK("simd_16bit_16MB") {
        swap_bytes_16_simd(large_data.data(), result_large.data(), kLargeSize);
        return result_large[0];
    };
}

// =============================================================================
// 32-bit byte swap benchmarks (OL - Other Long)
// =============================================================================

TEST_CASE("Byte swap 32-bit (OL) benchmarks", "[benchmark][simd][byte_swap][32bit]") {
    std::cout << "\n" << get_simd_features_string() << std::endl;

    SECTION("Small data (1 KB)") {
        auto result = run_byte_swap_benchmark(
            swap_bytes_32_scalar,
            swap_bytes_32_simd,
            kSmallSize
        );
        print_benchmark_result("32-bit byte swap", result);
        CHECK(result.speedup > 0.0);
    }

    SECTION("Medium data (1 MB)") {
        auto result = run_byte_swap_benchmark(
            swap_bytes_32_scalar,
            swap_bytes_32_simd,
            kMediumSize
        );
        print_benchmark_result("32-bit byte swap", result);
        CHECK(result.speedup > 0.0);
    }

    SECTION("Large data (16 MB)") {
        auto result = run_byte_swap_benchmark(
            swap_bytes_32_scalar,
            swap_bytes_32_simd,
            kLargeSize
        );
        print_benchmark_result("32-bit byte swap", result);
        CHECK(result.speedup > 0.0);
    }
}

TEST_CASE("32-bit byte swap - Catch2 BENCHMARK", "[benchmark][simd][byte_swap][32bit][catch2]") {
    auto small_data = generate_random_data(kSmallSize);
    auto medium_data = generate_random_data(kMediumSize);
    auto large_data = generate_random_data(kLargeSize);

    std::vector<uint8_t> result_small(kSmallSize);
    std::vector<uint8_t> result_medium(kMediumSize);
    std::vector<uint8_t> result_large(kLargeSize);

    BENCHMARK("scalar_32bit_1KB") {
        swap_bytes_32_scalar(small_data.data(), result_small.data(), kSmallSize);
        return result_small[0];
    };

    BENCHMARK("simd_32bit_1KB") {
        swap_bytes_32_simd(small_data.data(), result_small.data(), kSmallSize);
        return result_small[0];
    };

    BENCHMARK("scalar_32bit_1MB") {
        swap_bytes_32_scalar(medium_data.data(), result_medium.data(), kMediumSize);
        return result_medium[0];
    };

    BENCHMARK("simd_32bit_1MB") {
        swap_bytes_32_simd(medium_data.data(), result_medium.data(), kMediumSize);
        return result_medium[0];
    };

    BENCHMARK("scalar_32bit_16MB") {
        swap_bytes_32_scalar(large_data.data(), result_large.data(), kLargeSize);
        return result_large[0];
    };

    BENCHMARK("simd_32bit_16MB") {
        swap_bytes_32_simd(large_data.data(), result_large.data(), kLargeSize);
        return result_large[0];
    };
}

// =============================================================================
// 64-bit byte swap benchmarks (OD - Other Double)
// =============================================================================

TEST_CASE("Byte swap 64-bit (OD) benchmarks", "[benchmark][simd][byte_swap][64bit]") {
    std::cout << "\n" << get_simd_features_string() << std::endl;

    SECTION("Small data (1 KB)") {
        auto result = run_byte_swap_benchmark(
            swap_bytes_64_scalar,
            swap_bytes_64_simd,
            kSmallSize
        );
        print_benchmark_result("64-bit byte swap", result);
        CHECK(result.speedup > 0.0);
    }

    SECTION("Medium data (1 MB)") {
        auto result = run_byte_swap_benchmark(
            swap_bytes_64_scalar,
            swap_bytes_64_simd,
            kMediumSize
        );
        print_benchmark_result("64-bit byte swap", result);
        CHECK(result.speedup > 0.0);
    }

    SECTION("Large data (16 MB)") {
        auto result = run_byte_swap_benchmark(
            swap_bytes_64_scalar,
            swap_bytes_64_simd,
            kLargeSize
        );
        print_benchmark_result("64-bit byte swap", result);
        CHECK(result.speedup > 0.0);
    }
}

TEST_CASE("64-bit byte swap - Catch2 BENCHMARK", "[benchmark][simd][byte_swap][64bit][catch2]") {
    auto small_data = generate_random_data(kSmallSize);
    auto medium_data = generate_random_data(kMediumSize);
    auto large_data = generate_random_data(kLargeSize);

    std::vector<uint8_t> result_small(kSmallSize);
    std::vector<uint8_t> result_medium(kMediumSize);
    std::vector<uint8_t> result_large(kLargeSize);

    BENCHMARK("scalar_64bit_1KB") {
        swap_bytes_64_scalar(small_data.data(), result_small.data(), kSmallSize);
        return result_small[0];
    };

    BENCHMARK("simd_64bit_1KB") {
        swap_bytes_64_simd(small_data.data(), result_small.data(), kSmallSize);
        return result_small[0];
    };

    BENCHMARK("scalar_64bit_1MB") {
        swap_bytes_64_scalar(medium_data.data(), result_medium.data(), kMediumSize);
        return result_medium[0];
    };

    BENCHMARK("simd_64bit_1MB") {
        swap_bytes_64_simd(medium_data.data(), result_medium.data(), kMediumSize);
        return result_medium[0];
    };

    BENCHMARK("scalar_64bit_16MB") {
        swap_bytes_64_scalar(large_data.data(), result_large.data(), kLargeSize);
        return result_large[0];
    };

    BENCHMARK("simd_64bit_16MB") {
        swap_bytes_64_simd(large_data.data(), result_large.data(), kLargeSize);
        return result_large[0];
    };
}

// =============================================================================
// Summary test case
// =============================================================================

TEST_CASE("Byte swap benchmark summary", "[benchmark][simd][byte_swap][summary]") {
    std::cout << "\n========================================" << std::endl;
    std::cout << "BYTE SWAP BENCHMARK SUMMARY" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << get_simd_features_string() << std::endl;

    struct summary_entry {
        std::string name;
        size_t size;
        double speedup;
    };

    std::vector<summary_entry> results;

    // 16-bit
    auto r16_small = run_byte_swap_benchmark(swap_bytes_16_scalar, swap_bytes_16_simd, kSmallSize);
    auto r16_medium = run_byte_swap_benchmark(swap_bytes_16_scalar, swap_bytes_16_simd, kMediumSize);
    auto r16_large = run_byte_swap_benchmark(swap_bytes_16_scalar, swap_bytes_16_simd, kLargeSize);

    // 32-bit
    auto r32_small = run_byte_swap_benchmark(swap_bytes_32_scalar, swap_bytes_32_simd, kSmallSize);
    auto r32_medium = run_byte_swap_benchmark(swap_bytes_32_scalar, swap_bytes_32_simd, kMediumSize);
    auto r32_large = run_byte_swap_benchmark(swap_bytes_32_scalar, swap_bytes_32_simd, kLargeSize);

    // 64-bit
    auto r64_small = run_byte_swap_benchmark(swap_bytes_64_scalar, swap_bytes_64_simd, kSmallSize);
    auto r64_medium = run_byte_swap_benchmark(swap_bytes_64_scalar, swap_bytes_64_simd, kMediumSize);
    auto r64_large = run_byte_swap_benchmark(swap_bytes_64_scalar, swap_bytes_64_simd, kLargeSize);

    std::cout << "\n+--------------------+----------+----------+" << std::endl;
    std::cout << "| Operation          | Size     | Speedup  |" << std::endl;
    std::cout << "+--------------------+----------+----------+" << std::endl;

    auto print_row = [](const std::string& op, const std::string& size, double speedup) {
        std::cout << "| " << std::left << std::setw(18) << op
                  << " | " << std::setw(8) << size
                  << " | " << std::setw(8) << format_speedup(speedup)
                  << " |" << std::endl;
    };

    print_row("16-bit swap (OW)", "1 KB", r16_small.speedup);
    print_row("16-bit swap (OW)", "1 MB", r16_medium.speedup);
    print_row("16-bit swap (OW)", "16 MB", r16_large.speedup);
    print_row("32-bit swap (OL)", "1 KB", r32_small.speedup);
    print_row("32-bit swap (OL)", "1 MB", r32_medium.speedup);
    print_row("32-bit swap (OL)", "16 MB", r32_large.speedup);
    print_row("64-bit swap (OD)", "1 KB", r64_small.speedup);
    print_row("64-bit swap (OD)", "1 MB", r64_medium.speedup);
    print_row("64-bit swap (OD)", "16 MB", r64_large.speedup);

    std::cout << "+--------------------+----------+----------+" << std::endl;

    // Verify results are valid (speedup can be < 1.0 on platforms where
    // compiler auto-vectorization is already applied to scalar code)
    CHECK(r16_small.speedup > 0.0);
    CHECK(r16_medium.speedup > 0.0);
    CHECK(r16_large.speedup > 0.0);
    CHECK(r32_small.speedup > 0.0);
    CHECK(r32_medium.speedup > 0.0);
    CHECK(r32_large.speedup > 0.0);
    CHECK(r64_small.speedup > 0.0);
    CHECK(r64_medium.speedup > 0.0);
    CHECK(r64_large.speedup > 0.0);
}
