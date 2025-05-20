#pragma once

#include <string>
#include <string_view>
#include <functional>
#include <optional>
#include <chrono>
#include <memory>
#include <iostream>

#include "thread_system/sources/logger/log_types.h"

namespace pacs {
namespace common {
namespace logger {

/**
 * @brief Log level enumeration for PACS system
 */
enum class LogLevel {
    None,
    Exception,
    Error,
    Info,
    Debug,
    Trace
};

/**
 * @brief Convert PACS log level to thread_system log type
 * @param level PACS log level
 * @return Corresponding thread_system log type
 */
log_module::log_types convertToLogType(LogLevel level);

/**
 * @brief Convert thread_system log type to PACS log level
 * @param type Thread_system log type
 * @return Corresponding PACS log level
 */
LogLevel convertToLogLevel(log_module::log_types type);

/**
 * @brief Initialize the logging system
 * @param appName Application name to include in log messages
 * @param logDir Directory for log files
 * @param consoleLevel Minimum log level for console output
 * @param fileLevel Minimum log level for file output
 * @param maxLogFiles Maximum number of log files to keep
 * @param maxLogLines Maximum number of lines per log file
 * @return Error message if initialization failed, std::nullopt otherwise
 */
std::optional<std::string> initialize(
    const std::string& appName,
    const std::string& logDir,
    LogLevel consoleLevel = LogLevel::Info,
    LogLevel fileLevel = LogLevel::Debug,
    int maxLogFiles = 10,
    int maxLogLines = 10000);

/**
 * @brief Shutdown the logging system
 */
void shutdown();

/**
 * @brief Set the log level for console output
 * @param level Minimum log level for console output
 */
void setConsoleLogLevel(LogLevel level);

/**
 * @brief Set the log level for file output
 * @param level Minimum log level for file output
 */
void setFileLogLevel(LogLevel level);

/**
 * @brief Set the callback for log messages
 * @param callback Function to call for each log message
 */
void setLogCallback(
    const std::function<void(LogLevel level, const std::string& time, const std::string& message)>& callback);

/**
 * @brief Get the current log directory
 * @return Current log directory path
 */
std::string getLogDirectory();

/**
 * @brief Log an exception message
 * @param format Format string
 * @param args Arguments for format string
 */
template<typename... Args>
void logException(const char* format, const Args&... args) {
    // Placeholder for actual implementation
    std::cout << "EXCEPTION: " << format << std::endl;
}

/**
 * @brief Log an error message
 * @param format Format string
 * @param args Arguments for format string
 */
template<typename... Args>
void logError(const char* format, const Args&... args) {
    // Placeholder for actual implementation
    std::cout << "ERROR: " << format << std::endl;
}

/**
 * @brief Log a warning message
 * @param format Format string
 * @param args Arguments for format string
 */
template<typename... Args>
void logWarning(const char* format, const Args&... args) {
    // Placeholder for actual implementation
    std::cout << "WARNING: " << format << std::endl;
}

/**
 * @brief Log an information message
 * @param format Format string
 * @param args Arguments for format string
 */
template<typename... Args>
void logInfo(const char* format, const Args&... args) {
    // Placeholder for actual implementation
    std::cout << "INFO: " << format << std::endl;
}

/**
 * @brief Log a debug message
 * @param format Format string
 * @param args Arguments for format string
 */
template<typename... Args>
void logDebug(const char* format, const Args&... args) {
    // Placeholder for actual implementation
    std::cout << "DEBUG: " << format << std::endl;
}

/**
 * @brief Log a trace message (sequence or parameter)
 * @param format Format string
 * @param args Arguments for format string
 */
template<typename... Args>
void logTrace(const char* format, const Args&... args) {
    // Placeholder for actual implementation
    std::cout << "TRACE: " << format << std::endl;
}

/**
 * @brief RAII class for automatic function entry/exit logging
 */
class ScopedLogger {
public:
    /**
     * @brief Constructor that logs function entry
     * @param functionName Name of the function
     */
    ScopedLogger(const std::string& functionName);
    
    /**
     * @brief Destructor that logs function exit
     */
    ~ScopedLogger();
    
private:
    std::string functionName_;
    std::chrono::time_point<std::chrono::high_resolution_clock> startTime_;
};

} // namespace logger
} // namespace common
} // namespace pacs

// Convenience macro for function entry/exit logging
#define PACS_FUNCTION_LOG pacs::common::logger::ScopedLogger _scoped_logger_(__func__)