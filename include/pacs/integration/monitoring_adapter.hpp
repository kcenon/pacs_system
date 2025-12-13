/**
 * @file monitoring_adapter.hpp
 * @brief Adapter for PACS performance metrics and distributed tracing
 *
 * This file provides the monitoring_adapter class for integrating monitoring_system
 * with PACS operations. It supports performance metrics collection, distributed
 * tracing for request tracking, and health check functionality.
 *
 * @see IR-5 (monitoring_system Integration), NFR-1 (Performance)
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace pacs::integration {

// ─────────────────────────────────────────────────────
// Forward Declarations
// ─────────────────────────────────────────────────────

// Note: query_level is defined in logger_adapter.hpp
// If logger_adapter.hpp is not included, we define it here
#ifndef PACS_INTEGRATION_QUERY_LEVEL_DEFINED
#define PACS_INTEGRATION_QUERY_LEVEL_DEFINED
/**
 * @enum query_level
 * @brief DICOM query retrieve level for metrics
 */
enum class query_level { patient, study, series, image };
#endif

// ─────────────────────────────────────────────────────
// Configuration
// ─────────────────────────────────────────────────────

/**
 * @struct monitoring_config
 * @brief Configuration options for the monitoring adapter
 */
struct monitoring_config {
    /// Enable metrics collection
    bool enable_metrics{true};

    /// Enable distributed tracing
    bool enable_tracing{true};

    /// Interval for exporting metrics
    std::chrono::seconds export_interval{30};

    /// Prometheus-style metrics endpoint port (nullopt = disabled)
    std::optional<std::uint16_t> metrics_port;

    /// Jaeger/Zipkin-style tracing endpoint (nullopt = disabled)
    std::optional<std::string> tracing_endpoint;

    /// Service name for tracing
    std::string service_name{"pacs_server"};

    /// Maximum samples to keep per operation
    std::size_t max_samples_per_operation{10000};
};

// ─────────────────────────────────────────────────────
// Monitoring Adapter Class
// ─────────────────────────────────────────────────────

/**
 * @class monitoring_adapter
 * @brief Adapter for PACS performance metrics and distributed tracing
 *
 * This class provides a unified interface for monitoring in the PACS system,
 * including:
 * - Performance metrics (counters, gauges, histograms, timing)
 * - DICOM-specific metrics (C-STORE, C-FIND, associations, storage)
 * - Distributed tracing for request tracking across services
 * - Health check endpoints for component status
 *
 * The adapter uses monitoring_system's high-performance profiling
 * internally while providing a PACS-specific API.
 *
 * Thread Safety: All methods are thread-safe.
 *
 * @example
 * @code
 * // Initialize the monitoring adapter
 * monitoring_config config;
 * config.metrics_port = 9090;
 * config.service_name = "pacs_server";
 * monitoring_adapter::initialize(config);
 *
 * // Record DICOM metrics
 * monitoring_adapter::record_c_store(
 *     std::chrono::milliseconds(150),
 *     1024 * 1024,  // 1MB
 *     true);
 *
 * // Use distributed tracing
 * {
 *     auto span = monitoring_adapter::start_span("c_store_operation");
 *     span.set_tag("sop_class", "1.2.840.10008.5.1.4.1.1.2");
 *     // ... do work ...
 * } // span automatically finished
 *
 * // Health check
 * auto health = monitoring_adapter::get_health();
 * if (!health.healthy) {
 *     // Handle unhealthy state
 * }
 *
 * // Shutdown
 * monitoring_adapter::shutdown();
 * @endcode
 */
class monitoring_adapter {
public:
    // ─────────────────────────────────────────────────────
    // Initialization
    // ─────────────────────────────────────────────────────

    /**
     * @brief Initialize the monitoring adapter with configuration
     *
     * Must be called before any monitoring operations. Sets up metrics
     * collection, tracing, and health check endpoints if configured.
     *
     * @param config Configuration options
     */
    static void initialize(const monitoring_config& config);

    /**
     * @brief Shutdown the monitoring adapter
     *
     * Flushes all pending metrics and releases resources.
     * Should be called before application exit.
     */
    static void shutdown();

    /**
     * @brief Check if the monitoring adapter is initialized
     * @return true if initialized, false otherwise
     */
    [[nodiscard]] static auto is_initialized() noexcept -> bool;

    // ─────────────────────────────────────────────────────
    // Metrics
    // ─────────────────────────────────────────────────────

    /**
     * @brief Increment a counter metric
     * @param name The counter name
     * @param value The value to add (default: 1)
     */
    static void increment_counter(std::string_view name, std::int64_t value = 1);

    /**
     * @brief Set a gauge metric value
     * @param name The gauge name
     * @param value The current value
     */
    static void set_gauge(std::string_view name, double value);

    /**
     * @brief Record a histogram sample
     * @param name The histogram name
     * @param value The sample value
     */
    static void record_histogram(std::string_view name, double value);

    /**
     * @brief Record a timing measurement
     * @param name The timing metric name
     * @param duration The duration to record
     */
    static void record_timing(std::string_view name,
                              std::chrono::nanoseconds duration);

    // ─────────────────────────────────────────────────────
    // DICOM-Specific Metrics
    // ─────────────────────────────────────────────────────

    /**
     * @brief Record C-STORE operation metrics
     *
     * Records performance metrics for a DICOM C-STORE operation including
     * duration, bytes transferred, and success/failure status.
     *
     * Metrics recorded:
     * - pacs_c_store_total (counter)
     * - pacs_c_store_duration_seconds (histogram)
     * - pacs_c_store_bytes_total (counter)
     *
     * @param duration Time taken for the operation
     * @param bytes Number of bytes transferred
     * @param success Whether the operation succeeded
     */
    static void record_c_store(std::chrono::nanoseconds duration,
                               std::size_t bytes,
                               bool success);

    /**
     * @brief Record C-FIND operation metrics
     *
     * Records performance metrics for a DICOM C-FIND query operation.
     *
     * Metrics recorded:
     * - pacs_c_find_total (counter)
     * - pacs_c_find_duration_seconds (histogram)
     * - pacs_c_find_matches (histogram)
     *
     * @param duration Time taken for the query
     * @param matches Number of matching records returned
     * @param level Query retrieve level (patient, study, series, image)
     */
    static void record_c_find(std::chrono::nanoseconds duration,
                              std::size_t matches,
                              query_level level);

    /**
     * @brief Record DICOM association metrics
     *
     * Records metrics for DICOM association establishment/release.
     *
     * Metrics recorded:
     * - pacs_associations_total (counter)
     * - pacs_associations_active (gauge)
     *
     * @param calling_ae AE title of the remote system
     * @param established true if association was established, false if released
     */
    static void record_association(const std::string& calling_ae,
                                   bool established);

    /**
     * @brief Update storage statistics
     *
     * Updates gauge metrics for storage usage.
     *
     * Metrics recorded:
     * - pacs_storage_instances (gauge)
     * - pacs_storage_bytes (gauge)
     *
     * @param total_instances Total number of stored DICOM instances
     * @param total_bytes Total storage used in bytes
     */
    static void update_storage_stats(std::size_t total_instances,
                                     std::size_t total_bytes);

    // ─────────────────────────────────────────────────────
    // Distributed Tracing
    // ─────────────────────────────────────────────────────

    /**
     * @class span
     * @brief Represents a unit of work in distributed tracing
     *
     * A span tracks the execution of a single operation, recording
     * timing, tags, and events. Spans are automatically finished
     * when they go out of scope (RAII pattern).
     */
    class span {
    public:
        /**
         * @brief Construct a new span
         * @param operation_name Name of the operation being traced
         */
        explicit span(std::string_view operation_name);

        /**
         * @brief Destructor - automatically finishes the span
         */
        ~span();

        // Disable copy
        span(const span&) = delete;
        span& operator=(const span&) = delete;

        // Enable move
        span(span&& other) noexcept;
        span& operator=(span&& other) noexcept;

        /**
         * @brief Set a tag on the span
         * @param key Tag name
         * @param value Tag value
         */
        void set_tag(std::string_view key, std::string_view value);

        /**
         * @brief Add an event to the span
         * @param name Event name
         */
        void add_event(std::string_view name);

        /**
         * @brief Mark the span as an error
         * @param e The exception that occurred
         */
        void set_error(const std::exception& e);

        /**
         * @brief Get the trace ID
         * @return The trace identifier
         */
        [[nodiscard]] auto trace_id() const -> std::string;

        /**
         * @brief Get the span ID
         * @return The span identifier
         */
        [[nodiscard]] auto span_id() const -> std::string;

        /**
         * @brief Check if span is valid (properly initialized)
         * @return true if span is valid
         */
        [[nodiscard]] auto is_valid() const noexcept -> bool;

    private:
        class impl;
        std::unique_ptr<impl> impl_;
    };

    /**
     * @brief Start a new trace span
     * @param operation Name of the operation
     * @return A new span for the operation
     */
    [[nodiscard]] static auto start_span(std::string_view operation) -> span;

    // ─────────────────────────────────────────────────────
    // Health Check
    // ─────────────────────────────────────────────────────

    /**
     * @struct health_status
     * @brief Health check result containing component status
     */
    struct health_status {
        /// Overall health status
        bool healthy{true};

        /// Human-readable status message
        std::string status{"healthy"};

        /// Per-component health status
        std::map<std::string, std::string> components;
    };

    /**
     * @brief Get current health status
     * @return Health status of all registered components
     */
    [[nodiscard]] static auto get_health() -> health_status;

    /**
     * @brief Register a health check for a component
     *
     * The provided function will be called during health checks.
     * It should return true if the component is healthy.
     *
     * @param component Name of the component
     * @param check Function that returns true if healthy
     */
    static void register_health_check(std::string_view component,
                                      std::function<bool()> check);

    /**
     * @brief Unregister a health check
     * @param component Name of the component to unregister
     */
    static void unregister_health_check(std::string_view component);

    // ─────────────────────────────────────────────────────
    // Configuration
    // ─────────────────────────────────────────────────────

    /**
     * @brief Get the current configuration
     * @return Current monitoring configuration
     */
    [[nodiscard]] static auto get_config() -> const monitoring_config&;

private:
    // Private implementation helpers
    [[nodiscard]] static auto query_level_to_string(query_level level)
        -> std::string;

    // Internal state (managed through pimpl or static members)
    class impl;
    static std::unique_ptr<impl> pimpl_;
};

}  // namespace pacs::integration
