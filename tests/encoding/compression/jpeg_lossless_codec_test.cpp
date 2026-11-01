#include <catch2/catch_test_macros.hpp>
#include <kcenon/pacs/core/result.h>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "kcenon/pacs/encoding/compression/jpeg_lossless_codec.h"
#include "kcenon/pacs/encoding/compression/codec_factory.h"
#include "kcenon/pacs/encoding/compression/image_params.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <vector>

using namespace kcenon::pacs::encoding::compression;

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

        REQUIRE(encode_result.is_ok() == true);
        REQUIRE(encode_result.value().data.size() > 0);
    }

    SECTION("round-trip is perfectly lossless") {
        auto encode_result = codec.encode(original, params);
        REQUIRE(encode_result.is_ok() == true);

        auto decode_result = codec.decode(encode_result.value().data, params);
        REQUIRE(decode_result.is_ok() == true);
        REQUIRE(decode_result.value().data.size() == original.size());

        // Lossless verification - must be exactly identical
        REQUIRE(images_identical(original, decode_result.value().data));
    }

    SECTION("output params are set correctly") {
        auto encode_result = codec.encode(original, params);
        REQUIRE(encode_result.is_ok() == true);

        auto decode_result = codec.decode(encode_result.value().data, params);
        REQUIRE(decode_result.is_ok() == true);
        REQUIRE(decode_result.value().output_params.width == width);
        REQUIRE(decode_result.value().output_params.height == height);
        REQUIRE(decode_result.value().output_params.samples_per_pixel == 1);
        REQUIRE(decode_result.value().output_params.bits_allocated == 8);
        REQUIRE(decode_result.value().output_params.bits_stored == 8);
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

        REQUIRE(encode_result.is_ok() == true);
        REQUIRE(encode_result.value().data.size() > 0);
    }

    SECTION("round-trip is perfectly lossless") {
        auto encode_result = codec.encode(original, params);
        REQUIRE(encode_result.is_ok() == true);

        auto decode_result = codec.decode(encode_result.value().data, params);
        REQUIRE(decode_result.is_ok() == true);
        REQUIRE(decode_result.value().data.size() == original.size());

        // Lossless verification
        REQUIRE(images_identical(original, decode_result.value().data));
    }

    SECTION("output params reflect 12-bit precision") {
        auto encode_result = codec.encode(original, params);
        REQUIRE(encode_result.is_ok() == true);

        auto decode_result = codec.decode(encode_result.value().data, params);
        REQUIRE(decode_result.is_ok() == true);
        REQUIRE(decode_result.value().output_params.bits_allocated == 16);
        REQUIRE(decode_result.value().output_params.bits_stored == 12);
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
        REQUIRE(encode_result.is_ok() == true);

        auto decode_result = codec.decode(encode_result.value().data, params);
        REQUIRE(decode_result.is_ok() == true);
        REQUIRE(decode_result.value().data.size() == original.size());

        // Lossless verification
        REQUIRE(images_identical(original, decode_result.value().data));
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
        REQUIRE(encode_result.is_ok() == true);

        auto decode_result = codec.decode(encode_result.value().data, params);
        REQUIRE(decode_result.is_ok() == true);

        // Even high-entropy data must be perfectly reconstructed
        REQUIRE(images_identical(original, decode_result.value().data));
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
            REQUIRE(encode_result.is_ok() == true);

            auto decode_result = codec.decode(encode_result.value().data, params);
            REQUIRE(decode_result.is_ok() == true);

            REQUIRE(images_identical(original, decode_result.value().data));
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

        REQUIRE(result.is_ok() == false);
        REQUIRE_FALSE(result.error().message.empty());
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
        REQUIRE_THAT(result.error().message,
                     Catch::Matchers::ContainsSubstring("mismatch"));
    }

    SECTION("empty compressed data returns error") {
        image_params params;
        params.width = 64;
        params.height = 64;

        std::vector<uint8_t> empty_data;
        auto result = codec.decode(empty_data, params);

        REQUIRE(result.is_ok() == false);
    }

    SECTION("invalid JPEG data returns error") {
        image_params params;
        params.width = 64;
        params.height = 64;

        std::vector<uint8_t> invalid_data = {0x00, 0x00, 0x00, 0x00};
        auto result = codec.decode(invalid_data, params);

        REQUIRE(result.is_ok() == false);
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
        kcenon::pacs::encoding::transfer_syntax ts("1.2.840.10008.1.2.4.70");
        auto codec = codec_factory::create(ts);

        REQUIRE(codec != nullptr);
        REQUIRE(codec->is_lossy() == false);
    }

    SECTION("is_supported returns correct values") {
        REQUIRE(codec_factory::is_supported("1.2.840.10008.1.2.4.70") == true);
        REQUIRE(codec_factory::is_supported("1.2.840.10008.1.2.4.50") == true);  // Baseline
        REQUIRE(codec_factory::is_supported("1.2.840.10008.1.2.4.80") == true);  // JPEG-LS Lossless
        REQUIRE(codec_factory::is_supported("1.2.840.10008.1.2.4.81") == true);  // JPEG-LS Near-Lossless
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

namespace {

/// Builds a one-component SV1 stream that assigns its own 4-bit codes to categories 0-8.
std::vector<uint8_t> lossless_stream(uint8_t precision, uint16_t rows, uint16_t columns, uint8_t components,
                                     const std::vector<uint8_t>& dht_body, const std::vector<uint8_t>& scan) {
    std::vector<uint8_t> out{0xFF, 0xD8, 0xFF, 0xC3};
    const uint16_t sof_length = static_cast<uint16_t>(8 + 3 * components);
    out.insert(out.end(), {static_cast<uint8_t>(sof_length >> 8), static_cast<uint8_t>(sof_length), precision,
                           static_cast<uint8_t>(rows >> 8), static_cast<uint8_t>(rows),
                           static_cast<uint8_t>(columns >> 8), static_cast<uint8_t>(columns), components});
    for (uint8_t c = 1; c <= components; ++c) {
        out.insert(out.end(), {c, 0x11, 0x00});
    }
    if (!dht_body.empty()) {
        const uint16_t length = static_cast<uint16_t>(2 + dht_body.size());
        out.insert(out.end(), {0xFF, 0xC4, static_cast<uint8_t>(length >> 8), static_cast<uint8_t>(length)});
        out.insert(out.end(), dht_body.begin(), dht_body.end());
    }
    // SOS: one component, table 0, predictor 1, point transform 0.
    out.insert(out.end(), {0xFF, 0xDA, 0x00, 0x08, 0x01, 0x01, 0x00, 0x01, 0x00, 0x00});
    out.insert(out.end(), scan.begin(), scan.end());
    out.insert(out.end(), {0xFF, 0xD9});
    return out;
}

std::vector<uint8_t> four_bit_table() {
    std::vector<uint8_t> body{0x00};
    for (int length = 1; length <= 16; ++length) {
        body.push_back(length == 4 ? 9 : 0);
    }
    for (uint8_t category = 0; category <= 8; ++category) {
        body.push_back(category);
    }
    return body;
}

image_params gray_params(uint16_t rows, uint16_t columns, uint16_t bits) {
    image_params params;
    params.width = columns;
    params.height = rows;
    params.bits_allocated = bits > 8 ? 16 : 8;
    params.bits_stored = bits;
    params.high_bit = static_cast<uint16_t>(bits - 1);
    params.samples_per_pixel = 1;
    return params;
}

}  // namespace

TEST_CASE("jpeg_lossless_codec decodes with the stream's Huffman table",
          "[encoding][compression][lossless][dht]") {
    // 133 is category 3 (bits 101) from the initial prediction 128; 130 is -3,
    // category 2 (bits 00). With 4-bit codes: 0011 101 0010 00 then 1-bit padding.
    const auto stream = lossless_stream(8, 1, 2, 1, four_bit_table(), {0x3A, 0x47});
    jpeg_lossless_codec codec;
    auto result = codec.decode(stream, gray_params(1, 2, 8));
    REQUIRE(result.is_ok());
    CHECK(result.value().data == std::vector<uint8_t>{133, 130});
}

TEST_CASE("jpeg_lossless_codec treats category 16 as 32768 with no additional bits",
          "[encoding][compression][lossless][dht]") {
    jpeg_lossless_codec codec;

    SECTION("decoding a stream from another encoder") {
        // Category 0 is 00 and 16 is 01: 0 from prediction 32768, then no change.
        std::vector<uint8_t> table{0x00};
        for (int length = 1; length <= 16; ++length) {
            table.push_back(length == 2 ? 2 : 0);
        }
        table.insert(table.end(), {0, 16});
        const auto stream = lossless_stream(16, 1, 2, 1, table, {0x4F});
        auto result = codec.decode(stream, gray_params(1, 2, 16));
        REQUIRE(result.is_ok());
        CHECK(result.value().data == std::vector<uint8_t>{0, 0, 0, 0});
    }

    SECTION("encoding differences of exactly 32768 round-trips") {
        const std::vector<uint8_t> pixels{0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0xFF, 0xFF};
        auto params = gray_params(1, 4, 16);
        auto encoded = codec.encode(pixels, params, {});
        REQUIRE(encoded.is_ok());
        auto decoded = codec.decode(encoded.value().data, params);
        REQUIRE(decoded.is_ok());
        CHECK(decoded.value().data == pixels);
    }
}

TEST_CASE("jpeg_lossless_codec codes samples by Bits Stored",
          "[encoding][compression][lossless]") {
    // A sign-extended 12-bit sample has high bits that are not part of the sample.
    const std::vector<uint8_t> pixels{0x00, 0xF8, 0xFF, 0x07, 0xFF, 0xFF, 0x00, 0x00};
    auto params = gray_params(1, 4, 12);
    params.pixel_representation = 1;
    jpeg_lossless_codec codec;
    auto encoded = codec.encode(pixels, params, {});
    REQUIRE(encoded.is_ok());
    auto decoded = codec.decode(encoded.value().data, params);
    REQUIRE(decoded.is_ok());
    const std::vector<uint8_t> masked{0x00, 0x08, 0xFF, 0x07, 0xFF, 0x0F, 0x00, 0x00};
    CHECK(decoded.value().data == masked);
}

TEST_CASE("jpeg_lossless_codec refuses streams it cannot decode correctly",
          "[encoding][compression][lossless][dht]") {
    jpeg_lossless_codec codec;
    const auto params = gray_params(1, 2, 8);

    SECTION("no Huffman table in the stream") {
        CHECK(codec.decode(lossless_stream(8, 1, 2, 1, {}, {0x3A, 0x47}), params).is_err());
    }
    SECTION("impossible Huffman table") {
        std::vector<uint8_t> table{0x00, 3};
        table.insert(table.end(), 15, 0);
        table.insert(table.end(), {0, 1, 2});
        CHECK(codec.decode(lossless_stream(8, 1, 2, 1, table, {0x3A, 0x47}), params).is_err());
    }
    SECTION("color frame") {
        CHECK(codec.decode(lossless_stream(8, 1, 2, 3, four_bit_table(), {0x3A, 0x47}), {}).is_err());
    }
    SECTION("empty scan") {
        CHECK(codec.decode(lossless_stream(8, 1, 2, 1, four_bit_table(), {}), params).is_err());
    }
    SECTION("scan shorter than the frame") {
        CHECK(codec.decode(lossless_stream(8, 4, 4, 1, four_bit_table(), {0x3A}), gray_params(4, 4, 8)).is_err());
    }
}
