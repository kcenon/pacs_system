// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file storage_interface.cpp
 * @brief Default implementations for storage_interface batch operations
 */

#include <kcenon/pacs/storage/storage_interface.h>
#include <kcenon/pacs/core/encapsulated_pixel_data.h>

namespace kcenon::pacs::storage {

// Use common_system's ok() function
using kcenon::common::ok;
using kcenon::common::make_error;

auto storage_interface::native_store_syntax(const core::dicom_dataset& dataset)
    -> Result<encoding::transfer_syntax> {
    const auto* pixel = dataset.get(core::tags::pixel_data);
    if (pixel && (pixel->is_encapsulated_pixel_data()
                  || core::inspect_encapsulated_pixel_data(pixel->raw_data()))) {
        return make_error<encoding::transfer_syntax>(
            kcenon::pacs::error_codes::unsupported_transfer_syntax,
            "Pixel Data contains encapsulated or ambiguous items; supply its transfer syntax explicitly",
            "storage_interface");
    }
    return encoding::transfer_syntax::explicit_vr_little_endian;
}

auto storage_interface::store(const core::dicom_dataset& dataset,
                              const encoding::transfer_syntax& syntax) -> VoidResult {
    auto validation = core::dicom_file::validate_pixel_data_representation(dataset,syntax);
    if (validation.is_err()) return validation;
    if (syntax != encoding::transfer_syntax::explicit_vr_little_endian) {
        return make_error<std::monostate>(kcenon::pacs::error_codes::unsupported_transfer_syntax,
            "This storage backend does not implement explicit transfer syntax preservation", "storage_interface");
    }
    return store(dataset);
}

auto storage_interface::retrieve_file(std::string_view uid) -> Result<core::dicom_file> {
    auto dataset = retrieve(uid);
    if (dataset.is_err()) return dataset.error();
    auto syntax = native_store_syntax(dataset.value());
    if (syntax.is_err()) return syntax.error();
    return core::dicom_file::create(std::move(dataset.value()),syntax.value());
}

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

}  // namespace kcenon::pacs::storage
