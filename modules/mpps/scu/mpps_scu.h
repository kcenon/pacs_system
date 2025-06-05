#pragma once

#include <string>
#include <memory>
#include <mutex>

// DCMTK includes
#include "dcmtk/config/osconfig.h"
#include "dcmtk/dcmdata/dcdatset.h"
#include "dcmtk/dcmnet/dimse.h"
#include "dcmtk/dcmnet/diutil.h"
#include "dcmtk/dcmnet/assoc.h"
#include "dcmtk/dcmnet/cond.h"

#include "core/interfaces/mpps/mpps_interface.h"
#include "common/pacs_common.h"

namespace pacs {
namespace mpps {
namespace scu {

/**
 * @brief MPPS SCU (Service Class User) implementation
 * 
 * This class implements the MPPS SCU role, which allows initiating and updating 
 * Modality Performed Procedure Step objects on a remote MPPS SCP.
 */
class MPPSSCU : public core::interfaces::mpps::MPPSInterface {
public:
    /**
     * @brief Constructor
     * @param config Configuration for the MPPS SCU service
     */
    explicit MPPSSCU(const common::ServiceConfig& config);
    
    /**
     * @brief Destructor
     */
    ~MPPSSCU() override;
    
    /**
     * @brief Start a new MPPS with IN PROGRESS status
     * @param dataset The MPPS dataset
     * @return Result with success or failure information
     */
    core::Result<void> createMPPS(const DcmDataset* dataset) override;
    
    /**
     * @brief Update an existing MPPS with COMPLETED or DISCONTINUED status
     * @param sopInstanceUID The SOP Instance UID of the MPPS to update
     * @param dataset The updated MPPS dataset
     * @return Result with success or failure information
     */
    core::Result<void> updateMPPS(const std::string& sopInstanceUID, const DcmDataset* dataset) override;
    
    /**
     * @brief Set callback for MPPS N-CREATE notifications (not used in SCU role)
     * @param callback The callback function to invoke
     */
    void setCreateCallback(core::interfaces::mpps::MPPSCallback callback) override;
    
    /**
     * @brief Set callback for MPPS N-SET notifications (not used in SCU role)
     * @param callback The callback function to invoke
     */
    void setUpdateCallback(core::interfaces::mpps::MPPSCallback callback) override;
    
private:
    /**
     * @brief Establish DICOM association with the MPPS SCP
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
} // namespace mpps
} // namespace pacs