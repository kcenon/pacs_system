#pragma once

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <map>

// DCMTK includes
#include "dcmtk/config/osconfig.h"
#include "dcmtk/dcmdata/dcdatset.h"
#include "dcmtk/dcmnet/dimse.h"
#include "dcmtk/dcmnet/diutil.h"
#include "dcmtk/dcmnet/assoc.h"
#include "dcmtk/dcmnet/cond.h"
#include "dcmtk/dcmwlm/wldsfs.h"

// Include for unique_ptr
#include <memory>

#include "core/interfaces/worklist/worklist_interface.h"
#include "common/pacs_common.h"

namespace pacs {
namespace worklist {
namespace scp {

/**
 * @brief Worklist SCP (Service Class Provider) implementation
 * 
 * This class implements the Modality Worklist SCP role, which responds to
 * queries from remote Modality Worklist SCUs for scheduled procedures.
 */
class WorklistSCP : public core::interfaces::worklist::WorklistInterface {
public:
    /**
     * @brief Constructor
     * @param config Configuration for the Worklist SCP service
     * @param worklistDirectory Directory where worklist files are stored (optional)
     */
    explicit WorklistSCP(const common::ServiceConfig& config, const std::string& worklistDirectory = "");
    
    /**
     * @brief Destructor
     */
    ~WorklistSCP() override;
    
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
     * @brief Find worklist items matching the specified criteria (used by C-FIND handler)
     * @param searchDataset The dataset containing search criteria
     * @return Result containing a list of matching worklist items
     */
    core::Result<std::vector<DcmDataset*>> findWorklist(const DcmDataset* searchDataset) override;
    
    /**
     * @brief Add a new worklist item
     * @param dataset The worklist item dataset
     * @return Result with success or failure information
     */
    core::Result<void> addWorklistItem(const DcmDataset* dataset) override;
    
    /**
     * @brief Update an existing worklist item
     * @param accessionNumber The accession number of the worklist item to update
     * @param dataset The updated worklist item dataset
     * @return Result with success or failure information
     */
    core::Result<void> updateWorklistItem(const std::string& accessionNumber, const DcmDataset* dataset) override;
    
    /**
     * @brief Remove a worklist item
     * @param accessionNumber The accession number of the worklist item to remove
     * @return Result with success or failure information
     */
    core::Result<void> removeWorklistItem(const std::string& accessionNumber) override;
    
    /**
     * @brief Set callback for worklist find operations
     * @param callback The callback function to invoke
     */
    void setWorklistCallback(core::interfaces::worklist::WorklistCallback callback) override;
    
    /**
     * @brief Set the directory where worklist files are stored
     * @param directory Directory path
     */
    void setWorklistDirectory(const std::string& directory);
    
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
     * @brief Handle C-FIND request
     * @param assoc The association
     * @param request The C-FIND request message
     * @param presID The presentation context ID
     * @param dataset The dataset
     */
    void handleCFindRequest(T_ASC_Association* assoc, T_DIMSE_Message& request, 
                           T_ASC_PresentationContextID presID, DcmDataset* dataset);
    
    /**
     * @brief Match a worklist item against search criteria
     * @param searchDataset The search criteria dataset
     * @param worklistDataset The worklist item dataset to match
     * @return true if the worklist item matches the criteria, false otherwise
     */
    bool matchWorklistItem(const DcmDataset* searchDataset, const DcmDataset* worklistDataset);
    
    /**
     * @brief Load worklist items from the worklist directory
     */
    void loadWorklistItems();
    
    /**
     * @brief Extract WorklistItem structure from a DICOM dataset
     * @param dataset The DICOM dataset
     * @return Extracted WorklistItem structure
     */
    core::interfaces::worklist::WorklistItem extractWorklistItem(const DcmDataset* dataset);
    
    common::ServiceConfig config_;
    std::string worklistDirectory_;
    std::thread serverThread_;
    std::atomic<bool> running_;
    
    // Map of worklist items keyed by accession number
    std::map<std::string, DcmDataset*> worklistItems_; // Keyed by accession number
    std::mutex worklistMutex_;
    
    core::interfaces::worklist::WorklistCallback worklistCallback_;
    std::mutex callbackMutex_;
};

} // namespace scp
} // namespace worklist
} // namespace pacs