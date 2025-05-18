#include <string>
#include <thread>
#include <chrono>
#include <filesystem>
#include <memory>
#include <vector>
#include <fstream>

#include "common/pacs_common.h"
#include "core/thread/thread_manager.h"
#include "modules/worklist/scp/worklist_scp.h"
#include "core/interfaces/worklist/worklist_interface.h"
#include "thread_system/sources/logger/logger.h"
#include "thread_system/sources/logger/log_types.h"

namespace fs = std::filesystem;
using namespace log_module;

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
    item1.scheduledStationAETitle = "STORAGE_SCU";
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
    item2.scheduledStationAETitle = "STORAGE_SCU";
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
            
            write_information("Created worklist file: %s", filename.c_str());
        } else {
            write_error("Failed to create worklist file: %s", filename.c_str());
        }
    }
}

int main(int argc, char* argv[]) {
    try {
        // Initialize logger
        set_title("WORKLIST_SCP");
        console_target(log_types::Information | log_types::Error | log_types::Exception);
        auto logResult = start();
        if (!logResult) {
            return 1;
        }
        
        write_information("Starting Worklist SCP Sample...");
        
        // Initialize thread manager
        pacs::core::thread::ThreadManager::getInstance().initialize(4, 2);
        
        // Configure AE Title and port
        pacs::common::ServiceConfig config;
        config.aeTitle = "WORKLIST_SCP";
        config.localPort = 11113;  // Using a different port than Storage SCP
        
        // Create worklist directory
        std::string worklistDir = "./worklist_data";
        
        // Create sample worklist files
        createSampleWorklistFiles(worklistDir);
        
        // Create and configure Worklist SCP
        auto worklistSCP = std::make_unique<pacs::worklist::scp::WorklistSCP>(config, worklistDir);
        
        // Start Worklist SCP service
        auto result = worklistSCP->start();
        if (!result.isSuccess()) {
            write_error("Failed to start Worklist SCP: %s", result.getErrorMessage().c_str());
            stop();
            return 1;
        }
        
        write_information("Worklist SCP started successfully on port %d", config.localPort);
        write_information("AE Title: %s", config.aeTitle.c_str());
        write_information("Worklist Directory: %s", worklistDir.c_str());
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