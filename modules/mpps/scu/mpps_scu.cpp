#include "mpps_scu.h"

#include <iostream>
#include <string>


// DCMTK includes
#include "dcmtk/dcmnet/diutil.h"
#include "dcmtk/dcmnet/scu.h"
#include "dcmtk/dcmdata/dcuid.h"
#include "dcmtk/dcmdata/dcdeftag.h"
#include "dcmtk/dcmnet/dimse.h"
#include "dcmtk/dcmnet/dicom.h"
#include "dcmtk/dcmnet/assoc.h"
#include "dcmtk/dcmnet/cond.h"


#include "common/dicom_util.h"
#include "common/logger/logger.h"

using namespace pacs::common::logger;

namespace pacs {
namespace mpps {
namespace scu {

MPPSSCU::MPPSSCU(const common::ServiceConfig& config)
    : config_(config) {

    // Configure DCMTK logging
    OFLog::configure(OFLogger::WARN_LOG_LEVEL);

}

MPPSSCU::~MPPSSCU() = default;

core::Result<void> MPPSSCU::createMPPS(const DcmDataset* dataset) {

    if (!dataset) {
        return core::Result<void>::error("MPPS dataset is null");
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // Create association with remote MPPS SCP
        T_ASC_Association* assoc = createAssociation();
        if (!assoc) {
            return core::Result<void>::error("Failed to create association with MPPS SCP");
        }
        
        // Prepare N-CREATE request
        T_DIMSE_Message request;
        T_DIMSE_Message response;
        
        memset(&request, 0, sizeof(request));
        memset(&response, 0, sizeof(response));
        
        request.CommandField = DIMSE_N_CREATE_RQ;
        
        // Set up N-CREATE parameters
        T_DIMSE_N_CreateRQ& n_create = request.msg.NCreateRQ;
        
        // Extract SOP Class UID and SOP Instance UID from dataset
        std::string sopClassUID;
        std::string sopInstanceUID;
        
        OFCondition cond;
        
        // Get SOP Class UID using constant values instead of macros
        const DcmTagKey sopClassUIDTag(0x0008, 0x0016); // DCM_SOPClassUID
        const DcmTagKey sopInstanceUIDTag(0x0008, 0x0018); // DCM_SOPInstanceUID
        
        // Create a non-const copy for API compatibility
        DcmDataset dataset_copy(*dataset);
        
        // Get SOP Class UID
        DcmElement* element = nullptr;
        cond = dataset_copy.findAndGetElement(sopClassUIDTag, element);
        if (cond.good() && element) {
            OFString uidStr;
            element->getOFString(uidStr, 0);
            sopClassUID = uidStr.c_str();
        }
        
        // Get SOP Instance UID
        element = nullptr;
        cond = dataset_copy.findAndGetElement(sopInstanceUIDTag, element);
        if (cond.good() && element) {
            OFString uidStr;
            element->getOFString(uidStr, 0);
            sopInstanceUID = uidStr.c_str();
        }
        
        if (sopClassUID.empty() || sopInstanceUID.empty()) {
            releaseAssociation(assoc);
            return core::Result<void>::error("Missing SOP Class UID or SOP Instance UID in MPPS dataset");
        }
        
        OFStandard::strlcpy(n_create.AffectedSOPClassUID, sopClassUID.c_str(), sizeof(n_create.AffectedSOPClassUID));
        OFStandard::strlcpy(n_create.AffectedSOPInstanceUID, sopInstanceUID.c_str(), sizeof(n_create.AffectedSOPInstanceUID));
        
        n_create.DataSetType = DIMSE_DATASET_PRESENT;
        
        // Find appropriate presentation context ID
        T_ASC_PresentationContextID presID = ASC_findAcceptedPresentationContextID(assoc, sopClassUID.c_str());
        if (presID == 0) {
            releaseAssociation(assoc);
            return core::Result<void>::error("No accepted presentation context for MPPS SOP class");
        }
        
        // Send N-CREATE request
        cond = DIMSE_sendMessageUsingMemoryData(assoc, presID, &request, nullptr, const_cast<DcmDataset*>(dataset), nullptr, nullptr, nullptr);
        
        if (cond.bad()) {
            releaseAssociation(assoc);
            return core::Result<void>::error(std::string("Failed to send N-CREATE request: ") + cond.text());
        }
        
        // Receive N-CREATE response
        DcmDataset* responseDataset = nullptr;
        cond = DIMSE_receiveCommand(assoc, DIMSE_BLOCKING, 0, &presID, &response, &responseDataset);
        
        if (cond.bad()) {
            releaseAssociation(assoc);
            return core::Result<void>::error(std::string("Failed to receive N-CREATE response: ") + cond.text());
        }
        
        // Check status
        if (response.CommandField != DIMSE_N_CREATE_RSP) {
            releaseAssociation(assoc);
            delete responseDataset;
            return core::Result<void>::error("Unexpected response command");
        }
        
        T_DIMSE_N_CreateRSP& n_create_rsp = response.msg.NCreateRSP;
        
        if (n_create_rsp.DimseStatus != STATUS_Success) {
            releaseAssociation(assoc);
            delete responseDataset;
            return core::Result<void>::error(std::string("N-CREATE failed with status: ") + 
                                          std::to_string(n_create_rsp.DimseStatus));
        }
        
        // Cleanup
        delete responseDataset;
        releaseAssociation(assoc);
        
        return core::Result<void>::ok();
    }
    catch (const std::exception& ex) {
        return core::Result<void>::error(std::string("Exception during MPPS N-CREATE: ") + ex.what());
    }

}

core::Result<void> MPPSSCU::updateMPPS(const std::string& sopInstanceUID, const DcmDataset* dataset) {

    if (!dataset) {
        return core::Result<void>::error("MPPS dataset is null");
    }
    
    if (sopInstanceUID.empty()) {
        return core::Result<void>::error("SOP Instance UID is empty");
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // Create association with remote MPPS SCP
        T_ASC_Association* assoc = createAssociation();
        if (!assoc) {
            return core::Result<void>::error("Failed to create association with MPPS SCP");
        }
        
        // Prepare N-SET request
        T_DIMSE_Message request;
        T_DIMSE_Message response;
        
        memset(&request, 0, sizeof(request));
        memset(&response, 0, sizeof(response));
        
        request.CommandField = DIMSE_N_SET_RQ;
        
        // Set up N-SET parameters
        T_DIMSE_N_SetRQ& n_set = request.msg.NSetRQ;
        
        // Get SOP Class UID from dataset
        std::string sopClassUID;
        
        OFCondition cond;
        
        // Get SOP Class UID using constant values instead of macros
        const DcmTagKey sopClassUIDTag(0x0008, 0x0016); // DCM_SOPClassUID
        
        // Create a non-const copy for API compatibility
        DcmDataset dataset_copy(*dataset);
        
        // Get SOP Class UID
        DcmElement* element = nullptr;
        cond = dataset_copy.findAndGetElement(sopClassUIDTag, element);
        if (cond.good() && element) {
            OFString uidStr;
            element->getOFString(uidStr, 0);
            sopClassUID = uidStr.c_str();
        }
        
        if (sopClassUID.empty()) {
            releaseAssociation(assoc);
            return core::Result<void>::error("Missing SOP Class UID in MPPS dataset");
        }
        
        OFStandard::strlcpy(n_set.RequestedSOPClassUID, sopClassUID.c_str(), sizeof(n_set.RequestedSOPClassUID));
        OFStandard::strlcpy(n_set.RequestedSOPInstanceUID, sopInstanceUID.c_str(), sizeof(n_set.RequestedSOPInstanceUID));
        
        n_set.DataSetType = DIMSE_DATASET_PRESENT;
        
        // Find appropriate presentation context ID
        T_ASC_PresentationContextID presID = ASC_findAcceptedPresentationContextID(assoc, sopClassUID.c_str());
        if (presID == 0) {
            releaseAssociation(assoc);
            return core::Result<void>::error("No accepted presentation context for MPPS SOP class");
        }
        
        // Send N-SET request
        cond = DIMSE_sendMessageUsingMemoryData(assoc, presID, &request, nullptr, const_cast<DcmDataset*>(dataset), nullptr, nullptr, nullptr);
        
        if (cond.bad()) {
            releaseAssociation(assoc);
            return core::Result<void>::error(std::string("Failed to send N-SET request: ") + cond.text());
        }
        
        // Receive N-SET response
        DcmDataset* responseDataset = nullptr;
        cond = DIMSE_receiveCommand(assoc, DIMSE_BLOCKING, 0, &presID, &response, &responseDataset);
        
        if (cond.bad()) {
            releaseAssociation(assoc);
            return core::Result<void>::error(std::string("Failed to receive N-SET response: ") + cond.text());
        }
        
        // Check status
        if (response.CommandField != DIMSE_N_SET_RSP) {
            releaseAssociation(assoc);
            delete responseDataset;
            return core::Result<void>::error("Unexpected response command");
        }
        
        T_DIMSE_N_SetRSP& n_set_rsp = response.msg.NSetRSP;
        
        if (n_set_rsp.DimseStatus != STATUS_Success) {
            releaseAssociation(assoc);
            delete responseDataset;
            return core::Result<void>::error(std::string("N-SET failed with status: ") + 
                                          std::to_string(n_set_rsp.DimseStatus));
        }
        
        // Cleanup
        delete responseDataset;
        releaseAssociation(assoc);
        
        return core::Result<void>::ok();
    }
    catch (const std::exception& ex) {
        return core::Result<void>::error(std::string("Exception during MPPS N-SET: ") + ex.what());
    }

}

void MPPSSCU::setCreateCallback(core::interfaces::mpps::MPPSCallback callback) {
    // Not used in SCU role
}

void MPPSSCU::setUpdateCallback(core::interfaces::mpps::MPPSCallback callback) {
    // Not used in SCU role
}

T_ASC_Association* MPPSSCU::createAssociation() {

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
    
    // Add presentation contexts for MPPS
    const char* transferSyntaxes[] = { UID_LittleEndianExplicitTransferSyntax, 
                                     UID_LittleEndianImplicitTransferSyntax };
    int numTransferSyntaxes = 2;
    
    // Add MPPS SOP class
    cond = ASC_addPresentationContext(params, 1, UID_ModalityPerformedProcedureStepSOPClass,
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

void MPPSSCU::releaseAssociation(T_ASC_Association* assoc) {

    if (assoc) {
        ASC_releaseAssociation(assoc);
        ASC_dropAssociation(assoc);
        // Network is handled by association cleanup
    }

}

} // namespace scu
} // namespace mpps
} // namespace pacs