#include <catch2/catch_test_macros.hpp>

#include "pacs/encoding/transfer_syntax.hpp"

using namespace pacs::encoding;

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
        CHECK_FALSE(ts.is_supported());  // Not supported in Phase 1
    }

    SECTION("JPEG Baseline") {
        const auto& ts = transfer_syntax::jpeg_baseline;

        CHECK(ts.uid() == "1.2.840.10008.1.2.4.50");
        CHECK(ts.endianness() == byte_order::little_endian);
        CHECK(ts.vr_type() == vr_encoding::explicit_vr);
        CHECK(ts.is_encapsulated());
        CHECK_FALSE(ts.is_deflated());
        CHECK(ts.is_valid());
        CHECK(ts.is_supported());  // Supported since Phase 3
    }

    SECTION("JPEG Lossless") {
        const auto& ts = transfer_syntax::jpeg_lossless;

        CHECK(ts.uid() == "1.2.840.10008.1.2.4.70");
        CHECK(ts.is_encapsulated());
        CHECK_FALSE(ts.is_supported());
    }

    SECTION("JPEG 2000 Lossless") {
        const auto& ts = transfer_syntax::jpeg2000_lossless;

        CHECK(ts.uid() == "1.2.840.10008.1.2.4.90");
        CHECK(ts.is_encapsulated());
        CHECK_FALSE(ts.is_supported());
    }

    SECTION("JPEG 2000 Lossy") {
        const auto& ts = transfer_syntax::jpeg2000_lossy;

        CHECK(ts.uid() == "1.2.840.10008.1.2.4.91");
        CHECK(ts.is_encapsulated());
        CHECK_FALSE(ts.is_supported());
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

        // Phase 3: uncompressed syntaxes (3) + JPEG Baseline (1)
        CHECK(supported.size() == 4);

        for (const auto& ts : supported) {
            CHECK(ts.is_supported());
            CHECK_FALSE(ts.is_deflated());
        }
    }

    SECTION("all_transfer_syntaxes returns all registered") {
        auto all = all_transfer_syntaxes();

        CHECK(all.size() >= 8);

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
