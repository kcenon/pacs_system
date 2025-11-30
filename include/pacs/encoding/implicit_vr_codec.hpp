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

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace pacs::encoding {

/**
 * @brief Error codes for codec operations
 */
enum class codec_error {
    success = 0,
    invalid_tag,           ///< Invalid or malformed tag
    invalid_length,        ///< Length field is invalid
    insufficient_data,     ///< Not enough data to decode
    invalid_sequence,      ///< Malformed sequence structure
    unknown_vr,            ///< VR could not be determined from dictionary
    encoding_failed,       ///< General encoding failure
    decoding_failed        ///< General decoding failure
};

/**
 * @brief Converts codec_error to human-readable string
 * @param error The error code
 * @return String description of the error
 */
[[nodiscard]] std::string to_string(codec_error error);

/**
 * @brief Result type for codec operations (C++20 compatible)
 *
 * A simple result type that holds either a value or an error.
 * This is a C++20 compatible alternative to std::expected (C++23).
 *
 * @tparam T The success value type
 */
template <typename T>
class codec_result {
public:
    /// Create a success result
    codec_result(T value) : data_(std::move(value)) {}

    /// Create an error result
    codec_result(codec_error error) : data_(error) {}

    /// Check if the result is successful
    [[nodiscard]] bool has_value() const noexcept {
        return std::holds_alternative<T>(data_);
    }

    /// Check if the result is successful (operator bool)
    explicit operator bool() const noexcept { return has_value(); }

    /// Get the value (undefined behavior if error)
    [[nodiscard]] T& value() & { return std::get<T>(data_); }
    [[nodiscard]] const T& value() const& { return std::get<T>(data_); }
    [[nodiscard]] T&& value() && { return std::get<T>(std::move(data_)); }

    /// Dereference operator (undefined behavior if error)
    [[nodiscard]] T& operator*() & { return value(); }
    [[nodiscard]] const T& operator*() const& { return value(); }
    [[nodiscard]] T&& operator*() && { return std::move(*this).value(); }

    /// Arrow operator (undefined behavior if error)
    [[nodiscard]] T* operator->() { return &value(); }
    [[nodiscard]] const T* operator->() const { return &value(); }

    /// Get the error (undefined behavior if success)
    [[nodiscard]] codec_error error() const { return std::get<codec_error>(data_); }

private:
    std::variant<T, codec_error> data_;
};

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
     * @brief Result type for decode operations
     */
    template <typename T>
    using result = codec_result<T>;

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
