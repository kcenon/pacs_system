/**
 * @file command_field.hpp
 * @brief DIMSE command field enumeration
 *
 * This file defines the DIMSE command field values as specified in DICOM PS3.7.
 * Command fields identify the type of DIMSE operation being performed.
 *
 * @see DICOM PS3.7 Section 9.3 - DIMSE-C Service and Protocol
 * @see DICOM PS3.7 Section 10.3 - DIMSE-N Service and Protocol
 */

#ifndef PACS_NETWORK_DIMSE_COMMAND_FIELD_HPP
#define PACS_NETWORK_DIMSE_COMMAND_FIELD_HPP

#include <cstdint>
#include <string_view>

namespace pacs::network::dimse {

/**
 * @brief DIMSE command field values
 *
 * These values represent the Command Field (0000,0100) in DICOM command sets.
 * Request commands have values in the range 0x0001-0x0FFF,
 * while response commands have values in the range 0x8001-0x8FFF.
 *
 * The pattern is: response = request | 0x8000
 */
enum class command_field : uint16_t {
    // ========================================================================
    // DIMSE-C Commands (Composite SOP Classes)
    // ========================================================================

    /// C-STORE Request - Store composite SOP instance
    c_store_rq = 0x0001,
    /// C-STORE Response
    c_store_rsp = 0x8001,

    /// C-GET Request - Retrieve composite SOP instances
    c_get_rq = 0x0010,
    /// C-GET Response
    c_get_rsp = 0x8010,

    /// C-FIND Request - Query for matching instances
    c_find_rq = 0x0020,
    /// C-FIND Response
    c_find_rsp = 0x8020,

    /// C-MOVE Request - Move composite SOP instances
    c_move_rq = 0x0021,
    /// C-MOVE Response
    c_move_rsp = 0x8021,

    /// C-ECHO Request - Verify DICOM connection
    c_echo_rq = 0x0030,
    /// C-ECHO Response
    c_echo_rsp = 0x8030,

    /// C-CANCEL Request - Cancel pending operation
    c_cancel_rq = 0x0FFF,

    // ========================================================================
    // DIMSE-N Commands (Normalized SOP Classes)
    // ========================================================================

    /// N-EVENT-REPORT Request - Report event notification
    n_event_report_rq = 0x0100,
    /// N-EVENT-REPORT Response
    n_event_report_rsp = 0x8100,

    /// N-GET Request - Get attribute values
    n_get_rq = 0x0110,
    /// N-GET Response
    n_get_rsp = 0x8110,

    /// N-SET Request - Set attribute values
    n_set_rq = 0x0120,
    /// N-SET Response
    n_set_rsp = 0x8120,

    /// N-ACTION Request - Request action
    n_action_rq = 0x0130,
    /// N-ACTION Response
    n_action_rsp = 0x8130,

    /// N-CREATE Request - Create SOP instance
    n_create_rq = 0x0140,
    /// N-CREATE Response
    n_create_rsp = 0x8140,

    /// N-DELETE Request - Delete SOP instance
    n_delete_rq = 0x0150,
    /// N-DELETE Response
    n_delete_rsp = 0x8150,
};

/// @name Command Field Utilities
/// @{

/**
 * @brief Check if a command field represents a request
 * @param cmd The command field to check
 * @return true if the command is a request (not a response)
 */
[[nodiscard]] constexpr bool is_request(command_field cmd) noexcept {
    return (static_cast<uint16_t>(cmd) & 0x8000) == 0;
}

/**
 * @brief Check if a command field represents a response
 * @param cmd The command field to check
 * @return true if the command is a response
 */
[[nodiscard]] constexpr bool is_response(command_field cmd) noexcept {
    return (static_cast<uint16_t>(cmd) & 0x8000) != 0;
}

/**
 * @brief Check if a command is a DIMSE-C command
 * @param cmd The command field to check
 * @return true if the command is a DIMSE-C (composite) command
 */
[[nodiscard]] constexpr bool is_dimse_c(command_field cmd) noexcept {
    // Remove response bit (0x8000) and check range
    const auto value = static_cast<uint16_t>(cmd) & 0x7FFF;
    return value <= 0x0030 || value == 0x0FFF;
}

/**
 * @brief Check if a command is a DIMSE-N command
 * @param cmd The command field to check
 * @return true if the command is a DIMSE-N (normalized) command
 */
[[nodiscard]] constexpr bool is_dimse_n(command_field cmd) noexcept {
    // Remove response bit (0x8000) and check range
    const auto value = static_cast<uint16_t>(cmd) & 0x7FFF;
    return value >= 0x0100 && value <= 0x0150;
}

/**
 * @brief Get the corresponding response command for a request
 * @param request The request command field
 * @return The corresponding response command field
 * @note Behavior is undefined if called with C-CANCEL-RQ (has no response)
 */
[[nodiscard]] constexpr command_field get_response_command(
    command_field request) noexcept {
    return static_cast<command_field>(static_cast<uint16_t>(request) | 0x8000);
}

/**
 * @brief Get the corresponding request command for a response
 * @param response The response command field
 * @return The corresponding request command field
 */
[[nodiscard]] constexpr command_field get_request_command(
    command_field response) noexcept {
    return static_cast<command_field>(static_cast<uint16_t>(response) & 0x7FFF);
}

/**
 * @brief Convert command field to string representation
 * @param cmd The command field to convert
 * @return Human-readable string representation
 */
[[nodiscard]] constexpr std::string_view to_string(command_field cmd) noexcept {
    switch (cmd) {
        case command_field::c_store_rq: return "C-STORE-RQ";
        case command_field::c_store_rsp: return "C-STORE-RSP";
        case command_field::c_get_rq: return "C-GET-RQ";
        case command_field::c_get_rsp: return "C-GET-RSP";
        case command_field::c_find_rq: return "C-FIND-RQ";
        case command_field::c_find_rsp: return "C-FIND-RSP";
        case command_field::c_move_rq: return "C-MOVE-RQ";
        case command_field::c_move_rsp: return "C-MOVE-RSP";
        case command_field::c_echo_rq: return "C-ECHO-RQ";
        case command_field::c_echo_rsp: return "C-ECHO-RSP";
        case command_field::c_cancel_rq: return "C-CANCEL-RQ";
        case command_field::n_event_report_rq: return "N-EVENT-REPORT-RQ";
        case command_field::n_event_report_rsp: return "N-EVENT-REPORT-RSP";
        case command_field::n_get_rq: return "N-GET-RQ";
        case command_field::n_get_rsp: return "N-GET-RSP";
        case command_field::n_set_rq: return "N-SET-RQ";
        case command_field::n_set_rsp: return "N-SET-RSP";
        case command_field::n_action_rq: return "N-ACTION-RQ";
        case command_field::n_action_rsp: return "N-ACTION-RSP";
        case command_field::n_create_rq: return "N-CREATE-RQ";
        case command_field::n_create_rsp: return "N-CREATE-RSP";
        case command_field::n_delete_rq: return "N-DELETE-RQ";
        case command_field::n_delete_rsp: return "N-DELETE-RSP";
        default: return "UNKNOWN";
    }
}

/// @}

}  // namespace pacs::network::dimse

#endif  // PACS_NETWORK_DIMSE_COMMAND_FIELD_HPP
