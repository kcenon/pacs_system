#include <string>
#include <chrono>
#include <thread>
#include <vector>
#include <memory>

#include "common/pacs_common.h"
#include "core/thread/thread_manager.h"
#include "modules/mpps/scu/mpps_scu.h"
#include "core/interfaces/mpps/mpps_interface.h"
#include "thread_system/sources/logger/logger.h"
#include "thread_system/sources/logger/log_types.h"

using namespace log_module;

// Utility function to generate a UID (in a real application, use a proper UID generator)
std::string generateUID(const std::string& prefix) {
    static int counter = 0;
    return prefix + "." + std::to_string(counter++);
}

int main(int argc, char* argv[]) {
    try {
        // Initialize logger
        set_title("MPPS_SCU");
        console_target(log_types::Information | log_types::Error | log_types::Exception);
        auto logResult = start();
        if (!logResult) {
            return 1;
        }
        
        write_information("MPPS SCU Sample");
        
        // Default values
        std::string remotePeerAETitle = "MPPS_SCP";
        std::string remotePeerHost = "localhost";
        unsigned short remotePeerPort = 11115;
        
        // Parse command line arguments
        if (argc >= 2) {
            remotePeerAETitle = argv[1];
        }
        if (argc >= 3) {
            remotePeerHost = argv[2];
        }
        if (argc >= 4) {
            remotePeerPort = std::stoi(argv[3]);
        }
        
        // Initialize thread manager
        pacs::core::thread::ThreadManager::getInstance().initialize(2, 1);
        
        // Configure SCU
        pacs::common::ServiceConfig config;
        config.aeTitle = "MPPS_SCU";
        config.localPort = 0;  // Let system choose a port
        
        // Create MPPS SCU
        auto mppsSCU = std::make_unique<pacs::mpps::scu::MPPSSCU>(config);
        
        // Configure remote peer
        mppsSCU->setRemotePeer(remotePeerAETitle, remotePeerHost, remotePeerPort);
        
        write_information("Connecting to " + remotePeerAETitle + "@" + remotePeerHost + ":" + std::to_string(remotePeerPort));
        
        // Create a sample MPPS item for "IN PROGRESS" status
        pacs::core::interfaces::mpps::MPPSItem mppsItem;
        mppsItem.patientID = "PAT001";
        mppsItem.patientName = "DOE^JOHN";
        mppsItem.studyInstanceUID = generateUID("1.2.840.10008.5.1.4.1.1.4");
        mppsItem.performedProcedureStepID = "MPPS001";
        mppsItem.procedureStatus = pacs::core::interfaces::mpps::MPPSStatus::IN_PROGRESS;
        
        // Set the start date and time
        auto now = std::chrono::system_clock::now();
        auto now_time_t = std::chrono::system_clock::to_time_t(now);
        std::tm now_tm = *std::localtime(&now_time_t);
        
        char dateTimeBuffer[20];
        std::strftime(dateTimeBuffer, sizeof(dateTimeBuffer), "%Y%m%d%H%M%S", &now_tm);
        mppsItem.procedureStepStartDateTime = dateTimeBuffer;
        
        // Add a performed series to the MPPS item
        pacs::core::interfaces::mpps::PerformedSeries series;
        series.seriesInstanceUID = generateUID("1.2.840.10008.5.1.4.1.1.4.1");
        series.modality = "CT";
        series.numberOfInstances = 0;  // No images acquired yet
        mppsItem.performedSeriesSequence.push_back(series);
        
        // Create the MPPS
        write_information("Creating MPPS with status IN PROGRESS...");
        auto createResult = mppsSCU->createMPPS(mppsItem);
        
        if (!createResult.isSuccess()) {
            write_error("Failed to create MPPS: " + createResult.getErrorMessage());
            return 1;
        }
        
        write_information("MPPS created successfully");
        
        // Simulate passage of time during which the procedure is performed
        write_information("Simulating procedure execution...");
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // Update the MPPS item for "COMPLETED" status
        mppsItem.procedureStatus = pacs::core::interfaces::mpps::MPPSStatus::COMPLETED;
        
        // Set the end date and time
        now = std::chrono::system_clock::now();
        now_time_t = std::chrono::system_clock::to_time_t(now);
        now_tm = *std::localtime(&now_time_t);
        std::strftime(dateTimeBuffer, sizeof(dateTimeBuffer), "%Y%m%d%H%M%S", &now_tm);
        mppsItem.endDateTime = dateTimeBuffer;
        
        // Update the series information to indicate completion
        mppsItem.performedSeriesSequence[0].numberOfInstances = 120;  // 120 images acquired
        
        // Update the MPPS
        write_information("Updating MPPS with status COMPLETED...");
        auto updateResult = mppsSCU->updateMPPS(mppsItem);
        
        if (!updateResult.isSuccess()) {
            write_error("Failed to update MPPS: " + updateResult.getErrorMessage());
            return 1;
        }
        
        write_information("MPPS updated successfully");
        write_information("MPPS SCU completed");
    }
    catch (const std::exception& ex) {
        write_error("Error: " + std::string(ex.what()));
        return 1;
    }
    
    return 0;
}