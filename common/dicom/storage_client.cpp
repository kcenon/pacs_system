#include "storage_client.h"
#include "dicom_error.h"

#include "dcmtk/config/osconfig.h"
#include "dcmtk/dcmdata/dcdatset.h"
#include "dcmtk/dcmdata/dcfilefo.h"
#include "dcmtk/dcmdata/dcuid.h"
#include "dcmtk/dcmnet/diutil.h"
#include "dcmtk/dcmnet/dimse.h"
#include "dcmtk/dcmnet/scu.h"
#include "dcmtk/dcmnet/assoc.h"
#include "dcmtk/dcmnet/cond.h"

#include <filesystem>
#include <thread>
#include <chrono>
#include <vector>
#include <mutex>

namespace fs = std::filesystem;

namespace pacs {
namespace common {
namespace dicom {

class StorageClient::Impl {
public:
    explicit Impl(const StorageClient::Config& config) : config_(config) {}
    
    DicomVoidResult storeFile(const std::string& filename) {
        // Check if file exists
        if (!fs::exists(filename)) {
            return DicomVoidResult(DicomErrorCode::FileNotFound, 
                                 "File not found: " + filename);
        }
        
        // Load the DICOM file
        DicomFile file;
        if (!file.load(filename)) {
            return DicomVoidResult(DicomErrorCode::FileReadError, 
                                 "Failed to load DICOM file: " + filename);
        }
        
        // Store the object
        return store(file.getObject());
    }
    
    DicomVoidResult store(const DicomObject& object) {
        // Get the dataset
        DcmDataset* dataset = object.getDataset();
        if (!dataset) {
            return DicomVoidResult(DicomErrorCode::InvalidArgument, 
                                 "Invalid DICOM object, dataset is null");
        }
        
        // Initialize network
        T_ASC_Network* net = nullptr;
        OFCondition cond = ASC_initializeNetwork(NET_REQUESTOR, 0, config_.connectTimeout, &net);
        if (cond.bad()) {
            return makeDcmtkResult(cond.code(), "Network initialization");
        }
        
        // Clean up network on exit
        auto netCleanup = [&net]() {
            if (net) {
                ASC_dropNetwork(&net);
            }
        };
        std::unique_ptr<void, decltype(netCleanup)> netGuard(nullptr, netCleanup);
        
        // Create association parameters
        T_ASC_Parameters* params = nullptr;
        cond = ASC_createAssociationParameters(&params, ASC_MAXIMUMPDUSIZE);
        if (cond.bad()) {
            return makeDcmtkResult(cond.code(), "Association parameter creation");
        }
        
        // Set AP titles
        cond = ASC_setAPTitles(params, 
                             config_.localAETitle.c_str(),
                             config_.remoteAETitle.c_str(), 
                             nullptr);
        if (cond.bad()) {
            ASC_destroyAssociationParameters(&params);
            return makeDcmtkResult(cond.code(), "Setting AP titles");
        }
        
        // Set network parameters
        cond = ASC_setTransportLayerType(params, config_.useTLS);
        if (cond.bad()) {
            ASC_destroyAssociationParameters(&params);
            return makeDcmtkResult(cond.code(), "Setting transport layer");
        }
        
        // Set network addresses
        cond = ASC_setPresentationAddresses(params, 
                                          "localhost", 
                                          (config_.remoteHost + ":" + std::to_string(config_.remotePort)).c_str());
        if (cond.bad()) {
            ASC_destroyAssociationParameters(&params);
            return makeDcmtkResult(cond.code(), "Setting presentation addresses");
        }
        
        // Get SOP Class UID from dataset
        OFString sopClassUID;
        cond = dataset->findAndGetOFString(DCM_SOPClassUID, sopClassUID);
        if (cond.bad() || sopClassUID.empty()) {
            // Try to guess from modality if SOP Class UID is not present
            OFString modality;
            cond = dataset->findAndGetOFString(DCM_Modality, modality);
            if (cond.good() && !modality.empty()) {
                if (modality == "CT") {
                    sopClassUID = UID_CTImageStorage;
                } else if (modality == "MR") {
                    sopClassUID = UID_MRImageStorage;
                } else if (modality == "US") {
                    sopClassUID = UID_UltrasoundImageStorage;
                } else if (modality == "CR") {
                    sopClassUID = UID_ComputedRadiographyImageStorage;
                } else if (modality == "DX") {
                    sopClassUID = UID_DigitalXRayImageStorageForPresentation;
                } else if (modality == "RF") {
                    sopClassUID = UID_XRayRadiofluoroscopicImageStorage;
                } else if (modality == "NM") {
                    sopClassUID = UID_NuclearMedicineImageStorage;
                } else {
                    sopClassUID = UID_SecondaryCaptureImageStorage;
                }
            } else {
                // Default to Secondary Capture
                sopClassUID = UID_SecondaryCaptureImageStorage;
            }
        }
        
        // Prepare presentation contexts
        unsigned char contextID = 1;
        const char* transferSyntaxes[] = { 
            UID_LittleEndianExplicitTransferSyntax,
            UID_BigEndianExplicitTransferSyntax,
            UID_LittleEndianImplicitTransferSyntax 
        };
        int numTransferSyntaxes = 3;
        
        cond = ASC_addPresentationContext(params, contextID, 
                                         sopClassUID.c_str(),
                                         transferSyntaxes, numTransferSyntaxes);
        if (cond.bad()) {
            ASC_destroyAssociationParameters(&params);
            return makeDcmtkResult(cond.code(), "Adding presentation context");
        }
        
        // Create association
        T_ASC_Association* assoc = nullptr;
        cond = ASC_requestAssociation(net, params, &assoc);
        if (cond.bad()) {
            ASC_destroyAssociationParameters(&params);
            return makeDcmtkResult(cond.code(), "Requesting association");
        }
        
        // Clean up association on exit
        auto assocCleanup = [&assoc]() {
            if (assoc) {
                ASC_releaseAssociation(assoc);
                ASC_destroyAssociation(&assoc);
            }
        };
        std::unique_ptr<void, decltype(assocCleanup)> assocGuard(nullptr, assocCleanup);
        
        // Check if the association was accepted
        if (ASC_countAcceptedPresentationContexts(assoc->params) == 0) {
            return DicomVoidResult(DicomErrorCode::AssociationRejected, 
                                 "No presentation contexts accepted");
        }
        
        // Find the accepted presentation context for the SOP class
        T_ASC_PresentationContextID pc = ASC_findAcceptedPresentationContextID(assoc, 
                                                                             sopClassUID.c_str());
        if (pc == 0) {
            return DicomVoidResult(DicomErrorCode::UnsupportedSOPClass, 
                                 "No accepted presentation context for SOP class: " + 
                                 std::string(sopClassUID.c_str()));
        }
        
        // Prepare C-STORE request
        T_DIMSE_C_StoreRQ req;
        memset(&req, 0, sizeof(req));
        
        // Generate a unique Message ID
        req.MessageID = assoc->nextMsgID++;
        
        // Set the SOP Class UID
        strncpy(req.AffectedSOPClassUID, sopClassUID.c_str(), 
               sizeof(req.AffectedSOPClassUID));
        
        // Get the SOP Instance UID
        OFString sopInstanceUID;
        cond = dataset->findAndGetOFString(DCM_SOPInstanceUID, sopInstanceUID);
        if (cond.bad() || sopInstanceUID.empty()) {
            // Generate a new UID if not present
            char uid[100];
            dcmGenerateUniqueIdentifier(uid, SITE_INSTANCE_UID_ROOT);
            sopInstanceUID = uid;
            dataset->putAndInsertString(DCM_SOPInstanceUID, uid);
        }
        
        // Set the SOP Instance UID
        strncpy(req.AffectedSOPInstanceUID, sopInstanceUID.c_str(), 
               sizeof(req.AffectedSOPInstanceUID));
        
        // Set other fields
        req.DataSetType = DIMSE_DATASET_PRESENT;
        req.Priority = DIMSE_PRIORITY_MEDIUM;
        
        // Prepare the response variable
        T_DIMSE_C_StoreRSP rsp;
        DcmDataset* statusDetail = nullptr;
        
        // Send the C-STORE request
        cond = DIMSE_storeUser(assoc, pc, &req, dataset, nullptr, nullptr, 
                              config_.dimseTimeout, 
                              DIMSE_BLOCKING, 0, 
                              &rsp, &statusDetail);
        
        // Clean up status detail dataset
        if (statusDetail != nullptr) {
            delete statusDetail;
        }
        
        // Check for errors
        if (cond.bad()) {
            return makeDcmtkResult(cond.code(), "DIMSE store operation");
        }
        
        // Check response status
        if (rsp.DimseStatus != STATUS_Success) {
            std::string errorMsg = "C-STORE failed with status: " + 
                                  std::to_string(rsp.DimseStatus);
            
            DicomErrorCode errorCode;
            if (rsp.DimseStatus == STATUS_STORE_Refused_OutOfResources) {
                errorCode = DicomErrorCode::CStoreFailure;
                errorMsg += " (Out of Resources)";
            } else {
                errorCode = DicomErrorCode::CStoreFailure;
            }
            
            return DicomVoidResult(errorCode, errorMsg);
        }
        
        return DicomVoidResult(); // Success
    }
    
    DicomVoidResult storeFiles(const std::vector<std::string>& filenames, 
                             ProgressCallback progressCallback) {
        if (filenames.empty()) {
            return DicomVoidResult();  // Nothing to do
        }
        
        int totalCount = static_cast<int>(filenames.size());
        int failedCount = 0;
        std::string lastError;
        
        for (int i = 0; i < totalCount; i++) {
            const std::string& filename = filenames[i];
            
            // Call progress callback if provided
            if (progressCallback) {
                progressCallback(i, totalCount, filename);
            }
            
            // Store the file
            auto result = storeFile(filename);
            if (result.isError()) {
                failedCount++;
                lastError = result.getErrorMessage();
            }
        }
        
        // Report final status
        if (failedCount > 0) {
            std::string errorMsg = std::to_string(failedCount) + " of " + 
                                  std::to_string(totalCount) + " files failed to store. Last error: " + 
                                  lastError;
            return DicomVoidResult(DicomErrorCode::CStoreFailure, errorMsg);
        }
        
        return DicomVoidResult();  // Success
    }
    
    DicomVoidResult storeDirectory(const std::string& directory, bool recursive,
                                 ProgressCallback progressCallback) {
        // Check if directory exists
        if (!fs::exists(directory) || !fs::is_directory(directory)) {
            return DicomVoidResult(DicomErrorCode::FileNotFound, 
                                 "Directory not found: " + directory);
        }
        
        // Collect DICOM files from the directory
        std::vector<std::string> dicomFiles;
        
        auto fileCollector = [&dicomFiles](const fs::path& path) {
            if (fs::is_regular_file(path)) {
                std::string ext = path.extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".dcm" || ext == ".dicom" || ext == ".dic") {
                    dicomFiles.push_back(path.string());
                }
            }
        };
        
        if (recursive) {
            for (const auto& entry : fs::recursive_directory_iterator(directory)) {
                fileCollector(entry.path());
            }
        } else {
            for (const auto& entry : fs::directory_iterator(directory)) {
                fileCollector(entry.path());
            }
        }
        
        // Store the collected files
        return storeFiles(dicomFiles, progressCallback);
    }
    
private:
    StorageClient::Config config_;
};

StorageClient::StorageClient(const Config& config)
    : config_(config), impl_(std::make_unique<Impl>(config)) {
}

StorageClient::~StorageClient() = default;

DicomVoidResult StorageClient::store(const DicomObject& object) {
    return impl_->store(object);
}

DicomVoidResult StorageClient::store(const DicomFile& file) {
    return impl_->store(file.getObject());
}

DicomVoidResult StorageClient::storeFile(const std::string& filename) {
    return impl_->storeFile(filename);
}

DicomVoidResult StorageClient::storeFiles(const std::vector<std::string>& filenames, 
                                        ProgressCallback progressCallback) {
    return impl_->storeFiles(filenames, progressCallback);
}

DicomVoidResult StorageClient::storeDirectory(const std::string& directory, bool recursive,
                                            ProgressCallback progressCallback) {
    return impl_->storeDirectory(directory, recursive, progressCallback);
}

void StorageClient::setConfig(const Config& config) {
    config_ = config;
    impl_ = std::make_unique<Impl>(config);
}

const StorageClient::Config& StorageClient::getConfig() const {
    return config_;
}

} // namespace dicom
} // namespace common
} // namespace pacs