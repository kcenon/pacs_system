#include <string>
#include <chrono>
#include <thread>
#include <filesystem>
#include <memory>
#include <vector>
#include <map>
#include <iomanip>
#include <ctime>
#include <iostream>  // 필요한 부분에만 사용

#include "common/pacs_common.h"
#include "core/thread/thread_manager.h"
#include "thread_system/sources/logger/logger.h"
#include "thread_system/sources/logger/log_types.h"

// Include all SCU modules
#include "modules/storage/scu/storage_scu.h"
#include "modules/worklist/scu/worklist_scu.h"
#include "modules/query_retrieve/scu/query_retrieve_scu.h"
#include "modules/mpps/scu/mpps_scu.h"

// Interface includes
#include "core/interfaces/storage/storage_interface.h"
#include "core/interfaces/worklist/worklist_interface.h"
#include "core/interfaces/query_retrieve/query_retrieve_interface.h"
#include "core/interfaces/mpps/mpps_interface.h"

namespace fs = std::filesystem;

// Utility function to generate a UID (in a real application, use a proper UID generator)
std::string generateUID(const std::string& prefix) {
    static int counter = 0;
    return prefix + "." + std::to_string(counter++);
}

// Function to get current date-time string in DICOM format
std::string getCurrentDateTime() {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm now_tm = *std::localtime(&now_time_t);
    
    char dateTimeBuffer[20];
    std::strftime(dateTimeBuffer, sizeof(dateTimeBuffer), "%Y%m%d%H%M%S", &now_tm);
    return dateTimeBuffer;
}

// Function to display a menu and get user choice
int showMenu() {
    int choice = 0;
    
    log_module::write_information("\n=== PACS Client Menu ===");
    log_module::write_information("1. Query Worklist");
    log_module::write_information("2. Create MPPS (In Progress)");
    log_module::write_information("3. Update MPPS (Completed)");
    log_module::write_information("4. Store DICOM Files");
    log_module::write_information("5. Query Patient/Study");
    log_module::write_information("6. Retrieve Images");
    log_module::write_information("7. Run Complete Workflow");
    log_module::write_information("8. Exit");
    log_module::write_information("Enter your choice: ");
    std::cin >> choice;
    
    return choice;
}

// Function to display worklist items
void displayWorklistItems(const std::vector<pacs::core::interfaces::worklist::WorklistItem>& items) {
    if (items.empty()) {
        log_module::write_information("No worklist items found.");
        return;
    }
    
    log_module::write_information("Found %zu worklist item(s):", items.size());
    for (size_t i = 0; i < items.size(); ++i) {
        const auto& item = items[i];
        log_module::write_information("[%zu] ------------------------", (i + 1));
        log_module::write_information("Patient ID: %s", item.patientID.c_str());
        log_module::write_information("Patient Name: %s", item.patientName.c_str());
        log_module::write_information("Accession Number: %s", item.accessionNumber.c_str());
        log_module::write_information("Requested Procedure ID: %s", item.requestedProcedureID.c_str());
        log_module::write_information("Modality: %s", item.modality.c_str());
        log_module::write_information("Scheduled Start: %s %s", 
                                    item.scheduledProcedureStepStartDate.c_str(),
                                    item.scheduledProcedureStepStartTime.c_str());
    }
}

// Function to query worklist
std::vector<pacs::core::interfaces::worklist::WorklistItem> queryWorklist(
    std::unique_ptr<pacs::worklist::scu::WorklistSCU>& worklistSCU) {
    
    std::vector<pacs::core::interfaces::worklist::WorklistItem> worklistItems;
    
    // Setup callback to receive worklist items
    worklistSCU->setWorklistCallback([&worklistItems](const pacs::core::interfaces::worklist::WorklistItem& item) {
        worklistItems.push_back(item);
    });
    
    // Create search criteria
    pacs::core::interfaces::worklist::WorklistSearchCriteria criteria;
    // Empty criteria to get all worklist items
    
    log_module::write_information("Querying worklist...");
    auto result = worklistSCU->findWorklist(criteria);
    
    if (!result.isSuccess()) {
        log_module::write_error("Failed to query worklist: %s", result.getErrorMessage().c_str());
    } else {
        displayWorklistItems(worklistItems);
    }
    
    return worklistItems;
}

// Function to create MPPS
bool createMPPS(std::unique_ptr<pacs::mpps::scu::MPPSSCU>& mppsSCU,
                const pacs::core::interfaces::worklist::WorklistItem& worklistItem) {
    
    // Create MPPS item from worklist item
    pacs::core::interfaces::mpps::MPPSItem mppsItem;
    mppsItem.patientID = worklistItem.patientID;
    mppsItem.patientName = worklistItem.patientName;
    mppsItem.studyInstanceUID = generateUID("1.2.840.10008.5.1.4.1.1.4");
    mppsItem.performedProcedureStepID = worklistItem.requestedProcedureID;
    mppsItem.procedureStatus = pacs::core::interfaces::mpps::MPPSStatus::IN_PROGRESS;
    mppsItem.procedureStepStartDateTime = getCurrentDateTime();
    
    // Add performed series
    pacs::core::interfaces::mpps::PerformedSeries series;
    series.seriesInstanceUID = generateUID("1.2.840.10008.5.1.4.1.1.4.1");
    series.modality = worklistItem.modality;
    series.numberOfInstances = 0;  // No images acquired yet
    mppsItem.performedSeriesSequence.push_back(series);
    
    log_module::write_information("Creating MPPS for Patient ID: %s", mppsItem.patientID.c_str());
    log_module::write_information("Procedure Status: IN PROGRESS");
    
    auto result = mppsSCU->createMPPS(mppsItem);
    
    if (!result.isSuccess()) {
        log_module::write_error("Failed to create MPPS: %s", result.getErrorMessage().c_str());
        return false;
    }
    
    log_module::write_information("MPPS successfully created");
    return true;
}

// Function to update MPPS
bool updateMPPS(std::unique_ptr<pacs::mpps::scu::MPPSSCU>& mppsSCU,
                pacs::core::interfaces::mpps::MPPSItem& mppsItem) {
    
    // Update MPPS status to completed
    mppsItem.procedureStatus = pacs::core::interfaces::mpps::MPPSStatus::COMPLETED;
    mppsItem.endDateTime = getCurrentDateTime();
    
    // Update series information (assuming images have been acquired)
    for (auto& series : mppsItem.performedSeriesSequence) {
        series.numberOfInstances = 20;  // Assume 20 images were acquired
    }
    
    log_module::write_information("Updating MPPS for Patient ID: %s", mppsItem.patientID.c_str());
    log_module::write_information("Procedure Status: COMPLETED");
    
    auto result = mppsSCU->updateMPPS(mppsItem);
    
    if (!result.isSuccess()) {
        log_module::write_error("Failed to update MPPS: %s", result.getErrorMessage().c_str());
        return false;
    }
    
    log_module::write_information("MPPS successfully updated");
    return true;
}

// Function to store DICOM files
bool storeDICOMFiles(std::unique_ptr<pacs::storage::scu::StorageSCU>& storageSCU,
                     const std::string& dicomDir) {
    
    // Check if directory exists
    if (!fs::exists(dicomDir)) {
        log_module::write_error("DICOM directory does not exist: %s", dicomDir.c_str());
        return false;
    }
    
    // Find all files in the directory
    std::vector<std::string> dicomFiles;
    for (const auto& entry : fs::directory_iterator(dicomDir)) {
        if (entry.is_regular_file()) {
            dicomFiles.push_back(entry.path().string());
        }
    }
    
    if (dicomFiles.empty()) {
        log_module::write_error("No DICOM files found in %s", dicomDir.c_str());
        return false;
    }
    
    log_module::write_information("Found %zu file(s) to send", dicomFiles.size());
    
    // Send each file
    int successCount = 0;
    for (const auto& file : dicomFiles) {
        log_module::write_information("Sending file: %s", file.c_str());
        auto result = storageSCU->storeDICOMFile(file);
        
        if (result.isSuccess()) {
            log_module::write_information("File successfully sent");
            successCount++;
        } else {
            log_module::write_error("Failed to send file: %s", result.getErrorMessage().c_str());
        }
        
        // Add a small delay between sends
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    log_module::write_information("Storage complete: %d of %zu files sent successfully", 
                                successCount, dicomFiles.size());
    return successCount > 0;
}

// Function to query patient/study
std::vector<pacs::core::interfaces::query_retrieve::QueryResultItem> queryPatientStudy(
    std::unique_ptr<pacs::query_retrieve::scu::QueryRetrieveSCU>& qrSCU,
    const std::string& patientID) {
    
    std::vector<pacs::core::interfaces::query_retrieve::QueryResultItem> queryResults;
    
    // Set query callback
    qrSCU->setQueryCallback([&queryResults](
            const pacs::core::interfaces::query_retrieve::QueryResultItem& item,
            const DcmDataset* dataset) {
        queryResults.push_back(item);
    });
    
    // Create a search dataset
    DcmDataset searchDataset;
    // In a real implementation, this would populate the dataset with search criteria
    // including the patientID
    
    // Perform a query at PATIENT level
    log_module::write_information("Performing query for Patient ID: %s", 
                                (!patientID.empty() ? patientID.c_str() : "ALL"));
    auto result = qrSCU->query(&searchDataset, 
                              pacs::core::interfaces::query_retrieve::QueryRetrieveLevel::PATIENT);
    
    if (!result.isSuccess()) {
        log_module::write_error("Query failed: %s", result.getErrorMessage().c_str());
        return queryResults;
    }
    
    // Display results
    if (queryResults.empty()) {
        log_module::write_information("No results found.");
    } else {
        log_module::write_information("Found %zu result(s):", queryResults.size());
        for (size_t i = 0; i < queryResults.size(); ++i) {
            const auto& item = queryResults[i];
            log_module::write_information("[%zu] ------------------------", (i + 1));
            log_module::write_information("Patient ID: %s", item.patientID.c_str());
            log_module::write_information("Patient Name: %s", item.patientName.c_str());
            if (!item.studyInstanceUID.empty()) {
                log_module::write_information("Study UID: %s", item.studyInstanceUID.c_str());
                log_module::write_information("Study Description: %s", item.studyDescription.c_str());
            }
        }
    }
    
    return queryResults;
}

// Function to retrieve images
bool retrieveImages(std::unique_ptr<pacs::query_retrieve::scu::QueryRetrieveSCU>& qrSCU,
                   const std::string& studyInstanceUID) {
    
    if (studyInstanceUID.empty()) {
        log_module::write_error("No Study Instance UID provided for retrieval");
        return false;
    }
    
    // Set callbacks
    int imageCount = 0;
    qrSCU->setRetrieveCallback([&imageCount](const std::string& sopInstanceUID, const DcmDataset* dataset) {
        log_module::write_information("Retrieved image: %s", sopInstanceUID.c_str());
        imageCount++;
    });
    
    qrSCU->setMoveCallback([](const pacs::core::interfaces::query_retrieve::MoveResult& result) {
        log_module::write_information("Retrieve operation complete");
        log_module::write_information("  Success: %s", result.success ? "Yes" : "No");
        log_module::write_information("  Completed: %d", result.completed);
        log_module::write_information("  Failed: %d", result.failed);
    });
    
    log_module::write_information("Retrieving study: %s", studyInstanceUID.c_str());
    auto result = qrSCU->retrieve(studyInstanceUID);
    
    if (!result.isSuccess()) {
        log_module::write_error("Retrieve failed: %s", result.getErrorMessage().c_str());
        return false;
    }
    
    // Give some time for the retrieve operation to complete
    log_module::write_information("Waiting for retrieve operation to complete...");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    log_module::write_information("Retrieved %d images", imageCount);
    return true;
}

// Function to run a complete workflow
void runCompleteWorkflow(std::unique_ptr<pacs::worklist::scu::WorklistSCU>& worklistSCU,
                        std::unique_ptr<pacs::mpps::scu::MPPSSCU>& mppsSCU,
                        std::unique_ptr<pacs::storage::scu::StorageSCU>& storageSCU,
                        std::unique_ptr<pacs::query_retrieve::scu::QueryRetrieveSCU>& qrSCU) {
    
    log_module::write_information("\n=== Starting Complete PACS Workflow ===");
    
    // Step 1: Query worklist
    log_module::write_information("\n--- Step 1: Query Worklist ---");
    auto worklistItems = queryWorklist(worklistSCU);
    
    if (worklistItems.empty()) {
        log_module::write_error("No worklist items found. Cannot continue workflow.");
        return;
    }
    
    // Use the first worklist item
    auto& selectedItem = worklistItems[0];
    log_module::write_information("Selected worklist item for Patient: %s (%s)", 
                                selectedItem.patientName.c_str(), 
                                selectedItem.patientID.c_str());
    
    // Step 2: Create MPPS (procedure started)
    log_module::write_information("\n--- Step 2: Create MPPS (In Progress) ---");
    pacs::core::interfaces::mpps::MPPSItem mppsItem;
    mppsItem.patientID = selectedItem.patientID;
    mppsItem.patientName = selectedItem.patientName;
    mppsItem.studyInstanceUID = generateUID("1.2.840.10008.5.1.4.1.1.4");
    mppsItem.performedProcedureStepID = selectedItem.requestedProcedureID;
    mppsItem.procedureStatus = pacs::core::interfaces::mpps::MPPSStatus::IN_PROGRESS;
    mppsItem.procedureStepStartDateTime = getCurrentDateTime();
    
    // Add performed series
    pacs::core::interfaces::mpps::PerformedSeries series;
    series.seriesInstanceUID = generateUID("1.2.840.10008.5.1.4.1.1.4.1");
    series.modality = selectedItem.modality;
    series.numberOfInstances = 0;  // No images acquired yet
    mppsItem.performedSeriesSequence.push_back(series);
    
    auto mppsResult = mppsSCU->createMPPS(mppsItem);
    if (!mppsResult.isSuccess()) {
        log_module::write_error("Failed to create MPPS: %s", mppsResult.getErrorMessage().c_str());
        return;
    }
    
    log_module::write_information("MPPS created successfully. Procedure is in progress.");
    
    // Simulate image acquisition
    log_module::write_information("\n--- Step 3: Simulating Image Acquisition ---");
    log_module::write_information("Acquiring images... (simulating delay)");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Step 4: Store DICOM files
    log_module::write_information("\n--- Step 4: Store DICOM Images ---");
    std::string dicomDir = "./sample_dicom";  // Directory with sample DICOM files
    bool storeSuccess = false;
    
    if (fs::exists(dicomDir)) {
        storeSuccess = storeDICOMFiles(storageSCU, dicomDir);
    } else {
        log_module::write_information("Sample DICOM directory not found. Simulating image storage.");
        storeSuccess = true;  // Simulate success
    }
    
    if (!storeSuccess) {
        log_module::write_error("Failed to store DICOM files. Continuing workflow anyway.");
    }
    
    // Step 5: Update MPPS (procedure completed)
    log_module::write_information("\n--- Step 5: Update MPPS (Completed) ---");
    mppsItem.procedureStatus = pacs::core::interfaces::mpps::MPPSStatus::COMPLETED;
    mppsItem.endDateTime = getCurrentDateTime();
    
    // Update series information (assuming images have been acquired)
    for (auto& s : mppsItem.performedSeriesSequence) {
        s.numberOfInstances = 20;  // Assume 20 images were acquired
    }
    
    auto updateResult = mppsSCU->updateMPPS(mppsItem);
    if (!updateResult.isSuccess()) {
        log_module::write_error("Failed to update MPPS: %s", updateResult.getErrorMessage().c_str());
        return;
    }
    
    log_module::write_information("MPPS updated successfully. Procedure is completed.");
    
    // Step 6: Query Patient/Study
    log_module::write_information("\n--- Step 6: Query Patient/Study ---");
    auto queryResults = queryPatientStudy(qrSCU, selectedItem.patientID);
    
    // Step 7: Retrieve Images (not actually performed in this simulation)
    log_module::write_information("\n--- Step 7: Retrieve Images ---");
    log_module::write_information("In a real system, this would retrieve the images for the study.");
    
    log_module::write_information("\n=== Complete PACS Workflow Finished ===");
}

int main(int argc, char* argv[]) {
    try {
        // Initialize logger
        log_module::set_title("PACS_CLIENT");
        log_module::console_target(log_module::log_types::Information | 
                                   log_module::log_types::Error | 
                                   log_module::log_types::Exception);
        auto logResult = log_module::start();
        if (!logResult) {
            return 1;
        }
        
        log_module::write_information("Integrated PACS Client");
        
        // Default connection values
        std::string serverAETitle = "PACS_SERVER";
        std::string serverHost = "localhost";
        unsigned short storagePort = 11112;
        unsigned short worklistPort = 11113;
        unsigned short qrPort = 11114;
        unsigned short mppsPort = 11115;
        
        // Client configuration
        pacs::common::ServiceConfig clientConfig;
        clientConfig.aeTitle = "PACS_CLIENT";
        clientConfig.localPort = 0;  // Let system choose port
        
        // Initialize thread manager
        pacs::core::thread::ThreadManager::getInstance().initialize(4, 2);
        
        // Create client directories
        std::string baseDir = "./pacs_client_data";
        std::string retrieveDir = baseDir + "/retrieved";
        
        if (!fs::exists(baseDir)) {
            fs::create_directories(baseDir);
        }
        if (!fs::exists(retrieveDir)) {
            fs::create_directories(retrieveDir);
        }
        
        // Create all SCU modules
        auto storageSCU = std::make_unique<pacs::storage::scu::StorageSCU>(clientConfig);
        auto worklistSCU = std::make_unique<pacs::worklist::scu::WorklistSCU>(clientConfig);
        auto qrSCU = std::make_unique<pacs::query_retrieve::scu::QueryRetrieveSCU>(clientConfig);
        auto mppsSCU = std::make_unique<pacs::mpps::scu::MPPSSCU>(clientConfig);
        
        // Configure remote peers
        storageSCU->setRemotePeer(serverAETitle, serverHost, storagePort);
        worklistSCU->setRemotePeer(serverAETitle, serverHost, worklistPort);
        qrSCU->setRemotePeer(serverAETitle, serverHost, qrPort);
        mppsSCU->setRemotePeer(serverAETitle, serverHost, mppsPort);
        
        // Set directory for retrieved images
        qrSCU->setRetrieveDirectory(retrieveDir);
        
        // Menu-driven interface
        bool running = true;
        while (running) {
            int choice = showMenu();
            
            // Track MPPS item between operations
            static pacs::core::interfaces::mpps::MPPSItem currentMppsItem;
            static bool mppsCreated = false;
            
            switch (choice) {
                case 1: {
                    // Query worklist
                    queryWorklist(worklistSCU);
                    break;
                }
                case 2: {
                    // Create MPPS (In Progress)
                    auto worklistItems = queryWorklist(worklistSCU);
                    if (!worklistItems.empty()) {
                        int itemIndex = 0;
                        if (worklistItems.size() > 1) {
                            log_module::write_information("Select worklist item (1-%zu): ", worklistItems.size());
                            std::cin >> itemIndex;
                            if (itemIndex < 1 || itemIndex > static_cast<int>(worklistItems.size())) {
                                log_module::write_error("Invalid selection");
                                break;
                            }
                            itemIndex--;  // Convert to 0-based index
                        }
                        
                        createMPPS(mppsSCU, worklistItems[itemIndex]);
                        mppsCreated = true;
                        
                        // Store for later update
                        currentMppsItem.patientID = worklistItems[itemIndex].patientID;
                        currentMppsItem.patientName = worklistItems[itemIndex].patientName;
                        currentMppsItem.studyInstanceUID = generateUID("1.2.840.10008.5.1.4.1.1.4");
                        currentMppsItem.performedProcedureStepID = worklistItems[itemIndex].requestedProcedureID;
                        currentMppsItem.procedureStatus = pacs::core::interfaces::mpps::MPPSStatus::IN_PROGRESS;
                        currentMppsItem.procedureStepStartDateTime = getCurrentDateTime();
                        
                        pacs::core::interfaces::mpps::PerformedSeries series;
                        series.seriesInstanceUID = generateUID("1.2.840.10008.5.1.4.1.1.4.1");
                        series.modality = worklistItems[itemIndex].modality;
                        series.numberOfInstances = 0;
                        currentMppsItem.performedSeriesSequence.push_back(series);
                    }
                    break;
                }
                case 3: {
                    // Update MPPS (Completed)
                    if (!mppsCreated) {
                        log_module::write_error("No MPPS created yet. Create MPPS first.");
                        break;
                    }
                    updateMPPS(mppsSCU, currentMppsItem);
                    break;
                }
                case 4: {
                    // Store DICOM Files
                    std::string dicomDir;
                    log_module::write_information("Enter path to DICOM files directory: ");
                    std::cin.ignore();
                    std::getline(std::cin, dicomDir);
                    if (dicomDir.empty()) {
                        dicomDir = "./sample_dicom";  // Default
                    }
                    storeDICOMFiles(storageSCU, dicomDir);
                    break;
                }
                case 5: {
                    // Query Patient/Study
                    std::string patientID;
                    log_module::write_information("Enter Patient ID (leave empty for all): ");
                    std::cin.ignore();
                    std::getline(std::cin, patientID);
                    queryPatientStudy(qrSCU, patientID);
                    break;
                }
                case 6: {
                    // Retrieve Images
                    std::string studyUID;
                    log_module::write_information("Enter Study Instance UID: ");
                    std::cin.ignore();
                    std::getline(std::cin, studyUID);
                    if (!studyUID.empty()) {
                        retrieveImages(qrSCU, studyUID);
                    } else {
                        log_module::write_error("No Study Instance UID provided");
                    }
                    break;
                }
                case 7: {
                    // Run Complete Workflow
                    runCompleteWorkflow(worklistSCU, mppsSCU, storageSCU, qrSCU);
                    break;
                }
                case 8:
                    // Exit
                    running = false;
                    break;
                default:
                    log_module::write_error("Invalid choice. Please try again.");
                    break;
            }
            
            if (running) {
                log_module::write_information("Press Enter to continue...");
                std::cin.ignore();
                std::cin.get();
            }
        }
        
        log_module::write_information("PACS Client exiting");
        log_module::stop();
    }
    catch (const std::exception& ex) {
        log_module::write_error("Error: %s", ex.what());
        log_module::stop();
        return 1;
    }
    
    return 0;
}