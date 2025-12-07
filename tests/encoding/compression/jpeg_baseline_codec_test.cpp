#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "pacs/encoding/compression/jpeg_baseline_codec.hpp"
#include "pacs/encoding/compression/codec_factory.hpp"
#include "pacs/encoding/compression/image_params.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

using namespace pacs::encoding::compression;

namespace {

/**
 * @brief Creates a simple grayscale gradient test image.
 */
std::vector<uint8_t> create_gradient_image(uint16_t width, uint16_t height) {
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
 * @brief Creates a simple RGB color test image.
 */
std::vector<uint8_t> create_color_image(uint16_t width, uint16_t height) {
    std::vector<uint8_t> data(width * height * 3);
    for (uint16_t y = 0; y < height; ++y) {
        for (uint16_t x = 0; x < width; ++x) {
            size_t idx = (y * width + x) * 3;
            data[idx + 0] = static_cast<uint8_t>(x * 255 / (width - 1));   // R
            data[idx + 1] = static_cast<uint8_t>(y * 255 / (height - 1));  // G
            data[idx + 2] = 128;                                           // B
        }
    }
    return data;
}

/**
 * @brief Calculates PSNR between original and reconstructed images.
 */
double calculate_psnr(const std::vector<uint8_t>& original,
                      const std::vector<uint8_t>& reconstructed) {
    if (original.size() != reconstructed.size() || original.empty()) {
        return 0.0;
    }

    double mse = 0.0;
    for (size_t i = 0; i < original.size(); ++i) {
        double diff = static_cast<double>(original[i]) - reconstructed[i];
        mse += diff * diff;
    }
    mse /= original.size();

    if (mse == 0.0) {
        return std::numeric_limits<double>::infinity();
    }

    return 10.0 * std::log10(255.0 * 255.0 / mse);
}

}  // namespace

TEST_CASE("jpeg_baseline_codec basic properties", "[encoding][compression]") {
    jpeg_baseline_codec codec;

    SECTION("transfer syntax UID is correct") {
        REQUIRE(codec.transfer_syntax_uid() == "1.2.840.10008.1.2.4.50");
    }

    SECTION("name is JPEG Baseline") {
        REQUIRE(codec.name() == "JPEG Baseline (Process 1)");
    }

    SECTION("is lossy codec") {
        REQUIRE(codec.is_lossy() == true);
    }
}

TEST_CASE("jpeg_baseline_codec can_encode validation", "[encoding][compression]") {
    jpeg_baseline_codec codec;

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

    SECTION("accepts valid 8-bit RGB parameters") {
        image_params params;
        params.width = 256;
        params.height = 256;
        params.bits_allocated = 8;
        params.bits_stored = 8;
        params.high_bit = 7;
        params.samples_per_pixel = 3;

        REQUIRE(codec.can_encode(params) == true);
    }

    SECTION("rejects 16-bit parameters") {
        image_params params;
        params.width = 256;
        params.height = 256;
        params.bits_allocated = 16;
        params.bits_stored = 12;
        params.high_bit = 11;
        params.samples_per_pixel = 1;

        REQUIRE(codec.can_encode(params) == false);
    }

    SECTION("rejects invalid samples per pixel") {
        image_params params;
        params.width = 256;
        params.height = 256;
        params.bits_allocated = 8;
        params.bits_stored = 8;
        params.high_bit = 7;
        params.samples_per_pixel = 4;  // JPEG doesn't support 4 components natively

        REQUIRE(codec.can_encode(params) == false);
    }
}

TEST_CASE("jpeg_baseline_codec grayscale round-trip", "[encoding][compression]") {
    jpeg_baseline_codec codec;

    uint16_t width = 64;
    uint16_t height = 64;
    auto original = create_gradient_image(width, height);

    image_params params;
    params.width = width;
    params.height = height;
    params.bits_allocated = 8;
    params.bits_stored = 8;
    params.high_bit = 7;
    params.samples_per_pixel = 1;
    params.photometric = photometric_interpretation::monochrome2;

    SECTION("encode succeeds") {
        compression_options options;
        options.quality = 90;

        auto encode_result = codec.encode(original, params, options);

        REQUIRE(encode_result.success == true);
        REQUIRE(encode_result.data.size() > 0);
        REQUIRE(encode_result.data.size() < original.size());  // Should compress
    }

    SECTION("round-trip maintains quality") {
        compression_options options;
        options.quality = 95;

        auto encode_result = codec.encode(original, params, options);
        REQUIRE(encode_result.success == true);

        auto decode_result = codec.decode(encode_result.data, params);
        REQUIRE(decode_result.success == true);
        REQUIRE(decode_result.data.size() == original.size());

        // Calculate PSNR - should be high for quality 95
        double psnr = calculate_psnr(original, decode_result.data);
        REQUIRE(psnr > 30.0);  // 30 dB is generally considered good quality
    }

    SECTION("output params are set correctly") {
        compression_options options;
        auto encode_result = codec.encode(original, params, options);
        REQUIRE(encode_result.success == true);

        auto decode_result = codec.decode(encode_result.data, params);
        REQUIRE(decode_result.success == true);
        REQUIRE(decode_result.output_params.width == width);
        REQUIRE(decode_result.output_params.height == height);
        REQUIRE(decode_result.output_params.samples_per_pixel == 1);
        REQUIRE(decode_result.output_params.bits_allocated == 8);
    }
}

TEST_CASE("jpeg_baseline_codec color round-trip", "[encoding][compression]") {
    jpeg_baseline_codec codec;

    uint16_t width = 64;
    uint16_t height = 64;
    auto original = create_color_image(width, height);

    image_params params;
    params.width = width;
    params.height = height;
    params.bits_allocated = 8;
    params.bits_stored = 8;
    params.high_bit = 7;
    params.samples_per_pixel = 3;
    params.photometric = photometric_interpretation::rgb;

    SECTION("encode succeeds") {
        compression_options options;
        options.quality = 90;

        auto encode_result = codec.encode(original, params, options);

        REQUIRE(encode_result.success == true);
        REQUIRE(encode_result.data.size() > 0);
    }

    SECTION("round-trip maintains quality") {
        compression_options options;
        options.quality = 95;
        options.chroma_subsampling = 0;  // 4:4:4 for best quality

        auto encode_result = codec.encode(original, params, options);
        REQUIRE(encode_result.success == true);

        auto decode_result = codec.decode(encode_result.data, params);
        REQUIRE(decode_result.success == true);
        REQUIRE(decode_result.data.size() == original.size());

        double psnr = calculate_psnr(original, decode_result.data);
        REQUIRE(psnr > 25.0);  // Color compression typically has lower PSNR
    }

    SECTION("output params are set correctly for color") {
        compression_options options;
        auto encode_result = codec.encode(original, params, options);
        REQUIRE(encode_result.success == true);

        auto decode_result = codec.decode(encode_result.data, params);
        REQUIRE(decode_result.success == true);
        REQUIRE(decode_result.output_params.samples_per_pixel == 3);
        REQUIRE(decode_result.output_params.photometric == photometric_interpretation::rgb);
    }
}

TEST_CASE("jpeg_baseline_codec quality settings", "[encoding][compression]") {
    jpeg_baseline_codec codec;

    uint16_t width = 64;
    uint16_t height = 64;
    auto original = create_gradient_image(width, height);

    image_params params;
    params.width = width;
    params.height = height;
    params.bits_allocated = 8;
    params.bits_stored = 8;
    params.high_bit = 7;
    params.samples_per_pixel = 1;

    SECTION("lower quality produces smaller files") {
        compression_options high_quality;
        high_quality.quality = 95;

        compression_options low_quality;
        low_quality.quality = 25;

        auto high_result = codec.encode(original, params, high_quality);
        auto low_result = codec.encode(original, params, low_quality);

        REQUIRE(high_result.success == true);
        REQUIRE(low_result.success == true);
        REQUIRE(low_result.data.size() < high_result.data.size());
    }

    SECTION("quality is clamped to valid range") {
        compression_options invalid_quality;
        invalid_quality.quality = 200;  // Invalid, should be clamped to 100

        auto result = codec.encode(original, params, invalid_quality);
        REQUIRE(result.success == true);
    }
}

TEST_CASE("jpeg_baseline_codec error handling", "[encoding][compression]") {
    jpeg_baseline_codec codec;

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

        std::vector<uint8_t> invalid_data = {0xFF, 0xD8, 0xFF, 0x00};  // Truncated JPEG
        auto result = codec.decode(invalid_data, params);

        REQUIRE(result.success == false);
    }
}

TEST_CASE("codec_factory creates jpeg_baseline_codec", "[encoding][compression]") {
    SECTION("create by UID") {
        auto codec = codec_factory::create("1.2.840.10008.1.2.4.50");

        REQUIRE(codec != nullptr);
        REQUIRE(codec->transfer_syntax_uid() == "1.2.840.10008.1.2.4.50");
        REQUIRE(codec->name() == "JPEG Baseline (Process 1)");
    }

    SECTION("create by transfer_syntax") {
        pacs::encoding::transfer_syntax ts("1.2.840.10008.1.2.4.50");
        auto codec = codec_factory::create(ts);

        REQUIRE(codec != nullptr);
        REQUIRE(codec->is_lossy() == true);
    }

    SECTION("unsupported UID returns nullptr") {
        // Use JPEG-LS UID which is not yet implemented
        auto codec = codec_factory::create("1.2.840.10008.1.2.4.80");  // JPEG-LS Lossless
        REQUIRE(codec == nullptr);
    }

    SECTION("is_supported returns correct values") {
        REQUIRE(codec_factory::is_supported("1.2.840.10008.1.2.4.50") == true);   // JPEG Baseline
        REQUIRE(codec_factory::is_supported("1.2.840.10008.1.2.4.70") == true);   // JPEG Lossless
        REQUIRE(codec_factory::is_supported("1.2.840.10008.1.2.4.90") == true);   // JPEG 2000 Lossless
        REQUIRE(codec_factory::is_supported("1.2.840.10008.1.2.4.91") == true);   // JPEG 2000
        REQUIRE(codec_factory::is_supported("1.2.840.10008.1.2.4.80") == false);  // JPEG-LS (not implemented)
        REQUIRE(codec_factory::is_supported("invalid.uid") == false);
    }

    SECTION("supported_transfer_syntaxes includes JPEG Baseline") {
        auto supported = codec_factory::supported_transfer_syntaxes();

        REQUIRE_FALSE(supported.empty());
        REQUIRE(std::find(supported.begin(), supported.end(),
                          "1.2.840.10008.1.2.4.50") != supported.end());
    }
}

TEST_CASE("image_params validation", "[encoding][compression]") {
    SECTION("valid_for_jpeg_baseline accepts 8-bit") {
        image_params params;
        params.bits_allocated = 8;
        params.bits_stored = 8;
        params.samples_per_pixel = 1;

        REQUIRE(params.valid_for_jpeg_baseline() == true);
    }

    SECTION("valid_for_jpeg_baseline rejects 16-bit") {
        image_params params;
        params.bits_allocated = 16;
        params.bits_stored = 12;
        params.samples_per_pixel = 1;

        REQUIRE(params.valid_for_jpeg_baseline() == false);
    }

    SECTION("frame_size_bytes calculates correctly") {
        image_params params;
        params.width = 512;
        params.height = 512;
        params.bits_allocated = 8;
        params.samples_per_pixel = 1;

        REQUIRE(params.frame_size_bytes() == 512 * 512);

        params.samples_per_pixel = 3;
        REQUIRE(params.frame_size_bytes() == 512 * 512 * 3);
    }

    SECTION("is_grayscale and is_color") {
        image_params params;
        params.samples_per_pixel = 1;
        REQUIRE(params.is_grayscale() == true);
        REQUIRE(params.is_color() == false);

        params.samples_per_pixel = 3;
        REQUIRE(params.is_grayscale() == false);
        REQUIRE(params.is_color() == true);
    }
}

TEST_CASE("photometric_interpretation conversion", "[encoding][compression]") {
    SECTION("to_string conversions") {
        REQUIRE(to_string(photometric_interpretation::monochrome1) == "MONOCHROME1");
        REQUIRE(to_string(photometric_interpretation::monochrome2) == "MONOCHROME2");
        REQUIRE(to_string(photometric_interpretation::rgb) == "RGB");
        REQUIRE(to_string(photometric_interpretation::ycbcr_full) == "YBR_FULL");
    }

    SECTION("parse_photometric_interpretation conversions") {
        REQUIRE(parse_photometric_interpretation("MONOCHROME1") ==
                photometric_interpretation::monochrome1);
        REQUIRE(parse_photometric_interpretation("RGB") ==
                photometric_interpretation::rgb);
        REQUIRE(parse_photometric_interpretation("UNKNOWN_VALUE") ==
                photometric_interpretation::unknown);
    }
}
