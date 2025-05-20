#include <iostream>
#include <filesystem>
#include <string>
#include <thread>
#include <chrono>

#include "common/logger/logger.h"
#include "common/logger/logging_service.h"
#include "common/config/config_manager.h"

using namespace pacs::common;

// Function with automatic entry/exit logging
void performOperationWithLogging() {
    PACS_FUNCTION_LOG;
    
    // Simulate some work
    logger::logInfo("Performing an operation...");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    logger::logDebug("Operation step 1 completed");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    logger::logDebug("Operation step 2 completed");
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    logger::logInfo("Operation completed successfully");
}

// Demonstrate manual logging
void demonstrateLogLevels() {
    logger::logInfo("=== Demonstrating Different Log Levels ===");
    
    // Exception level
    try {
        throw std::runtime_error("This is a test exception");
    }
    catch (const std::exception& ex) {
        logger::logException("Caught exception: {}", ex.what());
    }
    
    // Error level
    logger::logError("This is an ERROR level message");
    
    // Info level
    logger::logInfo("This is an INFO level message");
    
    // Debug level
    logger::logDebug("This is a DEBUG level message");
    
    // Trace level
    logger::logTrace("This is a TRACE level message");
}

// Demonstrate log level changes
void demonstrateLogLevelChanges() {
    logger::logInfo("=== Demonstrating Log Level Changes ===");
    
    // Change console log level to DEBUG
    logger::setConsoleLogLevel(logger::LogLevel::Debug);
    logger::logInfo("Console log level set to DEBUG");
    logger::logDebug("This DEBUG message should now appear in the console");
    
    // Change console log level to INFO
    logger::setConsoleLogLevel(logger::LogLevel::Info);
    logger::logInfo("Console log level set back to INFO");
    logger::logDebug("This DEBUG message should NOT appear in the console");
    
    // Try ERROR level
    logger::setConsoleLogLevel(logger::LogLevel::Error);
    logger::logInfo("Console log level set to ERROR");
    logger::logInfo("This INFO message should NOT appear in the console");
    logger::logError("This ERROR message should appear in the console");
    
    // Set back to INFO
    logger::setConsoleLogLevel(logger::LogLevel::Info);
    logger::logInfo("Console log level reset to INFO");
}

int main(int argc, char* argv[]) {
    try {
        std::cout << "PACS Logger Sample" << std::endl;
        std::cout << "=================" << std::endl << std::endl;
        
        // Create directories
        std::filesystem::create_directories("./logs");
        
        // Initialize configuration
        std::cout << "Initializing configuration..." << std::endl;
        auto& configManager = config::ConfigManager::getInstance();
        
        // Set log configuration values
        configManager.setValue("log.level.console", "INFO");
        configManager.setValue("log.level.file", "DEBUG");
        configManager.setValue("log.max.files", "5");
        configManager.setValue("log.max.lines", "1000");
        
        // Initialize logging service
        std::cout << "Initializing logging service..." << std::endl;
        auto& loggingService = logger::LoggingService::getInstance();
        auto result = loggingService.initialize("PACS_LOGGER_SAMPLE");
        
        if (result.has_value()) {
            std::cerr << "Failed to initialize logging service: " << *result << std::endl;
            return 1;
        }
        
        std::cout << "Logging service initialized successfully." << std::endl;
        std::cout << "Log output will appear in console and ./logs directory." << std::endl << std::endl;
        
        // Demonstrate logging
        logger::logInfo("Logger sample application started");
        
        // Demonstrate different log levels
        demonstrateLogLevels();
        
        // Demonstrate function with automatic entry/exit logging
        performOperationWithLogging();
        
        // Demonstrate log level changes
        demonstrateLogLevelChanges();
        
        // Log application shutdown
        logger::logInfo("Logger sample application shutting down");
        
        // Shutdown logging
        std::cout << std::endl << "Shutting down logging service..." << std::endl;
        loggingService.shutdown();
        
        std::cout << "Logging service shut down successfully." << std::endl;
        std::cout << "Check the ./logs directory for log files." << std::endl;
        
        return 0;
    }
    catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
}