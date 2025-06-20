#include "query_retrieve_scp.h"

#include <iostream>
#include <string>
#include <filesystem>
#include <thread>

// DCMTK includes
#include "dcmtk/dcmnet/diutil.h"
#include "dcmtk/dcmdata/dcfilefo.h"
#include "dcmtk/dcmdata/dcuid.h"
#include "dcmtk/dcmdata/dcdeftag.h"
#include "dcmtk/dcmdata/dcdict.h"
#include "dcmtk/dcmnet/dimse.h"
#include "dcmtk/dcmnet/dicom.h"
#include "dcmtk/dcmnet/dimcmd.h"
#include "dcmtk/dcmnet/dul.h"
#include "dcmtk/dcmqrdb/dcmqrdbs.h"
#include "dcmtk/dcmqrdb/dcmqrsrv.h"

#include "common/dicom_util.h"
#include "thread_system/sources/logger/logger.h"

namespace pacs {
namespace query_retrieve {
namespace scp {

using namespace log_module;

namespace fs = std::filesystem;

QueryRetrieveSCP::QueryRetrieveSCP(const common::ServiceConfig& config, const std::string& storageDirectory)
    : config_(config), storageDirectory_(storageDirectory), running_(false) {
    // Configure DCMTK logging
    OFLog::configure(OFLogger::WARN_LOG_LEVEL);
    
    // Create storage directory if it doesn't exist
    if (!storageDirectory_.empty() && !fs::exists(storageDirectory_)) {
        try {
            fs::create_directories(storageDirectory_);
        } catch (const std::exception& e) {
            write_error("Failed to create storage directory: %s", e.what());
        }
    }
    
    // Index all DICOM files in the storage directory
    if (!storageDirectory_.empty()) {
        indexStorageDirectory();
    }
}

QueryRetrieveSCP::~QueryRetrieveSCP() {
    stop();
}

core::Result<void> QueryRetrieveSCP::start() {
    if (running_) {
        return core::Result<void>::error("Query/Retrieve SCP is already running");
    }
    
    running_ = true;
    serverThread_ = std::thread(&QueryRetrieveSCP::serverLoop, this);
    
    return core::Result<void>::ok();
}

void QueryRetrieveSCP::stop() {
    if (running_) {
        running_ = false;
        if (serverThread_.joinable()) {
            serverThread_.join();
        }
    }
}

core::Result<void> pacs::query_retrieve::scp::QueryRetrieveSCP::addFile(const std::string& filename) {
    if (!fs::exists(filename)) {
        return core::Result<void>::error("File does not exist: " + filename);
    }
    
    try {
        std::string destFilename;
        std::map<std::string, std::string> metadata;
        
        // Load DICOM file
        DcmFileFormat fileFormat;
        OFCondition cond = fileFormat.loadFile(filename.c_str());
        
        if (cond.bad()) {
            return core::Result<void>::error(std::string("Failed to load DICOM file: ") + cond.text());
        }
        
        // Extract metadata for indexing
        metadata = extractMetadata(fileFormat.getDataset());
        
        // Generate a destination filename based on SOP Instance UID
        std::string sopInstanceUID = metadata["SOPInstanceUID"];
        
        if (sopInstanceUID.empty()) {
            return core::Result<void>::error("Missing SOP Instance UID in DICOM file");
        }
        
        // Create destination filename
        destFilename = storageDirectory_ + "/" + sopInstanceUID + ".dcm";
        
        // Copy file if source and destination are different
        if (filename != destFilename) {
            fs::copy_file(filename, destFilename, fs::copy_options::overwrite_existing);
        }
        
        // Add to index
        {
            std::lock_guard<std::mutex> lock(indexMutex_);
            dicomIndex_[destFilename] = metadata;
        }
        
        return core::Result<void>::ok();
    }
    catch (const std::exception& ex) {
        return core::Result<void>::error(std::string("Exception adding file: ") + ex.what());
    }
}

core::Result<std::vector<DcmDataset*>> pacs::query_retrieve::scp::QueryRetrieveSCP::query(const DcmDataset* searchDataset, 
                                                      core::interfaces::query_retrieve::QueryRetrieveLevel level) {
    if (!searchDataset) {
        return core::Result<std::vector<DcmDataset*>>::error("Search dataset is null");
    }
    
    std::vector<DcmDataset*> result;
    
    // Lock the index
    std::lock_guard<std::mutex> lock(indexMutex_);
    
    try {
        // Go through all indexed files and check if they match the search criteria
        for (const auto& [filename, metadata] : dicomIndex_) {
            // Load the DICOM file
            DcmFileFormat fileFormat;
            OFCondition cond = fileFormat.loadFile(filename.c_str());
            
            if (cond.good()) {
                DcmDataset* dataset = fileFormat.getDataset();
                
                // Check if this dataset matches the search criteria
                if (matchDataset(searchDataset, dataset)) {
                    // Create a copy of the matching dataset
                    result.push_back(new DcmDataset(*dataset));
                    
                    // Invoke callback if set
                    if (queryCallback_) {
                        try {
                            // Create a QueryResultItem from the dataset
                            core::interfaces::query_retrieve::QueryResultItem item;
                            item.level = level;
                            
                            DcmElement* element = nullptr;
                            OFString valueStr;
                            
                            if (dataset->findAndGetElement(DCM_PatientID, element).good() && element) {
                                element->getOFString(valueStr, 0);
                                item.patientID = valueStr.c_str();
                            }
                            
                            if (dataset->findAndGetElement(DCM_PatientName, element).good() && element) {
                                element->getOFString(valueStr, 0);
                                item.patientName = valueStr.c_str();
                            }
                            
                            if (dataset->findAndGetElement(DCM_StudyInstanceUID, element).good() && element) {
                                element->getOFString(valueStr, 0);
                                item.studyInstanceUID = valueStr.c_str();
                            }
                            
                            if (dataset->findAndGetElement(DCM_StudyDescription, element).good() && element) {
                                element->getOFString(valueStr, 0);
                                item.studyDescription = valueStr.c_str();
                            }
                            
                            if (dataset->findAndGetElement(DCM_SeriesInstanceUID, element).good() && element) {
                                element->getOFString(valueStr, 0);
                                item.seriesInstanceUID = valueStr.c_str();
                            }
                            
                            if (dataset->findAndGetElement(DCM_SeriesDescription, element).good() && element) {
                                element->getOFString(valueStr, 0);
                                item.seriesDescription = valueStr.c_str();
                            }
                            
                            if (dataset->findAndGetElement(DCM_SOPInstanceUID, element).good() && element) {
                                element->getOFString(valueStr, 0);
                                item.sopInstanceUID = valueStr.c_str();
                            }
                            
                            if (dataset->findAndGetElement(DCM_SOPClassUID, element).good() && element) {
                                element->getOFString(valueStr, 0);
                                item.sopClassUID = valueStr.c_str();
                            }
                            
                            queryCallback_(item, dataset);
                        }
                        catch (const std::exception& ex) {
                            write_error("Error in query callback: %s", ex.what());
                        }
                    }
                }
            }
        }
        
        return core::Result<std::vector<DcmDataset*>>::ok(result);
    }
    catch (const std::exception& ex) {
        // Clean up in case of exception
        for (auto* dataset : result) {
            delete dataset;
        }
        result.clear();
        
        return core::Result<std::vector<DcmDataset*>>::error(std::string("Exception during query: ") + ex.what());
    }
}

core::Result<void> pacs::query_retrieve::scp::QueryRetrieveSCP::retrieve(const std::string& /*studyInstanceUID*/,
                                     const std::string& /*seriesInstanceUID*/,
                                     const std::string& /*sopInstanceUID*/) {
    // Not used in SCP role
    return core::Result<void>::error("retrieve not implemented for SCP role");
}

void pacs::query_retrieve::scp::QueryRetrieveSCP::setQueryCallback(core::interfaces::query_retrieve::QueryCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    queryCallback_ = callback;
}

void pacs::query_retrieve::scp::QueryRetrieveSCP::setRetrieveCallback(core::interfaces::query_retrieve::RetrieveCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    retrieveCallback_ = callback;
}

void pacs::query_retrieve::scp::QueryRetrieveSCP::setMoveCallback(core::interfaces::query_retrieve::MoveCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    moveCallback_ = callback;
}

void pacs::query_retrieve::scp::QueryRetrieveSCP::setStorageDirectory(const std::string& directory) {
    storageDirectory_ = directory;
    
    // Create directory if it doesn't exist
    if (!storageDirectory_.empty() && !fs::exists(storageDirectory_)) {
        try {
            fs::create_directories(storageDirectory_);
        } catch (const std::exception& e) {
            write_error("Failed to create storage directory: %s", e.what());
        }
    }
    
    // Reindex the storage directory
    indexStorageDirectory();
}

void pacs::query_retrieve::scp::QueryRetrieveSCP::serverLoop() {
    T_ASC_Network* net = nullptr;
    
    OFCondition cond;
    
    // Initialize network
    cond = ASC_initializeNetwork(NET_ACCEPTOR, config_.localPort, 30, &net);
    if (cond.bad()) {
        write_error("Error initializing network: %s", cond.text());
        return;
    }
    
    // Main server loop
    while (running_) {
        // Accept associations
        T_ASC_Association* assoc = nullptr;
        cond = ASC_receiveAssociation(net, &assoc, ASC_DEFAULTMAXPDU);
        
        if (cond.bad()) {
            if (cond != DUL_ASSOCIATIONREJECTED) { // Updated from DUL_ASSOCIATIONABORTED
                write_error("Error receiving association: %s", cond.text());
            }
            continue;
        }
        
        // Process the association in a separate thread
        std::thread([this, assoc]() {
            this->processAssociation(assoc);
        }).detach();
    }
    
    // Cleanup
    ASC_dropNetwork(&net);
}

void QueryRetrieveSCP::processAssociation(T_ASC_Association* assoc) {
    if (!assoc) {
        return;
    }
    
    // Accept association
    const char* transferSyntaxes[] = { UID_LittleEndianExplicitTransferSyntax, 
                                    UID_BigEndianExplicitTransferSyntax, 
                                    UID_LittleEndianImplicitTransferSyntax };
    int numTransferSyntaxes = 3;
    
    // Accept presentation contexts for Query/Retrieve
    for (int i = 0; i < ASC_countPresentationContexts(assoc->params); i++) {
        T_ASC_PresentationContext pc;
        ASC_getPresentationContext(assoc->params, i, &pc);
        
        // Accept Study Root Query/Retrieve Information Models
        if (strcmp(pc.abstractSyntax, UID_FINDStudyRootQueryRetrieveInformationModel) == 0 ||
            strcmp(pc.abstractSyntax, UID_MOVEStudyRootQueryRetrieveInformationModel) == 0 ||
            strcmp(pc.abstractSyntax, UID_GETStudyRootQueryRetrieveInformationModel) == 0) {
            ASC_acceptPresentationContext(assoc->params, pc.presentationContextID,
                                        transferSyntaxes[0], numTransferSyntaxes);
        }
        // Also accept storage SOP classes for C-STORE operations during C-GET
        else {
            for (int j = 0; j < numberOfDcmAllStorageSOPClassUIDs; j++) { // Updated from numberOfDcmStorageSOPClassUIDs
                if (strcmp(pc.abstractSyntax, dcmAllStorageSOPClassUIDs[j]) == 0) { // Updated from dcmStorageSOPClassUIDs
                    ASC_acceptPresentationContext(assoc->params, pc.presentationContextID,
                                                transferSyntaxes[0], numTransferSyntaxes);
                    break;
                }
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
            case DIMSE_C_FIND_RQ:
                handleCFindRequest(assoc, request, presID, dataset);
                break;
                
            case DIMSE_C_MOVE_RQ:
                handleCMoveRequest(assoc, request, presID, dataset);
                break;
                
            case DIMSE_C_GET_RQ:
                handleCGetRequest(assoc, request, presID, dataset);
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
                    DIMSE_sendMessage(assoc, presID, &response, nullptr, nullptr);
                }
                break;
                
            case DIMSE_C_STORE_RQ: // Adding missing case for STORE request
                // Just acknowledge the STORE request with success
                {
                    T_DIMSE_Message response;
                    memset(&response, 0, sizeof(response));
                    response.CommandField = DIMSE_C_STORE_RSP;
                    response.msg.CStoreRSP.MessageIDBeingRespondedTo = request.msg.CStoreRQ.MessageID;
                    response.msg.CStoreRSP.DimseStatus = STATUS_Success;
                    response.msg.CStoreRSP.DataSetType = DIMSE_DATASET_NULL;
                    OFStandard::strlcpy(response.msg.CStoreRSP.AffectedSOPClassUID,
                                       request.msg.CStoreRQ.AffectedSOPClassUID,
                                       sizeof(response.msg.CStoreRSP.AffectedSOPClassUID));
                    OFStandard::strlcpy(response.msg.CStoreRSP.AffectedSOPInstanceUID,
                                       request.msg.CStoreRQ.AffectedSOPInstanceUID,
                                       sizeof(response.msg.CStoreRSP.AffectedSOPInstanceUID));
                    DIMSE_sendMessage(assoc, presID, &response, nullptr, nullptr);
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

void QueryRetrieveSCP::handleCFindRequest(T_ASC_Association* assoc, T_DIMSE_Message& request, 
                                   T_ASC_PresentationContextID presID, DcmDataset* dataset) {
    if (!dataset) {
        // Error - no dataset
        T_DIMSE_Message response;
        memset(&response, 0, sizeof(response));
        response.CommandField = DIMSE_C_FIND_RSP;
        response.msg.CFindRSP.MessageIDBeingRespondedTo = request.msg.CFindRQ.MessageID;
        OFStandard::strlcpy(response.msg.CFindRSP.AffectedSOPClassUID, 
                          request.msg.CFindRQ.AffectedSOPClassUID, 
                          sizeof(response.msg.CFindRSP.AffectedSOPClassUID));
        response.msg.CFindRSP.DimseStatus = STATUS_FIND_Failed_UnableToProcess;
        response.msg.CFindRSP.DataSetType = DIMSE_DATASET_NULL;
        DIMSE_sendMessage(assoc, presID, &response, nullptr, nullptr);
        return;
    }
    
    // Determine query level
    core::interfaces::query_retrieve::QueryRetrieveLevel level = core::interfaces::query_retrieve::QueryRetrieveLevel::STUDY;
    
    DcmElement* levelElement = nullptr;
    if (dataset->findAndGetElement(DCM_QueryRetrieveLevel, levelElement).good() && levelElement) {
        OFString levelStr;
        levelElement->getOFString(levelStr, 0);
        
        if (levelStr == "PATIENT") {
            level = core::interfaces::query_retrieve::QueryRetrieveLevel::PATIENT;
        } else if (levelStr == "STUDY") {
            level = core::interfaces::query_retrieve::QueryRetrieveLevel::STUDY;
        } else if (levelStr == "SERIES") {
            level = core::interfaces::query_retrieve::QueryRetrieveLevel::SERIES;
        } else if (levelStr == "IMAGE") {
            level = core::interfaces::query_retrieve::QueryRetrieveLevel::IMAGE;
        }
    }
    
    // Find matching datasets
    core::Result<std::vector<DcmDataset*>> result = query(dataset, level);
    
    if (result.isError()) {
        // Error finding matches
        T_DIMSE_Message response;
        memset(&response, 0, sizeof(response));
        response.CommandField = DIMSE_C_FIND_RSP;
        response.msg.CFindRSP.MessageIDBeingRespondedTo = request.msg.CFindRQ.MessageID;
        OFStandard::strlcpy(response.msg.CFindRSP.AffectedSOPClassUID, 
                          request.msg.CFindRQ.AffectedSOPClassUID, 
                          sizeof(response.msg.CFindRSP.AffectedSOPClassUID));
        response.msg.CFindRSP.DimseStatus = STATUS_FIND_Failed_UnableToProcess;
        response.msg.CFindRSP.DataSetType = DIMSE_DATASET_NULL;
        DIMSE_sendMessage(assoc, presID, &response, nullptr, nullptr);
        return;
    }
    
    // Send matching datasets
    T_DIMSE_Message response;
    
    for (auto& matchingDataset : result.value()) {
        // Send matching dataset with pending status
        memset(&response, 0, sizeof(response));
        response.CommandField = DIMSE_C_FIND_RSP;
        response.msg.CFindRSP.MessageIDBeingRespondedTo = request.msg.CFindRQ.MessageID;
        OFStandard::strlcpy(response.msg.CFindRSP.AffectedSOPClassUID, 
                          request.msg.CFindRQ.AffectedSOPClassUID, 
                          sizeof(response.msg.CFindRSP.AffectedSOPClassUID));
        response.msg.CFindRSP.DimseStatus = STATUS_Pending;
        response.msg.CFindRSP.DataSetType = DIMSE_DATASET_PRESENT;
        
        OFCondition cond = DIMSE_sendMessageUsingMemoryData(assoc, presID, &response, nullptr, 
                                                         matchingDataset, nullptr, nullptr, nullptr);
        
        // Clean up dataset
        delete matchingDataset;
        
        if (cond.bad()) {
            // Error sending response, clean up remaining datasets
            for (size_t i = 1; i < result.value().size(); i++) {
                delete result.value()[i];
            }
            
            return;
        }
    }
    
    // Send final success response
    memset(&response, 0, sizeof(response));
    response.CommandField = DIMSE_C_FIND_RSP;
    response.msg.CFindRSP.MessageIDBeingRespondedTo = request.msg.CFindRQ.MessageID;
    OFStandard::strlcpy(response.msg.CFindRSP.AffectedSOPClassUID, 
                      request.msg.CFindRQ.AffectedSOPClassUID, 
                      sizeof(response.msg.CFindRSP.AffectedSOPClassUID));
    response.msg.CFindRSP.DimseStatus = STATUS_Success;
    response.msg.CFindRSP.DataSetType = DIMSE_DATASET_NULL;
    DIMSE_sendMessage(assoc, presID, &response, nullptr, nullptr);
}

void QueryRetrieveSCP::handleCMoveRequest(T_ASC_Association* assoc, T_DIMSE_Message& request, 
                                   T_ASC_PresentationContextID presID, DcmDataset* dataset) {
    if (!dataset) {
        // Error - no dataset
        T_DIMSE_Message response;
        memset(&response, 0, sizeof(response));
        response.CommandField = DIMSE_C_MOVE_RSP;
        response.msg.CMoveRSP.MessageIDBeingRespondedTo = request.msg.CMoveRQ.MessageID;
        OFStandard::strlcpy(response.msg.CMoveRSP.AffectedSOPClassUID, 
                          request.msg.CMoveRQ.AffectedSOPClassUID, 
                          sizeof(response.msg.CMoveRSP.AffectedSOPClassUID));
        response.msg.CMoveRSP.DimseStatus = STATUS_MOVE_Failed_UnableToProcess;
        response.msg.CMoveRSP.DataSetType = DIMSE_DATASET_NULL;
        response.msg.CMoveRSP.NumberOfRemainingSubOperations = 0;
        response.msg.CMoveRSP.NumberOfCompletedSubOperations = 0;
        response.msg.CMoveRSP.NumberOfFailedSubOperations = 0;
        response.msg.CMoveRSP.NumberOfWarningSubOperations = 0;
        DIMSE_sendMessage(assoc, presID, &response, nullptr, nullptr);
        return;
    }
    
    // Extract move destination
    const char* moveDestination = request.msg.CMoveRQ.MoveDestination;
    
    // Determine query level
    std::string queryLevel = "STUDY";
    DcmElement* levelElement = nullptr;
    if (dataset->findAndGetElement(DCM_QueryRetrieveLevel, levelElement).good() && levelElement) {
        OFString levelStr;
        levelElement->getOFString(levelStr, 0);
        queryLevel = levelStr.c_str();
    }
    
    // Extract UIDs from the dataset
    std::string studyInstanceUID;
    std::string seriesInstanceUID;
    std::string sopInstanceUID;
    
    DcmElement* element = nullptr;
    OFString valueStr;
    
    if (dataset->findAndGetElement(DCM_StudyInstanceUID, element).good() && element) {
        element->getOFString(valueStr, 0);
        studyInstanceUID = valueStr.c_str();
    }
    
    if (dataset->findAndGetElement(DCM_SeriesInstanceUID, element).good() && element) {
        element->getOFString(valueStr, 0);
        seriesInstanceUID = valueStr.c_str();
    }
    
    if (dataset->findAndGetElement(DCM_SOPInstanceUID, element).good() && element) {
        element->getOFString(valueStr, 0);
        sopInstanceUID = valueStr.c_str();
    }
    
    // Find matching files
    std::vector<std::string> matchingFiles;
    {
        std::lock_guard<std::mutex> lock(indexMutex_);
        
        for (const auto& [filename, metadata] : dicomIndex_) {
            bool matches = true;
            
            // Match based on UIDs
            if (!studyInstanceUID.empty() && metadata.find("StudyInstanceUID") != metadata.end()) {
                matches = matches && (metadata.at("StudyInstanceUID") == studyInstanceUID);
            }
            
            if (!seriesInstanceUID.empty() && metadata.find("SeriesInstanceUID") != metadata.end()) {
                matches = matches && (metadata.at("SeriesInstanceUID") == seriesInstanceUID);
            }
            
            if (!sopInstanceUID.empty() && metadata.find("SOPInstanceUID") != metadata.end()) {
                matches = matches && (metadata.at("SOPInstanceUID") == sopInstanceUID);
            }
            
            if (matches) {
                matchingFiles.push_back(filename);
            }
        }
    }
    
    // Create association with the move destination
    T_ASC_Association* subAssoc = nullptr;
    T_ASC_Network* net = nullptr;
    T_ASC_Parameters* params = nullptr;
    OFCondition cond;
    
    // Initialize network for sub-association
    cond = ASC_initializeNetwork(NET_REQUESTOR, 0, 0, &net);
    if (cond.bad()) {
        // Send error response
        T_DIMSE_Message response;
        memset(&response, 0, sizeof(response));
        response.CommandField = DIMSE_C_MOVE_RSP;
        response.msg.CMoveRSP.MessageIDBeingRespondedTo = request.msg.CMoveRQ.MessageID;
        OFStandard::strlcpy(response.msg.CMoveRSP.AffectedSOPClassUID, 
                          request.msg.CMoveRQ.AffectedSOPClassUID, 
                          sizeof(response.msg.CMoveRSP.AffectedSOPClassUID));
        response.msg.CMoveRSP.DimseStatus = STATUS_MOVE_Failed_UnableToProcess;
        response.msg.CMoveRSP.DataSetType = DIMSE_DATASET_NULL;
        response.msg.CMoveRSP.NumberOfRemainingSubOperations = matchingFiles.size();
        response.msg.CMoveRSP.NumberOfCompletedSubOperations = 0;
        response.msg.CMoveRSP.NumberOfFailedSubOperations = matchingFiles.size();
        response.msg.CMoveRSP.NumberOfWarningSubOperations = 0;
        DIMSE_sendMessage(assoc, presID, &response, nullptr, nullptr);
        return;
    }
    
    // Create association parameters
    cond = ASC_createAssociationParameters(&params, ASC_MAXIMUMPDUSIZE);
    if (cond.bad()) {
        ASC_dropNetwork(&net);
        
        // Send error response
        T_DIMSE_Message response;
        memset(&response, 0, sizeof(response));
        response.CommandField = DIMSE_C_MOVE_RSP;
        response.msg.CMoveRSP.MessageIDBeingRespondedTo = request.msg.CMoveRQ.MessageID;
        OFStandard::strlcpy(response.msg.CMoveRSP.AffectedSOPClassUID, 
                          request.msg.CMoveRQ.AffectedSOPClassUID, 
                          sizeof(response.msg.CMoveRSP.AffectedSOPClassUID));
        response.msg.CMoveRSP.DimseStatus = STATUS_MOVE_Failed_UnableToProcess;
        response.msg.CMoveRSP.DataSetType = DIMSE_DATASET_NULL;
        response.msg.CMoveRSP.NumberOfRemainingSubOperations = matchingFiles.size();
        response.msg.CMoveRSP.NumberOfCompletedSubOperations = 0;
        response.msg.CMoveRSP.NumberOfFailedSubOperations = matchingFiles.size();
        response.msg.CMoveRSP.NumberOfWarningSubOperations = 0;
        DIMSE_sendMessage(assoc, presID, &response, nullptr, nullptr);
        return;
    }
    
    // Query the configuration for the move destination information
    std::string destHost;
    int destPort = 0;
    
    // TODO: In a real implementation, this would look up the destination from a config
    // For now, use a fixed host:port combination for the move destination
    destHost = "localhost";
    destPort = 11112;
    
    // Set calling and called AE titles
    ASC_setAPTitles(params, config_.aeTitle.c_str(), moveDestination, nullptr);
    
    // Set network addresses
    ASC_setTransportLayerType(params, OFFalse);
    cond = ASC_setPresentationAddresses(params, OFStandard::getHostName().c_str(), 
                                      (destHost + ":" + std::to_string(destPort)).c_str());
    if (cond.bad()) {
        ASC_dropNetwork(&net);
        ASC_destroyAssociationParameters(&params);
        
        // Send error response
        T_DIMSE_Message response;
        memset(&response, 0, sizeof(response));
        response.CommandField = DIMSE_C_MOVE_RSP;
        response.msg.CMoveRSP.MessageIDBeingRespondedTo = request.msg.CMoveRQ.MessageID;
        OFStandard::strlcpy(response.msg.CMoveRSP.AffectedSOPClassUID, 
                          request.msg.CMoveRQ.AffectedSOPClassUID, 
                          sizeof(response.msg.CMoveRSP.AffectedSOPClassUID));
        response.msg.CMoveRSP.DimseStatus = STATUS_MOVE_Failed_UnableToProcess;
        response.msg.CMoveRSP.DataSetType = DIMSE_DATASET_NULL;
        response.msg.CMoveRSP.NumberOfRemainingSubOperations = matchingFiles.size();
        response.msg.CMoveRSP.NumberOfCompletedSubOperations = 0;
        response.msg.CMoveRSP.NumberOfFailedSubOperations = matchingFiles.size();
        response.msg.CMoveRSP.NumberOfWarningSubOperations = 0;
        DIMSE_sendMessage(assoc, presID, &response, nullptr, nullptr);
        return;
    }
    
    // Add presentation contexts for all storage SOP classes
    const char* transferSyntaxes[] = { UID_LittleEndianExplicitTransferSyntax, 
                                     UID_BigEndianExplicitTransferSyntax, 
                                     UID_LittleEndianImplicitTransferSyntax };
    int numTransferSyntaxes = 3;
    
    for (int i = 0; i < numberOfDcmStorageSOPClassUIDs; i++) {
        cond = ASC_addPresentationContext(params, (i*2)+1, dcmStorageSOPClassUIDs[i],
                                        transferSyntaxes, numTransferSyntaxes);
        if (cond.bad()) {
            break;
        }
    }
    
    if (cond.bad()) {
        ASC_dropNetwork(&net);
        ASC_destroyAssociationParameters(&params);
        
        // Send error response
        T_DIMSE_Message response;
        memset(&response, 0, sizeof(response));
        response.CommandField = DIMSE_C_MOVE_RSP;
        response.msg.CMoveRSP.MessageIDBeingRespondedTo = request.msg.CMoveRQ.MessageID;
        OFStandard::strlcpy(response.msg.CMoveRSP.AffectedSOPClassUID, 
                          request.msg.CMoveRQ.AffectedSOPClassUID, 
                          sizeof(response.msg.CMoveRSP.AffectedSOPClassUID));
        response.msg.CMoveRSP.DimseStatus = STATUS_MOVE_Failed_UnableToProcess;
        response.msg.CMoveRSP.DataSetType = DIMSE_DATASET_NULL;
        response.msg.CMoveRSP.NumberOfRemainingSubOperations = matchingFiles.size();
        response.msg.CMoveRSP.NumberOfCompletedSubOperations = 0;
        response.msg.CMoveRSP.NumberOfFailedSubOperations = matchingFiles.size();
        response.msg.CMoveRSP.NumberOfWarningSubOperations = 0;
        DIMSE_sendMessage(assoc, presID, &response, nullptr, nullptr);
        return;
    }
    
    // Request association
    cond = ASC_requestAssociation(net, params, &subAssoc);
    if (cond.bad() || !subAssoc) {
        ASC_dropNetwork(&net);
        ASC_destroyAssociationParameters(&params);
        
        // Send error response
        T_DIMSE_Message response;
        memset(&response, 0, sizeof(response));
        response.CommandField = DIMSE_C_MOVE_RSP;
        response.msg.CMoveRSP.MessageIDBeingRespondedTo = request.msg.CMoveRQ.MessageID;
        OFStandard::strlcpy(response.msg.CMoveRSP.AffectedSOPClassUID, 
                          request.msg.CMoveRQ.AffectedSOPClassUID, 
                          sizeof(response.msg.CMoveRSP.AffectedSOPClassUID));
        response.msg.CMoveRSP.DimseStatus = STATUS_MOVE_Failed_UnableToProcess;
        response.msg.CMoveRSP.DataSetType = DIMSE_DATASET_NULL;
        response.msg.CMoveRSP.NumberOfRemainingSubOperations = matchingFiles.size();
        response.msg.CMoveRSP.NumberOfCompletedSubOperations = 0;
        response.msg.CMoveRSP.NumberOfFailedSubOperations = matchingFiles.size();
        response.msg.CMoveRSP.NumberOfWarningSubOperations = 0;
        DIMSE_sendMessage(assoc, presID, &response, nullptr, nullptr);
        return;
    }
    
    // Initialize counters
    int remaining = matchingFiles.size();
    int completed = 0;
    int failed = 0;
    int warning = 0;
    
    // Send progress response
    T_DIMSE_Message progressResponse;
    memset(&progressResponse, 0, sizeof(progressResponse));
    progressResponse.CommandField = DIMSE_C_MOVE_RSP;
    progressResponse.msg.CMoveRSP.MessageIDBeingRespondedTo = request.msg.CMoveRQ.MessageID;
    OFStandard::strlcpy(progressResponse.msg.CMoveRSP.AffectedSOPClassUID, 
                      request.msg.CMoveRQ.AffectedSOPClassUID, 
                      sizeof(progressResponse.msg.CMoveRSP.AffectedSOPClassUID));
    progressResponse.msg.CMoveRSP.DimseStatus = STATUS_Pending;
    progressResponse.msg.CMoveRSP.DataSetType = DIMSE_DATASET_NULL;
    progressResponse.msg.CMoveRSP.NumberOfRemainingSubOperations = remaining;
    progressResponse.msg.CMoveRSP.NumberOfCompletedSubOperations = completed;
    progressResponse.msg.CMoveRSP.NumberOfFailedSubOperations = failed;
    progressResponse.msg.CMoveRSP.NumberOfWarningSubOperations = warning;
    DIMSE_sendMessage(assoc, presID, &progressResponse, nullptr, nullptr);
    
    // Create the move result
    core::interfaces::query_retrieve::MoveResult moveResult;
    moveResult.completed = 0;
    moveResult.remaining = remaining;
    moveResult.failed = 0;
    moveResult.warning = 0;
    moveResult.success = true;
    moveResult.message = "Move operation in progress";
    
    // Invoke move callback if set
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        if (moveCallback_) {
            try {
                moveCallback_(moveResult);
            }
            catch (const std::exception& ex) {
                write_error("Error in move callback: %s", ex.what());
            }
        }
    }
    
    // Send each file via C-STORE
    for (const auto& filename : matchingFiles) {
        remaining--;
        
        // Load DICOM file
        DcmFileFormat fileFormat;
        OFCondition cond = fileFormat.loadFile(filename.c_str());
        
        if (cond.bad()) {
            failed++;
            continue;
        }
        
        DcmDataset* storeDataset = fileFormat.getDataset();
        
        // Extract SOP Class UID and SOP Instance UID
        OFString sopClassUIDStr, sopInstanceUIDStr;
        if (storeDataset->findAndGetOFString(DCM_SOPClassUID, sopClassUIDStr).bad() ||
            storeDataset->findAndGetOFString(DCM_SOPInstanceUID, sopInstanceUIDStr).bad()) {
            failed++;
            continue;
        }
        
        // Find presentation context for this SOP class
        T_ASC_PresentationContextID storePresID = 0;
        for (int i = 0; i < ASC_countAcceptedPresentationContexts(subAssoc->params); i++) {
            T_ASC_PresentationContext pc;
            ASC_getAcceptedPresentationContext(subAssoc->params, i, &pc);
            if (strcmp(pc.abstractSyntax, sopClassUIDStr.c_str()) == 0) {
                storePresID = pc.presentationContextID;
                break;
            }
        }
        
        if (storePresID == 0) {
            failed++;
            continue;
        }
        
        // Create C-STORE request
        T_DIMSE_Message storeRequest;
        T_DIMSE_Message storeResponse;
        
        memset(&storeRequest, 0, sizeof(storeRequest));
        memset(&storeResponse, 0, sizeof(storeResponse));
        
        storeRequest.CommandField = DIMSE_C_STORE_RQ;
        
        // Set up C-STORE parameters
        T_DIMSE_C_StoreRQ& storeReq = storeRequest.msg.CStoreRQ;
        storeReq.MessageID = subAssoc->nextMsgID++;
        
        OFStandard::strlcpy(storeReq.AffectedSOPClassUID, sopClassUIDStr.c_str(), 
                          sizeof(storeReq.AffectedSOPClassUID));
        OFStandard::strlcpy(storeReq.AffectedSOPInstanceUID, sopInstanceUIDStr.c_str(), 
                          sizeof(storeReq.AffectedSOPInstanceUID));
        
        storeReq.DataSetType = DIMSE_DATASET_PRESENT;
        storeReq.Priority = DIMSE_PRIORITY_MEDIUM;
        
        // Send C-STORE request
        cond = DIMSE_sendMessageUsingMemoryData(subAssoc, storePresID, &storeRequest, nullptr, 
                                              storeDataset, nullptr, nullptr, nullptr);
        
        if (cond.bad()) {
            failed++;
            continue;
        }
        
        // Receive C-STORE response
        cond = DIMSE_receiveCommand(subAssoc, DIMSE_BLOCKING, 0, &storePresID, 
                                  &storeResponse, nullptr);
        
        if (cond.bad() || storeResponse.CommandField != DIMSE_C_STORE_RSP) {
            failed++;
            continue;
        }
        
        // Check status
        if (storeResponse.msg.CStoreRSP.DimseStatus == STATUS_Success) {
            completed++;
            
            // Invoke retrieve callback if set
            {
                std::lock_guard<std::mutex> lock(callbackMutex_);
                if (retrieveCallback_) {
                    try {
                        retrieveCallback_(sopInstanceUIDStr.c_str(), storeDataset);
                    }
                    catch (const std::exception& ex) {
                        write_error("Error in retrieve callback: %s", ex.what());
                    }
                }
            }
        } else if (storeResponse.msg.CStoreRSP.DimseStatus == STATUS_STORE_Warning_CoercionOfDataElements) {
            warning++;
            completed++;
        } else {
            failed++;
        }
        
        // Send progress response every 10 files or if this is the last file
        if ((completed + failed + warning) % 10 == 0 || remaining == 0) {
            memset(&progressResponse, 0, sizeof(progressResponse));
            progressResponse.CommandField = DIMSE_C_MOVE_RSP;
            progressResponse.msg.CMoveRSP.MessageIDBeingRespondedTo = request.msg.CMoveRQ.MessageID;
            OFStandard::strlcpy(progressResponse.msg.CMoveRSP.AffectedSOPClassUID, 
                              request.msg.CMoveRQ.AffectedSOPClassUID, 
                              sizeof(progressResponse.msg.CMoveRSP.AffectedSOPClassUID));
            progressResponse.msg.CMoveRSP.DimseStatus = STATUS_Pending;
            progressResponse.msg.CMoveRSP.DataSetType = DIMSE_DATASET_NULL;
            progressResponse.msg.CMoveRSP.NumberOfRemainingSubOperations = remaining;
            progressResponse.msg.CMoveRSP.NumberOfCompletedSubOperations = completed;
            progressResponse.msg.CMoveRSP.NumberOfFailedSubOperations = failed;
            progressResponse.msg.CMoveRSP.NumberOfWarningSubOperations = warning;
            DIMSE_sendMessage(assoc, presID, &progressResponse, nullptr, nullptr);
            
            // Update the move result
            moveResult.completed = completed;
            moveResult.remaining = remaining;
            moveResult.failed = failed;
            moveResult.warning = warning;
            
            // Invoke move callback if set
            {
                std::lock_guard<std::mutex> lock(callbackMutex_);
                if (moveCallback_) {
                    try {
                        moveCallback_(moveResult);
                    }
                    catch (const std::exception& ex) {
                        write_error("Error in move callback: %s", ex.what());
                    }
                }
            }
        }
    }
    
    // Release association with destination
    ASC_releaseAssociation(subAssoc);
    ASC_dropAssociation(subAssoc);
    ASC_dropNetwork(&net);
    
    // Send final response
    T_DIMSE_Message finalResponse;
    memset(&finalResponse, 0, sizeof(finalResponse));
    finalResponse.CommandField = DIMSE_C_MOVE_RSP;
    finalResponse.msg.CMoveRSP.MessageIDBeingRespondedTo = request.msg.CMoveRQ.MessageID;
    OFStandard::strlcpy(finalResponse.msg.CMoveRSP.AffectedSOPClassUID, 
                      request.msg.CMoveRQ.AffectedSOPClassUID, 
                      sizeof(finalResponse.msg.CMoveRSP.AffectedSOPClassUID));
    
    // Determine status based on results
    if (failed == 0 && warning == 0) {
        finalResponse.msg.CMoveRSP.DimseStatus = STATUS_Success;
        moveResult.success = true;
        moveResult.message = "Move operation completed successfully";
    } else if (completed > 0) {
        finalResponse.msg.CMoveRSP.DimseStatus = STATUS_MOVE_Warning_SubOperationsCompleteOneOrMoreFailures;
        moveResult.success = true;
        moveResult.message = "Move operation completed with some failures";
    } else {
        finalResponse.msg.CMoveRSP.DimseStatus = STATUS_MOVE_Failed_UnableToProcess;
        moveResult.success = false;
        moveResult.message = "Move operation failed";
    }
    
    finalResponse.msg.CMoveRSP.DataSetType = DIMSE_DATASET_NULL;
    finalResponse.msg.CMoveRSP.NumberOfRemainingSubOperations = 0;
    finalResponse.msg.CMoveRSP.NumberOfCompletedSubOperations = completed;
    finalResponse.msg.CMoveRSP.NumberOfFailedSubOperations = failed;
    finalResponse.msg.CMoveRSP.NumberOfWarningSubOperations = warning;
    DIMSE_sendMessage(assoc, presID, &finalResponse, nullptr, nullptr);
    
    // Update the move result
    moveResult.completed = completed;
    moveResult.remaining = 0;
    moveResult.failed = failed;
    moveResult.warning = warning;
    
    // Invoke move callback if set
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        if (moveCallback_) {
            try {
                moveCallback_(moveResult);
            }
            catch (const std::exception& ex) {
                write_error("Error in move callback: %s", ex.what());
            }
        }
    }
}

void QueryRetrieveSCP::handleCGetRequest(T_ASC_Association* assoc, T_DIMSE_Message& request, 
                                  T_ASC_PresentationContextID presID, DcmDataset* dataset) {
    // Similar to handleCMoveRequest, but without creating a sub-association
    // Instead, it sends C-STORE requests directly on the same association
    
    if (!dataset) {
        // Error - no dataset
        T_DIMSE_Message response;
        memset(&response, 0, sizeof(response));
        response.CommandField = DIMSE_C_GET_RSP;
        response.msg.CGetRSP.MessageIDBeingRespondedTo = request.msg.CGetRQ.MessageID;
        OFStandard::strlcpy(response.msg.CGetRSP.AffectedSOPClassUID, 
                          request.msg.CGetRQ.AffectedSOPClassUID, 
                          sizeof(response.msg.CGetRSP.AffectedSOPClassUID));
        response.msg.CGetRSP.DimseStatus = STATUS_GET_Failed_UnableToProcess;
        response.msg.CGetRSP.DataSetType = DIMSE_DATASET_NULL;
        response.msg.CGetRSP.NumberOfRemainingSubOperations = 0;
        response.msg.CGetRSP.NumberOfCompletedSubOperations = 0;
        response.msg.CGetRSP.NumberOfFailedSubOperations = 0;
        response.msg.CGetRSP.NumberOfWarningSubOperations = 0;
        DIMSE_sendMessage(assoc, presID, &response, nullptr, nullptr);
        return;
    }
    
    // Determine query level
    std::string queryLevel = "STUDY";
    DcmElement* levelElement = nullptr;
    if (dataset->findAndGetElement(DCM_QueryRetrieveLevel, levelElement).good() && levelElement) {
        OFString levelStr;
        levelElement->getOFString(levelStr, 0);
        queryLevel = levelStr.c_str();
    }
    
    // Extract UIDs from the dataset
    std::string studyInstanceUID;
    std::string seriesInstanceUID;
    std::string sopInstanceUID;
    
    DcmElement* element = nullptr;
    OFString valueStr;
    
    if (dataset->findAndGetElement(DCM_StudyInstanceUID, element).good() && element) {
        element->getOFString(valueStr, 0);
        studyInstanceUID = valueStr.c_str();
    }
    
    if (dataset->findAndGetElement(DCM_SeriesInstanceUID, element).good() && element) {
        element->getOFString(valueStr, 0);
        seriesInstanceUID = valueStr.c_str();
    }
    
    if (dataset->findAndGetElement(DCM_SOPInstanceUID, element).good() && element) {
        element->getOFString(valueStr, 0);
        sopInstanceUID = valueStr.c_str();
    }
    
    // Find matching files
    std::vector<std::string> matchingFiles;
    {
        std::lock_guard<std::mutex> lock(indexMutex_);
        
        for (const auto& [filename, metadata] : dicomIndex_) {
            bool matches = true;
            
            // Match based on UIDs
            if (!studyInstanceUID.empty() && metadata.find("StudyInstanceUID") != metadata.end()) {
                matches = matches && (metadata.at("StudyInstanceUID") == studyInstanceUID);
            }
            
            if (!seriesInstanceUID.empty() && metadata.find("SeriesInstanceUID") != metadata.end()) {
                matches = matches && (metadata.at("SeriesInstanceUID") == seriesInstanceUID);
            }
            
            if (!sopInstanceUID.empty() && metadata.find("SOPInstanceUID") != metadata.end()) {
                matches = matches && (metadata.at("SOPInstanceUID") == sopInstanceUID);
            }
            
            if (matches) {
                matchingFiles.push_back(filename);
            }
        }
    }
    
    // Initialize counters
    int remaining = matchingFiles.size();
    int completed = 0;
    int failed = 0;
    int warning = 0;
    
    // Send progress response
    T_DIMSE_Message progressResponse;
    memset(&progressResponse, 0, sizeof(progressResponse));
    progressResponse.CommandField = DIMSE_C_GET_RSP;
    progressResponse.msg.CGetRSP.MessageIDBeingRespondedTo = request.msg.CGetRQ.MessageID;
    OFStandard::strlcpy(progressResponse.msg.CGetRSP.AffectedSOPClassUID, 
                      request.msg.CGetRQ.AffectedSOPClassUID, 
                      sizeof(progressResponse.msg.CGetRSP.AffectedSOPClassUID));
    progressResponse.msg.CGetRSP.DimseStatus = STATUS_Pending;
    progressResponse.msg.CGetRSP.DataSetType = DIMSE_DATASET_NULL;
    progressResponse.msg.CGetRSP.NumberOfRemainingSubOperations = remaining;
    progressResponse.msg.CGetRSP.NumberOfCompletedSubOperations = completed;
    progressResponse.msg.CGetRSP.NumberOfFailedSubOperations = failed;
    progressResponse.msg.CGetRSP.NumberOfWarningSubOperations = warning;
    DIMSE_sendMessage(assoc, presID, &progressResponse, nullptr, nullptr);
    
    // Send each file via C-STORE
    for (const auto& filename : matchingFiles) {
        remaining--;
        
        // Load DICOM file
        DcmFileFormat fileFormat;
        OFCondition cond = fileFormat.loadFile(filename.c_str());
        
        if (cond.bad()) {
            failed++;
            continue;
        }
        
        DcmDataset* storeDataset = fileFormat.getDataset();
        
        // Extract SOP Class UID and SOP Instance UID
        OFString sopClassUIDStr, sopInstanceUIDStr;
        if (storeDataset->findAndGetOFString(DCM_SOPClassUID, sopClassUIDStr).bad() ||
            storeDataset->findAndGetOFString(DCM_SOPInstanceUID, sopInstanceUIDStr).bad()) {
            failed++;
            continue;
        }
        
        // Find presentation context for this SOP class
        T_ASC_PresentationContextID storePresID = 0;
        for (int i = 0; i < ASC_countAcceptedPresentationContexts(assoc->params); i++) {
            T_ASC_PresentationContext pc;
            ASC_getAcceptedPresentationContext(assoc->params, i, &pc);
            if (strcmp(pc.abstractSyntax, sopClassUIDStr.c_str()) == 0) {
                storePresID = pc.presentationContextID;
                break;
            }
        }
        
        if (storePresID == 0) {
            failed++;
            continue;
        }
        
        // Create C-STORE request
        T_DIMSE_Message storeRequest;
        T_DIMSE_Message storeResponse;
        
        memset(&storeRequest, 0, sizeof(storeRequest));
        memset(&storeResponse, 0, sizeof(storeResponse));
        
        storeRequest.CommandField = DIMSE_C_STORE_RQ;
        
        // Set up C-STORE parameters
        T_DIMSE_C_StoreRQ& storeReq = storeRequest.msg.CStoreRQ;
        storeReq.MessageID = assoc->nextMsgID++;
        
        OFStandard::strlcpy(storeReq.AffectedSOPClassUID, sopClassUIDStr.c_str(), 
                          sizeof(storeReq.AffectedSOPClassUID));
        OFStandard::strlcpy(storeReq.AffectedSOPInstanceUID, sopInstanceUIDStr.c_str(), 
                          sizeof(storeReq.AffectedSOPInstanceUID));
        
        storeReq.DataSetType = DIMSE_DATASET_PRESENT;
        storeReq.Priority = DIMSE_PRIORITY_MEDIUM;
        
        // Send C-STORE request
        cond = DIMSE_sendMessageUsingMemoryData(assoc, storePresID, &storeRequest, nullptr, 
                                              storeDataset, nullptr, nullptr, nullptr);
        
        if (cond.bad()) {
            failed++;
            continue;
        }
        
        // Receive C-STORE response
        cond = DIMSE_receiveCommand(assoc, DIMSE_BLOCKING, 0, &storePresID, 
                                  &storeResponse, nullptr);
        
        if (cond.bad() || storeResponse.CommandField != DIMSE_C_STORE_RSP) {
            failed++;
            continue;
        }
        
        // Check status
        if (storeResponse.msg.CStoreRSP.DimseStatus == STATUS_Success) {
            completed++;
            
            // Invoke retrieve callback if set
            {
                std::lock_guard<std::mutex> lock(callbackMutex_);
                if (retrieveCallback_) {
                    try {
                        retrieveCallback_(sopInstanceUIDStr.c_str(), storeDataset);
                    }
                    catch (const std::exception& ex) {
                        write_error("Error in retrieve callback: %s", ex.what());
                    }
                }
            }
        } else if (storeResponse.msg.CStoreRSP.DimseStatus == STATUS_STORE_Warning_CoercionOfDataElements) {
            warning++;
            completed++;
        } else {
            failed++;
        }
        
        // Send progress response every 10 files or if this is the last file
        if ((completed + failed + warning) % 10 == 0 || remaining == 0) {
            memset(&progressResponse, 0, sizeof(progressResponse));
            progressResponse.CommandField = DIMSE_C_GET_RSP;
            progressResponse.msg.CGetRSP.MessageIDBeingRespondedTo = request.msg.CGetRQ.MessageID;
            OFStandard::strlcpy(progressResponse.msg.CGetRSP.AffectedSOPClassUID, 
                              request.msg.CGetRQ.AffectedSOPClassUID, 
                              sizeof(progressResponse.msg.CGetRSP.AffectedSOPClassUID));
            progressResponse.msg.CGetRSP.DimseStatus = STATUS_Pending;
            progressResponse.msg.CGetRSP.DataSetType = DIMSE_DATASET_NULL;
            progressResponse.msg.CGetRSP.NumberOfRemainingSubOperations = remaining;
            progressResponse.msg.CGetRSP.NumberOfCompletedSubOperations = completed;
            progressResponse.msg.CGetRSP.NumberOfFailedSubOperations = failed;
            progressResponse.msg.CGetRSP.NumberOfWarningSubOperations = warning;
            DIMSE_sendMessage(assoc, presID, &progressResponse, nullptr, nullptr);
        }
    }
    
    // Send final response
    T_DIMSE_Message finalResponse;
    memset(&finalResponse, 0, sizeof(finalResponse));
    finalResponse.CommandField = DIMSE_C_GET_RSP;
    finalResponse.msg.CGetRSP.MessageIDBeingRespondedTo = request.msg.CGetRQ.MessageID;
    OFStandard::strlcpy(finalResponse.msg.CGetRSP.AffectedSOPClassUID, 
                      request.msg.CGetRQ.AffectedSOPClassUID, 
                      sizeof(finalResponse.msg.CGetRSP.AffectedSOPClassUID));
    
    // Determine status based on results
    if (failed == 0 && warning == 0) {
        finalResponse.msg.CGetRSP.DimseStatus = STATUS_Success;
    } else if (completed > 0) {
        finalResponse.msg.CGetRSP.DimseStatus = STATUS_GET_Warning_SubOperationsCompleteOneOrMoreFailures;
    } else {
        finalResponse.msg.CGetRSP.DimseStatus = STATUS_GET_Failed_UnableToProcess;
    }
    
    finalResponse.msg.CGetRSP.DataSetType = DIMSE_DATASET_NULL;
    finalResponse.msg.CGetRSP.NumberOfRemainingSubOperations = 0;
    finalResponse.msg.CGetRSP.NumberOfCompletedSubOperations = completed;
    finalResponse.msg.CGetRSP.NumberOfFailedSubOperations = failed;
    finalResponse.msg.CGetRSP.NumberOfWarningSubOperations = warning;
    DIMSE_sendMessage(assoc, presID, &finalResponse, nullptr, nullptr);
}

bool QueryRetrieveSCP::matchDataset(const DcmDataset* searchDataset, const DcmDataset* candidateDataset) {
    if (!searchDataset || !candidateDataset) {
        return false;
    }
    
    // Iterate through all elements in the search dataset
    for (unsigned long i = 0; i < searchDataset->card(); i++) {
        DcmElement* searchElement = searchDataset->getElement(i);
        if (!searchElement) continue;
        
        // Skip group length elements
        if (searchElement->getTag().getElement() == 0) continue;
        
        // Skip empty elements
        if (searchElement->getLength() == 0) continue;
        
        // Skip QueryRetrieveLevel element (not part of matching)
        if (searchElement->getTag() == DCM_QueryRetrieveLevel) continue;
        
        // Get the corresponding element from the candidate dataset
        DcmElement* candidateElement = nullptr;
        if (candidateDataset->findAndGetElement(searchElement->getTag(), candidateElement).bad() || 
            !candidateElement) {
            // Element not found in candidate dataset
            return false;
        }
        
        // Compare elements
        OFString searchValue, candidateValue;
        searchElement->getOFString(searchValue, 0);
        candidateElement->getOFString(candidateValue, 0);
        
        // Skip empty search values (universal matching)
        if (searchValue.empty()) continue;
        
        // Handle wildcard matching for string types
        if (searchElement->isaString() && candidateElement->isaString()) {
            // Simple wildcard handling - "*" matches anything
            if (searchValue == "*") continue;
            
            // Handle trailing wildcard
            if (searchValue.length() > 1 && searchValue[searchValue.length()-1] == '*') {
                OFString prefix = searchValue.substr(0, searchValue.length()-1);
                if (candidateValue.length() < prefix.length() || 
                    candidateValue.substr(0, prefix.length()) != prefix) {
                    return false;
                }
                continue;
            }
        }
        
        // Exact matching for non-string types or without wildcards
        if (searchValue != candidateValue) {
            return false;
        }
    }
    
    // All elements matched
    return true;
}

void pacs::query_retrieve::scp::QueryRetrieveSCP::indexStorageDirectory() {
    if (storageDirectory_.empty()) return;
    
    // Clear existing index
    {
        std::lock_guard<std::mutex> lock(indexMutex_);
        dicomIndex_.clear();
    }
    
    try {
        for (const auto& entry : fs::directory_iterator(storageDirectory_)) {
            if (entry.is_regular_file() && entry.path().extension() == ".dcm") {
                std::map<std::string, std::string> metadata;
                
                // Extract metadata using DCMTK
                DcmFileFormat fileFormat;
                if (fileFormat.loadFile(entry.path().string().c_str()).good()) {
                    metadata = extractMetadata(fileFormat.getDataset());
                }
                
                // Add to index
                std::lock_guard<std::mutex> lock(indexMutex_);
                dicomIndex_[entry.path().string()] = metadata;
            }
        }
    }
    catch (const std::exception& ex) {
        write_error("Error indexing storage directory: %s", ex.what());
    }
}

std::map<std::string, std::string> pacs::query_retrieve::scp::QueryRetrieveSCP::extractMetadata(const DcmDataset* dataset) {
    std::map<std::string, std::string> metadata;
    
    if (!dataset) return metadata;
    
    // Extract common DICOM elements
    static const DcmTagKey tags[] = {
        DCM_PatientID,
        DCM_PatientName,
        DCM_StudyInstanceUID,
        DCM_StudyDescription,
        DCM_StudyDate,
        DCM_StudyTime,
        DCM_SeriesInstanceUID,
        DCM_SeriesDescription,
        DCM_SeriesNumber,
        DCM_Modality,
        DCM_SOPInstanceUID,
        DCM_SOPClassUID,
        DCM_InstanceNumber
    };
    
    // The names corresponding to the tags above
    static const char* names[] = {
        "PatientID",
        "PatientName",
        "StudyInstanceUID",
        "StudyDescription",
        "StudyDate",
        "StudyTime",
        "SeriesInstanceUID",
        "SeriesDescription",
        "SeriesNumber",
        "Modality",
        "SOPInstanceUID",
        "SOPClassUID",
        "InstanceNumber"
    };
    
    // Extract value for each tag
    for (size_t i = 0; i < sizeof(tags)/sizeof(tags[0]); i++) {
        DcmElement* element = nullptr;
        if (dataset->findAndGetElement(tags[i], element).good() && element) {
            OFString valueStr;
            element->getOFString(valueStr, 0);
            metadata[names[i]] = valueStr.c_str();
        }
    }
    
    return metadata;
}

} // namespace scp
} // namespace query_retrieve
} // namespace pacs