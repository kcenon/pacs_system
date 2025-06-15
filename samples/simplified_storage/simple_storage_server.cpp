#include <iostream>
#include <string>
#include <filesystem>
#include <chrono>
#include <thread>

#include "common/dicom/storage_server.h"
#include "thread_system/sources/logger/logger.h"

namespace fs = std::filesystem;
using namespace log_module;
using namespace pacs::common::dicom;

void handleStorageEvent(const StorageServer::StorageEvent& event) {
    write_information("DICOM object received:");
    write_information("  Patient: %s (%s)", event.patientName.c_str(), event.patientID.c_str());
    write_information("  Study UID: %s", event.studyInstanceUID.c_str());
    write_information("  Series UID: %s", event.seriesInstanceUID.c_str());
    write_information("  SOP Instance UID: %s", event.sopInstanceUID.c_str());
    write_information("  Modality: %s", event.modality.c_str());
    write_information("  Stored at: %s", event.filename.c_str());
    write_information("  Received from: %s", event.callingAETitle.c_str());
    write_information("");
}

int main(int argc, char* argv[]) {
    try {
        // Initialize logger
        set_title("SIMPLE_STORAGE_SERVER");
        console_target(log_types::Information | log_types::Error);
        start();
        
        write_information("Starting Simple Storage Server...");
        
        // Create storage directory if it doesn't exist
        std::string storageDir = "./storage_data";
        if (!fs::exists(storageDir)) {
            fs::create_directories(storageDir);
        }
        
        // Configure and create the storage server
        auto config = StorageServer::Config::createDefault()
            .withAETitle("SIMPLE_STORAGE")
            .withPort(11112)
            .withStorageDirectory(storageDir)
            .withFolderOrganization(true)
            .withAllowAnyAETitle(true);
        
        StorageServer server(config);
        
        // Set callback for storage events
        server.setStorageCallback(handleStorageEvent);
        
        // Start the server
        auto result = server.start();
        if (result.isError()) {
            write_error("Failed to start Storage Server: %s", result.getErrorMessage().c_str());
            stop();
            return 1;
        }
        
        write_information("Storage Server started successfully");
        write_information("AE Title: %s", config.aeTitle.c_str());
        write_information("Port: %d", config.port);
        write_information("Storage Directory: %s", config.storageDirectory.c_str());
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