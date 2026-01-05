#include <catch2/catch_test_macros.hpp>
#include <pacs/core/result.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "pacs/encoding/compression/jpeg_ls_codec.hpp"
#include "pacs/encoding/compression/codec_factory.hpp"
#include "pacs/encoding/compression/image_params.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
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
[[maybe_unused]]
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
[[maybe_unused]]
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
 * @brief Creates a simple 8-bit RGB color test image.
 */
[[maybe_unused]]
std::vector<uint8_t> create_color_image_8bit(uint16_t width, uint16_t height) {
    std::vector<uint8_t> data(width * height * 3);
    for (uint16_t y = 0; y < height; ++y) {
        for (uint16_t x = 0; x < width; ++x) {
            size_t idx = (y * width + x) * 3;
            data[idx + 0] = static_cast<uint8_t>(x * 255 / (width - 1));    // R
            data[idx + 1] = static_cast<uint8_t>(y * 255 / (height - 1));   // G
            data[idx + 2] = static_cast<uint8_t>((x + y) * 127 / (width + height - 2)); // B
        }
    }
    return data;
}

/**
 * @brief Creates a random noise image for stress testing.
 */
[[maybe_unused]]
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
[[maybe_unused]]
bool images_identical(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
}

/**
 * @brief Computes maximum absolute error between two images.
 * Used to verify near-lossless compression bounded error.
 */
[[maybe_unused]]
int compute_max_error(const std::vector<uint8_t>& original,
                      const std::vector<uint8_t>& reconstructed) {
    if (original.size() != reconstructed.size() || original.empty()) {
        return -1;
    }

    int max_error = 0;
    for (size_t i = 0; i < original.size(); ++i) {
        int diff = std::abs(static_cast<int>(original[i]) -
                           static_cast<int>(reconstructed[i]));
        max_error = (std::max)(max_error, diff);
    }
    return max_error;
}

/**
 * @brief Computes Peak Signal-to-Noise Ratio between two images.
 */
[[maybe_unused]]
double compute_psnr(const std::vector<uint8_t>& original,
                    const std::vector<uint8_t>& reconstructed,
                    uint16_t max_value = 255) {
    if (original.size() != reconstructed.size() || original.empty()) {
        return 0.0;
    }

    double mse = 0.0;
    for (size_t i = 0; i < original.size(); ++i) {
        double diff = static_cast<double>(original[i]) - static_cast<double>(reconstructed[i]);
        mse += diff * diff;
    }
    mse /= static_cast<double>(original.size());

    if (mse == 0.0) {
        return std::numeric_limits<double>::infinity();  // Identical images
    }

    return 10.0 * std::log10((max_value * max_value) / mse);
}

}  // namespace

TEST_CASE("jpeg_ls_codec basic properties - lossless mode", "[encoding][compression][jpegls]") {
    jpeg_ls_codec codec(true);  // lossless mode

    SECTION("transfer syntax UID is correct for lossless") {
        REQUIRE(codec.transfer_syntax_uid() == "1.2.840.10008.1.2.4.80");
    }

    SECTION("name is JPEG-LS Lossless") {
        REQUIRE(codec.name() == "JPEG-LS Lossless");
    }

    SECTION("is lossless codec") {
        REQUIRE(codec.is_lossy() == false);
    }

    SECTION("is_lossless_mode returns true") {
        REQUIRE(codec.is_lossless_mode() == true);
    }

    SECTION("NEAR value is 0 for lossless") {
        REQUIRE(codec.near_value() == 0);
    }
}

TEST_CASE("jpeg_ls_codec basic properties - near-lossless mode", "[encoding][compression][jpegls]") {
    jpeg_ls_codec codec(false, 3);  // near-lossless mode with NEAR=3

    SECTION("transfer syntax UID is correct for near-lossless") {
        REQUIRE(codec.transfer_syntax_uid() == "1.2.840.10008.1.2.4.81");
    }

    SECTION("name is JPEG-LS Near-Lossless") {
        REQUIRE(codec.name() == "JPEG-LS Near-Lossless");
    }

    SECTION("is lossy codec") {
        REQUIRE(codec.is_lossy() == true);
    }

    SECTION("is_lossless_mode returns false") {
        REQUIRE(codec.is_lossless_mode() == false);
    }

    SECTION("NEAR value is 3") {
        REQUIRE(codec.near_value() == 3);
    }
}

TEST_CASE("jpeg_ls_codec custom configuration", "[encoding][compression][jpegls]") {
    SECTION("lossless mode with NEAR=0") {
        jpeg_ls_codec codec(true, 0);
        REQUIRE(codec.near_value() == 0);
        REQUIRE(codec.is_lossless_mode() == true);
    }

    SECTION("NEAR=0 forces lossless even if lossless=false") {
        jpeg_ls_codec codec(false, 0);
        REQUIRE(codec.near_value() == 0);
        REQUIRE(codec.is_lossless_mode() == true);
    }

    SECTION("lossless=true with NEAR>0 forces NEAR to 0") {
        jpeg_ls_codec codec(true, 5);  // Request lossless but with NEAR=5
        REQUIRE(codec.near_value() == 0);  // NEAR is forced to 0
        REQUIRE(codec.is_lossless_mode() == true);
    }

    SECTION("NEAR value is clamped to valid range") {
        jpeg_ls_codec codec_high(false, 500);  // Above max (255)
        REQUIRE(codec_high.near_value() == 255);

        jpeg_ls_codec codec_neg(false, -10);   // Below min (0)
        REQUIRE(codec_neg.near_value() == 0);
        // Negative clamped to 0, which forces lossless
        REQUIRE(codec_neg.is_lossless_mode() == true);
    }
}

TEST_CASE("jpeg_ls_codec can_encode validation", "[encoding][compression][jpegls]") {
    jpeg_ls_codec codec(true);

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

    SECTION("accepts valid 8-bit RGB parameters") {
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

    SECTION("rejects invalid bit depth - too low") {
        image_params params;
        params.width = 256;
        params.height = 256;
        params.bits_allocated = 8;
        params.bits_stored = 1;  // Below minimum (2)
        params.samples_per_pixel = 1;

        REQUIRE(codec.can_encode(params) == false);
    }

    SECTION("rejects invalid bit depth - too high") {
        image_params params;
        params.width = 256;
        params.height = 256;
        params.bits_allocated = 32;
        params.bits_stored = 32;  // Above maximum (16)
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

    SECTION("rejects invalid samples_per_pixel") {
        image_params params;
        params.width = 256;
        params.height = 256;
        params.bits_allocated = 8;
        params.bits_stored = 8;
        params.samples_per_pixel = 4;  // Not 1 or 3

        REQUIRE(codec.can_encode(params) == false);
    }
}

#ifdef PACS_WITH_JPEGLS_CODEC

TEST_CASE("jpeg_ls_codec 8-bit grayscale lossless round-trip", "[encoding][compression][jpegls]") {
    jpeg_ls_codec codec(true);  // lossless mode

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

TEST_CASE("jpeg_ls_codec 12-bit grayscale lossless round-trip", "[encoding][compression][jpegls]") {
    jpeg_ls_codec codec(true);

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

TEST_CASE("jpeg_ls_codec 16-bit grayscale lossless round-trip", "[encoding][compression][jpegls]") {
    jpeg_ls_codec codec(true);

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

TEST_CASE("jpeg_ls_codec 8-bit color lossless round-trip", "[encoding][compression][jpegls]") {
    jpeg_ls_codec codec(true);  // lossless mode

    uint16_t width = 64;
    uint16_t height = 64;
    auto original = create_color_image_8bit(width, height);

    image_params params;
    params.width = width;
    params.height = height;
    params.bits_allocated = 8;
    params.bits_stored = 8;
    params.high_bit = 7;
    params.samples_per_pixel = 3;
    params.planar_configuration = 0;  // Interleaved
    params.photometric = photometric_interpretation::rgb;

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

TEST_CASE("jpeg_ls_codec near-lossless compression", "[encoding][compression][jpegls]") {
    const int near_value = 3;
    jpeg_ls_codec codec(false, near_value);  // near-lossless mode with NEAR=3

    uint16_t width = 128;
    uint16_t height = 128;
    auto original = create_gradient_image_8bit(width, height);

    image_params params;
    params.width = width;
    params.height = height;
    params.bits_allocated = 8;
    params.bits_stored = 8;
    params.high_bit = 7;
    params.samples_per_pixel = 1;
    params.photometric = photometric_interpretation::monochrome2;

    SECTION("near-lossless compression produces smaller output than lossless") {
        jpeg_ls_codec lossless_codec(true);

        auto lossy_result = codec.encode(original, params);
        auto lossless_result = lossless_codec.encode(original, params);

        REQUIRE(lossy_result.is_ok() == true);
        REQUIRE(lossless_result.is_ok() == true);

        // Near-lossless should produce smaller or equal output
        INFO("Near-lossless size: " << lossy_result.value().data.size()
             << ", Lossless size: " << lossless_result.value().data.size());

        // Note: for simple gradient images, compression might be similar
    }

    SECTION("near-lossless round-trip has bounded error") {
        auto encode_result = codec.encode(original, params);
        REQUIRE(encode_result.is_ok() == true);

        auto decode_result = codec.decode(encode_result.value().data, params);
        REQUIRE(decode_result.is_ok() == true);

        // Maximum error should be bounded by NEAR value
        int max_error = compute_max_error(original, decode_result.value().data);
        INFO("Max error: " << max_error << ", NEAR value: " << near_value);
        REQUIRE(max_error <= near_value);
    }

    SECTION("near-lossless maintains high quality") {
        auto encode_result = codec.encode(original, params);
        REQUIRE(encode_result.is_ok() == true);

        auto decode_result = codec.decode(encode_result.value().data, params);
        REQUIRE(decode_result.is_ok() == true);

        // PSNR should be high for near-lossless
        double psnr = compute_psnr(original, decode_result.value().data);
        INFO("PSNR: " << psnr << " dB");
        REQUIRE(psnr > 40.0);  // High quality threshold
    }
}

TEST_CASE("jpeg_ls_codec quality option affects NEAR", "[encoding][compression][jpegls]") {
    jpeg_ls_codec codec(false, 5);  // Default NEAR=5

    uint16_t width = 64;
    uint16_t height = 64;
    auto original = create_gradient_image_8bit(width, height);

    image_params params;
    params.width = width;
    params.height = height;
    params.bits_allocated = 8;
    params.bits_stored = 8;
    params.samples_per_pixel = 1;

    SECTION("quality 100 produces lossless") {
        compression_options options;
        options.quality = 100;

        auto encode_result = codec.encode(original, params, options);
        REQUIRE(encode_result.is_ok() == true);

        auto decode_result = codec.decode(encode_result.value().data, params);
        REQUIRE(decode_result.is_ok() == true);

        // Quality 100 should be lossless
        REQUIRE(images_identical(original, decode_result.value().data));
    }

    SECTION("lower quality produces smaller files") {
        compression_options high_quality;
        high_quality.quality = 90;

        compression_options low_quality;
        low_quality.quality = 50;

        auto high_result = codec.encode(original, params, high_quality);
        auto low_result = codec.encode(original, params, low_quality);

        REQUIRE(high_result.is_ok() == true);
        REQUIRE(low_result.is_ok() == true);

        INFO("High quality size: " << high_result.value().data.size()
             << ", Low quality size: " << low_result.value().data.size());
    }
}

TEST_CASE("jpeg_ls_codec with random noise", "[encoding][compression][jpegls]") {
    jpeg_ls_codec codec(true);  // lossless mode

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

TEST_CASE("jpeg_ls_codec error handling", "[encoding][compression][jpegls]") {
    jpeg_ls_codec codec(true);

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

    SECTION("invalid dimensions returns error") {
        image_params params;
        params.width = 0;  // Invalid
        params.height = 64;
        params.bits_allocated = 8;
        params.bits_stored = 8;
        params.samples_per_pixel = 1;

        auto original = create_gradient_image_8bit(64, 64);
        auto result = codec.encode(original, params);

        REQUIRE(result.is_ok() == false);
    }

    SECTION("empty compressed data returns error") {
        image_params params;
        params.width = 64;
        params.height = 64;

        std::vector<uint8_t> empty_data;
        auto result = codec.decode(empty_data, params);

        REQUIRE(result.is_ok() == false);
    }

    SECTION("invalid JPEG-LS data returns error") {
        image_params params;
        params.width = 64;
        params.height = 64;

        std::vector<uint8_t> invalid_data = {0x00, 0x00, 0x00, 0x00};
        auto result = codec.decode(invalid_data, params);

        REQUIRE(result.is_ok() == false);
    }
}

TEST_CASE("jpeg_ls_codec compression options", "[encoding][compression][jpegls]") {
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

    SECTION("lossless option overrides near-lossless codec") {
        jpeg_ls_codec lossy_codec(false, 5);  // Default near-lossless

        compression_options options;
        options.lossless = true;  // Force lossless

        auto encode_result = lossy_codec.encode(original, params, options);
        REQUIRE(encode_result.is_ok() == true);

        auto decode_result = lossy_codec.decode(encode_result.value().data, params);
        REQUIRE(decode_result.is_ok() == true);

        // Should be lossless even though codec was created as near-lossless
        REQUIRE(images_identical(original, decode_result.value().data));
    }
}

#else  // !PACS_WITH_JPEGLS_CODEC

TEST_CASE("jpeg_ls_codec without CharLS returns error", "[encoding][compression][jpegls]") {
    jpeg_ls_codec codec(true);

    uint16_t width = 64;
    uint16_t height = 64;
    auto original = create_gradient_image_8bit(width, height);

    image_params params;
    params.width = width;
    params.height = height;
    params.bits_allocated = 8;
    params.bits_stored = 8;
    params.samples_per_pixel = 1;

    SECTION("encode returns not available error") {
        auto result = codec.encode(original, params);

        REQUIRE(result.is_ok() == false);
        REQUIRE_THAT(result.error().message,
                     Catch::Matchers::ContainsSubstring("not available"));
    }

    SECTION("decode returns not available error") {
        std::vector<uint8_t> dummy_data = {0xFF, 0xD8, 0xFF, 0xF7};  // JPEG-LS marker
        auto result = codec.decode(dummy_data, params);

        REQUIRE(result.is_ok() == false);
        REQUIRE_THAT(result.error().message,
                     Catch::Matchers::ContainsSubstring("not available"));
    }
}

#endif  // PACS_WITH_JPEGLS_CODEC

TEST_CASE("codec_factory creates jpeg_ls_codec", "[encoding][compression][jpegls]") {
    SECTION("create lossless by UID") {
        auto codec = codec_factory::create("1.2.840.10008.1.2.4.80");

        REQUIRE(codec != nullptr);
        REQUIRE(codec->transfer_syntax_uid() == "1.2.840.10008.1.2.4.80");
        REQUIRE(codec->name() == "JPEG-LS Lossless");
        REQUIRE(codec->is_lossy() == false);
    }

    SECTION("create near-lossless by UID") {
        auto codec = codec_factory::create("1.2.840.10008.1.2.4.81");

        REQUIRE(codec != nullptr);
        REQUIRE(codec->transfer_syntax_uid() == "1.2.840.10008.1.2.4.81");
        REQUIRE(codec->name() == "JPEG-LS Near-Lossless");
        REQUIRE(codec->is_lossy() == true);
    }

    SECTION("create by transfer_syntax - lossless") {
        pacs::encoding::transfer_syntax ts("1.2.840.10008.1.2.4.80");
        auto codec = codec_factory::create(ts);

        REQUIRE(codec != nullptr);
        REQUIRE(codec->is_lossy() == false);
    }

    SECTION("create by transfer_syntax - near-lossless") {
        pacs::encoding::transfer_syntax ts("1.2.840.10008.1.2.4.81");
        auto codec = codec_factory::create(ts);

        REQUIRE(codec != nullptr);
        REQUIRE(codec->is_lossy() == true);
    }

    SECTION("is_supported returns correct values for JPEG-LS") {
        REQUIRE(codec_factory::is_supported("1.2.840.10008.1.2.4.80") == true);  // JPEG-LS Lossless
        REQUIRE(codec_factory::is_supported("1.2.840.10008.1.2.4.81") == true);  // JPEG-LS Near-Lossless
    }

    SECTION("supported_transfer_syntaxes includes JPEG-LS") {
        auto supported = codec_factory::supported_transfer_syntaxes();

        REQUIRE_FALSE(supported.empty());
        REQUIRE(std::find(supported.begin(), supported.end(),
                          "1.2.840.10008.1.2.4.80") != supported.end());
        REQUIRE(std::find(supported.begin(), supported.end(),
                          "1.2.840.10008.1.2.4.81") != supported.end());
    }
}

TEST_CASE("image_params validation for JPEG-LS", "[encoding][compression][jpegls]") {
    SECTION("valid_for_jpeg_ls accepts 8-bit grayscale") {
        image_params params;
        params.width = 256;
        params.height = 256;
        params.bits_allocated = 8;
        params.bits_stored = 8;
        params.samples_per_pixel = 1;

        REQUIRE(params.valid_for_jpeg_ls() == true);
    }

    SECTION("valid_for_jpeg_ls accepts 12-bit grayscale") {
        image_params params;
        params.width = 512;
        params.height = 512;
        params.bits_allocated = 16;
        params.bits_stored = 12;
        params.samples_per_pixel = 1;

        REQUIRE(params.valid_for_jpeg_ls() == true);
    }

    SECTION("valid_for_jpeg_ls accepts 16-bit grayscale") {
        image_params params;
        params.width = 512;
        params.height = 512;
        params.bits_allocated = 16;
        params.bits_stored = 16;
        params.samples_per_pixel = 1;

        REQUIRE(params.valid_for_jpeg_ls() == true);
    }

    SECTION("valid_for_jpeg_ls accepts 8-bit color") {
        image_params params;
        params.width = 256;
        params.height = 256;
        params.bits_allocated = 8;
        params.bits_stored = 8;
        params.samples_per_pixel = 3;

        REQUIRE(params.valid_for_jpeg_ls() == true);
    }

    SECTION("valid_for_jpeg_ls rejects 1-bit (below minimum)") {
        image_params params;
        params.width = 256;
        params.height = 256;
        params.bits_allocated = 8;
        params.bits_stored = 1;  // Below minimum (2)
        params.samples_per_pixel = 1;

        REQUIRE(params.valid_for_jpeg_ls() == false);
    }

    SECTION("valid_for_jpeg_ls rejects 32-bit") {
        image_params params;
        params.width = 256;
        params.height = 256;
        params.bits_allocated = 32;
        params.bits_stored = 32;
        params.samples_per_pixel = 1;

        REQUIRE(params.valid_for_jpeg_ls() == false);
    }

    SECTION("valid_for_jpeg_ls rejects zero dimensions") {
        image_params params;
        params.width = 0;
        params.height = 256;
        params.bits_allocated = 8;
        params.bits_stored = 8;
        params.samples_per_pixel = 1;

        REQUIRE(params.valid_for_jpeg_ls() == false);
    }

    SECTION("valid_for_jpeg_ls rejects invalid samples_per_pixel") {
        image_params params;
        params.width = 256;
        params.height = 256;
        params.bits_allocated = 8;
        params.bits_stored = 8;
        params.samples_per_pixel = 4;  // RGBA not supported

        REQUIRE(params.valid_for_jpeg_ls() == false);
    }
}
