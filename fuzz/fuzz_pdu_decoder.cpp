// Fuzz target for DICOM PDU (Protocol Data Unit) decoder
//
// Feeds arbitrary byte buffers into pdu_decoder::decode() to detect
// memory safety issues when processing malformed network protocol data:
// - Invalid PDU type bytes
// - Truncated PDU headers (< 6 bytes)
// - Oversized PDU length fields
// - Malformed A-ASSOCIATE-RQ/AC variable items
// - Corrupt presentation context data
// - Invalid P-DATA-TF presentation data values

#include <pacs/network/pdu_decoder.hpp>

#include <cstdint>
#include <span>
#include <variant>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Exercise the generic decode entry point which dispatches to
    // type-specific decoders based on the first byte.
    auto result =
        kcenon::pacs::network::pdu_decoder::decode({data, size});

    // If decoding succeeded, visit the variant to exercise type-specific
    // accessor paths.
    if (result.is_ok()) {
        std::visit(
            [](const auto& pdu_val) {
                // Force instantiation of accessors for each PDU type
                (void)pdu_val;
            },
            result.value());
    }

    // Also exercise the lightweight helpers which parse only the header.
    (void)kcenon::pacs::network::pdu_decoder::pdu_length({data, size});
    (void)kcenon::pacs::network::pdu_decoder::peek_pdu_type({data, size});

    return 0;
}
