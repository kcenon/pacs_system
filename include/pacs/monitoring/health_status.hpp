/**
 * @file health_status.hpp
 * @brief Health status data structures for PACS system monitoring
 *
 * This file defines the health_status struct that tracks the overall health
 * of the PACS system including database connectivity, storage availability,
 * active associations, and version information.
 *
 * @see Issue #211 - Implement health check endpoint
 * @see DICOM PS3.7 - Message Exchange (Association Status)
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace pacs::monitoring {

/**
 * @enum health_level
 * @brief Overall health level indicator
 *
 * Represents the aggregated health status of the system based on
 * individual component checks.
 */
enum class health_level {
    /// All components healthy, system fully operational
    healthy,

    /// Some non-critical components degraded, system operational
    degraded,

    /// Critical components failing, system may not function correctly
    unhealthy
};

/**
 * @brief Convert health level to string representation
 * @param level The health level to convert
 * @return String representation ("healthy", "degraded", "unhealthy")
 */
[[nodiscard]] constexpr std::string_view to_string(health_level level) noexcept {
    switch (level) {
        case health_level::healthy:
            return "healthy";
        case health_level::degraded:
            return "degraded";
        case health_level::unhealthy:
            return "unhealthy";
        default:
            return "unknown";
    }
}

/**
 * @struct database_status
 * @brief Database connection health information
 */
struct database_status {
    /// Whether database connection is active
    bool connected{false};

    /// Last successful connection timestamp
    std::optional<std::chrono::system_clock::time_point> last_connected;

    /// Number of active database connections
    std::uint32_t active_connections{0};

    /// Database response time in milliseconds
    std::optional<std::chrono::milliseconds> response_time;

    /// Error message if connection failed
    std::optional<std::string> error_message;
};

/**
 * @struct storage_status
 * @brief Storage subsystem health information
 */
struct storage_status {
    /// Whether storage is writable
    bool writable{false};

    /// Whether storage is readable
    bool readable{false};

    /// Total storage capacity in bytes
    std::uint64_t total_bytes{0};

    /// Used storage space in bytes
    std::uint64_t used_bytes{0};

    /// Available storage space in bytes
    std::uint64_t available_bytes{0};

    /// Storage usage percentage (0-100)
    [[nodiscard]] double usage_percent() const noexcept {
        if (total_bytes == 0) {
            return 0.0;
        }
        return static_cast<double>(used_bytes) / static_cast<double>(total_bytes) *
               100.0;
    }

    /// Error message if storage check failed
    std::optional<std::string> error_message;
};

/**
 * @struct association_metrics
 * @brief DICOM association statistics
 */
struct association_metrics {
    /// Number of currently active associations
    std::uint32_t active_associations{0};

    /// Maximum concurrent associations allowed
    std::uint32_t max_associations{100};

    /// Total associations since server start
    std::uint64_t total_associations{0};

    /// Number of failed associations
    std::uint64_t failed_associations{0};
};

/**
 * @struct storage_metrics
 * @brief DICOM storage operation statistics
 */
struct storage_metrics {
    /// Total DICOM instances stored
    std::uint64_t total_instances{0};

    /// Total studies in the archive
    std::uint64_t total_studies{0};

    /// Total series in the archive
    std::uint64_t total_series{0};

    /// Successful C-STORE operations
    std::uint64_t successful_stores{0};

    /// Failed C-STORE operations
    std::uint64_t failed_stores{0};
};

/**
 * @struct version_info
 * @brief PACS system version information
 */
struct version_info {
    /// Major version number
    std::uint16_t major{1};

    /// Minor version number
    std::uint16_t minor{0};

    /// Patch version number
    std::uint16_t patch{0};

    /// Build identifier (e.g., git commit hash)
    std::string build_id;

    /// Server startup timestamp
    std::chrono::system_clock::time_point startup_time{
        std::chrono::system_clock::now()};

    /// Calculate uptime duration
    [[nodiscard]] std::chrono::seconds uptime() const noexcept {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now() - startup_time);
    }

    /// Get version as string (e.g., "1.0.0")
    [[nodiscard]] std::string version_string() const {
        return std::to_string(major) + "." + std::to_string(minor) + "." +
               std::to_string(patch);
    }
};

/**
 * @struct health_status
 * @brief Comprehensive health status of the PACS system
 *
 * This struct aggregates all component health information into a single
 * data structure suitable for health check endpoints and monitoring systems.
 *
 * Thread Safety: Read operations are thread-safe. Write operations require
 * external synchronization.
 *
 * @example
 * @code
 * health_status status;
 * status.level = health_level::healthy;
 * status.database.connected = true;
 * status.storage.writable = true;
 *
 * // Serialize to JSON for REST API response
 * std::string json = to_json(status);
 * @endcode
 */
struct health_status {
    /// Overall health level
    health_level level{health_level::unhealthy};

    /// Timestamp of this health check
    std::chrono::system_clock::time_point timestamp{
        std::chrono::system_clock::now()};

    /// Database connection status
    database_status database;

    /// Storage subsystem status
    storage_status storage;

    /// DICOM association metrics
    association_metrics associations;

    /// Storage operation metrics
    storage_metrics metrics;

    /// Version and uptime information
    version_info version;

    /// Optional human-readable status message
    std::optional<std::string> message;

    /**
     * @brief Calculate overall health level from component status
     *
     * Health level determination:
     * - healthy: Database connected AND storage writable AND readable
     * - degraded: Database connected but storage issues
     * - unhealthy: Database disconnected OR critical failures
     */
    void update_level() noexcept {
        if (!database.connected) {
            level = health_level::unhealthy;
            return;
        }

        if (!storage.writable || !storage.readable) {
            level = health_level::unhealthy;
            return;
        }

        // Check for degraded conditions
        if (storage.usage_percent() > 90.0) {
            level = health_level::degraded;
            return;
        }

        if (associations.active_associations >=
            associations.max_associations * 0.9) {
            level = health_level::degraded;
            return;
        }

        level = health_level::healthy;
    }

    /**
     * @brief Check if the system is healthy
     * @return true if health level is healthy
     */
    [[nodiscard]] bool is_healthy() const noexcept {
        return level == health_level::healthy;
    }

    /**
     * @brief Check if the system is at least operational
     * @return true if health level is healthy or degraded
     */
    [[nodiscard]] bool is_operational() const noexcept {
        return level != health_level::unhealthy;
    }
};

}  // namespace pacs::monitoring
