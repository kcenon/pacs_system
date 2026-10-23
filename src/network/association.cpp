// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file association.cpp
 * @brief DICOM Association implementation
 */

#include "kcenon/pacs/network/association.h"
#include "kcenon/pacs/network/pdu_encoder.h"
#include "kcenon/pacs/network/dicom_server.h"

#include <algorithm>
#include <array>
#include <sstream>
#include <stdexcept>

namespace kcenon::pacs::network {

// Use standardized error_info from kcenon::pacs::core::result.hpp
using kcenon::pacs::error_info;
using kcenon::pacs::error_codes::association_rejected;
using kcenon::pacs::error_codes::association_aborted;
using kcenon::pacs::error_codes::invalid_association_state;
using kcenon::pacs::error_codes::no_acceptable_context;
using kcenon::pacs::error_codes::release_failed;
using kcenon::pacs::error_codes::already_released;
using kcenon::pacs::error_codes::receive_timeout;

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

    other.state_ = association_state::idle;
}

association& association::operator=(association&& other) noexcept {
    if (this != &other) {
        std::scoped_lock lock(mutex_, other.mutex_);
        // The association being overwritten ends here
        leave_peer_link();
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

        other.state_ = association_state::idle;
    }
    return *this;
}

association::~association() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ == association_state::established) {
        // Abort silently on destruction
        state_ = association_state::aborted;
    }
    // An in-memory peer must not keep sending to this object
    leave_peer_link();
}

// =============================================================================
// Factory Methods
// =============================================================================

Result<association> association::connect(
    const std::string& host,
    uint16_t port,
    const association_config& config,
    duration /*timeout*/) {

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

    // Check for in-memory server (Test Hook)
    auto* server = dicom_server::get_server_on_port(port);
    if (server) {
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

    // Note: Actual network connection should be handled by network_system
    // This implementation prepares the association for PDU exchange
    (void)host;  // Will be used with network_system integration
    (void)port;

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
    assoc.max_pdu_size_ = (std::min)(config.max_pdu_length, rq.user_info.max_pdu_length);
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

    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ != association_state::established) {
        return error_info{invalid_association_state, "Cannot send DIMSE: association not established", "network"};
    }

    // Verify context exists
    if (context_to_transfer_syntax_.find(context_id) ==
        context_to_transfer_syntax_.end()) {
        return error_info{kcenon::pacs::error_codes::dimse_error, "Invalid presentation context ID", "network"};
    }

    // Message encoding and network send would be handled by network_system
    // For now, validate the message
    if (!msg.is_valid()) {
        return error_info{kcenon::pacs::error_codes::dimse_error, "Invalid DIMSE message", "network"};
    }

    // Placeholder for actual send implementation
    (void)msg;

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

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ != association_state::established) {
            return error_info{invalid_association_state, "Cannot receive DIMSE: association not established", "network"};
        }
    }

    // Placeholder - actual receive would be implemented with network_system
    // return error_info("Not implemented: requires network_system integration");

    // Use lock-free queue with blocking wait
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

    // Update max PDU size to negotiated value
    max_pdu_size_ = (std::min)(max_pdu_size_, ac.user_info.max_pdu_length);

    // Process accepted presentation contexts
    accepted_contexts_.clear();
    for (const auto& pc_ac : ac.presentation_contexts) {
        // Find the corresponding proposed context
        for (const auto& pc_rq : proposed_contexts_) {
            if (pc_rq.id == pc_ac.id) {
                accepted_contexts_.push_back(accepted_presentation_context{
                    pc_ac.id,
                    pc_rq.abstract_syntax,
                    pc_ac.transfer_syntax,
                    pc_ac.result
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

Result<std::monostate> association::release(duration /*timeout*/) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ == association_state::released) {
        return error_info{already_released, "Association already released", "network"};
    }

    if (state_ != association_state::established) {
        return error_info{invalid_association_state, "Cannot release: association not established", "network"};
    }

    // Transition to awaiting release response
    state_ = association_state::awaiting_release_rp;

    // Note: Actual A-RELEASE-RQ sending and A-RELEASE-RP receiving
    // would be handled by network_system integration

    // For now, simulate successful release
    state_ = association_state::released;

    // An in-memory peer sees the release once it has drained its queue
    leave_peer_link();

    return std::monostate{};
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
    std::lock_guard<std::mutex> lock(mutex_);

    abort_source_ = source;
    abort_reason_ = reason;
    state_ = association_state::aborted;
    leave_peer_link();

    // Wake up any waiters on the lock-free queue
    incoming_queue_->notify_all();
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

}  // namespace kcenon::pacs::network
