#include <iostream>
#include <string>
#include <thread>
#include <chrono>

#include "common/pacs_common.h"
#include "core/thread/thread_manager.h"

#include "modules/mpps/scp/mpps_scp.h"
#include "modules/storage/scp/storage_scp.h"
#include "modules/worklist/scp/worklist_scp.h"
#include "modules/query_retrieve/scp/query_retrieve_scp.h"

int main(int argc, char* argv[]) {
    try {
        std::cout << "Starting PACS Server..." << std::endl;
        
        // Initialize thread manager
        pacs::core::thread::ThreadManager::getInstance().initialize(4, 2);
        
        // Configure AE Title and ports
        pacs::common::ServiceConfig config;
        config.aeTitle = "PACS";
        config.localPort = 11112;
        
        // Set up the service directories
        std::string dataDir = "./data";
        std::string storageDir = dataDir + "/storage";
        std::string worklistDir = dataDir + "/worklist";
        
        // Initialize PACS components
        auto mppsSCP = std::make_unique<pacs::mpps::scp::MPPSSCP>(config);
        auto storageSCP = std::make_unique<pacs::storage::scp::StorageSCP>(config, storageDir);
        auto worklistSCP = std::make_unique<pacs::worklist::scp::WorklistSCP>(config, worklistDir);
        auto qrSCP = std::make_unique<pacs::query_retrieve::scp::QueryRetrieveSCP>(config, storageDir);
        
        // Start services
        mppsSCP->start();
        storageSCP->start();
        worklistSCP->start();
        qrSCP->start();
        
        std::cout << "PACS Server started successfully" << std::endl;
        std::cout << "Press Ctrl+C to stop..." << std::endl;
        
        // Keep the server running
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
    
    return 0;
}