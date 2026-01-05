/**
 * @file pacs_monitor.hpp
 * @brief Unified PACS monitoring with IMonitor interface integration
 *
 * This file provides the pacs_monitor class that implements the IMonitor
 * interface from common_system, integrating all DICOM-specific metric
 * collectors into a unified monitoring solution.
 *
 * @see Issue #310 - IMonitor Integration and DICOM Metric Collector
 * @see common_system IMonitor interface
 */

#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "collectors/dicom_association_collector.hpp"
#include "collectors/dicom_metrics_collector.hpp"
#include "collectors/dicom_service_collector.hpp"
#include "collectors/dicom_storage_collector.hpp"
#include "health_checker.hpp"
#include "pacs_metrics.hpp"

namespace pacs::monitoring {

// ─────────────────────────────────────────────────────────────────────────────
// Metric Types (compatible with common_system/monitoring_system)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @enum metric_type
 * @brief Types of metrics supported by the monitoring system
 */
enum class metric_type {
    gauge,      ///< Instant value that can go up or down
    counter,    ///< Monotonic increasing value
    histogram,  ///< Distribution of values across buckets
    summary     ///< Statistical summary (min, max, mean, percentiles)
};

/**
 * @brief Convert metric type to string
 */
[[nodiscard]] inline std::string to_string(metric_type type) {
    switch (type) {
        case metric_type::gauge:
            return "gauge";
        case metric_type::counter:
            return "counter";
        case metric_type::histogram:
            return "histogram";
        case metric_type::summary:
            return "summary";
        default:
            return "unknown";
    }
}

/**
 * @struct metric_value
 * @brief Standard metric value structure with type information
 */
struct metric_value {
    std::string name;
    double value;
    metric_type type{metric_type::gauge};
    std::chrono::system_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> tags;

    metric_value() = default;

    metric_value(std::string n,
                 double v,
                 metric_type t = metric_type::gauge,
                 std::unordered_map<std::string, std::string> tg = {})
        : name(std::move(n))
        , value(v)
        , type(t)
        , timestamp(std::chrono::system_clock::now())
        , tags(std::move(tg)) {}
};

/**
 * @struct metrics_snapshot
 * @brief Complete snapshot of metrics at a point in time
 */
struct metrics_snapshot {
    std::vector<metric_value> metrics;
    std::chrono::system_clock::time_point capture_time;
    std::string source_id;

    metrics_snapshot()
        : capture_time(std::chrono::system_clock::now()) {}

    void add_metric(const std::string& name,
                    double value,
                    metric_type type = metric_type::gauge) {
        metrics.emplace_back(name, value, type);
    }
};

/**
 * @enum monitor_health_status
 * @brief Standard health status levels
 */
enum class monitor_health_status {
    healthy = 0,
    degraded = 1,
    unhealthy = 2,
    unknown = 3
};

/**
 * @struct health_check_result
 * @brief Result of a health check operation
 */
struct health_check_result {
    monitor_health_status status{monitor_health_status::unknown};
    std::string message;
    std::chrono::system_clock::time_point timestamp;
    std::chrono::milliseconds check_duration{0};
    std::unordered_map<std::string, std::string> metadata;

    health_check_result()
        : timestamp(std::chrono::system_clock::now()) {}

    [[nodiscard]] bool is_healthy() const {
        return status == monitor_health_status::healthy;
    }

    [[nodiscard]] bool is_operational() const {
        return status == monitor_health_status::healthy ||
               status == monitor_health_status::degraded;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// PACS Monitor Class
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @struct pacs_monitor_config
 * @brief Configuration for the PACS monitor
 */
struct pacs_monitor_config {
    /// Application Entity title for metric labels
    std::string ae_title{"PACS_SCP"};

    /// Enable association metrics collection
    bool enable_association_metrics{true};

    /// Enable DIMSE service metrics collection
    bool enable_service_metrics{true};

    /// Enable storage metrics collection
    bool enable_storage_metrics{true};

    /// Enable object pool metrics
    bool enable_pool_metrics{true};

    /// Enable unified CRTP-based metrics collector
    bool enable_unified_collector{true};

    /// Metric name prefix for Prometheus export
    std::string metric_prefix{"pacs"};
};

/**
 * @class pacs_monitor
 * @brief Unified PACS monitoring implementing IMonitor interface
 *
 * This class provides a unified monitoring interface for the PACS system,
 * integrating all DICOM-specific metric collectors and implementing the
 * IMonitor interface from common_system.
 *
 * Features:
 * - DICOM association metrics (active, peak, success rate)
 * - DIMSE service metrics (C-ECHO, C-STORE, C-FIND, C-MOVE, C-GET, N-*)
 * - Storage metrics (bytes transferred, images stored/retrieved)
 * - Object pool metrics (element, dataset, PDU buffer pools)
 * - Health check integration
 * - Prometheus-compatible metric export
 *
 * Thread Safety: All public methods are thread-safe.
 *
 * @example
 * @code
 * // Initialize the monitor
 * pacs_monitor_config config;
 * config.ae_title = "MY_PACS";
 * pacs_monitor monitor(config);
 *
 * // Record metrics
 * monitor.record_metric("custom_metric", 42.0);
 *
 * // Get all metrics
 * auto snapshot = monitor.get_metrics();
 *
 * // Export to Prometheus format
 * std::string prometheus_output = monitor.to_prometheus();
 *
 * // Health check
 * auto health = monitor.check_health();
 * if (!health.is_healthy()) {
 *     // Handle unhealthy state
 * }
 * @endcode
 */
class pacs_monitor {
public:
    // =========================================================================
    // Construction and Singleton Access
    // =========================================================================

    /**
     * @brief Construct a new PACS monitor
     * @param config Configuration options
     */
    explicit pacs_monitor(const pacs_monitor_config& config = {});

    /**
     * @brief Destructor
     */
    ~pacs_monitor() = default;

    /**
     * @brief Get the global singleton instance
     * @return Reference to the global monitor instance
     *
     * Thread-safe lazy initialization using Meyer's singleton pattern.
     */
    [[nodiscard]] static pacs_monitor& global_monitor() noexcept {
        static pacs_monitor instance;
        return instance;
    }

    // Non-copyable, non-movable
    pacs_monitor(const pacs_monitor&) = delete;
    pacs_monitor& operator=(const pacs_monitor&) = delete;
    pacs_monitor(pacs_monitor&&) = delete;
    pacs_monitor& operator=(pacs_monitor&&) = delete;

    // =========================================================================
    // IMonitor Interface Methods
    // =========================================================================

    /**
     * @brief Record a metric value
     * @param name Metric name
     * @param value Metric value
     */
    void record_metric(std::string_view name, double value);

    /**
     * @brief Record a metric with tags
     * @param name Metric name
     * @param value Metric value
     * @param tags Additional metadata tags
     */
    void record_metric(std::string_view name,
                       double value,
                       const std::unordered_map<std::string, std::string>& tags);

    /**
     * @brief Get current metrics snapshot
     * @return Metrics snapshot containing all collected metrics
     */
    [[nodiscard]] metrics_snapshot get_metrics();

    /**
     * @brief Perform health check
     * @return Health check result
     */
    [[nodiscard]] health_check_result check_health();

    /**
     * @brief Reset all metrics
     */
    void reset();

    // =========================================================================
    // Prometheus Export
    // =========================================================================

    /**
     * @brief Export all metrics in Prometheus text format
     * @return Prometheus exposition format text
     */
    [[nodiscard]] std::string to_prometheus() const;

    /**
     * @brief Export all metrics as JSON
     * @return JSON representation of all metrics
     */
    [[nodiscard]] std::string to_json() const;

    // =========================================================================
    // Health Check Registration
    // =========================================================================

    /**
     * @brief Register a health check for a component
     * @param component Component name
     * @param check Function that returns true if healthy
     */
    void register_health_check(std::string_view component,
                               std::function<bool()> check);

    /**
     * @brief Unregister a health check
     * @param component Component name to unregister
     */
    void unregister_health_check(std::string_view component);

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Get current configuration
     * @return Current monitor configuration
     */
    [[nodiscard]] const pacs_monitor_config& get_config() const;

    /**
     * @brief Update configuration
     * @param config New configuration
     */
    void update_config(const pacs_monitor_config& config);

    // =========================================================================
    // Collector Access
    // =========================================================================

    /**
     * @brief Get the association collector
     * @return Reference to the association collector
     */
    [[nodiscard]] dicom_association_collector& association_collector();

    /**
     * @brief Get the service collector
     * @return Reference to the service collector
     */
    [[nodiscard]] dicom_service_collector& service_collector();

    /**
     * @brief Get the storage collector
     * @return Reference to the storage collector
     */
    [[nodiscard]] dicom_storage_collector& storage_collector();

    /**
     * @brief Get the unified CRTP-based metrics collector
     * @return Reference to the unified metrics collector
     * @see Issue #490 - CRTP-based DICOM metrics collector
     */
    [[nodiscard]] dicom_metrics_collector& unified_collector();

    /**
     * @brief Get a snapshot from the unified collector
     * @return Metrics snapshot with all DICOM metrics
     */
    [[nodiscard]] dicom_metrics_snapshot get_unified_snapshot() const;

private:
    // Configuration
    pacs_monitor_config config_;
    mutable std::shared_mutex config_mutex_;

    // Collectors
    std::unique_ptr<dicom_association_collector> association_collector_;
    std::unique_ptr<dicom_service_collector> service_collector_;
    std::unique_ptr<dicom_storage_collector> storage_collector_;
    std::unique_ptr<dicom_metrics_collector> unified_collector_;

    // Custom metrics
    mutable std::mutex custom_metrics_mutex_;
    std::vector<metric_value> custom_metrics_;

    // Health checks
    mutable std::mutex health_checks_mutex_;
    std::unordered_map<std::string, std::function<bool()>> health_checks_;

    // Helper methods
    void initialize_collectors();
    void collect_all_metrics(metrics_snapshot& snapshot);
};

// ─────────────────────────────────────────────────────────────────────────────
// Inline Implementation
// ─────────────────────────────────────────────────────────────────────────────

inline pacs_monitor::pacs_monitor(const pacs_monitor_config& config)
    : config_(config) {
    initialize_collectors();
}

inline void pacs_monitor::initialize_collectors() {
    association_collector_ = std::make_unique<dicom_association_collector>(config_.ae_title);
    service_collector_ = std::make_unique<dicom_service_collector>(config_.ae_title);
    storage_collector_ = std::make_unique<dicom_storage_collector>(config_.ae_title);
    unified_collector_ = std::make_unique<dicom_metrics_collector>(config_.ae_title);

    // Initialize all collectors
    std::unordered_map<std::string, std::string> collector_config;
    collector_config["ae_title"] = config_.ae_title;

    (void)association_collector_->initialize(collector_config);
    (void)service_collector_->initialize(collector_config);
    (void)storage_collector_->initialize(collector_config);
    storage_collector_->set_pool_metrics_enabled(config_.enable_pool_metrics);

    // Initialize unified CRTP-based collector
    config_map unified_config;
    unified_config["ae_title"] = config_.ae_title;
    unified_config["collect_associations"] = config_.enable_association_metrics ? "true" : "false";
    unified_config["collect_transfers"] = config_.enable_storage_metrics ? "true" : "false";
    unified_config["collect_storage"] = config_.enable_storage_metrics ? "true" : "false";
    unified_config["collect_queries"] = config_.enable_service_metrics ? "true" : "false";
    unified_config["collect_pools"] = config_.enable_pool_metrics ? "true" : "false";
    (void)unified_collector_->initialize(unified_config);
}

inline void pacs_monitor::record_metric(std::string_view name, double value) {
    std::lock_guard<std::mutex> lock(custom_metrics_mutex_);
    custom_metrics_.emplace_back(std::string(name), value);
}

inline void pacs_monitor::record_metric(
    std::string_view name,
    double value,
    const std::unordered_map<std::string, std::string>& tags) {
    std::lock_guard<std::mutex> lock(custom_metrics_mutex_);
    custom_metrics_.emplace_back(std::string(name), value, metric_type::gauge, tags);
}

inline metrics_snapshot pacs_monitor::get_metrics() {
    metrics_snapshot snapshot;
    snapshot.source_id = config_.ae_title;
    collect_all_metrics(snapshot);
    return snapshot;
}

inline void pacs_monitor::collect_all_metrics(metrics_snapshot& snapshot) {
    std::shared_lock<std::shared_mutex> config_lock(config_mutex_);

    // Collect association metrics
    if (config_.enable_association_metrics && association_collector_) {
        for (const auto& m : association_collector_->collect()) {
            snapshot.add_metric(
                m.name,
                m.value,
                m.type == "counter" ? metric_type::counter : metric_type::gauge);
        }
    }

    // Collect service metrics
    if (config_.enable_service_metrics && service_collector_) {
        for (const auto& m : service_collector_->collect()) {
            snapshot.add_metric(
                m.name,
                m.value,
                m.type == "counter" ? metric_type::counter : metric_type::gauge);
        }
    }

    // Collect storage metrics
    if (config_.enable_storage_metrics && storage_collector_) {
        for (const auto& m : storage_collector_->collect()) {
            snapshot.add_metric(
                m.name,
                m.value,
                m.type == "counter" ? metric_type::counter : metric_type::gauge);
        }
    }

    // Add custom metrics
    {
        std::lock_guard<std::mutex> lock(custom_metrics_mutex_);
        for (const auto& m : custom_metrics_) {
            snapshot.metrics.push_back(m);
        }
    }
}

inline health_check_result pacs_monitor::check_health() {
    const auto start = std::chrono::steady_clock::now();
    health_check_result result;
    result.status = monitor_health_status::healthy;

    std::lock_guard<std::mutex> lock(health_checks_mutex_);

    bool all_healthy = true;
    bool any_check_failed = false;

    for (const auto& [component, check] : health_checks_) {
        try {
            if (!check()) {
                all_healthy = false;
                result.metadata[component] = "unhealthy";
            } else {
                result.metadata[component] = "healthy";
            }
        } catch (const std::exception& e) {
            any_check_failed = true;
            result.metadata[component] = std::string("error: ") + e.what();
        }
    }

    // Check collectors health
    if (association_collector_ && !association_collector_->is_healthy()) {
        all_healthy = false;
        result.metadata["association_collector"] = "unhealthy";
    }
    if (service_collector_ && !service_collector_->is_healthy()) {
        all_healthy = false;
        result.metadata["service_collector"] = "unhealthy";
    }
    if (storage_collector_ && !storage_collector_->is_healthy()) {
        all_healthy = false;
        result.metadata["storage_collector"] = "unhealthy";
    }
    if (unified_collector_ && !unified_collector_->is_healthy()) {
        all_healthy = false;
        result.metadata["unified_collector"] = "unhealthy";
    }

    if (any_check_failed) {
        result.status = monitor_health_status::unhealthy;
        result.message = "Some health checks threw exceptions";
    } else if (!all_healthy) {
        result.status = monitor_health_status::degraded;
        result.message = "Some components are unhealthy";
    } else {
        result.message = "All components healthy";
    }

    const auto end = std::chrono::steady_clock::now();
    result.check_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    return result;
}

inline void pacs_monitor::reset() {
    // Reset underlying metrics
    pacs_metrics::global_metrics().reset();

    // Clear custom metrics
    {
        std::lock_guard<std::mutex> lock(custom_metrics_mutex_);
        custom_metrics_.clear();
    }
}

inline std::string pacs_monitor::to_prometheus() const {
    // Use existing pacs_metrics Prometheus export
    return pacs_metrics::global_metrics().to_prometheus(config_.metric_prefix);
}

inline std::string pacs_monitor::to_json() const {
    // Use existing pacs_metrics JSON export
    return pacs_metrics::global_metrics().to_json();
}

inline void pacs_monitor::register_health_check(
    std::string_view component,
    std::function<bool()> check) {
    std::lock_guard<std::mutex> lock(health_checks_mutex_);
    health_checks_[std::string(component)] = std::move(check);
}

inline void pacs_monitor::unregister_health_check(std::string_view component) {
    std::lock_guard<std::mutex> lock(health_checks_mutex_);
    health_checks_.erase(std::string(component));
}

inline const pacs_monitor_config& pacs_monitor::get_config() const {
    std::shared_lock<std::shared_mutex> lock(config_mutex_);
    return config_;
}

inline void pacs_monitor::update_config(const pacs_monitor_config& config) {
    std::unique_lock<std::shared_mutex> lock(config_mutex_);
    config_ = config;

    // Update collector configurations
    if (association_collector_) {
        association_collector_->set_ae_title(config_.ae_title);
    }
    if (service_collector_) {
        service_collector_->set_ae_title(config_.ae_title);
    }
    if (storage_collector_) {
        storage_collector_->set_ae_title(config_.ae_title);
        storage_collector_->set_pool_metrics_enabled(config_.enable_pool_metrics);
    }
    if (unified_collector_) {
        unified_collector_->set_ae_title(config_.ae_title);
        unified_collector_->set_collect_associations(config_.enable_association_metrics);
        unified_collector_->set_collect_transfers(config_.enable_storage_metrics);
        unified_collector_->set_collect_storage(config_.enable_storage_metrics);
        unified_collector_->set_collect_queries(config_.enable_service_metrics);
        unified_collector_->set_collect_pools(config_.enable_pool_metrics);
    }
}

inline dicom_association_collector& pacs_monitor::association_collector() {
    return *association_collector_;
}

inline dicom_service_collector& pacs_monitor::service_collector() {
    return *service_collector_;
}

inline dicom_storage_collector& pacs_monitor::storage_collector() {
    return *storage_collector_;
}

inline dicom_metrics_collector& pacs_monitor::unified_collector() {
    return *unified_collector_;
}

inline dicom_metrics_snapshot pacs_monitor::get_unified_snapshot() const {
    if (unified_collector_) {
        return unified_collector_->get_snapshot();
    }
    return dicom_metrics_snapshot{};
}

}  // namespace pacs::monitoring
