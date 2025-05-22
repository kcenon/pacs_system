#include <string>
#include <thread>
#include <chrono>
#include <filesystem>
#include <memory>

#include "common/pacs_common.h"
#include "core/thread/thread_manager.h"
#include "modules/mpps/scp/mpps_scp.h"
#include "core/interfaces/mpps/mpps_interface.h"
#include "thread_system/sources/logger/logger.h"
#include "thread_system/sources/logger/log_types.h"

namespace fs = std::filesystem;
using namespace log_module;

// Callback function to handle N-CREATE requests (create MPPS)
void onMppsCreate(const pacs::core::interfaces::mpps::MPPSItem& mppsItem) {
    write_information("MPPS N-CREATE received:");
    write_information("  Patient ID: " + mppsItem.patientID);
    write_information("  Patient Name: " + mppsItem.patientName);
    write_information("  Study Instance UID: " + mppsItem.studyInstanceUID);
    write_information("  Performed Procedure Step ID: " + mppsItem.performedProcedureStepID);
    std::string statusStr = "  Procedure Status: ";
    switch (mppsItem.procedureStatus) {
        case pacs::core::interfaces::mpps::MPPSStatus::IN_PROGRESS:
            statusStr += "IN PROGRESS";
            break;
        case pacs::core::interfaces::mpps::MPPSStatus::COMPLETED:
            statusStr += "COMPLETED";
            break;
        case pacs::core::interfaces::mpps::MPPSStatus::DISCONTINUED:
            statusStr += "DISCONTINUED";
            break;
        default:
            statusStr += "UNKNOWN";
    }
    write_information(statusStr);
    write_information("  Start DateTime: " + mppsItem.procedureStepStartDateTime);
    
    if (!mppsItem.performedSeriesSequence.empty()) {
        write_information("  Performed Series:");
        for (const auto& series : mppsItem.performedSeriesSequence) {
            write_information("    Series Instance UID: " + series.seriesInstanceUID);
            write_information("    Modality: " + series.modality);
            write_information("    Number of Images: " + std::to_string(series.numberOfInstances));
        }
    }
}

// Callback function to handle N-SET requests (update MPPS)
void onMppsUpdate(const pacs::core::interfaces::mpps::MPPSItem& mppsItem) {
    write_information("MPPS N-SET received:");
    write_information("  Performed Procedure Step ID: " + mppsItem.performedProcedureStepID);
    std::string statusStr = "  Procedure Status: ";
    switch (mppsItem.procedureStatus) {
        case pacs::core::interfaces::mpps::MPPSStatus::IN_PROGRESS:
            statusStr += "IN PROGRESS";
            break;
        case pacs::core::interfaces::mpps::MPPSStatus::COMPLETED:
            statusStr += "COMPLETED";
            break;
        case pacs::core::interfaces::mpps::MPPSStatus::DISCONTINUED:
            statusStr += "DISCONTINUED";
            break;
        default:
            statusStr += "UNKNOWN";
    }
    write_information(statusStr);
    
    if (!mppsItem.endDateTime.empty()) {
        write_information("  End DateTime: " + mppsItem.endDateTime);
    }
    
    if (!mppsItem.performedSeriesSequence.empty()) {
        write_information("  Updated Performed Series:");
        for (const auto& series : mppsItem.performedSeriesSequence) {
            write_information("    Series Instance UID: " + series.seriesInstanceUID);
            write_information("    Modality: " + series.modality);
            write_information("    Number of Images: " + std::to_string(series.numberOfInstances));
        }
    }
}

int main(int argc, char* argv[]) {
    try {
        // Initialize logger
        set_title("MPPS_SCP");
        console_target(log_types::Information | log_types::Error | log_types::Exception);
        auto logResult = start();
        if (!logResult) {
            return 1;
        }
        
        write_information("Starting MPPS SCP Sample...");
        
        // Initialize thread manager
        pacs::core::thread::ThreadManager::getInstance().initialize(4, 2);
        
        // Configure AE Title and port
        pacs::common::ServiceConfig config;
        config.aeTitle = "MPPS_SCP";
        config.localPort = 11115;  // Using a different port than the other services
        
        // Create data directory
        std::string dataDir = "./mpps_data";
        if (!fs::exists(dataDir)) {
            fs::create_directories(dataDir);
        }
        
        // Create and configure MPPS SCP
        auto mppsSCP = std::make_unique<pacs::mpps::scp::MPPSSCP>(config);
        
        // Set callbacks
        mppsSCP->setMppsCreateCallback(onMppsCreate);
        mppsSCP->setMppsUpdateCallback(onMppsUpdate);
        
        // Start MPPS SCP service
        auto result = mppsSCP->start();
        if (!result.isSuccess()) {
            write_error("Failed to start MPPS SCP: " + result.getErrorMessage());
            return 1;
        }
        
        write_information("MPPS SCP started successfully on port " + std::to_string(config.localPort));
        write_information("AE Title: " + config.aeTitle);
        write_information("Press Ctrl+C to stop...");
        
        // Keep the server running
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    catch (const std::exception& ex) {
        write_error("Error: " + std::string(ex.what()));
        return 1;
    }
    
    return 0;
}