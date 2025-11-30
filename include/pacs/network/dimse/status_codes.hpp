/**
 * @file status_codes.hpp
 * @brief DIMSE status codes
 *
 * This file defines the DIMSE status codes as specified in DICOM PS3.7.
 * Status codes are returned in response messages to indicate the result
 * of a DIMSE operation.
 *
 * @see DICOM PS3.7 Annex C - Status Type Encoding
 */

#ifndef PACS_NETWORK_DIMSE_STATUS_CODES_HPP
#define PACS_NETWORK_DIMSE_STATUS_CODES_HPP

#include <cstdint>
#include <string_view>

namespace pacs::network::dimse {

/**
 * @brief DIMSE status code type alias
 *
 * Status codes are 16-bit unsigned integers.
 * The high nibble indicates the status type (Success, Warning, Failure, etc.)
 */
using status_code = uint16_t;

/// @name General Status Codes
/// @{

/// Operation completed successfully
constexpr status_code status_success = 0x0000;

/// Operation pending (more results available)
constexpr status_code status_pending = 0xFF00;

/// Pending with optional keys not supported
constexpr status_code status_pending_warning = 0xFF01;

/// Operation was canceled
constexpr status_code status_cancel = 0xFE00;

/// @}

/// @name Failure Status Codes (0xCxxx, 0xAxxx)
/// @{

/// Refused: Out of resources
constexpr status_code status_refused_out_of_resources = 0xA700;

/// Refused: Out of resources - Unable to calculate number of matches
constexpr status_code status_refused_out_of_resources_matches = 0xA701;

/// Refused: Out of resources - Unable to perform sub-operations
constexpr status_code status_refused_out_of_resources_subops = 0xA702;

/// Refused: Move destination unknown
constexpr status_code status_refused_move_destination_unknown = 0xA801;

/// Refused: SOP class not supported
constexpr status_code status_refused_sop_class_not_supported = 0x0122;

/// Error: Data set does not match SOP class
constexpr status_code status_error_dataset_mismatch = 0xA900;

/// Error: Cannot understand
constexpr status_code status_error_cannot_understand = 0xC000;

/// Error: Unable to process
constexpr status_code status_error_unable_to_process = 0xC001;

/// Error: Duplicate SOP instance
constexpr status_code status_error_duplicate_sop_instance = 0x0111;

/// Error: Missing attribute
constexpr status_code status_error_missing_attribute = 0x0120;

/// Error: Missing attribute value
constexpr status_code status_error_missing_attribute_value = 0x0121;

/// @}

/// @name Warning Status Codes (0xBxxx)
/// @{

/// Warning: Coercion of data elements
constexpr status_code status_warning_coercion = 0xB000;

/// Warning: Data set does not match SOP class (non-fatal)
constexpr status_code status_warning_dataset_mismatch = 0xB007;

/// Warning: Elements discarded
constexpr status_code status_warning_elements_discarded = 0xB006;

/// Warning: Sub-operations complete with failures
constexpr status_code status_warning_subops_complete_failures = 0xB000;

/// @}

/// @name Status Type Categories
/// @{

/**
 * @brief Check if status indicates success
 * @param status The status code to check
 * @return true if operation was successful
 */
[[nodiscard]] constexpr bool is_success(status_code status) noexcept {
    return status == status_success;
}

/**
 * @brief Check if status indicates pending (more results)
 * @param status The status code to check
 * @return true if more results are pending
 */
[[nodiscard]] constexpr bool is_pending(status_code status) noexcept {
    return status == status_pending || status == status_pending_warning;
}

/**
 * @brief Check if status indicates cancellation
 * @param status The status code to check
 * @return true if operation was canceled
 */
[[nodiscard]] constexpr bool is_cancel(status_code status) noexcept {
    return status == status_cancel;
}

/**
 * @brief Check if status indicates a warning
 * @param status The status code to check
 * @return true if operation completed with warning
 */
[[nodiscard]] constexpr bool is_warning(status_code status) noexcept {
    return (status & 0xF000) == 0xB000;
}

/**
 * @brief Check if status indicates a failure
 * @param status The status code to check
 * @return true if operation failed
 *
 * Failure status codes start with 0xA or 0xC in the high nibble,
 * or have specific values like 0x01xx.
 */
[[nodiscard]] constexpr bool is_failure(status_code status) noexcept {
    const auto high_nibble = (status & 0xF000) >> 12;
    return high_nibble == 0xA || high_nibble == 0xC ||
           (status >= 0x0100 && status <= 0x01FF);
}

/**
 * @brief Check if this is a final status (operation complete)
 * @param status The status code to check
 * @return true if this indicates the operation is complete
 */
[[nodiscard]] constexpr bool is_final(status_code status) noexcept {
    return !is_pending(status);
}

/// @}

/// @name Status Code String Conversion
/// @{

/**
 * @brief Get a human-readable description of a status code
 * @param status The status code to describe
 * @return Description string
 */
[[nodiscard]] constexpr std::string_view status_description(
    status_code status) noexcept {
    switch (status) {
        case status_success: return "Success";
        case status_pending: return "Pending";
        case status_pending_warning: return "Pending (Warning)";
        case status_cancel: return "Canceled";
        case status_refused_out_of_resources: return "Refused: Out of resources";
        case status_refused_out_of_resources_matches:
            return "Refused: Unable to calculate number of matches";
        case status_refused_out_of_resources_subops:
            return "Refused: Unable to perform sub-operations";
        case status_refused_move_destination_unknown:
            return "Refused: Move destination unknown";
        case status_refused_sop_class_not_supported:
            return "Refused: SOP class not supported";
        case status_error_dataset_mismatch:
            return "Error: Data set does not match SOP class";
        case status_error_cannot_understand: return "Error: Cannot understand";
        case status_error_unable_to_process: return "Error: Unable to process";
        case status_error_duplicate_sop_instance:
            return "Error: Duplicate SOP instance";
        case status_error_missing_attribute: return "Error: Missing attribute";
        case status_error_missing_attribute_value:
            return "Error: Missing attribute value";
        case status_warning_coercion: return "Warning: Coercion of data elements";
        case status_warning_dataset_mismatch:
            return "Warning: Data set does not match SOP class";
        case status_warning_elements_discarded:
            return "Warning: Elements discarded";
        default:
            if (is_success(status)) return "Success";
            if (is_pending(status)) return "Pending";
            if (is_warning(status)) return "Warning";
            if (is_failure(status)) return "Failure";
            return "Unknown status";
    }
}

/**
 * @brief Get the category name for a status code
 * @param status The status code to categorize
 * @return Category name (Success, Pending, Warning, Failure, or Unknown)
 */
[[nodiscard]] constexpr std::string_view status_category(
    status_code status) noexcept {
    if (is_success(status)) return "Success";
    if (is_pending(status)) return "Pending";
    if (is_cancel(status)) return "Cancel";
    if (is_warning(status)) return "Warning";
    if (is_failure(status)) return "Failure";
    return "Unknown";
}

/// @}

}  // namespace pacs::network::dimse

#endif  // PACS_NETWORK_DIMSE_STATUS_CODES_HPP
