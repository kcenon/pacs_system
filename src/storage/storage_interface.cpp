// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @file storage_interface.cpp
 * @brief Default implementations for storage_interface batch operations
 */

#include <pacs/storage/storage_interface.hpp>

namespace kcenon::pacs::storage {

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

}  // namespace kcenon::pacs::storage
