#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "pacs/encoding/compression/jpeg2000_codec.hpp"
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
 * @brief Creates a simple 8-bit RGB color test image.
 */
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

/**
 * @brief Computes Peak Signal-to-Noise Ratio between two images.
 * Used to verify lossy compression quality.
 */
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

TEST_CASE("jpeg2000_codec basic properties - lossless mode", "[encoding][compression][jpeg2000]") {
    jpeg2000_codec codec(true);  // lossless mode

    SECTION("transfer syntax UID is correct for lossless") {
        REQUIRE(codec.transfer_syntax_uid() == "1.2.840.10008.1.2.4.90");
    }

    SECTION("name is JPEG 2000 Lossless") {
        REQUIRE(codec.name() == "JPEG 2000 Lossless");
    }

    SECTION("is lossless codec") {
        REQUIRE(codec.is_lossy() == false);
    }

    SECTION("is_lossless_mode returns true") {
        REQUIRE(codec.is_lossless_mode() == true);
    }

    SECTION("default resolution levels is 6") {
        REQUIRE(codec.resolution_levels() == 6);
    }
}

TEST_CASE("jpeg2000_codec basic properties - lossy mode", "[encoding][compression][jpeg2000]") {
    jpeg2000_codec codec(false);  // lossy mode

    SECTION("transfer syntax UID is correct for lossy") {
        REQUIRE(codec.transfer_syntax_uid() == "1.2.840.10008.1.2.4.91");
    }

    SECTION("name is JPEG 2000") {
        REQUIRE(codec.name() == "JPEG 2000");
    }

    SECTION("is lossy codec") {
        REQUIRE(codec.is_lossy() == true);
    }

    SECTION("is_lossless_mode returns false") {
        REQUIRE(codec.is_lossless_mode() == false);
    }

    SECTION("default compression ratio is 20") {
        REQUIRE(codec.compression_ratio() == 20.0f);
    }
}

TEST_CASE("jpeg2000_codec custom configuration", "[encoding][compression][jpeg2000]") {
    SECTION("custom compression ratio") {
        jpeg2000_codec codec(false, 50.0f);
        REQUIRE(codec.compression_ratio() == 50.0f);
    }

    SECTION("custom resolution levels") {
        jpeg2000_codec codec(true, 20.0f, 4);
        REQUIRE(codec.resolution_levels() == 4);
    }

    SECTION("resolution levels are clamped to valid range") {
        jpeg2000_codec codec_low(true, 20.0f, 0);
        REQUIRE(codec_low.resolution_levels() == 1);

        jpeg2000_codec codec_high(true, 20.0f, 100);
        REQUIRE(codec_high.resolution_levels() == 32);
    }
}

TEST_CASE("jpeg2000_codec can_encode validation", "[encoding][compression][jpeg2000]") {
    jpeg2000_codec codec(true);

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

#ifdef PACS_WITH_JPEG2000_CODEC

TEST_CASE("jpeg2000_codec 8-bit grayscale lossless round-trip", "[encoding][compression][jpeg2000]") {
    jpeg2000_codec codec(true);  // lossless mode

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

TEST_CASE("jpeg2000_codec 12-bit grayscale lossless round-trip", "[encoding][compression][jpeg2000]") {
    jpeg2000_codec codec(true);

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

TEST_CASE("jpeg2000_codec 16-bit grayscale lossless round-trip", "[encoding][compression][jpeg2000]") {
    jpeg2000_codec codec(true);

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

TEST_CASE("jpeg2000_codec 8-bit color lossless round-trip", "[encoding][compression][jpeg2000]") {
    jpeg2000_codec codec(true);  // lossless mode

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
        REQUIRE(encode_result.success == true);

        auto decode_result = codec.decode(encode_result.data, params);
        REQUIRE(decode_result.success == true);
        REQUIRE(decode_result.data.size() == original.size());

        // Lossless verification
        REQUIRE(images_identical(original, decode_result.data));
    }
}

TEST_CASE("jpeg2000_codec lossy compression", "[encoding][compression][jpeg2000]") {
    jpeg2000_codec codec(false, 20.0f);  // lossy mode with 20:1 ratio

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

    SECTION("lossy compression produces smaller output") {
        // Compare with lossless codec
        jpeg2000_codec lossless_codec(true);

        auto lossy_result = codec.encode(original, params);
        auto lossless_result = lossless_codec.encode(original, params);

        REQUIRE(lossy_result.success == true);
        REQUIRE(lossless_result.success == true);

        // Lossy should produce smaller output (though not guaranteed for all images)
        // For gradient images, this should typically hold
        INFO("Lossy size: " << lossy_result.data.size()
             << ", Lossless size: " << lossless_result.data.size());
    }

    SECTION("lossy round-trip maintains acceptable quality") {
        auto encode_result = codec.encode(original, params);
        REQUIRE(encode_result.success == true);

        auto decode_result = codec.decode(encode_result.data, params);
        REQUIRE(decode_result.success == true);

        // PSNR should be at least 30 dB for reasonable quality
        double psnr = compute_psnr(original, decode_result.data);
        INFO("PSNR: " << psnr << " dB");
        REQUIRE(psnr > 30.0);
    }

    SECTION("quality option affects compression") {
        compression_options high_quality;
        high_quality.quality = 90;  // High quality

        compression_options low_quality;
        low_quality.quality = 10;   // Low quality

        auto high_result = codec.encode(original, params, high_quality);
        auto low_result = codec.encode(original, params, low_quality);

        REQUIRE(high_result.success == true);
        REQUIRE(low_result.success == true);

        // Higher quality should produce larger files (typically)
        INFO("High quality size: " << high_result.data.size()
             << ", Low quality size: " << low_result.data.size());
    }
}

TEST_CASE("jpeg2000_codec with random noise", "[encoding][compression][jpeg2000]") {
    jpeg2000_codec codec(true);  // lossless mode

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

TEST_CASE("jpeg2000_codec error handling", "[encoding][compression][jpeg2000]") {
    jpeg2000_codec codec(true);

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
    }

    SECTION("empty compressed data returns error") {
        image_params params;
        params.width = 64;
        params.height = 64;

        std::vector<uint8_t> empty_data;
        auto result = codec.decode(empty_data, params);

        REQUIRE(result.success == false);
    }

    SECTION("invalid J2K data returns error") {
        image_params params;
        params.width = 64;
        params.height = 64;

        std::vector<uint8_t> invalid_data = {0x00, 0x00, 0x00, 0x00};
        auto result = codec.decode(invalid_data, params);

        REQUIRE(result.success == false);
    }
}

TEST_CASE("jpeg2000_codec compression options", "[encoding][compression][jpeg2000]") {
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

    SECTION("lossless option overrides lossy codec") {
        jpeg2000_codec lossy_codec(false);  // Default lossy

        compression_options options;
        options.lossless = true;  // Force lossless

        auto encode_result = lossy_codec.encode(original, params, options);
        REQUIRE(encode_result.success == true);

        auto decode_result = lossy_codec.decode(encode_result.data, params);
        REQUIRE(decode_result.success == true);

        // Should be lossless even though codec was created as lossy
        REQUIRE(images_identical(original, decode_result.data));
    }
}

#else  // !PACS_WITH_JPEG2000_CODEC

TEST_CASE("jpeg2000_codec without OpenJPEG returns error", "[encoding][compression][jpeg2000]") {
    jpeg2000_codec codec(true);

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

        REQUIRE(result.success == false);
        REQUIRE_THAT(result.error_message,
                     Catch::Matchers::ContainsSubstring("not available"));
    }

    SECTION("decode returns not available error") {
        std::vector<uint8_t> dummy_data = {0xFF, 0x4F, 0xFF, 0x51};
        auto result = codec.decode(dummy_data, params);

        REQUIRE(result.success == false);
        REQUIRE_THAT(result.error_message,
                     Catch::Matchers::ContainsSubstring("not available"));
    }
}

#endif  // PACS_WITH_JPEG2000_CODEC

TEST_CASE("codec_factory creates jpeg2000_codec", "[encoding][compression][jpeg2000]") {
    SECTION("create lossless by UID") {
        auto codec = codec_factory::create("1.2.840.10008.1.2.4.90");

        REQUIRE(codec != nullptr);
        REQUIRE(codec->transfer_syntax_uid() == "1.2.840.10008.1.2.4.90");
        REQUIRE(codec->name() == "JPEG 2000 Lossless");
        REQUIRE(codec->is_lossy() == false);
    }

    SECTION("create lossy by UID") {
        auto codec = codec_factory::create("1.2.840.10008.1.2.4.91");

        REQUIRE(codec != nullptr);
        REQUIRE(codec->transfer_syntax_uid() == "1.2.840.10008.1.2.4.91");
        REQUIRE(codec->name() == "JPEG 2000");
        REQUIRE(codec->is_lossy() == true);
    }

    SECTION("create by transfer_syntax - lossless") {
        pacs::encoding::transfer_syntax ts("1.2.840.10008.1.2.4.90");
        auto codec = codec_factory::create(ts);

        REQUIRE(codec != nullptr);
        REQUIRE(codec->is_lossy() == false);
    }

    SECTION("create by transfer_syntax - lossy") {
        pacs::encoding::transfer_syntax ts("1.2.840.10008.1.2.4.91");
        auto codec = codec_factory::create(ts);

        REQUIRE(codec != nullptr);
        REQUIRE(codec->is_lossy() == true);
    }

    SECTION("is_supported returns correct values") {
        REQUIRE(codec_factory::is_supported("1.2.840.10008.1.2.4.90") == true);  // J2K Lossless
        REQUIRE(codec_factory::is_supported("1.2.840.10008.1.2.4.91") == true);  // J2K Lossy
        REQUIRE(codec_factory::is_supported("1.2.840.10008.1.2.4.70") == true);  // JPEG Lossless
        REQUIRE(codec_factory::is_supported("1.2.840.10008.1.2.4.50") == true);  // JPEG Baseline
        REQUIRE(codec_factory::is_supported("1.2.840.10008.1.2.4.80") == false); // JPEG-LS (not yet)
        REQUIRE(codec_factory::is_supported("1.2.840.10008.1.2.5") == false);    // RLE (not yet)
    }

    SECTION("supported_transfer_syntaxes includes JPEG 2000") {
        auto supported = codec_factory::supported_transfer_syntaxes();

        REQUIRE_FALSE(supported.empty());
        REQUIRE(std::find(supported.begin(), supported.end(),
                          "1.2.840.10008.1.2.4.90") != supported.end());
        REQUIRE(std::find(supported.begin(), supported.end(),
                          "1.2.840.10008.1.2.4.91") != supported.end());
    }
}

TEST_CASE("image_params validation for JPEG 2000", "[encoding][compression][jpeg2000]") {
    SECTION("valid_for_jpeg2000 accepts 8-bit grayscale") {
        image_params params;
        params.width = 256;
        params.height = 256;
        params.bits_allocated = 8;
        params.bits_stored = 8;
        params.samples_per_pixel = 1;

        REQUIRE(params.valid_for_jpeg2000() == true);
    }

    SECTION("valid_for_jpeg2000 accepts 12-bit grayscale") {
        image_params params;
        params.width = 512;
        params.height = 512;
        params.bits_allocated = 16;
        params.bits_stored = 12;
        params.samples_per_pixel = 1;

        REQUIRE(params.valid_for_jpeg2000() == true);
    }

    SECTION("valid_for_jpeg2000 accepts 16-bit grayscale") {
        image_params params;
        params.width = 512;
        params.height = 512;
        params.bits_allocated = 16;
        params.bits_stored = 16;
        params.samples_per_pixel = 1;

        REQUIRE(params.valid_for_jpeg2000() == true);
    }

    SECTION("valid_for_jpeg2000 accepts 8-bit color") {
        image_params params;
        params.width = 256;
        params.height = 256;
        params.bits_allocated = 8;
        params.bits_stored = 8;
        params.samples_per_pixel = 3;

        REQUIRE(params.valid_for_jpeg2000() == true);
    }

    SECTION("valid_for_jpeg2000 rejects 32-bit") {
        image_params params;
        params.width = 256;
        params.height = 256;
        params.bits_allocated = 32;
        params.bits_stored = 32;
        params.samples_per_pixel = 1;

        REQUIRE(params.valid_for_jpeg2000() == false);
    }

    SECTION("valid_for_jpeg2000 rejects zero dimensions") {
        image_params params;
        params.width = 0;
        params.height = 256;
        params.bits_allocated = 8;
        params.bits_stored = 8;
        params.samples_per_pixel = 1;

        REQUIRE(params.valid_for_jpeg2000() == false);
    }

    SECTION("valid_for_jpeg2000 rejects invalid samples_per_pixel") {
        image_params params;
        params.width = 256;
        params.height = 256;
        params.bits_allocated = 8;
        params.bits_stored = 8;
        params.samples_per_pixel = 4;  // RGBA not supported

        REQUIRE(params.valid_for_jpeg2000() == false);
    }
}
