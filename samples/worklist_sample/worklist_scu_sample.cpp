#include <string>
#include <iomanip>
#include <vector>
#include <chrono>
#include <thread>

#include "common/pacs_common.h"
#include "core/thread/thread_manager.h"
#include "modules/worklist/scu/worklist_scu.h"
#include "core/interfaces/worklist/worklist_interface.h"
#include "thread_system/sources/logger/logger.h"
#include "thread_system/sources/logger/log_types.h"

using namespace log_module;

// Function to display worklist item information
void displayWorklistItem(const pacs::core::interfaces::worklist::WorklistItem& item) {
    write_information("------------------------");
    write_information("Patient ID: %s", item.patientID.c_str());
    write_information("Patient Name: %s", item.patientName.c_str());
    write_information("Patient Birth Date: %s", item.patientBirthDate.c_str());
    write_information("Patient Sex: %s", item.patientSex.c_str());
    write_information("Accession Number: %s", item.accessionNumber.c_str());
    write_information("Requested Procedure ID: %s", item.requestedProcedureID.c_str());
    write_information("Scheduled Station AE Title: %s", item.scheduledStationAETitle.c_str());
    write_information("Scheduled Start Date: %s", item.scheduledProcedureStepStartDate.c_str());
    write_information("Scheduled Start Time: %s", item.scheduledProcedureStepStartTime.c_str());
    write_information("Modality: %s", item.modality.c_str());
    write_information("------------------------");
}

int main(int argc, char* argv[]) {
    try {
        // Initialize logger
        set_title("WORKLIST_SCU");
        console_target(log_types::Information | log_types::Error | log_types::Exception);
        auto logResult = start();
        if (!logResult) {
            return 1;
        }
        
        write_information("Worklist SCU Sample");
        
        // Default values
        std::string remotePeerAETitle = "WORKLIST_SCP";
        std::string remotePeerHost = "localhost";
        unsigned short remotePeerPort = 11113;
        std::string patientID = "";  // Empty means search for all patients
        std::string patientName = "";
        std::string modality = "";
        std::string scheduledDate = "";
        
        // Parse command line arguments
        if (argc >= 2) {
            patientID = argv[1];
        }
        if (argc >= 3) {
            remotePeerAETitle = argv[2];
        }
        if (argc >= 4) {
            remotePeerHost = argv[3];
        }
        if (argc >= 5) {
            remotePeerPort = std::stoi(argv[4]);
        }
        
        // Initialize thread manager
        pacs::core::thread::ThreadManager::getInstance().initialize(2, 1);
        
        // Configure SCU
        pacs::common::ServiceConfig config;
        config.aeTitle = "WORKLIST_SCU";
        config.localPort = 0;  // Let system choose a port
        
        // Create Worklist SCU
        auto worklistSCU = std::make_unique<pacs::worklist::scu::WorklistSCU>(config);
        
        // Configure remote peer
        worklistSCU->setRemotePeer(remotePeerAETitle, remotePeerHost, remotePeerPort);
        
        // Set worklist callback
        std::vector<pacs::core::interfaces::worklist::WorklistItem> receivedItems;
        worklistSCU->setWorklistCallback([&receivedItems](const pacs::core::interfaces::worklist::WorklistItem& item) {
            receivedItems.push_back(item);
        });
        
        // Prepare search criteria
        pacs::core::interfaces::worklist::WorklistSearchCriteria criteria;
        criteria.patientID = patientID;
        criteria.patientName = patientName;
        criteria.modality = modality;
        criteria.scheduledProcedureStepStartDate = scheduledDate;
        
        write_information("Querying worklist from %s@%s:%d", 
                      remotePeerAETitle.c_str(), remotePeerHost.c_str(), remotePeerPort);
        
        if (!patientID.empty()) {
            write_information("Filtering by Patient ID: %s", patientID.c_str());
        }
        
        // Query worklist
        auto result = worklistSCU->findWorklist(criteria);
        
        if (!result.isSuccess()) {
            write_error("Failed to query worklist: %s", result.getErrorMessage().c_str());
            stop();
            return 1;
        }
        
        // Display results
        if (receivedItems.empty()) {
            write_information("No worklist items found.");
        } else {
            write_information("Found %zu worklist item(s):", receivedItems.size());
            
            for (const auto& item : receivedItems) {
                displayWorklistItem(item);
            }
        }
        
        write_information("Worklist SCU completed");
    }
    catch (const std::exception& ex) {
        write_error("Error: %s", ex.what());
        stop();
        return 1;
    }
    
    stop();
    return 0;
}