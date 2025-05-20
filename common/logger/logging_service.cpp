#include "logging_service.h"
#include "common/config/config_manager.h"
#include <filesystem>
#include <utility>
#include <iostream>
#include <cctype>
#include <algorithm>

namespace pacs {
namespace common {
namespace logger {

LoggingService& LoggingService::getInstance() {
    static LoggingService instance;
    return instance;
}

std::optional<std::string> LoggingService::initialize(
    const std::string& appName,
    std::shared_ptr<config::ConfigManager> configOverride) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        return "Logging service is already initialized";
    }
    
    try {
        appName_ = appName;
        
        // Apply configuration
        auto configResult = applyConfig(configOverride);
        if (configResult.has_value()) {
            return configResult;
        }
        
        initialized_ = true;
        return std::nullopt; // Success
    }
    catch (const std::exception& ex) {
        return std::string("Failed to initialize logging service: ") + ex.what();
    }
}

void LoggingService::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        logger::shutdown();
        initialized_ = false;
    }
}

bool LoggingService::isInitialized() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return initialized_;
}

std::optional<std::string> LoggingService::applyConfig(
    std::shared_ptr<config::ConfigManager> configOverride) {
    
    try {
        // Get the appropriate config manager
        auto& configManager = configOverride ? 
            *configOverride : config::ConfigManager::getInstance();
        
        // Get log directory from configuration
        std::string logDir = configManager.getLogDirectory().string();
        
        // Get log levels from configuration
        std::string consoleLevelStr = configManager.getValue("log.level.console", "INFO");
        std::string fileLevelStr = configManager.getValue("log.level.file", "DEBUG");
        
        LogLevel consoleLevel = getLogLevelFromString(consoleLevelStr);
        LogLevel fileLevel = getLogLevelFromString(fileLevelStr);
        
        // Get max log files and lines from configuration
        int maxLogFiles = std::stoi(configManager.getValue("log.max.files", "10"));
        int maxLogLines = std::stoi(configManager.getValue("log.max.lines", "10000"));
        
        // Initialize or reconfigure logger
        if (!initialized_) {
            return logger::initialize(appName_, logDir, consoleLevel, fileLevel, maxLogFiles, maxLogLines);
        }
        else {
            // Update logger configuration
            logger::setConsoleLogLevel(consoleLevel);
            logger::setFileLogLevel(fileLevel);
            
            logger::logInfo("Logger configuration updated");
            logger::logInfo("Console log level: {}", consoleLevelStr);
            logger::logInfo("File log level: {}", fileLevelStr);
            
            return std::nullopt; // Success
        }
    }
    catch (const std::exception& ex) {
        return std::string("Failed to apply logging configuration: ") + ex.what();
    }
}

LogLevel LoggingService::getLogLevelFromString(const std::string& levelStr) {
    // Convert to uppercase for case-insensitive comparison
    std::string upperLevel = levelStr;
    std::transform(upperLevel.begin(), upperLevel.end(), upperLevel.begin(),
                   [](unsigned char c){ return std::toupper(c); });
    
    if (upperLevel == "NONE") {
        return LogLevel::None;
    }
    else if (upperLevel == "EXCEPTION") {
        return LogLevel::Exception;
    }
    else if (upperLevel == "ERROR") {
        return LogLevel::Error;
    }
    else if (upperLevel == "INFO" || upperLevel == "INFORMATION") {
        return LogLevel::Info;
    }
    else if (upperLevel == "DEBUG") {
        return LogLevel::Debug;
    }
    else if (upperLevel == "TRACE" || upperLevel == "SEQUENCE" || upperLevel == "PARAMETER") {
        return LogLevel::Trace;
    }
    else {
        // Default to Info level if unknown
        std::cerr << "Unknown log level: " << levelStr << ". Defaulting to INFO." << std::endl;
        return LogLevel::Info;
    }
}

std::string LoggingService::getStringFromLogLevel(LogLevel level) {
    switch (level) {
        case LogLevel::None:
            return "NONE";
        case LogLevel::Exception:
            return "EXCEPTION";
        case LogLevel::Error:
            return "ERROR";
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Debug:
            return "DEBUG";
        case LogLevel::Trace:
            return "TRACE";
        default:
            return "UNKNOWN";
    }
}

} // namespace logger
} // namespace common
} // namespace pacs