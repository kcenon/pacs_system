#include "worklist_scu.h"

#include <string>
#include <fstream>


// DCMTK includes
#include "dcmtk/dcmnet/diutil.h"
#include "dcmtk/dcmnet/scu.h"
#include "dcmtk/dcmwlm/wlds.h"
#include "dcmtk/dcmdata/dcfilefo.h"
#include "dcmtk/dcmdata/dcuid.h"


#include "common/dicom_util.h"
#include "thread_system/sources/logger/logger.h"

using namespace log_module;

namespace pacs {
namespace worklist {
namespace scu {

WorklistSCU::WorklistSCU(const common::ServiceConfig& config)
    : config_(config) {

    // Configure DCMTK logging
    OFLog::configure(OFLogger::WARN_LOG_LEVEL);

}

WorklistSCU::~WorklistSCU() = default;

core::Result<std::vector<DcmDataset*>> WorklistSCU::findWorklist(const DcmDataset* searchDataset) {
    if (!searchDataset) {
        return core::Result<std::vector<DcmDataset*>>::error("Search dataset is null");
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<DcmDataset*> result;
    

    try {
        // Create association with remote Worklist SCP
        T_ASC_Association* assoc = createAssociation();
        if (!assoc) {
            return core::Result<std::vector<DcmDataset*>>::error("Failed to create association with Worklist SCP");
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
        
        // Use Modality Worklist Information Model
        OFStandard::strlcpy(findReq.AffectedSOPClassUID, UID_FINDModalityWorklistInformationModel, 
                          sizeof(findReq.AffectedSOPClassUID));
        
        findReq.Priority = DIMSE_PRIORITY_MEDIUM;
        findReq.DataSetType = DIMSE_DATASET_PRESENT;
        
        // Find the first presentation context for Modality Worklist
        T_ASC_PresentationContextID presID = 0;
        
        // Find presentation context for Modality Worklist
        for (int i = 0; i < ASC_countAcceptedPresentationContexts(assoc->params); i++) {
            T_ASC_PresentationContext pc;
            ASC_getAcceptedPresentationContext(assoc->params, i, &pc);
            if (strcmp(pc.abstractSyntax, UID_FINDModalityWorklistInformationModel) == 0) {
                presID = pc.presentationContextID;
                break;
            }
        }
        
        if (presID == 0) {
            releaseAssociation(assoc);
            return core::Result<std::vector<DcmDataset*>>::error("No presentation context for Modality Worklist");
        }
        
        // Send C-FIND request - using DIMSE_sendMessage instead of DIMSE_sendMessageUsingMemoryData
        DcmDataset* nonConstDataset = const_cast<DcmDataset*>(searchDataset);
        OFCondition cond = DIMSE_sendMessage(assoc, presID, &request, nonConstDataset);
        
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
        
        return core::Result<std::vector<DcmDataset*>>::error(std::string("Exception during worklist find: ") + ex.what());
    }

}

core::Result<void> WorklistSCU::addWorklistItem(const DcmDataset* dataset) {
    // Not used in SCU role
    return core::Result<void>::error("addWorklistItem not implemented for SCU role");
}

core::Result<void> WorklistSCU::updateWorklistItem(const std::string& accessionNumber, const DcmDataset* dataset) {
    // Not used in SCU role
    return core::Result<void>::error("updateWorklistItem not implemented for SCU role");
}

core::Result<void> WorklistSCU::removeWorklistItem(const std::string& accessionNumber) {
    // Not used in SCU role
    return core::Result<void>::error("removeWorklistItem not implemented for SCU role");
}

void WorklistSCU::setWorklistCallback(core::interfaces::worklist::WorklistCallback callback) {
    // Not used in SCU role
}

T_ASC_Association* WorklistSCU::createAssociation() {

    T_ASC_Network* net = nullptr;
    T_ASC_Parameters* params = nullptr;
    T_ASC_Association* assoc = nullptr;
    
    OFCondition cond;
    
    // Initialize network
    cond = ASC_initializeNetwork(NET_REQUESTOR, 0, 0, &net);
    if (cond.bad()) {
        return nullptr;
    }
    
    // Create association parameters - using newer API when available
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
    
    // Add presentation contexts for Modality Worklist
    const char* transferSyntaxes[] = { UID_LittleEndianExplicitTransferSyntax, 
                                     UID_LittleEndianImplicitTransferSyntax };
    int numTransferSyntaxes = 2;
    
    // Add Modality Worklist SOP class
    cond = ASC_addPresentationContext(params, 1, UID_FINDModalityWorklistInformationModel,
                                    transferSyntaxes, numTransferSyntaxes);
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

void WorklistSCU::releaseAssociation(T_ASC_Association* assoc) {
    if (assoc) {
        ASC_releaseAssociation(assoc);
        ASC_dropAssociation(assoc);
        ASC_dropNetwork(&assoc->net);
    }
}

} // namespace scu
} // namespace worklist
} // namespace pacs