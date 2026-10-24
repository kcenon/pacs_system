#include <catch2/catch_test_macros.hpp>

#include "kcenon/pacs/encoding/transfer_syntax.h"

#include <algorithm>

using namespace kcenon::pacs::encoding;

TEST_CASE("transfer_syntax properties", "[encoding][transfer_syntax]") {
    SECTION("Implicit VR Little Endian") {
        const auto& ts = transfer_syntax::implicit_vr_little_endian;

        CHECK(ts.uid() == "1.2.840.10008.1.2");
        CHECK(ts.name() == "Implicit VR Little Endian");
        CHECK(ts.endianness() == byte_order::little_endian);
        CHECK(ts.vr_type() == vr_encoding::implicit);
        CHECK_FALSE(ts.is_encapsulated());
        CHECK_FALSE(ts.is_deflated());
        CHECK(ts.is_valid());
        CHECK(ts.is_supported());
    }

    SECTION("Explicit VR Little Endian") {
        const auto& ts = transfer_syntax::explicit_vr_little_endian;

        CHECK(ts.uid() == "1.2.840.10008.1.2.1");
        CHECK(ts.name() == "Explicit VR Little Endian");
        CHECK(ts.endianness() == byte_order::little_endian);
        CHECK(ts.vr_type() == vr_encoding::explicit_vr);
        CHECK_FALSE(ts.is_encapsulated());
        CHECK_FALSE(ts.is_deflated());
        CHECK(ts.is_valid());
        CHECK(ts.is_supported());
    }

    SECTION("Explicit VR Big Endian") {
        const auto& ts = transfer_syntax::explicit_vr_big_endian;

        CHECK(ts.uid() == "1.2.840.10008.1.2.2");
        CHECK(ts.name() == "Explicit VR Big Endian");
        CHECK(ts.endianness() == byte_order::big_endian);
        CHECK(ts.vr_type() == vr_encoding::explicit_vr);
        CHECK_FALSE(ts.is_encapsulated());
        CHECK_FALSE(ts.is_deflated());
        CHECK(ts.is_valid());
        CHECK(ts.is_supported());
    }

    SECTION("Deflated Explicit VR Little Endian") {
        const auto& ts = transfer_syntax::deflated_explicit_vr_le;

        CHECK(ts.uid() == "1.2.840.10008.1.2.1.99");
        CHECK(ts.endianness() == byte_order::little_endian);
        CHECK(ts.vr_type() == vr_encoding::explicit_vr);
        CHECK_FALSE(ts.is_encapsulated());
        CHECK(ts.is_deflated());
        CHECK(ts.is_valid());
        CHECK_FALSE(ts.is_supported());  // No inflate or deflate code
    }

    SECTION("JPEG Baseline") {
        const auto& ts = transfer_syntax::jpeg_baseline;

        CHECK(ts.uid() == "1.2.840.10008.1.2.4.50");
        CHECK(ts.endianness() == byte_order::little_endian);
        CHECK(ts.vr_type() == vr_encoding::explicit_vr);
        CHECK(ts.is_encapsulated());
        CHECK_FALSE(ts.is_deflated());
        CHECK(ts.is_valid());
        CHECK(ts.is_supported());  // jpeg_baseline_codec (libjpeg-turbo)
    }

    SECTION("JPEG Lossless") {
        const auto& ts = transfer_syntax::jpeg_lossless;

        CHECK(ts.uid() == "1.2.840.10008.1.2.4.70");
        CHECK(ts.is_encapsulated());
        CHECK(ts.is_supported());  // Built-in jpeg_lossless_codec
    }

    SECTION("JPEG-LS Lossless") {
        const auto& ts = transfer_syntax::jpeg_ls_lossless;

        CHECK(ts.uid() == "1.2.840.10008.1.2.4.80");
        CHECK(ts.name() == "JPEG-LS Lossless Image Compression");
        CHECK(ts.endianness() == byte_order::little_endian);
        CHECK(ts.vr_type() == vr_encoding::explicit_vr);
        CHECK(ts.is_encapsulated());
        CHECK_FALSE(ts.is_deflated());
        CHECK(ts.is_valid());
        CHECK(ts.is_supported());  // jpeg_ls_codec (CharLS)
    }

    SECTION("JPEG-LS Near-Lossless") {
        const auto& ts = transfer_syntax::jpeg_ls_near_lossless;

        CHECK(ts.uid() == "1.2.840.10008.1.2.4.81");
        CHECK(ts.name() == "JPEG-LS Lossy (Near-Lossless) Image Compression");
        CHECK(ts.endianness() == byte_order::little_endian);
        CHECK(ts.vr_type() == vr_encoding::explicit_vr);
        CHECK(ts.is_encapsulated());
        CHECK_FALSE(ts.is_deflated());
        CHECK(ts.is_valid());
        CHECK(ts.is_supported());  // jpeg_ls_codec (CharLS)
    }

    SECTION("JPEG 2000 Lossless") {
        const auto& ts = transfer_syntax::jpeg2000_lossless;

        CHECK(ts.uid() == "1.2.840.10008.1.2.4.90");
        CHECK(ts.is_encapsulated());
        CHECK(ts.is_supported());  // jpeg2000_codec (OpenJPEG)
    }

    SECTION("JPEG 2000 Lossy") {
        const auto& ts = transfer_syntax::jpeg2000_lossy;

        CHECK(ts.uid() == "1.2.840.10008.1.2.4.91");
        CHECK(ts.is_encapsulated());
        CHECK(ts.is_supported());  // jpeg2000_codec (OpenJPEG)
    }

    SECTION("HTJ2K Lossless") {
        const auto& ts = transfer_syntax::htj2k_lossless;

        CHECK(ts.uid() == "1.2.840.10008.1.2.4.201");
        CHECK(ts.name() == "High-Throughput JPEG 2000 Image Compression (Lossless Only)");
        CHECK(ts.endianness() == byte_order::little_endian);
        CHECK(ts.vr_type() == vr_encoding::explicit_vr);
        CHECK(ts.is_encapsulated());
        CHECK_FALSE(ts.is_deflated());
        CHECK(ts.is_valid());
        CHECK(ts.is_supported());
    }

    SECTION("HTJ2K RPCL") {
        const auto& ts = transfer_syntax::htj2k_rpcl;

        CHECK(ts.uid() == "1.2.840.10008.1.2.4.202");
        CHECK(ts.endianness() == byte_order::little_endian);
        CHECK(ts.vr_type() == vr_encoding::explicit_vr);
        CHECK(ts.is_encapsulated());
        CHECK_FALSE(ts.is_deflated());
        CHECK(ts.is_valid());
        CHECK(ts.is_supported());
    }

    SECTION("HTJ2K Lossy") {
        const auto& ts = transfer_syntax::htj2k_lossy;

        CHECK(ts.uid() == "1.2.840.10008.1.2.4.203");
        CHECK(ts.endianness() == byte_order::little_endian);
        CHECK(ts.vr_type() == vr_encoding::explicit_vr);
        CHECK(ts.is_encapsulated());
        CHECK_FALSE(ts.is_deflated());
        CHECK(ts.is_valid());
        CHECK(ts.is_supported());
    }

    SECTION("JPEG XL Lossless") {
        const auto& ts = transfer_syntax::jpegxl_lossless;

        CHECK(ts.uid() == "1.2.840.10008.1.2.4.110");
        CHECK(ts.name() == "JPEG XL Lossless");
        CHECK(ts.endianness() == byte_order::little_endian);
        CHECK(ts.vr_type() == vr_encoding::explicit_vr);
        CHECK(ts.is_encapsulated());
        CHECK_FALSE(ts.is_deflated());
        CHECK(ts.is_valid());
        CHECK_FALSE(ts.is_supported());  // Stub codec
    }

    SECTION("JPEG XL JPEG Recompression") {
        const auto& ts = transfer_syntax::jpegxl_jpeg_recompression;

        CHECK(ts.uid() == "1.2.840.10008.1.2.4.111");
        CHECK(ts.name() == "JPEG XL JPEG Recompression");
        CHECK(ts.endianness() == byte_order::little_endian);
        CHECK(ts.vr_type() == vr_encoding::explicit_vr);
        CHECK(ts.is_encapsulated());
        CHECK_FALSE(ts.is_deflated());
        CHECK(ts.is_valid());
        CHECK_FALSE(ts.is_supported());  // Stub codec
    }

    SECTION("JPEG XL Lossy") {
        const auto& ts = transfer_syntax::jpegxl_lossy;

        CHECK(ts.uid() == "1.2.840.10008.1.2.4.112");
        CHECK(ts.name() == "JPEG XL");
        CHECK(ts.endianness() == byte_order::little_endian);
        CHECK(ts.vr_type() == vr_encoding::explicit_vr);
        CHECK(ts.is_encapsulated());
        CHECK_FALSE(ts.is_deflated());
        CHECK(ts.is_valid());
        CHECK_FALSE(ts.is_supported());  // Stub codec
    }

    SECTION("Frame Deflate") {
        const auto& ts = transfer_syntax::frame_deflate;

        CHECK(ts.uid() == "1.2.840.10008.1.2.11");
        CHECK(ts.name() == "Frame Deflate");
        CHECK(ts.endianness() == byte_order::little_endian);
        CHECK(ts.vr_type() == vr_encoding::explicit_vr);
        CHECK(ts.is_encapsulated());
        CHECK_FALSE(ts.is_deflated());
        CHECK(ts.is_valid());
        CHECK_FALSE(ts.is_supported());  // Stub codec
    }
}

TEST_CASE("transfer_syntax construction from UID", "[encoding][transfer_syntax]") {
    SECTION("Valid UID creates valid transfer_syntax") {
        transfer_syntax ts{"1.2.840.10008.1.2.1"};

        CHECK(ts.is_valid());
        CHECK(ts.uid() == "1.2.840.10008.1.2.1");
        CHECK(ts.name() == "Explicit VR Little Endian");
    }

    SECTION("Invalid UID creates invalid transfer_syntax") {
        transfer_syntax ts{"1.2.3.4.5.invalid"};

        CHECK_FALSE(ts.is_valid());
        CHECK(ts.uid() == "1.2.3.4.5.invalid");
        CHECK(ts.name() == "Unknown");
        CHECK_FALSE(ts.is_supported());
    }

    SECTION("Empty UID creates invalid transfer_syntax") {
        transfer_syntax ts{""};

        CHECK_FALSE(ts.is_valid());
        CHECK(ts.name() == "Unknown");
    }
}

TEST_CASE("transfer_syntax construction from HTJ2K UID", "[encoding][transfer_syntax]") {
    SECTION("HTJ2K Lossless UID creates valid transfer_syntax") {
        transfer_syntax ts{"1.2.840.10008.1.2.4.201"};

        CHECK(ts.is_valid());
        CHECK(ts.uid() == "1.2.840.10008.1.2.4.201");
        CHECK(ts.is_encapsulated());
        CHECK(ts.is_supported());
    }

    SECTION("HTJ2K RPCL UID creates valid transfer_syntax") {
        transfer_syntax ts{"1.2.840.10008.1.2.4.202"};

        CHECK(ts.is_valid());
        CHECK(ts.uid() == "1.2.840.10008.1.2.4.202");
        CHECK(ts.is_encapsulated());
    }

    SECTION("HTJ2K Lossy UID creates valid transfer_syntax") {
        transfer_syntax ts{"1.2.840.10008.1.2.4.203"};

        CHECK(ts.is_valid());
        CHECK(ts.uid() == "1.2.840.10008.1.2.4.203");
        CHECK(ts.is_encapsulated());
    }
}

TEST_CASE("transfer_syntax lookup of JPEG-LS UIDs", "[encoding][transfer_syntax][jpegls]") {
    SECTION("JPEG-LS Lossless UID creates valid transfer_syntax") {
        transfer_syntax ts{"1.2.840.10008.1.2.4.80"};

        CHECK(ts.is_valid());
        CHECK(ts.name() == "JPEG-LS Lossless Image Compression");
        CHECK(ts.endianness() == byte_order::little_endian);
        CHECK(ts.vr_type() == vr_encoding::explicit_vr);
        CHECK(ts.is_encapsulated());
        CHECK_FALSE(ts.is_deflated());
        CHECK(ts == transfer_syntax::jpeg_ls_lossless);
    }

    SECTION("JPEG-LS Near-Lossless UID creates valid transfer_syntax") {
        transfer_syntax ts{"1.2.840.10008.1.2.4.81"};

        CHECK(ts.is_valid());
        CHECK(ts.name() == "JPEG-LS Lossy (Near-Lossless) Image Compression");
        CHECK(ts.endianness() == byte_order::little_endian);
        CHECK(ts.vr_type() == vr_encoding::explicit_vr);
        CHECK(ts.is_encapsulated());
        CHECK_FALSE(ts.is_deflated());
        CHECK(ts == transfer_syntax::jpeg_ls_near_lossless);
    }

    SECTION("find_transfer_syntax finds both JPEG-LS UIDs") {
        auto lossless = find_transfer_syntax("1.2.840.10008.1.2.4.80");
        REQUIRE(lossless.has_value());
        CHECK(lossless->name() == "JPEG-LS Lossless Image Compression");
        CHECK(lossless->is_encapsulated());

        auto near_lossless = find_transfer_syntax("1.2.840.10008.1.2.4.81");
        REQUIRE(near_lossless.has_value());
        CHECK(near_lossless->name() == "JPEG-LS Lossy (Near-Lossless) Image Compression");
        CHECK(near_lossless->is_encapsulated());
    }

    SECTION("all_transfer_syntaxes lists both JPEG-LS UIDs") {
        auto all = all_transfer_syntaxes();

        CHECK(std::find(all.begin(), all.end(), transfer_syntax::jpeg_ls_lossless) != all.end());
        CHECK(std::find(all.begin(), all.end(), transfer_syntax::jpeg_ls_near_lossless) != all.end());
    }
}

TEST_CASE("transfer_syntax lookup", "[encoding][transfer_syntax]") {
    SECTION("find_transfer_syntax with valid UID") {
        auto ts = find_transfer_syntax("1.2.840.10008.1.2.1");

        REQUIRE(ts.has_value());
        CHECK(ts->name() == "Explicit VR Little Endian");
        CHECK(ts->is_valid());
    }

    SECTION("find_transfer_syntax with invalid UID") {
        auto ts = find_transfer_syntax("1.2.3.4.5.invalid");

        CHECK_FALSE(ts.has_value());
    }

    SECTION("find_transfer_syntax with empty UID") {
        auto ts = find_transfer_syntax("");

        CHECK_FALSE(ts.has_value());
    }
}

TEST_CASE("transfer_syntax support enumeration", "[encoding][transfer_syntax]") {
    SECTION("supported_transfer_syntaxes returns only supported ones") {
        auto supported = supported_transfer_syntaxes();

        // Uncompressed (3) + JPEG Baseline and Lossless (2) + JPEG-LS (2) +
        // JPEG 2000 (2) + RLE Lossless (1) + HTJ2K (3); the HEVC, JPEG XL,
        // and Frame Deflate codecs are stubs
        CHECK(supported.size() == 13);

        for (const auto& ts : supported) {
            CHECK(ts.is_supported());
            CHECK_FALSE(ts.is_deflated());
        }
    }

    SECTION("all_transfer_syntaxes returns all registered") {
        auto all = all_transfer_syntaxes();

        CHECK(all.size() >= 20);

        for (const auto& ts : all) {
            CHECK(ts.is_valid());
        }
    }
}

TEST_CASE("transfer_syntax comparison", "[encoding][transfer_syntax]") {
    SECTION("Same UID compares equal") {
        transfer_syntax ts1{"1.2.840.10008.1.2"};
        transfer_syntax ts2{"1.2.840.10008.1.2"};

        CHECK(ts1 == ts2);
        CHECK_FALSE(ts1 != ts2);
    }

    SECTION("Different UID compares not equal") {
        transfer_syntax ts1{"1.2.840.10008.1.2"};
        transfer_syntax ts2{"1.2.840.10008.1.2.1"};

        CHECK(ts1 != ts2);
        CHECK_FALSE(ts1 == ts2);
    }

    SECTION("Static instance comparison") {
        CHECK(transfer_syntax::implicit_vr_little_endian ==
              transfer_syntax::implicit_vr_little_endian);
        CHECK(transfer_syntax::implicit_vr_little_endian !=
              transfer_syntax::explicit_vr_little_endian);
    }

    SECTION("Constructed matches static instance") {
        transfer_syntax ts{"1.2.840.10008.1.2"};

        CHECK(ts == transfer_syntax::implicit_vr_little_endian);
    }
}

TEST_CASE("byte_order and vr_encoding enums", "[encoding][transfer_syntax]") {
    SECTION("byte_order values are distinct") {
        CHECK(byte_order::little_endian != byte_order::big_endian);
    }

    SECTION("vr_encoding values are distinct") {
        CHECK(vr_encoding::implicit != vr_encoding::explicit_vr);
    }
}
