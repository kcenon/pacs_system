/**
 * @file transfer_syntax_support_test.cpp
 * @brief Checks transfer_syntax::is_supported() against codec_factory
 *
 * The registry flag, codec_factory::is_supported(), and
 * codec_factory::is_available() describe the same codecs; these tests fail
 * when one of them changes without the others.
 *
 * @see Issue #1275 - Transfer syntax support flags and the Conformance Statement
 */

#include <catch2/catch_test_macros.hpp>

#include "kcenon/pacs/encoding/compression/codec_factory.h"
#include "kcenon/pacs/encoding/transfer_syntax.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <set>
#include <string>
#include <string_view>
#include <vector>

using namespace kcenon::pacs::encoding;
using namespace kcenon::pacs::encoding::compression;

namespace {

/// An 8-bit grayscale frame that every implemented codec can encode.
image_params gray8_params() {
    image_params params;
    params.width = 64;
    params.height = 64;
    params.bits_allocated = 8;
    params.bits_stored = 8;
    params.high_bit = 7;
    params.samples_per_pixel = 1;
    params.photometric = photometric_interpretation::monochrome2;
    return params;
}

std::vector<uint8_t> gray8_gradient(const image_params& params) {
    std::vector<uint8_t> pixels(params.frame_size_bytes());
    for (size_t y = 0; y < params.height; ++y) {
        for (size_t x = 0; x < params.width; ++x) {
            pixels[y * params.width + x] = static_cast<uint8_t>((x * 4 + y * 2) & 0xFF);
        }
    }
    return pixels;
}

std::set<std::string> to_set(const std::vector<std::string_view>& uids) {
    return {uids.begin(), uids.end()};
}

}  // namespace

TEST_CASE("transfer_syntax::is_supported follows its definition",
          "[encoding][transfer_syntax][support]") {
    for (const auto& ts : all_transfer_syntaxes()) {
        INFO(ts.name() << " (" << ts.uid() << ")");

        if (ts.is_encapsulated()) {
            // Encapsulated: an implemented (non-stub) pixel data codec exists
            CHECK(ts.is_supported() == codec_factory::is_supported(ts));
        } else {
            // Native: the dataset codecs cover every syntax except deflate
            CHECK(ts.is_supported() == !ts.is_deflated());
            CHECK_FALSE(codec_factory::is_supported(ts));
            CHECK_FALSE(codec_factory::is_available(ts));
        }

        // Nothing the build can use is missing from the registry flag
        if (codec_factory::is_available(ts)) {
            CHECK(ts.is_supported());
        }
    }
}

TEST_CASE("Static transfer_syntax instances match their registry entries",
          "[encoding][transfer_syntax][support]") {
    const transfer_syntax* instances[] = {
        &transfer_syntax::implicit_vr_little_endian,
        &transfer_syntax::explicit_vr_little_endian,
        &transfer_syntax::explicit_vr_big_endian,
        &transfer_syntax::deflated_explicit_vr_le,
        &transfer_syntax::jpeg_baseline,
        &transfer_syntax::jpeg_lossless,
        &transfer_syntax::jpeg_ls_lossless,
        &transfer_syntax::jpeg_ls_near_lossless,
        &transfer_syntax::jpeg2000_lossless,
        &transfer_syntax::jpeg2000_lossy,
        &transfer_syntax::rle_lossless,
        &transfer_syntax::htj2k_lossless,
        &transfer_syntax::htj2k_rpcl,
        &transfer_syntax::htj2k_lossy,
        &transfer_syntax::hevc_main,
        &transfer_syntax::hevc_main10,
        &transfer_syntax::jpegxl_lossless,
        &transfer_syntax::jpegxl_jpeg_recompression,
        &transfer_syntax::jpegxl_lossy,
        &transfer_syntax::frame_deflate,
    };
    CHECK(std::size(instances) == all_transfer_syntaxes().size());

    for (const auto* ts : instances) {
        INFO(ts->name() << " (" << ts->uid() << ")");
        auto entry = find_transfer_syntax(ts->uid());
        REQUIRE(entry.has_value());
        CHECK(entry->name() == ts->name());
        CHECK(entry->endianness() == ts->endianness());
        CHECK(entry->vr_type() == ts->vr_type());
        CHECK(entry->is_encapsulated() == ts->is_encapsulated());
        CHECK(entry->is_deflated() == ts->is_deflated());
        CHECK(entry->is_supported() == ts->is_supported());
    }
}

TEST_CASE("Stub codecs are not reported as supported or available",
          "[encoding][transfer_syntax][support]") {
    const transfer_syntax* stubs[] = {
        &transfer_syntax::hevc_main,
        &transfer_syntax::hevc_main10,
        &transfer_syntax::jpegxl_lossless,
        &transfer_syntax::jpegxl_jpeg_recompression,
        &transfer_syntax::jpegxl_lossy,
        &transfer_syntax::frame_deflate,
    };

    for (const auto* ts : stubs) {
        INFO(ts->name() << " (" << ts->uid() << ")");
        CHECK_FALSE(ts->is_supported());
        CHECK_FALSE(codec_factory::is_supported(*ts));
        CHECK_FALSE(codec_factory::is_available(*ts));
    }

    auto listed = supported_transfer_syntaxes();
    for (const auto* ts : stubs) {
        INFO(ts->name() << " (" << ts->uid() << ")");
        CHECK(std::find(listed.begin(), listed.end(), *ts) == listed.end());
    }
}

TEST_CASE("codec_factory lists agree with the registry",
          "[encoding][transfer_syntax][support]") {
    std::set<std::string> registry_codecs;
    for (const auto& ts : all_transfer_syntaxes()) {
        if (ts.is_encapsulated() && ts.is_supported()) {
            registry_codecs.emplace(ts.uid());
        }
    }

    const auto supported = to_set(codec_factory::supported_transfer_syntaxes());
    const auto available = to_set(codec_factory::available_transfer_syntaxes());

    CHECK(supported == registry_codecs);
    for (const auto& uid : supported) {
        INFO(uid);
        CHECK(codec_factory::is_supported(uid));
        CHECK(codec_factory::is_available(uid) == (available.count(uid) == 1));
    }
    for (const auto& uid : available) {
        INFO(uid);
        CHECK(supported.count(uid) == 1);
    }

    // Built-in codecs need no optional library
    CHECK(codec_factory::is_available(transfer_syntax::rle_lossless));
    CHECK(codec_factory::is_available(transfer_syntax::jpeg_lossless));
}

TEST_CASE("codec_factory::is_available matches what the codecs do",
          "[encoding][transfer_syntax][support]") {
    const auto params = gray8_params();
    const auto pixels = gray8_gradient(params);

    for (const auto& ts : all_transfer_syntaxes()) {
        if (!ts.is_encapsulated()) {
            continue;
        }
        INFO(ts.name() << " (" << ts.uid() << ")");

        auto codec = codec_factory::create(ts);
        if (codec_factory::is_available(ts)) {
            REQUIRE(codec != nullptr);
            REQUIRE(codec->can_encode(params));

            auto encoded = codec->encode(pixels, params);
            REQUIRE(encoded.is_ok());
            auto decoded = codec->decode(encoded.value().data, params);
            REQUIRE(decoded.is_ok());
            CHECK(decoded.value().data.size() == pixels.size());
            if (!codec->is_lossy()) {
                CHECK(decoded.value().data == pixels);
            }
        } else if (codec != nullptr) {
            // A stub, or a codec whose library this build does not include
            CHECK(codec->encode(pixels, params).is_err());
        }
    }
}
