#include "storage_scu.h"

#include <string>
#include <fstream>

// DCMTK includes
#include "dcmtk/dcmnet/diutil.h"
#include "dcmtk/dcmnet/scu.h"
#include "dcmtk/dcmdata/dcfilefo.h"
#include "dcmtk/dcmdata/dcuid.h"
#include "dcmtk/dcmdata/dcdeftag.h"
#include "dcmtk/dcmnet/dimse.h"
#include "dcmtk/dcmnet/dicom.h"
#include "dcmtk/dcmnet/assoc.h"
#include "dcmtk/dcmnet/cond.h"
#include "dcmtk/dcmnet/dul.h"

#include "common/logger/logger.h"
#include "common/dicom_util.h"

namespace pacs {
namespace storage {
namespace scu {

using namespace log_module;

StorageSCU::StorageSCU(const common::ServiceConfig& config)
    : config_(config) {
    // Configure DCMTK logging
    OFLog::configure(OFLogger::WARN_LOG_LEVEL);
}

StorageSCU::~StorageSCU() = default;

core::Result<void> StorageSCU::storeDICOM(const DcmDataset* dataset) {
    if (!dataset) {
        return core::Result<void>::error("DICOM dataset is null");
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // Create association with remote Storage SCP
        T_ASC_Association* assoc = createAssociation();
        if (!assoc) {
            return core::Result<void>::error("Failed to create association with Storage SCP");
        }
        
        // Extract SOP Class UID from the dataset
        OFString sopClassUIDStr;
        OFCondition cond = const_cast<DcmDataset*>(dataset)->findAndGetOFString(DCM_SOPClassUID, sopClassUIDStr);
        if (cond.bad() || sopClassUIDStr.empty()) {
            releaseAssociation(assoc);
            return core::Result<void>::error("Missing SOP Class UID in DICOM dataset");
        }
        
        // Find appropriate presentation context
        T_ASC_PresentationContextID presID = findPresentationContextID(assoc, sopClassUIDStr.c_str());
        if (presID == 0) {
            releaseAssociation(assoc);
            return core::Result<void>::error("No presentation context for SOP Class UID: " + 
                                          std::string(sopClassUIDStr.c_str()));
        }
        
        // Create C-STORE request message
        T_DIMSE_Message request;
        T_DIMSE_Message response;
        
        memset(&request, 0, sizeof(request));
        memset(&response, 0, sizeof(response));
        
        request.CommandField = DIMSE_C_STORE_RQ;
        
        // Set up C-STORE parameters
        T_DIMSE_C_StoreRQ& storeReq = request.msg.CStoreRQ;
        storeReq.MessageID = assoc->nextMsgID++;
        
        // Generate a unique message ID
        storeReq.DataSetType = DIMSE_DATASET_PRESENT;
        
        // Extract SOP Instance UID from the dataset
        OFString sopInstanceUIDStr;
        cond = const_cast<DcmDataset*>(dataset)->findAndGetOFString(DCM_SOPInstanceUID, sopInstanceUIDStr);
        if (cond.bad() || sopInstanceUIDStr.empty()) {
            releaseAssociation(assoc);
            return core::Result<void>::error("Missing SOP Instance UID in DICOM dataset");
        }
        
        OFStandard::strlcpy(storeReq.AffectedSOPClassUID, sopClassUIDStr.c_str(), 
                          sizeof(storeReq.AffectedSOPClassUID));
        OFStandard::strlcpy(storeReq.AffectedSOPInstanceUID, sopInstanceUIDStr.c_str(), 
                          sizeof(storeReq.AffectedSOPInstanceUID));
        
        storeReq.Priority = DIMSE_PRIORITY_MEDIUM;
        storeReq.MoveOriginatorID = 0;
        
        // Send C-STORE request
        cond = DIMSE_sendMessageUsingMemoryData(assoc, presID, &request, nullptr, 
                                              const_cast<DcmDataset*>(dataset), 
                                              nullptr, nullptr, nullptr);
        
        if (cond.bad()) {
            releaseAssociation(assoc);
            return core::Result<void>::error(std::string("Failed to send C-STORE request: ") + 
                                          cond.text());
        }
        
        // Receive C-STORE response
        cond = DIMSE_receiveCommand(assoc, DIMSE_BLOCKING, 0, &presID, &response, nullptr);
        
        if (cond.bad()) {
            releaseAssociation(assoc);
            return core::Result<void>::error(std::string("Failed to receive C-STORE response: ") + 
                                          cond.text());
        }
        
        // Check status
        if (response.CommandField != DIMSE_C_STORE_RSP) {
            releaseAssociation(assoc);
            return core::Result<void>::error("Unexpected response command");
        }
        
        T_DIMSE_C_StoreRSP& storeRsp = response.msg.CStoreRSP;
        
        if (storeRsp.DimseStatus != STATUS_Success) {
            releaseAssociation(assoc);
            return core::Result<void>::error(std::string("C-STORE failed with status: ") + 
                                          DU_cstoreStatusString(storeRsp.DimseStatus));
        }
        
        // Release association
        releaseAssociation(assoc);
        
        return core::Result<void>::ok();
    }
    catch (const std::exception& ex) {
        return core::Result<void>::error(std::string("Exception during DICOM storage: ") + ex.what());
    }
}

core::Result<void> StorageSCU::storeDICOMFile(const std::string& filename) {
    // Load DICOM file
    DcmFileFormat fileFormat;
    OFCondition cond = fileFormat.loadFile(filename.c_str());
    
    if (cond.bad()) {
        return core::Result<void>::error(std::string("Failed to load DICOM file: ") + cond.text());
    }
    
    // Store the DICOM dataset
    return storeDICOM(fileFormat.getDataset());
}

core::Result<void> StorageSCU::storeDICOMFiles(const std::vector<std::string>& filenames) {
    if (filenames.empty()) {
        return core::Result<void>::error("No DICOM files to store");
    }
    
    // Store each file
    for (const auto& filename : filenames) {
        core::Result<void> result = storeDICOMFile(filename);
        if (result.isError()) {
            return result;
        }
    }
    
    return core::Result<void>::ok();
}

void StorageSCU::setStorageCallback(core::interfaces::storage::StorageCallback callback) {
    // Not used in SCU role
}

T_ASC_Association* StorageSCU::createAssociation() {
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
    cond = ASC_createAssociationParameters(&params, ASC_DEFAULTMAXPDU);
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
    
    // Add presentation contexts for Storage SOP Classes
    const char* transferSyntaxes[] = { UID_LittleEndianExplicitTransferSyntax, 
                                      UID_LittleEndianImplicitTransferSyntax, 
                                      UID_BigEndianExplicitTransferSyntax };
    int numTransferSyntaxes = 3;
    
    // Add common storage SOP classes
    int pid = 1;
    cond = ASC_addPresentationContext(params, pid, UID_ComputedRadiographyImageStorage,
                                     transferSyntaxes, numTransferSyntaxes);
    pid += 2;
    cond = ASC_addPresentationContext(params, pid, UID_DigitalXRayImageStorageForPresentation,
                                     transferSyntaxes, numTransferSyntaxes);
    pid += 2;
    cond = ASC_addPresentationContext(params, pid, UID_DigitalXRayImageStorageForProcessing,
                                     transferSyntaxes, numTransferSyntaxes);
    pid += 2;
    cond = ASC_addPresentationContext(params, pid, UID_DigitalMammographyXRayImageStorageForPresentation,
                                     transferSyntaxes, numTransferSyntaxes);
    pid += 2;
    cond = ASC_addPresentationContext(params, pid, UID_DigitalMammographyXRayImageStorageForProcessing,
                                     transferSyntaxes, numTransferSyntaxes);
    pid += 2;
    cond = ASC_addPresentationContext(params, pid, UID_CTImageStorage,
                                     transferSyntaxes, numTransferSyntaxes);
    pid += 2;
    cond = ASC_addPresentationContext(params, pid, UID_MRImageStorage,
                                     transferSyntaxes, numTransferSyntaxes);
    pid += 2;
    cond = ASC_addPresentationContext(params, pid, UID_UltrasoundImageStorage,
                                     transferSyntaxes, numTransferSyntaxes);
    pid += 2;
    cond = ASC_addPresentationContext(params, pid, UID_SecondaryCaptureImageStorage,
                                     transferSyntaxes, numTransferSyntaxes);
    pid += 2;
    cond = ASC_addPresentationContext(params, pid, UID_EnhancedCTImageStorage,
                                     transferSyntaxes, numTransferSyntaxes);
    pid += 2;
    cond = ASC_addPresentationContext(params, pid, UID_EnhancedMRImageStorage,
                                     transferSyntaxes, numTransferSyntaxes);
    pid += 2;
    cond = ASC_addPresentationContext(params, pid, UID_EnhancedMRColorImageStorage,
                                     transferSyntaxes, numTransferSyntaxes);
    pid += 2;
    cond = ASC_addPresentationContext(params, pid, UID_EnhancedUSVolumeStorage,
                                     transferSyntaxes, numTransferSyntaxes);
    
    // Request association
    cond = ASC_requestAssociation(net, params, &assoc);
    if (cond.bad() || !assoc) {
        ASC_dropNetwork(&net);
        ASC_destroyAssociationParameters(&params);
        return nullptr;
    }
    
    // Check if the remote AE accepted at least one of our proposed presentation contexts
    if (ASC_countAcceptedPresentationContexts(params) == 0) {
        ASC_releaseAssociation(assoc);
        ASC_dropAssociation(assoc);
        ASC_dropNetwork(&net);
        ASC_destroyAssociationParameters(&params);
        return nullptr;
    }
    
    return assoc;
}

void StorageSCU::releaseAssociation(T_ASC_Association* assoc) {
    if (assoc) {
        ASC_releaseAssociation(assoc);
        ASC_dropAssociation(assoc);
        ASC_destroyAssociation(&assoc);
    }
}

T_ASC_PresentationContextID StorageSCU::findPresentationContextID(T_ASC_Association* assoc, 
                                                                const char* sopClassUID) {
    T_ASC_PresentationContextID presID = 0;
    
    if (assoc && sopClassUID) {
        // Find the first accepted presentation context for this SOP class
        presID = ASC_findAcceptedPresentationContextID(assoc, sopClassUID);
    }
    
    return presID;
}

} // namespace scu
} // namespace storage
} // namespace pacs