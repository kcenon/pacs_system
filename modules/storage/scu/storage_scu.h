#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <vector>

#ifndef DCMTK_NOT_AVAILABLE
// DCMTK includes
#include "dcmtk/config/osconfig.h"
#include "dcmtk/dcmdata/dcdatset.h"
#include "dcmtk/dcmdata/dcfilefo.h"
#include "dcmtk/dcmnet/dimse.h"
#include "dcmtk/dcmnet/diutil.h"
#else
// Forward declarations for when DCMTK is not available
struct T_ASC_Association;
typedef unsigned char T_ASC_PresentationContextID;
class DcmDataset;
#endif

#include "core/interfaces/storage/storage_interface.h"
#include "common/pacs_common.h"

namespace pacs {
namespace storage {
namespace scu {

/**
 * @brief Storage SCU (Service Class User) implementation
 * 
 * This class implements the Storage SCU role, which allows sending
 * DICOM objects to a remote Storage SCP.
 */
class StorageSCU : public core::interfaces::storage::StorageInterface {
public:
    /**
     * @brief Constructor
     * @param config Configuration for the Storage SCU service
     */
    explicit StorageSCU(const common::ServiceConfig& config);
    
    /**
     * @brief Destructor
     */
    ~StorageSCU() override;
    
    /**
     * @brief Store a DICOM dataset on a remote DICOM storage SCP
     * @param dataset The DICOM dataset to store
     * @return Result with success or failure information
     */
    core::Result<void> storeDICOM(const DcmDataset* dataset) override;
    
    /**
     * @brief Store a DICOM file on a remote DICOM storage SCP
     * @param filename Path to the DICOM file
     * @return Result with success or failure information
     */
    core::Result<void> storeDICOMFile(const std::string& filename) override;
    
    /**
     * @brief Store multiple DICOM files on a remote DICOM storage SCP
     * @param filenames List of paths to DICOM files
     * @return Result with success or failure information
     */
    core::Result<void> storeDICOMFiles(const std::vector<std::string>& filenames) override;
    
    /**
     * @brief Set callback for storage notifications (not used in SCU role)
     * @param callback The callback function to invoke
     */
    void setStorageCallback(core::interfaces::storage::StorageCallback callback) override;
    
private:
    /**
     * @brief Establish DICOM association with the Storage SCP
     * @return Association pointer or nullptr on failure
     */
    T_ASC_Association* createAssociation();
    
    /**
     * @brief Release the DICOM association
     * @param assoc Association to release
     */
    void releaseAssociation(T_ASC_Association* assoc);
    
    /**
     * @brief Get presentation context ID for a given SOP class UID
     * @param assoc Association
     * @param sopClassUID SOP class UID
     * @return Presentation context ID or 0 if not found
     */
    T_ASC_PresentationContextID findPresentationContextID(T_ASC_Association* assoc, const char* sopClassUID);
    
    common::ServiceConfig config_;
    std::mutex mutex_;
};

} // namespace scu
} // namespace storage
} // namespace pacs