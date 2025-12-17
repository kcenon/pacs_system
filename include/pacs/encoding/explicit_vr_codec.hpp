/**
 * @file explicit_vr_codec.hpp
 * @brief Encoder/decoder for Explicit VR Little Endian transfer syntax
 *
 * This file provides the explicit_vr_codec class for encoding and decoding
 * DICOM data elements using the Explicit VR Little Endian transfer syntax
 * (UID: 1.2.840.10008.1.2.1).
 *
 * In Explicit VR encoding, the VR IS encoded in the data stream as
 * two ASCII characters following the tag.
 *
 * @see DICOM PS3.5 Section 7.1.2 - Explicit VR Little Endian Transfer Syntax
 */

#ifndef PACS_ENCODING_EXPLICIT_VR_CODEC_HPP
#define PACS_ENCODING_EXPLICIT_VR_CODEC_HPP

#include <pacs/core/dicom_dataset.hpp>
#include <pacs/core/dicom_element.hpp>
#include <pacs/core/result.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace pacs::encoding {

/**
 * @brief Encoder/decoder for Explicit VR Little Endian transfer syntax
 *
 * Explicit VR encoding has two formats depending on VR:
 *
 * **Standard VRs (16-bit length):**
 * ┌─────────────────────────────────────────────────────┐
 * │ Data Element                                        │
 * ├───────────┬───────────┬────────┬──────────┬─────────┤
 * │ Group     │ Element   │ VR     │ Length   │ Value   │
 * │ (2 bytes) │ (2 bytes) │(2 char)│ (2 bytes)│         │
 * │ LE        │ LE        │ ASCII  │ LE       │         │
 * └───────────┴───────────┴────────┴──────────┴─────────┘
 * VRs: AE, AS, AT, CS, DA, DS, DT, FL, FD, IS, LO, LT, PN, SH, SL, SS, ST, TM, UI, UL, US
 *
 * **Extended VRs (32-bit length):**
 * ┌──────────────────────────────────────────────────────────────┐
 * │ Data Element                                                 │
 * ├───────────┬───────────┬────────┬──────────┬──────────┬───────┤
 * │ Group     │ Element   │ VR     │ Reserved │ Length   │ Value │
 * │ (2 bytes) │ (2 bytes) │(2 char)│ (2 bytes)│ (4 bytes)│       │
 * │ LE        │ LE        │ ASCII  │ 0x0000   │ LE       │       │
 * └───────────┴───────────┴────────┴──────────┴──────────┴───────┘
 * VRs: OB, OD, OF, OL, OV, OW, SQ, SV, UC, UN, UR, UT, UV
 *
 * @see DICOM PS3.5 Section 7.1.2
 */
class explicit_vr_codec {
public:
    /**
     * @brief Result type for decode operations using pacs::Result<T> pattern
     */
    template <typename T>
    using result = pacs::Result<T>;

    // ========================================================================
    // Dataset Encoding/Decoding
    // ========================================================================

    /**
     * @brief Encode a dataset to bytes using Explicit VR Little Endian
     * @param dataset The dataset to encode
     * @return Encoded byte vector
     */
    [[nodiscard]] static std::vector<uint8_t> encode(
        const core::dicom_dataset& dataset);

    /**
     * @brief Decode bytes to a dataset using Explicit VR Little Endian
     * @param data The bytes to decode
     * @return Result containing the decoded dataset or an error
     */
    [[nodiscard]] static result<core::dicom_dataset> decode(
        std::span<const uint8_t> data);

    // ========================================================================
    // Element Encoding/Decoding
    // ========================================================================

    /**
     * @brief Encode a single element to bytes
     * @param element The element to encode
     * @return Encoded byte vector
     */
    [[nodiscard]] static std::vector<uint8_t> encode_element(
        const core::dicom_element& element);

    /**
     * @brief Decode a single element from bytes
     *
     * The span reference is advanced past the decoded element.
     *
     * @param data Reference to the data span (will be modified)
     * @return Result containing the decoded element or an error
     */
    [[nodiscard]] static result<core::dicom_element> decode_element(
        std::span<const uint8_t>& data);

private:
    // Internal encoding helpers
    static void encode_sequence(std::vector<uint8_t>& buffer,
                                const core::dicom_element& element);

    static void encode_sequence_item(std::vector<uint8_t>& buffer,
                                     const core::dicom_dataset& item);

    // Internal decoding helpers
    static result<core::dicom_element> decode_undefined_length(
        core::dicom_tag tag, vr_type vr,
        std::span<const uint8_t>& data);

    static result<core::dicom_dataset> decode_sequence_item(
        std::span<const uint8_t>& data);
};

}  // namespace pacs::encoding

#endif  // PACS_ENCODING_EXPLICIT_VR_CODEC_HPP
