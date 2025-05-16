#include <string>
#include <filesystem>
#include <vector>
#include <chrono>
#include <thread>

#include "common/pacs_common.h"
#include "core/thread/thread_manager.h"
#include "modules/storage/scu/storage_scu.h"
#include "thread_system/sources/logger/logger.h"
#include "thread_system/sources/logger/log_types.h"

namespace fs = std::filesystem;
using namespace log_module;

int main(int argc, char* argv[]) {
    try {
        // Initialize logger
        set_title("STORAGE_SCU");
        console_target(log_types::Information | log_types::Error | log_types::Exception);
        auto logResult = start();
        if (!logResult) {
            return 1;
        }
        
        write_information("Storage SCU Sample");
        
        // Default values
        std::string remotePeerAETitle = "STORAGE_SCP";
        std::string remotePeerHost = "localhost";
        unsigned short remotePeerPort = 11112;
        std::string dicomFilePath = "./sample_data";
        
        // Parse command line arguments
        if (argc >= 2) {
            dicomFilePath = argv[1];
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
        config.aeTitle = "STORAGE_SCU";
        config.localPort = 0;  // Let system choose a port
        
        // Create Storage SCU
        auto storageSCU = std::make_unique<pacs::storage::scu::StorageSCU>(config);
        
        // Configure remote peer
        storageSCU->setRemotePeer(remotePeerAETitle, remotePeerHost, remotePeerPort);
        
        // Find all DICOM files in the directory
        std::vector<std::string> dicomFiles;
        
        if (fs::exists(dicomFilePath) && fs::is_directory(dicomFilePath)) {
            // If path is a directory, send all DICOM files in it
            for (const auto& entry : fs::directory_iterator(dicomFilePath)) {
                if (entry.is_regular_file()) {
                    dicomFiles.push_back(entry.path().string());
                }
            }
        } else if (fs::exists(dicomFilePath) && fs::is_regular_file(dicomFilePath)) {
            // If path is a file, just send that one
            dicomFiles.push_back(dicomFilePath);
        } else {
            write_error("Error: Path does not exist: %s", dicomFilePath.c_str());
            stop();
            return 1;
        }
        
        if (dicomFiles.empty()) {
            write_error("Error: No DICOM files found to send.");
            stop();
            return 1;
        }
        
        // Send DICOM files
        write_information("Found %zu file(s) to send.", dicomFiles.size());
        write_information("Sending to %s@%s:%d", remotePeerAETitle.c_str(), remotePeerHost.c_str(), remotePeerPort);
        
        for (const auto& file : dicomFiles) {
            write_information("Sending file: %s", file.c_str());
            auto result = storageSCU->storeDICOMFile(file);
            
            if (result.isSuccess()) {
                write_information("Successfully sent file: %s", file.c_str());
            } else {
                write_error("Failed to send file: %s", file.c_str());
                write_error("Error: %s", result.getErrorMessage().c_str());
            }
            
            // Add a small delay between sends to avoid overwhelming the SCP
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        write_information("Storage SCU completed");
        stop();
    }
    catch (const std::exception& ex) {
        write_error("Error: %s", ex.what());
        stop();
        return 1;
    }
    
    return 0;
}