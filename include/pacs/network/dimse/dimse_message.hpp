/**
 * @file dimse_message.hpp
 * @brief DIMSE message encoding and decoding
 *
 * This file provides the dimse_message class for constructing and parsing
 * DICOM Message Service Element (DIMSE) messages.
 *
 * @see DICOM PS3.7 Section 6 - Message Structure
 * @see DICOM PS3.7 Section 9 - DIMSE-C Services
 * @see DICOM PS3.7 Section 10 - DIMSE-N Services
 */

#ifndef PACS_NETWORK_DIMSE_DIMSE_MESSAGE_HPP
#define PACS_NETWORK_DIMSE_DIMSE_MESSAGE_HPP

#include "command_field.hpp"
#include "status_codes.hpp"

#include <pacs/core/dicom_dataset.hpp>
#include <pacs/core/dicom_tag.hpp>
#include <pacs/core/result.hpp>
#include <pacs/encoding/transfer_syntax.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace pacs::network::dimse {

/// @name DIMSE Command Tags
/// @{

/// Command Group Length (0000,0000) - UL
constexpr core::dicom_tag tag_command_group_length{0x0000, 0x0000};

/// Affected SOP Class UID (0000,0002) - UI
constexpr core::dicom_tag tag_affected_sop_class_uid{0x0000, 0x0002};

/// Requested SOP Class UID (0000,0003) - UI
constexpr core::dicom_tag tag_requested_sop_class_uid{0x0000, 0x0003};

/// Command Field (0000,0100) - US
constexpr core::dicom_tag tag_command_field{0x0000, 0x0100};

/// Message ID (0000,0110) - US
constexpr core::dicom_tag tag_message_id{0x0000, 0x0110};

/// Message ID Being Responded To (0000,0120) - US
constexpr core::dicom_tag tag_message_id_responded_to{0x0000, 0x0120};

/// Move Destination (0000,0600) - AE
constexpr core::dicom_tag tag_move_destination{0x0000, 0x0600};

/// Priority (0000,0700) - US
constexpr core::dicom_tag tag_priority{0x0000, 0x0700};

/// Command Data Set Type (0000,0800) - US
constexpr core::dicom_tag tag_command_data_set_type{0x0000, 0x0800};

/// Status (0000,0900) - US
constexpr core::dicom_tag tag_status{0x0000, 0x0900};

/// Offending Element (0000,0901) - AT
constexpr core::dicom_tag tag_offending_element{0x0000, 0x0901};

/// Error Comment (0000,0902) - LO
constexpr core::dicom_tag tag_error_comment{0x0000, 0x0902};

/// Error ID (0000,0903) - US
constexpr core::dicom_tag tag_error_id{0x0000, 0x0903};

/// Affected SOP Instance UID (0000,1000) - UI
constexpr core::dicom_tag tag_affected_sop_instance_uid{0x0000, 0x1000};

/// Requested SOP Instance UID (0000,1001) - UI
constexpr core::dicom_tag tag_requested_sop_instance_uid{0x0000, 0x1001};

/// Event Type ID (0000,1002) - US
constexpr core::dicom_tag tag_event_type_id{0x0000, 0x1002};

/// Attribute Identifier List (0000,1005) - AT
constexpr core::dicom_tag tag_attribute_identifier_list{0x0000, 0x1005};

/// Action Type ID (0000,1008) - US
constexpr core::dicom_tag tag_action_type_id{0x0000, 0x1008};

/// Number of Remaining Sub-operations (0000,1020) - US
constexpr core::dicom_tag tag_number_of_remaining_subops{0x0000, 0x1020};

/// Number of Completed Sub-operations (0000,1021) - US
constexpr core::dicom_tag tag_number_of_completed_subops{0x0000, 0x1021};

/// Number of Failed Sub-operations (0000,1022) - US
constexpr core::dicom_tag tag_number_of_failed_subops{0x0000, 0x1022};

/// Number of Warning Sub-operations (0000,1023) - US
constexpr core::dicom_tag tag_number_of_warning_subops{0x0000, 0x1023};

/// Move Originator Application Entity Title (0000,1030) - AE
constexpr core::dicom_tag tag_move_originator_aet{0x0000, 0x1030};

/// Move Originator Message ID (0000,1031) - US
constexpr core::dicom_tag tag_move_originator_message_id{0x0000, 0x1031};

/// @}

/// @name Command Data Set Type Values
/// @{

/// Null value indicating no data set present
constexpr uint16_t command_data_set_type_null = 0x0101;

/// Value indicating data set is present (any value other than 0x0101)
constexpr uint16_t command_data_set_type_present = 0x0001;

/// @}

/// @name Priority Values
/// @{

/// Low priority
constexpr uint16_t priority_low = 0x0002;

/// Medium priority
constexpr uint16_t priority_medium = 0x0000;

/// High priority
constexpr uint16_t priority_high = 0x0001;

/// @}

/**
 * @brief Error codes for DIMSE message operations
 */
enum class dimse_error {
    success = 0,
    invalid_command_set,
    missing_required_field,
    invalid_data_format,
    encoding_error,
    decoding_error,
};

/**
 * @brief Get error description
 */
[[nodiscard]] constexpr std::string_view to_string(dimse_error err) noexcept {
    switch (err) {
        case dimse_error::success: return "Success";
        case dimse_error::invalid_command_set: return "Invalid command set";
        case dimse_error::missing_required_field: return "Missing required field";
        case dimse_error::invalid_data_format: return "Invalid data format";
        case dimse_error::encoding_error: return "Encoding error";
        case dimse_error::decoding_error: return "Decoding error";
        default: return "Unknown error";
    }
}

/**
 * @brief Result type for DIMSE operations using standardized pacs::Result<T>
 *
 * @tparam T The success value type
 */
template <typename T>
using dimse_result = pacs::Result<T>;

/**
 * @brief DIMSE message class
 *
 * Represents a DICOM Message Service Element (DIMSE) message, consisting of
 * a command set and an optional data set.
 *
 * The command set is always encoded using Implicit VR Little Endian.
 * The data set uses the negotiated transfer syntax.
 *
 * @example
 * @code
 * // Create a C-STORE request
 * dimse_message msg{command_field::c_store_rq, 1};
 * msg.set_affected_sop_class_uid("1.2.840.10008.5.1.4.1.1.2");
 * msg.set_affected_sop_instance_uid("1.2.3.4.5.6.7.8.9");
 *
 * core::dicom_dataset ds;
 * ds.set_string(core::tags::patient_name, encoding::vr_type::PN, "DOE^JOHN");
 * msg.set_dataset(std::move(ds));
 *
 * // Encode the message
 * auto encoded = dimse_message::encode(msg, transfer_syntax::explicit_vr_le);
 * @endcode
 */
class dimse_message {
public:
    // ========================================================================
    // Construction
    // ========================================================================

    /**
     * @brief Construct a new DIMSE message
     * @param cmd The command field (type of operation)
     * @param message_id Unique message identifier
     */
    dimse_message(command_field cmd, uint16_t message_id);

    /**
     * @brief Default constructor (creates an invalid message)
     */
    dimse_message() = default;

    /**
     * @brief Copy constructor
     */
    dimse_message(const dimse_message&) = default;

    /**
     * @brief Move constructor
     */
    dimse_message(dimse_message&&) noexcept = default;

    /**
     * @brief Copy assignment
     */
    auto operator=(const dimse_message&) -> dimse_message& = default;

    /**
     * @brief Move assignment
     */
    auto operator=(dimse_message&&) noexcept -> dimse_message& = default;

    /**
     * @brief Destructor
     */
    ~dimse_message() = default;

    // ========================================================================
    // Command Set Access
    // ========================================================================

    /**
     * @brief Get the command field
     * @return The command field value
     */
    [[nodiscard]] auto command() const noexcept -> command_field;

    /**
     * @brief Get the message ID
     * @return The message ID
     */
    [[nodiscard]] auto message_id() const noexcept -> uint16_t;

    /**
     * @brief Get mutable reference to the command set
     * @return Reference to the command dataset
     */
    [[nodiscard]] auto command_set() noexcept -> core::dicom_dataset&;

    /**
     * @brief Get const reference to the command set
     * @return Const reference to the command dataset
     */
    [[nodiscard]] auto command_set() const noexcept -> const core::dicom_dataset&;

    // ========================================================================
    // Dataset Access
    // ========================================================================

    /**
     * @brief Check if the message has an associated data set
     * @return true if a data set is present
     */
    [[nodiscard]] auto has_dataset() const noexcept -> bool;

    /**
     * @brief Get mutable reference to the data set
     * @return Reference to the data set
     * @throws std::runtime_error if no data set is present
     */
    [[nodiscard]] auto dataset() -> core::dicom_dataset&;

    /**
     * @brief Get const reference to the data set
     * @return Const reference to the data set
     * @throws std::runtime_error if no data set is present
     */
    [[nodiscard]] auto dataset() const -> const core::dicom_dataset&;

    /**
     * @brief Set the data set for this message
     * @param ds The data set to associate with this message
     */
    void set_dataset(core::dicom_dataset ds);

    /**
     * @brief Remove the data set from this message
     */
    void clear_dataset() noexcept;

    // ========================================================================
    // Status (for responses)
    // ========================================================================

    /**
     * @brief Get the status code (for response messages)
     * @return The status code value
     */
    [[nodiscard]] auto status() const -> status_code;

    /**
     * @brief Set the status code (for response messages)
     * @param status The status code to set
     */
    void set_status(status_code status);

    // ========================================================================
    // Common Attributes
    // ========================================================================

    /**
     * @brief Get the Affected SOP Class UID
     * @return The SOP Class UID if present
     */
    [[nodiscard]] auto affected_sop_class_uid() const -> std::string;

    /**
     * @brief Set the Affected SOP Class UID
     * @param uid The SOP Class UID
     */
    void set_affected_sop_class_uid(std::string_view uid);

    /**
     * @brief Get the Affected SOP Instance UID
     * @return The SOP Instance UID if present
     */
    [[nodiscard]] auto affected_sop_instance_uid() const -> std::string;

    /**
     * @brief Set the Affected SOP Instance UID
     * @param uid The SOP Instance UID
     */
    void set_affected_sop_instance_uid(std::string_view uid);

    /**
     * @brief Get the priority
     * @return The priority value (0=medium, 1=high, 2=low)
     */
    [[nodiscard]] auto priority() const -> uint16_t;

    /**
     * @brief Set the priority
     * @param priority The priority value
     */
    void set_priority(uint16_t priority);

    /**
     * @brief Get the Message ID Being Responded To (for responses)
     * @return The message ID being responded to
     */
    [[nodiscard]] auto message_id_responded_to() const -> uint16_t;

    /**
     * @brief Set the Message ID Being Responded To (for responses)
     * @param id The message ID being responded to
     */
    void set_message_id_responded_to(uint16_t id);

    // ========================================================================
    // DIMSE-N Specific Attributes
    // ========================================================================

    /**
     * @brief Get the Requested SOP Class UID (for N-SET, N-GET, N-ACTION, N-DELETE)
     * @return The SOP Class UID if present
     */
    [[nodiscard]] auto requested_sop_class_uid() const -> std::string;

    /**
     * @brief Set the Requested SOP Class UID
     * @param uid The SOP Class UID
     */
    void set_requested_sop_class_uid(std::string_view uid);

    /**
     * @brief Get the Requested SOP Instance UID (for N-SET, N-GET, N-ACTION, N-DELETE)
     * @return The SOP Instance UID if present
     */
    [[nodiscard]] auto requested_sop_instance_uid() const -> std::string;

    /**
     * @brief Set the Requested SOP Instance UID
     * @param uid The SOP Instance UID
     */
    void set_requested_sop_instance_uid(std::string_view uid);

    /**
     * @brief Get the Event Type ID (for N-EVENT-REPORT)
     * @return The event type ID if present
     */
    [[nodiscard]] auto event_type_id() const -> std::optional<uint16_t>;

    /**
     * @brief Set the Event Type ID
     * @param type_id The event type ID
     */
    void set_event_type_id(uint16_t type_id);

    /**
     * @brief Get the Action Type ID (for N-ACTION)
     * @return The action type ID if present
     */
    [[nodiscard]] auto action_type_id() const -> std::optional<uint16_t>;

    /**
     * @brief Set the Action Type ID
     * @param type_id The action type ID
     */
    void set_action_type_id(uint16_t type_id);

    /**
     * @brief Get the Attribute Identifier List (for N-GET)
     * @return Vector of tags to retrieve
     */
    [[nodiscard]] auto attribute_identifier_list() const -> std::vector<core::dicom_tag>;

    /**
     * @brief Set the Attribute Identifier List
     * @param tags Vector of tags to retrieve
     */
    void set_attribute_identifier_list(const std::vector<core::dicom_tag>& tags);

    // ========================================================================
    // Sub-operation Counts (for C-GET/C-MOVE responses)
    // ========================================================================

    /**
     * @brief Get the number of remaining sub-operations
     * @return Optional count of remaining sub-operations
     */
    [[nodiscard]] auto remaining_subops() const -> std::optional<uint16_t>;

    /**
     * @brief Set the number of remaining sub-operations
     */
    void set_remaining_subops(uint16_t count);

    /**
     * @brief Get the number of completed sub-operations
     */
    [[nodiscard]] auto completed_subops() const -> std::optional<uint16_t>;

    /**
     * @brief Set the number of completed sub-operations
     */
    void set_completed_subops(uint16_t count);

    /**
     * @brief Get the number of failed sub-operations
     */
    [[nodiscard]] auto failed_subops() const -> std::optional<uint16_t>;

    /**
     * @brief Set the number of failed sub-operations
     */
    void set_failed_subops(uint16_t count);

    /**
     * @brief Get the number of warning sub-operations
     */
    [[nodiscard]] auto warning_subops() const -> std::optional<uint16_t>;

    /**
     * @brief Set the number of warning sub-operations
     */
    void set_warning_subops(uint16_t count);

    // ========================================================================
    // Encoding/Decoding
    // ========================================================================

    /// Encoded message result type: pair of (command_set_bytes, dataset_bytes)
    using encoded_message = std::pair<std::vector<uint8_t>, std::vector<uint8_t>>;

    /**
     * @brief Encode the DIMSE message to bytes
     * @param msg The message to encode
     * @param dataset_ts The transfer syntax for the data set
     * @return Encoded command set and data set bytes as a pair
     *
     * The command set is always encoded using Implicit VR Little Endian.
     * The data set uses the specified transfer syntax.
     */
    [[nodiscard]] static auto encode(
        const dimse_message& msg,
        const encoding::transfer_syntax& dataset_ts)
        -> dimse_result<encoded_message>;

    /**
     * @brief Decode a DIMSE message from bytes
     * @param command_data The encoded command set (Implicit VR LE)
     * @param dataset_data The encoded data set (per transfer syntax)
     * @param dataset_ts The transfer syntax of the data set
     * @return The decoded message or an error
     */
    [[nodiscard]] static auto decode(
        std::span<const uint8_t> command_data,
        std::span<const uint8_t> dataset_data,
        const encoding::transfer_syntax& dataset_ts)
        -> dimse_result<dimse_message>;

    // ========================================================================
    // Validation
    // ========================================================================

    /**
     * @brief Check if the message is valid
     * @return true if the message has valid command field and message ID
     */
    [[nodiscard]] auto is_valid() const noexcept -> bool;

    /**
     * @brief Check if this is a request message
     * @return true if the command field represents a request
     */
    [[nodiscard]] auto is_request() const noexcept -> bool;

    /**
     * @brief Check if this is a response message
     * @return true if the command field represents a response
     */
    [[nodiscard]] auto is_response() const noexcept -> bool;

private:
    /// Update the CommandDataSetType field based on dataset presence
    void update_data_set_type();

    /// Calculate and set CommandGroupLength
    void update_command_group_length();

    command_field command_{};
    uint16_t message_id_{0};
    core::dicom_dataset command_set_;
    std::optional<core::dicom_dataset> dataset_;
};

// ============================================================================
// Factory Functions
// ============================================================================

/**
 * @brief Create a C-ECHO request message
 * @param message_id The message ID
 * @param sop_class_uid The Verification SOP Class UID
 * @return The constructed message
 */
[[nodiscard]] auto make_c_echo_rq(
    uint16_t message_id,
    std::string_view sop_class_uid = "1.2.840.10008.1.1") -> dimse_message;

/**
 * @brief Create a C-ECHO response message
 * @param message_id_responded_to The message ID being responded to
 * @param status The response status code
 * @param sop_class_uid The Verification SOP Class UID
 * @return The constructed message
 */
[[nodiscard]] auto make_c_echo_rsp(
    uint16_t message_id_responded_to,
    status_code status = status_success,
    std::string_view sop_class_uid = "1.2.840.10008.1.1") -> dimse_message;

/**
 * @brief Create a C-STORE request message
 * @param message_id The message ID
 * @param sop_class_uid The SOP Class UID
 * @param sop_instance_uid The SOP Instance UID
 * @param priority Operation priority
 * @return The constructed message
 */
[[nodiscard]] auto make_c_store_rq(
    uint16_t message_id,
    std::string_view sop_class_uid,
    std::string_view sop_instance_uid,
    uint16_t priority = priority_medium) -> dimse_message;

/**
 * @brief Create a C-STORE response message
 * @param message_id_responded_to The message ID being responded to
 * @param sop_class_uid The SOP Class UID
 * @param sop_instance_uid The SOP Instance UID
 * @param status The response status code
 * @return The constructed message
 */
[[nodiscard]] auto make_c_store_rsp(
    uint16_t message_id_responded_to,
    std::string_view sop_class_uid,
    std::string_view sop_instance_uid,
    status_code status = status_success) -> dimse_message;

/**
 * @brief Create a C-FIND request message
 * @param message_id The message ID
 * @param sop_class_uid The Query/Retrieve SOP Class UID
 * @param priority Operation priority
 * @return The constructed message
 */
[[nodiscard]] auto make_c_find_rq(
    uint16_t message_id,
    std::string_view sop_class_uid,
    uint16_t priority = priority_medium) -> dimse_message;

/**
 * @brief Create a C-FIND response message
 * @param message_id_responded_to The message ID being responded to
 * @param sop_class_uid The Query/Retrieve SOP Class UID
 * @param status The response status code
 * @return The constructed message
 */
[[nodiscard]] auto make_c_find_rsp(
    uint16_t message_id_responded_to,
    std::string_view sop_class_uid,
    status_code status) -> dimse_message;

// ============================================================================
// DIMSE-N Factory Functions
// ============================================================================

/**
 * @brief Create an N-CREATE request message
 * @param message_id The message ID
 * @param sop_class_uid The Affected SOP Class UID
 * @param sop_instance_uid Optional Affected SOP Instance UID (may be generated by SCP)
 * @return The constructed message
 */
[[nodiscard]] auto make_n_create_rq(
    uint16_t message_id,
    std::string_view sop_class_uid,
    std::string_view sop_instance_uid = "") -> dimse_message;

/**
 * @brief Create an N-CREATE response message
 * @param message_id_responded_to The message ID being responded to
 * @param sop_class_uid The Affected SOP Class UID
 * @param sop_instance_uid The Affected SOP Instance UID (returned by SCP)
 * @param status The response status code
 * @return The constructed message
 */
[[nodiscard]] auto make_n_create_rsp(
    uint16_t message_id_responded_to,
    std::string_view sop_class_uid,
    std::string_view sop_instance_uid,
    status_code status = status_success) -> dimse_message;

/**
 * @brief Create an N-SET request message
 * @param message_id The message ID
 * @param sop_class_uid The Requested SOP Class UID
 * @param sop_instance_uid The Requested SOP Instance UID
 * @return The constructed message
 */
[[nodiscard]] auto make_n_set_rq(
    uint16_t message_id,
    std::string_view sop_class_uid,
    std::string_view sop_instance_uid) -> dimse_message;

/**
 * @brief Create an N-SET response message
 * @param message_id_responded_to The message ID being responded to
 * @param sop_class_uid The Affected SOP Class UID
 * @param sop_instance_uid The Affected SOP Instance UID
 * @param status The response status code
 * @return The constructed message
 */
[[nodiscard]] auto make_n_set_rsp(
    uint16_t message_id_responded_to,
    std::string_view sop_class_uid,
    std::string_view sop_instance_uid,
    status_code status = status_success) -> dimse_message;

/**
 * @brief Create an N-GET request message
 * @param message_id The message ID
 * @param sop_class_uid The Requested SOP Class UID
 * @param sop_instance_uid The Requested SOP Instance UID
 * @param attribute_tags Optional list of attribute tags to retrieve
 * @return The constructed message
 */
[[nodiscard]] auto make_n_get_rq(
    uint16_t message_id,
    std::string_view sop_class_uid,
    std::string_view sop_instance_uid,
    const std::vector<core::dicom_tag>& attribute_tags = {}) -> dimse_message;

/**
 * @brief Create an N-GET response message
 * @param message_id_responded_to The message ID being responded to
 * @param sop_class_uid The Affected SOP Class UID
 * @param sop_instance_uid The Affected SOP Instance UID
 * @param status The response status code
 * @return The constructed message
 */
[[nodiscard]] auto make_n_get_rsp(
    uint16_t message_id_responded_to,
    std::string_view sop_class_uid,
    std::string_view sop_instance_uid,
    status_code status = status_success) -> dimse_message;

/**
 * @brief Create an N-EVENT-REPORT request message
 * @param message_id The message ID
 * @param sop_class_uid The Affected SOP Class UID
 * @param sop_instance_uid The Affected SOP Instance UID
 * @param event_type_id The event type identifier
 * @return The constructed message
 */
[[nodiscard]] auto make_n_event_report_rq(
    uint16_t message_id,
    std::string_view sop_class_uid,
    std::string_view sop_instance_uid,
    uint16_t event_type_id) -> dimse_message;

/**
 * @brief Create an N-EVENT-REPORT response message
 * @param message_id_responded_to The message ID being responded to
 * @param sop_class_uid The Affected SOP Class UID
 * @param sop_instance_uid The Affected SOP Instance UID
 * @param event_type_id The event type identifier
 * @param status The response status code
 * @return The constructed message
 */
[[nodiscard]] auto make_n_event_report_rsp(
    uint16_t message_id_responded_to,
    std::string_view sop_class_uid,
    std::string_view sop_instance_uid,
    uint16_t event_type_id,
    status_code status = status_success) -> dimse_message;

/**
 * @brief Create an N-ACTION request message
 * @param message_id The message ID
 * @param sop_class_uid The Requested SOP Class UID
 * @param sop_instance_uid The Requested SOP Instance UID
 * @param action_type_id The action type identifier
 * @return The constructed message
 */
[[nodiscard]] auto make_n_action_rq(
    uint16_t message_id,
    std::string_view sop_class_uid,
    std::string_view sop_instance_uid,
    uint16_t action_type_id) -> dimse_message;

/**
 * @brief Create an N-ACTION response message
 * @param message_id_responded_to The message ID being responded to
 * @param sop_class_uid The Affected SOP Class UID
 * @param sop_instance_uid The Affected SOP Instance UID
 * @param action_type_id The action type identifier
 * @param status The response status code
 * @return The constructed message
 */
[[nodiscard]] auto make_n_action_rsp(
    uint16_t message_id_responded_to,
    std::string_view sop_class_uid,
    std::string_view sop_instance_uid,
    uint16_t action_type_id,
    status_code status = status_success) -> dimse_message;

/**
 * @brief Create an N-DELETE request message
 * @param message_id The message ID
 * @param sop_class_uid The Requested SOP Class UID
 * @param sop_instance_uid The Requested SOP Instance UID
 * @return The constructed message
 */
[[nodiscard]] auto make_n_delete_rq(
    uint16_t message_id,
    std::string_view sop_class_uid,
    std::string_view sop_instance_uid) -> dimse_message;

/**
 * @brief Create an N-DELETE response message
 * @param message_id_responded_to The message ID being responded to
 * @param sop_class_uid The Affected SOP Class UID
 * @param sop_instance_uid The Affected SOP Instance UID
 * @param status The response status code
 * @return The constructed message
 */
[[nodiscard]] auto make_n_delete_rsp(
    uint16_t message_id_responded_to,
    std::string_view sop_class_uid,
    std::string_view sop_instance_uid,
    status_code status = status_success) -> dimse_message;

}  // namespace pacs::network::dimse

#endif  // PACS_NETWORK_DIMSE_DIMSE_MESSAGE_HPP
