#pragma once

#include <string>
#include <memory>
#include <functional>
#include <vector>

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
namespace worklist {

/**
 * @brief Structure representing a worklist item's key attributes
 */
struct WorklistItem {
    std::string patientID;
    std::string patientName;
    std::string accessionNumber;
    std::string scheduledProcedureStepStartDate;
    std::string scheduledProcedureStepStartTime;
    std::string modality;
    std::string scheduledStationAETitle;
    std::string scheduledProcedureStepDescription;
};

/**
 * @brief Callback function type definition for worklist events
 */
using WorklistCallback = std::function<void(const WorklistItem& item, const DcmDataset* dataset)>;

/**
 * @brief Interface for Modality Worklist operations
 */
class WorklistInterface {
public:
    virtual ~WorklistInterface() = default;

    /**
     * @brief Find worklist items matching the specified criteria
     * @param searchDataset The dataset containing search criteria
     * @return Result containing a list of matching worklist items
     */
    virtual Result<std::vector<DcmDataset*>> findWorklist(const DcmDataset* searchDataset) = 0;
    
    /**
     * @brief Add a new worklist item
     * @param dataset The worklist item dataset
     * @return Result with success or failure information
     */
    virtual Result<void> addWorklistItem(const DcmDataset* dataset) = 0;
    
    /**
     * @brief Update an existing worklist item
     * @param accessionNumber The accession number of the worklist item to update
     * @param dataset The updated worklist item dataset
     * @return Result with success or failure information
     */
    virtual Result<void> updateWorklistItem(const std::string& accessionNumber, const DcmDataset* dataset) = 0;
    
    /**
     * @brief Remove a worklist item
     * @param accessionNumber The accession number of the worklist item to remove
     * @return Result with success or failure information
     */
    virtual Result<void> removeWorklistItem(const std::string& accessionNumber) = 0;

    /**
     * @brief Set callback for worklist find operations
     * @param callback The callback function to invoke
     */
    virtual void setWorklistCallback(WorklistCallback callback) = 0;
};

} // namespace worklist
} // namespace interfaces
} // namespace core
} // namespace pacs