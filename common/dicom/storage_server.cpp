#include "storage_server.h"
#include "dicom_error.h"

#include "dcmtk/config/osconfig.h"
#include "dcmtk/dcmdata/dcdatset.h"
#include "dcmtk/dcmdata/dcfilefo.h"
#include "dcmtk/dcmdata/dcuid.h"
#include "dcmtk/dcmnet/diutil.h"
#include "dcmtk/dcmnet/dimse.h"
#include "dcmtk/dcmnet/scp.h"
#include "dcmtk/dcmnet/assoc.h"
#include "dcmtk/dcmnet/cond.h"

#include "thread_system/sources/logger/logger.h"
#include "thread_system/sources/logger/log_types.h"

#include <filesystem>
#include <thread>
#include <chrono>
#include <vector>
#include <mutex>
#include <atomic>
#include <condition_variable>

namespace fs = std::filesystem;
using namespace log_module;

namespace pacs {
namespace common {
namespace dicom {

class StorageServer::Impl {
public:
    explicit Impl(const StorageServer::Config& config, StorageCallback callback)
        : config_(config), callback_(callback), running_(false) {}
    
    ~Impl() {
        // Make sure to stop the server on destruction
        stop();
    }
    
    DicomVoidResult start() {
        // Check if already running
        if (running_) {
            return DicomVoidResult();  // Already running
        }
        
        // Create storage directory if it doesn't exist
        if (!fs::exists(config_.storageDirectory)) {
            std::error_code ec;
            if (!fs::create_directories(config_.storageDirectory, ec) && ec) {
                return DicomVoidResult(DicomErrorCode::FileWriteError,
                                     "Failed to create storage directory: " + 
                                     config_.storageDirectory);
            }
        }
        
        // Start server thread
        try {
            running_ = true;
            serverThread_ = std::thread(&Impl::serverLoop, this);
            return DicomVoidResult();  // Success
        } catch (const std::exception& ex) {
            running_ = false;
            return DicomVoidResult(DicomErrorCode::Unknown,
                                 "Failed to start server thread: " + 
                                 std::string(ex.what()));
        }
    }
    
    void stop() {
        if (running_) {
            running_ = false;
            
            // Wait for server thread to finish
            if (serverThread_.joinable()) {
                serverThread_.join();
            }
        }
    }
    
    bool isRunning() const {
        return running_;
    }
    
private:
    // Main server loop
    void serverLoop() {
        // Initialize network
        T_ASC_Network* net = nullptr;
        OFCondition cond = ASC_initializeNetwork(NET_ACCEPTOR, config_.port, 
                                               30, &net);
        if (cond.bad()) {
            write_error("Failed to initialize network: %s", cond.text());
            running_ = false;
            return;
        }
        
        // Clean up network on exit
        auto netCleanup = [&net]() {
            if (net) {
                ASC_dropNetwork(&net);
            }
        };
        std::unique_ptr<void, decltype(netCleanup)> netGuard(nullptr, netCleanup);
        
        // Server loop
        write_information("Storage SCP started on port %d with AE title: %s", 
                         config_.port, config_.aeTitle.c_str());
        
        while (running_) {
            // Create association parameters
            T_ASC_Parameters* params = nullptr;
            cond = ASC_createAssociationParameters(&params, ASC_MAXIMUMPDUSIZE);
            if (cond.bad()) {
                write_error("Failed to create association parameters: %s", cond.text());
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }
            
            // Set transport layer
            cond = ASC_setTransportLayerType(params, config_.useTLS);
            if (cond.bad()) {
                write_error("Failed to set transport layer: %s", cond.text());
                ASC_destroyAssociationParameters(&params);
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }
            
            // Define all Storage SOP Classes for storage SCP
            const char* transferSyntaxes[] = { 
                UID_LittleEndianExplicitTransferSyntax,
                UID_BigEndianExplicitTransferSyntax,
                UID_LittleEndianImplicitTransferSyntax 
            };
            int numTransferSyntaxes = 3;
            
            // Add presentation contexts for common Storage SOP Classes
            const char* sopClasses[] = {
                UID_CTImageStorage,
                UID_MRImageStorage,
                UID_UltrasoundImageStorage,
                UID_ComputedRadiographyImageStorage,
                UID_DigitalXRayImageStorageForPresentation,
                UID_DigitalXRayImageStorageForProcessing,
                UID_XRayAngiographicImageStorage,
                UID_XRayRadiofluoroscopicImageStorage,
                UID_NuclearMedicineImageStorage,
                UID_SecondaryCaptureImageStorage,
                UID_EnhancedMRImageStorage,
                UID_EnhancedCTImageStorage,
                UID_EnhancedXAImageStorage,
                UID_EnhancedUSVolumeStorage,
                UID_MultiframeGrayscaleByteSecondaryCaptureImageStorage,
                UID_MultiframeGrayscaleWordSecondaryCaptureImageStorage,
                UID_MultiframeTrueColorSecondaryCaptureImageStorage
            };
            int numSOPClasses = sizeof(sopClasses) / sizeof(sopClasses[0]);
            
            unsigned char contextID = 1;
            for (int i = 0; i < numSOPClasses; i++) {
                cond = ASC_addPresentationContext(params, contextID, 
                                                sopClasses[i],
                                                transferSyntaxes, numTransferSyntaxes);
                if (cond.bad()) {
                    write_error("Failed to add presentation context: %s", cond.text());
                    continue;
                }
                contextID += 2;  // Add with odd IDs per DICOM standard
            }
            
            // Wait for and accept association
            write_information("Waiting for association on port %d", config_.port);
            T_ASC_Association* assoc = nullptr;
            cond = ASC_waitForAssociation(net, &assoc, 5);
            
            if (cond.bad()) {
                if (cond != DUL_NOASSOCIATIONREQUEST) {
                    write_error("Failed while waiting for association: %s", cond.text());
                }
                
                ASC_destroyAssociationParameters(&params);
                continue;  // Try again
            }
            
            // Clean up association on exit
            auto assocCleanup = [&assoc]() {
                if (assoc) {
                    ASC_dropAssociation(assoc);
                    ASC_destroyAssociation(&assoc);
                }
            };
            std::unique_ptr<void, decltype(assocCleanup)> assocGuard(nullptr, assocCleanup);
            
            // Handle association in a separate thread to continue waiting for new ones
            std::thread associationThread(&Impl::handleAssociation, this, assoc);
            associationThread.detach();  // Let it run independently
            
            ASC_destroyAssociationParameters(&params);
        }
    }
    
    // Handle a single association
    void handleAssociation(T_ASC_Association* assoc) {
        if (!assoc) return;
        
        // Get calling AE title
        const char* callingAE = assoc->params->DULparams.callingAPTitle;
        
        // Check if peer is allowed
        if (!config_.allowAnyAETitle && !config_.allowedAEPeers.empty()) {
            bool allowed = false;
            for (const auto& peer : config_.allowedAEPeers) {
                if (peer == callingAE) {
                    allowed = true;
                    break;
                }
            }
            
            if (!allowed) {
                write_error("Association rejected, calling AE title not allowed: %s", callingAE);
                ASC_rejectAssociation(assoc, ASC_RESULT_REJECTEDPERMANENT, 
                                    ASC_SOURCE_SERVICEUSER, 
                                    ASC_REASON_SU_CALLINGAETITLENOTRECOGNIZED);
                return;
            }
        }
        
        // Accept association
        write_information("Association received from %s", callingAE);
        OFCondition cond = ASC_acknowledgeAssociation(assoc);
        if (cond.bad()) {
            write_error("Failed to acknowledge association: %s", cond.text());
            return;
        }
        
        // Handle commands until released or aborted
        bool finished = false;
        T_DIMSE_Message msg;
        T_ASC_PresentationContextID presID;
        
        while (!finished && running_) {
            // Receive command
            cond = DIMSE_receiveCommand(assoc, DIMSE_BLOCKING, 0, 
                                      &presID, &msg, nullptr);
            
            if (cond.bad()) {
                if (cond == DUL_PEERREQUESTEDRELEASE) {
                    write_information("Association released by peer");
                    ASC_acknowledgeRelease(assoc);
                    finished = true;
                } else if (cond == DUL_PEERABORTEDASSOCIATION) {
                    write_information("Association aborted by peer");
                    finished = true;
                } else {
                    write_error("Failed to receive command: %s", cond.text());
                    finished = true;
                }
                
                continue;
            }
            
            // Process command
            switch (msg.CommandField) {
                case DIMSE_C_STORE_RQ:
                    handleCStoreRequest(assoc, msg, presID);
                    break;
                    
                case DIMSE_C_ECHO_RQ:
                    handleCEchoRequest(assoc, msg, presID);
                    break;
                    
                default:
                    // Unsupported command, reject it
                    write_error("Unsupported command received: 0x%X", msg.CommandField);
                    DIMSE_rejectCommand(assoc, &msg, DIMSE_BADCOMMANDTYPE);
                    break;
            }
        }
    }
    
    // Handle C-STORE request
    void handleCStoreRequest(T_ASC_Association* assoc, T_DIMSE_Message& msg, 
                           T_ASC_PresentationContextID presID) {
        // Extract request
        T_DIMSE_C_StoreRQ& req = msg.msg.CStoreRQ;
        
        // Prepare response
        T_DIMSE_C_StoreRSP rsp;
        DcmDataset* statusDetail = nullptr;
        
        // Initialize response with default success
        rsp.DimseStatus = STATUS_Success;
        rsp.MessageIDBeingRespondedTo = req.MessageID;
        rsp.DataSetType = DIMSE_DATASET_NULL;
        strncpy(rsp.AffectedSOPClassUID, req.AffectedSOPClassUID, 
               sizeof(rsp.AffectedSOPClassUID));
        strncpy(rsp.AffectedSOPInstanceUID, req.AffectedSOPInstanceUID, 
               sizeof(rsp.AffectedSOPInstanceUID));
        
        // Get SOP Class and Instance UIDs
        std::string sopClassUID = req.AffectedSOPClassUID;
        std::string sopInstanceUID = req.AffectedSOPInstanceUID;
        
        // Generate a unique filename for storing the dataset
        std::string filename = generateStorageFilename(sopClassUID, sopInstanceUID);
        
        // Receive dataset
        DcmDataset* dataset = nullptr;
        OFCondition cond = DIMSE_receiveDataSetInMemory(assoc, DIMSE_BLOCKING, 0, 
                                                      &presID, &dataset, nullptr, nullptr);
        
        // Handle error in receiving dataset
        if (cond.bad() || dataset == nullptr) {
            write_error("Failed to receive dataset: %s", 
                       cond.bad() ? cond.text() : "dataset is null");
            rsp.DimseStatus = STATUS_STORE_Error_CannotUnderstand;
            DIMSE_sendStoreResponse(assoc, presID, &rsp, nullptr);
            
            if (dataset) {
                delete dataset;
            }
            
            return;
        }
        
        // Store the dataset
        cond = storeDataset(dataset, filename, sopClassUID, sopInstanceUID, 
                          assoc->params->DULparams.callingAPTitle);
        
        // Handle error in storing dataset
        if (cond.bad()) {
            write_error("Failed to store dataset: %s", cond.text());
            rsp.DimseStatus = STATUS_STORE_Refused_OutOfResources;
            DIMSE_sendStoreResponse(assoc, presID, &rsp, nullptr);
            
            delete dataset;
            return;
        }
        
        // Send success response
        cond = DIMSE_sendStoreResponse(assoc, presID, &rsp, nullptr);
        
        if (cond.bad()) {
            write_error("Failed to send C-STORE response: %s", cond.text());
        }
        
        // Clean up
        delete dataset;
    }
    
    // Handle C-ECHO request
    void handleCEchoRequest(T_ASC_Association* assoc, T_DIMSE_Message& msg, 
                          T_ASC_PresentationContextID presID) {
        // Extract request
        T_DIMSE_C_EchoRQ& req = msg.msg.CEchoRQ;
        
        // Prepare response
        T_DIMSE_C_EchoRSP rsp;
        
        // Initialize response with default success
        rsp.DimseStatus = STATUS_Success;
        rsp.MessageIDBeingRespondedTo = req.MessageID;
        rsp.DataSetType = DIMSE_DATASET_NULL;
        strncpy(rsp.AffectedSOPClassUID, req.AffectedSOPClassUID, 
               sizeof(rsp.AffectedSOPClassUID));
        
        // Send response
        OFCondition cond = DIMSE_sendEchoResponse(assoc, presID, &rsp, nullptr);
        
        if (cond.bad()) {
            write_error("Failed to send C-ECHO response: %s", cond.text());
        } else {
            write_information("C-ECHO response sent successfully");
        }
    }
    
    // Store received dataset to disk
    OFCondition storeDataset(DcmDataset* dataset, const std::string& filename, 
                           const std::string& sopClassUID, 
                           const std::string& sopInstanceUID,
                           const char* callingAE) {
        if (!dataset) {
            return EC_IllegalParameter;
        }
        
        // Create DicomObject from dataset
        DicomObject obj(dataset->clone());
        
        // Build path and create directories if needed
        std::string fullPath = filename;
        std::string directoryPath = fs::path(filename).parent_path().string();
        
        if (!directoryPath.empty() && !fs::exists(directoryPath)) {
            std::error_code ec;
            if (!fs::create_directories(directoryPath, ec) && ec) {
                return EC_InvalidFilename;
            }
        }
        
        // Create DicomFile and save it
        DicomFile file(obj);
        bool success = file.save(fullPath);
        
        if (!success) {
            return EC_CannotWriteFile;
        }
        
        // Extract additional information for the callback
        std::string patientID = obj.getPatientID();
        std::string patientName = obj.getPatientName();
        std::string studyInstanceUID = obj.getStudyInstanceUID();
        std::string seriesInstanceUID = obj.getSeriesInstanceUID();
        std::string modality = obj.getModality();
        
        // Call the callback if set
        if (callback_) {
            StorageEvent event;
            event.sopClassUID = sopClassUID;
            event.sopInstanceUID = sopInstanceUID;
            event.patientID = patientID;
            event.patientName = patientName;
            event.studyInstanceUID = studyInstanceUID;
            event.seriesInstanceUID = seriesInstanceUID;
            event.modality = modality;
            event.filename = fullPath;
            event.callingAETitle = callingAE ? callingAE : "";
            event.object = std::move(obj);
            
            // Call the callback asynchronously to not block the server
            std::thread callbackThread([this, event]() {
                callback_(event);
            });
            callbackThread.detach();
        }
        
        write_information("Stored DICOM object: %s", fullPath.c_str());
        return EC_Normal;
    }
    
    // Generate a filename for storing a received dataset
    std::string generateStorageFilename(const std::string& sopClassUID, 
                                      const std::string& sopInstanceUID) {
        if (!config_.organizeFolders) {
            // Simple flat structure with instance UID as filename
            std::string filename = sopInstanceUID;
            std::replace(filename.begin(), filename.end(), '.', '_');
            return config_.storageDirectory + "/" + filename + ".dcm";
        }
        
        // We'll need to load the dataset to get patient/study/series info
        // Use a temporary path for now, will be organized later in storeDataset
        std::string filename = sopInstanceUID;
        std::replace(filename.begin(), filename.end(), '.', '_');
        return config_.storageDirectory + "/" + filename + ".dcm";
    }
    
private:
    StorageServer::Config config_;
    StorageCallback callback_;
    std::atomic<bool> running_;
    std::thread serverThread_;
};

StorageServer::StorageServer(const Config& config) 
    : config_(config), callback_(nullptr), running_(false) {
    impl_ = std::make_unique<Impl>(config, callback_);
}

StorageServer::~StorageServer() = default;

DicomVoidResult StorageServer::start() {
    return impl_->start();
}

void StorageServer::stop() {
    impl_->stop();
    running_ = false;
}

bool StorageServer::isRunning() const {
    return impl_->isRunning();
}

void StorageServer::setStorageCallback(StorageCallback callback) {
    callback_ = callback;
    impl_ = std::make_unique<Impl>(config_, callback_);
}

void StorageServer::setConfig(const Config& config) {
    config_ = config;
    impl_ = std::make_unique<Impl>(config_, callback_);
}

const StorageServer::Config& StorageServer::getConfig() const {
    return config_;
}

} // namespace dicom
} // namespace common
} // namespace pacs