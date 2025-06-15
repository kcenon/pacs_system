#include "logger.h"
#include <filesystem>
#include <iostream>

// Include the full thread_system logger
#include "logger/logger.h"
#include "logger/types/log_types.h"

namespace pacs {
namespace common {
namespace logger {

log_module::log_types convertToLogType(LogLevel level) {
    switch (level) {
        case LogLevel::None:
            return log_module::log_types::None;
        case LogLevel::Exception:
            return log_module::log_types::Exception;
        case LogLevel::Error:
            return log_module::log_types::Error;
        case LogLevel::Info:
            return log_module::log_types::Information;
        case LogLevel::Debug:
            return log_module::log_types::Debug;
        case LogLevel::Trace:
            return log_module::log_types::Sequence;
        default:
            return log_module::log_types::None;
    }
}

LogLevel convertToLogLevel(log_module::log_types type) {
    switch (type) {
        case log_module::log_types::None:
            return LogLevel::None;
        case log_module::log_types::Exception:
            return LogLevel::Exception;
        case log_module::log_types::Error:
            return LogLevel::Error;
        case log_module::log_types::Information:
            return LogLevel::Info;
        case log_module::log_types::Debug:
            return LogLevel::Debug;
        case log_module::log_types::Sequence:
        case log_module::log_types::Parameter:
            return LogLevel::Trace;
        default:
            return LogLevel::None;
    }
}

std::optional<std::string> initialize(
    const std::string& appName,
    const std::string& logDir,
    LogLevel consoleLevel,
    LogLevel fileLevel,
    int maxLogFiles,
    int maxLogLines) {
    
    try {
        // Create log directory if it doesn't exist
        std::filesystem::create_directories(logDir);
        
        // Set the application name in the logger
        log_module::set_title(appName);
        
        // Set max lines per log file
        log_module::set_max_lines(static_cast<uint32_t>(maxLogLines));
        
        // Enable log file backups
        log_module::set_use_backup(maxLogFiles > 1);
        
        // Set log levels for different outputs
        log_module::console_target(convertToLogType(consoleLevel));
        log_module::file_target(convertToLogType(fileLevel));
        
        // Start the logger with a check interval of 100ms
        log_module::set_wake_interval(std::chrono::milliseconds(100));
        
        // Start the logger
        auto result = log_module::start();
        
        // Log initialization message
        log_module::write_information("PACS Logger initialized for application: {}", appName);
        log_module::write_information("Log directory: {}", logDir);
        log_module::write_information("Console log level: {}", log_module::to_string(convertToLogType(consoleLevel)));
        log_module::write_information("File log level: {}", log_module::to_string(convertToLogType(fileLevel)));
        
        return result;
    }
    catch (const std::exception& ex) {
        return std::string("Failed to initialize logger: ") + ex.what();
    }
}

void shutdown() {
    try {
        log_module::write_information("PACS Logger shutting down");
        log_module::stop();
    }
    catch (const std::exception& ex) {
        std::cerr << "Error shutting down logger: " << ex.what() << std::endl;
    }
}

void setConsoleLogLevel(LogLevel level) {
    log_module::console_target(convertToLogType(level));
    log_module::write_information("Console log level changed to: {}", 
                                 log_module::to_string(convertToLogType(level)));
}

void setFileLogLevel(LogLevel level) {
    log_module::file_target(convertToLogType(level));
    log_module::write_information("File log level changed to: {}", 
                                 log_module::to_string(convertToLogType(level)));
}

void setLogCallback(
    const std::function<void(LogLevel level, const std::string& time, const std::string& message)>& callback) {
    
    if (callback) {
        // Set callback for all log types - temporarily using single type
        log_module::callback_target(log_module::log_types::Information);
        
        // Set the callback function
        log_module::message_callback(
            [callback](const log_module::log_types& type, const std::string& time, const std::string& message) {
                callback(convertToLogLevel(type), time, message);
            });
    }
    else {
        // Disable callback if nullptr is passed
        log_module::callback_target(log_module::log_types::None);
    }
}

std::string getLogDirectory() {
    // Default implementation - in a real system we would get this from the logger
    return "./logs";
}

// ScopedLogger implementation
ScopedLogger::ScopedLogger(const std::string& functionName)
    : functionName_(functionName), startTime_(std::chrono::high_resolution_clock::now()) {
    log_module::write_sequence("> Entering function: {}", functionName_);
}

ScopedLogger::~ScopedLogger() {
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime_).count();
    log_module::write_sequence("< Exiting function: {} (duration: {}ms)", functionName_, duration);
}

} // namespace logger
} // namespace common
} // namespace pacs