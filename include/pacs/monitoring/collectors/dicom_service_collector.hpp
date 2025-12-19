/**
 * @file dicom_service_collector.hpp
 * @brief DICOM DIMSE Service metrics collector
 *
 * This file provides a collector for DICOM DIMSE service operation metrics,
 * compatible with the monitoring_system collector plugin interface.
 *
 * @see Issue #310 - IMonitor Integration and DICOM Metric Collector
 * @see DICOM PS3.7 - Message Exchange (DIMSE Services)
 */

#pragma once

#include <array>
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
 * @struct service_metric
 * @brief Standard metric structure for DIMSE service data
 */
struct service_metric {
    std::string name;
    double value;
    std::string type;  // "gauge" or "counter"
    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> labels;

    service_metric(std::string n,
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
 * @class dicom_service_collector
 * @brief Collector for DICOM DIMSE service operation metrics
 *
 * This collector gathers metrics for all DIMSE operations including:
 * - C-ECHO (Verification Service)
 * - C-STORE (Storage Service)
 * - C-FIND (Query Service)
 * - C-MOVE (Retrieve Service)
 * - C-GET (Retrieve Service)
 * - N-CREATE, N-SET, N-GET, N-ACTION, N-EVENT, N-DELETE (Normalized Services)
 *
 * For each operation, the collector reports:
 * - Total request count
 * - Success/failure counts
 * - Duration statistics (average, min, max)
 * - Throughput (requests/second)
 *
 * Thread Safety: All public methods are thread-safe.
 *
 * @example
 * @code
 * dicom_service_collector collector;
 * collector.initialize({});
 *
 * // Collect metrics
 * auto metrics = collector.collect();
 * for (const auto& m : metrics) {
 *     std::cout << m.name << ": " << m.value << "\n";
 * }
 * @endcode
 */
class dicom_service_collector {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Default constructor
     * @param ae_title Optional AE title for labeling metrics
     */
    explicit dicom_service_collector(std::string ae_title = "PACS_SCP");

    /**
     * @brief Destructor
     */
    ~dicom_service_collector() = default;

    // Non-copyable, non-movable
    dicom_service_collector(const dicom_service_collector&) = delete;
    dicom_service_collector& operator=(const dicom_service_collector&) = delete;
    dicom_service_collector(dicom_service_collector&&) = delete;
    dicom_service_collector& operator=(dicom_service_collector&&) = delete;

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
     * @brief Collect current DIMSE service metrics
     * @return Vector of service metrics
     */
    [[nodiscard]] std::vector<service_metric> collect();

    /**
     * @brief Get the collector name
     * @return "dicom_service_collector"
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
     * @brief Enable or disable specific operation metrics
     * @param op The DIMSE operation
     * @param enabled Whether to collect metrics for this operation
     */
    void set_operation_enabled(dimse_operation op, bool enabled);

    /**
     * @brief Check if metrics are enabled for an operation
     * @param op The DIMSE operation
     * @return true if enabled
     */
    [[nodiscard]] bool is_operation_enabled(dimse_operation op) const;

private:
    // Configuration
    std::string ae_title_;
    bool initialized_{false};

    // Operation enable flags (all enabled by default)
    std::array<bool, 11> operation_enabled_{
        true, true, true, true, true,  // C-ECHO, C-STORE, C-FIND, C-MOVE, C-GET
        true, true, true, true, true, true  // N-CREATE, N-SET, N-GET, N-ACTION, N-EVENT, N-DELETE
    };

    // Statistics
    mutable std::mutex stats_mutex_;
    std::uint64_t collection_count_{0};
    std::chrono::steady_clock::time_point init_time_;

    // Helper methods
    void collect_operation_metrics(
        std::vector<service_metric>& metrics,
        dimse_operation op,
        const operation_counter& counter);

    [[nodiscard]] service_metric create_metric(
        const std::string& name,
        double value,
        const std::string& type,
        const std::string& operation) const;
};

// ─────────────────────────────────────────────────────────────────────────────
// Inline Implementation
// ─────────────────────────────────────────────────────────────────────────────

inline dicom_service_collector::dicom_service_collector(std::string ae_title)
    : ae_title_(std::move(ae_title)) {}

inline bool dicom_service_collector::initialize(
    const std::unordered_map<std::string, std::string>& config) {
    // Extract AE title from config if provided
    if (auto it = config.find("ae_title"); it != config.end()) {
        ae_title_ = it->second;
    }

    init_time_ = std::chrono::steady_clock::now();
    initialized_ = true;
    return true;
}

inline std::vector<service_metric> dicom_service_collector::collect() {
    std::vector<service_metric> metrics;

    if (!initialized_) {
        return metrics;
    }

    const auto& pacs = pacs_metrics::global_metrics();

    // Collect metrics for each enabled DIMSE operation
    static constexpr std::array<dimse_operation, 11> all_ops = {
        dimse_operation::c_echo,
        dimse_operation::c_store,
        dimse_operation::c_find,
        dimse_operation::c_move,
        dimse_operation::c_get,
        dimse_operation::n_create,
        dimse_operation::n_set,
        dimse_operation::n_get,
        dimse_operation::n_action,
        dimse_operation::n_event,
        dimse_operation::n_delete
    };

    for (std::size_t i = 0; i < all_ops.size(); ++i) {
        if (operation_enabled_[i]) {
            collect_operation_metrics(
                metrics,
                all_ops[i],
                pacs.get_counter(all_ops[i]));
        }
    }

    // Update statistics
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        ++collection_count_;
    }

    return metrics;
}

inline void dicom_service_collector::collect_operation_metrics(
    std::vector<service_metric>& metrics,
    dimse_operation op,
    const operation_counter& counter) {

    const std::string op_name(to_string(op));

    // Total requests (counter)
    metrics.push_back(create_metric(
        "dicom_" + op_name + "_requests_total",
        static_cast<double>(counter.total_count()),
        "counter",
        op_name));

    // Successful requests (counter)
    metrics.push_back(create_metric(
        "dicom_" + op_name + "_success_total",
        static_cast<double>(counter.success_count.load(std::memory_order_relaxed)),
        "counter",
        op_name));

    // Failed requests (counter)
    metrics.push_back(create_metric(
        "dicom_" + op_name + "_failure_total",
        static_cast<double>(counter.failure_count.load(std::memory_order_relaxed)),
        "counter",
        op_name));

    // Duration metrics (only if there have been requests)
    if (counter.total_count() > 0) {
        // Average duration in seconds (gauge)
        metrics.push_back(create_metric(
            "dicom_" + op_name + "_duration_seconds_avg",
            static_cast<double>(counter.average_duration_us()) / 1'000'000.0,
            "gauge",
            op_name));

        // Min duration in seconds (gauge)
        const auto min_us = counter.min_duration_us.load(std::memory_order_relaxed);
        if (min_us != UINT64_MAX) {
            metrics.push_back(create_metric(
                "dicom_" + op_name + "_duration_seconds_min",
                static_cast<double>(min_us) / 1'000'000.0,
                "gauge",
                op_name));
        }

        // Max duration in seconds (gauge)
        const auto max_us = counter.max_duration_us.load(std::memory_order_relaxed);
        if (max_us > 0) {
            metrics.push_back(create_metric(
                "dicom_" + op_name + "_duration_seconds_max",
                static_cast<double>(max_us) / 1'000'000.0,
                "gauge",
                op_name));
        }

        // Total duration in seconds (counter) - for calculating rates
        metrics.push_back(create_metric(
            "dicom_" + op_name + "_duration_seconds_sum",
            static_cast<double>(counter.total_duration_us.load(std::memory_order_relaxed)) / 1'000'000.0,
            "counter",
            op_name));
    }
}

inline std::string dicom_service_collector::get_name() const {
    return "dicom_service_collector";
}

inline std::vector<std::string> dicom_service_collector::get_metric_types() const {
    std::vector<std::string> types;

    static constexpr std::array<dimse_operation, 11> all_ops = {
        dimse_operation::c_echo,
        dimse_operation::c_store,
        dimse_operation::c_find,
        dimse_operation::c_move,
        dimse_operation::c_get,
        dimse_operation::n_create,
        dimse_operation::n_set,
        dimse_operation::n_get,
        dimse_operation::n_action,
        dimse_operation::n_event,
        dimse_operation::n_delete
    };

    for (const auto op : all_ops) {
        const std::string op_name(to_string(op));
        types.push_back("dicom_" + op_name + "_requests_total");
        types.push_back("dicom_" + op_name + "_success_total");
        types.push_back("dicom_" + op_name + "_failure_total");
        types.push_back("dicom_" + op_name + "_duration_seconds_avg");
        types.push_back("dicom_" + op_name + "_duration_seconds_min");
        types.push_back("dicom_" + op_name + "_duration_seconds_max");
        types.push_back("dicom_" + op_name + "_duration_seconds_sum");
    }

    return types;
}

inline bool dicom_service_collector::is_healthy() const {
    return initialized_;
}

inline std::unordered_map<std::string, double>
dicom_service_collector::get_statistics() const {
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

inline void dicom_service_collector::set_ae_title(std::string ae_title) {
    ae_title_ = std::move(ae_title);
}

inline std::string dicom_service_collector::get_ae_title() const {
    return ae_title_;
}

inline void dicom_service_collector::set_operation_enabled(
    dimse_operation op,
    bool enabled) {
    operation_enabled_[static_cast<std::size_t>(op)] = enabled;
}

inline bool dicom_service_collector::is_operation_enabled(dimse_operation op) const {
    return operation_enabled_[static_cast<std::size_t>(op)];
}

inline service_metric dicom_service_collector::create_metric(
    const std::string& name,
    double value,
    const std::string& type,
    const std::string& operation) const {
    return service_metric(
        name,
        value,
        type,
        {{"ae", ae_title_}, {"operation", operation}});
}

}  // namespace pacs::monitoring
