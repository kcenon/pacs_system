#ifndef PACS_NETWORK_PDU_DECODER_HPP
#define PACS_NETWORK_PDU_DECODER_HPP

#include "pdu_types.hpp"
#include "pacs/core/result.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <variant>
#include <vector>

namespace pacs::network {

/**
 * @brief PDU variant type representing any DICOM Upper Layer PDU.
 *
 * This variant can hold any of the 7 PDU types defined in DICOM PS3.8.
 * Using std::variant provides type-safe handling of different PDU types
 * without dynamic memory allocation.
 */
struct release_rq_pdu {};  ///< A-RELEASE-RQ has no data fields
struct release_rp_pdu {};  ///< A-RELEASE-RP has no data fields

/**
 * @brief A-ABORT PDU data.
 */
struct abort_pdu {
    abort_source source;   ///< Source of abort
    abort_reason reason;   ///< Reason for abort

    abort_pdu() : source(abort_source::service_user),
                  reason(abort_reason::not_specified) {}
    abort_pdu(abort_source src, abort_reason rsn)
        : source(src), reason(rsn) {}
};

/**
 * @brief P-DATA-TF PDU data.
 */
struct p_data_tf_pdu {
    std::vector<presentation_data_value> pdvs;  ///< Presentation Data Values

    p_data_tf_pdu() = default;
    explicit p_data_tf_pdu(std::vector<presentation_data_value> values)
        : pdvs(std::move(values)) {}
};

/**
 * @brief Variant type that can hold any PDU.
 */
using pdu = std::variant<
    associate_rq,
    associate_ac,
    associate_rj,
    p_data_tf_pdu,
    release_rq_pdu,
    release_rp_pdu,
    abort_pdu
>;

/**
 * @brief Error codes for PDU decoding errors.
 */
enum class pdu_decode_error : int {
    success = 0,
    incomplete_header = 1,      ///< Less than 6 bytes available
    incomplete_pdu = 2,         ///< PDU length exceeds available data
    invalid_pdu_type = 3,       ///< Unknown PDU type byte
    invalid_protocol_version = 4, ///< Unsupported protocol version
    invalid_item_type = 5,      ///< Unknown item type in variable items
    malformed_pdu = 6,          ///< PDU structure is invalid
    buffer_overflow = 7,        ///< Item length exceeds PDU bounds
};

/**
 * @brief Convert pdu_decode_error to string description.
 */
[[nodiscard]] constexpr const char* to_string(pdu_decode_error err) noexcept {
    switch (err) {
        case pdu_decode_error::success: return "Success";
        case pdu_decode_error::incomplete_header: return "Incomplete PDU header";
        case pdu_decode_error::incomplete_pdu: return "Incomplete PDU data";
        case pdu_decode_error::invalid_pdu_type: return "Invalid PDU type";
        case pdu_decode_error::invalid_protocol_version: return "Invalid protocol version";
        case pdu_decode_error::invalid_item_type: return "Invalid item type";
        case pdu_decode_error::malformed_pdu: return "Malformed PDU";
        case pdu_decode_error::buffer_overflow: return "Buffer overflow";
        default: return "Unknown error";
    }
}

/// Result type alias for PDU decoding operations using standardized pacs::Result<T>
template<typename T>
using DecodeResult = pacs::Result<T>;

/**
 * @brief Decoder for DICOM PDU (Protocol Data Unit) messages.
 *
 * This class provides static methods to decode various PDU types
 * according to DICOM PS3.8 Upper Layer Protocol.
 *
 * PDU Structure:
 * @code
 * ┌─────────────────────────────────────┐
 * │ PDU Header                          │
 * ├───────────┬───────────┬─────────────┤
 * │ Type      │ Reserved  │ Length      │
 * │ (1 byte)  │ (1 byte)  │ (4 bytes)   │
 * └───────────┴───────────┴─────────────┘
 * │ PDU Data (variable)                 │
 * └─────────────────────────────────────┘
 * @endcode
 *
 * @see DICOM PS3.8 Section 9 - Upper Layer Protocol
 */
class pdu_decoder {
public:
    /// @name General Decoding
    /// @{

    /**
     * @brief Decode any PDU from bytes.
     * @param data Input byte buffer
     * @return Result containing decoded PDU or error
     *
     * Automatically detects PDU type from the first byte and
     * dispatches to the appropriate specific decoder.
     */
    [[nodiscard]] static DecodeResult<pdu> decode(std::span<const uint8_t> data);

    /**
     * @brief Check if a complete PDU is available in the buffer.
     * @param data Input byte buffer
     * @return PDU length if complete PDU available, std::nullopt otherwise
     *
     * Returns the total PDU length (header + data) if at least one
     * complete PDU is present in the buffer. Useful for streaming
     * protocols where data arrives in chunks.
     */
    [[nodiscard]] static std::optional<size_t> pdu_length(
        std::span<const uint8_t> data);

    /**
     * @brief Get the PDU type from buffer without full decoding.
     * @param data Input byte buffer (must have at least 1 byte)
     * @return PDU type if valid, std::nullopt if invalid or insufficient data
     */
    [[nodiscard]] static std::optional<pdu_type> peek_pdu_type(
        std::span<const uint8_t> data);

    /// @}

    /// @name Specific Decoders
    /// @{

    /**
     * @brief Decode an A-ASSOCIATE-RQ PDU.
     * @param data Input byte buffer
     * @return Result containing associate_rq or error
     */
    [[nodiscard]] static DecodeResult<associate_rq> decode_associate_rq(
        std::span<const uint8_t> data);

    /**
     * @brief Decode an A-ASSOCIATE-AC PDU.
     * @param data Input byte buffer
     * @return Result containing associate_ac or error
     */
    [[nodiscard]] static DecodeResult<associate_ac> decode_associate_ac(
        std::span<const uint8_t> data);

    /**
     * @brief Decode an A-ASSOCIATE-RJ PDU.
     * @param data Input byte buffer
     * @return Result containing associate_rj or error
     */
    [[nodiscard]] static DecodeResult<associate_rj> decode_associate_rj(
        std::span<const uint8_t> data);

    /**
     * @brief Decode a P-DATA-TF PDU.
     * @param data Input byte buffer
     * @return Result containing p_data_tf_pdu or error
     */
    [[nodiscard]] static DecodeResult<p_data_tf_pdu> decode_p_data_tf(
        std::span<const uint8_t> data);

    /**
     * @brief Decode an A-RELEASE-RQ PDU.
     * @param data Input byte buffer
     * @return Result containing release_rq_pdu or error
     */
    [[nodiscard]] static DecodeResult<release_rq_pdu> decode_release_rq(
        std::span<const uint8_t> data);

    /**
     * @brief Decode an A-RELEASE-RP PDU.
     * @param data Input byte buffer
     * @return Result containing release_rp_pdu or error
     */
    [[nodiscard]] static DecodeResult<release_rp_pdu> decode_release_rp(
        std::span<const uint8_t> data);

    /**
     * @brief Decode an A-ABORT PDU.
     * @param data Input byte buffer
     * @return Result containing abort_pdu or error
     */
    [[nodiscard]] static DecodeResult<abort_pdu> decode_abort(
        std::span<const uint8_t> data);

    /// @}

private:
    /// @name Helper Functions
    /// @{

    /**
     * @brief Read a 16-bit unsigned integer in big-endian format.
     */
    [[nodiscard]] static uint16_t read_uint16_be(
        std::span<const uint8_t> data, size_t offset);

    /**
     * @brief Read a 32-bit unsigned integer in big-endian format.
     */
    [[nodiscard]] static uint32_t read_uint32_be(
        std::span<const uint8_t> data, size_t offset);

    /**
     * @brief Read an AE Title (16 bytes, space-trimmed).
     */
    [[nodiscard]] static std::string read_ae_title(
        std::span<const uint8_t> data, size_t offset);

    /**
     * @brief Read a UID string (trim trailing null/space padding).
     */
    [[nodiscard]] static std::string read_uid(
        std::span<const uint8_t> data, size_t offset, size_t length);

    /**
     * @brief Validate PDU header and extract length.
     * @param data Input data
     * @param expected_type Expected PDU type (0 for any)
     * @return Pair of (is_valid, pdu_length) or error
     */
    [[nodiscard]] static DecodeResult<uint32_t> validate_pdu_header(
        std::span<const uint8_t> data, uint8_t expected_type = 0);

    /**
     * @brief Decode variable items from ASSOCIATE-RQ/AC PDUs.
     */
    [[nodiscard]] static DecodeResult<std::tuple<
        std::string,                           // application_context
        std::vector<presentation_context_rq>,  // presentation_contexts_rq
        std::vector<presentation_context_ac>,  // presentation_contexts_ac
        user_information                       // user_info
    >> decode_variable_items(std::span<const uint8_t> data, bool is_rq);

    /**
     * @brief Decode User Information sub-items.
     */
    [[nodiscard]] static DecodeResult<user_information> decode_user_info_item(
        std::span<const uint8_t> data);

    /// @}
};

}  // namespace pacs::network

#endif  // PACS_NETWORK_PDU_DECODER_HPP
