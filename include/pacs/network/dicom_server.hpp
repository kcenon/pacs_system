/**
 * @file dicom_server.hpp
 * @brief Multi-threaded DICOM server for handling multiple associations
 *
 * This file provides the dicom_server class for managing DICOM network
 * associations, including connection acceptance, service dispatching,
 * and association lifecycle management.
 *
 * @see DICOM PS3.8 - Network Communication Support for Message Exchange
 * @see DES-NET-005 - DICOM Server Class Design Specification
 */

#ifndef PACS_NETWORK_DICOM_SERVER_HPP
#define PACS_NETWORK_DICOM_SERVER_HPP

#include "pacs/network/association.hpp"
#include "pacs/network/server_config.hpp"
#include "pacs/network/detail/accept_worker.hpp"
#include "pacs/services/scp_service.hpp"

#include <kcenon/thread/core/cancellation_token.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace pacs::network {

// =============================================================================
// Forward Declarations
// =============================================================================

class association_pool;

// =============================================================================
// DICOM Server Class
// =============================================================================

/**
 * @brief Multi-threaded DICOM server for handling multiple associations
 *
 * Manages the DICOM network server lifecycle including:
 * - TCP connection acceptance
 * - Association negotiation
 * - Service registration and dispatching
 * - Worker thread pool management
 * - Association pooling
 *
 * The server integrates with the ecosystem's thread_system for efficient
 * task scheduling and network_system for TCP operations.
 *
 * @example Usage
 * @code
 * server_config config;
 * config.ae_title = "MY_PACS";
 * config.port = 11112;
 * config.max_associations = 20;
 *
 * dicom_server server{config};
 *
 * // Register services
 * server.register_service(std::make_unique<verification_scp>());
 * server.register_service(std::make_unique<storage_scp>(storage_path));
 *
 * // Set callbacks
 * server.on_association_established([](const association& assoc) {
 *     std::cout << "New association from: " << assoc.calling_ae() << '\n';
 * });
 *
 * // Start server
 * auto result = server.start();
 * if (result.is_err()) {
 *     std::cerr << "Failed to start: " << result.error() << '\n';
 *     return 1;
 * }
 *
 * // Server runs until stop() is called
 * server.wait_for_shutdown();
 * @endcode
 */
class dicom_server {
public:
    // =========================================================================
    // Type Aliases
    // =========================================================================

    using clock = std::chrono::steady_clock;
    using duration = std::chrono::milliseconds;
    using time_point = clock::time_point;

    /// Callback type for association events
    using association_callback = std::function<void(const association&)>;

    /// Callback type for error events
    using error_callback = std::function<void(const std::string&)>;

    // =========================================================================
    // Construction / Destruction
    // =========================================================================

    /**
     * @brief Construct server with configuration
     * @param config Server configuration
     */
    explicit dicom_server(const server_config& config);

    /**
     * @brief Destructor (stops server if running)
     */
    ~dicom_server();

    // Non-copyable, non-movable (owns network resources)
    dicom_server(const dicom_server&) = delete;
    dicom_server& operator=(const dicom_server&) = delete;
    dicom_server(dicom_server&&) = delete;
    dicom_server& operator=(dicom_server&&) = delete;

    // =========================================================================
    // Service Registration
    // =========================================================================

    /**
     * @brief Register an SCP service
     *
     * The server takes ownership of the service and routes DIMSE messages
     * to it based on the SOP Classes it supports.
     *
     * @param service The SCP service to register
     * @note Services must be registered before calling start()
     */
    void register_service(services::scp_service_ptr service);

    /**
     * @brief Get list of supported SOP Class UIDs
     * @return Vector of all SOP Classes supported by registered services
     */
    [[nodiscard]] std::vector<std::string> supported_sop_classes() const;

    // =========================================================================
    // Lifecycle Management
    // =========================================================================

    /**
     * @brief Start the server
     *
     * Binds to the configured port and begins accepting connections.
     *
     * @return Success or error with description
     */
    [[nodiscard]] Result<std::monostate> start();

    /**
     * @brief Stop the server gracefully
     *
     * Stops accepting new connections and waits for active associations
     * to complete or timeout.
     *
     * @param timeout Maximum time to wait for associations to close
     */
    void stop(duration timeout = std::chrono::seconds{30});

    /**
     * @brief Wait for server shutdown
     *
     * Blocks until the server is stopped (either by calling stop() or
     * due to an error).
     */
    void wait_for_shutdown();

    // =========================================================================
    // Status Queries
    // =========================================================================

    /**
     * @brief Check if server is running
     * @return true if server is accepting connections
     */
    [[nodiscard]] bool is_running() const noexcept;

    /**
     * @brief Get number of active associations
     * @return Current number of active DICOM associations
     */
    [[nodiscard]] size_t active_associations() const noexcept;

    /**
     * @brief Get server statistics
     * @return Current server statistics
     */
    [[nodiscard]] server_statistics get_statistics() const;

    /**
     * @brief Get server configuration
     * @return Reference to current configuration
     */
    [[nodiscard]] const server_config& config() const noexcept;

    // =========================================================================
    // Callbacks
    // =========================================================================

    /**
     * @brief Set callback for association established events
     * @param callback Function to call when an association is established
     */
    void on_association_established(association_callback callback);

    /**
     * @brief Set callback for association released events
     * @param callback Function to call when an association is released
     */
    void on_association_released(association_callback callback);

    /**
     * @brief Set callback for error events
     * @param callback Function to call on server errors
     */
    void on_error(error_callback callback);

    /**
     * @brief Get server instance listening on port (for in-memory testing).
     */
    static dicom_server* get_server_on_port(uint16_t port);

    /**
     * @brief Simulate an incoming association request (for in-memory testing).
     */
    Result<associate_ac> simulate_association_request(const associate_rq& rq, association* client_peer);

private:
    // =========================================================================
    // Private Implementation
    // =========================================================================

    /// Internal association info for tracking
    struct association_info {
        uint64_t id;
        association assoc;
        time_point connected_at;
        time_point last_activity;
        std::string remote_address;
        /// Flag indicating if message loop is currently processing
        /// (replaces std::thread worker_thread - now using thread_adapter pool)
        /// Note: Protected by associations_mutex_ since std::atomic is not movable
        bool processing{false};
        /// Cancellation token for cooperative shutdown
        /// Allows graceful cancellation of message processing when stop() is called
        kcenon::thread::cancellation_token cancel_token;
    };

    // =========================================================================
    // Private Methods
    // =========================================================================

    /// Handle a single association (runs in worker thread)
    void handle_association(uint64_t session_id, association assoc);

    /// Process incoming DIMSE messages
    void message_loop(association_info& info);

    /// Dispatch message to appropriate service
    Result<std::monostate> dispatch_to_service(
        association& assoc,
        uint8_t context_id,
        const dimse::dimse_message& msg);

    /// Validate calling AE title
    [[nodiscard]] bool validate_calling_ae(const std::string& calling_ae) const;

    /// Validate called AE title
    [[nodiscard]] bool validate_called_ae(const std::string& called_ae) const;

    /// Get service for SOP Class
    [[nodiscard]] services::scp_service* find_service(
        const std::string& sop_class_uid) const;

    /// Add association to pool
    void add_association(association_info info);

    /// Remove association from pool
    void remove_association(uint64_t id);

    /// Update association activity timestamp
    void touch_association(uint64_t id);

    /// Check for idle timeout
    void check_idle_timeouts();

    /// Generate unique association ID
    [[nodiscard]] uint64_t next_association_id();

    /// Report error through callback
    void report_error(const std::string& error);

    // =========================================================================
    // Member Variables
    // =========================================================================

    /// Server configuration
    server_config config_;

    /// Registered SCP services
    std::vector<services::scp_service_ptr> services_;

    /// Map from SOP Class UID to service
    std::unordered_map<std::string, services::scp_service*> sop_class_to_service_;

    /// Active associations
    std::unordered_map<uint64_t, std::unique_ptr<association_info>> associations_;

    /// Server statistics
    mutable server_statistics stats_;

    /// Association ID counter
    std::atomic<uint64_t> association_id_counter_{0};

    /// Running flag
    std::atomic<bool> running_{false};

    /// Accept worker (replaces std::thread accept_thread_)
    std::unique_ptr<detail::accept_worker> accept_worker_;

    /// Association mutex
    mutable std::mutex associations_mutex_;

    /// Statistics mutex
    mutable std::mutex stats_mutex_;

    /// Service registry mutex
    mutable std::mutex services_mutex_;

    /// Shutdown condition variable
    std::condition_variable shutdown_cv_;

    /// Shutdown mutex
    std::mutex shutdown_mutex_;

    /// Association established callback
    association_callback on_established_cb_;

    /// Association released callback
    association_callback on_released_cb_;

    /// Error callback
    error_callback on_error_cb_;

    /// Callback mutex
    std::mutex callback_mutex_;
};

}  // namespace pacs::network

#endif  // PACS_NETWORK_DICOM_SERVER_HPP
