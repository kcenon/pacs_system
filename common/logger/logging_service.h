#pragma once

#include <string>
#include <optional>
#include <memory>
#include <mutex>

#include <memory>

namespace pacs {
namespace common {
namespace config {
class ConfigManager;
}
}
}
#include "logger.h"

namespace pacs {
namespace common {
namespace logger {

/**
 * @brief Service responsible for initializing and managing logging based on configuration
 * 
 * This class provides a singleton interface for centralized logging management
 * throughout the PACS application. It integrates with the configuration system
 * to automatically configure logging according to the application settings.
 */
class LoggingService {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the LoggingService singleton
     */
    static LoggingService& getInstance();
    
    /**
     * @brief Initialize the logging service
     * @param appName Application name to include in log messages
     * @param configOverride Optional config manager to use instead of the global one
     * @return Error message if initialization failed, std::nullopt otherwise
     */
    std::optional<std::string> initialize(
        const std::string& appName,
        std::shared_ptr<config::ConfigManager> configOverride = nullptr);
    
    /**
     * @brief Shutdown the logging service
     */
    void shutdown();
    
    /**
     * @brief Check if logger is initialized
     * @return True if initialized, false otherwise
     */
    bool isInitialized() const;
    
    /**
     * @brief Apply configuration changes to the logger
     * 
     * This method updates the logger configuration when the application
     * configuration changes.
     * 
     * @param configOverride Optional config manager to use instead of the global one
     * @return Error message if reconfiguration failed, std::nullopt otherwise
     */
    std::optional<std::string> applyConfig(
        std::shared_ptr<config::ConfigManager> configOverride = nullptr);
    
    /**
     * @brief Get log level from string representation
     * @param levelStr String representation of log level
     * @return Corresponding LogLevel enum value
     */
    static LogLevel getLogLevelFromString(const std::string& levelStr);
    
    /**
     * @brief Get string representation of log level
     * @param level LogLevel enum value
     * @return String representation of the log level
     */
    static std::string getStringFromLogLevel(LogLevel level);

private:
    // Private constructor for singleton pattern
    LoggingService() = default;
    
    // Delete copy and move constructors and assignment operators
    LoggingService(const LoggingService&) = delete;
    LoggingService& operator=(const LoggingService&) = delete;
    LoggingService(LoggingService&&) = delete;
    LoggingService& operator=(LoggingService&&) = delete;
    
    std::string appName_;
    bool initialized_ = false;
    mutable std::mutex mutex_;
};

} // namespace logger
} // namespace common
} // namespace pacs