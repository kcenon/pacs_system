// Fuzz target for Explicit VR Little Endian codec
//
// Feeds arbitrary byte buffers into explicit_vr_codec::decode() to detect
// memory safety issues when processing malformed DICOM data elements:
// - Invalid VR type strings (two ASCII characters)
// - Mismatched 16-bit vs 32-bit length encoding
// - Reserved bytes in extended VR format
// - Unknown or private VR types
// - Truncated elements and corrupt length fields

#include <pacs/encoding/explicit_vr_codec.hpp>

#include <cstdint>
#include <span>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    auto result =
        kcenon::pacs::encoding::explicit_vr_codec::decode({data, size});

    // If decoding succeeded, iterate over elements to exercise accessor
    // paths and confirm internal consistency.
    if (result.is_ok()) {
        auto& dataset = result.value();
        (void)dataset.size();
        for (const auto& [tag, element] : dataset) {
            (void)element.vr();
            (void)element.length();
        }
    }

    return 0;
}
