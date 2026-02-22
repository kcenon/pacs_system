// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
