// Fuzz target for DICOM RLE Lossless codec decompression
//
// Feeds arbitrary byte buffers into rle_codec::decode() to detect
// memory safety issues when processing malformed RLE-compressed data:
// - Invalid RLE header (64-byte segment offset table)
// - Corrupt segment count
// - Out-of-bounds segment offsets
// - Malformed run-length encoded byte sequences
// - Buffer overflows from incorrect segment lengths

#include <kcenon/pacs/encoding/compression/rle_codec.h>

#include <cstdint>
#include <span>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    using namespace kcenon::pacs::encoding::compression;

    // Use a small, fixed image geometry so the fuzzer focuses on the
    // compressed-data parsing logic rather than allocation-size extremes.
    image_params params;
    params.width = 8;
    params.height = 8;
    params.bits_allocated = 8;
    params.bits_stored = 8;
    params.high_bit = 7;
    params.samples_per_pixel = 1;
    params.planar_configuration = 0;
    params.pixel_representation = 0;
    params.photometric = photometric_interpretation::monochrome2;
    params.number_of_frames = 1;

    rle_codec codec;
    auto result = codec.decode({data, size}, params);

    // If decompression succeeded, touch the output buffer to confirm
    // that the returned data is accessible and internally consistent.
    if (result.is_ok()) {
        auto& output = result.value();
        (void)output.data.size();
        (void)output.output_params.width;
    }

    return 0;
}
