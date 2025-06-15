#pragma once

#include <string>
#include <vector>
#include <memory>

#ifndef USE_DCMTK_PLACEHOLDER
// DCMTK includes
#ifdef HAVE_CONFIG_H
#include "dcmtk/config/osconfig.h"
#endif
#include "dcmtk/dcmdata/dcdatset.h"
#include "dcmtk/dcmdata/dcfilefo.h"
#include "dcmtk/dcmdata/dcuid.h"
#include "dcmtk/dcmnet/diutil.h"
#else
// Forward declarations for when DCMTK is not available
class DcmDataset;
class DcmFileFormat;
class DcmTagKey;
#endif

namespace pacs {
namespace common {

/**
 * @brief Utility class for DICOM operations
 */
class DicomUtil {
public:
    /**
     * @brief Convert DICOM dataset to a string representation
     * @param dataset The DICOM dataset to convert
     * @return String representation of the dataset
     */
    static std::string datasetToString(const DcmDataset* dataset);
    
    /**
     * @brief Load DICOM file from disk
     * @param filename Path to the DICOM file
     * @return Loaded DcmFileFormat object or nullptr on failure
     */
    static std::unique_ptr<DcmFileFormat> loadDicomFile(const std::string& filename);
    
    /**
     * @brief Save DICOM dataset to file
     * @param dataset The dataset to save
     * @param filename The path to save to
     * @return True if successful, false otherwise
     */
    static bool saveDicomFile(DcmDataset* dataset, const std::string& filename);
    
    /**
     * @brief Get a specific element value from a DICOM dataset
     * @param dataset The DICOM dataset
     * @param tag The DICOM tag to retrieve
     * @return String value of the element or empty string if not found
     */
    static std::string getElementValue(const DcmDataset* dataset, const DcmTagKey& tag);
};

} // namespace common
} // namespace pacs