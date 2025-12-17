/**
 * @file pdu_buffer_pool.hpp
 * @brief Object pooling for PDU buffers and network data structures
 *
 * This file provides pooled allocation for PDU-related data structures
 * to reduce allocation overhead during network operations.
 *
 * @see Issue #315, #318 - ObjectPool Memory Management
 */

#pragma once

#include "pdu_types.hpp"
#include "pdu_decoder.hpp"

#include <kcenon/common/utils/object_pool.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace pacs::network {

/**
 * @brief Statistics for PDU buffer pool usage monitoring
 */
struct pdu_pool_statistics {
    std::atomic<uint64_t> total_acquisitions{0};
    std::atomic<uint64_t> pool_hits{0};
    std::atomic<uint64_t> pool_misses{0};
    std::atomic<uint64_t> total_releases{0};
    std::atomic<uint64_t> total_bytes_allocated{0};

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
        total_bytes_allocated.store(0, std::memory_order_relaxed);
    }
};

/**
 * @brief Pooled byte buffer for PDU data
 *
 * Wraps a std::vector<uint8_t> with pooled allocation semantics.
 * The buffer can be resized without reallocating if the capacity
 * is sufficient.
 */
class pooled_buffer {
public:
    pooled_buffer() = default;

    /**
     * @brief Clear the buffer contents (keeps capacity)
     */
    void clear() noexcept { data_.clear(); }

    /**
     * @brief Resize the buffer
     * @param new_size New size in bytes
     */
    void resize(std::size_t new_size) { data_.resize(new_size); }

    /**
     * @brief Reserve capacity
     * @param capacity Capacity to reserve
     */
    void reserve(std::size_t capacity) { data_.reserve(capacity); }

    /**
     * @brief Get the current size
     * @return Size in bytes
     */
    [[nodiscard]] auto size() const noexcept -> std::size_t {
        return data_.size();
    }

    /**
     * @brief Get the current capacity
     * @return Capacity in bytes
     */
    [[nodiscard]] auto capacity() const noexcept -> std::size_t {
        return data_.capacity();
    }

    /**
     * @brief Get raw pointer to data
     * @return Pointer to data
     */
    [[nodiscard]] auto data() noexcept -> uint8_t* { return data_.data(); }

    /**
     * @brief Get const raw pointer to data
     * @return Const pointer to data
     */
    [[nodiscard]] auto data() const noexcept -> const uint8_t* {
        return data_.data();
    }

    /**
     * @brief Get reference to underlying vector
     * @return Reference to vector
     */
    [[nodiscard]] auto vector() noexcept -> std::vector<uint8_t>& {
        return data_;
    }

    /**
     * @brief Get const reference to underlying vector
     * @return Const reference to vector
     */
    [[nodiscard]] auto vector() const noexcept -> const std::vector<uint8_t>& {
        return data_;
    }

    /**
     * @brief Array access operator
     */
    [[nodiscard]] auto operator[](std::size_t index) -> uint8_t& {
        return data_[index];
    }

    /**
     * @brief Const array access operator
     */
    [[nodiscard]] auto operator[](std::size_t index) const -> const uint8_t& {
        return data_[index];
    }

private:
    std::vector<uint8_t> data_;
};

/**
 * @brief Pool wrapper with statistics tracking for PDU buffers
 * @tparam T The type of objects to pool
 */
template <typename T>
class tracked_pdu_pool {
public:
    using pool_type = kcenon::common::utils::ObjectPool<T>;
    using unique_ptr_type = std::unique_ptr<T, std::function<void(T*)>>;

    explicit tracked_pdu_pool(std::size_t initial_size = 64)
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
    [[nodiscard]] auto statistics() const noexcept -> const pdu_pool_statistics& {
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
    pdu_pool_statistics stats_;
};

/**
 * @brief Centralized pool manager for PDU buffers
 *
 * Provides thread-safe access to object pools for PDU-related types.
 * Uses a singleton pattern for global access.
 *
 * @example
 * @code
 * // Acquire a pooled buffer
 * auto buffer = pdu_buffer_pool::get().acquire_buffer();
 * buffer->resize(16384);
 *
 * // Acquire a pooled PDV
 * auto pdv = pdu_buffer_pool::get().acquire_pdv(context_id, true, false);
 *
 * // Objects are automatically returned to the pool when destroyed
 * @endcode
 */
class pdu_buffer_pool {
public:
    /// Default pool sizes
    static constexpr std::size_t DEFAULT_BUFFER_POOL_SIZE = 256;
    static constexpr std::size_t DEFAULT_PDV_POOL_SIZE = 128;
    static constexpr std::size_t DEFAULT_PDATA_POOL_SIZE = 64;

    /**
     * @brief Get the global PDU buffer pool instance
     * @return Reference to the singleton instance
     */
    static auto get() noexcept -> pdu_buffer_pool&;

    /**
     * @brief Acquire a byte buffer from the pool
     * @return A pooled byte buffer
     */
    auto acquire_buffer()
        -> tracked_pdu_pool<pooled_buffer>::unique_ptr_type;

    /**
     * @brief Acquire a presentation_data_value from the pool
     * @param context_id Presentation Context ID
     * @param is_command Whether this is a command message
     * @param is_last Whether this is the last fragment
     * @return A pooled presentation_data_value
     */
    auto acquire_pdv(uint8_t context_id = 0, bool is_command = false,
                     bool is_last = true)
        -> tracked_pdu_pool<presentation_data_value>::unique_ptr_type;

    /**
     * @brief Acquire a p_data_tf_pdu from the pool
     * @return A pooled p_data_tf_pdu
     */
    auto acquire_p_data_tf()
        -> tracked_pdu_pool<p_data_tf_pdu>::unique_ptr_type;

    /**
     * @brief Get buffer pool statistics
     * @return Reference to buffer pool statistics
     */
    [[nodiscard]] auto buffer_statistics() const noexcept
        -> const pdu_pool_statistics&;

    /**
     * @brief Get PDV pool statistics
     * @return Reference to PDV pool statistics
     */
    [[nodiscard]] auto pdv_statistics() const noexcept
        -> const pdu_pool_statistics&;

    /**
     * @brief Get P-DATA-TF pool statistics
     * @return Reference to P-DATA-TF pool statistics
     */
    [[nodiscard]] auto p_data_statistics() const noexcept
        -> const pdu_pool_statistics&;

    /**
     * @brief Reserve capacity in buffer pool
     * @param count Number of buffers to pre-allocate
     */
    void reserve_buffers(std::size_t count);

    /**
     * @brief Reserve capacity in PDV pool
     * @param count Number of PDVs to pre-allocate
     */
    void reserve_pdvs(std::size_t count);

    /**
     * @brief Clear all pools
     */
    void clear_all();

    /**
     * @brief Reset all statistics
     */
    void reset_statistics();

    // Non-copyable, non-movable
    pdu_buffer_pool(const pdu_buffer_pool&) = delete;
    pdu_buffer_pool(pdu_buffer_pool&&) = delete;
    auto operator=(const pdu_buffer_pool&) -> pdu_buffer_pool& = delete;
    auto operator=(pdu_buffer_pool&&) -> pdu_buffer_pool& = delete;

private:
    pdu_buffer_pool();
    ~pdu_buffer_pool() = default;

    tracked_pdu_pool<pooled_buffer> buffer_pool_;
    tracked_pdu_pool<presentation_data_value> pdv_pool_;
    tracked_pdu_pool<p_data_tf_pdu> p_data_pool_;
};

// ============================================================================
// Convenience Factory Functions
// ============================================================================

/**
 * @brief Create a pooled byte buffer
 * @return A unique_ptr to the pooled buffer
 */
[[nodiscard]] inline auto make_pooled_pdu_buffer()
    -> tracked_pdu_pool<pooled_buffer>::unique_ptr_type {
    return pdu_buffer_pool::get().acquire_buffer();
}

/**
 * @brief Create a pooled byte buffer with initial size
 * @param size Initial size of the buffer
 * @return A unique_ptr to the pooled buffer
 */
[[nodiscard]] inline auto make_pooled_pdu_buffer(std::size_t size)
    -> tracked_pdu_pool<pooled_buffer>::unique_ptr_type {
    auto buffer = pdu_buffer_pool::get().acquire_buffer();
    buffer->resize(size);
    return buffer;
}

/**
 * @brief Create a pooled presentation_data_value
 * @param context_id Presentation Context ID
 * @param is_command Whether this is a command message
 * @param is_last Whether this is the last fragment
 * @return A unique_ptr to the pooled PDV
 */
[[nodiscard]] inline auto make_pooled_pdv(uint8_t context_id = 0,
                                          bool is_command = false,
                                          bool is_last = true)
    -> tracked_pdu_pool<presentation_data_value>::unique_ptr_type {
    return pdu_buffer_pool::get().acquire_pdv(context_id, is_command, is_last);
}

/**
 * @brief Create a pooled p_data_tf_pdu
 * @return A unique_ptr to the pooled P-DATA-TF PDU
 */
[[nodiscard]] inline auto make_pooled_p_data_tf()
    -> tracked_pdu_pool<p_data_tf_pdu>::unique_ptr_type {
    return pdu_buffer_pool::get().acquire_p_data_tf();
}

}  // namespace pacs::network
