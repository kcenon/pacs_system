#include <string>
#include <thread>
#include <chrono>
#include <filesystem>
#include <memory>
#include <vector>
#include <fstream>
#include <atomic>
#include <csignal>

#include "common/pacs_common.h"
#include "thread_system/sources/logger/logger.h"
#include "thread_system/sources/logger/log_types.h"
#include "core/thread/thread_manager.h"

// Include all SCP modules
#include "modules/storage/scp/storage_scp.h"
#include "modules/worklist/scp/worklist_scp.h"
#include "modules/query_retrieve/scp/query_retrieve_scp.h"
#include "modules/mpps/scp/mpps_scp.h"

// Interface includes
#include "core/interfaces/storage/storage_interface.h"
#include "core/interfaces/worklist/worklist_interface.h"
#include "core/interfaces/query_retrieve/query_retrieve_interface.h"
#include "core/interfaces/mpps/mpps_interface.h"

namespace fs = std::filesystem;

// Global flag for server shutdown
std::atomic<bool> g_running(true);

// Signal handler for graceful shutdown
void signalHandler(int signal) {
    log_module::write_information("Received signal %d, shutting down...", signal);
    g_running = false;
}

// Function to create sample worklist files for testing
void createSampleWorklistFiles(const std::string& worklistDir) {
    // Create sample worklist items
    std::vector<pacs::core::interfaces::worklist::WorklistItem> sampleItems;
    
    // Sample Patient 1
    pacs::core::interfaces::worklist::WorklistItem item1;
    item1.patientID = "PAT001";
    item1.patientName = "DOE^JOHN";
    item1.patientBirthDate = "19700101";
    item1.patientSex = "M";
    item1.accessionNumber = "ACC001";
    item1.requestedProcedureID = "PROC001";
    item1.scheduledStationAETitle = "PACS_CLIENT";
    item1.scheduledProcedureStepStartDate = "20250520";
    item1.scheduledProcedureStepStartTime = "100000";
    item1.modality = "CT";
    sampleItems.push_back(item1);
    
    // Sample Patient 2
    pacs::core::interfaces::worklist::WorklistItem item2;
    item2.patientID = "PAT002";
    item2.patientName = "SMITH^JANE";
    item2.patientBirthDate = "19800202";
    item2.patientSex = "F";
    item2.accessionNumber = "ACC002";
    item2.requestedProcedureID = "PROC002";
    item2.scheduledStationAETitle = "PACS_CLIENT";
    item2.scheduledProcedureStepStartDate = "20250520";
    item2.scheduledProcedureStepStartTime = "113000";
    item2.modality = "MR";
    sampleItems.push_back(item2);
    
    // Create a directory for worklist files if it doesn't exist
    if (!fs::exists(worklistDir)) {
        fs::create_directories(worklistDir);
    }
    
    // Write worklist items to files (in a real system, this would be in DICOM format)
    for (size_t i = 0; i < sampleItems.size(); ++i) {
        std::string filename = worklistDir + "/worklist_" + std::to_string(i + 1) + ".txt";
        std::ofstream file(filename);
        
        if (file.is_open()) {
            const auto& item = sampleItems[i];
            file << "PatientID: " << item.patientID << "\n";
            file << "PatientName: " << item.patientName << "\n";
            file << "PatientBirthDate: " << item.patientBirthDate << "\n";
            file << "PatientSex: " << item.patientSex << "\n";
            file << "AccessionNumber: " << item.accessionNumber << "\n";
            file << "RequestedProcedureID: " << item.requestedProcedureID << "\n";
            file << "ScheduledStationAETitle: " << item.scheduledStationAETitle << "\n";
            file << "ScheduledProcedureStepStartDate: " << item.scheduledProcedureStepStartDate << "\n";
            file << "ScheduledProcedureStepStartTime: " << item.scheduledProcedureStepStartTime << "\n";
            file << "Modality: " << item.modality << "\n";
            file.close();
            
            log_module::write_information("Created worklist file: %s", filename.c_str());
        } else {
            log_module::write_error("Failed to create worklist file: %s", filename.c_str());
        }
    }
}

// Callback functions for various modules
void onStorageCallback(const pacs::core::interfaces::storage::StorageEvent& event) {
    log_module::write_information("Storage SCP: DICOM object received");
    log_module::write_information("  SOP Instance UID: %s", event.sopInstanceUID.c_str());
    log_module::write_information("  SOP Class UID: %s", event.sopClassUID.c_str());
    log_module::write_information("  Stored at: %s", event.filename.c_str());
}

void onQueryCallback(const pacs::core::interfaces::query_retrieve::QueryResultItem& item, const DcmDataset* dataset) {
    log_module::write_information("Query/Retrieve SCP: Query received");
    
    std::string levelStr;
    switch (item.level) {
        case pacs::core::interfaces::query_retrieve::QueryRetrieveLevel::PATIENT:
            levelStr = "PATIENT";
            break;
        case pacs::core::interfaces::query_retrieve::QueryRetrieveLevel::STUDY:
            levelStr = "STUDY";
            break;
        case pacs::core::interfaces::query_retrieve::QueryRetrieveLevel::SERIES:
            levelStr = "SERIES";
            break;
        case pacs::core::interfaces::query_retrieve::QueryRetrieveLevel::IMAGE:
            levelStr = "IMAGE";
            break;
    }
    log_module::write_information("  Level: %s", levelStr.c_str());
    
    if (!item.patientID.empty()) {
        log_module::write_information("  Patient ID: %s", item.patientID.c_str());
    }
    if (!item.patientName.empty()) {
        log_module::write_information("  Patient Name: %s", item.patientName.c_str());
    }
}

void onRetrieveCallback(const std::string& sopInstanceUID, const DcmDataset* dataset) {
    log_module::write_information("Query/Retrieve SCP: Retrieve request received");
    log_module::write_information("  SOP Instance UID: %s", sopInstanceUID.c_str());
}

void onMoveCallback(const pacs::core::interfaces::query_retrieve::MoveResult& result) {
    log_module::write_information("Query/Retrieve SCP: Move operation completed");
    log_module::write_information("  Success: %s", result.success ? "Yes" : "No");
    log_module::write_information("  Completed: %d", result.completed);
    log_module::write_information("  Failed: %d", result.failed);
}

void onMppsCreate(const pacs::core::interfaces::mpps::MPPSItem& mppsItem) {
    log_module::write_information("MPPS SCP: N-CREATE received");
    log_module::write_information("  Patient ID: %s", mppsItem.patientID.c_str());
    log_module::write_information("  Patient Name: %s", mppsItem.patientName.c_str());
    
    std::string statusStr;
    switch (mppsItem.procedureStatus) {
        case pacs::core::interfaces::mpps::MPPSStatus::IN_PROGRESS:
            statusStr = "IN PROGRESS";
            break;
        case pacs::core::interfaces::mpps::MPPSStatus::COMPLETED:
            statusStr = "COMPLETED";
            break;
        case pacs::core::interfaces::mpps::MPPSStatus::DISCONTINUED:
            statusStr = "DISCONTINUED";
            break;
        default:
            statusStr = "UNKNOWN";
    }
    log_module::write_information("  Procedure Status: %s", statusStr.c_str());
}

void onMppsUpdate(const pacs::core::interfaces::mpps::MPPSItem& mppsItem) {
    log_module::write_information("MPPS SCP: N-SET received");
    
    std::string statusStr;
    switch (mppsItem.procedureStatus) {
        case pacs::core::interfaces::mpps::MPPSStatus::IN_PROGRESS:
            statusStr = "IN PROGRESS";
            break;
        case pacs::core::interfaces::mpps::MPPSStatus::COMPLETED:
            statusStr = "COMPLETED";
            break;
        case pacs::core::interfaces::mpps::MPPSStatus::DISCONTINUED:
            statusStr = "DISCONTINUED";
            break;
        default:
            statusStr = "UNKNOWN";
    }
    log_module::write_information("  Procedure Status: %s", statusStr.c_str());
}

int main(int argc, char* argv[]) {
    try {
        // Initialize logger
        log_module::set_title("PACS_SERVER");
        log_module::console_target(log_module::log_types::Information | 
                                   log_module::log_types::Error | 
                                   log_module::log_types::Exception);
        auto result = log_module::start();
        if (!result) {
            return 1;
        }
        
        // Setup signal handlers for graceful shutdown
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
        
        log_module::write_information("Starting Integrated PACS Server...");
        
        // Initialize thread manager with more threads for integrated server
        pacs::core::thread::ThreadManager::getInstance().initialize(8, 4);
        
        // Configure AE Title and ports for all services
        pacs::common::ServiceConfig storageConfig;
        storageConfig.aeTitle = "PACS_SERVER";
        storageConfig.localPort = 11112;  // Standard DICOM port
        
        pacs::common::ServiceConfig worklistConfig = storageConfig;
        worklistConfig.localPort = 11113;
        
        pacs::common::ServiceConfig qrConfig = storageConfig;
        qrConfig.localPort = 11114;
        
        pacs::common::ServiceConfig mppsConfig = storageConfig;
        mppsConfig.localPort = 11115;
        
        // Create data directories
        std::string baseDir = "./pacs_data";
        std::string storageDir = baseDir + "/storage";
        std::string worklistDir = baseDir + "/worklist";
        
        if (!fs::exists(baseDir)) {
            fs::create_directories(baseDir);
        }
        if (!fs::exists(storageDir)) {
            fs::create_directories(storageDir);
        }
        if (!fs::exists(worklistDir)) {
            fs::create_directories(worklistDir);
        }
        
        // Create sample worklist files
        createSampleWorklistFiles(worklistDir);
        
        // Create and configure all SCP modules
        auto storageSCP = std::make_unique<pacs::storage::scp::StorageSCP>(storageConfig, storageDir);
        auto worklistSCP = std::make_unique<pacs::worklist::scp::WorklistSCP>(worklistConfig, worklistDir);
        auto qrSCP = std::make_unique<pacs::query_retrieve::scp::QueryRetrieveSCP>(qrConfig, storageDir);
        auto mppsSCP = std::make_unique<pacs::mpps::scp::MPPSSCP>(mppsConfig);
        
        // Set callbacks
        storageSCP->setStorageCallback(onStorageCallback);
        
        qrSCP->setQueryCallback(onQueryCallback);
        qrSCP->setRetrieveCallback(onRetrieveCallback);
        qrSCP->setMoveCallback(onMoveCallback);
        
        mppsSCP->setMppsCreateCallback(onMppsCreate);
        mppsSCP->setMppsUpdateCallback(onMppsUpdate);
        
        // Start all services
        log_module::write_information("Starting Storage SCP on port %d...", storageConfig.localPort);
        auto storageResult = storageSCP->start();
        if (!storageResult.isSuccess()) {
            log_module::write_error("Failed to start Storage SCP: %s", storageResult.getErrorMessage().c_str());
            return 1;
        }
        
        log_module::write_information("Starting Worklist SCP on port %d...", worklistConfig.localPort);
        auto worklistResult = worklistSCP->start();
        if (!worklistResult.isSuccess()) {
            log_module::write_error("Failed to start Worklist SCP: %s", worklistResult.getErrorMessage().c_str());
            return 1;
        }
        
        log_module::write_information("Starting Query/Retrieve SCP on port %d...", qrConfig.localPort);
        auto qrResult = qrSCP->start();
        if (!qrResult.isSuccess()) {
            log_module::write_error("Failed to start Query/Retrieve SCP: %s", qrResult.getErrorMessage().c_str());
            return 1;
        }
        
        log_module::write_information("Starting MPPS SCP on port %d...", mppsConfig.localPort);
        auto mppsResult = mppsSCP->start();
        if (!mppsResult.isSuccess()) {
            log_module::write_error("Failed to start MPPS SCP: %s", mppsResult.getErrorMessage().c_str());
            return 1;
        }
        
        log_module::write_information("All PACS services started successfully");
        log_module::write_information("PACS Server AE Title: %s", storageConfig.aeTitle.c_str());
        log_module::write_information("Press Ctrl+C to stop the server");
        
        // Keep the server running until signaled to stop
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        // Graceful shutdown
        log_module::write_information("Shutting down PACS services...");
        
        // Stop all services in reverse order
        mppsSCP->stop();
        qrSCP->stop();
        worklistSCP->stop();
        storageSCP->stop();
        
        log_module::write_information("PACS Server shutdown complete");
        
        // Shutdown logger
        log_module::stop();
    }
    catch (const std::exception& ex) {
        log_module::write_error("Error: %s", ex.what());
        log_module::stop();
        return 1;
    }
    
    return 0;
}