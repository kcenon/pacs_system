/**
 * @file logger_adapter.hpp
 * @brief Adapter for DICOM audit logging using logger_system
 *
 * This file provides the logger_adapter class for integrating logger_system
 * with PACS operations. It supports standard logging, DICOM-specific audit
 * logging for HIPAA compliance, and security event logging.
 *
 * @see IR-4 (logger_system Integration), NFR-3.3 (Audit Logging)
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <pacs/compat/format.hpp>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace pacs::integration {

// ─────────────────────────────────────────────────────
// Forward Declarations
// ─────────────────────────────────────────────────────

/**
 * @enum log_level
 * @brief Log severity levels
 */
enum class log_level {
    trace = 0,
    debug = 1,
    info = 2,
    warn = 3,
    error = 4,
    fatal = 5,
    off = 6
};

/**
 * @enum storage_status
 * @brief Status of DICOM C-STORE operations
 */
enum class storage_status {
    success,
    out_of_resources,
    dataset_error,
    cannot_understand,
    processing_failure,
    duplicate_rejected,
    duplicate_stored,
    unknown_error
};

/**
 * @enum move_status
 * @brief Status of DICOM C-MOVE operations
 */
enum class move_status {
    success,
    partial_success,
    refused_out_of_resources,
    refused_move_destination_unknown,
    identifier_does_not_match,
    unable_to_process,
    cancelled,
    unknown_error
};

// Note: query_level may also be defined in monitoring_adapter.hpp
#ifndef PACS_INTEGRATION_QUERY_LEVEL_DEFINED
#define PACS_INTEGRATION_QUERY_LEVEL_DEFINED
/**
 * @enum query_level
 * @brief DICOM query retrieve level
 */
enum class query_level { patient, study, series, image };
#endif

/**
 * @enum security_event_type
 * @brief Types of security events for audit logging
 */
enum class security_event_type {
    authentication_success,
    authentication_failure,
    access_denied,
    configuration_change,
    data_export,
    association_rejected,
    invalid_request
};

// ─────────────────────────────────────────────────────
// Configuration
// ─────────────────────────────────────────────────────

/**
 * @struct logger_config
 * @brief Configuration options for the logger adapter
 */
struct logger_config {
    /// Directory for log files
    std::filesystem::path log_directory{"logs"};

    /// Minimum log level to output
    log_level min_level{log_level::info};

    /// Enable console output
    bool enable_console{true};

    /// Enable file output
    bool enable_file{true};

    /// Enable separate audit trail file for HIPAA compliance
    bool enable_audit_log{true};

    /// Maximum log file size in megabytes before rotation
    std::size_t max_file_size_mb{100};

    /// Maximum number of rotated log files to keep
    std::size_t max_files{10};

    /// Audit log format: "json" or "syslog"
    std::string audit_log_format{"json"};

    /// Use asynchronous logging for better performance
    bool async_mode{true};

    /// Buffer size for async logging
    std::size_t buffer_size{8192};
};

// ─────────────────────────────────────────────────────
// Logger Adapter Class
// ─────────────────────────────────────────────────────

/**
 * @class logger_adapter
 * @brief Adapter for DICOM audit logging using logger_system
 *
 * This class provides a unified interface for logging in the PACS system,
 * including:
 * - Standard application logging (trace through fatal)
 * - DICOM-specific audit logging for HIPAA/regulatory compliance
 * - Security event logging for access control and intrusion detection
 *
 * The adapter uses logger_system's high-performance async logging
 * (4.34M msg/s) internally while providing a PACS-specific API.
 *
 * Thread Safety: All methods are thread-safe.
 *
 * @example
 * @code
 * // Initialize the logger
 * logger_config config;
 * config.log_directory = "/var/log/pacs";
 * config.enable_audit_log = true;
 * logger_adapter::initialize(config);
 *
 * // Standard logging
 * logger_adapter::info("Server started on port {}", 11112);
 *
 * // DICOM audit logging
 * logger_adapter::log_c_store_received(
 *     "MODALITY1", "12345", "1.2.3.4", "1.2.3.4.5",
 *     storage_status::success);
 *
 * // Shutdown
 * logger_adapter::shutdown();
 * @endcode
 */
class logger_adapter {
public:
    // ─────────────────────────────────────────────────────
    // Initialization
    // ─────────────────────────────────────────────────────

    /**
     * @brief Initialize the logger with configuration
     *
     * Must be called before any logging operations. Sets up console
     * and file writers, configures log levels, and initializes the
     * audit logger if enabled.
     *
     * @param config Configuration options
     */
    static void initialize(const logger_config& config);

    /**
     * @brief Shutdown the logger
     *
     * Flushes all pending messages and releases resources.
     * Should be called before application exit.
     */
    static void shutdown();

    /**
     * @brief Check if the logger is initialized
     * @return true if initialized, false otherwise
     */
    [[nodiscard]] static auto is_initialized() noexcept -> bool;

    // ─────────────────────────────────────────────────────
    // Standard Logging
    // ─────────────────────────────────────────────────────

    /**
     * @brief Log a trace-level message
     * @tparam Args Format argument types
     * @param fmt Format string (std::format syntax)
     * @param args Format arguments
     */
    template <typename... Args>
    static void trace(pacs::compat::format_string<Args...> fmt, Args&&... args) {
        log(log_level::trace, pacs::compat::format(fmt, std::forward<Args>(args)...));
    }

    /**
     * @brief Log a debug-level message
     * @tparam Args Format argument types
     * @param fmt Format string (std::format syntax)
     * @param args Format arguments
     */
    template <typename... Args>
    static void debug(pacs::compat::format_string<Args...> fmt, Args&&... args) {
        log(log_level::debug, pacs::compat::format(fmt, std::forward<Args>(args)...));
    }

    /**
     * @brief Log an info-level message
     * @tparam Args Format argument types
     * @param fmt Format string (std::format syntax)
     * @param args Format arguments
     */
    template <typename... Args>
    static void info(pacs::compat::format_string<Args...> fmt, Args&&... args) {
        log(log_level::info, pacs::compat::format(fmt, std::forward<Args>(args)...));
    }

    /**
     * @brief Log a warning-level message
     * @tparam Args Format argument types
     * @param fmt Format string (std::format syntax)
     * @param args Format arguments
     */
    template <typename... Args>
    static void warn(pacs::compat::format_string<Args...> fmt, Args&&... args) {
        log(log_level::warn, pacs::compat::format(fmt, std::forward<Args>(args)...));
    }

    /**
     * @brief Log an error-level message
     * @tparam Args Format argument types
     * @param fmt Format string (std::format syntax)
     * @param args Format arguments
     */
    template <typename... Args>
    static void error(pacs::compat::format_string<Args...> fmt, Args&&... args) {
        log(log_level::error, pacs::compat::format(fmt, std::forward<Args>(args)...));
    }

    /**
     * @brief Log a fatal-level message
     * @tparam Args Format argument types
     * @param fmt Format string (std::format syntax)
     * @param args Format arguments
     */
    template <typename... Args>
    static void fatal(pacs::compat::format_string<Args...> fmt, Args&&... args) {
        log(log_level::fatal, pacs::compat::format(fmt, std::forward<Args>(args)...));
    }

    /**
     * @brief Log a message at the specified level
     * @param level Log severity level
     * @param message The message to log
     */
    static void log(log_level level, const std::string& message);

    /**
     * @brief Log a message with source location
     * @param level Log severity level
     * @param message The message to log
     * @param file Source file name
     * @param line Line number
     * @param function Function name
     */
    static void log(log_level level,
                    const std::string& message,
                    const std::string& file,
                    int line,
                    const std::string& function);

    /**
     * @brief Check if a log level is enabled
     * @param level The level to check
     * @return true if messages at this level will be logged
     */
    [[nodiscard]] static auto is_level_enabled(log_level level) noexcept -> bool;

    /**
     * @brief Flush all pending log messages
     */
    static void flush();

    // ─────────────────────────────────────────────────────
    // DICOM Audit Logging (for HIPAA/compliance)
    // ─────────────────────────────────────────────────────

    /**
     * @brief Log DICOM association establishment
     *
     * Records when a remote system connects and establishes a
     * DICOM association.
     *
     * @param calling_ae AE title of the remote system
     * @param called_ae AE title of this system
     * @param remote_ip IP address of the remote system
     */
    static void log_association_established(const std::string& calling_ae,
                                            const std::string& called_ae,
                                            const std::string& remote_ip);

    /**
     * @brief Log DICOM association release
     *
     * Records when a DICOM association is gracefully released.
     *
     * @param calling_ae AE title of the remote system
     * @param called_ae AE title of this system
     */
    static void log_association_released(const std::string& calling_ae,
                                         const std::string& called_ae);

    /**
     * @brief Log C-STORE operation
     *
     * Records when a DICOM object is received via C-STORE.
     * This is critical for HIPAA audit trail requirements.
     *
     * @param calling_ae AE title of the sender
     * @param patient_id Patient ID from the received object
     * @param study_uid Study Instance UID
     * @param sop_instance_uid SOP Instance UID of the stored object
     * @param status Status of the storage operation
     */
    static void log_c_store_received(const std::string& calling_ae,
                                     const std::string& patient_id,
                                     const std::string& study_uid,
                                     const std::string& sop_instance_uid,
                                     storage_status status);

    /**
     * @brief Log C-FIND operation
     *
     * Records when a query is executed against the PACS database.
     *
     * @param calling_ae AE title of the querying system
     * @param level Query retrieve level (patient, study, series, image)
     * @param matches_returned Number of matching records returned
     */
    static void log_c_find_executed(const std::string& calling_ae,
                                    query_level level,
                                    std::size_t matches_returned);

    /**
     * @brief Log C-MOVE operation
     *
     * Records when images are transferred to another system.
     *
     * @param calling_ae AE title requesting the move
     * @param destination_ae AE title of the destination
     * @param study_uid Study Instance UID being moved
     * @param instances_moved Number of instances successfully moved
     * @param status Status of the move operation
     */
    static void log_c_move_executed(const std::string& calling_ae,
                                    const std::string& destination_ae,
                                    const std::string& study_uid,
                                    std::size_t instances_moved,
                                    move_status status);

    /**
     * @brief Log a security-related event
     *
     * Records security events for audit and intrusion detection.
     *
     * @param type Type of security event
     * @param description Human-readable description
     * @param user_id Optional user or system identifier
     */
    static void log_security_event(security_event_type type,
                                   const std::string& description,
                                   const std::string& user_id = "");

    // ─────────────────────────────────────────────────────
    // Configuration
    // ─────────────────────────────────────────────────────

    /**
     * @brief Set the minimum log level
     * @param level New minimum log level
     */
    static void set_min_level(log_level level);

    /**
     * @brief Get the current minimum log level
     * @return Current minimum log level
     */
    [[nodiscard]] static auto get_min_level() noexcept -> log_level;

    /**
     * @brief Get the current configuration
     * @return Current logger configuration
     */
    [[nodiscard]] static auto get_config() -> const logger_config&;

private:
    // Private implementation helpers
    static void write_audit_log(const std::string& event_type,
                                const std::string& outcome,
                                const std::map<std::string, std::string>& fields);

    [[nodiscard]] static auto storage_status_to_string(storage_status status) -> std::string;
    [[nodiscard]] static auto move_status_to_string(move_status status) -> std::string;
    [[nodiscard]] static auto query_level_to_string(query_level level) -> std::string;
    [[nodiscard]] static auto security_event_to_string(security_event_type type) -> std::string;
    [[nodiscard]] static auto log_level_to_string(log_level level) -> std::string;

    // Internal state (managed through pimpl or static members)
    class impl;
    static std::unique_ptr<impl> pimpl_;
};

}  // namespace pacs::integration
