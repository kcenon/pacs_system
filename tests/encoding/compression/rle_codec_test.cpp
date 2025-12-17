#include <catch2/catch_test_macros.hpp>
#include <pacs/core/result.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "pacs/encoding/compression/rle_codec.hpp"
#include "pacs/encoding/compression/codec_factory.hpp"
#include "pacs/encoding/compression/image_params.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <vector>

using namespace pacs::encoding::compression;

namespace {

/**
 * @brief Creates a simple 8-bit grayscale gradient test image.
 */
std::vector<uint8_t> create_gradient_image_8bit(uint16_t width, uint16_t height) {
    std::vector<uint8_t> data(width * height);
    // Handle edge case where width + height - 2 would be 0 (1x1 image)
    uint32_t divisor = (width + height > 2) ? (width + height - 2) : 1;
    for (uint16_t y = 0; y < height; ++y) {
        for (uint16_t x = 0; x < width; ++x) {
            data[y * width + x] = static_cast<uint8_t>(
                (x + y) * 255 / divisor);
        }
    }
    return data;
}

/**
 * @brief Creates a 16-bit grayscale gradient test image.
 */
std::vector<uint8_t> create_gradient_image_16bit(uint16_t width, uint16_t height) {
    std::vector<uint8_t> data(width * height * 2);
    for (uint16_t y = 0; y < height; ++y) {
        for (uint16_t x = 0; x < width; ++x) {
            uint16_t value = static_cast<uint16_t>(
                static_cast<uint32_t>(x + y) * 65535 / (width + height - 2));
            size_t idx = (y * width + x) * 2;
            // Little-endian storage
            data[idx] = static_cast<uint8_t>(value & 0xFF);
            data[idx + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
        }
    }
    return data;
}

/**
 * @brief Creates an 8-bit RGB color gradient test image.
 */
std::vector<uint8_t> create_rgb_image_8bit(uint16_t width, uint16_t height) {
    std::vector<uint8_t> data(width * height * 3);
    for (uint16_t y = 0; y < height; ++y) {
        for (uint16_t x = 0; x < width; ++x) {
            size_t idx = (y * width + x) * 3;
            data[idx] = static_cast<uint8_t>(x * 255 / (width - 1));      // R
            data[idx + 1] = static_cast<uint8_t>(y * 255 / (height - 1)); // G
            data[idx + 2] = static_cast<uint8_t>((x + y) * 127 / (width + height - 2)); // B
        }
    }
    return data;
}

/**
 * @brief Creates a random noise image for stress testing.
 */
std::vector<uint8_t> create_noise_image_8bit(uint16_t width, uint16_t height, uint32_t seed) {
    std::vector<uint8_t> data(width * height);
    std::mt19937 gen(seed);
    std::uniform_int_distribution<int> dist(0, 255);
    for (auto& pixel : data) {
        pixel = static_cast<uint8_t>(dist(gen));
    }
    return data;
}

/**
 * @brief Creates a solid color image (good for RLE compression).
 */
std::vector<uint8_t> create_solid_image_8bit(uint16_t width, uint16_t height, uint8_t value) {
    return std::vector<uint8_t>(width * height, value);
}

/**
 * @brief Creates an image with repeating patterns (excellent for RLE).
 */
std::vector<uint8_t> create_pattern_image_8bit(uint16_t width, uint16_t height) {
    std::vector<uint8_t> data(width * height);
    for (uint16_t y = 0; y < height; ++y) {
        for (uint16_t x = 0; x < width; ++x) {
            // Creates bands of solid colors
            data[y * width + x] = static_cast<uint8_t>((y / 10) * 32);
        }
    }
    return data;
}

/**
 * @brief Compares two images for exact equality (lossless verification).
 */
bool images_identical(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
}

}  // namespace

TEST_CASE("rle_codec basic properties", "[encoding][compression][rle]") {
    rle_codec codec;

    SECTION("transfer syntax UID is correct") {
        REQUIRE(codec.transfer_syntax_uid() == "1.2.840.10008.1.2.5");
    }

    SECTION("name is RLE Lossless") {
        REQUIRE(codec.name() == "RLE Lossless");
    }

    SECTION("is lossless codec") {
        REQUIRE(codec.is_lossy() == false);
    }
}

TEST_CASE("rle_codec can_encode validation", "[encoding][compression][rle]") {
    rle_codec codec;

    SECTION("accepts valid 8-bit grayscale parameters") {
        image_params params;
        params.width = 256;
        params.height = 256;
        params.bits_allocated = 8;
        params.bits_stored = 8;
        params.high_bit = 7;
        params.samples_per_pixel = 1;

        REQUIRE(codec.can_encode(params) == true);
    }

    SECTION("accepts valid 16-bit grayscale parameters") {
        image_params params;
        params.width = 512;
        params.height = 512;
        params.bits_allocated = 16;
        params.bits_stored = 16;
        params.high_bit = 15;
        params.samples_per_pixel = 1;

        REQUIRE(codec.can_encode(params) == true);
    }

    SECTION("accepts valid RGB color parameters") {
        image_params params;
        params.width = 256;
        params.height = 256;
        params.bits_allocated = 8;
        params.bits_stored = 8;
        params.high_bit = 7;
        params.samples_per_pixel = 3;
        params.photometric = photometric_interpretation::rgb;

        REQUIRE(codec.can_encode(params) == true);
    }

    SECTION("rejects 32-bit images") {
        image_params params;
        params.width = 256;
        params.height = 256;
        params.bits_allocated = 32;
        params.bits_stored = 32;
        params.high_bit = 31;
        params.samples_per_pixel = 1;

        REQUIRE(codec.can_encode(params) == false);
    }

    SECTION("rejects zero dimensions") {
        image_params params;
        params.width = 0;
        params.height = 256;
        params.bits_allocated = 8;
        params.bits_stored = 8;
        params.samples_per_pixel = 1;

        REQUIRE(codec.can_encode(params) == false);
    }

    SECTION("rejects 4 samples per pixel (exceeds segment limit)") {
        image_params params;
        params.width = 256;
        params.height = 256;
        params.bits_allocated = 8;
        params.bits_stored = 8;
        params.samples_per_pixel = 4;

        REQUIRE(codec.can_encode(params) == false);
    }
}

TEST_CASE("rle_codec 8-bit grayscale round-trip", "[encoding][compression][rle]") {
    rle_codec codec;

    uint16_t width = 64;
    uint16_t height = 64;
    auto original = create_gradient_image_8bit(width, height);

    image_params params;
    params.width = width;
    params.height = height;
    params.bits_allocated = 8;
    params.bits_stored = 8;
    params.high_bit = 7;
    params.samples_per_pixel = 1;
    params.photometric = photometric_interpretation::monochrome2;

    SECTION("encode succeeds") {
        auto encode_result = codec.encode(original, params);

        REQUIRE(encode_result.is_ok() == true);
        REQUIRE(pacs::get_value(encode_result).data.size() > 0);
        // RLE data should have 64-byte header
        REQUIRE(pacs::get_value(encode_result).data.size() >= 64);
    }

    SECTION("round-trip is perfectly lossless") {
        auto encode_result = codec.encode(original, params);
        REQUIRE(encode_result.is_ok() == true);

        auto decode_result = codec.decode(pacs::get_value(encode_result).data, params);
        REQUIRE(decode_result.is_ok() == true);
        REQUIRE(pacs::get_value(decode_result).data.size() == original.size());

        // Lossless verification - must be exactly identical
        REQUIRE(images_identical(original, pacs::get_value(decode_result).data));
    }

    SECTION("output params are set correctly") {
        auto encode_result = codec.encode(original, params);
        REQUIRE(encode_result.is_ok() == true);

        auto decode_result = codec.decode(pacs::get_value(encode_result).data, params);
        REQUIRE(decode_result.is_ok() == true);
        REQUIRE(pacs::get_value(decode_result).output_params.width == width);
        REQUIRE(pacs::get_value(decode_result).output_params.height == height);
        REQUIRE(pacs::get_value(decode_result).output_params.samples_per_pixel == 1);
        REQUIRE(pacs::get_value(decode_result).output_params.bits_allocated == 8);
        REQUIRE(pacs::get_value(decode_result).output_params.bits_stored == 8);
    }
}

TEST_CASE("rle_codec 16-bit grayscale round-trip", "[encoding][compression][rle]") {
    rle_codec codec;

    uint16_t width = 64;
    uint16_t height = 64;
    auto original = create_gradient_image_16bit(width, height);

    image_params params;
    params.width = width;
    params.height = height;
    params.bits_allocated = 16;
    params.bits_stored = 16;
    params.high_bit = 15;
    params.samples_per_pixel = 1;
    params.photometric = photometric_interpretation::monochrome2;

    SECTION("round-trip is perfectly lossless") {
        auto encode_result = codec.encode(original, params);
        REQUIRE(encode_result.is_ok() == true);

        auto decode_result = codec.decode(pacs::get_value(encode_result).data, params);
        REQUIRE(decode_result.is_ok() == true);
        REQUIRE(pacs::get_value(decode_result).data.size() == original.size());

        // Lossless verification
        REQUIRE(images_identical(original, pacs::get_value(decode_result).data));
    }

    SECTION("output params reflect 16-bit precision") {
        auto encode_result = codec.encode(original, params);
        REQUIRE(encode_result.is_ok() == true);

        auto decode_result = codec.decode(pacs::get_value(encode_result).data, params);
        REQUIRE(decode_result.is_ok() == true);
        REQUIRE(pacs::get_value(decode_result).output_params.bits_allocated == 16);
        REQUIRE(pacs::get_value(decode_result).output_params.bits_stored == 16);
    }
}

TEST_CASE("rle_codec RGB color round-trip", "[encoding][compression][rle]") {
    rle_codec codec;

    uint16_t width = 64;
    uint16_t height = 64;
    auto original = create_rgb_image_8bit(width, height);

    image_params params;
    params.width = width;
    params.height = height;
    params.bits_allocated = 8;
    params.bits_stored = 8;
    params.high_bit = 7;
    params.samples_per_pixel = 3;
    params.photometric = photometric_interpretation::rgb;

    SECTION("encode succeeds") {
        auto encode_result = codec.encode(original, params);
        REQUIRE(encode_result.is_ok() == true);
        REQUIRE(pacs::get_value(encode_result).data.size() > 0);
    }

    SECTION("round-trip is perfectly lossless") {
        auto encode_result = codec.encode(original, params);
        REQUIRE(encode_result.is_ok() == true);

        auto decode_result = codec.decode(pacs::get_value(encode_result).data, params);
        REQUIRE(decode_result.is_ok() == true);
        REQUIRE(pacs::get_value(decode_result).data.size() == original.size());

        // Lossless verification
        REQUIRE(images_identical(original, pacs::get_value(decode_result).data));
    }
}

TEST_CASE("rle_codec with solid color image", "[encoding][compression][rle]") {
    rle_codec codec;

    uint16_t width = 128;
    uint16_t height = 128;
    auto original = create_solid_image_8bit(width, height, 128);

    image_params params;
    params.width = width;
    params.height = height;
    params.bits_allocated = 8;
    params.bits_stored = 8;
    params.high_bit = 7;
    params.samples_per_pixel = 1;

    SECTION("achieves good compression on solid images") {
        auto encode_result = codec.encode(original, params);
        REQUIRE(encode_result.is_ok() == true);

        // Solid images should compress very well with RLE
        // Original: 128*128 = 16384 bytes
        // Compressed should be much smaller
        REQUIRE(pacs::get_value(encode_result).data.size() < original.size());
    }

    SECTION("round-trip is perfectly lossless") {
        auto encode_result = codec.encode(original, params);
        REQUIRE(encode_result.is_ok() == true);

        auto decode_result = codec.decode(pacs::get_value(encode_result).data, params);
        REQUIRE(decode_result.is_ok() == true);
        REQUIRE(images_identical(original, pacs::get_value(decode_result).data));
    }
}

TEST_CASE("rle_codec with pattern image", "[encoding][compression][rle]") {
    rle_codec codec;

    uint16_t width = 128;
    uint16_t height = 128;
    auto original = create_pattern_image_8bit(width, height);

    image_params params;
    params.width = width;
    params.height = height;
    params.bits_allocated = 8;
    params.bits_stored = 8;
    params.high_bit = 7;
    params.samples_per_pixel = 1;

    SECTION("achieves compression on pattern images") {
        auto encode_result = codec.encode(original, params);
        REQUIRE(encode_result.is_ok() == true);

        // Pattern images with horizontal bands should compress well with RLE
        REQUIRE(pacs::get_value(encode_result).data.size() < original.size());
    }

    SECTION("round-trip is perfectly lossless") {
        auto encode_result = codec.encode(original, params);
        REQUIRE(encode_result.is_ok() == true);

        auto decode_result = codec.decode(pacs::get_value(encode_result).data, params);
        REQUIRE(decode_result.is_ok() == true);
        REQUIRE(images_identical(original, pacs::get_value(decode_result).data));
    }
}

TEST_CASE("rle_codec with random noise", "[encoding][compression][rle]") {
    rle_codec codec;

    uint16_t width = 128;
    uint16_t height = 128;

    image_params params;
    params.width = width;
    params.height = height;
    params.bits_allocated = 8;
    params.bits_stored = 8;
    params.high_bit = 7;
    params.samples_per_pixel = 1;

    SECTION("lossless even with high-entropy data") {
        auto original = create_noise_image_8bit(width, height, 12345);

        auto encode_result = codec.encode(original, params);
        REQUIRE(encode_result.is_ok() == true);

        auto decode_result = codec.decode(pacs::get_value(encode_result).data, params);
        REQUIRE(decode_result.is_ok() == true);

        // Even high-entropy data must be perfectly reconstructed
        REQUIRE(images_identical(original, pacs::get_value(decode_result).data));
    }
}

TEST_CASE("rle_codec error handling", "[encoding][compression][rle]") {
    rle_codec codec;

    SECTION("empty pixel data returns error") {
        image_params params;
        params.width = 64;
        params.height = 64;
        params.bits_allocated = 8;
        params.bits_stored = 8;
        params.samples_per_pixel = 1;

        std::vector<uint8_t> empty_data;
        auto result = codec.encode(empty_data, params);

        REQUIRE(result.is_ok() == false);
        REQUIRE_FALSE(pacs::get_error(result).message.empty());
    }

    SECTION("size mismatch returns error") {
        image_params params;
        params.width = 64;
        params.height = 64;
        params.bits_allocated = 8;
        params.bits_stored = 8;
        params.samples_per_pixel = 1;

        std::vector<uint8_t> wrong_size(100);  // Should be 64*64 = 4096
        auto result = codec.encode(wrong_size, params);

        REQUIRE(result.is_ok() == false);
        REQUIRE_THAT(pacs::get_error(result).message,
                     Catch::Matchers::ContainsSubstring("mismatch"));
    }

    SECTION("empty compressed data returns error") {
        image_params params;
        params.width = 64;
        params.height = 64;
        params.bits_allocated = 8;
        params.samples_per_pixel = 1;

        std::vector<uint8_t> empty_data;
        auto result = codec.decode(empty_data, params);

        REQUIRE(result.is_ok() == false);
    }

    SECTION("too small compressed data returns error") {
        image_params params;
        params.width = 64;
        params.height = 64;
        params.bits_allocated = 8;
        params.samples_per_pixel = 1;

        // RLE header is 64 bytes, so 10 bytes is too small
        std::vector<uint8_t> small_data(10, 0);
        auto result = codec.decode(small_data, params);

        REQUIRE(result.is_ok() == false);
    }

    SECTION("invalid RLE header returns error") {
        image_params params;
        params.width = 64;
        params.height = 64;
        params.bits_allocated = 8;
        params.samples_per_pixel = 1;

        // Create 64-byte header with invalid segment count
        std::vector<uint8_t> invalid_data(64, 0);
        invalid_data[0] = 20;  // Invalid: more than 15 segments

        auto result = codec.decode(invalid_data, params);

        REQUIRE(result.is_ok() == false);
    }
}

TEST_CASE("codec_factory creates rle_codec", "[encoding][compression][rle]") {
    SECTION("create by UID") {
        auto codec = codec_factory::create("1.2.840.10008.1.2.5");

        REQUIRE(codec != nullptr);
        REQUIRE(codec->transfer_syntax_uid() == "1.2.840.10008.1.2.5");
        REQUIRE(codec->name() == "RLE Lossless");
    }

    SECTION("create by transfer_syntax") {
        pacs::encoding::transfer_syntax ts("1.2.840.10008.1.2.5");
        auto codec = codec_factory::create(ts);

        REQUIRE(codec != nullptr);
        REQUIRE(codec->is_lossy() == false);
    }

    SECTION("is_supported returns true for RLE") {
        REQUIRE(codec_factory::is_supported("1.2.840.10008.1.2.5") == true);
    }

    SECTION("supported_transfer_syntaxes includes RLE") {
        auto supported = codec_factory::supported_transfer_syntaxes();

        REQUIRE_FALSE(supported.empty());
        REQUIRE(std::find(supported.begin(), supported.end(),
                          "1.2.840.10008.1.2.5") != supported.end());
    }
}

TEST_CASE("image_params validation for RLE", "[encoding][compression][rle]") {
    SECTION("valid_for_rle accepts 8-bit grayscale") {
        image_params params;
        params.width = 256;
        params.height = 256;
        params.bits_allocated = 8;
        params.bits_stored = 8;
        params.samples_per_pixel = 1;

        REQUIRE(params.valid_for_rle() == true);
    }

    SECTION("valid_for_rle accepts 16-bit grayscale") {
        image_params params;
        params.width = 256;
        params.height = 256;
        params.bits_allocated = 16;
        params.bits_stored = 16;
        params.samples_per_pixel = 1;

        REQUIRE(params.valid_for_rle() == true);
    }

    SECTION("valid_for_rle accepts 8-bit RGB") {
        image_params params;
        params.width = 256;
        params.height = 256;
        params.bits_allocated = 8;
        params.bits_stored = 8;
        params.samples_per_pixel = 3;

        REQUIRE(params.valid_for_rle() == true);
    }

    SECTION("valid_for_rle rejects 32-bit") {
        image_params params;
        params.width = 256;
        params.height = 256;
        params.bits_allocated = 32;
        params.bits_stored = 32;
        params.samples_per_pixel = 1;

        REQUIRE(params.valid_for_rle() == false);
    }

    SECTION("valid_for_rle rejects zero dimensions") {
        image_params params;
        params.width = 0;
        params.height = 256;
        params.bits_allocated = 8;
        params.bits_stored = 8;
        params.samples_per_pixel = 1;

        REQUIRE(params.valid_for_rle() == false);
    }

    SECTION("valid_for_rle rejects 4 samples per pixel") {
        image_params params;
        params.width = 256;
        params.height = 256;
        params.bits_allocated = 8;
        params.bits_stored = 8;
        params.samples_per_pixel = 4;

        REQUIRE(params.valid_for_rle() == false);
    }
}

TEST_CASE("rle_codec various image sizes", "[encoding][compression][rle]") {
    rle_codec codec;

    std::vector<std::pair<uint16_t, uint16_t>> sizes = {
        {1, 1},       // Minimum size
        {2, 2},       // Very small
        {7, 11},      // Odd dimensions
        {64, 64},     // Standard small
        {256, 256},   // Medium
        {512, 512},   // Larger
        {100, 200},   // Non-square
    };

    for (const auto& [width, height] : sizes) {
        DYNAMIC_SECTION("size " << width << "x" << height << " round-trip is lossless") {
            auto original = create_gradient_image_8bit(width, height);

            image_params params;
            params.width = width;
            params.height = height;
            params.bits_allocated = 8;
            params.bits_stored = 8;
            params.high_bit = 7;
            params.samples_per_pixel = 1;

            auto encode_result = codec.encode(original, params);
            REQUIRE(encode_result.is_ok() == true);

            auto decode_result = codec.decode(pacs::get_value(encode_result).data, params);
            REQUIRE(decode_result.is_ok() == true);

            REQUIRE(images_identical(original, pacs::get_value(decode_result).data));
        }
    }
}

TEST_CASE("rle_codec move semantics", "[encoding][compression][rle]") {
    SECTION("move construction works") {
        rle_codec codec1;
        rle_codec codec2(std::move(codec1));

        REQUIRE(codec2.transfer_syntax_uid() == "1.2.840.10008.1.2.5");
    }

    SECTION("move assignment works") {
        rle_codec codec1;
        rle_codec codec2;
        codec2 = std::move(codec1);

        REQUIRE(codec2.transfer_syntax_uid() == "1.2.840.10008.1.2.5");
    }
}
