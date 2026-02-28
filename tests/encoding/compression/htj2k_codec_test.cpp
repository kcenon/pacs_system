#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "pacs/encoding/compression/htj2k_codec.hpp"
#include "pacs/encoding/compression/codec_factory.hpp"
#include "pacs/encoding/compression/image_params.hpp"

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

TEST_CASE("htj2k_codec encode/decode returns not-implemented error",
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

    SECTION("Encode returns error with OpenJPH message") {
        auto result = codec.encode(data, params);
        REQUIRE_FALSE(result.is_ok());
        CHECK_THAT(result.error().message,
                   Catch::Matchers::ContainsSubstring("OpenJPH"));
    }

    SECTION("Decode returns error with OpenJPH message") {
        auto result = codec.decode(data, params);
        REQUIRE_FALSE(result.is_ok());
        CHECK_THAT(result.error().message,
                   Catch::Matchers::ContainsSubstring("OpenJPH"));
    }
}

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
