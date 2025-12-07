/**
 * @file pacs_metrics.hpp
 * @brief Operation metrics collection for PACS DICOM services
 *
 * This file provides the pacs_metrics class that tracks atomic counters and
 * timing data for DICOM operations to enable performance monitoring with
 * minimal overhead.
 *
 * @see Issue #210 - [Quick Win] feat(monitoring): Implement operation metrics collection
 * @see DICOM PS3.7 - Message Exchange (DIMSE Services)
 */

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <string_view>

namespace pacs::monitoring {

/**
 * @enum dimse_operation
 * @brief DICOM Message Service Element (DIMSE) operation types
 *
 * Represents the different DIMSE operations that can be tracked by the
 * metrics system. These correspond to the standard DICOM network services.
 */
enum class dimse_operation {
    c_echo,   ///< C-ECHO (Verification Service)
    c_store,  ///< C-STORE (Storage Service)
    c_find,   ///< C-FIND (Query Service)
    c_move,   ///< C-MOVE (Retrieve Service)
    c_get,    ///< C-GET (Retrieve Service)
    n_create, ///< N-CREATE (MPPS)
    n_set,    ///< N-SET (MPPS)
    n_get,    ///< N-GET
    n_action, ///< N-ACTION
    n_event,  ///< N-EVENT-REPORT
    n_delete  ///< N-DELETE
};

/**
 * @brief Convert DIMSE operation to string representation
 * @param op The DIMSE operation to convert
 * @return String representation (e.g., "c_echo", "c_store")
 */
[[nodiscard]] constexpr std::string_view to_string(dimse_operation op) noexcept {
    switch (op) {
        case dimse_operation::c_echo:
            return "c_echo";
        case dimse_operation::c_store:
            return "c_store";
        case dimse_operation::c_find:
            return "c_find";
        case dimse_operation::c_move:
            return "c_move";
        case dimse_operation::c_get:
            return "c_get";
        case dimse_operation::n_create:
            return "n_create";
        case dimse_operation::n_set:
            return "n_set";
        case dimse_operation::n_get:
            return "n_get";
        case dimse_operation::n_action:
            return "n_action";
        case dimse_operation::n_event:
            return "n_event";
        case dimse_operation::n_delete:
            return "n_delete";
        default:
            return "unknown";
    }
}

/**
 * @struct operation_counter
 * @brief Atomic counter for tracking operation success/failure counts
 *
 * Thread-safe counters for tracking the number of successful and failed
 * operations, along with timing statistics.
 */
struct operation_counter {
    std::atomic<std::uint64_t> success_count{0};
    std::atomic<std::uint64_t> failure_count{0};
    std::atomic<std::uint64_t> total_duration_us{0};  ///< Total duration in microseconds
    std::atomic<std::uint64_t> min_duration_us{UINT64_MAX};
    std::atomic<std::uint64_t> max_duration_us{0};

    /// Get total operation count (success + failure)
    [[nodiscard]] std::uint64_t total_count() const noexcept {
        return success_count.load(std::memory_order_relaxed) +
               failure_count.load(std::memory_order_relaxed);
    }

    /// Get average duration in microseconds (0 if no operations)
    [[nodiscard]] std::uint64_t average_duration_us() const noexcept {
        const auto total = total_count();
        if (total == 0) {
            return 0;
        }
        return total_duration_us.load(std::memory_order_relaxed) / total;
    }

    /// Record a successful operation with duration
    void record_success(std::chrono::microseconds duration) noexcept {
        success_count.fetch_add(1, std::memory_order_relaxed);
        const auto duration_us = static_cast<std::uint64_t>(duration.count());
        total_duration_us.fetch_add(duration_us, std::memory_order_relaxed);

        // Update min/max with CAS loops
        auto current_min = min_duration_us.load(std::memory_order_relaxed);
        while (duration_us < current_min &&
               !min_duration_us.compare_exchange_weak(current_min, duration_us,
                                                       std::memory_order_relaxed)) {
            // Loop until successful
        }

        auto current_max = max_duration_us.load(std::memory_order_relaxed);
        while (duration_us > current_max &&
               !max_duration_us.compare_exchange_weak(current_max, duration_us,
                                                       std::memory_order_relaxed)) {
            // Loop until successful
        }
    }

    /// Record a failed operation with duration
    void record_failure(std::chrono::microseconds duration) noexcept {
        failure_count.fetch_add(1, std::memory_order_relaxed);
        const auto duration_us = static_cast<std::uint64_t>(duration.count());
        total_duration_us.fetch_add(duration_us, std::memory_order_relaxed);

        // Update min/max with CAS loops
        auto current_min = min_duration_us.load(std::memory_order_relaxed);
        while (duration_us < current_min &&
               !min_duration_us.compare_exchange_weak(current_min, duration_us,
                                                       std::memory_order_relaxed)) {
        }

        auto current_max = max_duration_us.load(std::memory_order_relaxed);
        while (duration_us > current_max &&
               !max_duration_us.compare_exchange_weak(current_max, duration_us,
                                                       std::memory_order_relaxed)) {
        }
    }

    /// Reset all counters to zero
    void reset() noexcept {
        success_count.store(0, std::memory_order_relaxed);
        failure_count.store(0, std::memory_order_relaxed);
        total_duration_us.store(0, std::memory_order_relaxed);
        min_duration_us.store(UINT64_MAX, std::memory_order_relaxed);
        max_duration_us.store(0, std::memory_order_relaxed);
    }
};

/**
 * @struct data_transfer_metrics
 * @brief Metrics for tracking data transfer volumes
 *
 * Thread-safe counters for tracking bytes sent/received and image counts.
 */
struct data_transfer_metrics {
    std::atomic<std::uint64_t> bytes_sent{0};
    std::atomic<std::uint64_t> bytes_received{0};
    std::atomic<std::uint64_t> images_stored{0};
    std::atomic<std::uint64_t> images_retrieved{0};

    /// Record bytes sent
    void add_bytes_sent(std::uint64_t bytes) noexcept {
        bytes_sent.fetch_add(bytes, std::memory_order_relaxed);
    }

    /// Record bytes received
    void add_bytes_received(std::uint64_t bytes) noexcept {
        bytes_received.fetch_add(bytes, std::memory_order_relaxed);
    }

    /// Record an image stored
    void increment_images_stored() noexcept {
        images_stored.fetch_add(1, std::memory_order_relaxed);
    }

    /// Record an image retrieved
    void increment_images_retrieved() noexcept {
        images_retrieved.fetch_add(1, std::memory_order_relaxed);
    }

    /// Reset all counters to zero
    void reset() noexcept {
        bytes_sent.store(0, std::memory_order_relaxed);
        bytes_received.store(0, std::memory_order_relaxed);
        images_stored.store(0, std::memory_order_relaxed);
        images_retrieved.store(0, std::memory_order_relaxed);
    }
};

/**
 * @struct association_counters
 * @brief Metrics for tracking DICOM association lifecycle
 *
 * Thread-safe counters for tracking association establishment, rejection,
 * and current active count.
 */
struct association_counters {
    std::atomic<std::uint64_t> total_established{0};
    std::atomic<std::uint64_t> total_rejected{0};
    std::atomic<std::uint64_t> total_aborted{0};
    std::atomic<std::uint32_t> current_active{0};
    std::atomic<std::uint32_t> peak_active{0};

    /// Record an association being established
    void record_established() noexcept {
        total_established.fetch_add(1, std::memory_order_relaxed);
        const auto active = current_active.fetch_add(1, std::memory_order_relaxed) + 1;

        // Update peak with CAS loop
        auto current_peak = peak_active.load(std::memory_order_relaxed);
        while (active > current_peak &&
               !peak_active.compare_exchange_weak(current_peak, active,
                                                   std::memory_order_relaxed)) {
        }
    }

    /// Record an association being released normally
    void record_released() noexcept {
        const auto prev = current_active.load(std::memory_order_relaxed);
        if (prev > 0) {
            current_active.fetch_sub(1, std::memory_order_relaxed);
        }
    }

    /// Record an association being rejected
    void record_rejected() noexcept {
        total_rejected.fetch_add(1, std::memory_order_relaxed);
    }

    /// Record an association being aborted
    void record_aborted() noexcept {
        total_aborted.fetch_add(1, std::memory_order_relaxed);
        const auto prev = current_active.load(std::memory_order_relaxed);
        if (prev > 0) {
            current_active.fetch_sub(1, std::memory_order_relaxed);
        }
    }

    /// Reset all counters to zero
    void reset() noexcept {
        total_established.store(0, std::memory_order_relaxed);
        total_rejected.store(0, std::memory_order_relaxed);
        total_aborted.store(0, std::memory_order_relaxed);
        current_active.store(0, std::memory_order_relaxed);
        peak_active.store(0, std::memory_order_relaxed);
    }
};

/**
 * @class pacs_metrics
 * @brief Central metrics collection for PACS DICOM operations
 *
 * The pacs_metrics class provides a thread-safe, low-overhead mechanism for
 * tracking DICOM operation metrics including:
 * - DIMSE operation counts and timing (C-ECHO, C-STORE, C-FIND, C-MOVE, C-GET)
 * - Data transfer volumes (bytes sent/received, images stored/retrieved)
 * - Association lifecycle events (established, rejected, aborted)
 *
 * Thread Safety: All public methods are thread-safe using atomic operations.
 *
 * @example
 * @code
 * // Get global metrics instance
 * auto& metrics = pacs_metrics::global_metrics();
 *
 * // Record a successful C-STORE operation
 * auto start = std::chrono::steady_clock::now();
 * // ... perform C-STORE ...
 * auto end = std::chrono::steady_clock::now();
 * auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
 * metrics.record_store(true, duration, 1024 * 1024);  // 1MB image
 *
 * // Export metrics
 * std::string json = metrics.to_json();
 * std::string prometheus = metrics.to_prometheus();
 * @endcode
 */
class pacs_metrics {
public:
    // =========================================================================
    // Construction and Global Access
    // =========================================================================

    /**
     * @brief Default constructor
     */
    pacs_metrics() = default;

    /**
     * @brief Get the global singleton instance
     * @return Reference to the global metrics instance
     *
     * Thread-safe lazy initialization using Meyer's singleton pattern.
     */
    [[nodiscard]] static pacs_metrics& global_metrics() noexcept {
        static pacs_metrics instance;
        return instance;
    }

    /// Non-copyable
    pacs_metrics(const pacs_metrics&) = delete;
    pacs_metrics& operator=(const pacs_metrics&) = delete;

    /// Non-movable (singleton)
    pacs_metrics(pacs_metrics&&) = delete;
    pacs_metrics& operator=(pacs_metrics&&) = delete;

    // =========================================================================
    // DIMSE Operation Recording
    // =========================================================================

    /**
     * @brief Record a C-STORE operation
     * @param success Whether the operation succeeded
     * @param duration Operation duration
     * @param bytes_stored Number of bytes stored (0 if failed)
     */
    void record_store(bool success,
                      std::chrono::microseconds duration,
                      std::uint64_t bytes_stored = 0) noexcept {
        if (success) {
            c_store_.record_success(duration);
            if (bytes_stored > 0) {
                transfer_.add_bytes_received(bytes_stored);
                transfer_.increment_images_stored();
            }
        } else {
            c_store_.record_failure(duration);
        }
    }

    /**
     * @brief Record a C-FIND (query) operation
     * @param success Whether the operation succeeded
     * @param duration Operation duration
     * @param matches Number of matching results
     */
    void record_query(bool success,
                      std::chrono::microseconds duration,
                      [[maybe_unused]] std::uint32_t matches = 0) noexcept {
        if (success) {
            c_find_.record_success(duration);
        } else {
            c_find_.record_failure(duration);
        }
    }

    /**
     * @brief Record a C-ECHO (verification) operation
     * @param success Whether the operation succeeded
     * @param duration Operation duration
     */
    void record_echo(bool success, std::chrono::microseconds duration) noexcept {
        if (success) {
            c_echo_.record_success(duration);
        } else {
            c_echo_.record_failure(duration);
        }
    }

    /**
     * @brief Record a C-MOVE operation
     * @param success Whether the operation succeeded
     * @param duration Operation duration
     * @param images_moved Number of images moved
     */
    void record_move(bool success,
                     std::chrono::microseconds duration,
                     std::uint32_t images_moved = 0) noexcept {
        if (success) {
            c_move_.record_success(duration);
            for (std::uint32_t i = 0; i < images_moved; ++i) {
                transfer_.increment_images_retrieved();
            }
        } else {
            c_move_.record_failure(duration);
        }
    }

    /**
     * @brief Record a C-GET operation
     * @param success Whether the operation succeeded
     * @param duration Operation duration
     * @param images_retrieved Number of images retrieved
     * @param bytes_retrieved Number of bytes retrieved
     */
    void record_get(bool success,
                    std::chrono::microseconds duration,
                    std::uint32_t images_retrieved = 0,
                    std::uint64_t bytes_retrieved = 0) noexcept {
        if (success) {
            c_get_.record_success(duration);
            for (std::uint32_t i = 0; i < images_retrieved; ++i) {
                transfer_.increment_images_retrieved();
            }
            if (bytes_retrieved > 0) {
                transfer_.add_bytes_sent(bytes_retrieved);
            }
        } else {
            c_get_.record_failure(duration);
        }
    }

    /**
     * @brief Record a generic DIMSE operation
     * @param op Operation type
     * @param success Whether the operation succeeded
     * @param duration Operation duration
     */
    void record_operation(dimse_operation op,
                          bool success,
                          std::chrono::microseconds duration) noexcept {
        auto& counter = get_counter(op);
        if (success) {
            counter.record_success(duration);
        } else {
            counter.record_failure(duration);
        }
    }

    // =========================================================================
    // Data Transfer Recording
    // =========================================================================

    /**
     * @brief Record bytes sent over the network
     * @param bytes Number of bytes sent
     */
    void record_bytes_sent(std::uint64_t bytes) noexcept {
        transfer_.add_bytes_sent(bytes);
    }

    /**
     * @brief Record bytes received from the network
     * @param bytes Number of bytes received
     */
    void record_bytes_received(std::uint64_t bytes) noexcept {
        transfer_.add_bytes_received(bytes);
    }

    // =========================================================================
    // Association Recording
    // =========================================================================

    /**
     * @brief Record an association being established
     */
    void record_association_established() noexcept {
        associations_.record_established();
    }

    /**
     * @brief Record an association being released
     */
    void record_association_released() noexcept {
        associations_.record_released();
    }

    /**
     * @brief Record an association being rejected
     */
    void record_association_rejected() noexcept {
        associations_.record_rejected();
    }

    /**
     * @brief Record an association being aborted
     */
    void record_association_aborted() noexcept {
        associations_.record_aborted();
    }

    // =========================================================================
    // Metric Access
    // =========================================================================

    /**
     * @brief Get operation counter for a specific DIMSE operation
     * @param op The DIMSE operation type
     * @return Const reference to the operation counter
     */
    [[nodiscard]] const operation_counter& get_counter(dimse_operation op) const noexcept {
        switch (op) {
            case dimse_operation::c_echo:
                return c_echo_;
            case dimse_operation::c_store:
                return c_store_;
            case dimse_operation::c_find:
                return c_find_;
            case dimse_operation::c_move:
                return c_move_;
            case dimse_operation::c_get:
                return c_get_;
            case dimse_operation::n_create:
                return n_create_;
            case dimse_operation::n_set:
                return n_set_;
            case dimse_operation::n_get:
                return n_get_;
            case dimse_operation::n_action:
                return n_action_;
            case dimse_operation::n_event:
                return n_event_;
            case dimse_operation::n_delete:
                return n_delete_;
            default:
                return c_echo_;  // Should never happen
        }
    }

    /**
     * @brief Get mutable operation counter for a specific DIMSE operation
     * @param op The DIMSE operation type
     * @return Reference to the operation counter
     */
    [[nodiscard]] operation_counter& get_counter(dimse_operation op) noexcept {
        return const_cast<operation_counter&>(
            static_cast<const pacs_metrics*>(this)->get_counter(op));
    }

    /**
     * @brief Get data transfer metrics
     * @return Const reference to transfer metrics
     */
    [[nodiscard]] const data_transfer_metrics& transfer() const noexcept {
        return transfer_;
    }

    /**
     * @brief Get association counters
     * @return Const reference to association counters
     */
    [[nodiscard]] const association_counters& associations() const noexcept {
        return associations_;
    }

    // =========================================================================
    // Export Methods
    // =========================================================================

    /**
     * @brief Export metrics as JSON string
     * @return JSON representation of all metrics
     *
     * The JSON format is suitable for REST API responses and integration
     * with monitoring systems like Grafana.
     */
    [[nodiscard]] std::string to_json() const;

    /**
     * @brief Export metrics in Prometheus text format
     * @param prefix Metric name prefix (default: "pacs")
     * @return Prometheus exposition format text
     *
     * The output follows the Prometheus text-based format specification
     * suitable for scraping by Prometheus servers.
     */
    [[nodiscard]] std::string to_prometheus(std::string_view prefix = "pacs") const;

    // =========================================================================
    // Reset
    // =========================================================================

    /**
     * @brief Reset all metrics to zero
     *
     * Thread-safe reset of all counters. Useful for testing or periodic
     * metric collection windows.
     */
    void reset() noexcept {
        c_echo_.reset();
        c_store_.reset();
        c_find_.reset();
        c_move_.reset();
        c_get_.reset();
        n_create_.reset();
        n_set_.reset();
        n_get_.reset();
        n_action_.reset();
        n_event_.reset();
        n_delete_.reset();
        transfer_.reset();
        associations_.reset();
    }

private:
    // DIMSE operation counters
    operation_counter c_echo_;
    operation_counter c_store_;
    operation_counter c_find_;
    operation_counter c_move_;
    operation_counter c_get_;
    operation_counter n_create_;
    operation_counter n_set_;
    operation_counter n_get_;
    operation_counter n_action_;
    operation_counter n_event_;
    operation_counter n_delete_;

    // Data transfer metrics
    data_transfer_metrics transfer_;

    // Association lifecycle counters
    association_counters associations_;
};

}  // namespace pacs::monitoring
