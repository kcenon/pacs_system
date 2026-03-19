#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "pacs/encoding/compression/jpegxl_codec.hpp"
#include "pacs/encoding/compression/codec_factory.hpp"
#include "pacs/encoding/compression/image_params.hpp"

#include <vector>

using namespace kcenon::pacs::encoding::compression;

TEST_CASE("jpegxl_codec construction", "[encoding][compression][jpegxl]") {
    SECTION("Default construction creates lossless codec") {
        jpegxl_codec codec;

        CHECK(codec.transfer_syntax_uid() == jpegxl_codec::kTransferSyntaxUIDLossless);
        CHECK(codec.name() == "JPEG XL (Lossless)");
        CHECK_FALSE(codec.is_lossy());
        CHECK(codec.is_lossless_mode());
        CHECK_FALSE(codec.is_jpeg_recompression_mode());
        CHECK(codec.quality_distance() == jpegxl_codec::kDefaultQualityDistance);
    }

    SECTION("JPEG recompression mode") {
        jpegxl_codec codec(true, true);

        CHECK(codec.transfer_syntax_uid() == jpegxl_codec::kTransferSyntaxUIDJPEGRecompression);
        CHECK(codec.name() == "JPEG XL JPEG Recompression");
        CHECK_FALSE(codec.is_lossy());
        CHECK(codec.is_lossless_mode());
        CHECK(codec.is_jpeg_recompression_mode());
    }

    SECTION("Lossy construction creates lossy codec") {
        jpegxl_codec codec(false);

        CHECK(codec.transfer_syntax_uid() == jpegxl_codec::kTransferSyntaxUIDLossy);
        CHECK(codec.name() == "JPEG XL (Lossy)");
        CHECK(codec.is_lossy());
        CHECK_FALSE(codec.is_lossless_mode());
        CHECK_FALSE(codec.is_jpeg_recompression_mode());
    }

    SECTION("Custom quality distance") {
        jpegxl_codec codec(false, false, 2.5f);

        CHECK(codec.quality_distance() == 2.5f);
        CHECK(codec.is_lossy());
    }
}

TEST_CASE("jpegxl_codec transfer syntax UIDs", "[encoding][compression][jpegxl]") {
    SECTION("UIDs match DICOM Supplement 232 definitions") {
        CHECK(jpegxl_codec::kTransferSyntaxUIDLossless == "1.2.840.10008.1.2.4.110");
        CHECK(jpegxl_codec::kTransferSyntaxUIDJPEGRecompression == "1.2.840.10008.1.2.4.111");
        CHECK(jpegxl_codec::kTransferSyntaxUIDLossy == "1.2.840.10008.1.2.4.112");
    }

    SECTION("All three UIDs are distinct") {
        CHECK(jpegxl_codec::kTransferSyntaxUIDLossless != jpegxl_codec::kTransferSyntaxUIDJPEGRecompression);
        CHECK(jpegxl_codec::kTransferSyntaxUIDJPEGRecompression != jpegxl_codec::kTransferSyntaxUIDLossy);
        CHECK(jpegxl_codec::kTransferSyntaxUIDLossless != jpegxl_codec::kTransferSyntaxUIDLossy);
    }
}

TEST_CASE("jpegxl_codec can_encode/can_decode", "[encoding][compression][jpegxl]") {
    jpegxl_codec codec;

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

    SECTION("Zero bits_stored rejected") {
        image_params params;
        params.width = 64;
        params.height = 64;
        params.bits_allocated = 8;
        params.bits_stored = 0;
        params.samples_per_pixel = 1;

        CHECK_FALSE(codec.can_encode(params));
    }
}

TEST_CASE("jpegxl_codec encode/decode returns not-available error",
          "[encoding][compression][jpegxl]") {
    jpegxl_codec codec;
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

TEST_CASE("jpegxl_codec move semantics", "[encoding][compression][jpegxl]") {
    SECTION("Move construction preserves state") {
        jpegxl_codec original(false, false, 2.0f);
        jpegxl_codec moved(std::move(original));

        CHECK(moved.is_lossy());
        CHECK(moved.quality_distance() == 2.0f);
        CHECK(moved.transfer_syntax_uid() == jpegxl_codec::kTransferSyntaxUIDLossy);
    }

    SECTION("Move assignment preserves state") {
        jpegxl_codec original(true, true);
        jpegxl_codec target;
        target = std::move(original);

        CHECK_FALSE(target.is_lossy());
        CHECK(target.is_jpeg_recompression_mode());
        CHECK(target.transfer_syntax_uid() == jpegxl_codec::kTransferSyntaxUIDJPEGRecompression);
    }
}

TEST_CASE("codec_factory JPEG XL support", "[encoding][compression][jpegxl]") {
    SECTION("Factory recognizes JPEG XL Lossless UID") {
        CHECK(codec_factory::is_supported("1.2.840.10008.1.2.4.110"));

        auto codec = codec_factory::create("1.2.840.10008.1.2.4.110");
        REQUIRE(codec != nullptr);
        CHECK(codec->transfer_syntax_uid() == "1.2.840.10008.1.2.4.110");
        CHECK_FALSE(codec->is_lossy());
    }

    SECTION("Factory recognizes JPEG XL JPEG Recompression UID") {
        CHECK(codec_factory::is_supported("1.2.840.10008.1.2.4.111"));

        auto codec = codec_factory::create("1.2.840.10008.1.2.4.111");
        REQUIRE(codec != nullptr);
        CHECK(codec->transfer_syntax_uid() == "1.2.840.10008.1.2.4.111");
        CHECK_FALSE(codec->is_lossy());
    }

    SECTION("Factory recognizes JPEG XL Lossy UID") {
        CHECK(codec_factory::is_supported("1.2.840.10008.1.2.4.112"));

        auto codec = codec_factory::create("1.2.840.10008.1.2.4.112");
        REQUIRE(codec != nullptr);
        CHECK(codec->transfer_syntax_uid() == "1.2.840.10008.1.2.4.112");
        CHECK(codec->is_lossy());
    }

    SECTION("JPEG XL UIDs appear in supported transfer syntaxes list") {
        auto supported = codec_factory::supported_transfer_syntaxes();

        bool found_lossless = false;
        bool found_recompression = false;
        bool found_lossy = false;

        for (const auto& uid : supported) {
            if (uid == "1.2.840.10008.1.2.4.110") found_lossless = true;
            if (uid == "1.2.840.10008.1.2.4.111") found_recompression = true;
            if (uid == "1.2.840.10008.1.2.4.112") found_lossy = true;
        }

        CHECK(found_lossless);
        CHECK(found_recompression);
        CHECK(found_lossy);
    }
}

TEST_CASE("jpegxl_codec transfer syntax registry integration",
          "[encoding][compression][jpegxl]") {
    SECTION("JPEG XL Lossless in transfer syntax registry") {
        kcenon::pacs::encoding::transfer_syntax ts{"1.2.840.10008.1.2.4.110"};

        CHECK(ts.is_valid());
        CHECK(ts.is_encapsulated());
        CHECK(ts.is_supported());
        CHECK(ts.name() == "JPEG XL Lossless");
    }

    SECTION("JPEG XL JPEG Recompression in transfer syntax registry") {
        kcenon::pacs::encoding::transfer_syntax ts{"1.2.840.10008.1.2.4.111"};

        CHECK(ts.is_valid());
        CHECK(ts.is_encapsulated());
        CHECK(ts.is_supported());
        CHECK(ts.name() == "JPEG XL JPEG Recompression");
    }

    SECTION("JPEG XL Lossy in transfer syntax registry") {
        kcenon::pacs::encoding::transfer_syntax ts{"1.2.840.10008.1.2.4.112"};

        CHECK(ts.is_valid());
        CHECK(ts.is_encapsulated());
        CHECK(ts.is_supported());
        CHECK(ts.name() == "JPEG XL");
    }
}
