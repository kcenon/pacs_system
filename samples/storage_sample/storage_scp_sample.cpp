#include <string>
#include <thread>
#include <chrono>
#include <filesystem>

#include "common/pacs_common.h"
#include "core/thread/thread_manager.h"
#include "modules/storage/scp/storage_scp.h"
#include "thread_system/sources/logger/logger.h"
#include "thread_system/sources/logger/log_types.h"

namespace fs = std::filesystem;
using namespace log_module;

// Callback function for storage notifications
void onStorageCallback(const std::string& sopInstanceUID, const std::string& sopClassUID, 
                      const std::string& filename) {
    write_information("Storage SCP: DICOM object received");
    write_information("  SOP Instance UID: %s", sopInstanceUID.c_str());
    write_information("  SOP Class UID: %s", sopClassUID.c_str());
    write_information("  Stored at: %s", filename.c_str());
    write_information("");
}

int main(int argc, char* argv[]) {
    try {
        // Initialize logger
        set_title("STORAGE_SCP");
        console_target(log_types::Information | log_types::Error | log_types::Exception);
        auto logResult = start();
        if (!logResult) {
            return 1;
        }
        
        write_information("Starting Storage SCP Sample...");
        
        // Initialize thread manager
        pacs::core::thread::ThreadManager::getInstance().initialize(4, 2);
        
        // Configure AE Title and port
        pacs::common::ServiceConfig config;
        config.aeTitle = "STORAGE_SCP";
        config.localPort = 11112;  // Standard DICOM port
        
        // Create storage directory if it doesn't exist
        std::string storageDir = "./storage_data";
        if (!fs::exists(storageDir)) {
            fs::create_directories(storageDir);
        }
        
        // Create and configure Storage SCP
        auto storageSCP = std::make_unique<pacs::storage::scp::StorageSCP>(config, storageDir);
        
        // Set callback for storage notifications
        storageSCP->setStorageCallback([](const pacs::core::interfaces::storage::StorageEvent& event) {
            onStorageCallback(event.sopInstanceUID, event.sopClassUID, event.filename);
        });
        
        // Start Storage SCP service
        auto result = storageSCP->start();
        if (!result.isSuccess()) {
            write_error("Failed to start Storage SCP: %s", result.getErrorMessage().c_str());
            stop();
            return 1;
        }
        
        write_information("Storage SCP started successfully on port %d", config.localPort);
        write_information("AE Title: %s", config.aeTitle.c_str());
        write_information("Storage Directory: %s", storageDir.c_str());
        write_information("Press Ctrl+C to stop...");
        
        // Keep the server running
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    catch (const std::exception& ex) {
        write_error("Error: %s", ex.what());
        stop();
        return 1;
    }
    
    stop();
    return 0;
}