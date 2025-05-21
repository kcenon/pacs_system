#include "query_retrieve_scu.h"

#include <string>
#include <filesystem>

#ifndef DCMTK_NOT_AVAILABLE
// DCMTK includes
#include "dcmtk/dcmnet/diutil.h"
#include "dcmtk/dcmnet/scu.h"
#include "dcmtk/dcmdata/dcfilefo.h"
#include "dcmtk/dcmdata/dcuid.h"
#endif

#include "thread_system/sources/logger/logger.h"
#include "common/dicom_util.h"

namespace pacs {
namespace query_retrieve {
namespace scu {

namespace fs = std::filesystem;
using namespace log_module;

QueryRetrieveSCU::QueryRetrieveSCU(const common::ServiceConfig& config, const std::string& retrieveDirectory)
    : config_(config), retrieveDirectory_(retrieveDirectory) {
#ifndef DCMTK_NOT_AVAILABLE
    // Configure DCMTK logging
    OFLog::configure(OFLogger::WARN_LOG_LEVEL);
#endif
    
    // Create retrieve directory if it doesn't exist
    if (!retrieveDirectory_.empty() && !fs::exists(retrieveDirectory_)) {
        try {
            fs::create_directories(retrieveDirectory_);
            write_information("Created retrieve directory: %s", retrieveDirectory_.c_str());
        } catch (const std::exception& e) {
            write_error("Failed to create retrieve directory: %s", e.what());
        }
    }
}

QueryRetrieveSCU::~QueryRetrieveSCU() = default;

#ifndef DCMTK_NOT_AVAILABLE
core::Result<std::vector<DcmDataset*>> QueryRetrieveSCU::query(const DcmDataset* searchDataset, 
                                                      core::interfaces::query_retrieve::QueryRetrieveLevel level) {
    if (!searchDataset) {
        return core::Result<std::vector<DcmDataset*>>::error("Search dataset is null");
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<DcmDataset*> result;
    
    try {
        // Get the appropriate information model for the query level
        const char* informationModel = getInformationModelForLevel(level);
        
        // Create association with remote Query/Retrieve SCP
        T_ASC_Association* assoc = createAssociation(informationModel);
        if (!assoc) {
            return core::Result<std::vector<DcmDataset*>>::error("Failed to create association with Query/Retrieve SCP");
        }
        
        // Create C-FIND request message
        T_DIMSE_Message request;
        T_DIMSE_Message response;
        
        memset(&request, 0, sizeof(request));
        memset(&response, 0, sizeof(response));
        
        request.CommandField = DIMSE_C_FIND_RQ;
        
        // Set up C-FIND parameters
        T_DIMSE_C_FindRQ& findReq = request.msg.CFindRQ;
        findReq.MessageID = assoc->nextMsgID++;
        
        OFStandard::strlcpy(findReq.AffectedSOPClassUID, informationModel, 
                          sizeof(findReq.AffectedSOPClassUID));
        
        findReq.Priority = DIMSE_PRIORITY_MEDIUM;
        findReq.DataSetType = DIMSE_DATASET_PRESENT;
        
        // Find the first presentation context for the information model
        T_ASC_PresentationContextID presID = 0;
        for (int i = 0; i < ASC_countAcceptedPresentationContexts(assoc->params); i++) {
            T_ASC_PresentationContext pc;
            ASC_getAcceptedPresentationContext(assoc->params, i, &pc);
            if (strcmp(pc.abstractSyntax, informationModel) == 0) {
                presID = pc.presentationContextID;
                break;
            }
        }
        
        if (presID == 0) {
            releaseAssociation(assoc);
            return core::Result<std::vector<DcmDataset*>>::error("No presentation context for information model");
        }
        
        // Create a copy of the search dataset and add QueryRetrieveLevel attribute if not present
        DcmDataset searchDatasetCopy(*searchDataset);
        DcmElement* element = nullptr;
        if (searchDatasetCopy.findAndGetElement(DCM_QueryRetrieveLevel, element).bad() || !element) {
            // Add QueryRetrieveLevel
            std::string levelStr = getQueryLevelString(level);
            searchDatasetCopy.putAndInsertString(DCM_QueryRetrieveLevel, levelStr.c_str());
        }
        
        // Send C-FIND request
        OFCondition cond = DIMSE_sendMessageUsingMemoryData(assoc, presID, &request, nullptr, 
                                                         &searchDatasetCopy, nullptr, nullptr, nullptr);
        
        if (cond.bad()) {
            releaseAssociation(assoc);
            return core::Result<std::vector<DcmDataset*>>::error(std::string("Failed to send C-FIND request: ") + 
                                                              cond.text());
        }
        
        // Receive C-FIND responses
        bool done = false;
        while (!done) {
            DcmDataset* responseDataset = nullptr;
            cond = DIMSE_receiveCommand(assoc, DIMSE_BLOCKING, 0, &presID, &response, &responseDataset);
            
            if (cond.bad()) {
                // Clean up any datasets we've received so far
                for (auto* dataset : result) {
                    delete dataset;
                }
                result.clear();
                
                releaseAssociation(assoc);
                return core::Result<std::vector<DcmDataset*>>::error(std::string("Failed to receive C-FIND response: ") + 
                                                                  cond.text());
            }
            
            // Check if this is a C-FIND response
            if (response.CommandField != DIMSE_C_FIND_RSP) {
                // Clean up any datasets we've received so far and this dataset
                delete responseDataset;
                for (auto* dataset : result) {
                    delete dataset;
                }
                result.clear();
                
                releaseAssociation(assoc);
                return core::Result<std::vector<DcmDataset*>>::error("Unexpected response command");
            }
            
            // Check status
            T_DIMSE_C_FindRSP& findRsp = response.msg.CFindRSP;
            
            if (findRsp.DimseStatus == STATUS_Success) {
                // This is the final response, indicating the end of the matches
                done = true;
            } else if (findRsp.DimseStatus == STATUS_Pending) {
                // This is a pending response with matching dataset
                if (responseDataset) {
                    // Invoke callback if set
                    if (queryCallback_) {
                        try {
                            core::interfaces::query_retrieve::QueryResultItem item = 
                                parseQueryResult(responseDataset, level);
                            queryCallback_(item, responseDataset);
                        }
                        catch (const std::exception& ex) {
                            write_error("Error in query callback: %s", ex.what());
                        }
                    }
                    
                    result.push_back(responseDataset);
                    responseDataset = nullptr; // Ownership transferred to result vector
                }
            } else {
                // Error status
                // Clean up any datasets we've received so far and this dataset
                delete responseDataset;
                for (auto* dataset : result) {
                    delete dataset;
                }
                result.clear();
                
                releaseAssociation(assoc);
                return core::Result<std::vector<DcmDataset*>>::error(std::string("C-FIND failed with status: ") + 
                                                                  DU_cfindStatusString(findRsp.DimseStatus));
            }
            
            // Clean up dataset if not added to result
            delete responseDataset;
        }
        
        // Release association
        releaseAssociation(assoc);
        
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
#else
core::Result<std::vector<DcmDataset*>> QueryRetrieveSCU::query(const DcmDataset* /*searchDataset*/, 
                                                      core::interfaces::query_retrieve::QueryRetrieveLevel /*level*/) {
    // Placeholder implementation when DCMTK is not available
    return core::Result<std::vector<DcmDataset*>>::error("Query operation not available without DCMTK");
}
#endif

#ifndef DCMTK_NOT_AVAILABLE
core::Result<void> QueryRetrieveSCU::retrieve(const std::string& studyInstanceUID,
                                     const std::string& seriesInstanceUID,
                                     const std::string& sopInstanceUID) {
    if (studyInstanceUID.empty()) {
        return core::Result<void>::error("Study Instance UID is required");
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // Determine query retrieve level
        std::string queryRetrieveLevel;
        if (!sopInstanceUID.empty()) {
            queryRetrieveLevel = "IMAGE";
        } else if (!seriesInstanceUID.empty()) {
            queryRetrieveLevel = "SERIES";
        } else {
            queryRetrieveLevel = "STUDY";
        }
        
        // Execute C-MOVE operation
        return executeCMove(studyInstanceUID, seriesInstanceUID, sopInstanceUID, queryRetrieveLevel);
    }
    catch (const std::exception& ex) {
        return core::Result<void>::error(std::string("Exception during retrieve: ") + ex.what());
    }
}
#else
core::Result<void> QueryRetrieveSCU::retrieve(const std::string& /*studyInstanceUID*/,
                                     const std::string& /*seriesInstanceUID*/,
                                     const std::string& /*sopInstanceUID*/) {
    // Placeholder implementation when DCMTK is not available
    return core::Result<void>::error("Retrieve operation not available without DCMTK");
}
#endif

void QueryRetrieveSCU::setQueryCallback(core::interfaces::query_retrieve::QueryCallback callback) {
    queryCallback_ = callback;
}

void QueryRetrieveSCU::setRetrieveCallback(core::interfaces::query_retrieve::RetrieveCallback callback) {
    retrieveCallback_ = callback;
}

void QueryRetrieveSCU::setMoveCallback(core::interfaces::query_retrieve::MoveCallback callback) {
    moveCallback_ = callback;
}

void QueryRetrieveSCU::setRetrieveDirectory(const std::string& directory) {
    retrieveDirectory_ = directory;
    
    // Create directory if it doesn't exist
    if (!retrieveDirectory_.empty() && !fs::exists(retrieveDirectory_)) {
        try {
            fs::create_directories(retrieveDirectory_);
            write_information("Created retrieve directory: %s", retrieveDirectory_.c_str());
        } catch (const std::exception& e) {
            write_error("Failed to create retrieve directory: %s", e.what());
        }
    }
}

#ifndef DCMTK_NOT_AVAILABLE
T_ASC_Association* QueryRetrieveSCU::createAssociation(const char* queryRetrieveModel) {
    T_ASC_Network* net = nullptr;
    T_ASC_Parameters* params = nullptr;
    T_ASC_Association* assoc = nullptr;
    
    OFCondition cond;
    
    // Initialize network
    cond = ASC_initializeNetwork(NET_REQUESTOR, 0, 0, &net);
    if (cond.bad()) {
        return nullptr;
    }
    
    // Create association parameters
    cond = ASC_createAssociationParameters(&params, ASC_MAXIMUMPDUSIZE);
    if (cond.bad()) {
        ASC_dropNetwork(&net);
        return nullptr;
    }
    
    // Set calling and called AE titles
    ASC_setAPTitles(params, config_.aeTitle.c_str(), config_.peerAETitle.c_str(), nullptr);
    
    // Set network addresses
    ASC_setTransportLayerType(params, OFFalse);
    cond = ASC_setPresentationAddresses(params, OFStandard::getHostName().c_str(), 
                                      (config_.peerHost + ":" + std::to_string(config_.peerPort)).c_str());
    if (cond.bad()) {
        ASC_dropNetwork(&net);
        ASC_destroyAssociationParameters(&params);
        return nullptr;
    }
    
    // Add presentation contexts for Query/Retrieve
    const char* transferSyntaxes[] = { UID_LittleEndianExplicitTransferSyntax, 
                                     UID_LittleEndianImplicitTransferSyntax };
    int numTransferSyntaxes = 2;
    
    // Add requested Query/Retrieve Information Model
    cond = ASC_addPresentationContext(params, 1, queryRetrieveModel,
                                    transferSyntaxes, numTransferSyntaxes);
    
    // Also add the corresponding MOVE Information Model if this is a FIND model
    if (strcmp(queryRetrieveModel, UID_FINDPatientRootQueryRetrieveInformationModel) == 0) {
        cond = ASC_addPresentationContext(params, 3, UID_MOVEPatientRootQueryRetrieveInformationModel,
                                        transferSyntaxes, numTransferSyntaxes);
    } else if (strcmp(queryRetrieveModel, UID_FINDStudyRootQueryRetrieveInformationModel) == 0) {
        cond = ASC_addPresentationContext(params, 3, UID_MOVEStudyRootQueryRetrieveInformationModel,
                                        transferSyntaxes, numTransferSyntaxes);
    }
    
    if (cond.bad()) {
        ASC_dropNetwork(&net);
        ASC_destroyAssociationParameters(&params);
        return nullptr;
    }
    
    // Request association
    cond = ASC_requestAssociation(net, params, &assoc);
    if (cond.bad() || !assoc) {
        ASC_dropNetwork(&net);
        ASC_destroyAssociationParameters(&params);
        return nullptr;
    }
    
    // Check if the remote AE accepted our request
    if (ASC_countAcceptedPresentationContexts(params) == 0) {
        ASC_releaseAssociation(assoc);
        ASC_dropAssociation(assoc);
        ASC_dropNetwork(&net);
        ASC_destroyAssociationParameters(&params);
        return nullptr;
    }
    
    return assoc;
}

void QueryRetrieveSCU::releaseAssociation(T_ASC_Association* assoc) {
    if (assoc) {
        ASC_releaseAssociation(assoc);
        ASC_dropAssociation(assoc);
        ASC_dropNetwork(&assoc->net);
    }
}

core::Result<void> QueryRetrieveSCU::executeCMove(const std::string& studyInstanceUID,
                                        const std::string& seriesInstanceUID,
                                        const std::string& sopInstanceUID,
                                        const std::string& queryRetrieveLevel) {
    // Determine the appropriate information model for the query retrieve level
    const char* informationModel = UID_MOVEStudyRootQueryRetrieveInformationModel;
    
    // Create association with remote Query/Retrieve SCP
    T_ASC_Association* assoc = createAssociation(informationModel);
    if (!assoc) {
        return core::Result<void>::error("Failed to create association with Query/Retrieve SCP");
    }
    
    // Create C-MOVE request message
    T_DIMSE_Message request;
    T_DIMSE_Message response;
    
    memset(&request, 0, sizeof(request));
    memset(&response, 0, sizeof(response));
    
    request.CommandField = DIMSE_C_MOVE_RQ;
    
    // Set up C-MOVE parameters
    T_DIMSE_C_MoveRQ& moveReq = request.msg.CMoveRQ;
    moveReq.MessageID = assoc->nextMsgID++;
    
    OFStandard::strlcpy(moveReq.AffectedSOPClassUID, informationModel, 
                      sizeof(moveReq.AffectedSOPClassUID));
    
    moveReq.Priority = DIMSE_PRIORITY_MEDIUM;
    moveReq.DataSetType = DIMSE_DATASET_PRESENT;
    
    // Set move destination
    OFStandard::strlcpy(moveReq.MoveDestination, config_.aeTitle.c_str(), sizeof(moveReq.MoveDestination));
    
    // Find the first presentation context for the information model
    T_ASC_PresentationContextID presID = 0;
    for (int i = 0; i < ASC_countAcceptedPresentationContexts(assoc->params); i++) {
        T_ASC_PresentationContext pc;
        ASC_getAcceptedPresentationContext(assoc->params, i, &pc);
        if (strcmp(pc.abstractSyntax, informationModel) == 0) {
            presID = pc.presentationContextID;
            break;
        }
    }
    
    if (presID == 0) {
        releaseAssociation(assoc);
        return core::Result<void>::error("No presentation context for information model");
    }
    
    // Create the query dataset
    DcmDataset moveDataset;
    
    // Add QueryRetrieveLevel
    moveDataset.putAndInsertString(DCM_QueryRetrieveLevel, queryRetrieveLevel.c_str());
    
    // Add the UIDs at the appropriate levels
    moveDataset.putAndInsertString(DCM_StudyInstanceUID, studyInstanceUID.c_str());
    
    if (!seriesInstanceUID.empty()) {
        moveDataset.putAndInsertString(DCM_SeriesInstanceUID, seriesInstanceUID.c_str());
    }
    
    if (!sopInstanceUID.empty()) {
        moveDataset.putAndInsertString(DCM_SOPInstanceUID, sopInstanceUID.c_str());
    }
    
    // Send C-MOVE request
    OFCondition cond = DIMSE_sendMessageUsingMemoryData(assoc, presID, &request, nullptr, 
                                                     &moveDataset, nullptr, nullptr, nullptr);
    
    if (cond.bad()) {
        releaseAssociation(assoc);
        return core::Result<void>::error(std::string("Failed to send C-MOVE request: ") + cond.text());
    }
    
    // Receive C-MOVE responses
    bool done = false;
    core::interfaces::query_retrieve::MoveResult moveResult = {0, 0, 0, 0, true, ""};
    
    while (!done) {
        DcmDataset* responseDataset = nullptr;
        cond = DIMSE_receiveCommand(assoc, DIMSE_BLOCKING, 0, &presID, &response, &responseDataset);
        
        if (cond.bad()) {
            delete responseDataset;
            releaseAssociation(assoc);
            return core::Result<void>::error(std::string("Failed to receive C-MOVE response: ") + cond.text());
        }
        
        // Check if this is a C-MOVE response
        if (response.CommandField != DIMSE_C_MOVE_RSP) {
            delete responseDataset;
            releaseAssociation(assoc);
            return core::Result<void>::error("Unexpected response command");
        }
        
        // Check status
        T_DIMSE_C_MoveRSP& moveRsp = response.msg.CMoveRSP;
        
        if (moveRsp.DimseStatus == STATUS_Success) {
            // Final response
            moveResult.completed = moveRsp.NumberOfCompletedSubOperations;
            moveResult.remaining = moveRsp.NumberOfRemainingSubOperations;
            moveResult.failed = moveRsp.NumberOfFailedSubOperations;
            moveResult.warning = moveRsp.NumberOfWarningSubOperations;
            moveResult.success = true;
            moveResult.message = "Move operation completed successfully";
            done = true;
        } else if (moveRsp.DimseStatus == STATUS_Pending) {
            // Progress update
            moveResult.completed = moveRsp.NumberOfCompletedSubOperations;
            moveResult.remaining = moveRsp.NumberOfRemainingSubOperations;
            moveResult.failed = moveRsp.NumberOfFailedSubOperations;
            moveResult.warning = moveRsp.NumberOfWarningSubOperations;
            
            // Invoke move callback if set
            if (moveCallback_) {
                try {
                    moveCallback_(moveResult);
                }
                catch (const std::exception& ex) {
                    write_error("Error in move callback: %s", ex.what());
                }
            }
        } else {
            // Error status
            moveResult.completed = moveRsp.NumberOfCompletedSubOperations;
            moveResult.remaining = moveRsp.NumberOfRemainingSubOperations;
            moveResult.failed = moveRsp.NumberOfFailedSubOperations;
            moveResult.warning = moveRsp.NumberOfWarningSubOperations;
            moveResult.success = false;
            moveResult.message = std::string("C-MOVE failed with status: ") + 
                                DU_cmoveStatusString(moveRsp.DimseStatus);
            
            // Invoke move callback if set
            if (moveCallback_) {
                try {
                    moveCallback_(moveResult);
                }
                catch (const std::exception& ex) {
                    write_error("Error in move callback: %s", ex.what());
                }
            }
            
            delete responseDataset;
            releaseAssociation(assoc);
            return core::Result<void>::error(moveResult.message);
        }
        
        // Clean up dataset
        delete responseDataset;
    }
    
    // Invoke final move callback if set
    if (moveCallback_) {
        try {
            moveCallback_(moveResult);
        }
        catch (const std::exception& ex) {
            write_error("Error in move callback: %s", ex.what());
        }
    }
    
    // Release association
    releaseAssociation(assoc);
    
    return core::Result<void>::ok();
}

const char* QueryRetrieveSCU::getInformationModelForLevel(core::interfaces::query_retrieve::QueryRetrieveLevel level) {
    // Using Study Root Information Model for all levels
    return UID_FINDStudyRootQueryRetrieveInformationModel;
}
#endif

std::string QueryRetrieveSCU::getQueryLevelString(core::interfaces::query_retrieve::QueryRetrieveLevel level) {
    switch (level) {
        case core::interfaces::query_retrieve::QueryRetrieveLevel::PATIENT:
            return "PATIENT";
        case core::interfaces::query_retrieve::QueryRetrieveLevel::STUDY:
            return "STUDY";
        case core::interfaces::query_retrieve::QueryRetrieveLevel::SERIES:
            return "SERIES";
        case core::interfaces::query_retrieve::QueryRetrieveLevel::IMAGE:
            return "IMAGE";
        default:
            return "STUDY";
    }
}

#ifndef DCMTK_NOT_AVAILABLE
core::interfaces::query_retrieve::QueryResultItem QueryRetrieveSCU::parseQueryResult(
    const DcmDataset* dataset, 
    core::interfaces::query_retrieve::QueryRetrieveLevel level) {
    
    core::interfaces::query_retrieve::QueryResultItem item;
    item.level = level;
    
    if (!dataset) return item;
    
    // Extract values from dataset
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
    
    return item;
}
#else
// Simplified implementation when DCMTK is not available
core::interfaces::query_retrieve::QueryResultItem QueryRetrieveSCU::parseQueryResult(
    const DcmDataset* /*dataset*/, 
    core::interfaces::query_retrieve::QueryRetrieveLevel level) {
    
    core::interfaces::query_retrieve::QueryResultItem item;
    item.level = level;
    
    // Cannot extract actual values without DCMTK
    return item;
}
#endif

} // namespace scu
} // namespace query_retrieve
} // namespace pacs