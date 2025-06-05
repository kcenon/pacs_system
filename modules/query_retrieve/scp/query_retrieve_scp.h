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
#include "dcmtk/dcmqrdb/dcmqrdba.h"

#include "core/interfaces/query_retrieve/query_retrieve_interface.h"
#include "common/pacs_common.h"

namespace pacs {
namespace query_retrieve {
namespace scp {

/**
 * @brief Query/Retrieve SCP (Service Class Provider) implementation
 * 
 * This class implements the Query/Retrieve SCP role, which responds to
 * queries and retrieval requests from remote Query/Retrieve SCUs.
 */
class QueryRetrieveSCP : public core::interfaces::query_retrieve::QueryRetrieveInterface {
public:
    /**
     * @brief Constructor
     * @param config Configuration for the Query/Retrieve SCP service
     * @param storageDirectory Directory where DICOM files are stored
     */
    explicit QueryRetrieveSCP(const common::ServiceConfig& config, const std::string& storageDirectory = "");
    
    /**
     * @brief Destructor
     */
    ~QueryRetrieveSCP() override;
    
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
     * @brief Add a new DICOM file to the database
     * @param filename Path to the DICOM file to add
     * @return Result with success or failure information
     */
    core::Result<void> addFile(const std::string& filename);
    
    /**
     * @brief Query for DICOM objects matching the specified criteria
     * @param searchDataset The dataset containing search criteria
     * @param level The query retrieve level (PATIENT, STUDY, SERIES, IMAGE)
     * @return Result containing a list of matching datasets
     */
    core::Result<std::vector<DcmDataset*>> query(const DcmDataset* searchDataset, 
                                             core::interfaces::query_retrieve::QueryRetrieveLevel level) override;
    
    /**
     * @brief Retrieve DICOM objects (not used in SCP role)
     * @param studyInstanceUID Study Instance UID
     * @param seriesInstanceUID Series Instance UID (optional)
     * @param sopInstanceUID SOP Instance UID (optional)
     * @return Result with success or failure information
     */
    core::Result<void> retrieve(const std::string& studyInstanceUID,
                            const std::string& seriesInstanceUID = "",
                            const std::string& sopInstanceUID = "") override;
    
    /**
     * @brief Set callback for query operations
     * @param callback The callback function to invoke
     */
    void setQueryCallback(core::interfaces::query_retrieve::QueryCallback callback) override;
    
    /**
     * @brief Set callback for retrieve operations
     * @param callback The callback function to invoke
     */
    void setRetrieveCallback(core::interfaces::query_retrieve::RetrieveCallback callback) override;
    
    /**
     * @brief Set callback for move operations
     * @param callback The callback function to invoke
     */
    void setMoveCallback(core::interfaces::query_retrieve::MoveCallback callback) override;
    
    /**
     * @brief Set the directory where DICOM files are stored
     * @param directory Directory path
     */
    void setStorageDirectory(const std::string& directory);
    
private:
    /**
     * @brief SCP server main loop
     */
    void serverLoop();
    
#ifndef DCMTK_NOT_AVAILABLE
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
     * @brief Handle C-MOVE request
     * @param assoc The association
     * @param request The C-MOVE request message
     * @param presID The presentation context ID
     * @param dataset The dataset
     */
    void handleCMoveRequest(T_ASC_Association* assoc, T_DIMSE_Message& request, 
                           T_ASC_PresentationContextID presID, DcmDataset* dataset);
    
    /**
     * @brief Handle C-GET request
     * @param assoc The association
     * @param request The C-GET request message
     * @param presID The presentation context ID
     * @param dataset The dataset
     */
    void handleCGetRequest(T_ASC_Association* assoc, T_DIMSE_Message& request, 
                          T_ASC_PresentationContextID presID, DcmDataset* dataset);
    
    /**
     * @brief Match a dataset against search criteria
     * @param searchDataset The search criteria dataset
     * @param candidateDataset The candidate dataset to match
     * @return true if the dataset matches the criteria, false otherwise
     */
    bool matchDataset(const DcmDataset* searchDataset, const DcmDataset* candidateDataset);
    
    /**
     * @brief Extract basic metadata for indexing
     * @param dataset DICOM dataset
     * @return Map of key attributes for indexing
     */
    std::map<std::string, std::string> extractMetadata(const DcmDataset* dataset);
#else
    /**
     * @brief Extract basic metadata for indexing (simplified version when DCMTK is not available)
     * @param filename Path to the DICOM file
     * @return Map of key attributes for indexing
     */
    std::map<std::string, std::string> extractMetadata(const std::string& filename);
#endif

    /**
     * @brief Index all DICOM files in the storage directory
     */
    void indexStorageDirectory();
    
    common::ServiceConfig config_;
    std::string storageDirectory_;
    std::thread serverThread_;
    std::atomic<bool> running_;
    
    // Database of indexed DICOM files (filename -> metadata)
    std::map<std::string, std::map<std::string, std::string>> dicomIndex_;
    std::mutex indexMutex_;
    
    // Callbacks
    core::interfaces::query_retrieve::QueryCallback queryCallback_;
    core::interfaces::query_retrieve::RetrieveCallback retrieveCallback_;
    core::interfaces::query_retrieve::MoveCallback moveCallback_;
    std::mutex callbackMutex_;
};

} // namespace scp
} // namespace query_retrieve
} // namespace pacs