/**
 * @file pool_manager.hpp
 * @brief Centralized object pool management for DICOM objects
 *
 * This file provides thread-safe object pooling for frequently allocated
 * DICOM objects to reduce allocation overhead and memory fragmentation.
 *
 * @see Issue #315 - ObjectPool Memory Management
 */

#pragma once

#include "dicom_element.hpp"
#include "dicom_dataset.hpp"

#include <kcenon/common/utils/object_pool.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>

namespace pacs::core {

/**
 * @brief Statistics for object pool usage monitoring
 */
struct pool_statistics {
    std::atomic<uint64_t> total_acquisitions{0};
    std::atomic<uint64_t> pool_hits{0};
    std::atomic<uint64_t> pool_misses{0};
    std::atomic<uint64_t> total_releases{0};

    /**
     * @brief Calculate hit ratio (0.0 to 1.0)
     * @return Hit ratio, or 0.0 if no acquisitions
     */
    [[nodiscard]] auto hit_ratio() const noexcept -> double {
        const uint64_t total = total_acquisitions.load(std::memory_order_relaxed);
        if (total == 0) {
            return 0.0;
        }
        return static_cast<double>(pool_hits.load(std::memory_order_relaxed))
               / static_cast<double>(total);
    }

    /**
     * @brief Reset all statistics
     */
    void reset() noexcept {
        total_acquisitions.store(0, std::memory_order_relaxed);
        pool_hits.store(0, std::memory_order_relaxed);
        pool_misses.store(0, std::memory_order_relaxed);
        total_releases.store(0, std::memory_order_relaxed);
    }
};

/**
 * @brief Pool wrapper with statistics tracking
 * @tparam T The type of objects to pool
 */
template <typename T>
class tracked_pool {
public:
    using pool_type = kcenon::common::utils::ObjectPool<T>;
    using unique_ptr_type = std::unique_ptr<T, std::function<void(T*)>>;

    explicit tracked_pool(std::size_t initial_size = 32)
        : pool_(initial_size) {
        pool_.reserve(initial_size);
    }

    /**
     * @brief Acquire an object from the pool
     * @param args Arguments forwarded to the object's constructor
     * @return A unique_ptr that returns the object to the pool on destruction
     */
    template <typename... Args>
    auto acquire(Args&&... args) -> unique_ptr_type {
        bool reused = false;
        auto ptr = pool_.acquire(&reused, std::forward<Args>(args)...);

        stats_.total_acquisitions.fetch_add(1, std::memory_order_relaxed);
        if (reused) {
            stats_.pool_hits.fetch_add(1, std::memory_order_relaxed);
        } else {
            stats_.pool_misses.fetch_add(1, std::memory_order_relaxed);
        }

        return ptr;
    }

    /**
     * @brief Get the pool statistics
     * @return Reference to the statistics
     */
    [[nodiscard]] auto statistics() const noexcept -> const pool_statistics& {
        return stats_;
    }

    /**
     * @brief Get the number of available objects in the pool
     * @return Available object count
     */
    [[nodiscard]] auto available() const -> std::size_t {
        return pool_.available();
    }

    /**
     * @brief Reserve additional capacity in the pool
     * @param count Number of objects to pre-allocate
     */
    void reserve(std::size_t count) {
        pool_.reserve(count);
    }

    /**
     * @brief Clear the pool and release all memory
     */
    void clear() {
        pool_.clear();
    }

private:
    pool_type pool_;
    pool_statistics stats_;
};

/**
 * @brief Centralized pool manager for DICOM objects
 *
 * Provides thread-safe access to object pools for common DICOM types.
 * Uses a singleton pattern for global access while allowing explicit
 * pool instances for testing.
 *
 * @example
 * @code
 * // Acquire a pooled element
 * auto elem = pool_manager::get().acquire_element(tag, vr);
 *
 * // Acquire a pooled dataset
 * auto ds = pool_manager::get().acquire_dataset();
 *
 * // Objects are automatically returned to the pool when destroyed
 * @endcode
 */
class pool_manager {
public:
    /// Default pool sizes
    static constexpr std::size_t DEFAULT_ELEMENT_POOL_SIZE = 1024;
    static constexpr std::size_t DEFAULT_DATASET_POOL_SIZE = 128;

    /**
     * @brief Get the global pool manager instance
     * @return Reference to the singleton instance
     */
    static auto get() noexcept -> pool_manager&;

    /**
     * @brief Acquire a dicom_element from the pool
     * @param tag The DICOM tag
     * @param vr The value representation
     * @return A pooled dicom_element
     */
    auto acquire_element(dicom_tag tag, encoding::vr_type vr)
        -> tracked_pool<dicom_element>::unique_ptr_type;

    /**
     * @brief Acquire a dicom_dataset from the pool
     * @return A pooled dicom_dataset
     */
    auto acquire_dataset()
        -> tracked_pool<dicom_dataset>::unique_ptr_type;

    /**
     * @brief Get element pool statistics
     * @return Reference to element pool statistics
     */
    [[nodiscard]] auto element_statistics() const noexcept -> const pool_statistics&;

    /**
     * @brief Get dataset pool statistics
     * @return Reference to dataset pool statistics
     */
    [[nodiscard]] auto dataset_statistics() const noexcept -> const pool_statistics&;

    /**
     * @brief Reserve capacity in element pool
     * @param count Number of elements to pre-allocate
     */
    void reserve_elements(std::size_t count);

    /**
     * @brief Reserve capacity in dataset pool
     * @param count Number of datasets to pre-allocate
     */
    void reserve_datasets(std::size_t count);

    /**
     * @brief Clear all pools
     */
    void clear_all();

    /**
     * @brief Reset all statistics
     */
    void reset_statistics();

    // Non-copyable, non-movable
    pool_manager(const pool_manager&) = delete;
    pool_manager(pool_manager&&) = delete;
    auto operator=(const pool_manager&) -> pool_manager& = delete;
    auto operator=(pool_manager&&) -> pool_manager& = delete;

private:
    pool_manager();
    ~pool_manager() = default;

    tracked_pool<dicom_element> element_pool_;
    tracked_pool<dicom_dataset> dataset_pool_;
};

// ============================================================================
// Convenience Factory Functions
// ============================================================================

/**
 * @brief Create a pooled dicom_element
 *
 * This function provides a convenient way to create pooled elements.
 * The returned object is automatically returned to the pool when destroyed.
 *
 * @param tag The DICOM tag
 * @param vr The value representation
 * @return A unique_ptr to the pooled element
 */
[[nodiscard]] inline auto make_pooled_element(dicom_tag tag, encoding::vr_type vr)
    -> tracked_pool<dicom_element>::unique_ptr_type {
    return pool_manager::get().acquire_element(tag, vr);
}

/**
 * @brief Create a pooled dicom_element with string value
 *
 * @param tag The DICOM tag
 * @param vr The value representation
 * @param value The string value
 * @return A unique_ptr to the pooled element
 */
[[nodiscard]] inline auto make_pooled_element(dicom_tag tag, encoding::vr_type vr,
                                              std::string_view value)
    -> tracked_pool<dicom_element>::unique_ptr_type {
    auto elem = pool_manager::get().acquire_element(tag, vr);
    elem->set_string(value);
    return elem;
}

/**
 * @brief Create a pooled dicom_dataset
 *
 * This function provides a convenient way to create pooled datasets.
 * The returned object is automatically returned to the pool when destroyed.
 *
 * @return A unique_ptr to the pooled dataset
 */
[[nodiscard]] inline auto make_pooled_dataset()
    -> tracked_pool<dicom_dataset>::unique_ptr_type {
    return pool_manager::get().acquire_dataset();
}

}  // namespace pacs::core
