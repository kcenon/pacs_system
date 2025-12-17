/**
 * @file pool_manager.cpp
 * @brief Implementation of centralized object pool management
 */

#include <pacs/core/pool_manager.hpp>

namespace pacs::core {

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

}  // namespace pacs::core
