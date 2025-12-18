/**
 * @file ilogger.hpp
 * @brief Logger interface for dependency injection
 *
 * This file provides the ILogger interface and implementations (NullLogger,
 * LoggerService) for use in DICOM services. Separated from service_interfaces.hpp
 * to avoid circular dependencies with scp_service.hpp.
 *
 * @see Issue #309 - Full Adoption of ILogger Interface
 */

#pragma once

#include <pacs/integration/logger_adapter.hpp>
#include <pacs/compat/format.hpp>

#include <memory>
#include <string_view>

namespace pacs::di {

// =============================================================================
// Logger Interface
// =============================================================================

/**
 * @brief Abstract logger interface for dependency injection
 *
 * This interface provides a standardized logging API that can be injected
 * into DICOM services. It supports all standard log levels and enables
 * testable code through mock implementations.
 *
 * Thread Safety:
 * - All methods must be thread-safe in concrete implementations
 * - Logging from multiple threads should be properly serialized
 *
 * @see Issue #309 - Full Adoption of ILogger Interface
 */
class ILogger {
public:
    virtual ~ILogger() = default;

    // =========================================================================
    // Log Level Methods
    // =========================================================================

    /**
     * @brief Log a trace-level message
     * @param message The message to log
     */
    virtual void trace(std::string_view message) = 0;

    /**
     * @brief Log a debug-level message
     * @param message The message to log
     */
    virtual void debug(std::string_view message) = 0;

    /**
     * @brief Log an info-level message
     * @param message The message to log
     */
    virtual void info(std::string_view message) = 0;

    /**
     * @brief Log a warning-level message
     * @param message The message to log
     */
    virtual void warn(std::string_view message) = 0;

    /**
     * @brief Log an error-level message
     * @param message The message to log
     */
    virtual void error(std::string_view message) = 0;

    /**
     * @brief Log a fatal-level message
     * @param message The message to log
     */
    virtual void fatal(std::string_view message) = 0;

    // =========================================================================
    // Level Check
    // =========================================================================

    /**
     * @brief Check if a log level is enabled
     * @param level The level to check
     * @return true if messages at this level will be logged
     */
    [[nodiscard]] virtual bool is_enabled(integration::log_level level) const noexcept = 0;

    // =========================================================================
    // Formatted Logging (Convenience Templates)
    // =========================================================================

    /**
     * @brief Log a formatted trace-level message
     */
    template <typename... Args>
    void trace_fmt(pacs::compat::format_string<Args...> fmt, Args&&... args) {
        if (is_enabled(integration::log_level::trace)) {
            trace(pacs::compat::format(fmt, std::forward<Args>(args)...));
        }
    }

    /**
     * @brief Log a formatted debug-level message
     */
    template <typename... Args>
    void debug_fmt(pacs::compat::format_string<Args...> fmt, Args&&... args) {
        if (is_enabled(integration::log_level::debug)) {
            debug(pacs::compat::format(fmt, std::forward<Args>(args)...));
        }
    }

    /**
     * @brief Log a formatted info-level message
     */
    template <typename... Args>
    void info_fmt(pacs::compat::format_string<Args...> fmt, Args&&... args) {
        if (is_enabled(integration::log_level::info)) {
            info(pacs::compat::format(fmt, std::forward<Args>(args)...));
        }
    }

    /**
     * @brief Log a formatted warning-level message
     */
    template <typename... Args>
    void warn_fmt(pacs::compat::format_string<Args...> fmt, Args&&... args) {
        if (is_enabled(integration::log_level::warn)) {
            warn(pacs::compat::format(fmt, std::forward<Args>(args)...));
        }
    }

    /**
     * @brief Log a formatted error-level message
     */
    template <typename... Args>
    void error_fmt(pacs::compat::format_string<Args...> fmt, Args&&... args) {
        if (is_enabled(integration::log_level::error)) {
            error(pacs::compat::format(fmt, std::forward<Args>(args)...));
        }
    }

    /**
     * @brief Log a formatted fatal-level message
     */
    template <typename... Args>
    void fatal_fmt(pacs::compat::format_string<Args...> fmt, Args&&... args) {
        if (is_enabled(integration::log_level::fatal)) {
            fatal(pacs::compat::format(fmt, std::forward<Args>(args)...));
        }
    }

protected:
    ILogger() = default;
    ILogger(const ILogger&) = default;
    ILogger& operator=(const ILogger&) = default;
    ILogger(ILogger&&) = default;
    ILogger& operator=(ILogger&&) = default;
};

// =============================================================================
// Null Logger Implementation
// =============================================================================

/**
 * @brief No-op logger implementation for default injection
 *
 * NullLogger provides a safe default when no logger is configured.
 * All methods are no-ops with minimal overhead.
 *
 * Usage:
 * @code
 * class MyService {
 * public:
 *     explicit MyService(std::shared_ptr<ILogger> logger = nullptr)
 *         : logger_(logger ? std::move(logger) : std::make_shared<NullLogger>())
 *     {}
 * private:
 *     std::shared_ptr<ILogger> logger_;
 * };
 * @endcode
 */
class NullLogger final : public ILogger {
public:
    NullLogger() = default;
    ~NullLogger() override = default;

    void trace(std::string_view /*message*/) override {}
    void debug(std::string_view /*message*/) override {}
    void info(std::string_view /*message*/) override {}
    void warn(std::string_view /*message*/) override {}
    void error(std::string_view /*message*/) override {}
    void fatal(std::string_view /*message*/) override {}

    [[nodiscard]] bool is_enabled(integration::log_level /*level*/) const noexcept override {
        return false;
    }
};

// =============================================================================
// Logger Service Implementation
// =============================================================================

/**
 * @brief Default implementation of ILogger using logger_adapter
 *
 * LoggerService delegates all logging operations to the static
 * logger_adapter methods, providing dependency injection support
 * for the existing logging infrastructure.
 *
 * Thread Safety:
 * - All methods are thread-safe (delegates to thread-safe logger_adapter)
 */
class LoggerService final : public ILogger {
public:
    LoggerService() = default;
    ~LoggerService() override = default;

    void trace(std::string_view message) override {
        integration::logger_adapter::log(integration::log_level::trace, std::string{message});
    }

    void debug(std::string_view message) override {
        integration::logger_adapter::log(integration::log_level::debug, std::string{message});
    }

    void info(std::string_view message) override {
        integration::logger_adapter::log(integration::log_level::info, std::string{message});
    }

    void warn(std::string_view message) override {
        integration::logger_adapter::log(integration::log_level::warn, std::string{message});
    }

    void error(std::string_view message) override {
        integration::logger_adapter::log(integration::log_level::error, std::string{message});
    }

    void fatal(std::string_view message) override {
        integration::logger_adapter::log(integration::log_level::fatal, std::string{message});
    }

    [[nodiscard]] bool is_enabled(integration::log_level level) const noexcept override {
        return integration::logger_adapter::is_level_enabled(level);
    }
};

// =============================================================================
// Global Null Logger Instance
// =============================================================================

/**
 * @brief Get a shared null logger instance
 *
 * Returns a singleton NullLogger instance for use as a default.
 * This avoids repeated allocations when services need a default logger.
 *
 * @return Shared pointer to NullLogger
 */
[[nodiscard]] inline std::shared_ptr<ILogger> null_logger() {
    static auto instance = std::make_shared<NullLogger>();
    return instance;
}

}  // namespace pacs::di
