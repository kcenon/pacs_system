#pragma once

#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <map>

#ifndef DCMTK_NOT_AVAILABLE
// DCMTK includes
#include "dcmtk/config/osconfig.h"
#include "dcmtk/dcmdata/dcdatset.h"
#else
// Forward declarations for when DCMTK is not available
class DcmDataset;
#endif

#include "core/result/result.h"

namespace pacs {
namespace core {
namespace interfaces {
namespace query_retrieve {

/**
 * @brief Enum for different DICOM Query/Retrieve Levels
 */
enum class QueryRetrieveLevel {
    PATIENT,
    STUDY,
    SERIES,
    IMAGE
};

/**
 * @brief Structure representing a query result's key attributes
 */
struct QueryResultItem {
    std::string patientID;
    std::string patientName;
    std::string studyInstanceUID;
    std::string studyDescription;
    std::string seriesInstanceUID;
    std::string seriesDescription;
    std::string sopInstanceUID;
    std::string sopClassUID;
    QueryRetrieveLevel level;
};

/**
 * @brief Structure representing a move operation result
 */
struct MoveResult {
    int completed;  // Number of completed transfers
    int remaining;  // Number of remaining transfers
    int failed;     // Number of failed transfers
    int warning;    // Number of transfers with warnings
    bool success;   // Overall success status
    std::string message;  // Message about the transfer
};

/**
 * @brief Callback function type definition for query results
 */
using QueryCallback = std::function<void(const QueryResultItem& item, const DcmDataset* dataset)>;

/**
 * @brief Callback function type definition for retrieve operations
 */
using RetrieveCallback = std::function<void(const std::string& sopInstanceUID, const DcmDataset* dataset)>;

/**
 * @brief Callback function type definition for move operations
 */
using MoveCallback = std::function<void(const MoveResult& result)>;

/**
 * @brief Interface for DICOM Query/Retrieve operations
 */
class QueryRetrieveInterface {
public:
    virtual ~QueryRetrieveInterface() = default;

    /**
     * @brief Query for DICOM objects matching the specified criteria
     * @param searchDataset The dataset containing search criteria
     * @param level The query retrieve level (PATIENT, STUDY, SERIES, IMAGE)
     * @return Result containing a list of matching datasets
     */
    virtual Result<std::vector<DcmDataset*>> query(const DcmDataset* searchDataset, 
                                                QueryRetrieveLevel level) = 0;
    
    /**
     * @brief Retrieve DICOM objects
     * @param studyInstanceUID Study Instance UID
     * @param seriesInstanceUID Series Instance UID (optional)
     * @param sopInstanceUID SOP Instance UID (optional)
     * @return Result with success or failure information
     */
    virtual Result<void> retrieve(const std::string& studyInstanceUID,
                               const std::string& seriesInstanceUID = "",
                               const std::string& sopInstanceUID = "") = 0;
    
    /**
     * @brief Set callback for query operations
     * @param callback The callback function to invoke
     */
    virtual void setQueryCallback(QueryCallback callback) = 0;
    
    /**
     * @brief Set callback for retrieve operations
     * @param callback The callback function to invoke
     */
    virtual void setRetrieveCallback(RetrieveCallback callback) = 0;
    
    /**
     * @brief Set callback for move operations
     * @param callback The callback function to invoke
     */
    virtual void setMoveCallback(MoveCallback callback) = 0;
};

} // namespace query_retrieve
} // namespace interfaces
} // namespace core
} // namespace pacs