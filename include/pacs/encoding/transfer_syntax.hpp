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
 * @file transfer_syntax.hpp
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_ENCODING_TRANSFER_SYNTAX_HPP
#define PACS_ENCODING_TRANSFER_SYNTAX_HPP

#include "pacs/encoding/byte_order.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pacs::encoding {

/**
 * @brief Represents a DICOM Transfer Syntax.
 *
 * Transfer Syntax defines how DICOM data is encoded, including:
 * - Byte ordering (little-endian or big-endian)
 * - VR encoding (implicit or explicit)
 * - Compression (encapsulated pixel data)
 *
 * Each Transfer Syntax is uniquely identified by a UID.
 *
 * @see DICOM PS3.5 Section 10 - Transfer Syntax
 */
class transfer_syntax {
public:
    /**
     * @brief Constructs a transfer_syntax from a UID string.
     * @param uid The Transfer Syntax UID (e.g., "1.2.840.10008.1.2")
     *
     * If the UID is not recognized, the transfer_syntax will be invalid
     * (is_valid() returns false).
     */
    explicit transfer_syntax(std::string_view uid);

    /// @name Property Accessors
    /// @{

    /**
     * @brief Returns the Transfer Syntax UID.
     * @return The UID string (e.g., "1.2.840.10008.1.2")
     */
    [[nodiscard]] std::string_view uid() const noexcept;

    /**
     * @brief Returns the human-readable name of the Transfer Syntax.
     * @return The name (e.g., "Implicit VR Little Endian")
     */
    [[nodiscard]] std::string_view name() const noexcept;

    /**
     * @brief Returns the byte ordering for this Transfer Syntax.
     * @return byte_order::little_endian or byte_order::big_endian
     */
    [[nodiscard]] byte_order endianness() const noexcept;

    /**
     * @brief Returns the VR encoding mode for this Transfer Syntax.
     * @return vr_encoding::implicit or vr_encoding::explicit_vr
     */
    [[nodiscard]] vr_encoding vr_type() const noexcept;

    /**
     * @brief Checks if this Transfer Syntax uses encapsulated (compressed) format.
     * @return true if pixel data is encapsulated (JPEG, JPEG 2000, etc.)
     */
    [[nodiscard]] bool is_encapsulated() const noexcept;

    /**
     * @brief Checks if this Transfer Syntax uses deflate compression.
     * @return true if the entire dataset is deflate-compressed
     */
    [[nodiscard]] bool is_deflated() const noexcept;

    /// @}

    /// @name Validation
    /// @{

    /**
     * @brief Checks if this is a recognized DICOM Transfer Syntax.
     * @return true if the UID was recognized in the registry
     */
    [[nodiscard]] bool is_valid() const noexcept;

    /**
     * @brief Checks if this Transfer Syntax is currently supported.
     * @return true if encoding/decoding is implemented
     *
     * In Phase 1, only uncompressed transfer syntaxes are supported.
     */
    [[nodiscard]] bool is_supported() const noexcept;

    /// @}

    /// @name Standard Transfer Syntax Instances
    /// @{

    /// Implicit VR Little Endian (1.2.840.10008.1.2)
    static const transfer_syntax implicit_vr_little_endian;

    /// Explicit VR Little Endian (1.2.840.10008.1.2.1)
    static const transfer_syntax explicit_vr_little_endian;

    /// Explicit VR Big Endian (1.2.840.10008.1.2.2) - Retired
    static const transfer_syntax explicit_vr_big_endian;

    /// Deflated Explicit VR Little Endian (1.2.840.10008.1.2.1.99)
    static const transfer_syntax deflated_explicit_vr_le;

    /// JPEG Baseline (Process 1) (1.2.840.10008.1.2.4.50)
    static const transfer_syntax jpeg_baseline;

    /// JPEG Lossless, Non-Hierarchical (1.2.840.10008.1.2.4.70)
    static const transfer_syntax jpeg_lossless;

    /// JPEG 2000 Image Compression (Lossless Only) (1.2.840.10008.1.2.4.90)
    static const transfer_syntax jpeg2000_lossless;

    /// JPEG 2000 Image Compression (1.2.840.10008.1.2.4.91)
    static const transfer_syntax jpeg2000_lossy;

    /// RLE Lossless (1.2.840.10008.1.2.5)
    static const transfer_syntax rle_lossless;

    /// HTJ2K Lossless Only (1.2.840.10008.1.2.4.201)
    static const transfer_syntax htj2k_lossless;

    /// HTJ2K RPCL (1.2.840.10008.1.2.4.202) - Lossless Only
    static const transfer_syntax htj2k_rpcl;

    /// HTJ2K (1.2.840.10008.1.2.4.203) - Lossless or Lossy
    static const transfer_syntax htj2k_lossy;

    /// HEVC/H.265 Main Profile / Level 5.1 (1.2.840.10008.1.2.4.107)
    static const transfer_syntax hevc_main;

    /// HEVC/H.265 Main 10 Profile / Level 5.1 (1.2.840.10008.1.2.4.108)
    static const transfer_syntax hevc_main10;

    /// Frame Deflate (1.2.840.10008.1.2.11) - Supplement 244
    static const transfer_syntax frame_deflate;

    /// @}

    /// @name Comparison Operators
    /// @{

    /**
     * @brief Compares two Transfer Syntaxes by UID.
     * @param other The Transfer Syntax to compare with
     * @return true if UIDs are equal
     */
    bool operator==(const transfer_syntax& other) const noexcept;

    /**
     * @brief Compares two Transfer Syntaxes by UID.
     * @param other The Transfer Syntax to compare with
     * @return true if UIDs are not equal
     */
    bool operator!=(const transfer_syntax& other) const noexcept;

    /// @}

private:
    /// Allow registry functions to use private constructor
    friend std::optional<transfer_syntax> find_transfer_syntax(std::string_view);
    friend std::vector<transfer_syntax> supported_transfer_syntaxes();
    friend std::vector<transfer_syntax> all_transfer_syntaxes();

    /// Private constructor for static instances and registry functions
    transfer_syntax(std::string_view uid, std::string_view name,
                    byte_order endian, vr_encoding vr,
                    bool encapsulated, bool deflated, bool supported);

    std::string uid_;
    std::string name_;
    byte_order endianness_;
    vr_encoding vr_type_;
    bool encapsulated_;
    bool deflated_;
    bool valid_;
    bool supported_;
};

/// @name Registry Functions
/// @{

/**
 * @brief Looks up a Transfer Syntax by its UID.
 * @param uid The Transfer Syntax UID to search for
 * @return The transfer_syntax if found, std::nullopt otherwise
 */
[[nodiscard]] std::optional<transfer_syntax> find_transfer_syntax(
    std::string_view uid);

/**
 * @brief Returns a list of all supported Transfer Syntaxes.
 * @return Vector of supported transfer_syntax instances
 */
[[nodiscard]] std::vector<transfer_syntax> supported_transfer_syntaxes();

/**
 * @brief Returns a list of all known Transfer Syntaxes.
 * @return Vector of all registered transfer_syntax instances
 */
[[nodiscard]] std::vector<transfer_syntax> all_transfer_syntaxes();

/// @}

}  // namespace pacs::encoding

#endif  // PACS_ENCODING_TRANSFER_SYNTAX_HPP
