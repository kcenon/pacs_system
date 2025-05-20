#include <string>
#include <thread>
#include <chrono>
#include <filesystem>
#include <memory>
#include <vector>
#include <fstream>

#include "common/pacs_common.h"
#include "core/thread/thread_manager.h"
#include "modules/query_retrieve/scp/query_retrieve_scp.h"
#include "core/interfaces/query_retrieve/query_retrieve_interface.h"
#include "thread_system/sources/logger/logger.h"
#include "thread_system/sources/logger/log_types.h"

using namespace log_module;

namespace fs = std::filesystem;

// Function to display query information
void onQueryCallback(const pacs::core::interfaces::query_retrieve::QueryResultItem& item, const DcmDataset* dataset) {
    write_information("Query received:");
    std::string level_str;
    switch (item.level) {
        case pacs::core::interfaces::query_retrieve::QueryRetrieveLevel::PATIENT:
            level_str = "PATIENT";
            break;
        case pacs::core::interfaces::query_retrieve::QueryRetrieveLevel::STUDY:
            level_str = "STUDY";
            break;
        case pacs::core::interfaces::query_retrieve::QueryRetrieveLevel::SERIES:
            level_str = "SERIES";
            break;
        case pacs::core::interfaces::query_retrieve::QueryRetrieveLevel::IMAGE:
            level_str = "IMAGE";
            break;
    }
    write_information("  Level: {}", level_str);
    
    if (!item.patientID.empty()) {
        write_information("  Patient ID: {}", item.patientID);
    }
    if (!item.patientName.empty()) {
        write_information("  Patient Name: {}", item.patientName);
    }
    if (!item.studyInstanceUID.empty()) {
        write_information("  Study Instance UID: {}", item.studyInstanceUID);
    }
    if (!item.studyDescription.empty()) {
        write_information("  Study Description: {}", item.studyDescription);
    }
    if (!item.seriesInstanceUID.empty()) {
        write_information("  Series Instance UID: {}", item.seriesInstanceUID);
    }
    if (!item.seriesDescription.empty()) {
        write_information("  Series Description: {}", item.seriesDescription);
    }
    if (!item.sopInstanceUID.empty()) {
        write_information("  SOP Instance UID: {}", item.sopInstanceUID);
    }
    if (!item.sopClassUID.empty()) {
        write_information("  SOP Class UID: {}", item.sopClassUID);
    }
    write_information("");
}

// Function to display retrieve information
void onRetrieveCallback(const std::string& sopInstanceUID, const DcmDataset* dataset) {
    write_information("Retrieve request received:");
    write_information("  SOP Instance UID: {}", sopInstanceUID);
    write_information("");
}

// Function to display move operation result
void onMoveCallback(const pacs::core::interfaces::query_retrieve::MoveResult& result) {
    write_information("Move operation result:");
    write_information("  Success: {}", result.success ? "Yes" : "No");
    write_information("  Completed Transfers: {}", result.completed);
    write_information("  Remaining Transfers: {}", result.remaining);
    write_information("  Failed Transfers: {}", result.failed);
    write_information("  Warnings: {}", result.warning);
    write_information("  Message: {}", result.message);
    write_information("");
}

int main(int argc, char* argv[]) {
    try {
        write_information("Starting Query/Retrieve SCP Sample...");
        
        // Initialize thread manager
        pacs::core::thread::ThreadManager::getInstance().initialize(4, 2);
        
        // Configure AE Title and port
        pacs::common::ServiceConfig config;
        config.aeTitle = "QR_SCP";
        config.localPort = 11114;  // Using a different port than the other services
        
        // Create storage directory for DICOM data
        std::string storageDir = "./qr_data";
        if (!fs::exists(storageDir)) {
            fs::create_directories(storageDir);
        }
        
        // Create and configure Query/Retrieve SCP
        auto qrSCP = std::make_unique<pacs::query_retrieve::scp::QueryRetrieveSCP>(config, storageDir);
        
        // Set callbacks
        qrSCP->setQueryCallback(onQueryCallback);
        qrSCP->setRetrieveCallback(onRetrieveCallback);
        qrSCP->setMoveCallback(onMoveCallback);
        
        // Start Query/Retrieve SCP service
        auto result = qrSCP->start();
        if (!result.isSuccess()) {
            write_error("Failed to start Query/Retrieve SCP: {}", result.getErrorMessage());
            return 1;
        }
        
        write_information("Query/Retrieve SCP started successfully on port {}", config.localPort);
        write_information("AE Title: {}", config.aeTitle);
        write_information("Storage Directory: {}", storageDir);
        write_information("Press Ctrl+C to stop...");
        
        // Keep the server running
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    catch (const std::exception& ex) {
        write_error("Error: {}", ex.what());
        return 1;
    }
    
    return 0;
}