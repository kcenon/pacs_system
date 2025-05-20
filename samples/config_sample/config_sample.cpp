#include <iostream>
#include <filesystem>
#include <string>

#include "common/config/config_manager.h"
#include "common/service_config.h"

using namespace pacs::common;
using namespace pacs::common::config;

int main(int argc, char* argv[]) {
    try {
        std::cout << "PACS Configuration Sample" << std::endl;
        std::cout << "=========================" << std::endl << std::endl;
        
        // Create data directory if it doesn't exist
        std::filesystem::create_directories("./data");
        
        // Create a sample configuration file
        std::string configPath = "./data/pacs_config.json";
        std::cout << "Creating sample configuration file at " << configPath << std::endl;
        
        // Create a simple configuration
        ServiceConfig config;
        config.aeTitle = "PACS_SAMPLE";
        config.localPort = 11113;
        config.maxAssociations = 5;
        config.dataDirectory = "./data/images";
        config.logDirectory = "./logs";
        config.databaseDirectory = "./data/db";
        config.useTLS = false;
        config.serviceSpecificConfig["STORAGE_MAX_PDU_SIZE"] = "16384";
        config.serviceSpecificConfig["WORKLIST_FILE_PATH"] = "./data/worklist.wl";
        
        // Initialize the configuration manager
        auto& configManager = ConfigManager::getInstance();
        configManager.setServiceConfig(config);
        
        // Set some additional configuration values
        configManager.setValue("log.level", "INFO");
        configManager.setValue("log.console", "true");
        configManager.setValue("log.file", "true");
        configManager.setValue("storage.compress", "true");
        
        // Save the configuration to file
        auto result = configManager.saveToFile(configPath);
        if (!result.isSuccess()) {
            std::cerr << "Failed to save configuration: " << result.getMessage() << std::endl;
            return 1;
        }
        
        std::cout << "Configuration saved successfully." << std::endl << std::endl;
        
        // Reset the configuration and load from file
        configManager = ConfigManager::getInstance(); // Get a fresh instance
        
        std::cout << "Loading configuration from file..." << std::endl;
        result = configManager.initialize(configPath);
        
        if (!result.isSuccess()) {
            std::cerr << "Failed to load configuration: " << result.getMessage() << std::endl;
            return 1;
        }
        
        std::cout << "Configuration loaded successfully." << std::endl << std::endl;
        
        // Display loaded configuration
        const auto& loadedConfig = configManager.getServiceConfig();
        
        std::cout << "Service Configuration:" << std::endl;
        std::cout << "  AE Title: " << loadedConfig.aeTitle << std::endl;
        std::cout << "  Local Port: " << loadedConfig.localPort << std::endl;
        std::cout << "  Max Associations: " << loadedConfig.maxAssociations << std::endl;
        
        if (loadedConfig.dataDirectory) {
            std::cout << "  Data Directory: " << *loadedConfig.dataDirectory << std::endl;
        }
        
        if (loadedConfig.logDirectory) {
            std::cout << "  Log Directory: " << *loadedConfig.logDirectory << std::endl;
        }
        
        if (loadedConfig.databaseDirectory) {
            std::cout << "  Database Directory: " << *loadedConfig.databaseDirectory << std::endl;
        }
        
        if (loadedConfig.useTLS) {
            std::cout << "  Use TLS: " << (*loadedConfig.useTLS ? "Yes" : "No") << std::endl;
        }
        
        std::cout << "  Service-Specific Configuration:" << std::endl;
        for (const auto& [key, value] : loadedConfig.serviceSpecificConfig) {
            std::cout << "    " << key << ": " << value << std::endl;
        }
        
        std::cout << std::endl << "General Configuration Values:" << std::endl;
        std::cout << "  log.level: " << configManager.getValue("log.level") << std::endl;
        std::cout << "  log.console: " << configManager.getValue("log.console") << std::endl;
        std::cout << "  log.file: " << configManager.getValue("log.file") << std::endl;
        std::cout << "  storage.compress: " << configManager.getValue("storage.compress") << std::endl;
        
        // Create directories
        std::cout << std::endl << "Creating directories..." << std::endl;
        
        if (loadedConfig.dataDirectory) {
            std::filesystem::create_directories(*loadedConfig.dataDirectory);
            std::cout << "  Created data directory: " << *loadedConfig.dataDirectory << std::endl;
        }
        
        if (loadedConfig.logDirectory) {
            std::filesystem::create_directories(*loadedConfig.logDirectory);
            std::cout << "  Created log directory: " << *loadedConfig.logDirectory << std::endl;
        }
        
        if (loadedConfig.databaseDirectory) {
            std::filesystem::create_directories(*loadedConfig.databaseDirectory);
            std::cout << "  Created database directory: " << *loadedConfig.databaseDirectory << std::endl;
        }
        
        std::cout << std::endl << "Configuration sample completed successfully." << std::endl;
        
        return 0;
    }
    catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
}