/**
 * @file dicom_storage_collector.hpp
 * @brief DICOM Storage metrics collector
 *
 * This file provides a collector for DICOM storage and data transfer metrics,
 * compatible with the monitoring_system collector plugin interface.
 *
 * @see Issue #310 - IMonitor Integration and DICOM Metric Collector
 * @see DICOM PS3.4 - Service Class Specifications (Storage Service)
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "../pacs_metrics.hpp"

namespace pacs::monitoring {

/**
 * @struct storage_metric
 * @brief Standard metric structure for storage data
 */
struct storage_metric {
    std::string name;
    double value;
    std::string type;  // "gauge" or "counter"
    std::string unit;  // "bytes", "count", etc.
    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> labels;

    storage_metric(std::string n,
                   double v,
                   std::string t,
                   std::string u = "",
                   std::unordered_map<std::string, std::string> l = {})
        : name(std::move(n))
        , value(v)
        , type(std::move(t))
        , unit(std::move(u))
        , timestamp(std::chrono::system_clock::now())
        , labels(std::move(l)) {}
};

/**
 * @class dicom_storage_collector
 * @brief Collector for DICOM storage and data transfer metrics
 *
 * This collector gathers metrics related to DICOM storage including:
 * - Bytes sent/received over the network
 * - Images stored/retrieved counts
 * - Object pool statistics (element, dataset, PDU buffer pools)
 * - Storage throughput calculations
 *
 * Thread Safety: All public methods are thread-safe.
 *
 * @example
 * @code
 * dicom_storage_collector collector;
 * collector.initialize({});
 *
 * // Collect metrics
 * auto metrics = collector.collect();
 * for (const auto& m : metrics) {
 *     std::cout << m.name << ": " << m.value << " " << m.unit << "\n";
 * }
 * @endcode
 */
class dicom_storage_collector {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Default constructor
     * @param ae_title Optional AE title for labeling metrics
     */
    explicit dicom_storage_collector(std::string ae_title = "PACS_SCP");

    /**
     * @brief Destructor
     */
    ~dicom_storage_collector() = default;

    // Non-copyable, non-movable
    dicom_storage_collector(const dicom_storage_collector&) = delete;
    dicom_storage_collector& operator=(const dicom_storage_collector&) = delete;
    dicom_storage_collector(dicom_storage_collector&&) = delete;
    dicom_storage_collector& operator=(dicom_storage_collector&&) = delete;

    // =========================================================================
    // Collector Plugin Interface
    // =========================================================================

    /**
     * @brief Initialize the collector with configuration
     * @param config Configuration map
     * @return true if initialization succeeded
     */
    [[nodiscard]] bool initialize(
        const std::unordered_map<std::string, std::string>& config);

    /**
     * @brief Collect current storage metrics
     * @return Vector of storage metrics
     */
    [[nodiscard]] std::vector<storage_metric> collect();

    /**
     * @brief Get the collector name
     * @return "dicom_storage_collector"
     */
    [[nodiscard]] std::string get_name() const;

    /**
     * @brief Get supported metric types
     * @return Vector of metric type names
     */
    [[nodiscard]] std::vector<std::string> get_metric_types() const;

    /**
     * @brief Check if the collector is healthy
     * @return true if operational
     */
    [[nodiscard]] bool is_healthy() const;

    /**
     * @brief Get collector statistics
     * @return Map of statistic name to value
     */
    [[nodiscard]] std::unordered_map<std::string, double> get_statistics() const;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Set the AE title for metric labels
     * @param ae_title The Application Entity title
     */
    void set_ae_title(std::string ae_title);

    /**
     * @brief Get the current AE title
     * @return The Application Entity title
     */
    [[nodiscard]] std::string get_ae_title() const;

    /**
     * @brief Enable or disable pool metrics collection
     * @param enabled Whether to collect pool metrics
     */
    void set_pool_metrics_enabled(bool enabled);

    /**
     * @brief Check if pool metrics collection is enabled
     * @return true if pool metrics are collected
     */
    [[nodiscard]] bool is_pool_metrics_enabled() const;

private:
    // Configuration
    std::string ae_title_;
    bool initialized_{false};
    bool collect_pool_metrics_{true};

    // Statistics
    mutable std::mutex stats_mutex_;
    std::uint64_t collection_count_{0};
    std::chrono::steady_clock::time_point init_time_;

    // Previous values for rate calculation
    std::uint64_t prev_bytes_sent_{0};
    std::uint64_t prev_bytes_received_{0};
    std::chrono::steady_clock::time_point prev_collection_time_;

    // Helper methods
    void collect_transfer_metrics(std::vector<storage_metric>& metrics);
    void collect_pool_metrics(std::vector<storage_metric>& metrics);

    [[nodiscard]] storage_metric create_metric(
        const std::string& name,
        double value,
        const std::string& type,
        const std::string& unit = "") const;
};

// ─────────────────────────────────────────────────────────────────────────────
// Inline Implementation
// ─────────────────────────────────────────────────────────────────────────────

inline dicom_storage_collector::dicom_storage_collector(std::string ae_title)
    : ae_title_(std::move(ae_title)) {}

inline bool dicom_storage_collector::initialize(
    const std::unordered_map<std::string, std::string>& config) {
    // Extract AE title from config if provided
    if (auto it = config.find("ae_title"); it != config.end()) {
        ae_title_ = it->second;
    }

    // Check for pool metrics configuration
    if (auto it = config.find("collect_pool_metrics"); it != config.end()) {
        collect_pool_metrics_ = (it->second == "true" || it->second == "1");
    }

    init_time_ = std::chrono::steady_clock::now();
    prev_collection_time_ = init_time_;
    initialized_ = true;
    return true;
}

inline std::vector<storage_metric> dicom_storage_collector::collect() {
    std::vector<storage_metric> metrics;

    if (!initialized_) {
        return metrics;
    }

    // Collect transfer metrics
    collect_transfer_metrics(metrics);

    // Collect pool metrics if enabled
    if (collect_pool_metrics_) {
        collect_pool_metrics(metrics);
    }

    // Update statistics
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        ++collection_count_;
    }

    return metrics;
}

inline void dicom_storage_collector::collect_transfer_metrics(
    std::vector<storage_metric>& metrics) {

    const auto& transfer = pacs_metrics::global_metrics().transfer();
    const auto now = std::chrono::steady_clock::now();

    // Current values
    const auto bytes_sent = transfer.bytes_sent.load(std::memory_order_relaxed);
    const auto bytes_received = transfer.bytes_received.load(std::memory_order_relaxed);
    const auto images_stored = transfer.images_stored.load(std::memory_order_relaxed);
    const auto images_retrieved = transfer.images_retrieved.load(std::memory_order_relaxed);

    // Total bytes sent (counter)
    metrics.push_back(create_metric(
        "dicom_bytes_sent_total",
        static_cast<double>(bytes_sent),
        "counter",
        "bytes"));

    // Total bytes received (counter)
    metrics.push_back(create_metric(
        "dicom_bytes_received_total",
        static_cast<double>(bytes_received),
        "counter",
        "bytes"));

    // Images stored (counter)
    metrics.push_back(create_metric(
        "dicom_images_stored_total",
        static_cast<double>(images_stored),
        "counter",
        "count"));

    // Images retrieved (counter)
    metrics.push_back(create_metric(
        "dicom_images_retrieved_total",
        static_cast<double>(images_retrieved),
        "counter",
        "count"));

    // Calculate throughput rates
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - prev_collection_time_);

    if (elapsed.count() > 0) {
        // Bytes per second sent (gauge)
        const auto sent_delta = bytes_sent - prev_bytes_sent_;
        const double send_rate =
            static_cast<double>(sent_delta) * 1000.0 / static_cast<double>(elapsed.count());
        metrics.push_back(create_metric(
            "dicom_bytes_sent_rate",
            send_rate,
            "gauge",
            "bytes_per_second"));

        // Bytes per second received (gauge)
        const auto received_delta = bytes_received - prev_bytes_received_;
        const double receive_rate =
            static_cast<double>(received_delta) * 1000.0 / static_cast<double>(elapsed.count());
        metrics.push_back(create_metric(
            "dicom_bytes_received_rate",
            receive_rate,
            "gauge",
            "bytes_per_second"));
    }

    // Update previous values for next rate calculation
    prev_bytes_sent_ = bytes_sent;
    prev_bytes_received_ = bytes_received;
    prev_collection_time_ = now;
}

inline void dicom_storage_collector::collect_pool_metrics(
    std::vector<storage_metric>& metrics) {

    const auto& pacs = pacs_metrics::global_metrics();

    // Element pool metrics
    const auto& element_pool = pacs.element_pool();
    metrics.push_back(create_metric(
        "dicom_element_pool_acquisitions_total",
        static_cast<double>(element_pool.total_acquisitions.load(std::memory_order_relaxed)),
        "counter",
        "count"));
    metrics.push_back(create_metric(
        "dicom_element_pool_hit_ratio",
        element_pool.hit_ratio(),
        "gauge",
        "ratio"));
    metrics.push_back(create_metric(
        "dicom_element_pool_size",
        static_cast<double>(element_pool.current_pool_size.load(std::memory_order_relaxed)),
        "gauge",
        "count"));

    // Dataset pool metrics
    const auto& dataset_pool = pacs.dataset_pool();
    metrics.push_back(create_metric(
        "dicom_dataset_pool_acquisitions_total",
        static_cast<double>(dataset_pool.total_acquisitions.load(std::memory_order_relaxed)),
        "counter",
        "count"));
    metrics.push_back(create_metric(
        "dicom_dataset_pool_hit_ratio",
        dataset_pool.hit_ratio(),
        "gauge",
        "ratio"));
    metrics.push_back(create_metric(
        "dicom_dataset_pool_size",
        static_cast<double>(dataset_pool.current_pool_size.load(std::memory_order_relaxed)),
        "gauge",
        "count"));

    // PDU buffer pool metrics
    const auto& pdu_pool = pacs.pdu_buffer_pool();
    metrics.push_back(create_metric(
        "dicom_pdu_buffer_pool_acquisitions_total",
        static_cast<double>(pdu_pool.total_acquisitions.load(std::memory_order_relaxed)),
        "counter",
        "count"));
    metrics.push_back(create_metric(
        "dicom_pdu_buffer_pool_hit_ratio",
        pdu_pool.hit_ratio(),
        "gauge",
        "ratio"));
    metrics.push_back(create_metric(
        "dicom_pdu_buffer_pool_size",
        static_cast<double>(pdu_pool.current_pool_size.load(std::memory_order_relaxed)),
        "gauge",
        "count"));
}

inline std::string dicom_storage_collector::get_name() const {
    return "dicom_storage_collector";
}

inline std::vector<std::string> dicom_storage_collector::get_metric_types() const {
    std::vector<std::string> types = {
        // Transfer metrics
        "dicom_bytes_sent_total",
        "dicom_bytes_received_total",
        "dicom_images_stored_total",
        "dicom_images_retrieved_total",
        "dicom_bytes_sent_rate",
        "dicom_bytes_received_rate"
    };

    if (collect_pool_metrics_) {
        // Element pool
        types.push_back("dicom_element_pool_acquisitions_total");
        types.push_back("dicom_element_pool_hit_ratio");
        types.push_back("dicom_element_pool_size");
        // Dataset pool
        types.push_back("dicom_dataset_pool_acquisitions_total");
        types.push_back("dicom_dataset_pool_hit_ratio");
        types.push_back("dicom_dataset_pool_size");
        // PDU buffer pool
        types.push_back("dicom_pdu_buffer_pool_acquisitions_total");
        types.push_back("dicom_pdu_buffer_pool_hit_ratio");
        types.push_back("dicom_pdu_buffer_pool_size");
    }

    return types;
}

inline bool dicom_storage_collector::is_healthy() const {
    return initialized_;
}

inline std::unordered_map<std::string, double>
dicom_storage_collector::get_statistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    std::unordered_map<std::string, double> stats;
    stats["collection_count"] = static_cast<double>(collection_count_);

    if (initialized_) {
        const auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - init_time_);
        stats["uptime_seconds"] = static_cast<double>(uptime.count());
    }

    return stats;
}

inline void dicom_storage_collector::set_ae_title(std::string ae_title) {
    ae_title_ = std::move(ae_title);
}

inline std::string dicom_storage_collector::get_ae_title() const {
    return ae_title_;
}

inline void dicom_storage_collector::set_pool_metrics_enabled(bool enabled) {
    collect_pool_metrics_ = enabled;
}

inline bool dicom_storage_collector::is_pool_metrics_enabled() const {
    return collect_pool_metrics_;
}

inline storage_metric dicom_storage_collector::create_metric(
    const std::string& name,
    double value,
    const std::string& type,
    const std::string& unit) const {
    return storage_metric(
        name,
        value,
        type,
        unit,
        {{"ae", ae_title_}});
}

}  // namespace pacs::monitoring
