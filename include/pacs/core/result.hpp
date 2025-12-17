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

    // Network/Association errors (-760 to -779)
    constexpr int association_rejected = pacs_base - 60;
    constexpr int association_aborted = pacs_base - 61;
    constexpr int dimse_error = pacs_base - 62;
    constexpr int pdu_error = pacs_base - 63;

    // Storage errors (-780 to -799)
    constexpr int storage_failed = pacs_base - 80;
    constexpr int retrieve_failed = pacs_base - 81;
    constexpr int query_failed = pacs_base - 82;
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
