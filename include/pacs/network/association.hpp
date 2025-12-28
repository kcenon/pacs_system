/**
 * @file association.hpp
 * @brief DICOM Association management per PS3.8
 *
 * This file provides the association class for managing DICOM network
 * associations, including state machine, presentation context negotiation,
 * and DIMSE message exchange.
 *
 * @see DICOM PS3.8 - Network Communication Support for Message Exchange
 * @see DES-NET-004 - Association Class Design Specification
 */

#ifndef PACS_NETWORK_ASSOCIATION_HPP
#define PACS_NETWORK_ASSOCIATION_HPP

#include "pacs/network/pdu_types.hpp"
#include "pacs/network/dimse/dimse_message.hpp"
#include "pacs/encoding/transfer_syntax.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <kcenon/thread/lockfree/lockfree_queue.h>

#include "pacs/core/result.hpp"

namespace pacs::network {

// =============================================================================
// Forward Declarations
// =============================================================================

class association;

// =============================================================================
// Result Type
// =============================================================================

/// Result type alias using standardized pacs::Result<T>
template <typename T>
using Result = pacs::Result<T>;

/// VoidResult type alias for operations without return value
using VoidResult = pacs::VoidResult;

// =============================================================================
// Association State
// =============================================================================

/**
 * @brief DICOM Association state machine states per PS3.8.
 *
 * Simplified state model based on DICOM PS3.8 Table 9-1.
 */
enum class association_state {
    idle,              ///< Sta1: No TCP connection, waiting for transport
    awaiting_associate_ac, ///< Sta5: Awaiting A-ASSOCIATE response (SCU)
    awaiting_associate_rq, ///< Sta2: Awaiting A-ASSOCIATE request (SCP)
    established,       ///< Sta6: Association established, ready for DIMSE
    awaiting_release_rp,   ///< Sta7: Awaiting A-RELEASE response (initiator)
    awaiting_release_rq,   ///< Sta8: Awaiting potential A-RELEASE request
    released,          ///< Association gracefully released
    aborted            ///< Association aborted (error condition)
};

/**
 * @brief Convert association_state to string representation.
 * @param state The state to convert
 * @return String representation of the state
 */
[[nodiscard]] constexpr const char* to_string(association_state state) noexcept {
    switch (state) {
        case association_state::idle: return "Idle (Sta1)";
        case association_state::awaiting_associate_ac: return "Awaiting A-ASSOCIATE-AC (Sta5)";
        case association_state::awaiting_associate_rq: return "Awaiting A-ASSOCIATE-RQ (Sta2)";
        case association_state::established: return "Established (Sta6)";
        case association_state::awaiting_release_rp: return "Awaiting A-RELEASE-RP (Sta7)";
        case association_state::awaiting_release_rq: return "Awaiting A-RELEASE-RQ (Sta8)";
        case association_state::released: return "Released";
        case association_state::aborted: return "Aborted";
        default: return "Unknown";
    }
}

// =============================================================================
// Association Error
// =============================================================================

/**
 * @brief Error codes for association operations.
 */
enum class association_error {
    success = 0,
    connection_failed,
    connection_timeout,
    association_rejected,
    association_aborted,
    invalid_state,
    negotiation_failed,
    no_acceptable_context,
    pdu_encoding_error,
    pdu_decoding_error,
    send_failed,
    receive_failed,
    receive_timeout,
    protocol_error,
    release_failed,
    already_released
};

/**
 * @brief Convert association_error to string representation.
 */
[[nodiscard]] constexpr std::string_view to_string(association_error err) noexcept {
    switch (err) {
        case association_error::success: return "Success";
        case association_error::connection_failed: return "Connection failed";
        case association_error::connection_timeout: return "Connection timeout";
        case association_error::association_rejected: return "Association rejected";
        case association_error::association_aborted: return "Association aborted";
        case association_error::invalid_state: return "Invalid state for operation";
        case association_error::negotiation_failed: return "Negotiation failed";
        case association_error::no_acceptable_context: return "No acceptable presentation context";
        case association_error::pdu_encoding_error: return "PDU encoding error";
        case association_error::pdu_decoding_error: return "PDU decoding error";
        case association_error::send_failed: return "Send failed";
        case association_error::receive_failed: return "Receive failed";
        case association_error::receive_timeout: return "Receive timeout";
        case association_error::protocol_error: return "Protocol error";
        case association_error::release_failed: return "Release failed";
        case association_error::already_released: return "Already released";
        default: return "Unknown error";
    }
}

// =============================================================================
// Association Configuration
// =============================================================================

/**
 * @brief Proposed presentation context for SCU association request.
 */
struct proposed_presentation_context {
    uint8_t id;                           ///< Presentation Context ID (odd 1-255)
    std::string abstract_syntax;          ///< Abstract Syntax UID (SOP Class)
    std::vector<std::string> transfer_syntaxes; ///< Proposed Transfer Syntaxes

    proposed_presentation_context() : id(0) {}
    proposed_presentation_context(uint8_t ctx_id, std::string abs_syntax,
                                  std::vector<std::string> ts_list)
        : id(ctx_id)
        , abstract_syntax(std::move(abs_syntax))
        , transfer_syntaxes(std::move(ts_list)) {}
};

/**
 * @brief Accepted presentation context after negotiation.
 */
struct accepted_presentation_context {
    uint8_t id;                           ///< Presentation Context ID
    std::string abstract_syntax;          ///< Abstract Syntax UID
    std::string transfer_syntax;          ///< Accepted Transfer Syntax UID
    presentation_context_result result;   ///< Negotiation result

    accepted_presentation_context()
        : id(0), result(presentation_context_result::acceptance) {}
    accepted_presentation_context(uint8_t ctx_id, std::string abs_syntax,
                                  std::string ts, presentation_context_result res)
        : id(ctx_id)
        , abstract_syntax(std::move(abs_syntax))
        , transfer_syntax(std::move(ts))
        , result(res) {}

    [[nodiscard]] bool is_accepted() const noexcept {
        return result == presentation_context_result::acceptance;
    }
};

/**
 * @brief Configuration for SCU association request.
 */
struct association_config {
    std::string calling_ae_title;         ///< Our AE Title (16 chars max)
    std::string called_ae_title;          ///< Remote AE Title (16 chars max)
    std::vector<proposed_presentation_context> proposed_contexts;
    uint32_t max_pdu_length = DEFAULT_MAX_PDU_LENGTH;
    std::string implementation_class_uid;
    std::string implementation_version_name;

    association_config() = default;
};

/**
 * @brief Configuration for SCP to accept associations.
 */
struct scp_config {
    std::string ae_title;                 ///< Our AE Title
    std::vector<std::string> accepted_ae_titles; ///< Allowed calling AE titles (empty = all)
    std::vector<std::string> supported_abstract_syntaxes;
    std::vector<std::string> supported_transfer_syntaxes;
    uint32_t max_pdu_length = DEFAULT_MAX_PDU_LENGTH;
    std::string implementation_class_uid;
    std::string implementation_version_name;

    scp_config() = default;
};

// =============================================================================
// Association Rejection Info
// =============================================================================

/**
 * @brief Information about an association rejection.
 */
struct rejection_info {
    reject_result result;
    uint8_t source;
    uint8_t reason;
    std::string description;

    rejection_info()
        : result(reject_result::rejected_permanent)
        , source(0)
        , reason(0) {}

    rejection_info(reject_result res, uint8_t src, uint8_t rsn)
        : result(res), source(src), reason(rsn) {
        build_description();
    }

private:
    void build_description();
};

// =============================================================================
// Association Class
// =============================================================================

/**
 * @brief DICOM Association management class.
 *
 * Manages the DICOM association state machine, presentation context
 * negotiation, and DIMSE message exchange per PS3.8.
 *
 * @example SCU Usage
 * @code
 * association_config config;
 * config.calling_ae_title = "MY_SCU";
 * config.called_ae_title = "REMOTE_SCP";
 * config.proposed_contexts.push_back({
 *     1,
 *     "1.2.840.10008.1.1",  // Verification SOP Class
 *     {"1.2.840.10008.1.2.1"}  // Explicit VR LE
 * });
 *
 * auto result = association::connect("192.168.1.100", 104, config);
 * if (result.is_ok()) {
 *     auto& assoc = result.value();
 *     // Send DIMSE messages...
 *     assoc.release();
 * }
 * @endcode
 */
class association {
public:
    // =========================================================================
    // Type Aliases
    // =========================================================================

    using clock = std::chrono::steady_clock;
    using duration = std::chrono::milliseconds;
    using time_point = clock::time_point;

    /// Default timeout for operations
    static constexpr duration default_timeout{30000};  // 30 seconds

    // =========================================================================
    // Construction / Destruction
    // =========================================================================

    /**
     * @brief Default constructor (creates idle association).
     */
    association();

    /**
     * @brief Move constructor.
     */
    association(association&& other) noexcept;

    /**
     * @brief Move assignment operator.
     */
    association& operator=(association&& other) noexcept;

    /**
     * @brief Destructor (aborts if still established).
     */
    ~association();

    // Disable copy
    association(const association&) = delete;
    association& operator=(const association&) = delete;

    // =========================================================================
    // Factory Methods
    // =========================================================================

    /**
     * @brief Initiate an SCU association to a remote SCP.
     *
     * @param host Remote host address
     * @param port Remote port (typically 104 or 11112)
     * @param config Association configuration
     * @param timeout Connection timeout
     * @return Result containing established association or error
     */
    [[nodiscard]] static Result<association> connect(
        const std::string& host,
        uint16_t port,
        const association_config& config,
        duration timeout = default_timeout);

    /**
     * @brief Accept an incoming SCP association.
     *
     * @param rq The received A-ASSOCIATE-RQ PDU
     * @param config SCP configuration for negotiation
     * @return Configured association ready for DIMSE exchange
     */
    [[nodiscard]] static association accept(
        const associate_rq& rq,
        const scp_config& config);

    /**
     * @brief Reject an incoming association request.
     *
     * @param result Rejection result (permanent/transient)
     * @param source Rejection source
     * @param reason Rejection reason
     * @return Rejection PDU to send
     */
    [[nodiscard]] static associate_rj reject(
        reject_result result,
        uint8_t source,
        uint8_t reason);

    // =========================================================================
    // State Queries
    // =========================================================================

    /**
     * @brief Get current association state.
     */
    [[nodiscard]] association_state state() const noexcept;

    /**
     * @brief Check if association is established and ready for DIMSE.
     */
    [[nodiscard]] bool is_established() const noexcept;

    /**
     * @brief Check if association has been released or aborted.
     */
    [[nodiscard]] bool is_closed() const noexcept;

    // =========================================================================
    // Negotiated Parameters
    // =========================================================================

    /**
     * @brief Get calling AE title.
     */
    [[nodiscard]] std::string_view calling_ae() const noexcept;

    /**
     * @brief Get called AE title.
     */
    [[nodiscard]] std::string_view called_ae() const noexcept;

    /**
     * @brief Get negotiated maximum PDU size.
     */
    [[nodiscard]] uint32_t max_pdu_size() const noexcept;

    /**
     * @brief Get remote implementation class UID.
     */
    [[nodiscard]] std::string_view remote_implementation_class() const noexcept;

    /**
     * @brief Get remote implementation version name.
     */
    [[nodiscard]] std::string_view remote_implementation_version() const noexcept;

    // =========================================================================
    // Presentation Context Management
    // =========================================================================

    /**
     * @brief Check if a presentation context for the abstract syntax was accepted.
     */
    [[nodiscard]] bool has_accepted_context(std::string_view abstract_syntax) const;

    /**
     * @brief Get the presentation context ID for an abstract syntax.
     * @return Context ID if accepted, std::nullopt if not available
     */
    [[nodiscard]] std::optional<uint8_t> accepted_context_id(
        std::string_view abstract_syntax) const;

    /**
     * @brief Get the transfer syntax for an accepted context.
     * @param pc_id Presentation Context ID
     * @return Transfer syntax for the context
     * @throws std::out_of_range if context not found
     */
    [[nodiscard]] const encoding::transfer_syntax& context_transfer_syntax(
        uint8_t pc_id) const;

    /**
     * @brief Get all accepted presentation contexts.
     */
    [[nodiscard]] const std::vector<accepted_presentation_context>&
        accepted_contexts() const noexcept;

    // =========================================================================
    // DIMSE Operations
    // =========================================================================

    /**
     * @brief Send a DIMSE message.
     *
     * @param context_id Presentation Context ID to use
     * @param msg The DIMSE message to send
     * @return Success or error
     */
    [[nodiscard]] Result<std::monostate> send_dimse(
        uint8_t context_id,
        const dimse::dimse_message& msg);

    /**
     * @brief Receive a DIMSE message.
     *
     * @param timeout Receive timeout
     * @return Received message with context ID, or error
     */
    [[nodiscard]] Result<std::pair<uint8_t, dimse::dimse_message>> receive_dimse(
        duration timeout = default_timeout);

    // =========================================================================
    // PDU Access (for network layer integration)
    // =========================================================================

    /**
     * @brief Build A-ASSOCIATE-RQ PDU for sending.
     */
    [[nodiscard]] associate_rq build_associate_rq() const;

    /**
     * @brief Build A-ASSOCIATE-AC PDU for sending.
     */
    [[nodiscard]] associate_ac build_associate_ac() const;

    /**
     * @brief Process received A-ASSOCIATE-AC PDU.
     * @return true if association established successfully
     */
    bool process_associate_ac(const associate_ac& ac);

    /**
     * @brief Process received A-ASSOCIATE-RJ PDU.
     */
    void process_associate_rj(const associate_rj& rj);

    /**
     * @brief Get rejection info if association was rejected.
     */
    [[nodiscard]] std::optional<rejection_info> get_rejection_info() const;

    // =========================================================================
    // Lifecycle Management
    // =========================================================================

    /**
     * @brief Gracefully release the association.
     *
     * Sends A-RELEASE-RQ and waits for A-RELEASE-RP.
     *
     * @param timeout Timeout for release handshake
     * @return Success or error
     */
    [[nodiscard]] Result<std::monostate> release(duration timeout = default_timeout);

    /**
     * @brief Process received A-RELEASE-RQ.
     * @return A-RELEASE-RP PDU to send
     */
    void process_release_rq();

    /**
     * @brief Process received A-RELEASE-RP.
     */
    void process_release_rp();

    /**
     * @brief Abort the association immediately.
     *
     * @param source Abort source (0=service-user, 2=service-provider)
     * @param reason Abort reason
     */
    void abort(uint8_t source = 0, uint8_t reason = 0);

    /**
     * @brief Process received A-ABORT PDU.
     */
    void process_abort(const abort_source& source, const abort_reason& reason);

    // =========================================================================
    // Internal State Management (for testing/debugging)
    // =========================================================================

    /**
     * @brief Force state transition (for testing).
     * @warning Internal use only
     */
    void set_state(association_state new_state);

    /**
     * @brief Set peer association for in-memory testing.
     */
    void set_peer(association* peer);

    /**
     * @brief Enqueue message from peer (for in-memory testing).
     */
    void enqueue_message(uint8_t context_id, dimse::dimse_message msg);

    /**
     * @brief Update peer pointer (for in-memory testing).
     */
    void update_peer(association* old_peer, association* new_peer);

private:
    // =========================================================================
    // Private Implementation
    // =========================================================================

    /// Validate state transition
    [[nodiscard]] bool can_transition_to(association_state new_state) const;

    /// Perform state transition
    void transition(association_state new_state);

    /// Build presentation context map from accepted contexts
    void build_context_map();

    /// Negotiate presentation contexts for SCP
    void negotiate_contexts(const associate_rq& rq, const scp_config& config);

    // =========================================================================
    // Member Variables
    // =========================================================================

    /// Current state
    association_state state_{association_state::idle};

    /// Calling AE Title
    std::string calling_ae_;

    /// Called AE Title
    std::string called_ae_;

    /// Our AE Title (may be calling or called depending on role)
    std::string our_ae_;

    /// Negotiated maximum PDU size
    uint32_t max_pdu_size_{DEFAULT_MAX_PDU_LENGTH};

    /// Our implementation class UID
    std::string our_implementation_class_;

    /// Our implementation version name
    std::string our_implementation_version_;

    /// Remote implementation class UID
    std::string remote_implementation_class_;

    /// Remote implementation version name
    std::string remote_implementation_version_;

    /// Proposed presentation contexts (SCU)
    std::vector<proposed_presentation_context> proposed_contexts_;

    /// Accepted presentation contexts
    std::vector<accepted_presentation_context> accepted_contexts_;

    /// Map from abstract syntax to accepted context ID
    std::map<std::string, uint8_t> abstract_syntax_to_context_;

    /// Map from context ID to transfer syntax
    std::map<uint8_t, encoding::transfer_syntax> context_to_transfer_syntax_;

    /// Rejection information (if rejected)
    std::optional<rejection_info> rejection_info_;

    /// Abort source (if aborted)
    uint8_t abort_source_{0};

    /// Abort reason (if aborted)
    uint8_t abort_reason_{0};

    /// Thread safety mutex
    mutable std::mutex mutex_;

    /// Is this an SCU (true) or SCP (false)?
    bool is_scu_{true};

    /// Peer association for in-memory testing
    association* peer_{nullptr};

    /// Incoming message queue for in-memory testing (thread-safe)
    using message_type = std::pair<uint8_t, dimse::dimse_message>;
    using message_queue_type = kcenon::thread::concurrent_queue<message_type>;
    mutable std::unique_ptr<message_queue_type> incoming_queue_{
        std::make_unique<message_queue_type>()};
};

}  // namespace pacs::network

#endif  // PACS_NETWORK_ASSOCIATION_HPP
