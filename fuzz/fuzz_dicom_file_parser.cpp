// Fuzz target for DICOM Part 10 file parser (dicom_file::from_bytes)
//
// Feeds arbitrary byte buffers into the DICOM file parser to detect
// memory safety issues when processing malformed DICOM files:
// - Truncated preamble/header
// - Invalid DICM prefix
// - Malformed File Meta Information
// - Corrupt transfer syntax UIDs
// - Nested sequences with undefined length
// - Oversized value length fields

#include <pacs/core/dicom_file.hpp>

#include <cstdint>
#include <span>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Feed raw bytes into dicom_file::from_bytes().
    // The parser must handle all inputs without crashing, regardless of
    // whether the bytes represent a valid DICOM file.
    auto result =
        kcenon::pacs::core::dicom_file::from_bytes({data, size});

    // If parsing succeeded, exercise read-only accessors to ensure the
    // returned object is internally consistent and does not trigger
    // undefined behaviour when inspected.
    if (result.is_ok()) {
        auto& file = result.value();
        (void)file.dataset();
        (void)file.transfer_syntax();
        (void)file.meta_information();
    }

    return 0;
}
