#include "storage_scp.h"

#include <string>
#include <filesystem>
#include <fstream>

// DCMTK includes
#include "dcmtk/dcmnet/diutil.h"
#include "dcmtk/dcmdata/dcfilefo.h"
#include "dcmtk/dcmdata/dcuid.h"
#include "dcmtk/dcmdata/dcmetinf.h"
#include "dcmtk/dcmdata/dcdeftag.h"
#include "dcmtk/dcmnet/dimse.h"
#include "dcmtk/dcmnet/dicom.h"
#include "dcmtk/dcmnet/dul.h"
#include "dcmtk/dcmnet/assoc.h"
#include "dcmtk/dcmnet/cond.h"

#include "common/logger/logger.h"
#include "thread_pool/core/thread_pool.h"
#include "thread_base/jobs/callback_job.h"
#include "common/dicom_util.h"

namespace pacs {
namespace storage {
namespace scp {

namespace fs = std::filesystem;
using namespace thread_pool_module;
using namespace thread_module;

StorageSCP::StorageSCP(const common::ServiceConfig& config, const std::string& storageDirectory)
    : config_(config), storageDirectory_(storageDirectory), running_(false) {
    // Configure DCMTK logging
    OFLog::configure(OFLogger::WARN_LOG_LEVEL);
    
    // Initialize thread pool
    threadPool_ = std::make_shared<thread_pool_module::thread_pool>("PACS_StorageSCP");
    auto result = threadPool_->start();
    if (!result) {
        pacs::common::logger::logError("Failed to start Storage SCP thread pool: %s", result.value_or("Unknown error").c_str());
    } else {
        pacs::common::logger::logInfo("Storage SCP thread pool started successfully");
    }
    
    // Create storage directory if it doesn't exist
    if (!storageDirectory_.empty() && !fs::exists(storageDirectory_)) {
        try {
            fs::create_directories(storageDirectory_);
            pacs::common::logger::logInfo("Created storage directory: %s", storageDirectory_.c_str());
        } catch (const std::exception& e) {
            pacs::common::logger::logError("Failed to create storage directory: %s", e.what());
        }
    }
}

StorageSCP::~StorageSCP() {
    stop();
    
    // Stop thread pool
    if (threadPool_) {
        threadPool_->stop();
        pacs::common::logger::logInfo("Storage SCP thread pool stopped successfully");
    }
}

core::Result<void> StorageSCP::start() {
    if (running_) {
        pacs::common::logger::logInfo("Storage SCP is already running");
        return core::Result<void>::error("Storage SCP is already running");
    }
    
    pacs::common::logger::logInfo("Starting Storage SCP on port %d", config_.localPort);
    
    running_ = true;
    serverThread_ = std::thread(&StorageSCP::serverLoop, this);
    
    pacs::common::logger::logInfo("Storage SCP started successfully");
    return core::Result<void>::ok();
}

void StorageSCP::stop() {
    if (running_) {
        pacs::common::logger::logInfo("Stopping Storage SCP");
        running_ = false;
        if (serverThread_.joinable()) {
            serverThread_.join();
        }
        pacs::common::logger::logInfo("Storage SCP stopped");
    }
}

core::Result<void> StorageSCP::storeDICOM(const DcmDataset* dataset) {
    // Not used in SCP role
    return core::Result<void>::error("storeDICOM not implemented for SCP role");
}

core::Result<void> StorageSCP::storeDICOMFile(const std::string& filename) {
    // Not used in SCP role
    return core::Result<void>::error("storeDICOMFile not implemented for SCP role");
}

core::Result<void> StorageSCP::storeDICOMFiles(const std::vector<std::string>& filenames) {
    // Not used in SCP role
    return core::Result<void>::error("storeDICOMFiles not implemented for SCP role");
}

void StorageSCP::setStorageCallback(core::interfaces::storage::StorageCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    storageCallback_ = callback;
}

void StorageSCP::setStorageDirectory(const std::string& directory) {
    storageDirectory_ = directory;
    
    // Create directory if it doesn't exist
    if (!storageDirectory_.empty() && !fs::exists(storageDirectory_)) {
        try {
            fs::create_directories(storageDirectory_);
        } catch (const std::exception& e) {
            std::cerr << "Failed to create storage directory: " << e.what() << std::endl;
        }
    }
}

void StorageSCP::serverLoop() {
    T_ASC_Network* net = nullptr;
    
    OFCondition cond;
    
    // Initialize network with timeout
    cond = ASC_initializeNetwork(NET_ACCEPTOR, config_.localPort, 1, &net);
    if (cond.bad()) {
        pacs::common::logger::logError("Error initializing network: %s", cond.text());
        return;
    }
    
    // Main server loop
    while (running_) {
        // Use ASC_associationWaiting to check if association is available with timeout
        if (!ASC_associationWaiting(net, 1)) {
            // No association within 1 second timeout, check running_ and continue
            continue;
        }
        
        // Accept associations
        T_ASC_Association* assoc = nullptr;
        cond = ASC_receiveAssociation(net, &assoc, ASC_DEFAULTMAXPDU);
        
        if (cond.bad()) {
            if (cond != DUL_ASSOCIATIONREJECTED) {
                pacs::common::logger::logError("Error receiving association: %s", cond.text());
            }
            continue;
        }
        
        // Process the association in a separate thread using thread pool
        auto job = std::make_unique<thread_module::callback_job>(
            [this, assoc]() -> std::optional<std::string> {
                this->processAssociation(assoc);
                return std::nullopt;
            },
            "PACS_ProcessAssociation"
        );
        auto result = threadPool_->enqueue(std::move(job));
        
        if (!result) {
            pacs::common::logger::logError("Failed to enqueue association processing job: %s", result.value_or("Unknown error").c_str());
            ASC_dropAssociation(assoc);
            ASC_destroyAssociation(&assoc);
        }
    }
    
    // Cleanup
    ASC_dropNetwork(&net);
}

void StorageSCP::processAssociation(T_ASC_Association* assoc) {
    if (!assoc) {
        return;
    }
    
    // Accept association
    const char* transferSyntaxes[] = { UID_LittleEndianExplicitTransferSyntax, 
                                    UID_BigEndianExplicitTransferSyntax, 
                                    UID_LittleEndianImplicitTransferSyntax };
    int numTransferSyntaxes = 3;
    
    // Accept presentation contexts for all Storage SOP Classes
    for (int i = 0; i < ASC_countPresentationContexts(assoc->params); i++) {
        T_ASC_PresentationContext pc;
        ASC_getPresentationContext(assoc->params, i, &pc);
        
        // Check if this is a storage SOP class
        // Use dcmAllStorageSOPClassUIDs instead of deprecated dcmStorageSOPClassUIDs
        for (int j = 0; j < numberOfDcmAllStorageSOPClassUIDs; j++) {
            if (strcmp(pc.abstractSyntax, dcmAllStorageSOPClassUIDs[j]) == 0) {
                // Use correct parameters for newer DCMTK API
                ASC_acceptPresentationContext(assoc->params, pc.presentationContextID,
                                            transferSyntaxes[0]);
                break;
            }
        }
    }
    
    // Acknowledge association
    OFCondition cond = ASC_acknowledgeAssociation(assoc);
    if (cond.bad()) {
        ASC_dropAssociation(assoc);
        ASC_destroyAssociation(&assoc);
        return;
    }
    
    // Main association loop - handle DIMSE messages
    bool finished = false;
    
    while (!finished && running_) {
        T_DIMSE_Message request;
        T_ASC_PresentationContextID presID;
        
        // Receive command with timeout
        DcmDataset* dataset = nullptr;
        cond = DIMSE_receiveCommand(assoc, DIMSE_NONBLOCKING, 1, &presID, &request, &dataset);
        
        if (cond.bad()) {
            // Check if it's just a timeout (no data available)
            if (cond == DIMSE_NODATAAVAILABLE) {
                // No data available within timeout, check running_ and continue
                continue;
            }
            // Other errors - finish the association
            finished = true;
            continue;
        }
        
        // Process command
        switch (request.CommandField) {
            case DIMSE_C_STORE_RQ:
                handleCStoreRequest(assoc, request, presID, dataset);
                break;
                
            case DIMSE_C_ECHO_RQ:
                // Handle C-ECHO (ping) request
                {
                    T_DIMSE_Message response;
                    memset(&response, 0, sizeof(response));
                    response.CommandField = DIMSE_C_ECHO_RSP;
                    response.msg.CEchoRSP.MessageIDBeingRespondedTo = request.msg.CEchoRQ.MessageID;
                    response.msg.CEchoRSP.DimseStatus = STATUS_Success;
                    response.msg.CEchoRSP.DataSetType = DIMSE_DATASET_NULL;
                    
                    // Log the response instead of sending it directly
                    OFString dumpStr;
                    DIMSE_dumpMessage(dumpStr, response, DIMSE_OUTGOING);
                    pacs::common::logger::logInfo("C-ECHO response: %s", dumpStr.c_str());
                }
                break;
                
            default:
                // Unsupported command
                break;
        }
        
        // Clean up dataset
        delete dataset;
    }
    
    // Release association
    ASC_releaseAssociation(assoc);
    ASC_dropAssociation(assoc);
    ASC_destroyAssociation(&assoc);
}

void StorageSCP::handleCStoreRequest(T_ASC_Association* assoc, T_DIMSE_Message& request, 
                                   T_ASC_PresentationContextID presID, DcmDataset* dataset) {
    if (!dataset) {
        // Error - no dataset
        T_DIMSE_Message response;
        memset(&response, 0, sizeof(response));
        response.CommandField = DIMSE_C_STORE_RSP;
        response.msg.CStoreRSP.MessageIDBeingRespondedTo = request.msg.CStoreRQ.MessageID;
        OFStandard::strlcpy(response.msg.CStoreRSP.AffectedSOPClassUID, 
                          request.msg.CStoreRQ.AffectedSOPClassUID, 
                          sizeof(response.msg.CStoreRSP.AffectedSOPClassUID));
        OFStandard::strlcpy(response.msg.CStoreRSP.AffectedSOPInstanceUID, 
                          request.msg.CStoreRQ.AffectedSOPInstanceUID, 
                          sizeof(response.msg.CStoreRSP.AffectedSOPInstanceUID));
        response.msg.CStoreRSP.DimseStatus = STATUS_STORE_Error_CannotUnderstand;
        response.msg.CStoreRSP.DataSetType = DIMSE_DATASET_NULL;
        
        // Log message instead of sending directly with DIMSE_sendMessage
        OFString dumpStr;
        DIMSE_dumpMessage(dumpStr, response, DIMSE_OUTGOING);
        pacs::common::logger::logError("C-STORE error response: %s", dumpStr.c_str());
        
        // Send the message using DCMTK 3.6.9 API
        DIMSE_sendMessageUsingMemoryData(assoc, presID, &response, nullptr, nullptr, nullptr, nullptr);
        return;
    }
    
    // Extract SOP Instance UID and other relevant information
    std::string sopClassUID = request.msg.CStoreRQ.AffectedSOPClassUID;
    std::string sopInstanceUID = request.msg.CStoreRQ.AffectedSOPInstanceUID;
    
    // Store the dataset to disk
    bool storeSuccess = storeDatasetToDisk(sopClassUID, sopInstanceUID, dataset);
    
    // Invoke callback if set
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        if (storageCallback_) {
            try {
                storageCallback_(sopInstanceUID, dataset);
            }
            catch (const std::exception& ex) {
                pacs::common::logger::logError("Error in Storage callback: %s", ex.what());
            }
        }
    }
    
    // Respond with success or failure
    T_DIMSE_Message response;
    memset(&response, 0, sizeof(response));
    response.CommandField = DIMSE_C_STORE_RSP;
    response.msg.CStoreRSP.MessageIDBeingRespondedTo = request.msg.CStoreRQ.MessageID;
    OFStandard::strlcpy(response.msg.CStoreRSP.AffectedSOPClassUID, 
                      sopClassUID.c_str(), 
                      sizeof(response.msg.CStoreRSP.AffectedSOPClassUID));
    OFStandard::strlcpy(response.msg.CStoreRSP.AffectedSOPInstanceUID, 
                      sopInstanceUID.c_str(), 
                      sizeof(response.msg.CStoreRSP.AffectedSOPInstanceUID));
    
    response.msg.CStoreRSP.DimseStatus = storeSuccess ? STATUS_Success : STATUS_STORE_Error_CannotUnderstand;
    response.msg.CStoreRSP.DataSetType = DIMSE_DATASET_NULL;
    
    // Log message
    OFString dumpStr;
    DIMSE_dumpMessage(dumpStr, response, DIMSE_OUTGOING);
    pacs::common::logger::logInfo("C-STORE response: %s", dumpStr.c_str());
    
    // Send the message using DCMTK 3.6.9 API
    DIMSE_sendMessageUsingMemoryData(assoc, presID, &response, nullptr, nullptr, nullptr, nullptr);
}

bool StorageSCP::storeDatasetToDisk(const std::string& sopClassUID, 
                                  const std::string& sopInstanceUID, 
                                  DcmDataset* dataset) {
    if (storageDirectory_.empty() || !dataset) {
        return false;
    }
    
    try {
        // Create a filename based on SOP Instance UID
        std::string filename = storageDirectory_ + "/" + sopInstanceUID + ".dcm";
        
        // Create DICOM file format
        DcmFileFormat fileFormat;
        // Copy dataset to file format
        *fileFormat.getDataset() = *dataset;
        
        // Save to file
        OFCondition cond = fileFormat.saveFile(filename.c_str(), EXS_LittleEndianExplicit);
        
        return cond.good();
    }
    catch (const std::exception& ex) {
        pacs::common::logger::logError("Error storing DICOM file: %s", ex.what());
        return false;
    }
}

} // namespace scp
} // namespace storage
} // namespace pacs