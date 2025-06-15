#pragma once

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

// DCMTK includes
#ifdef HAVE_CONFIG_H
#include "dcmtk/config/osconfig.h"
#endif
#include "dcmtk/dcmdata/dcdatset.h"
#include "dcmtk/dcmnet/dimse.h"
#include "dcmtk/dcmnet/diutil.h"
#include "dcmtk/dcmnet/assoc.h"
#include "dcmtk/dcmnet/cond.h"

#include "core/interfaces/mpps/mpps_interface.h"
#include "common/pacs_common.h"

namespace pacs {
namespace mpps {
namespace scp {

/**
 * @brief MPPS SCP (Service Class Provider) implementation
 * 
 * This class implements the MPPS SCP role, which listens for and processes
 * Modality Performed Procedure Step objects sent by remote MPPS SCUs.
 */
class MPPSSCP : public core::interfaces::mpps::MPPSInterface {
public:
    /**
     * @brief Constructor
     * @param config Configuration for the MPPS SCP service
     */
    explicit MPPSSCP(const common::ServiceConfig& config);
    
    /**
     * @brief Destructor
     */
    ~MPPSSCP() override;
    
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
     * @brief Start a new MPPS with IN PROGRESS status (not used in SCP role)
     * @param dataset The MPPS dataset
     * @return Result with success or failure information
     */
    core::Result<void> createMPPS(const DcmDataset* dataset) override;
    
    /**
     * @brief Update an existing MPPS with COMPLETED or DISCONTINUED status (not used in SCP role)
     * @param sopInstanceUID The SOP Instance UID of the MPPS to update
     * @param dataset The updated MPPS dataset
     * @return Result with success or failure information
     */
    core::Result<void> updateMPPS(const std::string& sopInstanceUID, const DcmDataset* dataset) override;
    
    /**
     * @brief Set callback for MPPS N-CREATE notifications
     * @param callback The callback function to invoke
     */
    void setCreateCallback(core::interfaces::mpps::MPPSCallback callback) override;
    
    /**
     * @brief Set callback for MPPS N-SET notifications
     * @param callback The callback function to invoke
     */
    void setUpdateCallback(core::interfaces::mpps::MPPSCallback callback) override;
    
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
     * @brief Handle N-CREATE request
     * @param assoc The association
     * @param request The N-CREATE request message
     * @param presID The presentation context ID
     * @param dataset The dataset
     */
    void handleNCreateRequest(T_ASC_Association* assoc, T_DIMSE_Message& request, 
                             T_ASC_PresentationContextID presID, DcmDataset* dataset);
    
    /**
     * @brief Handle N-SET request
     * @param assoc The association
     * @param request The N-SET request message
     * @param presID The presentation context ID
     * @param dataset The dataset
     */
    void handleNSetRequest(T_ASC_Association* assoc, T_DIMSE_Message& request, 
                          T_ASC_PresentationContextID presID, DcmDataset* dataset);
    
    common::ServiceConfig config_;
    std::thread serverThread_;
    std::atomic<bool> running_;
    
    core::interfaces::mpps::MPPSCallback createCallback_;
    core::interfaces::mpps::MPPSCallback updateCallback_;
    
    std::mutex callbackMutex_;
};

} // namespace scp
} // namespace mpps
} // namespace pacs