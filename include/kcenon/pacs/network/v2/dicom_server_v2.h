// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file dicom_server_v2.h
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
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_NETWORK_V2_DICOM_SERVER_V2_HPP
#define PACS_NETWORK_V2_DICOM_SERVER_V2_HPP

#include "kcenon/pacs/network/v2/dicom_association_handler.h"
#include "kcenon/pacs/network/server_config.h"
#include "kcenon/pacs/security/access_control_manager.h"
#include "kcenon/pacs/services/scp_service.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

// Forward declarations for public network_system interfaces
namespace kcenon::network::interfaces {
class i_protocol_server;
class i_session;
}  // namespace kcenon::network::interfaces

namespace kcenon::pacs::network::v2 {

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
 * @par Example: Usage
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
     * @brief Stop the server
     *
     * Closes the active associations and their connections, then stops
     * accepting connections. network_system offers no way to stop accepting
     * while sessions continue, so active associations are not waited for.
     *
     * @param timeout Unused; kept for source compatibility
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

    // =========================================================================
    // Security / Access Control
    // =========================================================================

    /**
     * @brief Set the access control manager for RBAC
     *
     * When set, the server will validate permissions before allowing
     * DICOM operations. If not set, all operations are allowed.
     *
     * @param acm Shared pointer to access control manager
     * @note Should be set before calling start()
     */
    void set_access_control(std::shared_ptr<security::access_control_manager> acm);

    /**
     * @brief Get the access control manager
     * @return Shared pointer to access control manager, or nullptr if not set
     */
    [[nodiscard]] std::shared_ptr<security::access_control_manager>
    get_access_control() const noexcept;

    /**
     * @brief Enable or disable access control enforcement
     * @param enabled If true, access control is enforced; if false, all operations allowed
     */
    void set_access_control_enabled(bool enabled);

    /**
     * @brief Check if access control is enabled
     * @return true if access control is enforced
     */
    [[nodiscard]] bool is_access_control_enabled() const noexcept;

private:
    // =========================================================================
    // Network System Callbacks
    // =========================================================================

    /// Handle new connection from server
    void on_connection(std::shared_ptr<kcenon::network::interfaces::i_session> session);

    /// Handle disconnection notification
    void on_disconnection(std::string_view session_id);

    /// Handle receive data (forwarded to handler)
    void on_receive(std::string_view session_id,
                    const std::vector<uint8_t>& data);

    /// Handle network error
    void on_network_error(std::string_view session_id,
                          std::error_code ec);

    // =========================================================================
    // Handler Management
    // =========================================================================

    /// Create and register a new handler for a session
    void create_handler(std::shared_ptr<kcenon::network::interfaces::i_session> session);

    /// Deliver @p session's data and errors straight to @p handler
    /// (registered as @p key). network_system reports the same id() for
    /// every session of a server, so the server-level callbacks cannot tell
    /// sessions apart. Returns false when the session type has no
    /// per-session callbacks.
    bool route_session_events(
        const std::shared_ptr<kcenon::network::interfaces::i_session>& session,
        const std::string& key,
        const std::weak_ptr<dicom_association_handler>& handler);

    /// Close @p session on the closer thread (see
    /// dicom_association_handler::set_session_closer()) once the peer had
    /// association_timeout (ARTIM) to close it first, as PS3.8 Sta13 has the
    /// acceptor wait after A-RELEASE-RP, A-ABORT, or A-ASSOCIATE-RJ. Closing
    /// at once would also cancel the write of that last PDU.
    void close_session_later(
        const std::shared_ptr<kcenon::network::interfaces::i_session>& session);

    /// Body of the closer thread
    void run_session_closer();

    /// Stop the closer thread after it closed what is queued
    void stop_session_closer();

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

    /// network_system's protocol server (via tcp_facade)
    std::shared_ptr<kcenon::network::interfaces::i_protocol_server> server_;

    /// Registered SCP services
    std::vector<services::scp_service_ptr> services_;

    /// Map from SOP Class UID to service (non-owning pointers)
    std::map<std::string, services::scp_service*> sop_class_to_service_;

    /// Active association handlers (keyed by a per-connection session ID)
    std::unordered_map<std::string, std::shared_ptr<dicom_association_handler>> handlers_;

    /// Source of per-connection session IDs
    std::atomic<uint64_t> next_session_number_{0};

    /// Thread that closes sessions outside network_system's callbacks
    std::thread closer_thread_;

    /// Sessions waiting for the closer thread, with the time to close them
    std::vector<std::pair<clock::time_point,
                          std::shared_ptr<kcenon::network::interfaces::i_session>>>
        sessions_to_close_;

    /// Whether the closer thread accepts work (guarded by closer_mutex_)
    bool closer_running_{false};

    /// Guards sessions_to_close_ and closer_running_
    std::mutex closer_mutex_;

    /// Wakes the closer thread
    std::condition_variable closer_cv_;

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

    /// Access control manager for RBAC
    std::shared_ptr<security::access_control_manager> access_control_;

    /// Whether access control is enabled
    std::atomic<bool> access_control_enabled_{false};

    /// Access control mutex
    mutable std::mutex acl_mutex_;
};

}  // namespace kcenon::pacs::network::v2

#endif  // PACS_NETWORK_V2_DICOM_SERVER_V2_HPP
