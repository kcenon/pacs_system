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
#include "dcmtk/dcmnet/dimcmd.h"
#include "dcmtk/dcmnet/dul.h"

#include "thread_system/sources/priority_thread_pool/callback_priority_job.h"
#include "thread_system/sources/logger/logger.h"
#include "common/dicom_util.h"

namespace pacs {
namespace storage {
namespace scp {

namespace fs = std::filesystem;
using namespace priority_thread_pool_module;
using namespace log_module;

StorageSCP::StorageSCP(const common::ServiceConfig& config, const std::string& storageDirectory)
    : config_(config), storageDirectory_(storageDirectory), running_(false) {
    // Configure DCMTK logging
    OFLog::configure(OFLogger::WARN_LOG_LEVEL);
    
    // Initialize thread pool
    threadPool_ = std::make_shared<priority_thread_pool_module::priority_thread_pool>("PACS_StorageSCP");
    auto result = threadPool_->start();
    if (result.has_error()) {
        write_error("Failed to start Storage SCP thread pool: {}", result.get_error().message());
    } else {
        write_information("Storage SCP thread pool started successfully");
    }
    
    // Create storage directory if it doesn't exist
    if (!storageDirectory_.empty() && !fs::exists(storageDirectory_)) {
        try {
            fs::create_directories(storageDirectory_);
            write_information("Created storage directory: {}", storageDirectory_);
        } catch (const std::exception& e) {
            write_error("Failed to create storage directory: {}", e.what());
        }
    }
}

StorageSCP::~StorageSCP() {
    stop();
    
    // Stop thread pool
    if (threadPool_) {
        auto result = threadPool_->stop();
        if (result.has_error()) {
            write_error("Error stopping Storage SCP thread pool: {}", result.get_error().message());
        } else {
            write_information("Storage SCP thread pool stopped successfully");
        }
    }
}

core::Result<void> StorageSCP::start() {
    if (running_) {
        write_information("Storage SCP is already running");
        return core::Result<void>::error("Storage SCP is already running");
    }
    
    write_information("Starting Storage SCP on port {}", config_.localPort);
    
    running_ = true;
    serverThread_ = std::thread(&StorageSCP::serverLoop, this);
    
    write_information("Storage SCP started successfully");
    return core::Result<void>::ok();
}

void StorageSCP::stop() {
    if (running_) {
        write_information("Stopping Storage SCP");
        running_ = false;
        if (serverThread_.joinable()) {
            serverThread_.join();
        }
        write_information("Storage SCP stopped");
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
    
    // Initialize network
    cond = ASC_initializeNetwork(NET_ACCEPTOR, config_.localPort, 30, &net);
    if (cond.bad()) {
        write_error("Error initializing network: {}", cond.text());
        return;
    }
    
    // Main server loop
    while (running_) {
        // Accept associations
        T_ASC_Association* assoc = nullptr;
        cond = ASC_receiveAssociation(net, &assoc, ASC_DEFAULTMAXPDU);
        
        if (cond.bad()) {
            if (cond != DUL_ASSOCIATIONREJECTED) { // Changed from DUL_ASSOCIATIONABORTED
                write_error("Error receiving association: {}", cond.text());
            }
            continue;
        }
        
        // Process the association in a separate thread using priority thread pool
        auto job = std::make_unique<callback_priority_job>(
            [this, assoc]() -> result_void {
                try {
                    this->processAssociation(assoc);
                    return {};
                } catch (const std::exception& ex) {
                    return result_void::error(fmt::format("Error processing association: {}", ex.what()));
                }
            },
            job_priorities::High,
            "PACS_ProcessAssociation"
        );
        
        auto result = threadPool_->enqueue(std::move(job));
        if (result.has_error()) {
            write_error("Failed to enqueue association processing job: {}", result.get_error().message());
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
        
        // Receive command
        DcmDataset* dataset = nullptr;
        cond = DIMSE_receiveCommand(assoc, DIMSE_BLOCKING, 0, &presID, &request, &dataset);
        
        if (cond.bad()) {
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
                    write_information("C-ECHO response: {}", dumpStr.c_str());
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
        write_error("C-STORE error response: {}", dumpStr.c_str());
        
        // Send the message
        DIMSE_sendMessage(assoc, presID, &response, nullptr, nullptr);
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
                write_error("Error in Storage callback: {}", ex.what());
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
    write_information("C-STORE response: {}", dumpStr.c_str());
    
    // Send the message
    DIMSE_sendMessage(assoc, presID, &response, nullptr, nullptr);
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
        fileFormat.setDataset(new DcmDataset(*dataset));  // Create a copy of the dataset
        
        // Save to file
        OFCondition cond = fileFormat.saveFile(filename.c_str(), EXS_LittleEndianExplicit);
        
        return cond.good();
    }
    catch (const std::exception& ex) {
        write_error("Error storing DICOM file: {}", ex.what());
        return false;
    }
}

} // namespace scp
} // namespace storage
} // namespace pacs