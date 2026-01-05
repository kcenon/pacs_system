/**
 * @file dicom_metrics_collector.hpp
 * @brief CRTP-based unified DICOM metrics collector
 *
 * This file provides a comprehensive DICOM metrics collector that integrates
 * with the monitoring_system using the CRTP pattern for zero-overhead
 * polymorphism and efficient metric collection.
 *
 * @see Issue #490 - Implement CRTP-based DICOM metrics collector
 * @see monitoring_system/src/impl/battery_collector.cpp for pattern reference
 */

#pragma once

#include "dicom_collector_base.hpp"
#include "../pacs_metrics.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace pacs::monitoring {

/**
 * @struct dicom_metrics_snapshot
 * @brief Snapshot of all DICOM metrics at a point in time
 *
 * Provides a consolidated view of all DICOM operation metrics
 * for efficient access and serialization.
 */
struct dicom_metrics_snapshot {
    // Association metrics
    std::uint64_t total_associations{0};
    std::uint64_t active_associations{0};
    std::uint64_t failed_associations{0};
    std::uint64_t peak_active_associations{0};

    // Transfer metrics
    std::uint64_t images_sent{0};
    std::uint64_t images_received{0};
    std::uint64_t bytes_sent{0};
    std::uint64_t bytes_received{0};

    // Storage metrics
    std::uint64_t store_operations{0};
    std::uint64_t successful_stores{0};
    std::uint64_t failed_stores{0};
    double avg_store_latency_ms{0.0};

    // Query metrics
    std::uint64_t query_operations{0};
    std::uint64_t successful_queries{0};
    std::uint64_t failed_queries{0};
    double avg_query_latency_ms{0.0};

    // Timestamp
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @class dicom_metrics_collector
 * @brief CRTP-based unified DICOM metrics collector
 *
 * This collector gathers all DICOM-related metrics in a single, efficient
 * collection pass, following the monitoring_system's CRTP collector pattern.
 *
 * Collected metrics include:
 * - Association metrics (opened, closed, duration)
 * - Transfer metrics (images, bytes, rate)
 * - Storage metrics (operations, latency)
 * - Query metrics (C-FIND, C-MOVE)
 * - Pool metrics (element, dataset, PDU buffer)
 *
 * Performance targets (based on monitoring_system benchmarks):
 * - Metric collection: < 100ns per operation
 * - Memory overhead: < 1MB for counters
 * - CPU overhead: < 1% at 1000 ops/sec
 *
 * Thread Safety: All public methods are thread-safe.
 *
 * @example
 * @code
 * dicom_metrics_collector collector;
 * collector.initialize({{"ae_title", "MY_PACS"}});
 *
 * // Collect metrics
 * auto metrics = collector.collect();
 * for (const auto& m : metrics) {
 *     std::cout << m.name << ": " << m.value << "\n";
 * }
 *
 * // Get snapshot for quick access
 * auto snapshot = collector.get_snapshot();
 * std::cout << "Active associations: " << snapshot.active_associations << "\n";
 * @endcode
 */
class dicom_metrics_collector
    : public dicom_collector_base<dicom_metrics_collector> {
public:
    /// Collector name for CRTP base class
    static constexpr const char* collector_name = "dicom_metrics_collector";

    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Default constructor
     */
    dicom_metrics_collector() = default;

    /**
     * @brief Constructor with AE title
     * @param ae_title Application Entity title for metric labels
     */
    explicit dicom_metrics_collector(std::string ae_title);

    /**
     * @brief Destructor
     */
    ~dicom_metrics_collector() override = default;

    // Non-copyable, non-movable
    dicom_metrics_collector(const dicom_metrics_collector&) = delete;
    dicom_metrics_collector& operator=(const dicom_metrics_collector&) = delete;
    dicom_metrics_collector(dicom_metrics_collector&&) = delete;
    dicom_metrics_collector& operator=(dicom_metrics_collector&&) = delete;

    // =========================================================================
    // CRTP Interface Implementation
    // =========================================================================

    /**
     * @brief Collector-specific initialization
     * @param config Configuration options:
     *   - "collect_associations": "true"/"false" (default: true)
     *   - "collect_transfers": "true"/"false" (default: true)
     *   - "collect_storage": "true"/"false" (default: true)
     *   - "collect_queries": "true"/"false" (default: true)
     *   - "collect_pools": "true"/"false" (default: true)
     * @return true if initialization successful
     */
    bool do_initialize(const config_map& config);

    /**
     * @brief Collect all DICOM metrics
     * @return Vector of collected metrics
     */
    std::vector<dicom_metric> do_collect();

    /**
     * @brief Check if DICOM metrics collection is available
     * @return true (always available as pacs_metrics is always present)
     */
    [[nodiscard]] bool is_available() const;

    /**
     * @brief Get supported metric types
     * @return Vector of supported metric type names
     */
    [[nodiscard]] std::vector<std::string> do_get_metric_types() const;

    /**
     * @brief Add collector-specific statistics
     * @param stats Map to add statistics to
     */
    void do_add_statistics(stats_map& stats) const;

    // =========================================================================
    // DICOM-Specific Methods
    // =========================================================================

    /**
     * @brief Get a snapshot of current metrics
     * @return Metrics snapshot
     */
    [[nodiscard]] dicom_metrics_snapshot get_snapshot() const;

    /**
     * @brief Get last collected snapshot
     * @return Last metrics snapshot (thread-safe copy)
     */
    [[nodiscard]] dicom_metrics_snapshot get_last_snapshot() const;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Enable or disable association metrics collection
     * @param enabled Whether to collect association metrics
     */
    void set_collect_associations(bool enabled);

    /**
     * @brief Enable or disable transfer metrics collection
     * @param enabled Whether to collect transfer metrics
     */
    void set_collect_transfers(bool enabled);

    /**
     * @brief Enable or disable storage metrics collection
     * @param enabled Whether to collect storage metrics
     */
    void set_collect_storage(bool enabled);

    /**
     * @brief Enable or disable query metrics collection
     * @param enabled Whether to collect query metrics
     */
    void set_collect_queries(bool enabled);

    /**
     * @brief Enable or disable pool metrics collection
     * @param enabled Whether to collect pool metrics
     */
    void set_collect_pools(bool enabled);

private:
    // Configuration flags
    bool collect_associations_{true};
    bool collect_transfers_{true};
    bool collect_storage_{true};
    bool collect_queries_{true};
    bool collect_pools_{true};

    // Cached snapshot
    mutable std::mutex snapshot_mutex_;
    dicom_metrics_snapshot last_snapshot_;

    // Helper methods for metric collection
    void collect_association_metrics(std::vector<dicom_metric>& metrics);
    void collect_transfer_metrics(std::vector<dicom_metric>& metrics);
    void collect_storage_metrics(std::vector<dicom_metric>& metrics);
    void collect_query_metrics(std::vector<dicom_metric>& metrics);
    void collect_pool_metrics(std::vector<dicom_metric>& metrics);
    void collect_dimse_operation_metrics(
        std::vector<dicom_metric>& metrics,
        const std::string& op_name,
        const operation_counter& counter);
};

// ─────────────────────────────────────────────────────────────────────────────
// Inline Implementation
// ─────────────────────────────────────────────────────────────────────────────

inline dicom_metrics_collector::dicom_metrics_collector(std::string ae_title) {
    ae_title_ = std::move(ae_title);
}

inline bool dicom_metrics_collector::do_initialize(const config_map& config) {
    // Parse collection configuration
    if (auto it = config.find("collect_associations"); it != config.end()) {
        collect_associations_ = (it->second == "true" || it->second == "1");
    }
    if (auto it = config.find("collect_transfers"); it != config.end()) {
        collect_transfers_ = (it->second == "true" || it->second == "1");
    }
    if (auto it = config.find("collect_storage"); it != config.end()) {
        collect_storage_ = (it->second == "true" || it->second == "1");
    }
    if (auto it = config.find("collect_queries"); it != config.end()) {
        collect_queries_ = (it->second == "true" || it->second == "1");
    }
    if (auto it = config.find("collect_pools"); it != config.end()) {
        collect_pools_ = (it->second == "true" || it->second == "1");
    }

    return true;
}

inline std::vector<dicom_metric> dicom_metrics_collector::do_collect() {
    std::vector<dicom_metric> metrics;
    metrics.reserve(64);  // Pre-allocate for typical metric count

    if (collect_associations_) {
        collect_association_metrics(metrics);
    }
    if (collect_transfers_) {
        collect_transfer_metrics(metrics);
    }
    if (collect_storage_) {
        collect_storage_metrics(metrics);
    }
    if (collect_queries_) {
        collect_query_metrics(metrics);
    }
    if (collect_pools_) {
        collect_pool_metrics(metrics);
    }

    return metrics;
}

inline bool dicom_metrics_collector::is_available() const {
    return true;  // pacs_metrics singleton is always available
}

inline std::vector<std::string> dicom_metrics_collector::do_get_metric_types() const {
    std::vector<std::string> types;

    // Association metrics
    types.push_back("dicom_associations_active");
    types.push_back("dicom_associations_peak");
    types.push_back("dicom_associations_total");
    types.push_back("dicom_associations_rejected_total");
    types.push_back("dicom_associations_aborted_total");

    // Transfer metrics
    types.push_back("dicom_images_sent_total");
    types.push_back("dicom_images_received_total");
    types.push_back("dicom_bytes_sent_total");
    types.push_back("dicom_bytes_received_total");
    types.push_back("dicom_transfer_rate_mbps");

    // DIMSE operation metrics
    static constexpr std::array<const char*, 11> ops = {
        "c_echo", "c_store", "c_find", "c_move", "c_get",
        "n_create", "n_set", "n_get", "n_action", "n_event", "n_delete"
    };
    for (const auto* op : ops) {
        types.push_back(std::string("dicom_") + op + "_total");
        types.push_back(std::string("dicom_") + op + "_success_total");
        types.push_back(std::string("dicom_") + op + "_failure_total");
        types.push_back(std::string("dicom_") + op + "_duration_seconds");
    }

    // Pool metrics
    types.push_back("dicom_element_pool_hits");
    types.push_back("dicom_element_pool_misses");
    types.push_back("dicom_dataset_pool_hits");
    types.push_back("dicom_dataset_pool_misses");

    return types;
}

inline void dicom_metrics_collector::do_add_statistics(stats_map& stats) const {
    std::lock_guard<std::mutex> lock(snapshot_mutex_);

    stats["active_associations"] =
        static_cast<double>(last_snapshot_.active_associations);
    stats["total_images_processed"] =
        static_cast<double>(last_snapshot_.images_sent + last_snapshot_.images_received);
    stats["total_bytes_transferred"] =
        static_cast<double>(last_snapshot_.bytes_sent + last_snapshot_.bytes_received);
}

inline dicom_metrics_snapshot dicom_metrics_collector::get_snapshot() const {
    dicom_metrics_snapshot snapshot;
    snapshot.timestamp = std::chrono::system_clock::now();

    const auto& pacs = pacs_metrics::global_metrics();
    const auto& assoc = pacs.associations();
    const auto& transfer = pacs.transfer();

    // Association metrics
    snapshot.total_associations =
        assoc.total_established.load(std::memory_order_relaxed);
    snapshot.active_associations =
        assoc.current_active.load(std::memory_order_relaxed);
    snapshot.failed_associations =
        assoc.total_rejected.load(std::memory_order_relaxed) +
        assoc.total_aborted.load(std::memory_order_relaxed);
    snapshot.peak_active_associations =
        assoc.peak_active.load(std::memory_order_relaxed);

    // Transfer metrics
    snapshot.images_sent = 0;  // Not tracked separately
    snapshot.images_received =
        transfer.images_stored.load(std::memory_order_relaxed);
    snapshot.bytes_sent =
        transfer.bytes_sent.load(std::memory_order_relaxed);
    snapshot.bytes_received =
        transfer.bytes_received.load(std::memory_order_relaxed);

    // Storage metrics (from C-STORE counter)
    const auto& store = pacs.get_counter(dimse_operation::c_store);
    snapshot.store_operations = store.total_count();
    snapshot.successful_stores =
        store.success_count.load(std::memory_order_relaxed);
    snapshot.failed_stores =
        store.failure_count.load(std::memory_order_relaxed);
    snapshot.avg_store_latency_ms =
        static_cast<double>(store.average_duration_us()) / 1000.0;

    // Query metrics (from C-FIND counter)
    const auto& query = pacs.get_counter(dimse_operation::c_find);
    snapshot.query_operations = query.total_count();
    snapshot.successful_queries =
        query.success_count.load(std::memory_order_relaxed);
    snapshot.failed_queries =
        query.failure_count.load(std::memory_order_relaxed);
    snapshot.avg_query_latency_ms =
        static_cast<double>(query.average_duration_us()) / 1000.0;

    return snapshot;
}

inline dicom_metrics_snapshot dicom_metrics_collector::get_last_snapshot() const {
    std::lock_guard<std::mutex> lock(snapshot_mutex_);
    return last_snapshot_;
}

inline void dicom_metrics_collector::set_collect_associations(bool enabled) {
    collect_associations_ = enabled;
}

inline void dicom_metrics_collector::set_collect_transfers(bool enabled) {
    collect_transfers_ = enabled;
}

inline void dicom_metrics_collector::set_collect_storage(bool enabled) {
    collect_storage_ = enabled;
}

inline void dicom_metrics_collector::set_collect_queries(bool enabled) {
    collect_queries_ = enabled;
}

inline void dicom_metrics_collector::set_collect_pools(bool enabled) {
    collect_pools_ = enabled;
}

inline void dicom_metrics_collector::collect_association_metrics(
    std::vector<dicom_metric>& metrics) {

    const auto& assoc = pacs_metrics::global_metrics().associations();

    // Active associations (gauge)
    metrics.push_back(create_base_metric(
        "dicom_associations_active",
        static_cast<double>(assoc.current_active.load(std::memory_order_relaxed)),
        "gauge"));

    // Peak active associations (gauge)
    metrics.push_back(create_base_metric(
        "dicom_associations_peak",
        static_cast<double>(assoc.peak_active.load(std::memory_order_relaxed)),
        "gauge"));

    // Total established (counter)
    metrics.push_back(create_base_metric(
        "dicom_associations_total",
        static_cast<double>(assoc.total_established.load(std::memory_order_relaxed)),
        "counter"));

    // Rejected (counter)
    metrics.push_back(create_base_metric(
        "dicom_associations_rejected_total",
        static_cast<double>(assoc.total_rejected.load(std::memory_order_relaxed)),
        "counter"));

    // Aborted (counter)
    metrics.push_back(create_base_metric(
        "dicom_associations_aborted_total",
        static_cast<double>(assoc.total_aborted.load(std::memory_order_relaxed)),
        "counter"));

    // Success rate (gauge)
    const auto total = assoc.total_established.load(std::memory_order_relaxed);
    const auto rejected = assoc.total_rejected.load(std::memory_order_relaxed);
    const auto attempted = total + rejected;
    const double success_rate = (attempted > 0)
        ? static_cast<double>(total) / static_cast<double>(attempted)
        : 1.0;

    metrics.push_back(create_base_metric(
        "dicom_associations_success_rate",
        success_rate,
        "gauge"));
}

inline void dicom_metrics_collector::collect_transfer_metrics(
    std::vector<dicom_metric>& metrics) {

    const auto& transfer = pacs_metrics::global_metrics().transfer();

    // Images stored (counter)
    metrics.push_back(create_base_metric(
        "dicom_images_stored_total",
        static_cast<double>(transfer.images_stored.load(std::memory_order_relaxed)),
        "counter"));

    // Images retrieved (counter)
    metrics.push_back(create_base_metric(
        "dicom_images_retrieved_total",
        static_cast<double>(transfer.images_retrieved.load(std::memory_order_relaxed)),
        "counter"));

    // Bytes sent (counter)
    metrics.push_back(create_base_metric(
        "dicom_bytes_sent_total",
        static_cast<double>(transfer.bytes_sent.load(std::memory_order_relaxed)),
        "counter"));

    // Bytes received (counter)
    metrics.push_back(create_base_metric(
        "dicom_bytes_received_total",
        static_cast<double>(transfer.bytes_received.load(std::memory_order_relaxed)),
        "counter"));
}

inline void dicom_metrics_collector::collect_storage_metrics(
    std::vector<dicom_metric>& metrics) {

    const auto& pacs = pacs_metrics::global_metrics();

    // C-STORE operation metrics
    collect_dimse_operation_metrics(
        metrics, "c_store", pacs.get_counter(dimse_operation::c_store));
}

inline void dicom_metrics_collector::collect_query_metrics(
    std::vector<dicom_metric>& metrics) {

    const auto& pacs = pacs_metrics::global_metrics();

    // All query-related DIMSE operations
    collect_dimse_operation_metrics(
        metrics, "c_echo", pacs.get_counter(dimse_operation::c_echo));
    collect_dimse_operation_metrics(
        metrics, "c_find", pacs.get_counter(dimse_operation::c_find));
    collect_dimse_operation_metrics(
        metrics, "c_move", pacs.get_counter(dimse_operation::c_move));
    collect_dimse_operation_metrics(
        metrics, "c_get", pacs.get_counter(dimse_operation::c_get));

    // N-services
    collect_dimse_operation_metrics(
        metrics, "n_create", pacs.get_counter(dimse_operation::n_create));
    collect_dimse_operation_metrics(
        metrics, "n_set", pacs.get_counter(dimse_operation::n_set));
    collect_dimse_operation_metrics(
        metrics, "n_get", pacs.get_counter(dimse_operation::n_get));
    collect_dimse_operation_metrics(
        metrics, "n_action", pacs.get_counter(dimse_operation::n_action));
    collect_dimse_operation_metrics(
        metrics, "n_event", pacs.get_counter(dimse_operation::n_event));
    collect_dimse_operation_metrics(
        metrics, "n_delete", pacs.get_counter(dimse_operation::n_delete));
}

inline void dicom_metrics_collector::collect_pool_metrics(
    std::vector<dicom_metric>& metrics) {

    const auto& pacs = pacs_metrics::global_metrics();

    // Element pool
    const auto& elem_pool = pacs.element_pool();
    metrics.push_back(create_base_metric(
        "dicom_element_pool_acquisitions_total",
        static_cast<double>(elem_pool.total_acquisitions.load(std::memory_order_relaxed)),
        "counter",
        {{"pool", "element"}}));
    metrics.push_back(create_base_metric(
        "dicom_element_pool_hits_total",
        static_cast<double>(elem_pool.pool_hits.load(std::memory_order_relaxed)),
        "counter",
        {{"pool", "element"}}));
    metrics.push_back(create_base_metric(
        "dicom_element_pool_misses_total",
        static_cast<double>(elem_pool.pool_misses.load(std::memory_order_relaxed)),
        "counter",
        {{"pool", "element"}}));
    metrics.push_back(create_base_metric(
        "dicom_element_pool_hit_ratio",
        elem_pool.hit_ratio(),
        "gauge",
        {{"pool", "element"}}));

    // Dataset pool
    const auto& ds_pool = pacs.dataset_pool();
    metrics.push_back(create_base_metric(
        "dicom_dataset_pool_acquisitions_total",
        static_cast<double>(ds_pool.total_acquisitions.load(std::memory_order_relaxed)),
        "counter",
        {{"pool", "dataset"}}));
    metrics.push_back(create_base_metric(
        "dicom_dataset_pool_hits_total",
        static_cast<double>(ds_pool.pool_hits.load(std::memory_order_relaxed)),
        "counter",
        {{"pool", "dataset"}}));
    metrics.push_back(create_base_metric(
        "dicom_dataset_pool_misses_total",
        static_cast<double>(ds_pool.pool_misses.load(std::memory_order_relaxed)),
        "counter",
        {{"pool", "dataset"}}));
    metrics.push_back(create_base_metric(
        "dicom_dataset_pool_hit_ratio",
        ds_pool.hit_ratio(),
        "gauge",
        {{"pool", "dataset"}}));

    // PDU buffer pool
    const auto& pdu_pool = pacs.pdu_buffer_pool();
    metrics.push_back(create_base_metric(
        "dicom_pdu_buffer_pool_acquisitions_total",
        static_cast<double>(pdu_pool.total_acquisitions.load(std::memory_order_relaxed)),
        "counter",
        {{"pool", "pdu_buffer"}}));
    metrics.push_back(create_base_metric(
        "dicom_pdu_buffer_pool_hits_total",
        static_cast<double>(pdu_pool.pool_hits.load(std::memory_order_relaxed)),
        "counter",
        {{"pool", "pdu_buffer"}}));
    metrics.push_back(create_base_metric(
        "dicom_pdu_buffer_pool_misses_total",
        static_cast<double>(pdu_pool.pool_misses.load(std::memory_order_relaxed)),
        "counter",
        {{"pool", "pdu_buffer"}}));
    metrics.push_back(create_base_metric(
        "dicom_pdu_buffer_pool_hit_ratio",
        pdu_pool.hit_ratio(),
        "gauge",
        {{"pool", "pdu_buffer"}}));
}

inline void dicom_metrics_collector::collect_dimse_operation_metrics(
    std::vector<dicom_metric>& metrics,
    const std::string& op_name,
    const operation_counter& counter) {

    std::unordered_map<std::string, std::string> op_tags{{"operation", op_name}};

    // Total requests (counter)
    metrics.push_back(create_base_metric(
        "dicom_" + op_name + "_total",
        static_cast<double>(counter.total_count()),
        "counter",
        op_tags));

    // Successful requests (counter)
    metrics.push_back(create_base_metric(
        "dicom_" + op_name + "_success_total",
        static_cast<double>(counter.success_count.load(std::memory_order_relaxed)),
        "counter",
        op_tags));

    // Failed requests (counter)
    metrics.push_back(create_base_metric(
        "dicom_" + op_name + "_failure_total",
        static_cast<double>(counter.failure_count.load(std::memory_order_relaxed)),
        "counter",
        op_tags));

    // Duration metrics (only if there have been requests)
    if (counter.total_count() > 0) {
        // Average duration in seconds (gauge)
        metrics.push_back(create_base_metric(
            "dicom_" + op_name + "_duration_seconds_avg",
            static_cast<double>(counter.average_duration_us()) / 1'000'000.0,
            "gauge",
            op_tags));

        // Total duration in seconds (counter) - for rate calculations
        metrics.push_back(create_base_metric(
            "dicom_" + op_name + "_duration_seconds_sum",
            static_cast<double>(counter.total_duration_us.load(std::memory_order_relaxed)) / 1'000'000.0,
            "counter",
            op_tags));

        // Min duration (gauge)
        const auto min_us = counter.min_duration_us.load(std::memory_order_relaxed);
        if (min_us != UINT64_MAX) {
            metrics.push_back(create_base_metric(
                "dicom_" + op_name + "_duration_seconds_min",
                static_cast<double>(min_us) / 1'000'000.0,
                "gauge",
                op_tags));
        }

        // Max duration (gauge)
        const auto max_us = counter.max_duration_us.load(std::memory_order_relaxed);
        if (max_us > 0) {
            metrics.push_back(create_base_metric(
                "dicom_" + op_name + "_duration_seconds_max",
                static_cast<double>(max_us) / 1'000'000.0,
                "gauge",
                op_tags));
        }
    }
}

}  // namespace pacs::monitoring
