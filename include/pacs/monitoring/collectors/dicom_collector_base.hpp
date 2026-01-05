/**
 * @file dicom_collector_base.hpp
 * @brief CRTP base class for DICOM metrics collectors
 *
 * This file provides a CRTP (Curiously Recurring Template Pattern) base class
 * for DICOM-specific metric collectors, following the monitoring_system's
 * collector pattern for zero-overhead polymorphism.
 *
 * @see Issue #490 - Implement CRTP-based DICOM metrics collector
 * @see monitoring_system/include/kcenon/monitoring/collectors/collector_base.h
 */

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace pacs::monitoring {

/**
 * @brief Type alias for configuration map
 */
using config_map = std::unordered_map<std::string, std::string>;

/**
 * @brief Type alias for statistics map
 */
using stats_map = std::unordered_map<std::string, double>;

/**
 * @struct dicom_metric
 * @brief Standard metric structure for DICOM data
 *
 * Compatible with monitoring_system's metric format for seamless integration.
 */
struct dicom_metric {
    std::string name;
    double value;
    std::string type;  // "gauge", "counter", "histogram"
    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> tags;

    dicom_metric() : value(0.0), timestamp(std::chrono::system_clock::now()) {}

    dicom_metric(std::string n,
                 double v,
                 std::string t,
                 std::unordered_map<std::string, std::string> tgs = {})
        : name(std::move(n))
        , value(v)
        , type(std::move(t))
        , timestamp(std::chrono::system_clock::now())
        , tags(std::move(tgs)) {}
};

/**
 * @class dicom_collector_base
 * @brief CRTP base class for DICOM metric collectors
 *
 * This template class implements common functionality shared by all DICOM collectors:
 * - Configuration parsing (enabled state, AE title)
 * - Collection with error handling and statistics
 * - Health monitoring
 * - Statistics tracking (collection count, error count)
 *
 * Usage:
 * @code
 * class my_dicom_collector : public dicom_collector_base<my_dicom_collector> {
 * public:
 *     static constexpr const char* collector_name = "my_dicom_collector";
 *
 *     bool do_initialize(const config_map& config) {
 *         return true;
 *     }
 *
 *     std::vector<dicom_metric> do_collect() {
 *         return metrics;
 *     }
 *
 *     bool is_available() const { return true; }
 *
 *     std::vector<std::string> do_get_metric_types() const {
 *         return {"metric_1", "metric_2"};
 *     }
 *
 *     void do_add_statistics(stats_map& stats) const {}
 * };
 * @endcode
 *
 * @tparam Derived The derived collector class (CRTP pattern)
 */
template <typename Derived>
class dicom_collector_base {
public:
    dicom_collector_base() = default;
    virtual ~dicom_collector_base() = default;

    // Non-copyable, non-moveable
    dicom_collector_base(const dicom_collector_base&) = delete;
    dicom_collector_base& operator=(const dicom_collector_base&) = delete;
    dicom_collector_base(dicom_collector_base&&) = delete;
    dicom_collector_base& operator=(dicom_collector_base&&) = delete;

    /**
     * @brief Initialize the collector with configuration
     * @param config Configuration options
     *   - "enabled": "true"/"false" (default: true)
     *   - "ae_title": Application Entity title for labeling
     * @return true if initialization successful
     */
    bool initialize(const config_map& config) {
        // Parse common configuration
        if (auto it = config.find("enabled"); it != config.end()) {
            enabled_ = (it->second == "true" || it->second == "1");
        }

        if (auto it = config.find("ae_title"); it != config.end()) {
            ae_title_ = it->second;
        }

        init_time_ = std::chrono::steady_clock::now();

        // Delegate to derived class for specific initialization
        return derived().do_initialize(config);
    }

    /**
     * @brief Collect metrics from the data source
     * @return Collection of DICOM metrics
     */
    std::vector<dicom_metric> collect() {
        if (!enabled_) {
            return {};
        }

        try {
            auto metrics = derived().do_collect();
            ++collection_count_;
            last_collection_time_ = std::chrono::system_clock::now();
            return metrics;
        } catch (...) {
            ++collection_errors_;
            return {};
        }
    }

    /**
     * @brief Get the name of this collector
     * @return Collector name from Derived::collector_name
     */
    [[nodiscard]] std::string get_name() const {
        return Derived::collector_name;
    }

    /**
     * @brief Get supported metric types
     * @return Vector of supported metric type names
     */
    [[nodiscard]] std::vector<std::string> get_metric_types() const {
        return derived().do_get_metric_types();
    }

    /**
     * @brief Check if the collector is healthy
     * @return true if collector is operational
     */
    [[nodiscard]] bool is_healthy() const {
        if (!enabled_) {
            return true;  // Disabled collectors are considered healthy
        }
        return derived().is_available();
    }

    /**
     * @brief Get collector statistics
     * @return Map of statistic name to value
     */
    [[nodiscard]] stats_map get_statistics() const {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_map stats;

        // Common statistics
        stats["enabled"] = enabled_ ? 1.0 : 0.0;
        stats["collection_count"] = static_cast<double>(collection_count_.load());
        stats["collection_errors"] = static_cast<double>(collection_errors_.load());

        // Uptime if initialized
        if (init_time_.time_since_epoch().count() > 0) {
            const auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - init_time_);
            stats["uptime_seconds"] = static_cast<double>(uptime.count());
        }

        // Let derived class add specific statistics
        derived().do_add_statistics(stats);

        return stats;
    }

    /**
     * @brief Check if collector is enabled
     * @return true if enabled
     */
    [[nodiscard]] bool is_enabled() const { return enabled_; }

    /**
     * @brief Get collection count
     * @return Number of successful collections
     */
    [[nodiscard]] std::size_t get_collection_count() const {
        return collection_count_.load();
    }

    /**
     * @brief Get error count
     * @return Number of failed collections
     */
    [[nodiscard]] std::size_t get_collection_errors() const {
        return collection_errors_.load();
    }

    /**
     * @brief Get the AE title
     * @return Application Entity title
     */
    [[nodiscard]] std::string get_ae_title() const { return ae_title_; }

    /**
     * @brief Set the AE title
     * @param ae_title Application Entity title
     */
    void set_ae_title(std::string ae_title) { ae_title_ = std::move(ae_title); }

protected:
    /**
     * @brief Create a metric with common tags
     * @param name Metric name
     * @param value Metric value
     * @param type Metric type ("gauge", "counter", "histogram")
     * @param extra_tags Additional tags to include
     * @return Created metric
     */
    [[nodiscard]] dicom_metric create_base_metric(
        const std::string& name,
        double value,
        const std::string& type,
        const std::unordered_map<std::string, std::string>& extra_tags = {}) const {

        std::unordered_map<std::string, std::string> tags = extra_tags;
        tags["collector"] = Derived::collector_name;
        if (!ae_title_.empty()) {
            tags["ae_title"] = ae_title_;
        }

        return dicom_metric(name, value, type, std::move(tags));
    }

    // Configuration
    bool enabled_{true};
    std::string ae_title_{"PACS_SCP"};

    // Statistics
    mutable std::mutex stats_mutex_;
    std::atomic<std::size_t> collection_count_{0};
    std::atomic<std::size_t> collection_errors_{0};
    std::chrono::steady_clock::time_point init_time_;
    std::chrono::system_clock::time_point last_collection_time_;

private:
    /**
     * @brief Get reference to derived class (CRTP helper)
     */
    Derived& derived() { return static_cast<Derived&>(*this); }
    const Derived& derived() const { return static_cast<const Derived&>(*this); }
};

}  // namespace pacs::monitoring
