#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <vector>

// DCMTK includes
#ifdef HAVE_CONFIG_H
#include "dcmtk/config/osconfig.h"
#endif
#include "dcmtk/dcmdata/dcdatset.h"
#include "dcmtk/dcmnet/dimse.h"
#include "dcmtk/dcmnet/diutil.h"
#include "dcmtk/dcmnet/assoc.h"
#include "dcmtk/dcmnet/cond.h"

#include "core/interfaces/query_retrieve/query_retrieve_interface.h"
#include "common/pacs_common.h"

namespace pacs {
namespace query_retrieve {
namespace scu {

/**
 * @brief Query/Retrieve SCU (Service Class User) implementation
 * 
 * This class implements the Query/Retrieve SCU role, which allows querying
 * and retrieving DICOM objects from a remote Query/Retrieve SCP.
 */
class QueryRetrieveSCU : public core::interfaces::query_retrieve::QueryRetrieveInterface {
public:
    /**
     * @brief Constructor
     * @param config Configuration for the Query/Retrieve SCU service
     * @param retrieveDirectory Directory to store retrieved DICOM files
     */
    explicit QueryRetrieveSCU(const common::ServiceConfig& config, 
                           const std::string& retrieveDirectory = "");
    
    /**
     * @brief Destructor
     */
    ~QueryRetrieveSCU() override;
    
    /**
     * @brief Query for DICOM objects matching the specified criteria
     * @param searchDataset The dataset containing search criteria
     * @param level The query retrieve level (PATIENT, STUDY, SERIES, IMAGE)
     * @return Result containing a list of matching datasets
     */
    core::Result<std::vector<DcmDataset*>> query(const DcmDataset* searchDataset, 
                                             core::interfaces::query_retrieve::QueryRetrieveLevel level) override;
    
    /**
     * @brief Retrieve DICOM objects
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
     * @brief Set the directory where retrieved DICOM files will be stored
     * @param directory Directory path
     */
    void setRetrieveDirectory(const std::string& directory);
    
private:
    /**
     * @brief Establish DICOM association with the Query/Retrieve SCP
     * @param queryRetrieveModel The Query/Retrieve Information Model to use
     * @return Association pointer or nullptr on failure
     */
    T_ASC_Association* createAssociation(const char* queryRetrieveModel);
    
    /**
     * @brief Release the DICOM association
     * @param assoc Association to release
     */
    void releaseAssociation(T_ASC_Association* assoc);
    
    /**
     * @brief Execute C-MOVE operation
     * @param studyInstanceUID Study Instance UID
     * @param seriesInstanceUID Series Instance UID (optional)
     * @param sopInstanceUID SOP Instance UID (optional)
     * @param queryRetrieveLevel The query/retrieve level
     * @return Result with success or failure information
     */
    core::Result<void> executeCMove(const std::string& studyInstanceUID,
                                const std::string& seriesInstanceUID,
                                const std::string& sopInstanceUID,
                                const std::string& queryRetrieveLevel);
    
    /**
     * @brief Get the appropriate information model for a query level
     * @param level The query retrieve level
     * @return The DICOM UID for the information model
     */
    const char* getInformationModelForLevel(core::interfaces::query_retrieve::QueryRetrieveLevel level);
    
    /**
     * @brief Parse a query result into a QueryResultItem structure
     * @param dataset The DICOM dataset
     * @param level The query level
     * @return Parsed QueryResultItem
     */
    core::interfaces::query_retrieve::QueryResultItem parseQueryResult(const DcmDataset* dataset, 
                                                                   core::interfaces::query_retrieve::QueryRetrieveLevel level);

    /**
     * @brief Get the string representation of a query level
     * @param level The query retrieve level
     * @return String representation of the level
     */
    std::string getQueryLevelString(core::interfaces::query_retrieve::QueryRetrieveLevel level);
    
    common::ServiceConfig config_;
    std::string retrieveDirectory_;
    std::mutex mutex_;
    
    core::interfaces::query_retrieve::QueryCallback queryCallback_;
    core::interfaces::query_retrieve::RetrieveCallback retrieveCallback_;
    core::interfaces::query_retrieve::MoveCallback moveCallback_;
};

} // namespace scu
} // namespace query_retrieve
} // namespace pacs