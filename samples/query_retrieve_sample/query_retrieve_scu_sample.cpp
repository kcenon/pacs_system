#include <string>
#include <iomanip>
#include <vector>
#include <chrono>
#include <thread>
#include <memory>
#include <filesystem>

#include "common/pacs_common.h"
#include "core/thread/thread_manager.h"
#include "modules/query_retrieve/scu/query_retrieve_scu.h"
#include "core/interfaces/query_retrieve/query_retrieve_interface.h"
#include "thread_system/sources/logger/logger.h"
#include "thread_system/sources/logger/log_types.h"

using namespace log_module;

namespace fs = std::filesystem;

// Function to display query result items
void displayQueryResultItem(const pacs::core::interfaces::query_retrieve::QueryResultItem& item) {
    write_information("------------------------");
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
    write_information("Level: {}", level_str);
    
    if (!item.patientID.empty()) {
        write_information("Patient ID: {}", item.patientID);
    }
    if (!item.patientName.empty()) {
        write_information("Patient Name: {}", item.patientName);
    }
    if (!item.studyInstanceUID.empty()) {
        write_information("Study Instance UID: {}", item.studyInstanceUID);
    }
    if (!item.studyDescription.empty()) {
        write_information("Study Description: {}", item.studyDescription);
    }
    if (!item.seriesInstanceUID.empty()) {
        write_information("Series Instance UID: {}", item.seriesInstanceUID);
    }
    if (!item.seriesDescription.empty()) {
        write_information("Series Description: {}", item.seriesDescription);
    }
    if (!item.sopInstanceUID.empty()) {
        write_information("SOP Instance UID: {}", item.sopInstanceUID);
    }
    if (!item.sopClassUID.empty()) {
        write_information("SOP Class UID: {}", item.sopClassUID);
    }
    write_information("------------------------");
}

int main(int argc, char* argv[]) {
    try {
        write_information("Query/Retrieve SCU Sample");
        
        // Default values
        std::string remotePeerAETitle = "QR_SCP";
        std::string remotePeerHost = "localhost";
        unsigned short remotePeerPort = 11114;
        std::string patientID = "";
        std::string studyInstanceUID = "";
        std::string retrieveDirectory = "./retrieved_data";
        
        // Initialize thread manager
        pacs::core::thread::ThreadManager::getInstance().initialize(2, 1);
        
        // Configure SCU
        pacs::common::ServiceConfig config;
        config.aeTitle = "QR_SCU";
        config.localPort = 0;  // Let system choose a port
        
        // Create Query/Retrieve SCU
        auto qrSCU = std::make_unique<pacs::query_retrieve::scu::QueryRetrieveSCU>(config);
        
        // Configure remote peer
        qrSCU->setRemotePeer(remotePeerAETitle, remotePeerHost, remotePeerPort);
        
        // Create retrieve directory if it doesn't exist
        if (!fs::exists(retrieveDirectory)) {
            fs::create_directories(retrieveDirectory);
        }
        
        // Set the directory where retrieved files will be stored
        qrSCU->setRetrieveDirectory(retrieveDirectory);
        
        // Set query callback
        std::vector<pacs::core::interfaces::query_retrieve::QueryResultItem> queryResults;
        qrSCU->setQueryCallback([&queryResults](
                const pacs::core::interfaces::query_retrieve::QueryResultItem& item,
                const DcmDataset* dataset) {
            queryResults.push_back(item);
        });
        
        // Set retrieve callback
        qrSCU->setRetrieveCallback([](const std::string& sopInstanceUID, const DcmDataset* dataset) {
            write_information("Retrieved image: {}", sopInstanceUID);
        });
        
        // Set move callback
        qrSCU->setMoveCallback([](const pacs::core::interfaces::query_retrieve::MoveResult& result) {
            write_information("Move operation result:");
            write_information("  Success: {}", result.success ? "Yes" : "No");
            write_information("  Completed: {}", result.completed);
            write_information("  Remaining: {}", result.remaining);
            write_information("  Failed: {}", result.failed);
            write_information("  Warnings: {}", result.warning);
            write_information("  Message: {}", result.message);
        });
        
        write_information("Connecting to {}@{}:{}", remotePeerAETitle, remotePeerHost, remotePeerPort);
        
        // Create a search dataset
        DcmDataset searchDataset;
        // In a real implementation, this would populate the dataset with search criteria
        
        // Perform a query at PATIENT level
        write_information("Performing query at PATIENT level...");
        auto queryResult = qrSCU->query(&searchDataset, 
                                        pacs::core::interfaces::query_retrieve::QueryRetrieveLevel::PATIENT);
        
        if (!queryResult.isSuccess()) {
            write_error("Query failed: {}", queryResult.getErrorMessage());
            return 1;
        }
        
        // Display query results
        if (queryResults.empty()) {
            write_information("No results found.");
        } else {
            write_information("Found {} result(s):", queryResults.size());
            for (const auto& item : queryResults) {
                displayQueryResultItem(item);
            }
            
            // If we found any results, try retrieving the first study
            if (!queryResults.empty() && !queryResults[0].studyInstanceUID.empty()) {
                std::string studyToRetrieve = queryResults[0].studyInstanceUID;
                write_information("Retrieving study: {}", studyToRetrieve);
                
                auto retrieveResult = qrSCU->retrieve(studyToRetrieve);
                
                if (retrieveResult.isSuccess()) {
                    write_information("Retrieve operation initiated successfully");
                    write_information("Retrieved images will be stored in: {}", retrieveDirectory);
                } else {
                    write_error("Retrieve failed: {}", retrieveResult.getErrorMessage());
                }
            }
        }
        
        write_information("Query/Retrieve SCU completed");
    }
    catch (const std::exception& ex) {
        write_error("Error: {}", ex.what());
        return 1;
    }
    
    return 0;
}