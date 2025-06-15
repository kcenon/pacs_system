#include "mpps_scp.h"

#include <string>
#include <thread>


// DCMTK includes
#include "dcmtk/dcmnet/diutil.h"
#include "dcmtk/dcmnet/scp.h"
#include "dcmtk/dcmdata/dcuid.h"
#include "dcmtk/dcmdata/dcdeftag.h"
#include "dcmtk/dcmnet/dimse.h"
#include "dcmtk/dcmnet/dicom.h"
#include "dcmtk/dcmnet/dul.h"
#include "dcmtk/dcmnet/assoc.h"
#include "dcmtk/dcmnet/cond.h"


#include "common/dicom_util.h"
#include "common/logger/logger.h"

using namespace pacs::common::logger;

namespace pacs {
namespace mpps {
namespace scp {

using namespace log_module;

MPPSSCP::MPPSSCP(const common::ServiceConfig& config)
    : config_(config), running_(false) {

    // Configure DCMTK logging
    OFLog::configure(OFLogger::WARN_LOG_LEVEL);

}

MPPSSCP::~MPPSSCP() {
    stop();
}

core::Result<void> MPPSSCP::start() {
    if (running_) {
        return core::Result<void>::error("MPPS SCP is already running");
    }
    
    running_ = true;
    serverThread_ = std::thread(&MPPSSCP::serverLoop, this);
    
    return core::Result<void>::ok();
}

void MPPSSCP::stop() {
    if (running_) {
        running_ = false;
        if (serverThread_.joinable()) {
            serverThread_.join();
        }
    }
}

core::Result<void> MPPSSCP::createMPPS(const DcmDataset* dataset) {
    // Not used in SCP role
    return core::Result<void>::error("createMPPS not implemented for SCP role");
}

core::Result<void> MPPSSCP::updateMPPS(const std::string& sopInstanceUID, const DcmDataset* dataset) {
    // Not used in SCP role
    return core::Result<void>::error("updateMPPS not implemented for SCP role");
}

void MPPSSCP::setCreateCallback(core::interfaces::mpps::MPPSCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    createCallback_ = callback;
}

void MPPSSCP::setUpdateCallback(core::interfaces::mpps::MPPSCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    updateCallback_ = callback;
}

void MPPSSCP::serverLoop() {

    T_ASC_Network* net = nullptr;
    T_ASC_Association* assoc = nullptr;
    
    OFCondition cond;
    
    // Initialize network
    cond = ASC_initializeNetwork(NET_ACCEPTOR, config_.localPort, 30, &net);
    if (cond.bad()) {
        logError("Error initializing network: {}", cond.text());
        return;
    }
    
    // Main server loop
    while (running_) {
        // Reset association pointer
        assoc = nullptr;
        
        // Listen for associations
        cond = ASC_receiveAssociation(net, &assoc, ASC_DEFAULTMAXPDU);
        
        if (cond.bad()) {
            if (cond != DUL_ASSOCIATIONREJECTED) {  // Changed to available constant
                logError("Error receiving association: {}", cond.text());
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

void MPPSSCP::processAssociation(T_ASC_Association* assoc) {

    if (!assoc) {
        return;
    }
    
    // Accept association
    const char* transferSyntaxes[] = { UID_LittleEndianExplicitTransferSyntax, 
                                    UID_LittleEndianImplicitTransferSyntax };
    int numTransferSyntaxes = 2;
    
    // Accept presentation contexts for MPPS
    for (int i = 0; i < ASC_countPresentationContexts(assoc->params); i++) {
        T_ASC_PresentationContext pc;
        ASC_getPresentationContext(assoc->params, i, &pc);
        
        if (strcmp(pc.abstractSyntax, UID_ModalityPerformedProcedureStepSOPClass) == 0) {
            // Changed to use proper parameters for newer DCMTK versions
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
            case DIMSE_N_CREATE_RQ:
                handleNCreateRequest(assoc, request, presID, dataset);
                break;
                
            case DIMSE_N_SET_RQ:
                handleNSetRequest(assoc, request, presID, dataset);
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
                    
                    // Send the echo response
                    OFCondition echoSendCond = DIMSE_sendMessageUsingMemoryData(assoc, presID, &response, nullptr, nullptr, nullptr, nullptr, nullptr);
                    if (echoSendCond.bad()) {
                        logError("Failed to send C-ECHO response: {}", echoSendCond.text());
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

}

void MPPSSCP::handleNCreateRequest(T_ASC_Association* assoc, T_DIMSE_Message& request, 
                                 T_ASC_PresentationContextID presID, DcmDataset* dataset) {

    if (!dataset) {
        // Error - no dataset
        T_DIMSE_Message response;
        memset(&response, 0, sizeof(response));
        response.CommandField = DIMSE_N_CREATE_RSP;
        response.msg.NCreateRSP.MessageIDBeingRespondedTo = request.msg.NCreateRQ.MessageID;
        OFStandard::strlcpy(response.msg.NCreateRSP.AffectedSOPClassUID, 
                          request.msg.NCreateRQ.AffectedSOPClassUID, 
                          sizeof(response.msg.NCreateRSP.AffectedSOPClassUID));
        OFStandard::strlcpy(response.msg.NCreateRSP.AffectedSOPInstanceUID, 
                          request.msg.NCreateRQ.AffectedSOPInstanceUID, 
                          sizeof(response.msg.NCreateRSP.AffectedSOPInstanceUID));
        response.msg.NCreateRSP.DimseStatus = STATUS_N_ProcessingFailure;
        response.msg.NCreateRSP.DataSetType = DIMSE_DATASET_NULL;
        
        // Send the response
        OFCondition sendCond = DIMSE_sendMessageUsingMemoryData(assoc, presID, &response, nullptr, nullptr, nullptr, nullptr, nullptr);
        if (sendCond.bad()) {
            logError("Failed to send N-CREATE error response: {}", sendCond.text());
        }
        return;
    }
    
    // Extract accession number and other relevant information
    std::string accessionNumber;
    std::string sopInstanceUID = request.msg.NCreateRQ.AffectedSOPInstanceUID;
    
    // Use DCMTK tag constants safely
    const DcmTagKey accessionNumberTag(0x0008, 0x0050); // DCM_AccessionNumber equivalent
    
    DcmElement* element = nullptr;
    if (dataset->findAndGetElement(accessionNumberTag, element).good() && element) {
        OFString str;
        element->getOFString(str, 0);
        accessionNumber = str.c_str();
    }
    
    // Invoke callback if set
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        if (createCallback_) {
            try {
                createCallback_(accessionNumber, dataset);
            }
            catch (const std::exception& ex) {
                logError("Error in MPPS create callback: {}", ex.what());
            }
        }
    }
    
    // Respond with success
    T_DIMSE_Message response;
    memset(&response, 0, sizeof(response));
    response.CommandField = DIMSE_N_CREATE_RSP;
    response.msg.NCreateRSP.MessageIDBeingRespondedTo = request.msg.NCreateRQ.MessageID;
    OFStandard::strlcpy(response.msg.NCreateRSP.AffectedSOPClassUID, 
                      request.msg.NCreateRQ.AffectedSOPClassUID, 
                      sizeof(response.msg.NCreateRSP.AffectedSOPClassUID));
    OFStandard::strlcpy(response.msg.NCreateRSP.AffectedSOPInstanceUID, 
                      sopInstanceUID.c_str(), 
                      sizeof(response.msg.NCreateRSP.AffectedSOPInstanceUID));
    response.msg.NCreateRSP.DimseStatus = STATUS_Success;
    response.msg.NCreateRSP.DataSetType = DIMSE_DATASET_NULL;
    
    // Send the response
    OFCondition sendCond = DIMSE_sendMessageUsingMemoryData(assoc, presID, &response, nullptr, nullptr, nullptr, nullptr, nullptr);
    if (sendCond.bad()) {
        logError("Failed to send N-CREATE response: {}", sendCond.text());
    }

}

void MPPSSCP::handleNSetRequest(T_ASC_Association* assoc, T_DIMSE_Message& request, 
                              T_ASC_PresentationContextID presID, DcmDataset* dataset) {

    if (!dataset) {
        // Error - no dataset
        T_DIMSE_Message response;
        memset(&response, 0, sizeof(response));
        response.CommandField = DIMSE_N_SET_RSP;
        response.msg.NSetRSP.MessageIDBeingRespondedTo = request.msg.NSetRQ.MessageID;
        
        // Use AffectedSOPClassUID instead of RequestedSOPClassUID which is not available in this version
        OFStandard::strlcpy(response.msg.NSetRSP.AffectedSOPClassUID, 
                          request.msg.NSetRQ.RequestedSOPClassUID, 
                          sizeof(response.msg.NSetRSP.AffectedSOPClassUID));
        
        // Use AffectedSOPInstanceUID instead of RequestedSOPInstanceUID
        OFStandard::strlcpy(response.msg.NSetRSP.AffectedSOPInstanceUID, 
                          request.msg.NSetRQ.RequestedSOPInstanceUID, 
                          sizeof(response.msg.NSetRSP.AffectedSOPInstanceUID));
        
        response.msg.NSetRSP.DimseStatus = STATUS_N_ProcessingFailure;
        response.msg.NSetRSP.DataSetType = DIMSE_DATASET_NULL;
        
        // Send the response
        OFCondition sendCond = DIMSE_sendMessageUsingMemoryData(assoc, presID, &response, nullptr, nullptr, nullptr, nullptr, nullptr);
        if (sendCond.bad()) {
            logError("Failed to send N-SET error response: {}", sendCond.text());
        }
        return;
    }
    
    // Extract accession number and other relevant information
    std::string accessionNumber;
    std::string sopInstanceUID = request.msg.NSetRQ.RequestedSOPInstanceUID;
    
    // Use DCMTK tag constants safely
    const DcmTagKey accessionNumberTag(0x0008, 0x0050); // DCM_AccessionNumber equivalent
    
    DcmElement* element = nullptr;
    if (dataset->findAndGetElement(accessionNumberTag, element).good() && element) {
        OFString str;
        element->getOFString(str, 0);
        accessionNumber = str.c_str();
    }
    
    // Invoke callback if set
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        if (updateCallback_) {
            try {
                updateCallback_(accessionNumber, dataset);
            }
            catch (const std::exception& ex) {
                logError("Error in MPPS update callback: {}", ex.what());
            }
        }
    }
    
    // Respond with success
    T_DIMSE_Message response;
    memset(&response, 0, sizeof(response));
    response.CommandField = DIMSE_N_SET_RSP;
    response.msg.NSetRSP.MessageIDBeingRespondedTo = request.msg.NSetRQ.MessageID;
    
    // Use AffectedSOPClassUID instead of RequestedSOPClassUID
    OFStandard::strlcpy(response.msg.NSetRSP.AffectedSOPClassUID, 
                      request.msg.NSetRQ.RequestedSOPClassUID, 
                      sizeof(response.msg.NSetRSP.AffectedSOPClassUID));
    
    // Use AffectedSOPInstanceUID instead of RequestedSOPInstanceUID
    OFStandard::strlcpy(response.msg.NSetRSP.AffectedSOPInstanceUID, 
                      sopInstanceUID.c_str(), 
                      sizeof(response.msg.NSetRSP.AffectedSOPInstanceUID));
    
    response.msg.NSetRSP.DimseStatus = STATUS_Success;
    response.msg.NSetRSP.DataSetType = DIMSE_DATASET_NULL;
    
    // Send the response
    OFCondition sendCond = DIMSE_sendMessageUsingMemoryData(assoc, presID, &response, nullptr, nullptr, nullptr, nullptr, nullptr);
    if (sendCond.bad()) {
        logError("Failed to send N-SET response: {}", sendCond.text());
    }

}

} // namespace scp
} // namespace mpps
} // namespace pacs