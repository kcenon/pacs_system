/**
 * @file storage_interface.cpp
 * @brief Default implementations for storage_interface batch operations
 */

#include <pacs/storage/storage_interface.hpp>

namespace pacs::storage {

// Use common_system's ok() function
using kcenon::common::ok;
using kcenon::common::make_error;

// ============================================================================
// Default Batch Operation Implementations
// ============================================================================

auto storage_interface::store_batch(
    const std::vector<core::dicom_dataset>& datasets) -> VoidResult {
    for (const auto& dataset : datasets) {
        auto result = store(dataset);
        if (result.is_err()) {
            return result;
        }
    }
    return ok();
}

auto storage_interface::retrieve_batch(
    const std::vector<std::string>& sop_instance_uids)
    -> Result<std::vector<core::dicom_dataset>> {
    std::vector<core::dicom_dataset> results;
    results.reserve(sop_instance_uids.size());

    for (const auto& uid : sop_instance_uids) {
        auto result = retrieve(uid);
        if (result.is_ok()) {
            results.push_back(std::move(result.value()));
        }
        // Silently skip missing instances as per interface contract
    }

    return results;
}

}  // namespace pacs::storage
