#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <vector>

// DCMTK includes
#include "dcmtk/config/osconfig.h"
#include "dcmtk/dcmdata/dcdatset.h"
#include "dcmtk/dcmnet/dimse.h"
#include "dcmtk/dcmnet/diutil.h"
#include "dcmtk/dcmnet/assoc.h"
#include "dcmtk/dcmnet/cond.h"

#include "core/interfaces/worklist/worklist_interface.h"
#include "common/pacs_common.h"

namespace pacs {
namespace worklist {
namespace scu {

/**
 * @brief Worklist SCU (Service Class User) implementation
 * 
 * This class implements the Modality Worklist SCU role, which allows querying
 * a remote Modality Worklist SCP for scheduled procedures.
 */
class WorklistSCU : public core::interfaces::worklist::WorklistInterface {
public:
    /**
     * @brief Constructor
     * @param config Configuration for the Worklist SCU service
     */
    explicit WorklistSCU(const common::ServiceConfig& config);
    
    /**
     * @brief Destructor
     */
    ~WorklistSCU() override;
    
    /**
     * @brief Find worklist items matching the specified criteria
     * @param searchDataset The dataset containing search criteria
     * @return Result containing a list of matching worklist items
     */
    core::Result<std::vector<DcmDataset*>> findWorklist(const DcmDataset* searchDataset) override;
    
    /**
     * @brief Add a new worklist item (not used in SCU role)
     * @param dataset The worklist item dataset
     * @return Result with success or failure information
     */
    core::Result<void> addWorklistItem(const DcmDataset* dataset) override;
    
    /**
     * @brief Update an existing worklist item (not used in SCU role)
     * @param accessionNumber The accession number of the worklist item to update
     * @param dataset The updated worklist item dataset
     * @return Result with success or failure information
     */
    core::Result<void> updateWorklistItem(const std::string& accessionNumber, const DcmDataset* dataset) override;
    
    /**
     * @brief Remove a worklist item (not used in SCU role)
     * @param accessionNumber The accession number of the worklist item to remove
     * @return Result with success or failure information
     */
    core::Result<void> removeWorklistItem(const std::string& accessionNumber) override;
    
    /**
     * @brief Set callback for worklist find operations (not used in SCU role)
     * @param callback The callback function to invoke
     */
    void setWorklistCallback(core::interfaces::worklist::WorklistCallback callback) override;
    
private:
    /**
     * @brief Establish DICOM association with the Worklist SCP
     * @return Association pointer or nullptr on failure
     */
    T_ASC_Association* createAssociation();
    
    /**
     * @brief Release the DICOM association
     * @param assoc Association to release
     */
    void releaseAssociation(T_ASC_Association* assoc);
    
    common::ServiceConfig config_;
    std::mutex mutex_;
};

} // namespace scu
} // namespace worklist
} // namespace pacs