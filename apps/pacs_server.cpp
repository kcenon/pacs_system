#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <filesystem>

#include "common/pacs_common.h"
// Avoid including both ServiceConfig definitions
#include <string>
#include <map>
#include <memory>
#include <optional>
#include <mutex>
#include <filesystem>

namespace pacs {
namespace common {
namespace config {
class ConfigManager;
}
}
}
#include "common/logger/logger.h"
#include "common/logger/logging_service.h"
#include "common/security/security_manager.h"
#include "common/security/tls_config.h"
#include "common/dicom/codec_manager.h"
#include "core/database/database_manager.h"

#include "modules/mpps/scp/mpps_scp.h"
#include "modules/storage/scp/storage_scp.h"
#include "modules/worklist/scp/worklist_scp.h"
#include "modules/query_retrieve/scp/query_retrieve_scp.h"

void printServerInfo(const pacs::common::ServiceConfig& config, 
                     const pacs::common::security::SecurityManager& securityManager) {
    std::cout << "======================================================" << std::endl;
    std::cout << "                  PACS Server                         " << std::endl;
    std::cout << "======================================================" << std::endl;
    std::cout << "  AE Title:        " << config.aeTitle << std::endl;
    std::cout << "  Port:            " << config.localPort << std::endl;
    std::cout << "  Peer:            " << config.peerAETitle << " @ " << config.peerHost << ":" << config.peerPort << std::endl;
    std::cout << "  Timeout:         " << config.timeout << " seconds" << std::endl;
    
    std::cout << "  Data Directory:  ./data (default)" << std::endl;
    std::cout << "  Log Directory:   ./logs (default)" << std::endl;
    
    std::cout << "  TLS Encryption:  ";
    if (config.enableTLS) {
        std::cout << "Enabled" << std::endl;
        
        const auto& tlsConfig = securityManager.getTLSConfig();
        std::cout << "    Certificate:    " << tlsConfig.getCertificatePath() << std::endl;
        std::cout << "    Private Key:    " << tlsConfig.getPrivateKeyPath() << std::endl;
        
        if (tlsConfig.getCACertificatePath().has_value()) {
            std::cout << "    CA Certificate: " << tlsConfig.getCACertificatePath().value() << std::endl;
        }
        
        std::cout << "    Client Auth:    " << (tlsConfig.useClientAuthentication() ? "Required" : "Not Required") << std::endl;
    } else {
        std::cout << "Disabled" << std::endl;
    }
    
    // Print authentication information
    std::cout << "  Authentication:  ";
    std::cout << "Basic (default)" << std::endl;
    
    std::cout << "======================================================" << std::endl;
}

int main(int argc, char* argv[]) {
    try {
        std::cout << "Initializing PACS Server..." << std::endl;
        
        // Initialize configuration
        std::string configPath;
        if (argc > 1) {
            configPath = argv[1];
        }
        
        // Create a basic ServiceConfig
        pacs::common::ServiceConfig config;
        
        // Create directories
        std::string dataDir = "./data";
        std::string storageDir = dataDir + "/storage";
        std::string worklistDir = dataDir + "/worklist";
        std::string logDir = "./logs";
        std::string dbDir = dataDir + "/db";
        
        std::filesystem::create_directories(dataDir);
        std::filesystem::create_directories(storageDir);
        std::filesystem::create_directories(worklistDir);
        std::filesystem::create_directories(logDir);
        std::filesystem::create_directories(dbDir);
        
        // Initialize logging service
        auto& loggingService = pacs::common::logger::LoggingService::getInstance();
        auto logResult = loggingService.initialize("PACS_SERVER");
        
        if (logResult.has_value()) {
            std::cerr << "Failed to initialize logging service: " << *logResult << std::endl;
            return 1;
        }
        
        // Log server startup
        pacs::common::logger::logInfo("PACS Server starting up");
        
        // Initialize security manager
        pacs::common::logger::logInfo("Initializing security manager");
        auto& securityManager = pacs::common::security::SecurityManager::getInstance();
        auto securityResult = securityManager.initialize();
        
        if (!securityResult.isOk()) {
            pacs::common::logger::logError("Failed to initialize security manager: {}", securityResult.error());
            std::cerr << "Failed to initialize security manager: " << securityResult.error() << std::endl;
            return 1;
        }
        
        pacs::common::logger::logInfo("Security manager initialized successfully");
        
        // Initialize DICOM codec manager
        pacs::common::logger::logInfo("Initializing DICOM codec manager");
        pacs::common::dicom::CodecManager::getInstance().initialize();
        pacs::common::logger::logInfo("DICOM codec manager initialized successfully");
        
        // Print server information
        printServerInfo(config, securityManager);
        
        // Initialize thread manager
        int threadPoolSize = 4;  // Default thread pool size
        int priorityLevels = 2;  // Default priority levels
        
        pacs::common::logger::logInfo("Thread manager settings: {} threads and {} priority levels", 
                                     threadPoolSize, priorityLevels);
        
        // Thread manager initialization commented out - not implemented yet
        // pacs::core::thread::ThreadManager::getInstance().initialize(threadPoolSize, priorityLevels);
        
        // Initialize database
        pacs::common::logger::logInfo("Initializing database at {}", dbDir + "/pacs.db");
        
        auto dbResult = pacs::core::database::DatabaseManager::getInstance()
            .initialize(pacs::core::database::DatabaseType::SQLite, dbDir + "/pacs.db");
        
        if (!dbResult.isOk()) {
            pacs::common::logger::logError("Failed to initialize database: {}", dbResult.error());
            std::cerr << "Failed to initialize database: " << dbResult.error() << std::endl;
            return 1;
        }
        
        pacs::common::logger::logInfo("Database initialized successfully");
        
        // Initialize PACS components
        pacs::common::logger::logInfo("Initializing DICOM services");
        
        auto mppsSCP = std::make_unique<pacs::mpps::scp::MPPSSCP>(config);
        auto storageSCP = std::make_unique<pacs::storage::scp::StorageSCP>(config, storageDir);
        auto worklistSCP = std::make_unique<pacs::worklist::scp::WorklistSCP>(config, worklistDir);
        auto qrSCP = std::make_unique<pacs::query_retrieve::scp::QueryRetrieveSCP>(config, storageDir);
        
        // Start services
        std::cout << "Starting DICOM services..." << std::endl;
        pacs::common::logger::logInfo("Starting DICOM services");
        
        // Function to start service with logging and error handling
        auto startService = [](const std::string& name, std::function<void()> startFunc) {
            try {
                pacs::common::logger::logInfo("Starting {} service", name);
                startFunc();
                std::cout << "  " << name << " service started" << std::endl;
                pacs::common::logger::logInfo("{} service started successfully", name);
            }
            catch (const std::exception& ex) {
                std::cout << "  Failed to start " << name << " service: " << ex.what() << std::endl;
                pacs::common::logger::logError("Failed to start {} service: {}", name, ex.what());
                throw; // Re-throw to handle at main level
            }
        };
        
        // Start each service with error handling
        startService("MPPS SCP", [&]() { mppsSCP->start(); });
        startService("Storage SCP", [&]() { storageSCP->start(); });
        startService("Worklist SCP", [&]() { worklistSCP->start(); });
        startService("Query/Retrieve SCP", [&]() { qrSCP->start(); });
        
        std::cout << "PACS Server started successfully" << std::endl;
        std::cout << "Press Ctrl+C to stop..." << std::endl;
        
        pacs::common::logger::logInfo("PACS Server started successfully - ready to accept connections");
        
        // Register shutdown handler for graceful shutdown
        auto shutdownHandler = [&]() {
            pacs::common::logger::logInfo("PACS Server shutting down");
            
            // Stop DICOM services
            pacs::common::logger::logInfo("Stopping DICOM services");
            qrSCP->stop();
            worklistSCP->stop();
            storageSCP->stop();
            mppsSCP->stop();
            
            // Shutdown database
            pacs::common::logger::logInfo("Shutting down database");
            pacs::core::database::DatabaseManager::getInstance().shutdown();
            
            // Cleanup DICOM codec manager
            pacs::common::logger::logInfo("Cleaning up DICOM codec manager");
            pacs::common::dicom::CodecManager::getInstance().cleanup();
            
            // Shutdown logging at the very end
            pacs::common::logger::logInfo("PACS Server shutdown complete");
            pacs::common::logger::LoggingService::getInstance().shutdown();
        };
        
        // Keep the server running
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    catch (const std::exception& ex) {
        pacs::common::logger::logError("PACS Server initialization failed: {}", ex.what());
        std::cerr << "Error: " << ex.what() << std::endl;
        
        // Attempt to shutdown logging service gracefully
        try {
            pacs::common::logger::LoggingService::getInstance().shutdown();
        }
        catch (...) {
            // Ignore errors during shutdown after an error
        }
        
        return 1;
    }
    
    return 0;
}