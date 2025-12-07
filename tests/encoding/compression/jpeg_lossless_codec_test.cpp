#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "pacs/encoding/compression/jpeg_lossless_codec.hpp"
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
    for (uint16_t y = 0; y < height; ++y) {
        for (uint16_t x = 0; x < width; ++x) {
            data[y * width + x] = static_cast<uint8_t>(
                (x + y) * 255 / (width + height - 2));
        }
    }
    return data;
}

/**
 * @brief Creates a 12-bit grayscale gradient test image (stored in 16-bit).
 */
std::vector<uint8_t> create_gradient_image_12bit(uint16_t width, uint16_t height) {
    std::vector<uint8_t> data(width * height * 2);
    for (uint16_t y = 0; y < height; ++y) {
        for (uint16_t x = 0; x < width; ++x) {
            uint16_t value = static_cast<uint16_t>(
                (x + y) * 4095 / (width + height - 2));
            size_t idx = (y * width + x) * 2;
            // Little-endian storage
            data[idx] = static_cast<uint8_t>(value & 0xFF);
            data[idx + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
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
 * @brief Compares two images for exact equality (lossless verification).
 */
bool images_identical(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
}

}  // namespace

TEST_CASE("jpeg_lossless_codec basic properties", "[encoding][compression][lossless]") {
    jpeg_lossless_codec codec;

    SECTION("transfer syntax UID is correct") {
        REQUIRE(codec.transfer_syntax_uid() == "1.2.840.10008.1.2.4.70");
    }

    SECTION("name is JPEG Lossless") {
        REQUIRE(codec.name() == "JPEG Lossless (Process 14, SV1)");
    }

    SECTION("is lossless codec") {
        REQUIRE(codec.is_lossy() == false);
    }

    SECTION("default predictor is 1") {
        REQUIRE(codec.predictor() == 1);
    }

    SECTION("default point transform is 0") {
        REQUIRE(codec.point_transform() == 0);
    }
}

TEST_CASE("jpeg_lossless_codec custom configuration", "[encoding][compression][lossless]") {
    SECTION("custom predictor") {
        jpeg_lossless_codec codec(7, 0);
        REQUIRE(codec.predictor() == 7);
    }

    SECTION("custom point transform") {
        jpeg_lossless_codec codec(1, 4);
        REQUIRE(codec.point_transform() == 4);
    }

    SECTION("predictor is clamped to valid range") {
        jpeg_lossless_codec codec_low(0, 0);
        REQUIRE(codec_low.predictor() == 1);

        jpeg_lossless_codec codec_high(10, 0);
        REQUIRE(codec_high.predictor() == 7);
    }

    SECTION("point transform is clamped to valid range") {
        jpeg_lossless_codec codec_high(1, 20);
        REQUIRE(codec_high.point_transform() == 15);
    }
}

TEST_CASE("jpeg_lossless_codec can_encode validation", "[encoding][compression][lossless]") {
    jpeg_lossless_codec codec;

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

    SECTION("accepts valid 12-bit grayscale parameters") {
        image_params params;
        params.width = 512;
        params.height = 512;
        params.bits_allocated = 16;
        params.bits_stored = 12;
        params.high_bit = 11;
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

    SECTION("rejects RGB color images") {
        image_params params;
        params.width = 256;
        params.height = 256;
        params.bits_allocated = 8;
        params.bits_stored = 8;
        params.high_bit = 7;
        params.samples_per_pixel = 3;

        REQUIRE(codec.can_encode(params) == false);
    }

    SECTION("rejects invalid bit depth") {
        image_params params;
        params.width = 256;
        params.height = 256;
        params.bits_allocated = 32;
        params.bits_stored = 32;
        params.high_bit = 31;
        params.samples_per_pixel = 1;

        REQUIRE(codec.can_encode(params) == false);
    }
}

TEST_CASE("jpeg_lossless_codec 8-bit grayscale round-trip", "[encoding][compression][lossless]") {
    jpeg_lossless_codec codec;

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

        REQUIRE(encode_result.success == true);
        REQUIRE(encode_result.data.size() > 0);
    }

    SECTION("round-trip is perfectly lossless") {
        auto encode_result = codec.encode(original, params);
        REQUIRE(encode_result.success == true);

        auto decode_result = codec.decode(encode_result.data, params);
        REQUIRE(decode_result.success == true);
        REQUIRE(decode_result.data.size() == original.size());

        // Lossless verification - must be exactly identical
        REQUIRE(images_identical(original, decode_result.data));
    }

    SECTION("output params are set correctly") {
        auto encode_result = codec.encode(original, params);
        REQUIRE(encode_result.success == true);

        auto decode_result = codec.decode(encode_result.data, params);
        REQUIRE(decode_result.success == true);
        REQUIRE(decode_result.output_params.width == width);
        REQUIRE(decode_result.output_params.height == height);
        REQUIRE(decode_result.output_params.samples_per_pixel == 1);
        REQUIRE(decode_result.output_params.bits_allocated == 8);
        REQUIRE(decode_result.output_params.bits_stored == 8);
    }
}

TEST_CASE("jpeg_lossless_codec 12-bit grayscale round-trip", "[encoding][compression][lossless]") {
    jpeg_lossless_codec codec;

    uint16_t width = 64;
    uint16_t height = 64;
    auto original = create_gradient_image_12bit(width, height);

    image_params params;
    params.width = width;
    params.height = height;
    params.bits_allocated = 16;
    params.bits_stored = 12;
    params.high_bit = 11;
    params.samples_per_pixel = 1;
    params.photometric = photometric_interpretation::monochrome2;

    SECTION("encode succeeds") {
        auto encode_result = codec.encode(original, params);

        REQUIRE(encode_result.success == true);
        REQUIRE(encode_result.data.size() > 0);
    }

    SECTION("round-trip is perfectly lossless") {
        auto encode_result = codec.encode(original, params);
        REQUIRE(encode_result.success == true);

        auto decode_result = codec.decode(encode_result.data, params);
        REQUIRE(decode_result.success == true);
        REQUIRE(decode_result.data.size() == original.size());

        // Lossless verification
        REQUIRE(images_identical(original, decode_result.data));
    }

    SECTION("output params reflect 12-bit precision") {
        auto encode_result = codec.encode(original, params);
        REQUIRE(encode_result.success == true);

        auto decode_result = codec.decode(encode_result.data, params);
        REQUIRE(decode_result.success == true);
        REQUIRE(decode_result.output_params.bits_allocated == 16);
        REQUIRE(decode_result.output_params.bits_stored == 12);
    }
}

TEST_CASE("jpeg_lossless_codec 16-bit grayscale round-trip", "[encoding][compression][lossless]") {
    jpeg_lossless_codec codec;

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
        REQUIRE(encode_result.success == true);

        auto decode_result = codec.decode(encode_result.data, params);
        REQUIRE(decode_result.success == true);
        REQUIRE(decode_result.data.size() == original.size());

        // Lossless verification
        REQUIRE(images_identical(original, decode_result.data));
    }
}

TEST_CASE("jpeg_lossless_codec with random noise", "[encoding][compression][lossless]") {
    jpeg_lossless_codec codec;

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
        REQUIRE(encode_result.success == true);

        auto decode_result = codec.decode(encode_result.data, params);
        REQUIRE(decode_result.success == true);

        // Even high-entropy data must be perfectly reconstructed
        REQUIRE(images_identical(original, decode_result.data));
    }
}

TEST_CASE("jpeg_lossless_codec different predictors", "[encoding][compression][lossless]") {
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

    for (int predictor = 1; predictor <= 7; ++predictor) {
        DYNAMIC_SECTION("predictor " << predictor << " produces lossless result") {
            jpeg_lossless_codec codec(predictor, 0);

            auto encode_result = codec.encode(original, params);
            REQUIRE(encode_result.success == true);

            auto decode_result = codec.decode(encode_result.data, params);
            REQUIRE(decode_result.success == true);

            REQUIRE(images_identical(original, decode_result.data));
        }
    }
}

TEST_CASE("jpeg_lossless_codec error handling", "[encoding][compression][lossless]") {
    jpeg_lossless_codec codec;

    SECTION("empty pixel data returns error") {
        image_params params;
        params.width = 64;
        params.height = 64;
        params.bits_allocated = 8;
        params.bits_stored = 8;
        params.samples_per_pixel = 1;

        std::vector<uint8_t> empty_data;
        auto result = codec.encode(empty_data, params);

        REQUIRE(result.success == false);
        REQUIRE_FALSE(result.error_message.empty());
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

        REQUIRE(result.success == false);
        REQUIRE_THAT(result.error_message,
                     Catch::Matchers::ContainsSubstring("mismatch"));
    }

    SECTION("empty compressed data returns error") {
        image_params params;
        params.width = 64;
        params.height = 64;

        std::vector<uint8_t> empty_data;
        auto result = codec.decode(empty_data, params);

        REQUIRE(result.success == false);
    }

    SECTION("invalid JPEG data returns error") {
        image_params params;
        params.width = 64;
        params.height = 64;

        std::vector<uint8_t> invalid_data = {0x00, 0x00, 0x00, 0x00};
        auto result = codec.decode(invalid_data, params);

        REQUIRE(result.success == false);
    }
}

TEST_CASE("codec_factory creates jpeg_lossless_codec", "[encoding][compression][lossless]") {
    SECTION("create by UID") {
        auto codec = codec_factory::create("1.2.840.10008.1.2.4.70");

        REQUIRE(codec != nullptr);
        REQUIRE(codec->transfer_syntax_uid() == "1.2.840.10008.1.2.4.70");
        REQUIRE(codec->name() == "JPEG Lossless (Process 14, SV1)");
    }

    SECTION("create by transfer_syntax") {
        pacs::encoding::transfer_syntax ts("1.2.840.10008.1.2.4.70");
        auto codec = codec_factory::create(ts);

        REQUIRE(codec != nullptr);
        REQUIRE(codec->is_lossy() == false);
    }

    SECTION("is_supported returns correct values") {
        REQUIRE(codec_factory::is_supported("1.2.840.10008.1.2.4.70") == true);
        REQUIRE(codec_factory::is_supported("1.2.840.10008.1.2.4.50") == true);  // Baseline
        REQUIRE(codec_factory::is_supported("1.2.840.10008.1.2.4.80") == false); // JPEG-LS (not yet)
    }

    SECTION("supported_transfer_syntaxes includes JPEG Lossless") {
        auto supported = codec_factory::supported_transfer_syntaxes();

        REQUIRE_FALSE(supported.empty());
        REQUIRE(std::find(supported.begin(), supported.end(),
                          "1.2.840.10008.1.2.4.70") != supported.end());
    }
}

TEST_CASE("image_params validation for JPEG Lossless", "[encoding][compression][lossless]") {
    SECTION("valid_for_jpeg_lossless accepts 8-bit") {
        image_params params;
        params.bits_allocated = 8;
        params.bits_stored = 8;
        params.samples_per_pixel = 1;

        REQUIRE(params.valid_for_jpeg_lossless() == true);
    }

    SECTION("valid_for_jpeg_lossless accepts 12-bit") {
        image_params params;
        params.bits_allocated = 16;
        params.bits_stored = 12;
        params.samples_per_pixel = 1;

        REQUIRE(params.valid_for_jpeg_lossless() == true);
    }

    SECTION("valid_for_jpeg_lossless accepts 16-bit") {
        image_params params;
        params.bits_allocated = 16;
        params.bits_stored = 16;
        params.samples_per_pixel = 1;

        REQUIRE(params.valid_for_jpeg_lossless() == true);
    }

    SECTION("valid_for_jpeg_lossless rejects color") {
        image_params params;
        params.bits_allocated = 8;
        params.bits_stored = 8;
        params.samples_per_pixel = 3;

        REQUIRE(params.valid_for_jpeg_lossless() == false);
    }

    SECTION("valid_for_jpeg_lossless rejects 1-bit") {
        image_params params;
        params.bits_allocated = 8;
        params.bits_stored = 1;
        params.samples_per_pixel = 1;

        REQUIRE(params.valid_for_jpeg_lossless() == false);
    }

    SECTION("valid_for_jpeg_lossless rejects 32-bit") {
        image_params params;
        params.bits_allocated = 32;
        params.bits_stored = 32;
        params.samples_per_pixel = 1;

        REQUIRE(params.valid_for_jpeg_lossless() == false);
    }
}
