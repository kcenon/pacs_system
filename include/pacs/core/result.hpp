/**
 * @file result.hpp
 * @brief Result<T> type aliases and helpers for PACS system
 *
 * This file provides standardized Result<T> types and error handling
 * utilities for the PACS system, integrating with common_system's
 * Result pattern.
 *
 * @see common_system/include/kcenon/common/patterns/result.h
 */

#pragma once

#include <kcenon/common/patterns/result.h>
#include <kcenon/common/error/error_codes.h>

namespace pacs {

/**
 * @brief Result type alias for PACS operations
 * @tparam T The success value type
 */
template <typename T>
using Result = kcenon::common::Result<T>;

/**
 * @brief Result type for void operations
 */
using VoidResult = kcenon::common::VoidResult;

/**
 * @brief Error information type
 */
using error_info = kcenon::common::error_info;

/**
 * @namespace error_codes
 * @brief PACS-specific error codes
 *
 * Error code range: -700 to -799
 * Provides access to both common error codes and PACS-specific codes.
 */
namespace error_codes {
    // Import common error codes
    using namespace kcenon::common::error::codes::common_errors;

    // ========================================================================
    // PACS-specific error codes (-700 to -799)
    // ========================================================================
    constexpr int pacs_base = -700;

    // DICOM file errors (-700 to -719)
    constexpr int file_not_found = pacs_base - 0;
    constexpr int file_read_error = pacs_base - 1;
    constexpr int file_write_error = pacs_base - 2;
    constexpr int invalid_dicom_file = pacs_base - 3;
    constexpr int missing_dicm_prefix = pacs_base - 4;
    constexpr int invalid_meta_info = pacs_base - 5;
    constexpr int missing_transfer_syntax = pacs_base - 6;
    constexpr int unsupported_transfer_syntax = pacs_base - 7;

    // DICOM element errors (-720 to -739)
    constexpr int element_not_found = pacs_base - 20;
    constexpr int value_conversion_error = pacs_base - 21;
    constexpr int invalid_vr = pacs_base - 22;
    constexpr int invalid_tag = pacs_base - 23;
    constexpr int data_size_mismatch = pacs_base - 24;

    // Encoding/Decoding errors (-740 to -759)
    constexpr int decode_error = pacs_base - 40;
    constexpr int encode_error = pacs_base - 41;
    constexpr int compression_error = pacs_base - 42;
    constexpr int decompression_error = pacs_base - 43;
    constexpr int invalid_tag_encoding = pacs_base - 44;
    constexpr int invalid_length_encoding = pacs_base - 45;
    constexpr int insufficient_data = pacs_base - 46;
    constexpr int invalid_sequence = pacs_base - 47;
    constexpr int unknown_vr = pacs_base - 48;
    constexpr int codec_not_supported = pacs_base - 49;

    // Network/Association errors (-760 to -779)
    constexpr int association_rejected = pacs_base - 60;
    constexpr int association_aborted = pacs_base - 61;
    constexpr int dimse_error = pacs_base - 62;
    constexpr int pdu_error = pacs_base - 63;

    // Connection errors (-764 to -769)
    constexpr int connection_failed = pacs_base - 64;
    constexpr int connection_timeout = pacs_base - 65;
    constexpr int send_failed = pacs_base - 66;
    constexpr int receive_failed = pacs_base - 67;
    constexpr int receive_timeout = pacs_base - 68;

    // Association state errors (-770 to -774)
    constexpr int invalid_association_state = pacs_base - 70;
    constexpr int negotiation_failed = pacs_base - 71;
    constexpr int no_acceptable_context = pacs_base - 72;
    constexpr int release_failed = pacs_base - 73;
    constexpr int already_released = pacs_base - 74;

    // PDU errors (-775 to -779)
    constexpr int pdu_encoding_error = pacs_base - 75;
    constexpr int pdu_decoding_error = pacs_base - 76;
    constexpr int incomplete_pdu = pacs_base - 77;
    constexpr int invalid_pdu_type = pacs_base - 78;
    constexpr int malformed_pdu = pacs_base - 79;

    // Storage errors (-780 to -799)
    constexpr int storage_failed = pacs_base - 80;
    constexpr int retrieve_failed = pacs_base - 81;
    constexpr int query_failed = pacs_base - 82;

    // Database errors (-783 to -789)
    constexpr int database_open_error = pacs_base - 83;
    constexpr int database_connection_error = pacs_base - 84;
    constexpr int database_query_error = pacs_base - 85;
    constexpr int database_transaction_error = pacs_base - 86;
    constexpr int database_migration_error = pacs_base - 87;
    constexpr int database_integrity_error = pacs_base - 88;

    // Cloud storage errors (-790 to -795)
    constexpr int bucket_not_found = pacs_base - 90;
    constexpr int object_not_found = pacs_base - 91;
    constexpr int upload_error = pacs_base - 92;
    constexpr int download_error = pacs_base - 93;
    constexpr int cloud_connection_error = pacs_base - 94;

    // HSM errors (-796 to -799)
    constexpr int tier_not_available = pacs_base - 96;
    constexpr int migration_failed = pacs_base - 97;
    constexpr int instance_not_found = pacs_base - 98;

    // ========================================================================
    // Service-specific error codes (-800 to -899)
    // ========================================================================
    constexpr int service_base = -800;

    // C-STORE service errors (-800 to -819)
    constexpr int store_handler_not_set = service_base - 0;
    constexpr int store_missing_sop_class_uid = service_base - 1;
    constexpr int store_missing_sop_instance_uid = service_base - 2;
    constexpr int store_no_accepted_context = service_base - 3;
    constexpr int store_pre_validation_failed = service_base - 4;
    constexpr int store_dataset_required = service_base - 5;
    constexpr int store_unexpected_command = service_base - 6;

    // C-FIND service errors (-820 to -839)
    constexpr int find_handler_not_set = service_base - 20;
    constexpr int find_invalid_query_level = service_base - 21;
    constexpr int find_missing_query_level = service_base - 22;
    constexpr int find_unexpected_command = service_base - 23;
    constexpr int find_query_cancelled = service_base - 24;

    // C-MOVE/C-GET service errors (-840 to -859)
    constexpr int retrieve_handler_not_set = service_base - 40;
    constexpr int retrieve_missing_destination = service_base - 41;
    constexpr int retrieve_unknown_destination = service_base - 42;
    constexpr int retrieve_sub_operation_failed = service_base - 43;
    constexpr int retrieve_unexpected_command = service_base - 44;
    constexpr int retrieve_cancelled = service_base - 45;

    // Verification service errors (-860 to -869)
    constexpr int echo_unexpected_command = service_base - 60;

    // MPPS service errors (-870 to -879)
    constexpr int mpps_handler_not_set = service_base - 70;
    constexpr int mpps_invalid_state = service_base - 71;
    constexpr int mpps_unexpected_command = service_base - 72;

    // Worklist service errors (-880 to -889)
    constexpr int worklist_handler_not_set = service_base - 80;
    constexpr int worklist_unexpected_command = service_base - 81;

    // General service errors (-890 to -899)
    constexpr int association_not_established = service_base - 90;
    constexpr int file_not_found_service = service_base - 91;
    constexpr int not_a_regular_file = service_base - 92;
    constexpr int file_parsing_not_implemented = service_base - 93;
} // namespace error_codes

// Re-export common utility functions
using kcenon::common::ok;
using kcenon::common::make_error;
using kcenon::common::is_ok;
using kcenon::common::is_error;
using kcenon::common::get_value;
using kcenon::common::get_error;
using kcenon::common::try_catch;
using kcenon::common::try_catch_void;

/**
 * @brief Create a PACS error result with module context
 * @tparam T The result value type
 * @param code Error code from pacs::error_codes
 * @param message Error message
 * @param details Optional additional details
 * @return Result<T> containing the error
 */
template <typename T>
inline Result<T> pacs_error(int code, const std::string& message,
                            const std::string& details = "") {
    if (details.empty()) {
        return kcenon::common::make_error<T>(code, message, "pacs");
    }
    return kcenon::common::make_error<T>(code, message, "pacs", details);
}

/**
 * @brief Create a PACS void error result
 * @param code Error code from pacs::error_codes
 * @param message Error message
 * @param details Optional additional details
 * @return VoidResult containing the error
 */
inline VoidResult pacs_void_error(int code, const std::string& message,
                                  const std::string& details = "") {
    if (details.empty()) {
        return VoidResult(error_info{code, message, "pacs"});
    }
    return VoidResult(error_info{code, message, "pacs", details});
}

} // namespace pacs

// Convenience macros for PACS Result pattern usage

/**
 * @brief Return early if expression is an error (PACS version)
 */
#define PACS_RETURN_IF_ERROR(expr) COMMON_RETURN_IF_ERROR(expr)

/**
 * @brief Assign value or return error (PACS version)
 */
#define PACS_ASSIGN_OR_RETURN(decl, expr) COMMON_ASSIGN_OR_RETURN(decl, expr)

/**
 * @brief Return PACS error if condition is true
 */
#define PACS_RETURN_ERROR_IF(condition, code, message) \
    COMMON_RETURN_ERROR_IF(condition, code, message, "pacs")
