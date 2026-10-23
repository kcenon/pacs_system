// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file scp_transfer_syntaxes.h
 * @brief Transfer Syntaxes that PACS_SCP accepts during association negotiation
 *
 * dicom_server and v2::dicom_association_handler both negotiate with this
 * list. Section 4 of docs/DICOM_CONFORMANCE_STATEMENT.md documents it, and
 * tests/network/scp_transfer_syntaxes_test.cpp fails when the two disagree.
 *
 * @see Issue #1275 - Transfer syntax support flags and the Conformance Statement
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_NETWORK_DETAIL_SCP_TRANSFER_SYNTAXES_HPP
#define PACS_NETWORK_DETAIL_SCP_TRANSFER_SYNTAXES_HPP

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace kcenon::pacs::network::detail {

/**
 * @brief Transfer Syntax UIDs that PACS_SCP accepts, compressed first.
 *
 * The SCP accepts the encapsulated syntaxes whether or not this build has
 * their codec, because it stores the pixel data without decoding it.
 * Explicit VR Big Endian and Deflated Explicit VR Little Endian are absent
 * because dimse_message encodes and decodes datasets only in Implicit and
 * Explicit VR Little Endian. HEVC, JPEG XL, and Frame Deflate are absent
 * because their codecs are stubs (transfer_syntax::is_supported() is false).
 */
inline constexpr std::array<std::string_view, 12> scp_transfer_syntaxes = {{
    "1.2.840.10008.1.2.4.201",  // HTJ2K Lossless Only
    "1.2.840.10008.1.2.4.202",  // HTJ2K RPCL Lossless Only
    "1.2.840.10008.1.2.4.203",  // HTJ2K (Lossless or Lossy)
    "1.2.840.10008.1.2.4.90",   // JPEG 2000 Lossless
    "1.2.840.10008.1.2.4.91",   // JPEG 2000 Lossy
    "1.2.840.10008.1.2.4.80",   // JPEG-LS Lossless
    "1.2.840.10008.1.2.4.81",   // JPEG-LS Near-Lossless
    "1.2.840.10008.1.2.4.70",   // JPEG Lossless
    "1.2.840.10008.1.2.4.50",   // JPEG Baseline
    "1.2.840.10008.1.2.5",      // RLE Lossless
    "1.2.840.10008.1.2.1",      // Explicit VR Little Endian
    "1.2.840.10008.1.2",        // Implicit VR Little Endian
}};

/**
 * @brief Returns scp_transfer_syntaxes in the form scp_config expects.
 * @return The UIDs in the same order
 */
[[nodiscard]] inline std::vector<std::string> scp_transfer_syntax_list() {
    return {scp_transfer_syntaxes.begin(), scp_transfer_syntaxes.end()};
}

}  // namespace kcenon::pacs::network::detail

#endif  // PACS_NETWORK_DETAIL_SCP_TRANSFER_SYNTAXES_HPP
