#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "pacs/encoding/compression/htj2k_codec.hpp"
#include "pacs/encoding/compression/codec_factory.hpp"
#include "pacs/encoding/compression/image_params.hpp"

#include <algorithm>
#include <cstdlib>
#include <numeric>
#include <vector>

using namespace pacs::encoding::compression;

TEST_CASE("htj2k_codec construction", "[encoding][compression][htj2k]") {
    SECTION("Default construction creates lossless codec") {
        htj2k_codec codec;

        CHECK(codec.transfer_syntax_uid() == htj2k_codec::kTransferSyntaxUIDLossless);
        CHECK(codec.name() == "HTJ2K (Lossless)");
        CHECK_FALSE(codec.is_lossy());
        CHECK(codec.is_lossless_mode());
        CHECK_FALSE(codec.is_rpcl_mode());
    }

    SECTION("Lossless with RPCL creates RPCL codec") {
        htj2k_codec codec(true, true);

        CHECK(codec.transfer_syntax_uid() == htj2k_codec::kTransferSyntaxUIDRPCL);
        CHECK(codec.name() == "HTJ2K with RPCL (Lossless)");
        CHECK_FALSE(codec.is_lossy());
        CHECK(codec.is_lossless_mode());
        CHECK(codec.is_rpcl_mode());
    }

    SECTION("Lossy construction creates lossy codec") {
        htj2k_codec codec(false);

        CHECK(codec.transfer_syntax_uid() == htj2k_codec::kTransferSyntaxUIDLossy);
        CHECK(codec.name() == "HTJ2K (Lossy)");
        CHECK(codec.is_lossy());
        CHECK_FALSE(codec.is_lossless_mode());
    }

    SECTION("Custom compression ratio and resolution levels") {
        htj2k_codec codec(false, false, 30.0f, 8);

        CHECK(codec.compression_ratio() == 30.0f);
        CHECK(codec.resolution_levels() == 8);
    }

    SECTION("Default compression ratio and resolution levels") {
        htj2k_codec codec;

        CHECK(codec.compression_ratio() == htj2k_codec::kDefaultCompressionRatio);
        CHECK(codec.resolution_levels() == htj2k_codec::kDefaultResolutionLevels);
    }
}

TEST_CASE("htj2k_codec transfer syntax UIDs", "[encoding][compression][htj2k]") {
    SECTION("UIDs match DICOM Supplement 235 definitions") {
        CHECK(htj2k_codec::kTransferSyntaxUIDLossless == "1.2.840.10008.1.2.4.201");
        CHECK(htj2k_codec::kTransferSyntaxUIDRPCL == "1.2.840.10008.1.2.4.202");
        CHECK(htj2k_codec::kTransferSyntaxUIDLossy == "1.2.840.10008.1.2.4.203");
    }

    SECTION("All three UIDs are distinct") {
        CHECK(htj2k_codec::kTransferSyntaxUIDLossless != htj2k_codec::kTransferSyntaxUIDRPCL);
        CHECK(htj2k_codec::kTransferSyntaxUIDRPCL != htj2k_codec::kTransferSyntaxUIDLossy);
        CHECK(htj2k_codec::kTransferSyntaxUIDLossless != htj2k_codec::kTransferSyntaxUIDLossy);
    }
}

TEST_CASE("htj2k_codec can_encode/can_decode", "[encoding][compression][htj2k]") {
    htj2k_codec codec;

    SECTION("Valid 8-bit grayscale parameters") {
        image_params params;
        params.width = 512;
        params.height = 512;
        params.bits_allocated = 8;
        params.bits_stored = 8;
        params.high_bit = 7;
        params.samples_per_pixel = 1;

        CHECK(codec.can_encode(params));
        CHECK(codec.can_decode(params));
    }

    SECTION("Valid 16-bit grayscale parameters") {
        image_params params;
        params.width = 256;
        params.height = 256;
        params.bits_allocated = 16;
        params.bits_stored = 16;
        params.high_bit = 15;
        params.samples_per_pixel = 1;

        CHECK(codec.can_encode(params));
        CHECK(codec.can_decode(params));
    }

    SECTION("Valid 8-bit RGB parameters") {
        image_params params;
        params.width = 128;
        params.height = 128;
        params.bits_allocated = 8;
        params.bits_stored = 8;
        params.high_bit = 7;
        params.samples_per_pixel = 3;

        CHECK(codec.can_encode(params));
        CHECK(codec.can_decode(params));
    }

    SECTION("Zero dimensions rejected") {
        image_params params;
        params.width = 0;
        params.height = 512;
        params.bits_allocated = 8;
        params.bits_stored = 8;
        params.samples_per_pixel = 1;

        CHECK_FALSE(codec.can_encode(params));

        params.width = 512;
        params.height = 0;
        CHECK_FALSE(codec.can_encode(params));
    }

    SECTION("Unsupported samples_per_pixel rejected") {
        image_params params;
        params.width = 64;
        params.height = 64;
        params.bits_allocated = 8;
        params.bits_stored = 8;
        params.samples_per_pixel = 4;

        CHECK_FALSE(codec.can_encode(params));
    }

    SECTION("Unsupported bit depth rejected") {
        image_params params;
        params.width = 64;
        params.height = 64;
        params.bits_allocated = 32;
        params.bits_stored = 32;
        params.samples_per_pixel = 1;

        CHECK_FALSE(codec.can_encode(params));
    }
}

#ifdef PACS_WITH_HTJ2K_CODEC

// ============================================================================
// Helper functions for generating test images
// ============================================================================

namespace {

image_params make_grayscale_params(uint16_t width, uint16_t height,
                                    uint16_t bits_stored) {
    image_params params;
    params.width = width;
    params.height = height;
    params.bits_allocated = (bits_stored <= 8) ? 8 : 16;
    params.bits_stored = bits_stored;
    params.high_bit = bits_stored - 1;
    params.samples_per_pixel = 1;
    params.pixel_representation = 0;
    params.planar_configuration = 0;
    params.photometric = photometric_interpretation::monochrome2;
    return params;
}

image_params make_rgb_params(uint16_t width, uint16_t height) {
    image_params params;
    params.width = width;
    params.height = height;
    params.bits_allocated = 8;
    params.bits_stored = 8;
    params.high_bit = 7;
    params.samples_per_pixel = 3;
    params.pixel_representation = 0;
    params.planar_configuration = 0;
    params.photometric = photometric_interpretation::rgb;
    return params;
}

std::vector<uint8_t> generate_gradient_8bit(uint16_t width, uint16_t height) {
    std::vector<uint8_t> data(static_cast<size_t>(width) * height);
    for (uint16_t y = 0; y < height; ++y) {
        for (uint16_t x = 0; x < width; ++x) {
            data[static_cast<size_t>(y) * width + x] =
                static_cast<uint8_t>((x + y) % 256);
        }
    }
    return data;
}

std::vector<uint8_t> generate_gradient_16bit(uint16_t width, uint16_t height,
                                              uint16_t max_val) {
    std::vector<uint8_t> data(static_cast<size_t>(width) * height * 2);
    auto* ptr = reinterpret_cast<uint16_t*>(data.data());
    for (uint16_t y = 0; y < height; ++y) {
        for (uint16_t x = 0; x < width; ++x) {
            ptr[static_cast<size_t>(y) * width + x] =
                static_cast<uint16_t>((x + y) % (max_val + 1));
        }
    }
    return data;
}

std::vector<uint8_t> generate_rgb_gradient(uint16_t width, uint16_t height) {
    std::vector<uint8_t> data(static_cast<size_t>(width) * height * 3);
    for (uint16_t y = 0; y < height; ++y) {
        for (uint16_t x = 0; x < width; ++x) {
            auto idx = (static_cast<size_t>(y) * width + x) * 3;
            data[idx + 0] = static_cast<uint8_t>(x % 256);       // R
            data[idx + 1] = static_cast<uint8_t>(y % 256);       // G
            data[idx + 2] = static_cast<uint8_t>((x + y) % 256); // B
        }
    }
    return data;
}

}  // namespace

// ============================================================================
// Lossless round-trip tests
// ============================================================================

TEST_CASE("htj2k_codec lossless 8-bit grayscale round-trip",
          "[encoding][compression][htj2k][lossless]") {
    htj2k_codec codec(true);
    const uint16_t width = 64;
    const uint16_t height = 64;

    auto params = make_grayscale_params(width, height, 8);
    auto original = generate_gradient_8bit(width, height);

    auto encode_result = codec.encode(original, params);
    REQUIRE(encode_result.is_ok());

    auto& compressed = encode_result.value();
    CHECK_FALSE(compressed.data.empty());
    CHECK(compressed.data.size() < original.size() * 2);

    auto decode_result = codec.decode(compressed.data, params);
    REQUIRE(decode_result.is_ok());

    auto& decompressed = decode_result.value();
    REQUIRE(decompressed.data.size() == original.size());
    CHECK(decompressed.data == original);
}

TEST_CASE("htj2k_codec lossless 12-bit grayscale round-trip",
          "[encoding][compression][htj2k][lossless]") {
    htj2k_codec codec(true);
    const uint16_t width = 64;
    const uint16_t height = 64;

    auto params = make_grayscale_params(width, height, 12);
    auto original = generate_gradient_16bit(width, height, 4095);

    auto encode_result = codec.encode(original, params);
    REQUIRE(encode_result.is_ok());

    auto decode_result = codec.decode(encode_result.value().data, params);
    REQUIRE(decode_result.is_ok());

    REQUIRE(decode_result.value().data.size() == original.size());
    CHECK(decode_result.value().data == original);
}

TEST_CASE("htj2k_codec lossless 16-bit grayscale round-trip",
          "[encoding][compression][htj2k][lossless]") {
    htj2k_codec codec(true);
    const uint16_t width = 64;
    const uint16_t height = 64;

    auto params = make_grayscale_params(width, height, 16);
    auto original = generate_gradient_16bit(width, height, 65535);

    auto encode_result = codec.encode(original, params);
    REQUIRE(encode_result.is_ok());

    auto decode_result = codec.decode(encode_result.value().data, params);
    REQUIRE(decode_result.is_ok());

    REQUIRE(decode_result.value().data.size() == original.size());
    CHECK(decode_result.value().data == original);
}

TEST_CASE("htj2k_codec lossless 8-bit RGB round-trip",
          "[encoding][compression][htj2k][lossless]") {
    htj2k_codec codec(true);
    const uint16_t width = 64;
    const uint16_t height = 64;

    auto params = make_rgb_params(width, height);
    auto original = generate_rgb_gradient(width, height);

    auto encode_result = codec.encode(original, params);
    REQUIRE(encode_result.is_ok());

    auto decode_result = codec.decode(encode_result.value().data, params);
    REQUIRE(decode_result.is_ok());

    REQUIRE(decode_result.value().data.size() == original.size());
    CHECK(decode_result.value().data == original);
}

TEST_CASE("htj2k_codec lossless with RPCL round-trip",
          "[encoding][compression][htj2k][lossless][rpcl]") {
    htj2k_codec codec(true, true);
    const uint16_t width = 64;
    const uint16_t height = 64;

    auto params = make_grayscale_params(width, height, 8);
    auto original = generate_gradient_8bit(width, height);

    auto encode_result = codec.encode(original, params);
    REQUIRE(encode_result.is_ok());

    auto decode_result = codec.decode(encode_result.value().data, params);
    REQUIRE(decode_result.is_ok());

    REQUIRE(decode_result.value().data.size() == original.size());
    CHECK(decode_result.value().data == original);
}

// ============================================================================
// Lossy encode/decode tests
// ============================================================================

TEST_CASE("htj2k_codec lossy 8-bit grayscale",
          "[encoding][compression][htj2k][lossy]") {
    htj2k_codec codec(false, false, 10.0f, 5);
    const uint16_t width = 64;
    const uint16_t height = 64;

    auto params = make_grayscale_params(width, height, 8);
    auto original = generate_gradient_8bit(width, height);

    auto encode_result = codec.encode(original, params);
    REQUIRE(encode_result.is_ok());

    auto& compressed = encode_result.value();
    CHECK(compressed.data.size() < original.size());

    auto decode_result = codec.decode(compressed.data, params);
    REQUIRE(decode_result.is_ok());

    auto& decompressed = decode_result.value();
    REQUIRE(decompressed.data.size() == original.size());

    // Lossy: data should be close but not identical
    // Allow maximum per-pixel error of 5 for moderate compression
    int max_error = 0;
    for (size_t i = 0; i < original.size(); ++i) {
        int diff = std::abs(static_cast<int>(original[i])
                            - static_cast<int>(decompressed.data[i]));
        max_error = std::max(max_error, diff);
    }
    CHECK(max_error < 50);
}

TEST_CASE("htj2k_codec lossy 8-bit RGB",
          "[encoding][compression][htj2k][lossy]") {
    htj2k_codec codec(false, false, 10.0f, 5);
    const uint16_t width = 64;
    const uint16_t height = 64;

    auto params = make_rgb_params(width, height);
    auto original = generate_rgb_gradient(width, height);

    auto encode_result = codec.encode(original, params);
    REQUIRE(encode_result.is_ok());

    auto decode_result = codec.decode(encode_result.value().data, params);
    REQUIRE(decode_result.is_ok());

    auto& decompressed = decode_result.value();
    REQUIRE(decompressed.data.size() == original.size());

    // Verify lossy reconstruction is reasonably close
    int max_error = 0;
    for (size_t i = 0; i < original.size(); ++i) {
        int diff = std::abs(static_cast<int>(original[i])
                            - static_cast<int>(decompressed.data[i]));
        max_error = std::max(max_error, diff);
    }
    CHECK(max_error < 50);
}

// ============================================================================
// Error handling tests
// ============================================================================

TEST_CASE("htj2k_codec encode validates input",
          "[encoding][compression][htj2k]") {
    htj2k_codec codec;

    SECTION("Empty pixel data returns error") {
        image_params params = make_grayscale_params(64, 64, 8);
        std::vector<uint8_t> empty;
        auto result = codec.encode(empty, params);
        REQUIRE_FALSE(result.is_ok());
    }

    SECTION("Pixel data too small returns error") {
        image_params params = make_grayscale_params(64, 64, 8);
        std::vector<uint8_t> small_data(100);
        auto result = codec.encode(small_data, params);
        REQUIRE_FALSE(result.is_ok());
    }
}

TEST_CASE("htj2k_codec decode validates input",
          "[encoding][compression][htj2k]") {
    htj2k_codec codec;

    SECTION("Empty compressed data returns error") {
        image_params params = make_grayscale_params(64, 64, 8);
        std::vector<uint8_t> empty;
        auto result = codec.decode(empty, params);
        REQUIRE_FALSE(result.is_ok());
    }

    SECTION("Invalid compressed data returns error") {
        image_params params = make_grayscale_params(64, 64, 8);
        std::vector<uint8_t> garbage(100, 0xFF);
        auto result = codec.decode(garbage, params);
        REQUIRE_FALSE(result.is_ok());
    }
}

// ============================================================================
// Output parameters verification
// ============================================================================

TEST_CASE("htj2k_codec decode sets output parameters correctly",
          "[encoding][compression][htj2k]") {
    htj2k_codec codec(true);
    const uint16_t width = 32;
    const uint16_t height = 32;

    SECTION("Grayscale output parameters") {
        auto params = make_grayscale_params(width, height, 8);
        auto data = generate_gradient_8bit(width, height);

        auto encoded = codec.encode(data, params);
        REQUIRE(encoded.is_ok());

        auto decoded = codec.decode(encoded.value().data, params);
        REQUIRE(decoded.is_ok());

        auto& out = decoded.value().output_params;
        CHECK(out.width == width);
        CHECK(out.height == height);
        CHECK(out.bits_stored == 8);
        CHECK(out.bits_allocated == 8);
        CHECK(out.samples_per_pixel == 1);
        CHECK(out.photometric == photometric_interpretation::monochrome2);
    }

    SECTION("RGB output parameters") {
        auto params = make_rgb_params(width, height);
        auto data = generate_rgb_gradient(width, height);

        auto encoded = codec.encode(data, params);
        REQUIRE(encoded.is_ok());

        auto decoded = codec.decode(encoded.value().data, params);
        REQUIRE(decoded.is_ok());

        auto& out = decoded.value().output_params;
        CHECK(out.width == width);
        CHECK(out.height == height);
        CHECK(out.bits_stored == 8);
        CHECK(out.samples_per_pixel == 3);
        CHECK(out.photometric == photometric_interpretation::rgb);
    }

    SECTION("16-bit output parameters") {
        auto params = make_grayscale_params(width, height, 16);
        auto data = generate_gradient_16bit(width, height, 65535);

        auto encoded = codec.encode(data, params);
        REQUIRE(encoded.is_ok());

        auto decoded = codec.decode(encoded.value().data, params);
        REQUIRE(decoded.is_ok());

        auto& out = decoded.value().output_params;
        CHECK(out.width == width);
        CHECK(out.height == height);
        CHECK(out.bits_stored == 16);
        CHECK(out.bits_allocated == 16);
        CHECK(out.high_bit == 15);
    }
}

#else  // !PACS_WITH_HTJ2K_CODEC

TEST_CASE("htj2k_codec encode/decode returns not-available error",
          "[encoding][compression][htj2k]") {
    htj2k_codec codec;
    std::vector<uint8_t> data(64 * 64);
    image_params params;
    params.width = 64;
    params.height = 64;
    params.bits_allocated = 8;
    params.bits_stored = 8;
    params.high_bit = 7;
    params.samples_per_pixel = 1;

    SECTION("Encode returns error with not-available message") {
        auto result = codec.encode(data, params);
        REQUIRE_FALSE(result.is_ok());
        CHECK_THAT(result.error().message,
                   Catch::Matchers::ContainsSubstring("not available"));
    }

    SECTION("Decode returns error with not-available message") {
        auto result = codec.decode(data, params);
        REQUIRE_FALSE(result.is_ok());
        CHECK_THAT(result.error().message,
                   Catch::Matchers::ContainsSubstring("not available"));
    }
}

#endif  // PACS_WITH_HTJ2K_CODEC

TEST_CASE("htj2k_codec move semantics", "[encoding][compression][htj2k]") {
    SECTION("Move construction preserves state") {
        htj2k_codec original(false, false, 30.0f, 8);
        htj2k_codec moved(std::move(original));

        CHECK(moved.is_lossy());
        CHECK(moved.compression_ratio() == 30.0f);
        CHECK(moved.resolution_levels() == 8);
        CHECK(moved.transfer_syntax_uid() == htj2k_codec::kTransferSyntaxUIDLossy);
    }

    SECTION("Move assignment preserves state") {
        htj2k_codec original(true, true);
        htj2k_codec target;
        target = std::move(original);

        CHECK_FALSE(target.is_lossy());
        CHECK(target.is_rpcl_mode());
        CHECK(target.transfer_syntax_uid() == htj2k_codec::kTransferSyntaxUIDRPCL);
    }
}

TEST_CASE("codec_factory HTJ2K support", "[encoding][compression][htj2k]") {
    SECTION("Factory recognizes HTJ2K Lossless UID") {
        CHECK(codec_factory::is_supported("1.2.840.10008.1.2.4.201"));

        auto codec = codec_factory::create("1.2.840.10008.1.2.4.201");
        REQUIRE(codec != nullptr);
        CHECK(codec->transfer_syntax_uid() == "1.2.840.10008.1.2.4.201");
        CHECK_FALSE(codec->is_lossy());
    }

    SECTION("Factory recognizes HTJ2K RPCL UID") {
        CHECK(codec_factory::is_supported("1.2.840.10008.1.2.4.202"));

        auto codec = codec_factory::create("1.2.840.10008.1.2.4.202");
        REQUIRE(codec != nullptr);
        CHECK(codec->transfer_syntax_uid() == "1.2.840.10008.1.2.4.202");
        CHECK_FALSE(codec->is_lossy());
    }

    SECTION("Factory recognizes HTJ2K Lossy UID") {
        CHECK(codec_factory::is_supported("1.2.840.10008.1.2.4.203"));

        auto codec = codec_factory::create("1.2.840.10008.1.2.4.203");
        REQUIRE(codec != nullptr);
        CHECK(codec->transfer_syntax_uid() == "1.2.840.10008.1.2.4.203");
        CHECK(codec->is_lossy());
    }

    SECTION("HTJ2K UIDs appear in supported transfer syntaxes list") {
        auto supported = codec_factory::supported_transfer_syntaxes();

        bool found_lossless = false;
        bool found_rpcl = false;
        bool found_lossy = false;

        for (const auto& uid : supported) {
            if (uid == "1.2.840.10008.1.2.4.201") found_lossless = true;
            if (uid == "1.2.840.10008.1.2.4.202") found_rpcl = true;
            if (uid == "1.2.840.10008.1.2.4.203") found_lossy = true;
        }

        CHECK(found_lossless);
        CHECK(found_rpcl);
        CHECK(found_lossy);
    }
}
