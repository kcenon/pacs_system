/**
 * @file explicit_vr_big_endian_codec.hpp
 * @brief Encoder/decoder for Explicit VR Big Endian transfer syntax
 *
 * This file provides the explicit_vr_big_endian_codec class for encoding and
 * decoding DICOM data elements using the Explicit VR Big Endian transfer syntax
 * (UID: 1.2.840.10008.1.2.2).
 *
 * This transfer syntax is retired in DICOM 2024 but still required for
 * interoperability with legacy systems.
 *
 * @see DICOM PS3.5 Section 7.1.2 - Explicit VR Transfer Syntaxes
 */

#ifndef PACS_ENCODING_EXPLICIT_VR_BIG_ENDIAN_CODEC_HPP
#define PACS_ENCODING_EXPLICIT_VR_BIG_ENDIAN_CODEC_HPP

#include <pacs/core/dicom_dataset.hpp>
#include <pacs/core/dicom_element.hpp>
#include <pacs/core/result.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace pacs::encoding {

/**
 * @brief Encoder/decoder for Explicit VR Big Endian transfer syntax
 *
 * Explicit VR Big Endian encoding has the same structure as Little Endian
 * but with big-endian byte ordering for all multi-byte numeric values:
 *
 * **Standard VRs (16-bit length):**
 * ┌─────────────────────────────────────────────────────┐
 * │ Data Element                                        │
 * ├───────────┬───────────┬────────┬──────────┬─────────┤
 * │ Group     │ Element   │ VR     │ Length   │ Value   │
 * │ (2 bytes) │ (2 bytes) │(2 char)│ (2 bytes)│         │
 * │ BE        │ BE        │ ASCII  │ BE       │ BE      │
 * └───────────┴───────────┴────────┴──────────┴─────────┘
 *
 * **Extended VRs (32-bit length):**
 * ┌──────────────────────────────────────────────────────────────┐
 * │ Data Element                                                 │
 * ├───────────┬───────────┬────────┬──────────┬──────────┬───────┤
 * │ Group     │ Element   │ VR     │ Reserved │ Length   │ Value │
 * │ (2 bytes) │ (2 bytes) │(2 char)│ (2 bytes)│ (4 bytes)│       │
 * │ BE        │ BE        │ ASCII  │ 0x0000   │ BE       │ BE    │
 * └───────────┴───────────┴────────┴──────────┴──────────┴───────┘
 *
 * Byte swapping is required for:
 * - Tag group and element numbers
 * - Length fields (16-bit and 32-bit)
 * - Numeric VR values (US, UL, SS, SL, FL, FD, AT)
 * - Bulk data VRs (OW, OL, OF, OD)
 *
 * @note This transfer syntax is retired in DICOM 2024 but implementation
 *       is required for legacy system interoperability.
 *
 * @see DICOM PS3.5 Section 7.1.2
 */
class explicit_vr_big_endian_codec {
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
     * @brief Encode a dataset to bytes using Explicit VR Big Endian
     * @param dataset The dataset to encode
     * @return Encoded byte vector
     */
    [[nodiscard]] static std::vector<uint8_t> encode(
        const core::dicom_dataset& dataset);

    /**
     * @brief Decode bytes to a dataset using Explicit VR Big Endian
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

    // ========================================================================
    // Byte Order Conversion
    // ========================================================================

    /**
     * @brief Convert element value from little-endian to big-endian
     * @param vr The VR type of the element
     * @param data The little-endian data to convert
     * @return Big-endian encoded data
     *
     * Performs byte swapping based on VR type:
     * - US/SS: 16-bit swap
     * - UL/SL/FL/AT: 32-bit swap
     * - FD: 64-bit swap
     * - OW: 16-bit word swap for each word
     * - OL/OF: 32-bit swap for each value
     * - OD: 64-bit swap for each value
     * - String VRs: No swap needed
     */
    [[nodiscard]] static std::vector<uint8_t> to_big_endian(
        vr_type vr, std::span<const uint8_t> data);

    /**
     * @brief Convert element value from big-endian to little-endian
     * @param vr The VR type of the element
     * @param data The big-endian data to convert
     * @return Little-endian encoded data
     *
     * Performs the inverse of to_big_endian().
     */
    [[nodiscard]] static std::vector<uint8_t> from_big_endian(
        vr_type vr, std::span<const uint8_t> data);

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

#endif  // PACS_ENCODING_EXPLICIT_VR_BIG_ENDIAN_CODEC_HPP
