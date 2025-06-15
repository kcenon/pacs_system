/**
 * @file simple_client.cpp
 * @brief Simple PACS client example demonstrating basic SDK usage
 *
 * Copyright (c) 2024 PACS System
 * Licensed under BSD License
 */

#include <iostream>
#include <string>
#include <vector>

#include "core/result.h"
#include "core/pacs_server.h"
#include "common/logger/log_module.h"
#include "common/config/config_manager.h"
#include "modules/storage/storage_scp_module.h"

int main(int argc, char* argv[]) {
    // Initialize logger
    Logger::LogLevel level = Logger::LogLevel::Info;
    logger::initialize("simple_pacs_client", level);
    
    logger::logInfo("Starting simple PACS client example");
    
    try {
        // Load configuration
        pacs::ConfigManager configManager;
        if (argc > 1) {
            configManager.loadConfig(argv[1]);
        } else {
            // Use default configuration
            logger::logInfo("Using default configuration");
            configManager.loadDefaultConfig();
        }
        
        // Get server configuration
        auto serverConfigResult = configManager.getServerConfig();
        if (!serverConfigResult.isOk()) {
            logger::logError("Failed to get server configuration: {}", 
                             serverConfigResult.getError());
            return 1;
        }
        
        auto serverConfig = serverConfigResult.getValue();
        logger::logInfo("PACS Server configuration:");
        logger::logInfo("  AE Title: {}", serverConfig.aeTitle);
        logger::logInfo("  Port: {}", serverConfig.port);
        logger::logInfo("  Max Connections: {}", serverConfig.maxConnections);
        
        // Create PACS server instance
        pacs::PACSServer server(serverConfig);
        
        // Add storage module
        auto storageModule = std::make_shared<pacs::StorageSCPModule>();
        server.addModule("storage", storageModule);
        
        // Configure storage
        pacs::StorageConfig storageConfig;
        auto storageConfigResult = configManager.getStorageConfig();
        if (storageConfigResult.isOk()) {
            storageConfig = storageConfigResult.getValue();
            logger::logInfo("Storage configuration:");
            logger::logInfo("  Root path: {}", storageConfig.rootPath);
            logger::logInfo("  Database path: {}", storageConfig.databasePath);
        }
        
        // Initialize server
        auto initResult = server.init();
        if (!initResult.isOk()) {
            logger::logError("Failed to initialize server: {}", 
                             initResult.getError());
            return 1;
        }
        
        logger::logInfo("PACS server initialized successfully");
        
        // Example: Query local database
        logger::logInfo("Querying local database for studies...");
        
        // This would normally use the query/retrieve module
        // For now, just demonstrate the pattern
        
        logger::logInfo("Simple PACS client example completed");
        
        // Shutdown server
        server.shutdown();
        
    } catch (const std::exception& e) {
        logger::logError("Exception caught: {}", e.what());
        return 1;
    }
    
    return 0;
}