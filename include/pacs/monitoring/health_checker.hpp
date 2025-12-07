/**
 * @file health_checker.hpp
 * @brief Health check service for PACS system components
 *
 * This file provides the health_checker class that performs diagnostic checks
 * on all PACS system components and aggregates the results into a health_status
 * structure suitable for monitoring and load balancer integration.
 *
 * @see Issue #211 - Implement health check endpoint
 * @see DICOM PS3.7 - Message Exchange (Association Status)
 */

#pragma once

#include "health_status.hpp"

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace pacs::storage {
// Forward declarations
class index_database;
class file_storage;
}  // namespace pacs::storage

namespace pacs::monitoring {

/**
 * @struct health_checker_config
 * @brief Configuration options for the health checker
 */
struct health_checker_config {
    /// Interval between automatic health checks
    std::chrono::seconds check_interval{30};

    /// Timeout for database connectivity test
    std::chrono::milliseconds database_timeout{5000};

    /// Timeout for storage write test
    std::chrono::milliseconds storage_timeout{5000};

    /// Storage usage threshold for degraded status (percentage)
    double storage_warning_threshold{80.0};

    /// Storage usage threshold for unhealthy status (percentage)
    double storage_critical_threshold{95.0};

    /// Cache health check results for this duration
    std::chrono::seconds cache_duration{5};

    /// Enable background health checking
    bool background_checks_enabled{false};
};

/**
 * @class health_checker
 * @brief Performs comprehensive health checks on PACS system components
 *
 * The health_checker class provides a unified interface for monitoring the
 * health of all PACS system components including:
 * - Database connectivity and response time
 * - Storage read/write capability and capacity
 * - Active DICOM associations
 * - System resource utilization
 *
 * The checker supports both on-demand checks and cached results for high-
 * frequency health check requests (e.g., from Kubernetes liveness probes).
 *
 * Thread Safety: All public methods are thread-safe.
 *
 * @example
 * @code
 * // Create and configure the health checker
 * health_checker_config config;
 * config.check_interval = std::chrono::seconds{30};
 * config.storage_warning_threshold = 80.0;
 *
 * health_checker checker{config};
 *
 * // Register components to monitor
 * checker.set_database(&database);
 * checker.set_storage(&storage);
 *
 * // Perform health check
 * auto status = checker.check();
 * if (status.is_healthy()) {
 *     // System is healthy
 * }
 *
 * // Get cached result for frequent calls
 * auto cached = checker.get_cached_status();
 * @endcode
 */
class health_checker {
public:
    // =========================================================================
    // Type Aliases
    // =========================================================================

    /// Custom health check callback type
    using check_callback = std::function<bool(std::string& error_message)>;

    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct health checker with default configuration
     */
    health_checker();

    /**
     * @brief Construct health checker with custom configuration
     * @param config Configuration options
     */
    explicit health_checker(const health_checker_config& config);

    /**
     * @brief Destructor - stops background checking if enabled
     */
    ~health_checker();

    /// Non-copyable
    health_checker(const health_checker&) = delete;
    health_checker& operator=(const health_checker&) = delete;

    /// Movable
    health_checker(health_checker&& other) noexcept;
    health_checker& operator=(health_checker&& other) noexcept;

    // =========================================================================
    // Component Registration
    // =========================================================================

    /**
     * @brief Set the database instance to monitor
     * @param database Pointer to the index_database (nullable to disable check)
     */
    void set_database(pacs::storage::index_database* database);

    /**
     * @brief Set the storage instance to monitor
     * @param storage Pointer to the file_storage (nullable to disable check)
     */
    void set_storage(pacs::storage::file_storage* storage);

    /**
     * @brief Register a custom health check
     *
     * Custom checks allow extending the health checker with application-
     * specific health indicators.
     *
     * @param name Unique identifier for the check
     * @param callback Function that returns true if healthy, false otherwise
     */
    void register_check(std::string_view name, check_callback callback);

    /**
     * @brief Unregister a custom health check
     * @param name The check identifier to remove
     */
    void unregister_check(std::string_view name);

    // =========================================================================
    // Health Check Operations
    // =========================================================================

    /**
     * @brief Perform a full health check
     *
     * Runs all registered health checks and aggregates results. This method
     * may take time depending on configured timeouts.
     *
     * @return Comprehensive health status
     */
    [[nodiscard]] health_status check();

    /**
     * @brief Perform a quick liveness check
     *
     * A minimal check suitable for Kubernetes liveness probes.
     * Only verifies that the service is running.
     *
     * @return true if the service is alive
     */
    [[nodiscard]] bool is_alive() const noexcept;

    /**
     * @brief Perform a readiness check
     *
     * Checks if the service is ready to accept traffic.
     * Verifies database and storage connectivity.
     *
     * @return true if the service is ready
     */
    [[nodiscard]] bool is_ready();

    /**
     * @brief Get cached health status
     *
     * Returns the most recent health check result without performing
     * new checks. Useful for high-frequency monitoring requests.
     *
     * @return Cached health status (may be stale)
     */
    [[nodiscard]] health_status get_cached_status() const;

    /**
     * @brief Get cached status or perform check if stale
     *
     * Returns cached result if within cache_duration, otherwise
     * performs a fresh check.
     *
     * @return Health status (fresh or cached)
     */
    [[nodiscard]] health_status get_status();

    // =========================================================================
    // Metrics Access
    // =========================================================================

    /**
     * @brief Update association metrics
     *
     * Called by the DICOM server to update active association count.
     *
     * @param active Current number of active associations
     * @param max Maximum allowed associations
     * @param total_established Total associations since startup
     * @param total_failed Total failed association attempts
     */
    void update_association_metrics(std::uint32_t active,
                                    std::uint32_t max,
                                    std::uint64_t total_established,
                                    std::uint64_t total_failed);

    /**
     * @brief Update storage metrics
     *
     * Called by storage service to update storage statistics.
     *
     * @param instances Total stored instances
     * @param studies Total studies
     * @param series Total series
     * @param successful_stores Successful C-STORE operations
     * @param failed_stores Failed C-STORE operations
     */
    void update_storage_metrics(std::uint64_t instances,
                                std::uint64_t studies,
                                std::uint64_t series,
                                std::uint64_t successful_stores,
                                std::uint64_t failed_stores);

    /**
     * @brief Set version information
     *
     * @param major Major version
     * @param minor Minor version
     * @param patch Patch version
     * @param build_id Build identifier (e.g., git hash)
     */
    void set_version(std::uint16_t major,
                     std::uint16_t minor,
                     std::uint16_t patch,
                     std::string_view build_id = "");

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Get current configuration
     * @return Configuration reference
     */
    [[nodiscard]] const health_checker_config& config() const noexcept;

    /**
     * @brief Update configuration
     * @param config New configuration
     */
    void set_config(const health_checker_config& config);

private:
    // =========================================================================
    // Internal Check Methods
    // =========================================================================

    /**
     * @brief Check database connectivity
     * @param status Status to update
     */
    void check_database(health_status& status);

    /**
     * @brief Check storage availability
     * @param status Status to update
     */
    void check_storage(health_status& status);

    /**
     * @brief Run all custom checks
     * @param status Status to update
     */
    void run_custom_checks(health_status& status);

    // =========================================================================
    // Member Variables
    // =========================================================================

    /// Configuration
    health_checker_config config_;

    /// Database instance to monitor
    pacs::storage::index_database* database_{nullptr};

    /// Storage instance to monitor
    pacs::storage::file_storage* storage_{nullptr};

    /// Custom health checks
    std::unordered_map<std::string, check_callback> custom_checks_;

    /// Cached health status
    health_status cached_status_;

    /// Timestamp of last check
    std::chrono::system_clock::time_point last_check_time_;

    /// Association metrics (updated externally)
    association_metrics associations_;

    /// Storage metrics (updated externally)
    storage_metrics storage_metrics_;

    /// Version information
    version_info version_;

    /// Mutex for thread safety
    mutable std::shared_mutex mutex_;
};

}  // namespace pacs::monitoring
