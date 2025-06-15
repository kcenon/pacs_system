#pragma once

#include <string>
#include <memory>
#include <functional>
#include <vector>

#ifndef USE_DCMTK_PLACEHOLDER
// DCMTK includes
#ifdef HAVE_CONFIG_H
#include "dcmtk/config/osconfig.h"
#endif
#include "dcmtk/dcmdata/dcdatset.h"
#include "dcmtk/dcmdata/dcfilefo.h"
#else
// Forward declarations for when DCMTK is not available
class DcmDataset;
class DcmFileFormat;
#endif

#include "core/result/result.h"

namespace pacs {
namespace core {
namespace interfaces {
namespace storage {

/**
 * @brief Callback function type definition for storage events
 */
using StorageCallback = std::function<void(const std::string& sopInstanceUID, const DcmDataset* dataset)>;

/**
 * @brief Interface for DICOM Storage operations
 */
class StorageInterface {
public:
    virtual ~StorageInterface() = default;

    /**
     * @brief Store a DICOM dataset on a remote DICOM storage SCP
     * @param dataset The DICOM dataset to store
     * @return Result with success or failure information
     */
    virtual Result<void> storeDICOM(const DcmDataset* dataset) = 0;
    
    /**
     * @brief Store a DICOM file on a remote DICOM storage SCP
     * @param filename Path to the DICOM file
     * @return Result with success or failure information
     */
    virtual Result<void> storeDICOMFile(const std::string& filename) = 0;
    
    /**
     * @brief Store multiple DICOM files on a remote DICOM storage SCP
     * @param filenames List of paths to DICOM files
     * @return Result with success or failure information
     */
    virtual Result<void> storeDICOMFiles(const std::vector<std::string>& filenames) = 0;

    /**
     * @brief Set callback for storage notifications
     * @param callback The callback function to invoke
     */
    virtual void setStorageCallback(StorageCallback callback) = 0;
};

} // namespace storage
} // namespace interfaces
} // namespace core
} // namespace pacs