/**
 * @file dicom_server_v2.hpp
 * @brief DICOM server implementation using network_system's messaging_server
 *
 * This file provides dicom_server_v2 class that uses network_system's
 * messaging_server for connection management, replacing direct socket handling.
 * It maintains API compatibility with the existing dicom_server while leveraging
 * the benefits of network_system integration.
 *
 * @see Issue #162 - Implement dicom_server_v2 using network_system messaging_server
 * @see Issue #161 - Design dicom_association_handler layer for network_system integration
 * @see DICOM PS3.8 - Network Communication Support for Message Exchange
 */

#ifndef PACS_NETWORK_V2_DICOM_SERVER_V2_HPP
#define PACS_NETWORK_V2_DICOM_SERVER_V2_HPP

#include "pacs/network/v2/dicom_association_handler.hpp"
#include "pacs/network/server_config.hpp"
#include "pacs/services/scp_service.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

// Forward declarations for network_system types
namespace network_system::core {
class messaging_server;
}  // namespace network_system::core

namespace network_system::session {
class messaging_session;
}  // namespace network_system::session

namespace pacs::network::v2 {

// =============================================================================
// DICOM Server V2 Class
// =============================================================================

/**
 * @class dicom_server_v2
 * @brief DICOM server using network_system's messaging_server for connection management
 *
 * This class provides the same functionality as dicom_server but uses network_system's
 * messaging_server for TCP connection management. Key benefits include:
 *
 * - **No manual thread management**: Accept loop and I/O handled by ASIO internally
 * - **Built-in session tracking**: Automatic cleanup on disconnect
 * - **TLS support**: Ready for secure DICOM (future enhancement)
 * - **Proven scalability**: ASIO's async model for efficient connection handling
 *
 * ### Architecture
 *
 * ```
 * dicom_server_v2
 *   └── messaging_server (network_system)
 *         └── messaging_session (per connection)
 *               └── dicom_association_handler (DICOM protocol)
 *                     └── scp_service (DIMSE handling)
 * ```
 *
 * ### Thread Safety
 * All public methods are thread-safe. The server uses internal mutexes to protect
 * shared state and delegates I/O operations to network_system's thread model.
 *
 * ### Migration Path
 * This class is API-compatible with dicom_server. To migrate:
 * 1. Replace `dicom_server` with `dicom_server_v2`
 * 2. No changes needed to service registration or callbacks
 * 3. Feature flag: `PACS_USE_NETWORK_SYSTEM_SERVER` (when available)
 *
 * @example Usage
 * @code
 * server_config config;
 * config.ae_title = "MY_PACS";
 * config.port = 11112;
 * config.max_associations = 20;
 *
 * dicom_server_v2 server{config};
 *
 * // Register services (same API as dicom_server)
 * server.register_service(std::make_unique<verification_scp>());
 * server.register_service(std::make_unique<storage_scp>(storage_path));
 *
 * // Set callbacks (same API as dicom_server)
 * server.on_association_established([](const std::string& session_id,
 *                                       const std::string& calling_ae,
 *                                       const std::string& called_ae) {
 *     std::cout << "Association: " << calling_ae << " -> " << called_ae << '\n';
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
class dicom_server_v2 {
public:
    // =========================================================================
    // Type Aliases
    // =========================================================================

    using clock = std::chrono::steady_clock;
    using duration = std::chrono::milliseconds;
    using time_point = clock::time_point;

    /// Callback type for association established events
    using association_established_callback =
        std::function<void(const std::string& session_id,
                           const std::string& calling_ae,
                           const std::string& called_ae)>;

    /// Callback type for association closed events
    using association_closed_callback =
        std::function<void(const std::string& session_id, bool graceful)>;

    /// Callback type for error events
    using error_callback = std::function<void(const std::string& error)>;

    // =========================================================================
    // Construction / Destruction
    // =========================================================================

    /**
     * @brief Construct server with configuration
     * @param config Server configuration
     */
    explicit dicom_server_v2(const server_config& config);

    /**
     * @brief Destructor (stops server if running)
     */
    ~dicom_server_v2();

    // Non-copyable, non-movable (owns network resources)
    dicom_server_v2(const dicom_server_v2&) = delete;
    dicom_server_v2& operator=(const dicom_server_v2&) = delete;
    dicom_server_v2(dicom_server_v2&&) = delete;
    dicom_server_v2& operator=(dicom_server_v2&&) = delete;

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
     * Binds to the configured port and begins accepting connections
     * using network_system's messaging_server.
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
    void on_association_established(association_established_callback callback);

    /**
     * @brief Set callback for association closed events
     * @param callback Function to call when an association is closed
     */
    void on_association_closed(association_closed_callback callback);

    /**
     * @brief Set callback for error events
     * @param callback Function to call on server errors
     */
    void on_error(error_callback callback);

private:
    // =========================================================================
    // Network System Callbacks
    // =========================================================================

    /// Handle new connection from messaging_server
    void on_connection(std::shared_ptr<network_system::session::messaging_session> session);

    /// Handle disconnection notification
    void on_disconnection(const std::string& session_id);

    /// Handle receive data (delegated to handler)
    void on_receive(std::shared_ptr<network_system::session::messaging_session> session,
                    const std::vector<uint8_t>& data);

    /// Handle network error
    void on_network_error(std::shared_ptr<network_system::session::messaging_session> session,
                          std::error_code ec);

    // =========================================================================
    // Handler Management
    // =========================================================================

    /// Create and register a new handler for a session
    void create_handler(std::shared_ptr<network_system::session::messaging_session> session);

    /// Remove handler by session ID
    void remove_handler(const std::string& session_id);

    /// Find handler by session ID
    [[nodiscard]] std::shared_ptr<dicom_association_handler>
    find_handler(const std::string& session_id) const;

    /// Check for idle timeouts on handlers
    void check_idle_timeouts();

    // =========================================================================
    // Internal Helpers
    // =========================================================================

    /// Build service map from registered services
    [[nodiscard]] dicom_association_handler::service_map build_service_map() const;

    /// Report error through callback
    void report_error(const std::string& error);

    // =========================================================================
    // Member Variables
    // =========================================================================

    /// Server configuration
    server_config config_;

    /// network_system's messaging server
    std::shared_ptr<network_system::core::messaging_server> server_;

    /// Registered SCP services
    std::vector<services::scp_service_ptr> services_;

    /// Map from SOP Class UID to service (non-owning pointers)
    std::map<std::string, services::scp_service*> sop_class_to_service_;

    /// Active association handlers (keyed by session ID)
    std::unordered_map<std::string, std::shared_ptr<dicom_association_handler>> handlers_;

    /// Server statistics
    mutable server_statistics stats_;

    /// Running flag
    std::atomic<bool> running_{false};

    /// Handler mutex (protects handlers_ map)
    mutable std::mutex handlers_mutex_;

    /// Service mutex (protects services_ and sop_class_to_service_)
    mutable std::mutex services_mutex_;

    /// Statistics mutex
    mutable std::mutex stats_mutex_;

    /// Shutdown condition variable
    std::condition_variable shutdown_cv_;

    /// Shutdown mutex
    std::mutex shutdown_mutex_;

    /// Association established callback
    association_established_callback on_established_cb_;

    /// Association closed callback
    association_closed_callback on_closed_cb_;

    /// Error callback
    error_callback on_error_cb_;

    /// Callback mutex
    mutable std::mutex callback_mutex_;
};

}  // namespace pacs::network::v2

#endif  // PACS_NETWORK_V2_DICOM_SERVER_V2_HPP
