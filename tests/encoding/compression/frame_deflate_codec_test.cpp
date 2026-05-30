#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "kcenon/pacs/encoding/compression/frame_deflate_codec.h"
#include "kcenon/pacs/encoding/compression/codec_factory.h"
#include "kcenon/pacs/encoding/compression/image_params.h"

#include <algorithm>
#include <numeric>
#include <vector>

using namespace kcenon::pacs::encoding::compression;

TEST_CASE("frame_deflate_codec construction", "[encoding][compression][frame_deflate]") {
    SECTION("Default construction creates codec with default compression level") {
        frame_deflate_codec codec;

        CHECK(codec.transfer_syntax_uid() == frame_deflate_codec::kTransferSyntaxUID);
        CHECK(codec.name() == "Frame Deflate");
        CHECK_FALSE(codec.is_lossy());
        CHECK(codec.compression_level() == frame_deflate_codec::kDefaultCompressionLevel);
    }

    SECTION("Custom compression level") {
        frame_deflate_codec codec(9);

        CHECK(codec.compression_level() == 9);
        CHECK_FALSE(codec.is_lossy());
    }

    SECTION("Compression level clamped to valid range") {
        frame_deflate_codec codec_low(0);
        CHECK(codec_low.compression_level() == frame_deflate_codec::kMinCompressionLevel);

        frame_deflate_codec codec_high(100);
        CHECK(codec_high.compression_level() == frame_deflate_codec::kMaxCompressionLevel);
    }
}

TEST_CASE("frame_deflate_codec transfer syntax UID", "[encoding][compression][frame_deflate]") {
    SECTION("UID matches DICOM Supplement 244 definition") {
        CHECK(frame_deflate_codec::kTransferSyntaxUID == "1.2.840.10008.1.2.11");
    }
}

TEST_CASE("frame_deflate_codec can_encode/can_decode", "[encoding][compression][frame_deflate]") {
    frame_deflate_codec codec;

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

    SECTION("Valid 1-bit binary segmentation parameters") {
        image_params params;
        params.width = 1024;
        params.height = 1024;
        params.bits_allocated = 8;
        params.bits_stored = 1;
        params.high_bit = 0;
        params.samples_per_pixel = 1;

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

    SECTION("Color images rejected") {
        image_params params;
        params.width = 64;
        params.height = 64;
        params.bits_allocated = 8;
        params.bits_stored = 8;
        params.samples_per_pixel = 3;

        CHECK_FALSE(codec.can_encode(params));
    }

    SECTION("Zero bits_stored rejected") {
        image_params params;
        params.width = 64;
        params.height = 64;
        params.bits_allocated = 8;
        params.bits_stored = 0;
        params.samples_per_pixel = 1;

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

#ifdef PACS_WITH_ZLIB

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

std::vector<uint8_t> generate_binary_segmentation(uint16_t width, uint16_t height) {
    // Simulate a binary segmentation mask: central circular region = 1, rest = 0
    std::vector<uint8_t> data(static_cast<size_t>(width) * height, 0);
    int cx = width / 2;
    int cy = height / 2;
    int radius = std::min(width, height) / 4;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int dx = x - cx;
            int dy = y - cy;
            if (dx * dx + dy * dy <= radius * radius) {
                data[static_cast<size_t>(y) * width + x] = 1;
            }
        }
    }
    return data;
}

}  // namespace

TEST_CASE("frame_deflate_codec lossless 8-bit grayscale round-trip",
          "[encoding][compression][frame_deflate][lossless]") {
    frame_deflate_codec codec;
    const uint16_t width = 64;
    const uint16_t height = 64;

    auto params = make_grayscale_params(width, height, 8);
    auto original = generate_gradient_8bit(width, height);

    auto encode_result = codec.encode(original, params);
    REQUIRE(encode_result.is_ok());

    auto& compressed = encode_result.value();
    CHECK_FALSE(compressed.data.empty());

    auto decode_result = codec.decode(compressed.data, params);
    REQUIRE(decode_result.is_ok());

    auto& decompressed = decode_result.value();
    REQUIRE(decompressed.data.size() == original.size());
    CHECK(decompressed.data == original);
}

TEST_CASE("frame_deflate_codec lossless 16-bit grayscale round-trip",
          "[encoding][compression][frame_deflate][lossless]") {
    frame_deflate_codec codec;
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

TEST_CASE("frame_deflate_codec binary segmentation mask compression",
          "[encoding][compression][frame_deflate][segmentation]") {
    frame_deflate_codec codec;
    const uint16_t width = 256;
    const uint16_t height = 256;

    auto params = make_grayscale_params(width, height, 1);
    // For 1-bit stored in 8-bit allocated, frame_size_bytes uses bits_allocated
    params.bits_allocated = 8;
    auto original = generate_binary_segmentation(width, height);

    auto encode_result = codec.encode(original, params);
    REQUIRE(encode_result.is_ok());

    auto& compressed = encode_result.value();
    // Binary data should compress very well
    CHECK(compressed.data.size() < original.size());

    auto decode_result = codec.decode(compressed.data, params);
    REQUIRE(decode_result.is_ok());

    REQUIRE(decode_result.value().data.size() == original.size());
    CHECK(decode_result.value().data == original);
}

TEST_CASE("frame_deflate_codec compression level affects output size",
          "[encoding][compression][frame_deflate]") {
    const uint16_t width = 128;
    const uint16_t height = 128;
    auto params = make_grayscale_params(width, height, 8);
    auto data = generate_gradient_8bit(width, height);

    frame_deflate_codec fast_codec(1);
    frame_deflate_codec best_codec(9);

    auto fast_result = fast_codec.encode(data, params);
    auto best_result = best_codec.encode(data, params);

    REQUIRE(fast_result.is_ok());
    REQUIRE(best_result.is_ok());

    // Best compression should produce smaller or equal output
    CHECK(best_result.value().data.size() <= fast_result.value().data.size());
}

TEST_CASE("frame_deflate_codec encode validates input",
          "[encoding][compression][frame_deflate]") {
    frame_deflate_codec codec;

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

TEST_CASE("frame_deflate_codec decode validates input",
          "[encoding][compression][frame_deflate]") {
    frame_deflate_codec codec;

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

#else  // !PACS_WITH_ZLIB

TEST_CASE("frame_deflate_codec encode/decode returns not-available error",
          "[encoding][compression][frame_deflate]") {
    frame_deflate_codec codec;
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

#endif  // PACS_WITH_ZLIB

TEST_CASE("frame_deflate_codec move semantics", "[encoding][compression][frame_deflate]") {
    SECTION("Move construction preserves state") {
        frame_deflate_codec original(9);
        frame_deflate_codec moved(std::move(original));

        CHECK(moved.compression_level() == 9);
        CHECK_FALSE(moved.is_lossy());
        CHECK(moved.transfer_syntax_uid() == frame_deflate_codec::kTransferSyntaxUID);
    }

    SECTION("Move assignment preserves state") {
        frame_deflate_codec original(3);
        frame_deflate_codec target;
        target = std::move(original);

        CHECK(target.compression_level() == 3);
        CHECK(target.transfer_syntax_uid() == frame_deflate_codec::kTransferSyntaxUID);
    }
}

TEST_CASE("codec_factory Frame Deflate support", "[encoding][compression][frame_deflate]") {
    SECTION("Factory recognizes Frame Deflate UID") {
        CHECK(codec_factory::is_supported("1.2.840.10008.1.2.11"));

        auto codec = codec_factory::create("1.2.840.10008.1.2.11");
        REQUIRE(codec != nullptr);
        CHECK(codec->transfer_syntax_uid() == "1.2.840.10008.1.2.11");
        CHECK_FALSE(codec->is_lossy());
    }

    SECTION("Frame Deflate UID appears in supported transfer syntaxes list") {
        auto supported = codec_factory::supported_transfer_syntaxes();

        bool found = false;
        for (const auto& uid : supported) {
            if (uid == "1.2.840.10008.1.2.11") {
                found = true;
                break;
            }
        }
        CHECK(found);
    }
}

TEST_CASE("transfer_syntax registry Frame Deflate support",
          "[encoding][transfer_syntax][frame_deflate]") {
    SECTION("Frame Deflate static instance properties") {
        const auto& ts = kcenon::pacs::encoding::transfer_syntax::frame_deflate;

        CHECK(ts.uid() == "1.2.840.10008.1.2.11");
        CHECK(ts.name() == "Frame Deflate");
        CHECK(ts.endianness() == kcenon::pacs::encoding::byte_order::little_endian);
        CHECK(ts.vr_type() == kcenon::pacs::encoding::vr_encoding::explicit_vr);
        CHECK(ts.is_encapsulated());
        CHECK_FALSE(ts.is_deflated());
        CHECK(ts.is_valid());
        CHECK(ts.is_supported());
    }

    SECTION("find_transfer_syntax finds Frame Deflate") {
        auto ts = kcenon::pacs::encoding::find_transfer_syntax("1.2.840.10008.1.2.11");
        REQUIRE(ts.has_value());
        CHECK(ts->name() == "Frame Deflate");
        CHECK(ts->is_supported());
    }

    SECTION("Frame Deflate in supported_transfer_syntaxes list") {
        auto supported = kcenon::pacs::encoding::supported_transfer_syntaxes();
        bool found = false;
        for (const auto& ts : supported) {
            if (ts.uid() == "1.2.840.10008.1.2.11") {
                found = true;
                break;
            }
        }
        CHECK(found);
    }
}
