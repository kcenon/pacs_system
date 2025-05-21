#include "worklist_scp.h"

#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <thread>

#ifndef DCMTK_NOT_AVAILABLE
// DCMTK includes
#include "dcmtk/dcmnet/diutil.h"
#include "dcmtk/dcmdata/dcfilefo.h"
#include "dcmtk/dcmdata/dcuid.h"
#include "dcmtk/dcmdata/dcitem.h"
#include "dcmtk/dcmdata/dcvrda.h"
#include "dcmtk/dcmdata/dcvrtm.h"
#include "dcmtk/dcmdata/dcvrpn.h"
#include "dcmtk/dcmdata/dcvrlo.h"
#include "dcmtk/dcmdata/dcvrsh.h"
#include "dcmtk/dcmdata/dcvrcs.h"
#include "dcmtk/dcmdata/dcvrui.h"
#endif

#include "common/dicom_util.h"
#include "thread_system/sources/logger/logger.h"

using namespace log_module;

namespace pacs {
namespace worklist {
namespace scp {

namespace fs = std::filesystem;

WorklistSCP::WorklistSCP(const common::ServiceConfig& config, const std::string& worklistDirectory)
    : config_(config), worklistDirectory_(worklistDirectory), running_(false) {
#ifndef DCMTK_NOT_AVAILABLE
    // Configure DCMTK logging
    OFLog::configure(OFLogger::WARN_LOG_LEVEL);
#endif
    
    // Create worklist directory if it doesn't exist
    if (!worklistDirectory_.empty() && !fs::exists(worklistDirectory_)) {
        try {
            fs::create_directories(worklistDirectory_);
        } catch (const std::exception& e) {
            write_error("Failed to create worklist directory: %s", e.what());
        }
    }
    
    // Load existing worklist items
    if (!worklistDirectory_.empty()) {
        loadWorklistItems();
    }
}

WorklistSCP::~WorklistSCP() {
    stop();
    
    // Clean up worklist items - the unique_ptr destructor will handle deletion
    std::lock_guard<std::mutex> lock(worklistMutex_);
    worklistItems_.clear();
}

core::Result<void> WorklistSCP::start() {
    if (running_) {
        return core::Result<void>::error("Worklist SCP is already running");
    }
    
    running_ = true;
    serverThread_ = std::thread(&WorklistSCP::serverLoop, this);
    
    return core::Result<void>::ok();
}

void WorklistSCP::stop() {
    if (running_) {
        running_ = false;
        if (serverThread_.joinable()) {
            serverThread_.join();
        }
    }
}

core::Result<std::vector<DcmDataset*>> WorklistSCP::findWorklist(const DcmDataset* searchDataset) {
    if (!searchDataset) {
        return core::Result<std::vector<DcmDataset*>>::error("Search dataset is null");
    }
    
    std::vector<DcmDataset*> result;
    
#ifndef DCMTK_NOT_AVAILABLE
    // Lock worklist items
    std::lock_guard<std::mutex> lock(worklistMutex_);
    
    // Go through all worklist items and check if they match the search criteria
    for (const auto& pair : worklistItems_) {
        if (matchWorklistItem(searchDataset, pair.second)) {
            // Create a copy of the matching dataset
            result.push_back(new DcmDataset(*pair.second));
        }
    }
#else
    // Placeholder implementation
    write_information("Searching worklist (placeholder implementation)");
    
    // Create a placeholder dataset for demonstration
    // Note: In real implementation with DCMTK, this would be a proper DcmDataset
    // Since we're in the placeholder implementation, this doesn't actually do anything useful
    // but allows the rest of the code to continue functioning
#endif
    
    return core::Result<std::vector<DcmDataset*>>::ok(result);
}

core::Result<void> WorklistSCP::addWorklistItem(const DcmDataset* dataset) {
    if (!dataset) {
        return core::Result<void>::error("Worklist dataset is null");
    }
    
#ifndef DCMTK_NOT_AVAILABLE
    // Extract accession number
    DcmElement* element = nullptr;
    OFString accessionNumberStr;
    DcmDataset* mutableDataset = const_cast<DcmDataset*>(dataset);
    OFCondition cond = mutableDataset->findAndGetElement(DCM_AccessionNumber, element);
    if (cond.good() && element) {
        element->getOFString(accessionNumberStr, 0);
    } else {
        return core::Result<void>::error("Missing accession number in worklist dataset");
    }
    
    std::string accessionNumber = accessionNumberStr.c_str();
    
    // Lock worklist items
    std::lock_guard<std::mutex> lock(worklistMutex_);
    
    // Check if item with this accession number already exists
    if (worklistItems_.find(accessionNumber) != worklistItems_.end()) {
        return core::Result<void>::error("Worklist item with accession number " + accessionNumber + " already exists");
    }
    
    // Add new worklist item
    worklistItems_[accessionNumber] = new DcmDataset(*dataset);
    
    // Save to file if directory is specified
    if (!worklistDirectory_.empty()) {
        try {
            std::string filename = worklistDirectory_ + "/" + accessionNumber + ".wl";
            
            // Create DICOM file format
            DcmFileFormat fileFormat;
            DcmDataset* datasetCopy = new DcmDataset(*dataset);
            fileFormat.clear();  // Clear any existing dataset
            fileFormat.getDataset()->copyFrom(*datasetCopy);  // Copy to the internal dataset
            delete datasetCopy;  // Free the temporary copy
            
            // Save to file
            OFCondition cond = fileFormat.saveFile(filename.c_str(), EXS_LittleEndianExplicit);
            
            if (cond.bad()) {
                return core::Result<void>::error(std::string("Failed to save worklist item to file: ") + cond.text());
            }
        }
        catch (const std::exception& ex) {
            return core::Result<void>::error(std::string("Error saving worklist item: ") + ex.what());
        }
    }
    
    // Invoke callback if set
    {
        std::lock_guard<std::mutex> callbackLock(callbackMutex_);
        if (worklistCallback_) {
            try {
                worklistCallback_(extractWorklistItem(dataset), dataset);
            }
            catch (const std::exception& ex) {
                write_error("Error in worklist callback: %s", ex.what());
            }
        }
    }
    
    return core::Result<void>::ok();
#else
    // Placeholder implementation when DCMTK is not available
    write_information("Adding worklist item (placeholder implementation)");
    
    // Create a placeholder worklist item
    core::interfaces::worklist::WorklistItem placeholderItem = extractWorklistItem(dataset);
    
    // Save to placeholder file if directory is specified
    if (!worklistDirectory_.empty()) {
        try {
            std::string filename = worklistDirectory_ + "/" + placeholderItem.accessionNumber + ".wl";
            
            // Create a simple text file as a placeholder
            std::ofstream outFile(filename);
            if (outFile.is_open()) {
                outFile << "PLACEHOLDER WORKLIST ITEM" << std::endl;
                outFile << "PatientID: " << placeholderItem.patientID << std::endl;
                outFile << "PatientName: " << placeholderItem.patientName << std::endl;
                outFile << "AccessionNumber: " << placeholderItem.accessionNumber << std::endl;
                outFile.close();
            } else {
                return core::Result<void>::error("Failed to create placeholder worklist file");
            }
        }
        catch (const std::exception& ex) {
            return core::Result<void>::error(std::string("Error creating placeholder worklist file: ") + ex.what());
        }
    }
    
    // Invoke callback if set
    {
        std::lock_guard<std::mutex> callbackLock(callbackMutex_);
        if (worklistCallback_) {
            try {
                worklistCallback_(placeholderItem, dataset);
            }
            catch (const std::exception& ex) {
                write_error("Error in worklist callback: %s", ex.what());
            }
        }
    }
    
    return core::Result<void>::ok();
#endif
}

core::Result<void> WorklistSCP::updateWorklistItem(const std::string& accessionNumber, const DcmDataset* dataset) {
    if (!dataset) {
        return core::Result<void>::error("Worklist dataset is null");
    }
    
    if (accessionNumber.empty()) {
        return core::Result<void>::error("Accession number is empty");
    }
    
#ifndef DCMTK_NOT_AVAILABLE
    // Lock worklist items
    std::lock_guard<std::mutex> lock(worklistMutex_);
    
    // Check if item with this accession number exists
    auto it = worklistItems_.find(accessionNumber);
    if (it == worklistItems_.end()) {
        return core::Result<void>::error("Worklist item with accession number " + accessionNumber + " does not exist");
    }
    
    // Update worklist item
    delete it->second;
    it->second = new DcmDataset(*dataset);
    
    // Save to file if directory is specified
    if (!worklistDirectory_.empty()) {
        try {
            std::string filename = worklistDirectory_ + "/" + accessionNumber + ".wl";
            
            // Create DICOM file format
            DcmFileFormat fileFormat;
            DcmDataset* datasetCopy = new DcmDataset(*dataset);
            fileFormat.clear();  // Clear any existing dataset
            fileFormat.getDataset()->copyFrom(*datasetCopy);  // Copy to the internal dataset
            delete datasetCopy;  // Free the temporary copy
            
            // Save to file
            OFCondition cond = fileFormat.saveFile(filename.c_str(), EXS_LittleEndianExplicit);
            
            if (cond.bad()) {
                return core::Result<void>::error(std::string("Failed to save worklist item to file: ") + cond.text());
            }
        }
        catch (const std::exception& ex) {
            return core::Result<void>::error(std::string("Error saving worklist item: ") + ex.what());
        }
    }
    
    // Invoke callback if set
    {
        std::lock_guard<std::mutex> callbackLock(callbackMutex_);
        if (worklistCallback_) {
            try {
                worklistCallback_(extractWorklistItem(dataset), dataset);
            }
            catch (const std::exception& ex) {
                write_error("Error in worklist callback: %s", ex.what());
            }
        }
    }
    
    return core::Result<void>::ok();
#else
    // Placeholder implementation when DCMTK is not available
    write_information("Updating worklist item with accession number %s (placeholder implementation)", accessionNumber.c_str());
    
    // Create a placeholder worklist item
    core::interfaces::worklist::WorklistItem placeholderItem = extractWorklistItem(dataset);
    
    // Save to placeholder file if directory is specified
    if (!worklistDirectory_.empty()) {
        try {
            std::string filename = worklistDirectory_ + "/" + accessionNumber + ".wl";
            
            // Create a simple text file as a placeholder
            std::ofstream outFile(filename);
            if (outFile.is_open()) {
                outFile << "PLACEHOLDER WORKLIST ITEM (UPDATED)" << std::endl;
                outFile << "PatientID: " << placeholderItem.patientID << std::endl;
                outFile << "PatientName: " << placeholderItem.patientName << std::endl;
                outFile << "AccessionNumber: " << accessionNumber << std::endl;
                outFile.close();
            } else {
                return core::Result<void>::error("Failed to update placeholder worklist file");
            }
        }
        catch (const std::exception& ex) {
            return core::Result<void>::error(std::string("Error updating placeholder worklist file: ") + ex.what());
        }
    }
    
    // Invoke callback if set
    {
        std::lock_guard<std::mutex> callbackLock(callbackMutex_);
        if (worklistCallback_) {
            try {
                worklistCallback_(placeholderItem, dataset);
            }
            catch (const std::exception& ex) {
                write_error("Error in worklist callback: %s", ex.what());
            }
        }
    }
    
    return core::Result<void>::ok();
#endif
}

core::Result<void> WorklistSCP::removeWorklistItem(const std::string& accessionNumber) {
    if (accessionNumber.empty()) {
        return core::Result<void>::error("Accession number is empty");
    }
    
    // Lock worklist items
    std::lock_guard<std::mutex> lock(worklistMutex_);
    
#ifndef DCMTK_NOT_AVAILABLE
    // Check if item with this accession number exists
    auto it = worklistItems_.find(accessionNumber);
    if (it == worklistItems_.end()) {
        return core::Result<void>::error("Worklist item with accession number " + accessionNumber + " does not exist");
    }
    
    // Remove worklist item
    delete it->second;
    worklistItems_.erase(it);
#endif
    
    // Delete file if directory is specified
    if (!worklistDirectory_.empty()) {
        try {
            std::string filename = worklistDirectory_ + "/" + accessionNumber + ".wl";
            if (fs::exists(filename)) {
                fs::remove(filename);
            } else {
#ifndef DCMTK_NOT_AVAILABLE
                return core::Result<void>::error("Worklist file not found: " + filename);
#else
                write_information("Worklist file not found: %s (placeholder implementation - ignoring)", filename.c_str());
#endif
            }
        }
        catch (const std::exception& ex) {
            return core::Result<void>::error(std::string("Error deleting worklist item file: ") + ex.what());
        }
    }
    
#ifdef DCMTK_NOT_AVAILABLE
    write_information("Removing worklist item with accession number %s (placeholder implementation)", accessionNumber.c_str());
#endif
    
    return core::Result<void>::ok();
}

void WorklistSCP::setWorklistCallback(core::interfaces::worklist::WorklistCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    worklistCallback_ = callback;
}

void WorklistSCP::setWorklistDirectory(const std::string& directory) {
    worklistDirectory_ = directory;
    
    // Create directory if it doesn't exist
    if (!worklistDirectory_.empty() && !fs::exists(worklistDirectory_)) {
        try {
            fs::create_directories(worklistDirectory_);
        } catch (const std::exception& e) {
            write_error("Failed to create worklist directory: %s", e.what());
        }
    }
    
    // Load worklist items from the new directory
    loadWorklistItems();
}

void WorklistSCP::serverLoop() {
#ifndef DCMTK_NOT_AVAILABLE
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
            if (cond != DUL_ASSOCIATIONREJECTED) { // Changed from DUL_ASSOCIATIONABORTED
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
#else
    // Placeholder implementation
    write_information("Worklist SCP started on port %d (placeholder)", config_.localPort);
    
    // Just keep the thread alive
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    write_information("Worklist SCP stopped (placeholder)");
#endif
}

void WorklistSCP::processAssociation(T_ASC_Association* assoc) {
#ifndef DCMTK_NOT_AVAILABLE
    if (!assoc) {
        return;
    }
    
    // Accept association
    const char* transferSyntaxes[] = { UID_LittleEndianExplicitTransferSyntax, 
                                    UID_BigEndianExplicitTransferSyntax, 
                                    UID_LittleEndianImplicitTransferSyntax };
    int numTransferSyntaxes = 3;
    
    // Accept presentation contexts for Modality Worklist
    for (int i = 0; i < ASC_countPresentationContexts(assoc->params); i++) {
        T_ASC_PresentationContext pc;
        ASC_getPresentationContext(assoc->params, i, &pc);
        
        // Accept if this is the Modality Worklist Information Model
        if (strcmp(pc.abstractSyntax, UID_FINDModalityWorklistInformationModel) == 0) {
            // Use correct parameters for newer DCMTK API
            ASC_acceptPresentationContext(assoc->params, pc.presentationContextID,
                                        transferSyntaxes[0]);
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
                    write_information("C-ECHO response: %s", dumpStr.c_str());
                    
                    // Send the message
                    OFCondition echoSendCond = DIMSE_sendResponseMessage(assoc, presID, &request, &response, nullptr);
                    if (echoSendCond.bad()) {
                        write_error("Failed to send C-ECHO response: %s", echoSendCond.text());
                    }
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
#endif
}

void WorklistSCP::handleCFindRequest(T_ASC_Association* assoc, T_DIMSE_Message& request, 
                                   T_ASC_PresentationContextID presID, DcmDataset* dataset) {
#ifndef DCMTK_NOT_AVAILABLE
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
        
        // Log message
        OFString dumpStr;
        DIMSE_dumpMessage(dumpStr, response, DIMSE_OUTGOING);
        write_information("C-FIND error response: %s", dumpStr.c_str());
        
        // Send the message
        OFCondition sendCond = DIMSE_sendResponseMessage(assoc, presID, &request, &response, nullptr);
        if (sendCond.bad()) {
            write_error("Failed to send C-FIND error response: %s", sendCond.text());
        }
        return;
    }
    
    // Find matching worklist items
    core::Result<std::vector<DcmDataset*>> result = findWorklist(dataset);
    
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
        
        // Log message
        OFString dumpStr;
        DIMSE_dumpMessage(dumpStr, response, DIMSE_OUTGOING);
        write_information("C-FIND error response: %s", dumpStr.c_str());
        
        // Send the message
        OFCondition sendCond = DIMSE_sendResponseMessage(assoc, presID, &request, &response, nullptr);
        if (sendCond.bad()) {
            write_error("Failed to send C-FIND error response: %s", sendCond.text());
        }
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
        
        // Log message
        OFString dumpStr;
        DIMSE_dumpMessage(dumpStr, response, DIMSE_OUTGOING);
        write_information("C-FIND pending response: %s", dumpStr.c_str());
        
        // Send the message
        OFCondition cond = DIMSE_sendResponseMessage(assoc, presID, &request, &response, matchingDataset);
        
        // Clean up dataset
        delete matchingDataset;
        
        if (cond.bad()) {
            // Error sending response, clean up remaining datasets
            for (size_t i = 1; i < result.value().size(); i++) {
                delete result.value()[i];
            }
            
            write_error("Failed to send C-FIND pending response: %s", cond.text());
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
    
    // Log message
    OFString dumpStr;
    DIMSE_dumpMessage(dumpStr, response, DIMSE_OUTGOING);
    write_information("C-FIND success response: %s", dumpStr.c_str());
    
    // Send the message
    OFCondition sendCond = DIMSE_sendResponseMessage(assoc, presID, &request, &response, nullptr);
    if (sendCond.bad()) {
        write_error("Failed to send C-FIND success response: %s", sendCond.text());
    }
#else
    // Placeholder implementation when DCMTK is not available
    write_information("Received C-FIND request (placeholder implementation)");
    
    // Find matching worklist items (placeholder)
    if (dataset != nullptr) {
        write_information("Found 0 matching worklist items (placeholder)");
    }
    
    write_information("Sent C-FIND response (placeholder implementation)");
#endif
}

bool WorklistSCP::matchWorklistItem(const DcmDataset* searchDataset, const DcmDataset* worklistDataset) {
#ifndef DCMTK_NOT_AVAILABLE
    if (!searchDataset || !worklistDataset) {
        return false;
    }
    
    // Go through all elements in the search dataset
    for (unsigned long i = 0; i < searchDataset->card(); i++) {
        DcmElement* searchElement = nullptr;
        // Use const-safe method to get element
        if (DcmObject* obj = searchDataset->getElement(i)) {
            searchElement = dynamic_cast<DcmElement*>(obj);
        }
        
        if (!searchElement) continue;
        
        // Skip group length elements
        if (searchElement->getTag().getElement() == 0) continue;
        
        // Skip empty elements
        if (searchElement->getLength() == 0) continue;
        
        // Get the corresponding element from the worklist dataset
        DcmElement* worklistElement = nullptr;
        // Use const-safe method to find element
        OFCondition cond = const_cast<DcmDataset*>(worklistDataset)->findAndGetElement(searchElement->getTag(), worklistElement);
        if (cond.bad() || !worklistElement) {
            // Element not found in worklist dataset
            return false;
        }
        
        // Compare elements
        OFString searchValue, worklistValue;
        searchElement->getOFString(searchValue, 0);
        worklistElement->getOFString(worklistValue, 0);
        
        // Skip empty search values (universal matching)
        if (searchValue.empty()) continue;
        
        // Handle wildcard matching for string types
        if (searchElement->isaString() && worklistElement->isaString()) {
            // Simple wildcard handling - "*" matches anything
            if (searchValue == "*") continue;
            
            // Handle trailing wildcard
            if (searchValue.length() > 1 && searchValue[searchValue.length()-1] == '*') {
                OFString prefix = searchValue.substr(0, searchValue.length()-1);
                if (worklistValue.substr(0, prefix.length()) != prefix) {
                    return false;
                }
                continue;
            }
        }
        
        // Exact matching for non-string types or without wildcards
        if (searchValue != worklistValue) {
            return false;
        }
    }
    
    // All elements matched
    return true;
#else
    // Placeholder implementation when DCMTK is not available
    return false;
#endif
}

void WorklistSCP::loadWorklistItems() {
    if (worklistDirectory_.empty()) return;
    
    // Clear existing items - the unique_ptr destructor will handle deletion
    {
        std::lock_guard<std::mutex> lock(worklistMutex_);
        worklistItems_.clear();
    }
    
#ifndef DCMTK_NOT_AVAILABLE
    try {
        // Iterate over all files in the directory
        for (const auto& entry : fs::directory_iterator(worklistDirectory_)) {
            if (entry.is_regular_file() && entry.path().extension() == ".wl") {
                // Load DICOM file
                DcmFileFormat fileFormat;
                OFCondition cond = fileFormat.loadFile(entry.path().string().c_str());
                
                if (cond.good()) {
                    DcmDataset* dataset = fileFormat.getDataset();
                    
                    // Extract accession number
                    DcmElement* element = nullptr;
                    OFString accessionNumberStr;
                    if (dataset->findAndGetElement(DCM_AccessionNumber, element).good() && element) {
                        element->getOFString(accessionNumberStr, 0);
                        
                        // Add to worklist items
                        std::lock_guard<std::mutex> lock(worklistMutex_);
                        worklistItems_[accessionNumberStr.c_str()] = new DcmDataset(*dataset);
                    }
                }
            }
        }
    }
    catch (const std::exception& ex) {
        write_error("Error loading worklist items: %s", ex.what());
    }
#else
    // Placeholder implementation
    write_information("Loading worklist items from %s (placeholder implementation)", worklistDirectory_.c_str());
    
    try {
        // Iterate over all files in the directory and just count them
        int count = 0;
        for (const auto& entry : fs::directory_iterator(worklistDirectory_)) {
            if (entry.is_regular_file() && entry.path().extension() == ".wl") {
                count++;
            }
        }
        write_information("Found %d worklist files (placeholder - not actually loaded)", count);
    }
    catch (const std::exception& ex) {
        write_error("Error counting worklist files: %s", ex.what());
    }
#endif
}

core::interfaces::worklist::WorklistItem WorklistSCP::extractWorklistItem(const DcmDataset* dataset) {
    core::interfaces::worklist::WorklistItem item;
    
    if (!dataset) return item;
    
#ifndef DCMTK_NOT_AVAILABLE
    // Extract values from dataset
    DcmElement* element = nullptr;
    OFString valueStr;
    DcmDataset* mutableDataset = const_cast<DcmDataset*>(dataset);
    
    if (mutableDataset->findAndGetElement(DCM_PatientID, element).good() && element) {
        element->getOFString(valueStr, 0);
        item.patientID = valueStr.c_str();
    }
    
    if (mutableDataset->findAndGetElement(DCM_PatientName, element).good() && element) {
        element->getOFString(valueStr, 0);
        item.patientName = valueStr.c_str();
    }
    
    if (mutableDataset->findAndGetElement(DCM_AccessionNumber, element).good() && element) {
        element->getOFString(valueStr, 0);
        item.accessionNumber = valueStr.c_str();
    }
    
    // Navigate to the Scheduled Procedure Step Sequence
    DcmSequenceOfItems* spsSeq = nullptr;
    if (mutableDataset->findAndGetSequence(DCM_ScheduledProcedureStepSequence, spsSeq).good() && spsSeq && spsSeq->card() > 0) {
        DcmItem* spsItem = spsSeq->getItem(0);
        
        if (spsItem->findAndGetElement(DCM_ScheduledProcedureStepStartDate, element).good() && element) {
            element->getOFString(valueStr, 0);
            item.scheduledProcedureStepStartDate = valueStr.c_str();
        }
        
        if (spsItem->findAndGetElement(DCM_ScheduledProcedureStepStartTime, element).good() && element) {
            element->getOFString(valueStr, 0);
            item.scheduledProcedureStepStartTime = valueStr.c_str();
        }
        
        if (spsItem->findAndGetElement(DCM_Modality, element).good() && element) {
            element->getOFString(valueStr, 0);
            item.modality = valueStr.c_str();
        }
        
        if (spsItem->findAndGetElement(DCM_ScheduledStationAETitle, element).good() && element) {
            element->getOFString(valueStr, 0);
            item.scheduledStationAETitle = valueStr.c_str();
        }
        
        if (spsItem->findAndGetElement(DCM_ScheduledProcedureStepDescription, element).good() && element) {
            element->getOFString(valueStr, 0);
            item.scheduledProcedureStepDescription = valueStr.c_str();
        }
    }
#else
    // Placeholder implementation
    item.patientID = "PLACEHOLDER_PATIENT_ID";
    item.patientName = "PLACEHOLDER_PATIENT_NAME";
    item.accessionNumber = "PLACEHOLDER_ACCESSION_NUMBER";
    item.scheduledProcedureStepStartDate = "20250101";
    item.scheduledProcedureStepStartTime = "120000";
    item.modality = "CT";
    item.scheduledStationAETitle = "PLACEHOLDER_AE";
    item.scheduledProcedureStepDescription = "PLACEHOLDER_PROCEDURE";
#endif
    
    return item;
}

} // namespace scp
} // namespace worklist
} // namespace pacs