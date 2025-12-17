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
 * Provides access to both common error codes and PACS-specific codes.
 */
namespace error_codes {
    // Import common error codes
    using namespace kcenon::common::error::codes::common_errors;

    // PACS-specific error codes
    namespace pacs = kcenon::common::error::codes::pacs_system;

    // Convenience aliases for frequently used PACS errors
    using kcenon::common::error::codes::pacs_system::file_not_found;
    using kcenon::common::error::codes::pacs_system::file_read_error;
    using kcenon::common::error::codes::pacs_system::file_write_error;
    using kcenon::common::error::codes::pacs_system::invalid_dicom_file;
    using kcenon::common::error::codes::pacs_system::missing_dicm_prefix;
    using kcenon::common::error::codes::pacs_system::invalid_meta_info;
    using kcenon::common::error::codes::pacs_system::missing_transfer_syntax;
    using kcenon::common::error::codes::pacs_system::unsupported_transfer_syntax;
    using kcenon::common::error::codes::pacs_system::element_not_found;
    using kcenon::common::error::codes::pacs_system::value_conversion_error;
    using kcenon::common::error::codes::pacs_system::invalid_vr;
    using kcenon::common::error::codes::pacs_system::invalid_tag;
    using kcenon::common::error::codes::pacs_system::data_size_mismatch;
    using kcenon::common::error::codes::pacs_system::decode_error;
    using kcenon::common::error::codes::pacs_system::encode_error;
    using kcenon::common::error::codes::pacs_system::compression_error;
    using kcenon::common::error::codes::pacs_system::decompression_error;
    using kcenon::common::error::codes::pacs_system::association_rejected;
    using kcenon::common::error::codes::pacs_system::association_aborted;
    using kcenon::common::error::codes::pacs_system::dimse_error;
    using kcenon::common::error::codes::pacs_system::pdu_error;
    using kcenon::common::error::codes::pacs_system::storage_failed;
    using kcenon::common::error::codes::pacs_system::retrieve_failed;
    using kcenon::common::error::codes::pacs_system::query_failed;
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
