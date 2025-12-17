/**
 * @file implicit_vr_codec.hpp
 * @brief Encoder/decoder for Implicit VR Little Endian transfer syntax
 *
 * This file provides the implicit_vr_codec class for encoding and decoding
 * DICOM data elements using the Implicit VR Little Endian transfer syntax
 * (UID: 1.2.840.10008.1.2).
 *
 * In Implicit VR encoding, the VR is NOT encoded in the data stream.
 * Instead, it is determined by looking up the tag in the DICOM dictionary.
 *
 * @see DICOM PS3.5 Section 7.1.1 - Implicit VR Little Endian Transfer Syntax
 */

#ifndef PACS_ENCODING_IMPLICIT_VR_CODEC_HPP
#define PACS_ENCODING_IMPLICIT_VR_CODEC_HPP

#include <pacs/core/dicom_dataset.hpp>
#include <pacs/core/dicom_element.hpp>
#include <pacs/core/result.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace pacs::encoding {

/**
 * @brief Encoder/decoder for Implicit VR Little Endian transfer syntax
 *
 * Implicit VR encoding format:
 * ┌─────────────────────────────────────────┐
 * │ Data Element                            │
 * ├───────────┬───────────┬─────────────────┤
 * │ Group     │ Element   │ Length    │Value│
 * │ (2 bytes) │ (2 bytes) │ (4 bytes) │     │
 * │ LE        │ LE        │ LE        │     │
 * └───────────┴───────────┴───────────┴─────┘
 *
 * Notes:
 * - VR is NOT encoded (determined from dictionary lookup)
 * - Length is always 32-bit little-endian
 * - Undefined length (0xFFFFFFFF) is used for sequences
 *
 * @see DICOM PS3.5 Section 7.1.1
 */
class implicit_vr_codec {
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
     * @brief Encode a dataset to bytes using Implicit VR Little Endian
     * @param dataset The dataset to encode
     * @return Encoded byte vector
     */
    [[nodiscard]] static std::vector<uint8_t> encode(
        const core::dicom_dataset& dataset);

    /**
     * @brief Decode bytes to a dataset using Implicit VR Little Endian
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

#endif  // PACS_ENCODING_IMPLICIT_VR_CODEC_HPP
