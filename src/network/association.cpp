// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file association.cpp
 * @brief DICOM Association implementation
 */

#include "kcenon/pacs/network/association.h"
#include "kcenon/pacs/network/detail/dimse_framing.h"
#include "kcenon/pacs/network/detail/tcp_stream.h"
#include "kcenon/pacs/network/pdu_encoder.h"
#include "kcenon/pacs/network/pdu_decoder.h"
#include "kcenon/pacs/network/dicom_server.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <deque>
#include <sstream>
#include <stdexcept>

namespace kcenon::pacs::network {

// Use standardized error_info from kcenon::pacs::core::result.hpp
using kcenon::pacs::error_info;
using kcenon::pacs::error_codes::association_rejected;
using kcenon::pacs::error_codes::association_aborted;
using kcenon::pacs::error_codes::connection_failed;
using kcenon::pacs::error_codes::connection_timeout;
using kcenon::pacs::error_codes::invalid_association_state;
using kcenon::pacs::error_codes::no_acceptable_context;
using kcenon::pacs::error_codes::pdu_decoding_error;
using kcenon::pacs::error_codes::pdu_error;
using kcenon::pacs::error_codes::release_failed;
using kcenon::pacs::error_codes::already_released;
using kcenon::pacs::error_codes::receive_failed;
using kcenon::pacs::error_codes::receive_timeout;
using kcenon::pacs::error_codes::send_failed;

namespace {

/// PDU header size (type, reserved, 4-byte length)
constexpr std::size_t pdu_header_size = 6;

/// Bytes requested from the socket per read
constexpr std::size_t read_chunk_size = 64 * 1024;

/// Time allowed for writing an A-ABORT, which is sent best effort
constexpr association::duration abort_write_timeout{1000};

/// Whether @p host names this machine through the loopback interface, so
/// an in-process dicom_server on the port is the intended peer
bool is_loopback_host(std::string_view host) {
    std::string lowered(host);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lowered == "localhost" || lowered == "::1" || lowered == "[::1]" ||
           lowered.rfind("127.", 0) == 0;
}

/// A whole PDU read from the TCP connection, header included
struct received_pdu {
    pdu_type type;
    std::vector<uint8_t> bytes;
};

/// A-ABORT reason for a received PDU that read_pdu() or a decoder refused
abort_reason abort_reason_for(int error_code) {
    return error_code == kcenon::pacs::error_codes::invalid_pdu_type
        ? abort_reason::unrecognized_pdu
        : abort_reason::invalid_pdu_parameter;
}

}  // namespace

// =============================================================================
// rejection_info Implementation
// =============================================================================

void rejection_info::build_description() {
    std::ostringstream oss;
    oss << "Association rejected: ";

    if (result == reject_result::rejected_permanent) {
        oss << "permanent, ";
    } else {
        oss << "transient, ";
    }

    switch (static_cast<reject_source>(source)) {
        case reject_source::service_user:
            oss << "source=service-user, ";
            switch (static_cast<reject_reason_user>(reason)) {
                case reject_reason_user::no_reason:
                    oss << "reason=no reason given";
                    break;
                case reject_reason_user::application_context_not_supported:
                    oss << "reason=application context not supported";
                    break;
                case reject_reason_user::calling_ae_not_recognized:
                    oss << "reason=calling AE title not recognized";
                    break;
                case reject_reason_user::called_ae_not_recognized:
                    oss << "reason=called AE title not recognized";
                    break;
                default:
                    oss << "reason=unknown (" << static_cast<int>(reason) << ")";
            }
            break;
        case reject_source::service_provider_acse:
            oss << "source=service-provider (ACSE), ";
            switch (static_cast<reject_reason_provider_acse>(reason)) {
                case reject_reason_provider_acse::no_reason:
                    oss << "reason=no reason given";
                    break;
                case reject_reason_provider_acse::protocol_version_not_supported:
                    oss << "reason=protocol version not supported";
                    break;
                default:
                    oss << "reason=unknown (" << static_cast<int>(reason) << ")";
            }
            break;
        case reject_source::service_provider_presentation:
            oss << "source=service-provider (Presentation), ";
            switch (static_cast<reject_reason_provider_presentation>(reason)) {
                case reject_reason_provider_presentation::temporary_congestion:
                    oss << "reason=temporary congestion";
                    break;
                case reject_reason_provider_presentation::local_limit_exceeded:
                    oss << "reason=local limit exceeded";
                    break;
                default:
                    oss << "reason=unknown (" << static_cast<int>(reason) << ")";
            }
            break;
        default:
            oss << "source=unknown (" << static_cast<int>(source) << ")";
    }

    description = oss.str();
}

// =============================================================================
// In-memory peer link (test hook)
// =============================================================================

/**
 * @brief Pairing state shared by the two ends of an in-memory association.
 *
 * The in-memory dicom_server hook pairs a client association with a server
 * association. Instead of holding a raw pointer to the other end, each end
 * owns a slot here and clears it when it is released, aborted, or destroyed.
 * A sender looks the other end up and enqueues under the same mutex, so a
 * send never reaches an association that is gone.
 *
 * Lock order: an association's mutex_ before peer_link::mutex. Code holding
 * peer_link::mutex never locks an association's mutex_; it only touches the
 * other end's incoming_queue_, which is thread-safe.
 */
struct association::peer_link {
    std::mutex mutex;

    /// The two ends; a slot is null once its end has left
    std::array<association*, 2> ends{nullptr, nullptr};

    /// How each end left (released or aborted), set when its slot is cleared
    std::array<association_state, 2> exit_states{
        association_state::aborted, association_state::aborted};
};

// =============================================================================
// Transport (TCP connection or PDU writer)
// =============================================================================

/**
 * @brief How an association that is not paired in memory reaches its peer.
 *
 * An SCU from connect() owns a TCP connection and reads its PDUs itself. An
 * SCP accepted by dicom_server_v2 only writes: the server reads the session
 * and hands decoded messages to its services.
 *
 * Lock order: read_mutex, then the association's mutex_, then write_mutex.
 * The association's mutex_ is never held while waiting for the peer.
 */
struct association::transport {
    /// TCP connection (null for a PDU writer)
    std::unique_ptr<detail::tcp_stream> stream;

    /// Network layer writer (empty for a TCP connection)
    pdu_writer writer;

    /// Limit for writing one batch of PDUs
    duration io_timeout{default_timeout};

    /// Largest PDU length field accepted for P-DATA-TF from the peer: the
    /// maximum PDU length this end announced
    uint32_t receive_limit{detail::max_accepted_pdu_length};

    /// Serializes writes
    std::mutex write_mutex;

    /// Serializes reads; guards the members below
    std::mutex read_mutex;

    /// Bytes read but not yet framed into a PDU
    std::vector<uint8_t> received;

    /// Scratch buffer for socket reads
    std::vector<uint8_t> chunk;

    /// Messages completed but not yet returned by receive_dimse()
    std::deque<message_type> ready;

    /// Reassembles P-DATA-TF fragments (set once the association is
    /// established)
    std::unique_ptr<detail::dimse_assembler> assembler;

    /// Write @p pdus (complete PDUs back to back)
    [[nodiscard]] VoidResult write(std::vector<uint8_t> pdus, duration timeout) {
        std::lock_guard<std::mutex> lock(write_mutex);
        if (stream) {
            return stream->write_all(pdus, timeout);
        }
        if (writer && writer(std::move(pdus))) {
            return kcenon::pacs::ok();
        }
        return error_info{send_failed, "The network layer did not take the PDUs", "network"};
    }

    /// Make a read or write blocked on another thread return
    void shutdown() noexcept {
        if (stream) {
            stream->shutdown();
        }
    }

    /**
     * @brief Read the next PDU (requires read_mutex and a TCP connection).
     *
     * The header is checked before the body is buffered: an unknown PDU
     * type, or a length above receive_limit (P-DATA-TF) or
     * max_accepted_pdu_length (others), is refused, so a length field never
     * makes the buffer grow beyond what the peer actually sent. Bytes of a
     * PDU that is still arriving stay buffered for the next call.
     *
     * @return The PDU; receive_timeout; receive_failed when the connection
     *         closed or broke; invalid_pdu_type or malformed_pdu for a
     *         header that must be answered with A-ABORT
     */
    [[nodiscard]] Result<received_pdu> read_pdu(time_point deadline) {
        if (chunk.empty()) {
            chunk.resize(read_chunk_size);
        }
        for (;;) {
            if (received.size() >= pdu_header_size) {
                const uint8_t type = received[0];
                if (!pdu_decoder::peek_pdu_type(received)) {
                    return error_info{kcenon::pacs::error_codes::invalid_pdu_type,
                        "Unrecognized PDU type " + std::to_string(type), "network"};
                }
                const uint32_t length = (static_cast<uint32_t>(received[2]) << 24) |
                                        (static_cast<uint32_t>(received[3]) << 16) |
                                        (static_cast<uint32_t>(received[4]) << 8) |
                                        static_cast<uint32_t>(received[5]);
                const uint32_t limit = type == static_cast<uint8_t>(pdu_type::p_data_tf)
                    ? receive_limit
                    : detail::max_accepted_pdu_length;
                if (length > limit) {
                    return error_info{kcenon::pacs::error_codes::malformed_pdu,
                        "PDU length " + std::to_string(length) +
                        " exceeds the limit " + std::to_string(limit), "network"};
                }
                const std::size_t total = pdu_header_size + length;
                if (received.size() >= total) {
                    received_pdu pdu{static_cast<pdu_type>(type),
                        std::vector<uint8_t>(received.begin(),
                            received.begin() + static_cast<std::ptrdiff_t>(total))};
                    received.erase(received.begin(),
                                   received.begin() + static_cast<std::ptrdiff_t>(total));
                    return pdu;
                }
            }

            const auto left = std::chrono::duration_cast<duration>(deadline - clock::now());
            auto count = stream->read_some(chunk, (std::max)(left, duration{0}));
            if (count.is_err()) {
                return count.error();
            }
            received.insert(received.end(), chunk.begin(),
                            chunk.begin() + static_cast<std::ptrdiff_t>(count.value()));
        }
    }
};

// =============================================================================
// association Implementation - Construction/Destruction
// =============================================================================

association::association() = default;

association::association(association&& other) noexcept {
    std::lock_guard<std::mutex> lock(other.mutex_);
    state_ = other.state_;
    calling_ae_ = std::move(other.calling_ae_);
    called_ae_ = std::move(other.called_ae_);
    our_ae_ = std::move(other.our_ae_);
    max_pdu_size_ = other.max_pdu_size_;
    our_implementation_class_ = std::move(other.our_implementation_class_);
    our_implementation_version_ = std::move(other.our_implementation_version_);
    remote_implementation_class_ = std::move(other.remote_implementation_class_);
    remote_implementation_version_ = std::move(other.remote_implementation_version_);
    proposed_contexts_ = std::move(other.proposed_contexts_);
    accepted_contexts_ = std::move(other.accepted_contexts_);
    abstract_syntax_to_context_ = std::move(other.abstract_syntax_to_context_);
    context_to_transfer_syntax_ = std::move(other.context_to_transfer_syntax_);
    rejection_info_ = std::move(other.rejection_info_);
    abort_source_ = other.abort_source_;
    abort_reason_ = other.abort_reason_;
    is_scu_ = other.is_scu_;
    take_peer_link_from(other);
    transport_ = std::move(other.transport_);

    other.state_ = association_state::idle;
}

association& association::operator=(association&& other) noexcept {
    if (this != &other) {
        std::scoped_lock lock(mutex_, other.mutex_);
        // The association being overwritten ends here
        leave_peer_link();
        if (transport_ && transport_->stream) {
            if (state_ == association_state::established) {
                (void)transport_->write(
                    pdu_encoder::encode_abort(abort_source::service_user,
                                              abort_reason::not_specified),
                    abort_write_timeout);
            }
            transport_->shutdown();
        }
        state_ = other.state_;
        calling_ae_ = std::move(other.calling_ae_);
        called_ae_ = std::move(other.called_ae_);
        our_ae_ = std::move(other.our_ae_);
        max_pdu_size_ = other.max_pdu_size_;
        our_implementation_class_ = std::move(other.our_implementation_class_);
        our_implementation_version_ = std::move(other.our_implementation_version_);
        remote_implementation_class_ = std::move(other.remote_implementation_class_);
        remote_implementation_version_ = std::move(other.remote_implementation_version_);
        proposed_contexts_ = std::move(other.proposed_contexts_);
        accepted_contexts_ = std::move(other.accepted_contexts_);
        abstract_syntax_to_context_ = std::move(other.abstract_syntax_to_context_);
        context_to_transfer_syntax_ = std::move(other.context_to_transfer_syntax_);
        rejection_info_ = std::move(other.rejection_info_);
        abort_source_ = other.abort_source_;
        abort_reason_ = other.abort_reason_;
        is_scu_ = other.is_scu_;
        take_peer_link_from(other);
        transport_ = std::move(other.transport_);

        other.state_ = association_state::idle;
    }
    return *this;
}

association::~association() {
    std::shared_ptr<transport> link;
    bool notify_peer = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ == association_state::established) {
            // Abort on destruction; over TCP the peer is told with A-ABORT
            state_ = association_state::aborted;
            notify_peer = true;
        }
        // An in-memory peer must not keep sending to this object
        leave_peer_link();
        link = std::move(transport_);
    }

    // A writer's session belongs to dicom_server_v2, which closes it
    if (link && link->stream) {
        if (notify_peer) {
            (void)link->write(
                pdu_encoder::encode_abort(abort_source::service_user,
                                          abort_reason::not_specified),
                abort_write_timeout);
        }
        link->shutdown();
    }
}

// =============================================================================
// Factory Methods
// =============================================================================

Result<association> association::connect(
    const std::string& host,
    uint16_t port,
    const association_config& config,
    duration timeout) {

    association assoc = propose(config);

    // In-process dicom_server (test hook): pair in memory, no socket. Only
    // for loopback hosts, so a remote peer on the same port is not hijacked.
    if (is_loopback_host(host)) {
        if (auto* server = dicom_server::get_server_on_port(port)) {
            auto rq = assoc.build_associate_rq();
            auto result = server->simulate_association_request(rq, &assoc);
            if (result.is_ok()) {
                if (assoc.process_associate_ac(result.value())) {
                    return assoc;
                } else {
                    return error_info{no_acceptable_context, "Association negotiation failed", "network"};
                }
            } else {
                return result.error();
            }
        }
    }

    auto established = assoc.establish_over_tcp(host, port, timeout);
    if (established.is_err()) {
        return established.error();
    }
    return assoc;
}

association association::propose(const association_config& config) {
    association assoc;
    assoc.is_scu_ = true;
    assoc.calling_ae_ = config.calling_ae_title;
    assoc.called_ae_ = config.called_ae_title;
    assoc.our_ae_ = config.calling_ae_title;
    assoc.max_pdu_size_ = config.max_pdu_length;
    assoc.our_implementation_class_ = config.implementation_class_uid;
    assoc.our_implementation_version_ = config.implementation_version_name;

    // Copy proposed contexts
    for (const auto& pc : config.proposed_contexts) {
        assoc.proposed_contexts_.push_back(pc);
    }

    // Transition to awaiting state
    assoc.transition(association_state::awaiting_associate_ac);

    return assoc;
}

association association::accept(
    const associate_rq& rq,
    const scp_config& config) {

    association assoc;
    assoc.is_scu_ = false;
    assoc.calling_ae_ = rq.calling_ae_title;
    assoc.called_ae_ = rq.called_ae_title;
    assoc.our_ae_ = config.ae_title;
    assoc.max_pdu_size_ = detail::negotiated_max_pdu_length(
        config.max_pdu_length, rq.user_info.max_pdu_length);
    assoc.our_implementation_class_ = config.implementation_class_uid;
    assoc.our_implementation_version_ = config.implementation_version_name;
    assoc.remote_implementation_class_ = rq.user_info.implementation_class_uid;
    assoc.remote_implementation_version_ = rq.user_info.implementation_version_name;

    // Negotiate presentation contexts
    assoc.negotiate_contexts(rq, config);

    // Build lookup maps
    assoc.build_context_map();

    // Establish association if at least one context was accepted
    // Note: For SCP, TCP connection is already established when accept() is called,
    // so we directly set state to established (bypassing state machine)
    bool has_accepted = std::any_of(
        assoc.accepted_contexts_.begin(),
        assoc.accepted_contexts_.end(),
        [](const auto& ctx) { return ctx.is_accepted(); });

    if (has_accepted) {
        // SCP: A-ASSOCIATE-RQ received and accepted, go directly to established
        assoc.state_ = association_state::established;
    } else {
        // No acceptable contexts - will need to reject
        assoc.state_ = association_state::idle;
    }

    return assoc;
}

associate_rj association::reject(
    reject_result result,
    uint8_t source,
    uint8_t reason) {

    return associate_rj{result, source, reason};
}

// =============================================================================
// State Queries
// =============================================================================

association_state association::state() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

bool association::is_established() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_ == association_state::established;
}

bool association::is_closed() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_ == association_state::released ||
           state_ == association_state::aborted ||
           state_ == association_state::idle;
}

// =============================================================================
// Negotiated Parameters
// =============================================================================

std::string_view association::calling_ae() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return calling_ae_;
}

std::string_view association::called_ae() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return called_ae_;
}

uint32_t association::max_pdu_size() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return max_pdu_size_;
}

std::string_view association::remote_implementation_class() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return remote_implementation_class_;
}

std::string_view association::remote_implementation_version() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return remote_implementation_version_;
}

// =============================================================================
// Presentation Context Management
// =============================================================================

bool association::has_accepted_context(std::string_view abstract_syntax) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return abstract_syntax_to_context_.find(std::string(abstract_syntax)) !=
           abstract_syntax_to_context_.end();
}

std::optional<uint8_t> association::accepted_context_id(
    std::string_view abstract_syntax) const {

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = abstract_syntax_to_context_.find(std::string(abstract_syntax));
    if (it != abstract_syntax_to_context_.end()) {
        return it->second;
    }
    return std::nullopt;
}

Result<encoding::transfer_syntax> association::context_transfer_syntax(
    uint8_t pc_id) const {

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = context_to_transfer_syntax_.find(pc_id);
    if (it == context_to_transfer_syntax_.end()) {
        return error_info{no_acceptable_context,
            "Presentation context ID not found: " + std::to_string(pc_id),
            "network"};
    }
    return it->second;
}

const std::vector<accepted_presentation_context>&
association::accepted_contexts() const noexcept {
    // Note: Not thread-safe for iteration, but const reference is OK
    return accepted_contexts_;
}

// =============================================================================
// DIMSE Operations
// =============================================================================

Result<std::monostate> association::send_dimse(
    uint8_t context_id,
    const dimse::dimse_message& msg) {

    std::unique_lock<std::mutex> lock(mutex_);

    if (state_ != association_state::established) {
        return error_info{invalid_association_state, "Cannot send DIMSE: association not established", "network"};
    }

    // Verify context exists
    auto context = context_to_transfer_syntax_.find(context_id);
    if (context == context_to_transfer_syntax_.end()) {
        return error_info{kcenon::pacs::error_codes::dimse_error, "Invalid presentation context ID", "network"};
    }

    if (!msg.is_valid()) {
        return error_info{kcenon::pacs::error_codes::dimse_error, "Invalid DIMSE message", "network"};
    }

    if (transport_) {
        // TCP connection or PDU writer: encode and write without holding
        // mutex_, so state queries and abort() stay responsive
        auto link = transport_;
        const auto transfer_syntax = context->second;
        const uint32_t peer_max_pdu = max_pdu_size_;
        lock.unlock();

        auto pdus = detail::encode_dimse_pdus(context_id, msg, transfer_syntax, peer_max_pdu);
        if (pdus.is_err()) {
            return pdus.error();
        }
        auto written = link->write(std::move(pdus.value()), link->io_timeout);
        if (written.is_err()) {
            // The connection is unusable once a write failed part way
            end_transport(*link, association_state::aborted);
            return error_info{send_failed,
                "Cannot send DIMSE: " + written.error().message, "network"};
        }
        return std::monostate{};
    }

    if (peer_link_) {
        {
            std::lock_guard<std::mutex> link_lock(peer_link_->mutex);
            if (auto* peer = peer_link_->ends[1 - peer_end_]) {
                peer->enqueue_message(context_id, msg);
                return std::monostate{};
            }
        }
        // The in-memory peer released, aborted, or was destroyed
        finish_after_peer_left(*peer_exit_state());
        return error_info{association_aborted,
            "Cannot send DIMSE: the in-memory peer released or aborted the association",
            "network"};
    }

    return std::monostate{};
}

Result<std::pair<uint8_t, dimse::dimse_message>> association::receive_dimse(
    duration timeout) {

    std::shared_ptr<transport> link;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ != association_state::established) {
            return error_info{invalid_association_state, "Cannot receive DIMSE: association not established", "network"};
        }
        link = transport_;
    }

    if (link && link->stream) {
        return receive_over_tcp(*link, timeout);
    }

    // In-memory peer: use the lock-free queue with blocking wait (a PDU
    // writer's SCP gets its messages from dicom_server_v2 instead, so its
    // queue stays empty)
    if (auto item = incoming_queue_->wait_dequeue(timeout)) {
        return std::move(*item);
    }

    // Check if we were aborted/released while waiting, or the in-memory peer
    // left (it wakes this queue when it does)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ == association_state::established) {
            if (auto peer_exit = peer_exit_state()) {
                // A peer that left cannot enqueue any more, so this only
                // drains what it sent before leaving
                if (auto item = incoming_queue_->try_dequeue()) {
                    return std::move(*item);
                }
                finish_after_peer_left(*peer_exit);
            }
        }
        if (state_ != association_state::established) {
            return error_info{association_aborted, "Association aborted or released", "network"};
        }
    }

    return error_info{receive_timeout, "Receive timeout", "network"};
}

// =============================================================================
// PDU Access
// =============================================================================

associate_rq association::build_associate_rq() const {
    std::lock_guard<std::mutex> lock(mutex_);

    associate_rq rq;
    rq.calling_ae_title = calling_ae_;
    rq.called_ae_title = called_ae_;
    rq.application_context = DICOM_APPLICATION_CONTEXT;

    // Convert proposed contexts
    for (const auto& pc : proposed_contexts_) {
        rq.presentation_contexts.push_back(
            presentation_context_rq{pc.id, pc.abstract_syntax, pc.transfer_syntaxes});
    }

    // User information
    rq.user_info.max_pdu_length = max_pdu_size_;
    rq.user_info.implementation_class_uid = our_implementation_class_;
    rq.user_info.implementation_version_name = our_implementation_version_;

    return rq;
}

associate_ac association::build_associate_ac() const {
    std::lock_guard<std::mutex> lock(mutex_);

    associate_ac ac;
    ac.calling_ae_title = calling_ae_;
    ac.called_ae_title = called_ae_;
    ac.application_context = DICOM_APPLICATION_CONTEXT;

    // Convert accepted contexts
    for (const auto& ctx : accepted_contexts_) {
        ac.presentation_contexts.push_back(
            presentation_context_ac{ctx.id, ctx.result, ctx.transfer_syntax});
    }

    // User information
    ac.user_info.max_pdu_length = max_pdu_size_;
    ac.user_info.implementation_class_uid = our_implementation_class_;
    ac.user_info.implementation_version_name = our_implementation_version_;

    return ac;
}

bool association::process_associate_ac(const associate_ac& ac) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ != association_state::awaiting_associate_ac) {
        return false;
    }

    // Store remote implementation info
    remote_implementation_class_ = ac.user_info.implementation_class_uid;
    remote_implementation_version_ = ac.user_info.implementation_version_name;

    // Update max PDU size to negotiated value (0 means no limit); it is
    // never above what the peer can receive, so send_dimse() fragments to it
    max_pdu_size_ = detail::negotiated_max_pdu_length(
        max_pdu_size_, ac.user_info.max_pdu_length);

    // Process accepted presentation contexts
    accepted_contexts_.clear();
    for (const auto& pc_ac : ac.presentation_contexts) {
        // Find the corresponding proposed context
        for (const auto& pc_rq : proposed_contexts_) {
            if (pc_rq.id == pc_ac.id) {
                auto result = pc_ac.result;
                // An acceptor must pick one of the proposed transfer
                // syntaxes (PS3.8 9.3.3.2); a context accepted with any
                // other one is not used
                if (result == presentation_context_result::acceptance &&
                    std::find(pc_rq.transfer_syntaxes.begin(),
                              pc_rq.transfer_syntaxes.end(),
                              pc_ac.transfer_syntax) == pc_rq.transfer_syntaxes.end()) {
                    result = presentation_context_result::transfer_syntaxes_not_supported;
                }
                accepted_contexts_.push_back(accepted_presentation_context{
                    pc_ac.id,
                    pc_rq.abstract_syntax,
                    pc_ac.transfer_syntax,
                    result
                });
                break;
            }
        }
    }

    // Build lookup maps
    build_context_map();

    // Check if at least one context was accepted
    bool has_accepted = std::any_of(
        accepted_contexts_.begin(),
        accepted_contexts_.end(),
        [](const auto& ctx) { return ctx.is_accepted(); });

    if (has_accepted) {
        transition(association_state::established);
        return true;
    }

    return false;
}

void association::process_associate_rj(const associate_rj& rj) {
    std::lock_guard<std::mutex> lock(mutex_);

    rejection_info_ = rejection_info{rj.result, rj.source, rj.reason};
    state_ = association_state::idle;
}

std::optional<rejection_info> association::get_rejection_info() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return rejection_info_;
}

// =============================================================================
// Lifecycle Management
// =============================================================================

Result<std::monostate> association::release(duration timeout) {
    std::shared_ptr<transport> link;
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (state_ == association_state::released) {
            return error_info{already_released, "Association already released", "network"};
        }

        if (state_ != association_state::established) {
            return error_info{invalid_association_state, "Cannot release: association not established", "network"};
        }

        // Transition to awaiting release response
        state_ = association_state::awaiting_release_rp;

        if (!transport_ || !transport_->stream) {
            // In-memory or detached: there is no A-RELEASE-RP to wait for
            state_ = association_state::released;

            // An in-memory peer sees the release once it has drained its queue
            leave_peer_link();

            return std::monostate{};
        }
        link = transport_;
    }

    return release_over_tcp(*link, timeout);
}

void association::process_release_rq() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ == association_state::established) {
        state_ = association_state::awaiting_release_rq;
    }
}

void association::process_release_rp() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ == association_state::awaiting_release_rp) {
        state_ = association_state::released;
        leave_peer_link();
    }
}

void association::abort(uint8_t source, uint8_t reason) {
    std::shared_ptr<transport> link;
    bool notify_peer = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Only a live association tells its peer
        notify_peer = state_ == association_state::awaiting_associate_ac ||
                      state_ == association_state::established ||
                      state_ == association_state::awaiting_release_rp ||
                      state_ == association_state::awaiting_release_rq;
        abort_source_ = source;
        abort_reason_ = reason;
        state_ = association_state::aborted;
        leave_peer_link();

        // Wake up any waiters on the lock-free queue
        incoming_queue_->notify_all();
        link = transport_;
    }

    if (link) {
        if (notify_peer) {
            (void)link->write(pdu_encoder::encode_abort(source, reason), abort_write_timeout);
        }
        // Wakes a receive_dimse() blocked on the connection
        link->shutdown();
    }
}

void association::process_abort(const abort_source& source, const abort_reason& reason) {
    std::lock_guard<std::mutex> lock(mutex_);

    abort_source_ = static_cast<uint8_t>(source);
    abort_reason_ = static_cast<uint8_t>(reason);
    state_ = association_state::aborted;
    leave_peer_link();

    // Wake up any waiters on the lock-free queue
    incoming_queue_->notify_all();
}

void association::set_state(association_state new_state) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = new_state;
}

void association::set_pdu_writer(pdu_writer writer) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!writer) {
        transport_.reset();
        return;
    }
    auto link = std::make_shared<transport>();
    link->writer = std::move(writer);
    transport_ = std::move(link);
}

void association::set_peer(association* peer) {
    if (peer == this) {
        return;
    }

    if (peer == nullptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        leave_peer_link();
        return;
    }

    std::scoped_lock lock(mutex_, peer->mutex_);
    if (peer_link_ && peer_link_ == peer->peer_link_) {
        return;  // Already paired with each other
    }
    leave_peer_link();
    peer->leave_peer_link();

    auto link = std::make_shared<peer_link>();
    link->ends = {this, peer};
    peer_link_ = link;
    peer_end_ = 0;
    peer->peer_link_ = std::move(link);
    peer->peer_end_ = 1;
}

void association::enqueue_message(uint8_t context_id, dimse::dimse_message msg) {
    // Lock-free enqueue with automatic notification
    incoming_queue_->enqueue({context_id, std::move(msg)});
}

void association::update_peer(association* old_peer, association* new_peer) {
    bool paired_with_old = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (peer_link_) {
            std::lock_guard<std::mutex> link_lock(peer_link_->mutex);
            paired_with_old = peer_link_->ends[1 - peer_end_] == old_peer;
        } else {
            paired_with_old = old_peer == nullptr;
        }
    }
    if (paired_with_old) {
        set_peer(new_peer);
    }
}

// =============================================================================
// Private Implementation
// =============================================================================

bool association::can_transition_to(association_state new_state) const {
    // Define valid state transitions per PS3.8
    switch (state_) {
        case association_state::idle:
            return new_state == association_state::awaiting_associate_ac ||
                   new_state == association_state::awaiting_associate_rq;

        case association_state::awaiting_associate_ac:
            return new_state == association_state::established ||
                   new_state == association_state::idle ||
                   new_state == association_state::aborted;

        case association_state::awaiting_associate_rq:
            return new_state == association_state::established ||
                   new_state == association_state::idle ||
                   new_state == association_state::aborted;

        case association_state::established:
            return new_state == association_state::awaiting_release_rp ||
                   new_state == association_state::awaiting_release_rq ||
                   new_state == association_state::aborted;

        case association_state::awaiting_release_rp:
            return new_state == association_state::released ||
                   new_state == association_state::aborted;

        case association_state::awaiting_release_rq:
            return new_state == association_state::released ||
                   new_state == association_state::aborted;

        case association_state::released:
        case association_state::aborted:
            return false;  // Terminal states

        default:
            return false;
    }
}

void association::transition(association_state new_state) {
    if (can_transition_to(new_state)) {
        state_ = new_state;
    }
}

void association::build_context_map() {
    abstract_syntax_to_context_.clear();
    context_to_transfer_syntax_.clear();

    for (const auto& ctx : accepted_contexts_) {
        if (ctx.is_accepted()) {
            abstract_syntax_to_context_[ctx.abstract_syntax] = ctx.id;

            encoding::transfer_syntax ts{ctx.transfer_syntax};
            if (ts.is_valid()) {
                context_to_transfer_syntax_.emplace(ctx.id, std::move(ts));
            }
        }
    }
}

void association::negotiate_contexts(
    const associate_rq& rq,
    const scp_config& config) {

    accepted_contexts_.clear();

    for (const auto& pc_rq : rq.presentation_contexts) {
        // Check if we support the abstract syntax
        bool supports_abstract = config.supported_abstract_syntaxes.empty() ||
            std::find(config.supported_abstract_syntaxes.begin(),
                      config.supported_abstract_syntaxes.end(),
                      pc_rq.abstract_syntax) != config.supported_abstract_syntaxes.end();

        if (!supports_abstract) {
            accepted_contexts_.push_back(accepted_presentation_context{
                pc_rq.id,
                pc_rq.abstract_syntax,
                "",
                presentation_context_result::abstract_syntax_not_supported
            });
            continue;
        }

        // Find first supported transfer syntax
        std::string accepted_ts;
        for (const auto& proposed_ts : pc_rq.transfer_syntaxes) {
            bool supports_ts = config.supported_transfer_syntaxes.empty() ||
                std::find(config.supported_transfer_syntaxes.begin(),
                          config.supported_transfer_syntaxes.end(),
                          proposed_ts) != config.supported_transfer_syntaxes.end();

            if (supports_ts) {
                accepted_ts = proposed_ts;
                break;
            }
        }

        if (accepted_ts.empty()) {
            accepted_contexts_.push_back(accepted_presentation_context{
                pc_rq.id,
                pc_rq.abstract_syntax,
                "",
                presentation_context_result::transfer_syntaxes_not_supported
            });
        } else {
            accepted_contexts_.push_back(accepted_presentation_context{
                pc_rq.id,
                pc_rq.abstract_syntax,
                accepted_ts,
                presentation_context_result::acceptance
            });
        }
    }
}

void association::leave_peer_link() {
    if (!peer_link_) {
        return;
    }

    {
        std::lock_guard<std::mutex> link_lock(peer_link_->mutex);
        peer_link_->ends[peer_end_] = nullptr;
        peer_link_->exit_states[peer_end_] =
            state_ == association_state::released
                ? association_state::released
                : association_state::aborted;

        if (auto* peer = peer_link_->ends[1 - peer_end_]) {
            // Wake the peer's receivers now instead of at their timeout; the
            // peer still dequeues what is queued, then sees this end gone
            peer->incoming_queue_->shutdown();
        }
    }
    peer_link_.reset();
}

void association::take_peer_link_from(association& other) {
    peer_link_ = std::move(other.peer_link_);
    peer_end_ = other.peer_end_;

    // The peer enqueues into whichever object owns this end's slot, so the
    // slot and the queue move together under the link mutex
    std::unique_lock<std::mutex> link_lock;
    if (peer_link_) {
        link_lock = std::unique_lock<std::mutex>(peer_link_->mutex);
        peer_link_->ends[peer_end_] = this;
    }
    incoming_queue_ = std::move(other.incoming_queue_);
    other.incoming_queue_ = std::make_unique<message_queue_type>();
}

std::optional<association_state> association::peer_exit_state() const {
    if (!peer_link_) {
        return std::nullopt;
    }

    std::lock_guard<std::mutex> link_lock(peer_link_->mutex);
    const auto other_end = 1 - peer_end_;
    if (peer_link_->ends[other_end] != nullptr) {
        return std::nullopt;
    }
    return peer_link_->exit_states[other_end];
}

void association::finish_after_peer_left(association_state peer_exit) {
    // The peer already cleared its slot, so dropping the link frees it
    peer_link_.reset();
    state_ = peer_exit == association_state::released
        ? association_state::released
        : association_state::aborted;
}

// =============================================================================
// TCP Transport
// =============================================================================

Result<std::monostate> association::establish_over_tcp(
    const std::string& host, uint16_t port, duration timeout) {

    const auto deadline = clock::now() + timeout;

    auto stream = detail::tcp_stream::connect(host, port, timeout);
    if (stream.is_err()) {
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = association_state::idle;
        return stream.error();
    }

    auto link = std::make_shared<transport>();
    link->stream = std::move(stream.value());
    link->io_timeout = timeout;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // A-ASSOCIATE-RQ announces max_pdu_size_, so that is what the peer
        // may send in one P-DATA-TF PDU
        link->receive_limit = detail::receive_pdu_limit(max_pdu_size_);
        transport_ = link;
    }

    auto sent = link->write(pdu_encoder::encode_associate_rq(build_associate_rq()), timeout);
    if (sent.is_err()) {
        end_transport(*link, association_state::aborted);
        return error_info{connection_failed,
            "Cannot send A-ASSOCIATE-RQ: " + sent.error().message, "network"};
    }

    std::lock_guard<std::mutex> read_lock(link->read_mutex);
    auto pdu = link->read_pdu(deadline);
    if (pdu.is_err()) {
        const auto& error = pdu.error();
        if (error.code == receive_timeout) {
            // ARTIM expired in Sta5
            abort_transport(*link, abort_source::service_provider, abort_reason::not_specified);
            return error_info{connection_timeout,
                "No A-ASSOCIATE response from " + host + ":" + std::to_string(port) +
                " within the timeout", "network"};
        }
        if (error.code == receive_failed) {
            // For example a server at its association limit closes at once
            end_transport(*link, association_state::aborted);
            return error_info{connection_failed,
                "Connection closed before the A-ASSOCIATE response: " + error.message,
                "network"};
        }
        abort_transport(*link, abort_source::service_provider, abort_reason_for(error.code));
        return error_info{pdu_decoding_error, error.message, "network"};
    }

    const auto& response = pdu.value();
    switch (response.type) {
        case pdu_type::associate_ac: {
            auto ac = pdu_decoder::decode_associate_ac(response.bytes);
            if (ac.is_err()) {
                abort_transport(*link, abort_source::service_provider,
                                abort_reason::invalid_pdu_parameter);
                return error_info{pdu_decoding_error,
                    "Malformed A-ASSOCIATE-AC: " + ac.error().message, "network"};
            }
            if (!process_associate_ac(ac.value())) {
                abort_transport(*link, abort_source::service_user, abort_reason::not_specified);
                return error_info{no_acceptable_context,
                    "The peer accepted no proposed presentation context", "network"};
            }

            // Contexts do not change after negotiation, so the assembler
            // keeps its own copy (it must not point into this object,
            // which moves)
            std::map<uint8_t, encoding::transfer_syntax> contexts;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                contexts = context_to_transfer_syntax_;
            }
            link->assembler = std::make_unique<detail::dimse_assembler>(
                [contexts = std::move(contexts)](uint8_t context_id)
                    -> std::optional<encoding::transfer_syntax> {
                    auto it = contexts.find(context_id);
                    if (it == contexts.end()) {
                        return std::nullopt;
                    }
                    return it->second;
                });
            return std::monostate{};
        }

        case pdu_type::associate_rj: {
            auto rj = pdu_decoder::decode_associate_rj(response.bytes);
            end_transport(*link, association_state::idle);
            if (rj.is_err()) {
                return error_info{association_rejected, "Association rejected", "network"};
            }
            process_associate_rj(rj.value());
            return error_info{association_rejected,
                rejection_info{rj.value().result, rj.value().source, rj.value().reason}.description,
                "network"};
        }

        case pdu_type::abort: {
            auto aborted = pdu_decoder::decode_abort(response.bytes);
            if (aborted.is_ok()) {
                process_abort(aborted.value().source, aborted.value().reason);
            }
            end_transport(*link, association_state::aborted);
            return error_info{association_aborted,
                "The peer aborted the association request", "network"};
        }

        default:
            abort_transport(*link, abort_source::service_provider, abort_reason::unexpected_pdu);
            return error_info{pdu_error,
                std::string("Unexpected ") + to_string(response.type) +
                " while awaiting A-ASSOCIATE-AC", "network"};
    }
}

Result<std::pair<uint8_t, dimse::dimse_message>> association::receive_over_tcp(
    transport& link, duration timeout) {

    const auto deadline = clock::now() + timeout;
    std::lock_guard<std::mutex> read_lock(link.read_mutex);

    for (;;) {
        if (!link.ready.empty()) {
            auto message = std::move(link.ready.front());
            link.ready.pop_front();
            return message;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (state_ != association_state::established) {
                return error_info{association_aborted, "Association aborted or released", "network"};
            }
        }

        auto pdu = link.read_pdu(deadline);
        if (pdu.is_err()) {
            const auto& error = pdu.error();
            if (error.code == receive_timeout) {
                return error_info{receive_timeout, "Receive timeout", "network"};
            }
            if (error.code == receive_failed) {
                // Closed by the peer, broken, or shut down by abort()
                end_transport(link, association_state::aborted);
                return error_info{association_aborted,
                    "Association ended: " + error.message, "network"};
            }
            abort_transport(link, abort_source::service_provider, abort_reason_for(error.code));
            return error_info{pdu_decoding_error, error.message, "network"};
        }

        auto& received = pdu.value();
        switch (received.type) {
            case pdu_type::p_data_tf: {
                auto data = pdu_decoder::decode_p_data_tf(received.bytes);
                if (data.is_err()) {
                    abort_transport(link, abort_source::service_provider,
                                    abort_reason::invalid_pdu_parameter);
                    return error_info{pdu_decoding_error,
                        "Malformed P-DATA-TF: " + data.error().message, "network"};
                }
                for (auto& pdv : data.value().pdvs) {
                    auto added = link.assembler->add(std::move(pdv));
                    if (added.is_err()) {
                        abort_transport(link, abort_source::service_provider,
                                        abort_reason::unexpected_pdu_parameter);
                        return added.error();
                    }
                    if (added.value()) {
                        link.ready.push_back(std::move(*added.value()));
                    }
                }
                break;
            }

            case pdu_type::release_rq:
                // The peer releases (Sta6 -> Sta8): confirm and end
                (void)link.write(pdu_encoder::encode_release_rp(), link.io_timeout);
                end_transport(link, association_state::released);
                return error_info{association_aborted, "Association released by the peer", "network"};

            case pdu_type::abort: {
                auto aborted = pdu_decoder::decode_abort(received.bytes);
                if (aborted.is_ok()) {
                    process_abort(aborted.value().source, aborted.value().reason);
                }
                end_transport(link, association_state::aborted);
                return error_info{association_aborted, "Association aborted by the peer", "network"};
            }

            default:
                abort_transport(link, abort_source::service_provider, abort_reason::unexpected_pdu);
                return error_info{pdu_error,
                    std::string("Unexpected ") + to_string(received.type) +
                    " on an established association", "network"};
        }
    }
}

Result<std::monostate> association::release_over_tcp(transport& link, duration timeout) {
    const auto deadline = clock::now() + timeout;

    auto sent = link.write(pdu_encoder::encode_release_rq(), link.io_timeout);
    if (sent.is_err()) {
        end_transport(link, association_state::aborted);
        return error_info{release_failed,
            "Cannot send A-RELEASE-RQ: " + sent.error().message, "network"};
    }

    std::lock_guard<std::mutex> read_lock(link.read_mutex);
    for (;;) {
        auto pdu = link.read_pdu(deadline);
        if (pdu.is_err()) {
            const auto& error = pdu.error();
            if (error.code == receive_timeout) {
                // ARTIM expired in Sta7
                abort_transport(link, abort_source::service_provider, abort_reason::not_specified);
                return error_info{release_failed,
                    "No A-RELEASE-RP within the timeout", "network"};
            }
            if (error.code == receive_failed) {
                end_transport(link, association_state::aborted);
                return error_info{release_failed,
                    "Connection closed before A-RELEASE-RP: " + error.message, "network"};
            }
            abort_transport(link, abort_source::service_provider, abort_reason_for(error.code));
            return error_info{release_failed, error.message, "network"};
        }

        switch (pdu.value().type) {
            case pdu_type::release_rp:
                end_transport(link, association_state::released);
                return std::monostate{};

            case pdu_type::release_rq:
                // Release collision (Sta7 -> Sta9 -> Sta11): as the
                // requestor, answer and keep waiting for the peer's reply
                (void)link.write(pdu_encoder::encode_release_rp(), link.io_timeout);
                break;

            case pdu_type::p_data_tf:
                // PS3.8 allows data before A-RELEASE-RP; nobody reads it now
                break;

            case pdu_type::abort: {
                auto aborted = pdu_decoder::decode_abort(pdu.value().bytes);
                if (aborted.is_ok()) {
                    process_abort(aborted.value().source, aborted.value().reason);
                }
                end_transport(link, association_state::aborted);
                return error_info{release_failed, "Association aborted by the peer", "network"};
            }

            default:
                abort_transport(link, abort_source::service_provider, abort_reason::unexpected_pdu);
                return error_info{release_failed,
                    std::string("Unexpected ") + to_string(pdu.value().type) +
                    " while awaiting A-RELEASE-RP", "network"};
        }
    }
}

void association::abort_transport(transport& link, abort_source source, abort_reason reason) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        abort_source_ = static_cast<uint8_t>(source);
        abort_reason_ = static_cast<uint8_t>(reason);
        state_ = association_state::aborted;
        incoming_queue_->notify_all();
    }
    (void)link.write(pdu_encoder::encode_abort(source, reason), abort_write_timeout);
    link.shutdown();
}

void association::end_transport(transport& link, association_state final_state) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // Keep an abort that another thread or the peer already recorded
        if (state_ != association_state::aborted) {
            state_ = final_state;
        }
        incoming_queue_->notify_all();
    }
    link.shutdown();
}

}  // namespace kcenon::pacs::network
