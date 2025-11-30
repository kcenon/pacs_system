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
