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
 * @file pool_manager.cpp
 * @brief Implementation of centralized object pool management
 */

#include <pacs/core/pool_manager.hpp>

namespace kcenon::pacs::core {

// ============================================================================
// pool_manager Implementation
// ============================================================================

pool_manager::pool_manager()
    : element_pool_(DEFAULT_ELEMENT_POOL_SIZE)
    , dataset_pool_(DEFAULT_DATASET_POOL_SIZE) {}

auto pool_manager::get() noexcept -> pool_manager& {
    static pool_manager instance;
    return instance;
}

auto pool_manager::acquire_element(dicom_tag tag, encoding::vr_type vr)
    -> tracked_pool<dicom_element>::unique_ptr_type {
    return element_pool_.acquire(tag, vr);
}

auto pool_manager::acquire_dataset()
    -> tracked_pool<dicom_dataset>::unique_ptr_type {
    return dataset_pool_.acquire();
}

auto pool_manager::element_statistics() const noexcept -> const pool_statistics& {
    return element_pool_.statistics();
}

auto pool_manager::dataset_statistics() const noexcept -> const pool_statistics& {
    return dataset_pool_.statistics();
}

void pool_manager::reserve_elements(std::size_t count) {
    element_pool_.reserve(count);
}

void pool_manager::reserve_datasets(std::size_t count) {
    dataset_pool_.reserve(count);
}

void pool_manager::clear_all() {
    element_pool_.clear();
    dataset_pool_.clear();
}

void pool_manager::reset_statistics() {
    const_cast<pool_statistics&>(element_pool_.statistics()).reset();
    const_cast<pool_statistics&>(dataset_pool_.statistics()).reset();
}

}  // namespace kcenon::pacs::core
