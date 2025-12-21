/**
 * @file dicom_association_handler.hpp
 * @brief DICOM association handler for network_system integration
 *
 * This file provides the dicom_association_handler class that bridges
 * network_system's session model with DICOM protocol requirements.
 * It handles PDU framing, parsing, and manages the DICOM state machine.
 *
 * @see Issue #161 - Design dicom_association_handler layer for network_system integration
 * @see DICOM PS3.8 - Network Communication Support for Message Exchange
 */

#ifndef PACS_NETWORK_V2_DICOM_ASSOCIATION_HANDLER_HPP
#define PACS_NETWORK_V2_DICOM_ASSOCIATION_HANDLER_HPP

#include "pacs/network/association.hpp"
#include "pacs/network/pdu_types.hpp"
#include "pacs/network/server_config.hpp"
#include "pacs/security/access_control_manager.hpp"
#include "pacs/services/scp_service.hpp"
#include "pacs/integration/dicom_session.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

// Forward declarations for network_system types (no ASIO dependency)
#include <network_system/forward.h>

namespace pacs::network::v2 {

// =============================================================================
// Handler State
// =============================================================================

/**
 * @brief State machine states for the association handler.
 *
 * Tracks the progression of a DICOM association from initial connection
 * through negotiation to established communication and eventual release.
 */
enum class handler_state {
    idle,              ///< Initial state, waiting for A-ASSOCIATE-RQ
    awaiting_response, ///< Sent response, awaiting next PDU
    established,       ///< Association established, processing DIMSE
    releasing,         ///< Graceful release in progress
    closed             ///< Association closed (released or aborted)
};

/**
 * @brief Convert handler_state to string representation.
 * @param state The state to convert
 * @return String representation of the state
 */
[[nodiscard]] constexpr const char* to_string(handler_state state) noexcept {
    switch (state) {
        case handler_state::idle: return "Idle";
        case handler_state::awaiting_response: return "Awaiting Response";
        case handler_state::established: return "Established";
        case handler_state::releasing: return "Releasing";
        case handler_state::closed: return "Closed";
        default: return "Unknown";
    }
}

// =============================================================================
// Handler Callbacks
// =============================================================================

/**
 * @brief Callback type for association established events.
 */
using association_established_callback =
    std::function<void(const std::string& session_id,
                       const std::string& calling_ae,
                       const std::string& called_ae)>;

/**
 * @brief Callback type for association closed events.
 */
using association_closed_callback =
    std::function<void(const std::string& session_id, bool graceful)>;

/**
 * @brief Callback type for error events.
 */
using handler_error_callback =
    std::function<void(const std::string& session_id, const std::string& error)>;

// =============================================================================
// DICOM Association Handler Class
// =============================================================================

/**
 * @class dicom_association_handler
 * @brief Bridges network_system sessions with DICOM protocol handling.
 *
 * This class wraps a network_system `messaging_session` to provide
 * DICOM-specific behavior including:
 *
 * - **PDU Framing**: Handles the 6-byte PDU header parsing and accumulation
 *   of fragmented PDUs from the TCP stream.
 * - **State Machine**: Manages DICOM association states (idle, awaiting,
 *   established, releasing, closed).
 * - **Service Dispatching**: Routes DIMSE messages to registered SCP services.
 * - **Association Negotiation**: Handles A-ASSOCIATE-RQ/AC/RJ PDU processing.
 *
 * ### Thread Safety
 * All public methods are thread-safe. The handler can be accessed from
 * multiple threads (e.g., network I/O thread and service threads).
 *
 * ### Lifecycle
 * 1. Construct with a session and server configuration
 * 2. Call `start()` to begin processing incoming PDUs
 * 3. Handle association negotiation automatically
 * 4. DIMSE messages are dispatched to registered services
 * 5. Call `stop()` or let graceful release complete
 *
 * @example
 * @code
 * auto handler = std::make_shared<dicom_association_handler>(
 *     session, config, services);
 *
 * handler->set_established_callback([](const auto& id, const auto& calling, const auto& called) {
 *     std::cout << "Association established: " << calling << " -> " << called << '\n';
 * });
 *
 * handler->start();
 * @endcode
 */
class dicom_association_handler
    : public std::enable_shared_from_this<dicom_association_handler> {
public:
    // =========================================================================
    // Type Aliases
    // =========================================================================

    using session_ptr = std::shared_ptr<network_system::session::messaging_session>;
    using service_map = std::map<std::string, services::scp_service*>;
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;
    using duration = std::chrono::milliseconds;

    /// PDU header size (type + reserved + length)
    static constexpr size_t pdu_header_size = 6;

    /// Maximum PDU size for safety checks
    static constexpr size_t max_pdu_size = 64 * 1024 * 1024;  // 64 MB

    // =========================================================================
    // Construction / Destruction
    // =========================================================================

    /**
     * @brief Construct a handler for a network session.
     *
     * @param session The network_system session to wrap
     * @param config Server configuration for association negotiation
     * @param services Map from SOP Class UID to service implementation
     */
    dicom_association_handler(
        session_ptr session,
        const server_config& config,
        const service_map& services);

    /**
     * @brief Destructor (stops handler if still running).
     */
    ~dicom_association_handler();

    // Non-copyable, non-movable
    dicom_association_handler(const dicom_association_handler&) = delete;
    dicom_association_handler& operator=(const dicom_association_handler&) = delete;
    dicom_association_handler(dicom_association_handler&&) = delete;
    dicom_association_handler& operator=(dicom_association_handler&&) = delete;

    // =========================================================================
    // Lifecycle Management
    // =========================================================================

    /**
     * @brief Start processing the session.
     *
     * Sets up receive callbacks and begins handling incoming PDUs.
     * The handler will automatically process association negotiation.
     */
    void start();

    /**
     * @brief Stop the handler and close the session.
     *
     * Sends an A-ABORT if the association is established and forces
     * immediate closure of the underlying network session.
     *
     * @param graceful If true, attempt graceful release before aborting
     */
    void stop(bool graceful = false);

    // =========================================================================
    // State Queries
    // =========================================================================

    /**
     * @brief Get current handler state.
     * @return Current state of the handler
     */
    [[nodiscard]] handler_state state() const noexcept;

    /**
     * @brief Check if the association is established.
     * @return true if association is in established state
     */
    [[nodiscard]] bool is_established() const noexcept;

    /**
     * @brief Check if the handler is closed.
     * @return true if handler is in closed state
     */
    [[nodiscard]] bool is_closed() const noexcept;

    /**
     * @brief Get the session identifier.
     * @return Unique session ID string
     */
    [[nodiscard]] std::string session_id() const;

    /**
     * @brief Get the calling AE title.
     * @return Calling AE title if negotiated, empty otherwise
     */
    [[nodiscard]] std::string calling_ae() const;

    /**
     * @brief Get the called AE title.
     * @return Called AE title if negotiated, empty otherwise
     */
    [[nodiscard]] std::string called_ae() const;

    /**
     * @brief Get the underlying association object.
     * @return Reference to the association
     * @throws std::runtime_error if association not established
     */
    [[nodiscard]] association& get_association();
    [[nodiscard]] const association& get_association() const;

    /**
     * @brief Get time of last activity.
     * @return Time point of last PDU received/sent
     */
    [[nodiscard]] time_point last_activity() const noexcept;

    // =========================================================================
    // Callbacks
    // =========================================================================

    /**
     * @brief Set callback for association established event.
     * @param callback Function called when association is established
     */
    void set_established_callback(association_established_callback callback);

    /**
     * @brief Set callback for association closed event.
     * @param callback Function called when association is closed
     */
    void set_closed_callback(association_closed_callback callback);

    /**
     * @brief Set callback for error events.
     * @param callback Function called on errors
     */
    void set_error_callback(handler_error_callback callback);

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get number of PDUs received.
     * @return Count of PDUs received
     */
    [[nodiscard]] uint64_t pdus_received() const noexcept;

    /**
     * @brief Get number of PDUs sent.
     * @return Count of PDUs sent
     */
    [[nodiscard]] uint64_t pdus_sent() const noexcept;

    /**
     * @brief Get number of DIMSE messages processed.
     * @return Count of DIMSE messages handled
     */
    [[nodiscard]] uint64_t messages_processed() const noexcept;

    // =========================================================================
    // Security / Access Control
    // =========================================================================

    /**
     * @brief Set the access control manager for RBAC
     * @param acm Shared pointer to access control manager
     */
    void set_access_control(std::shared_ptr<security::access_control_manager> acm);

    /**
     * @brief Enable or disable access control enforcement
     * @param enabled If true, access control is enforced
     */
    void set_access_control_enabled(bool enabled);

private:
    // =========================================================================
    // Network Callbacks
    // =========================================================================

    /// Handle data received from session
    void on_data_received(const std::vector<uint8_t>& data);

    /// Handle session disconnection
    void on_disconnected(const std::string& session_id);

    /// Handle session error
    void on_error(std::error_code ec);

    // =========================================================================
    // PDU Processing
    // =========================================================================

    /// Process accumulated buffer for complete PDUs
    void process_buffer();

    /// Process a complete PDU
    void process_pdu(const integration::pdu_data& pdu);

    /// Handle A-ASSOCIATE-RQ PDU
    void handle_associate_rq(const std::vector<uint8_t>& payload);

    /// Handle P-DATA-TF PDU
    void handle_p_data_tf(const std::vector<uint8_t>& payload);

    /// Handle A-RELEASE-RQ PDU
    void handle_release_rq();

    /// Handle A-ABORT PDU
    void handle_abort(const std::vector<uint8_t>& payload);

    // =========================================================================
    // Response Sending
    // =========================================================================

    /// Send A-ASSOCIATE-AC response
    void send_associate_ac();

    /// Send A-ASSOCIATE-RJ response
    void send_associate_rj(reject_result result, uint8_t source, uint8_t reason);

    /// Send P-DATA-TF PDU
    void send_p_data_tf(const std::vector<uint8_t>& payload);

    /// Send A-RELEASE-RP response
    void send_release_rp();

    /// Send A-ABORT PDU
    void send_abort(abort_source source, abort_reason reason);

    /// Send raw PDU data
    void send_pdu(pdu_type type, const std::vector<uint8_t>& payload);

    // =========================================================================
    // Service Dispatching
    // =========================================================================

    /// Dispatch DIMSE message to appropriate service
    Result<std::monostate> dispatch_to_service(
        uint8_t context_id,
        const dimse::dimse_message& msg);

    /// Find service for SOP Class UID
    [[nodiscard]] services::scp_service* find_service(const std::string& sop_class_uid) const;

    // =========================================================================
    // State Management
    // =========================================================================

    /// Transition to new state
    void transition_to(handler_state new_state);

    /// Update last activity timestamp
    void touch();

    /// Report error through callback
    void report_error(const std::string& error);

    /// Close and cleanup
    void close_handler(bool graceful);

    // =========================================================================
    // Member Variables
    // =========================================================================

    /// Network session
    session_ptr session_;

    /// Server configuration
    server_config config_;

    /// Service registry (non-owning pointers)
    service_map services_;

    /// DICOM association
    association association_;

    /// Current handler state
    std::atomic<handler_state> state_{handler_state::idle};

    /// PDU receive buffer
    std::vector<uint8_t> receive_buffer_;

    /// Expected PDU length (0 if waiting for header)
    [[maybe_unused]] uint32_t expected_pdu_length_{0};

    /// Current PDU type being received
    [[maybe_unused]] pdu_type current_pdu_type_{pdu_type::abort};

    /// Last activity timestamp
    time_point last_activity_;

    /// Statistics
    std::atomic<uint64_t> pdus_received_{0};
    std::atomic<uint64_t> pdus_sent_{0};
    std::atomic<uint64_t> messages_processed_{0};

    /// Callbacks
    association_established_callback established_callback_;
    association_closed_callback closed_callback_;
    handler_error_callback error_callback_;

    /// Thread safety
    mutable std::mutex mutex_;
    mutable std::mutex callback_mutex_;

    /// Access control manager for RBAC
    std::shared_ptr<security::access_control_manager> access_control_;

    /// User context for this association (set after A-ASSOCIATE negotiation)
    std::optional<security::user_context> user_context_;

    /// Whether access control is enabled
    bool access_control_enabled_{false};
};

}  // namespace pacs::network::v2

#endif  // PACS_NETWORK_V2_DICOM_ASSOCIATION_HANDLER_HPP
