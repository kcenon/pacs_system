#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "kcenon/pacs/encoding/compression/hevc_codec.h"
#include "kcenon/pacs/encoding/compression/codec_factory.h"
#include "kcenon/pacs/encoding/compression/image_params.h"
#include "kcenon/pacs/encoding/transfer_syntax.h"

#include <vector>

using namespace kcenon::pacs::encoding::compression;
using namespace kcenon::pacs::encoding;

TEST_CASE("hevc_codec construction", "[encoding][compression][hevc]") {
    SECTION("Default construction creates Main Profile codec") {
        hevc_codec codec;

        CHECK(codec.transfer_syntax_uid() == hevc_codec::kTransferSyntaxUIDMain);
        CHECK(codec.name() == "HEVC/H.265 Main Profile");
        CHECK(codec.is_lossy());
        CHECK_FALSE(codec.is_main10_profile());
        CHECK(codec.quality() == hevc_codec::kDefaultQuality);
    }

    SECTION("Main 10 Profile construction") {
        hevc_codec codec(true);

        CHECK(codec.transfer_syntax_uid() == hevc_codec::kTransferSyntaxUIDMain10);
        CHECK(codec.name() == "HEVC/H.265 Main 10 Profile");
        CHECK(codec.is_lossy());
        CHECK(codec.is_main10_profile());
    }

    SECTION("Custom quality") {
        hevc_codec codec(false, 18);

        CHECK(codec.quality() == 18);
    }

    SECTION("Default quality matches constant") {
        hevc_codec codec;

        CHECK(codec.quality() == hevc_codec::kDefaultQuality);
    }
}

TEST_CASE("hevc_codec transfer syntax UIDs", "[encoding][compression][hevc]") {
    SECTION("UIDs match DICOM definitions") {
        CHECK(hevc_codec::kTransferSyntaxUIDMain == "1.2.840.10008.1.2.4.107");
        CHECK(hevc_codec::kTransferSyntaxUIDMain10 == "1.2.840.10008.1.2.4.108");
    }

    SECTION("UIDs are distinct") {
        CHECK(hevc_codec::kTransferSyntaxUIDMain !=
              hevc_codec::kTransferSyntaxUIDMain10);
    }
}

TEST_CASE("hevc_codec is always lossy", "[encoding][compression][hevc]") {
    SECTION("Main Profile is lossy") {
        hevc_codec codec(false);
        CHECK(codec.is_lossy());
    }

    SECTION("Main 10 Profile is lossy") {
        hevc_codec codec(true);
        CHECK(codec.is_lossy());
    }
}

TEST_CASE("hevc_codec can_encode/can_decode", "[encoding][compression][hevc]") {
    SECTION("Main Profile accepts 8-bit grayscale") {
        hevc_codec codec(false);
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

    SECTION("Main Profile accepts 8-bit RGB") {
        hevc_codec codec(false);
        image_params params;
        params.width = 256;
        params.height = 256;
        params.bits_allocated = 8;
        params.bits_stored = 8;
        params.high_bit = 7;
        params.samples_per_pixel = 3;

        CHECK(codec.can_encode(params));
        CHECK(codec.can_decode(params));
    }

    SECTION("Main Profile rejects 10-bit") {
        hevc_codec codec(false);
        image_params params;
        params.width = 256;
        params.height = 256;
        params.bits_allocated = 16;
        params.bits_stored = 10;
        params.high_bit = 9;
        params.samples_per_pixel = 1;

        CHECK_FALSE(codec.can_encode(params));
    }

    SECTION("Main 10 Profile accepts 10-bit grayscale") {
        hevc_codec codec(true);
        image_params params;
        params.width = 256;
        params.height = 256;
        params.bits_allocated = 16;
        params.bits_stored = 10;
        params.high_bit = 9;
        params.samples_per_pixel = 1;

        CHECK(codec.can_encode(params));
        CHECK(codec.can_decode(params));
    }

    SECTION("Main 10 Profile accepts 8-bit") {
        hevc_codec codec(true);
        image_params params;
        params.width = 128;
        params.height = 128;
        params.bits_allocated = 8;
        params.bits_stored = 8;
        params.high_bit = 7;
        params.samples_per_pixel = 1;

        CHECK(codec.can_encode(params));
    }

    SECTION("Main 10 Profile rejects 16-bit") {
        hevc_codec codec(true);
        image_params params;
        params.width = 256;
        params.height = 256;
        params.bits_allocated = 16;
        params.bits_stored = 16;
        params.high_bit = 15;
        params.samples_per_pixel = 1;

        CHECK_FALSE(codec.can_encode(params));
    }

    SECTION("Zero dimensions rejected") {
        hevc_codec codec;
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
        hevc_codec codec;
        image_params params;
        params.width = 64;
        params.height = 64;
        params.bits_allocated = 8;
        params.bits_stored = 8;
        params.samples_per_pixel = 4;

        CHECK_FALSE(codec.can_encode(params));
    }
}

TEST_CASE("hevc_codec encode/decode returns not-available error",
          "[encoding][compression][hevc]") {
    hevc_codec codec;
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

TEST_CASE("hevc_codec move semantics", "[encoding][compression][hevc]") {
    SECTION("Move construction preserves state") {
        hevc_codec original(true, 18);
        hevc_codec moved(std::move(original));

        CHECK(moved.is_main10_profile());
        CHECK(moved.quality() == 18);
        CHECK(moved.transfer_syntax_uid() == hevc_codec::kTransferSyntaxUIDMain10);
    }

    SECTION("Move assignment preserves state") {
        hevc_codec original(true);
        hevc_codec target;
        target = std::move(original);

        CHECK(target.is_main10_profile());
        CHECK(target.transfer_syntax_uid() == hevc_codec::kTransferSyntaxUIDMain10);
    }
}

TEST_CASE("codec_factory HEVC support", "[encoding][compression][hevc]") {
    SECTION("Factory recognizes HEVC Main Profile UID") {
        CHECK(codec_factory::is_supported("1.2.840.10008.1.2.4.107"));

        auto codec = codec_factory::create("1.2.840.10008.1.2.4.107");
        REQUIRE(codec != nullptr);
        CHECK(codec->transfer_syntax_uid() == "1.2.840.10008.1.2.4.107");
        CHECK(codec->is_lossy());
    }

    SECTION("Factory recognizes HEVC Main 10 Profile UID") {
        CHECK(codec_factory::is_supported("1.2.840.10008.1.2.4.108"));

        auto codec = codec_factory::create("1.2.840.10008.1.2.4.108");
        REQUIRE(codec != nullptr);
        CHECK(codec->transfer_syntax_uid() == "1.2.840.10008.1.2.4.108");
        CHECK(codec->is_lossy());
    }

    SECTION("HEVC UIDs appear in supported transfer syntaxes list") {
        auto supported = codec_factory::supported_transfer_syntaxes();

        bool found_main = false;
        bool found_main10 = false;

        for (const auto& uid : supported) {
            if (uid == "1.2.840.10008.1.2.4.107") found_main = true;
            if (uid == "1.2.840.10008.1.2.4.108") found_main10 = true;
        }

        CHECK(found_main);
        CHECK(found_main10);
    }
}

TEST_CASE("HEVC transfer syntax registry", "[encoding][compression][hevc]") {
    SECTION("HEVC Main Profile is registered") {
        auto ts = find_transfer_syntax("1.2.840.10008.1.2.4.107");
        REQUIRE(ts.has_value());
        CHECK(ts->is_valid());
        CHECK(ts->is_supported());
        CHECK(ts->is_encapsulated());
        CHECK_FALSE(ts->is_deflated());
        CHECK(ts->name() == "HEVC/H.265 Main Profile / Level 5.1");
    }

    SECTION("HEVC Main 10 Profile is registered") {
        auto ts = find_transfer_syntax("1.2.840.10008.1.2.4.108");
        REQUIRE(ts.has_value());
        CHECK(ts->is_valid());
        CHECK(ts->is_supported());
        CHECK(ts->is_encapsulated());
        CHECK_FALSE(ts->is_deflated());
        CHECK(ts->name() == "HEVC/H.265 Main 10 Profile / Level 5.1");
    }

    SECTION("Static instances are available") {
        CHECK(transfer_syntax::hevc_main.uid() == "1.2.840.10008.1.2.4.107");
        CHECK(transfer_syntax::hevc_main10.uid() == "1.2.840.10008.1.2.4.108");
        CHECK(transfer_syntax::hevc_main.is_encapsulated());
        CHECK(transfer_syntax::hevc_main10.is_encapsulated());
    }
}
