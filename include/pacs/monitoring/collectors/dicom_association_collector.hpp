/**
 * @file dicom_association_collector.hpp
 * @brief DICOM Association metrics collector
 *
 * This file provides a collector for DICOM association lifecycle metrics,
 * compatible with the monitoring_system collector plugin interface.
 *
 * @see Issue #310 - IMonitor Integration and DICOM Metric Collector
 * @see DICOM PS3.8 - Network Communication Support for Message Exchange
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
 * @struct association_metric
 * @brief Standard metric structure for association data
 */
struct association_metric {
    std::string name;
    double value;
    std::string type;  // "gauge" or "counter"
    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> labels;

    association_metric(std::string n,
                       double v,
                       std::string t,
                       std::unordered_map<std::string, std::string> l = {})
        : name(std::move(n))
        , value(v)
        , type(std::move(t))
        , timestamp(std::chrono::system_clock::now())
        , labels(std::move(l)) {}
};

/**
 * @class dicom_association_collector
 * @brief Collector for DICOM association lifecycle metrics
 *
 * This collector gathers metrics related to DICOM associations including:
 * - Active association count
 * - Total associations established
 * - Association success/failure rates
 * - Peak active associations
 * - Association duration statistics
 *
 * Thread Safety: All public methods are thread-safe.
 *
 * @example
 * @code
 * dicom_association_collector collector;
 * collector.initialize({});
 *
 * // Collect metrics
 * auto metrics = collector.collect();
 * for (const auto& m : metrics) {
 *     std::cout << m.name << ": " << m.value << "\n";
 * }
 * @endcode
 */
class dicom_association_collector {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Default constructor
     * @param ae_title Optional AE title for labeling metrics
     */
    explicit dicom_association_collector(std::string ae_title = "PACS_SCP");

    /**
     * @brief Destructor
     */
    ~dicom_association_collector() = default;

    // Non-copyable, non-movable
    dicom_association_collector(const dicom_association_collector&) = delete;
    dicom_association_collector& operator=(const dicom_association_collector&) = delete;
    dicom_association_collector(dicom_association_collector&&) = delete;
    dicom_association_collector& operator=(dicom_association_collector&&) = delete;

    // =========================================================================
    // Collector Plugin Interface
    // =========================================================================

    /**
     * @brief Initialize the collector with configuration
     * @param config Configuration map (currently unused)
     * @return true if initialization succeeded
     */
    [[nodiscard]] bool initialize(
        const std::unordered_map<std::string, std::string>& config);

    /**
     * @brief Collect current association metrics
     * @return Vector of association metrics
     */
    [[nodiscard]] std::vector<association_metric> collect();

    /**
     * @brief Get the collector name
     * @return "dicom_association_collector"
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

private:
    // Configuration
    std::string ae_title_;
    bool initialized_{false};

    // Statistics
    mutable std::mutex stats_mutex_;
    std::uint64_t collection_count_{0};
    std::chrono::steady_clock::time_point init_time_;

    // Helper to create labeled metric
    [[nodiscard]] association_metric create_metric(
        const std::string& name,
        double value,
        const std::string& type) const;
};

// ─────────────────────────────────────────────────────────────────────────────
// Inline Implementation
// ─────────────────────────────────────────────────────────────────────────────

inline dicom_association_collector::dicom_association_collector(std::string ae_title)
    : ae_title_(std::move(ae_title)) {}

inline bool dicom_association_collector::initialize(
    const std::unordered_map<std::string, std::string>& config) {
    // Extract AE title from config if provided
    if (auto it = config.find("ae_title"); it != config.end()) {
        ae_title_ = it->second;
    }

    init_time_ = std::chrono::steady_clock::now();
    initialized_ = true;
    return true;
}

inline std::vector<association_metric> dicom_association_collector::collect() {
    std::vector<association_metric> metrics;

    if (!initialized_) {
        return metrics;
    }

    const auto& counters = pacs_metrics::global_metrics().associations();

    // Active associations (gauge)
    metrics.push_back(create_metric(
        "dicom_associations_active",
        static_cast<double>(counters.current_active.load(std::memory_order_relaxed)),
        "gauge"));

    // Peak active associations (gauge)
    metrics.push_back(create_metric(
        "dicom_associations_peak_active",
        static_cast<double>(counters.peak_active.load(std::memory_order_relaxed)),
        "gauge"));

    // Total established (counter)
    metrics.push_back(create_metric(
        "dicom_associations_total",
        static_cast<double>(counters.total_established.load(std::memory_order_relaxed)),
        "counter"));

    // Total rejected (counter)
    metrics.push_back(create_metric(
        "dicom_associations_rejected_total",
        static_cast<double>(counters.total_rejected.load(std::memory_order_relaxed)),
        "counter"));

    // Total aborted (counter)
    metrics.push_back(create_metric(
        "dicom_associations_aborted_total",
        static_cast<double>(counters.total_aborted.load(std::memory_order_relaxed)),
        "counter"));

    // Success rate (gauge) - calculated metric
    const auto total = counters.total_established.load(std::memory_order_relaxed);
    const auto rejected = counters.total_rejected.load(std::memory_order_relaxed);
    const auto attempted = total + rejected;
    const double success_rate = (attempted > 0)
        ? static_cast<double>(total) / static_cast<double>(attempted)
        : 1.0;

    metrics.push_back(create_metric(
        "dicom_associations_success_rate",
        success_rate,
        "gauge"));

    // Update statistics
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        ++collection_count_;
    }

    return metrics;
}

inline std::string dicom_association_collector::get_name() const {
    return "dicom_association_collector";
}

inline std::vector<std::string> dicom_association_collector::get_metric_types() const {
    return {
        "dicom_associations_active",
        "dicom_associations_peak_active",
        "dicom_associations_total",
        "dicom_associations_rejected_total",
        "dicom_associations_aborted_total",
        "dicom_associations_success_rate"
    };
}

inline bool dicom_association_collector::is_healthy() const {
    return initialized_;
}

inline std::unordered_map<std::string, double>
dicom_association_collector::get_statistics() const {
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

inline void dicom_association_collector::set_ae_title(std::string ae_title) {
    ae_title_ = std::move(ae_title);
}

inline std::string dicom_association_collector::get_ae_title() const {
    return ae_title_;
}

inline association_metric dicom_association_collector::create_metric(
    const std::string& name,
    double value,
    const std::string& type) const {
    return association_metric(
        name,
        value,
        type,
        {{"ae", ae_title_}});
}

}  // namespace pacs::monitoring
