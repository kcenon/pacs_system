#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>

#include "pacs/encoding/simd/simd_rle.hpp"
#include "pacs/encoding/simd/simd_config.hpp"

#include <algorithm>
#include <numeric>
#include <random>
#include <vector>

using namespace pacs::encoding::simd;

namespace {

/**
 * @brief Creates a test RGB interleaved image
 */
std::vector<uint8_t> create_rgb_interleaved(size_t pixel_count, uint32_t seed = 12345) {
    std::vector<uint8_t> data(pixel_count * 3);
    std::mt19937 gen(seed);
    std::uniform_int_distribution<int> dist(0, 255);
    for (auto& byte : data) {
        byte = static_cast<uint8_t>(dist(gen));
    }
    return data;
}

/**
 * @brief Creates a test 16-bit grayscale image
 */
std::vector<uint8_t> create_16bit_grayscale(size_t pixel_count, uint32_t seed = 12345) {
    std::vector<uint8_t> data(pixel_count * 2);
    std::mt19937 gen(seed);
    std::uniform_int_distribution<int> dist(0, 65535);
    for (size_t i = 0; i < pixel_count; ++i) {
        uint16_t value = static_cast<uint16_t>(dist(gen));
        data[i * 2] = static_cast<uint8_t>(value & 0xFF);
        data[i * 2 + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    }
    return data;
}

/**
 * @brief Verifies two vectors are identical
 */
bool vectors_equal(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
}

/**
 * @brief Reference scalar implementation for interleaved to planar
 */
void reference_interleaved_to_planar_rgb8(const uint8_t* src, uint8_t* r,
                                           uint8_t* g, uint8_t* b,
                                           size_t pixel_count) {
    for (size_t i = 0; i < pixel_count; ++i) {
        r[i] = src[i * 3];
        g[i] = src[i * 3 + 1];
        b[i] = src[i * 3 + 2];
    }
}

/**
 * @brief Reference scalar implementation for planar to interleaved
 */
void reference_planar_to_interleaved_rgb8(const uint8_t* r, const uint8_t* g,
                                           const uint8_t* b, uint8_t* dst,
                                           size_t pixel_count) {
    for (size_t i = 0; i < pixel_count; ++i) {
        dst[i * 3] = r[i];
        dst[i * 3 + 1] = g[i];
        dst[i * 3 + 2] = b[i];
    }
}

/**
 * @brief Reference scalar implementation for 16-bit split
 */
void reference_split_16bit(const uint8_t* src, uint8_t* high, uint8_t* low,
                           size_t pixel_count) {
    for (size_t i = 0; i < pixel_count; ++i) {
        low[i] = src[i * 2];
        high[i] = src[i * 2 + 1];
    }
}

/**
 * @brief Reference scalar implementation for 16-bit merge
 */
void reference_merge_16bit(const uint8_t* high, const uint8_t* low,
                           uint8_t* dst, size_t pixel_count) {
    for (size_t i = 0; i < pixel_count; ++i) {
        dst[i * 2] = low[i];
        dst[i * 2 + 1] = high[i];
    }
}

}  // namespace

TEST_CASE("simd_rle: CPU feature detection", "[encoding][simd][rle]") {
    SECTION("get_features returns valid flags") {
        simd_feature features = get_features();
        // At least check that no invalid bits are set
        uint32_t valid_mask = static_cast<uint32_t>(simd_feature::sse2) |
                              static_cast<uint32_t>(simd_feature::ssse3) |
                              static_cast<uint32_t>(simd_feature::sse41) |
                              static_cast<uint32_t>(simd_feature::avx) |
                              static_cast<uint32_t>(simd_feature::avx2) |
                              static_cast<uint32_t>(simd_feature::avx512f) |
                              static_cast<uint32_t>(simd_feature::neon);
        REQUIRE((static_cast<uint32_t>(features) & ~valid_mask) == 0);
    }

    SECTION("optimal_vector_width returns sensible value") {
        size_t width = optimal_vector_width();
        // Should be 0, 16, 32, or 64
        REQUIRE((width == 0 || width == 16 || width == 32 || width == 64));
    }
}

TEST_CASE("simd_rle: interleaved_to_planar_rgb8", "[encoding][simd][rle]") {
    std::vector<size_t> test_sizes = {
        1, 2, 3, 7, 15, 16, 17, 31, 32, 33, 63, 64, 65,
        127, 128, 129, 255, 256, 257, 1000, 4096, 65536
    };

    for (size_t pixel_count : test_sizes) {
        DYNAMIC_SECTION("pixel_count = " << pixel_count) {
            auto src = create_rgb_interleaved(pixel_count);

            // Reference output
            std::vector<uint8_t> ref_r(pixel_count);
            std::vector<uint8_t> ref_g(pixel_count);
            std::vector<uint8_t> ref_b(pixel_count);
            reference_interleaved_to_planar_rgb8(src.data(), ref_r.data(),
                                                  ref_g.data(), ref_b.data(),
                                                  pixel_count);

            // SIMD output
            std::vector<uint8_t> simd_r(pixel_count);
            std::vector<uint8_t> simd_g(pixel_count);
            std::vector<uint8_t> simd_b(pixel_count);
            interleaved_to_planar_rgb8(src.data(), simd_r.data(),
                                        simd_g.data(), simd_b.data(),
                                        pixel_count);

            REQUIRE(vectors_equal(ref_r, simd_r));
            REQUIRE(vectors_equal(ref_g, simd_g));
            REQUIRE(vectors_equal(ref_b, simd_b));
        }
    }
}

TEST_CASE("simd_rle: planar_to_interleaved_rgb8", "[encoding][simd][rle]") {
    std::vector<size_t> test_sizes = {
        1, 2, 3, 7, 15, 16, 17, 31, 32, 33, 63, 64, 65,
        127, 128, 129, 255, 256, 257, 1000, 4096, 65536
    };

    for (size_t pixel_count : test_sizes) {
        DYNAMIC_SECTION("pixel_count = " << pixel_count) {
            // Create separate planes
            std::vector<uint8_t> r(pixel_count);
            std::vector<uint8_t> g(pixel_count);
            std::vector<uint8_t> b(pixel_count);

            std::mt19937 gen(pixel_count);
            std::uniform_int_distribution<int> dist(0, 255);
            for (size_t i = 0; i < pixel_count; ++i) {
                r[i] = static_cast<uint8_t>(dist(gen));
                g[i] = static_cast<uint8_t>(dist(gen));
                b[i] = static_cast<uint8_t>(dist(gen));
            }

            // Reference output
            std::vector<uint8_t> ref_dst(pixel_count * 3);
            reference_planar_to_interleaved_rgb8(r.data(), g.data(), b.data(),
                                                  ref_dst.data(), pixel_count);

            // SIMD output
            std::vector<uint8_t> simd_dst(pixel_count * 3);
            planar_to_interleaved_rgb8(r.data(), g.data(), b.data(),
                                        simd_dst.data(), pixel_count);

            REQUIRE(vectors_equal(ref_dst, simd_dst));
        }
    }
}

TEST_CASE("simd_rle: RGB round-trip", "[encoding][simd][rle]") {
    std::vector<size_t> test_sizes = {16, 32, 64, 128, 256, 1024, 4096};

    for (size_t pixel_count : test_sizes) {
        DYNAMIC_SECTION("round-trip pixel_count = " << pixel_count) {
            auto original = create_rgb_interleaved(pixel_count);

            // Convert to planar
            std::vector<uint8_t> r(pixel_count);
            std::vector<uint8_t> g(pixel_count);
            std::vector<uint8_t> b(pixel_count);
            interleaved_to_planar_rgb8(original.data(), r.data(), g.data(),
                                        b.data(), pixel_count);

            // Convert back to interleaved
            std::vector<uint8_t> result(pixel_count * 3);
            planar_to_interleaved_rgb8(r.data(), g.data(), b.data(),
                                        result.data(), pixel_count);

            REQUIRE(vectors_equal(original, result));
        }
    }
}

TEST_CASE("simd_rle: split_16bit_to_planes", "[encoding][simd][rle]") {
    std::vector<size_t> test_sizes = {
        1, 2, 3, 7, 15, 16, 17, 31, 32, 33, 63, 64, 65,
        127, 128, 129, 255, 256, 257, 1000, 4096, 65536
    };

    for (size_t pixel_count : test_sizes) {
        DYNAMIC_SECTION("pixel_count = " << pixel_count) {
            auto src = create_16bit_grayscale(pixel_count);

            // Reference output
            std::vector<uint8_t> ref_high(pixel_count);
            std::vector<uint8_t> ref_low(pixel_count);
            reference_split_16bit(src.data(), ref_high.data(), ref_low.data(),
                                  pixel_count);

            // SIMD output
            std::vector<uint8_t> simd_high(pixel_count);
            std::vector<uint8_t> simd_low(pixel_count);
            split_16bit_to_planes(src.data(), simd_high.data(), simd_low.data(),
                                   pixel_count);

            REQUIRE(vectors_equal(ref_high, simd_high));
            REQUIRE(vectors_equal(ref_low, simd_low));
        }
    }
}

TEST_CASE("simd_rle: merge_planes_to_16bit", "[encoding][simd][rle]") {
    std::vector<size_t> test_sizes = {
        1, 2, 3, 7, 15, 16, 17, 31, 32, 33, 63, 64, 65,
        127, 128, 129, 255, 256, 257, 1000, 4096, 65536
    };

    for (size_t pixel_count : test_sizes) {
        DYNAMIC_SECTION("pixel_count = " << pixel_count) {
            // Create separate planes
            std::vector<uint8_t> high(pixel_count);
            std::vector<uint8_t> low(pixel_count);

            std::mt19937 gen(pixel_count);
            std::uniform_int_distribution<int> dist(0, 255);
            for (size_t i = 0; i < pixel_count; ++i) {
                high[i] = static_cast<uint8_t>(dist(gen));
                low[i] = static_cast<uint8_t>(dist(gen));
            }

            // Reference output
            std::vector<uint8_t> ref_dst(pixel_count * 2);
            reference_merge_16bit(high.data(), low.data(), ref_dst.data(),
                                  pixel_count);

            // SIMD output
            std::vector<uint8_t> simd_dst(pixel_count * 2);
            merge_planes_to_16bit(high.data(), low.data(), simd_dst.data(),
                                   pixel_count);

            REQUIRE(vectors_equal(ref_dst, simd_dst));
        }
    }
}

TEST_CASE("simd_rle: 16-bit round-trip", "[encoding][simd][rle]") {
    std::vector<size_t> test_sizes = {16, 32, 64, 128, 256, 1024, 4096};

    for (size_t pixel_count : test_sizes) {
        DYNAMIC_SECTION("round-trip pixel_count = " << pixel_count) {
            auto original = create_16bit_grayscale(pixel_count);

            // Split to planes
            std::vector<uint8_t> high(pixel_count);
            std::vector<uint8_t> low(pixel_count);
            split_16bit_to_planes(original.data(), high.data(), low.data(),
                                   pixel_count);

            // Merge back
            std::vector<uint8_t> result(pixel_count * 2);
            merge_planes_to_16bit(high.data(), low.data(), result.data(),
                                   pixel_count);

            REQUIRE(vectors_equal(original, result));
        }
    }
}

TEST_CASE("simd_rle: edge cases", "[encoding][simd][rle]") {
    SECTION("zero pixel count does nothing") {
        std::vector<uint8_t> src(48, 0xFF);
        std::vector<uint8_t> r(16, 0x00);
        std::vector<uint8_t> g(16, 0x00);
        std::vector<uint8_t> b(16, 0x00);

        interleaved_to_planar_rgb8(src.data(), r.data(), g.data(), b.data(), 0);

        // r, g, b should remain unchanged
        REQUIRE(std::all_of(r.begin(), r.end(), [](uint8_t v) { return v == 0x00; }));
        REQUIRE(std::all_of(g.begin(), g.end(), [](uint8_t v) { return v == 0x00; }));
        REQUIRE(std::all_of(b.begin(), b.end(), [](uint8_t v) { return v == 0x00; }));
    }

    SECTION("single pixel RGB works") {
        std::vector<uint8_t> src = {0x11, 0x22, 0x33};
        std::vector<uint8_t> r(1), g(1), b(1);

        interleaved_to_planar_rgb8(src.data(), r.data(), g.data(), b.data(), 1);

        REQUIRE(r[0] == 0x11);
        REQUIRE(g[0] == 0x22);
        REQUIRE(b[0] == 0x33);
    }

    SECTION("single pixel 16-bit works") {
        std::vector<uint8_t> src = {0x34, 0x12};  // Little-endian 0x1234
        std::vector<uint8_t> high(1), low(1);

        split_16bit_to_planes(src.data(), high.data(), low.data(), 1);

        REQUIRE(low[0] == 0x34);
        REQUIRE(high[0] == 0x12);
    }
}

TEST_CASE("simd_rle: known pattern verification", "[encoding][simd][rle]") {
    SECTION("RGB gradient pattern") {
        const size_t pixel_count = 256;
        std::vector<uint8_t> src(pixel_count * 3);

        // Create gradient: R=i, G=255-i, B=i/2
        for (size_t i = 0; i < pixel_count; ++i) {
            src[i * 3] = static_cast<uint8_t>(i);
            src[i * 3 + 1] = static_cast<uint8_t>(255 - i);
            src[i * 3 + 2] = static_cast<uint8_t>(i / 2);
        }

        std::vector<uint8_t> r(pixel_count), g(pixel_count), b(pixel_count);
        interleaved_to_planar_rgb8(src.data(), r.data(), g.data(), b.data(),
                                    pixel_count);

        // Verify pattern
        for (size_t i = 0; i < pixel_count; ++i) {
            REQUIRE(r[i] == static_cast<uint8_t>(i));
            REQUIRE(g[i] == static_cast<uint8_t>(255 - i));
            REQUIRE(b[i] == static_cast<uint8_t>(i / 2));
        }
    }

    SECTION("16-bit gradient pattern") {
        const size_t pixel_count = 256;
        std::vector<uint8_t> src(pixel_count * 2);

        // Create gradient: value = i * 256
        for (size_t i = 0; i < pixel_count; ++i) {
            uint16_t value = static_cast<uint16_t>(i * 256);
            src[i * 2] = static_cast<uint8_t>(value & 0xFF);
            src[i * 2 + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
        }

        std::vector<uint8_t> high(pixel_count), low(pixel_count);
        split_16bit_to_planes(src.data(), high.data(), low.data(), pixel_count);

        // Verify pattern: low bytes should all be 0, high bytes should be 0,1,2,...
        for (size_t i = 0; i < pixel_count; ++i) {
            REQUIRE(low[i] == 0);
            REQUIRE(high[i] == static_cast<uint8_t>(i));
        }
    }
}
