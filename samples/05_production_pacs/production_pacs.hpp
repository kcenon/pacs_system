/**
 * @file production_pacs.hpp
 * @brief Production-grade PACS with enterprise features
 *
 * This file provides a production-ready PACS implementation demonstrating:
 * - TLS security for DICOM connections
 * - Role-based access control (RBAC)
 * - Data anonymization profiles
 * - REST API for web access
 * - Health monitoring and metrics
 * - Event-driven architecture
 *
 * @see DICOM PS3.15 - Security Profiles
 * @see DICOM PS3.4 - Service Class Specifications
 */

#pragma once

#include "config_loader.hpp"
#include "../04_mini_pacs/mini_pacs.hpp"

#include <pacs/security/access_control_manager.hpp>
#include <pacs/security/anonymizer.hpp>
#include <pacs/monitoring/health_checker.hpp>
#include <pacs/monitoring/pacs_metrics.hpp>
#include <pacs/web/rest_server.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace pacs::samples {

// =============================================================================
// Event Types
// =============================================================================

namespace events {

/**
 * @brief Event fired when an image is received and stored
 */
struct image_received_event {
    std::string sop_instance_uid;
    std::string sop_class_uid;
    std::string patient_id;
    std::string study_instance_uid;
    std::string calling_ae;
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief Event fired when a query is executed
 */
struct query_executed_event {
    std::string query_level;
    std::size_t result_count;
    std::string calling_ae;
    std::chrono::milliseconds duration;
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief Event fired for association lifecycle events
 */
struct association_event {
    enum class type { opened, closed, rejected };

    type event_type;
    std::string calling_ae;
    std::string called_ae;
    std::string reason;
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief Event fired when access is denied
 */
struct access_denied_event {
    std::string calling_ae;
    std::string operation;
    std::string reason;
    std::chrono::system_clock::time_point timestamp;
};

}  // namespace events

// =============================================================================
// Production PACS Statistics
// =============================================================================

/**
 * @brief Extended statistics for production PACS
 */
struct production_statistics {
    /// Total images stored
    std::atomic<std::uint64_t> images_stored{0};

    /// Total images anonymized
    std::atomic<std::uint64_t> images_anonymized{0};

    /// Total queries executed
    std::atomic<std::uint64_t> queries_executed{0};

    /// Total access denied events
    std::atomic<std::uint64_t> access_denied_count{0};

    /// Total REST API requests
    std::atomic<std::uint64_t> rest_requests{0};

    /// Current active DICOM associations
    std::atomic<std::uint32_t> active_associations{0};

    /// Server uptime start
    std::chrono::system_clock::time_point start_time;

    /// Get uptime duration
    [[nodiscard]] auto uptime() const -> std::chrono::seconds {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now() - start_time);
    }
};

// =============================================================================
// Production PACS Class
// =============================================================================

/**
 * @brief Production-grade PACS server with enterprise features
 *
 * The production_pacs class extends the mini_pacs with enterprise features:
 * - TLS encryption for secure DICOM communication
 * - Role-based access control for AE titles
 * - Automatic anonymization of incoming images
 * - REST API for web-based access
 * - Health monitoring and metrics collection
 * - Event-driven architecture for extensibility
 *
 * ## Architecture
 *
 * ```
 * ┌─────────────────────────────────────────────────────────────┐
 * │                    Production PACS                          │
 * │                                                             │
 * │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
 * │  │    Config    │  │   Security   │  │  Monitoring  │      │
 * │  │   (YAML)     │  │   Manager    │  │   Health     │      │
 * │  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘      │
 * │         │                 │                 │               │
 * │  ┌──────▼─────────────────▼─────────────────▼───────┐      │
 * │  │                    Mini PACS                      │      │
 * │  │         (All Level 4 Services)                   │      │
 * │  └──────────────────────┬───────────────────────────┘      │
 * │                         │                                   │
 * │  ┌──────────────────────▼───────────────────────────┐      │
 * │  │                REST API Server                    │      │
 * │  │  /api/v1/patients, /studies, /series, /health    │      │
 * │  └──────────────────────────────────────────────────┘      │
 * │                                                             │
 * │  ┌─────────────────────────────────────────────────────┐   │
 * │  │                    Event Bus                        │   │
 * │  │  image_received, query_executed, association_*     │   │
 * │  └─────────────────────────────────────────────────────┘   │
 * └─────────────────────────────────────────────────────────────┘
 * ```
 *
 * @example
 * @code
 * production_config config;
 * config.server.ae_title = "PROD_PACS";
 * config.server.port = 11112;
 * config.rest_api.enabled = true;
 * config.rest_api.port = 8080;
 *
 * production_pacs pacs(config);
 *
 * pacs.on_image_received([](const events::image_received_event& evt) {
 *     std::cout << "Received: " << evt.sop_instance_uid << "\n";
 * });
 *
 * if (!pacs.start()) {
 *     std::cerr << "Failed to start\n";
 *     return 1;
 * }
 *
 * pacs.wait();
 * @endcode
 */
class production_pacs {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct Production PACS with configuration
     * @param config Production configuration
     */
    explicit production_pacs(const production_config& config);

    /**
     * @brief Destructor - ensures clean shutdown
     */
    ~production_pacs();

    // Non-copyable, non-movable
    production_pacs(const production_pacs&) = delete;
    production_pacs& operator=(const production_pacs&) = delete;
    production_pacs(production_pacs&&) = delete;
    production_pacs& operator=(production_pacs&&) = delete;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Start the production PACS server
     * @return true if started successfully
     */
    [[nodiscard]] bool start();

    /**
     * @brief Stop the production PACS server
     */
    void stop();

    /**
     * @brief Block until server shutdown
     */
    void wait();

    /**
     * @brief Check if server is running
     * @return true if running
     */
    [[nodiscard]] bool is_running() const noexcept;

    // =========================================================================
    // Event Handlers
    // =========================================================================

    /**
     * @brief Register handler for image received events
     * @param handler Callback function
     */
    void on_image_received(std::function<void(const events::image_received_event&)> handler);

    /**
     * @brief Register handler for query executed events
     * @param handler Callback function
     */
    void on_query_executed(std::function<void(const events::query_executed_event&)> handler);

    /**
     * @brief Register handler for association events
     * @param handler Callback function
     */
    void on_association_event(std::function<void(const events::association_event&)> handler);

    /**
     * @brief Register handler for access denied events
     * @param handler Callback function
     */
    void on_access_denied(std::function<void(const events::access_denied_event&)> handler);

    // =========================================================================
    // Status and Statistics
    // =========================================================================

    /**
     * @brief Print server status to stdout
     */
    void print_status() const;

    /**
     * @brief Get current statistics
     * @return Reference to statistics structure
     */
    [[nodiscard]] const production_statistics& statistics() const noexcept;

    /**
     * @brief Get health status
     * @return Current health status
     */
    [[nodiscard]] monitoring::health_status get_health() const;

    /**
     * @brief Export statistics to JSON file
     * @param path Output file path
     */
    void export_statistics(const std::filesystem::path& path) const;

    // =========================================================================
    // Configuration Access
    // =========================================================================

    /**
     * @brief Get the configuration
     * @return Reference to configuration
     */
    [[nodiscard]] const production_config& config() const noexcept;

private:
    // =========================================================================
    // Initialization
    // =========================================================================

    void setup_mini_pacs();
    void setup_security();
    void setup_anonymization();
    void setup_rest_api();
    void setup_monitoring();
    void setup_event_handlers();

    // =========================================================================
    // Event Dispatch
    // =========================================================================

    void dispatch_image_received(const events::image_received_event& event);
    void dispatch_query_executed(const events::query_executed_event& event);
    void dispatch_association_event(const events::association_event& event);
    void dispatch_access_denied(const events::access_denied_event& event);

    // =========================================================================
    // Member Variables
    // =========================================================================

    production_config config_;
    production_statistics stats_;

    // Core PACS (from Level 4)
    std::unique_ptr<mini_pacs> pacs_;

    // Security
    std::shared_ptr<security::access_control_manager> access_control_;
    std::unique_ptr<security::anonymizer> anonymizer_;

    // REST API
    std::unique_ptr<web::rest_server> rest_server_;

    // Monitoring
    std::shared_ptr<monitoring::health_checker> health_checker_;
    std::shared_ptr<monitoring::pacs_metrics> metrics_;

    // Event handlers
    std::vector<std::function<void(const events::image_received_event&)>> image_handlers_;
    std::vector<std::function<void(const events::query_executed_event&)>> query_handlers_;
    std::vector<std::function<void(const events::association_event&)>> association_handlers_;
    std::vector<std::function<void(const events::access_denied_event&)>> access_denied_handlers_;

    // Synchronization
    mutable std::mutex handlers_mutex_;
    std::mutex shutdown_mutex_;
    std::condition_variable shutdown_cv_;
    std::atomic<bool> running_{false};
};

}  // namespace pacs::samples
