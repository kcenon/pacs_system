#pragma once

#include <string>
#include <memory>
#include <functional>

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
namespace mpps {

/**
 * @brief Callback function type definition for MPPS events
 */
using MPPSCallback = std::function<void(const std::string& accessionNumber, const DcmDataset* dataset)>;

/**
 * @brief Interface for MPPS (Modality Performed Procedure Step) operations
 */
class MPPSInterface {
public:
    virtual ~MPPSInterface() = default;

    /**
     * @brief Start a new MPPS with IN PROGRESS status
     * @param dataset The MPPS dataset
     * @return Result with success or failure information
     */
    virtual Result<void> createMPPS(const DcmDataset* dataset) = 0;

    /**
     * @brief Update an existing MPPS with COMPLETED or DISCONTINUED status
     * @param sopInstanceUID The SOP Instance UID of the MPPS to update
     * @param dataset The updated MPPS dataset
     * @return Result with success or failure information
     */
    virtual Result<void> updateMPPS(const std::string& sopInstanceUID, const DcmDataset* dataset) = 0;

    /**
     * @brief Set callback for MPPS N-CREATE notifications
     * @param callback The callback function to invoke
     */
    virtual void setCreateCallback(MPPSCallback callback) = 0;

    /**
     * @brief Set callback for MPPS N-SET notifications
     * @param callback The callback function to invoke
     */
    virtual void setUpdateCallback(MPPSCallback callback) = 0;
};

} // namespace mpps
} // namespace interfaces
} // namespace core
} // namespace pacs