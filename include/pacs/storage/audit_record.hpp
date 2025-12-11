/**
 * @file audit_record.hpp
 * @brief Audit log record data structures
 *
 * This file provides the audit_record and audit_query structures for
 * storing and querying audit log entries in the PACS index database.
 *
 * @see NFR-3.3 (Audit Logging), HIPAA compliance requirements
 */

#pragma once

#include <chrono>
#include <optional>
#include <string>

namespace pacs::storage {

/**
 * @brief Audit event type enumeration
 */
enum class audit_event_type {
    association_established,
    association_released,
    c_store,
    c_find,
    c_move,
    c_get,
    security_event,
    configuration_change,
    system_startup,
    system_shutdown,
    user_login,
    user_logout,
    data_access,
    data_export,
    error
};

/**
 * @brief Convert audit_event_type enum to string representation
 */
[[nodiscard]] inline auto to_string(audit_event_type type) -> std::string {
    switch (type) {
        case audit_event_type::association_established:
            return "ASSOCIATION_ESTABLISHED";
        case audit_event_type::association_released:
            return "ASSOCIATION_RELEASED";
        case audit_event_type::c_store:
            return "C_STORE";
        case audit_event_type::c_find:
            return "C_FIND";
        case audit_event_type::c_move:
            return "C_MOVE";
        case audit_event_type::c_get:
            return "C_GET";
        case audit_event_type::security_event:
            return "SECURITY_EVENT";
        case audit_event_type::configuration_change:
            return "CONFIGURATION_CHANGE";
        case audit_event_type::system_startup:
            return "SYSTEM_STARTUP";
        case audit_event_type::system_shutdown:
            return "SYSTEM_SHUTDOWN";
        case audit_event_type::user_login:
            return "USER_LOGIN";
        case audit_event_type::user_logout:
            return "USER_LOGOUT";
        case audit_event_type::data_access:
            return "DATA_ACCESS";
        case audit_event_type::data_export:
            return "DATA_EXPORT";
        case audit_event_type::error:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}

/**
 * @brief Parse string to audit_event_type enum
 */
[[nodiscard]] inline auto parse_audit_event_type(std::string_view str)
    -> std::optional<audit_event_type> {
    if (str == "ASSOCIATION_ESTABLISHED") {
        return audit_event_type::association_established;
    }
    if (str == "ASSOCIATION_RELEASED") {
        return audit_event_type::association_released;
    }
    if (str == "C_STORE") {
        return audit_event_type::c_store;
    }
    if (str == "C_FIND") {
        return audit_event_type::c_find;
    }
    if (str == "C_MOVE") {
        return audit_event_type::c_move;
    }
    if (str == "C_GET") {
        return audit_event_type::c_get;
    }
    if (str == "SECURITY_EVENT") {
        return audit_event_type::security_event;
    }
    if (str == "CONFIGURATION_CHANGE") {
        return audit_event_type::configuration_change;
    }
    if (str == "SYSTEM_STARTUP") {
        return audit_event_type::system_startup;
    }
    if (str == "SYSTEM_SHUTDOWN") {
        return audit_event_type::system_shutdown;
    }
    if (str == "USER_LOGIN") {
        return audit_event_type::user_login;
    }
    if (str == "USER_LOGOUT") {
        return audit_event_type::user_logout;
    }
    if (str == "DATA_ACCESS") {
        return audit_event_type::data_access;
    }
    if (str == "DATA_EXPORT") {
        return audit_event_type::data_export;
    }
    if (str == "ERROR") {
        return audit_event_type::error;
    }
    return std::nullopt;
}

/**
 * @brief Audit log outcome/status
 */
enum class audit_outcome {
    success,
    failure,
    warning
};

/**
 * @brief Convert audit_outcome enum to string
 */
[[nodiscard]] inline auto to_string(audit_outcome outcome) -> std::string {
    switch (outcome) {
        case audit_outcome::success:
            return "SUCCESS";
        case audit_outcome::failure:
            return "FAILURE";
        case audit_outcome::warning:
            return "WARNING";
        default:
            return "UNKNOWN";
    }
}

/**
 * @brief Audit log record from the database
 *
 * Represents a single audit log entry for regulatory compliance
 * and system monitoring.
 */
struct audit_record {
    /// Primary key (auto-generated)
    int64_t pk{0};

    /// Event type
    std::string event_type;

    /// Outcome/status of the event
    std::string outcome;

    /// Timestamp of the event
    std::chrono::system_clock::time_point timestamp;

    /// User ID or AE Title that initiated the action
    std::string user_id;

    /// Source AE Title (for DICOM operations)
    std::string source_ae;

    /// Target/Called AE Title (for DICOM operations)
    std::string target_ae;

    /// Source IP address
    std::string source_ip;

    /// Patient ID (if applicable)
    std::string patient_id;

    /// Study Instance UID (if applicable)
    std::string study_uid;

    /// Human-readable message
    std::string message;

    /// Additional details in JSON format
    std::string details;

    /**
     * @brief Check if this record has valid data
     */
    [[nodiscard]] auto is_valid() const noexcept -> bool {
        return !event_type.empty();
    }
};

/**
 * @brief Query parameters for audit log search
 */
struct audit_query {
    /// Event type filter (exact match)
    std::optional<std::string> event_type;

    /// Outcome filter (exact match)
    std::optional<std::string> outcome;

    /// User ID filter (supports wildcards with '*')
    std::optional<std::string> user_id;

    /// Source AE filter (exact match)
    std::optional<std::string> source_ae;

    /// Patient ID filter (exact match)
    std::optional<std::string> patient_id;

    /// Study UID filter (exact match)
    std::optional<std::string> study_uid;

    /// Date range begin (inclusive, format: YYYY-MM-DD or YYYYMMDD)
    std::optional<std::string> date_from;

    /// Date range end (inclusive, format: YYYY-MM-DD or YYYYMMDD)
    std::optional<std::string> date_to;

    /// Maximum number of results to return (0 = unlimited)
    size_t limit{0};

    /// Offset for pagination
    size_t offset{0};

    /**
     * @brief Check if any filter criteria is set
     */
    [[nodiscard]] auto has_criteria() const noexcept -> bool {
        return event_type.has_value() || outcome.has_value() ||
               user_id.has_value() || source_ae.has_value() ||
               patient_id.has_value() || study_uid.has_value() ||
               date_from.has_value() || date_to.has_value();
    }
};

}  // namespace pacs::storage
