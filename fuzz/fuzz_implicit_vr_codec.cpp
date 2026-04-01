// Fuzz target for Implicit VR Little Endian codec
//
// Feeds arbitrary byte buffers into implicit_vr_codec::decode() to detect
// memory safety issues when processing malformed DICOM data elements:
// - Invalid tag group/element pairs
// - Oversized 32-bit length fields
// - Undefined-length sequences and items
// - Truncated data elements
// - Recursive/nested sequence structures

#include <pacs/encoding/implicit_vr_codec.hpp>

#include <cstdint>
#include <span>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    auto result =
        kcenon::pacs::encoding::implicit_vr_codec::decode({data, size});

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
