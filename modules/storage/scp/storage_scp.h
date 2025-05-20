#pragma once

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <vector>

#include "thread_system/sources/priority_thread_pool/priority_thread_pool.h"

#ifndef DCMTK_NOT_AVAILABLE
// DCMTK includes
#include "dcmtk/config/osconfig.h"
#include "dcmtk/dcmdata/dcdatset.h"
#include "dcmtk/dcmnet/dimse.h"
#include "dcmtk/dcmnet/diutil.h"
#else
// Forward declarations for when DCMTK is not available
struct T_ASC_Association;
typedef unsigned char T_ASC_PresentationContextID;
struct T_DIMSE_Message;
class DcmDataset;
#endif

#include "core/interfaces/storage/storage_interface.h"
#include "common/pacs_common.h"

namespace pacs {
namespace storage {
namespace scp {

/**
 * @brief Storage SCP (Service Class Provider) implementation
 * 
 * This class implements the Storage SCP role, which listens for and receives
 * DICOM objects sent by remote Storage SCUs.
 */
class StorageSCP : public core::interfaces::storage::StorageInterface {
public:
    /**
     * @brief Constructor
     * @param config Configuration for the Storage SCP service
     * @param storageDirectory Directory where received DICOM files will be stored
     */
    explicit StorageSCP(const common::ServiceConfig& config, const std::string& storageDirectory = "");
    
    /**
     * @brief Destructor
     */
    ~StorageSCP() override;
    
    /**
     * @brief Start the SCP server
     * @return Result with success or failure information
     */
    core::Result<void> start();
    
    /**
     * @brief Stop the SCP server
     */
    void stop();
    
    /**
     * @brief Store a DICOM dataset (not used in SCP role)
     * @param dataset The DICOM dataset to store
     * @return Result with success or failure information
     */
    core::Result<void> storeDICOM(const DcmDataset* dataset) override;
    
    /**
     * @brief Store a DICOM file (not used in SCP role)
     * @param filename Path to the DICOM file
     * @return Result with success or failure information
     */
    core::Result<void> storeDICOMFile(const std::string& filename) override;
    
    /**
     * @brief Store multiple DICOM files (not used in SCP role)
     * @param filenames List of paths to DICOM files
     * @return Result with success or failure information
     */
    core::Result<void> storeDICOMFiles(const std::vector<std::string>& filenames) override;
    
    /**
     * @brief Set callback for storage notifications
     * @param callback The callback function to invoke when DICOM objects are received
     */
    void setStorageCallback(core::interfaces::storage::StorageCallback callback) override;
    
    /**
     * @brief Set the directory where received DICOM files will be stored
     * @param directory Directory path
     */
    void setStorageDirectory(const std::string& directory);
    
private:
    /**
     * @brief SCP server main loop
     */
    void serverLoop();
    
    /**
     * @brief Process an incoming association
     * @param assoc The association to process
     */
    void processAssociation(T_ASC_Association* assoc);
    
    /**
     * @brief Handle C-STORE request
     * @param assoc The association
     * @param request The C-STORE request message
     * @param presID The presentation context ID
     * @param dataset The dataset
     */
    void handleCStoreRequest(T_ASC_Association* assoc, T_DIMSE_Message& request, 
                            T_ASC_PresentationContextID presID, DcmDataset* dataset);
    
    /**
     * @brief Store a received DICOM dataset to disk
     * @param sopClassUID SOP Class UID of the dataset
     * @param sopInstanceUID SOP Instance UID of the dataset 
     * @param dataset The DICOM dataset to store
     * @return true if storing succeeded, false otherwise
     */
    bool storeDatasetToDisk(const std::string& sopClassUID, const std::string& sopInstanceUID, 
                          DcmDataset* dataset);
    
    common::ServiceConfig config_;
    std::string storageDirectory_;
    std::thread serverThread_;
    std::atomic<bool> running_;
    
    core::interfaces::storage::StorageCallback storageCallback_;
    std::mutex callbackMutex_;
    
    // Thread pool for processing associations
    std::shared_ptr<priority_thread_pool_module::priority_thread_pool> threadPool_;
};

} // namespace scp
} // namespace storage
} // namespace pacs