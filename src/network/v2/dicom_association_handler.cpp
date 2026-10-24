// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file dicom_association_handler.cpp
 * @brief DICOM association handler implementation for network_system integration
 */

#include "kcenon/pacs/network/v2/dicom_association_handler.h"
#include "kcenon/pacs/network/detail/dimse_framing.h"
#include "kcenon/pacs/network/detail/scp_transfer_syntaxes.h"
#include "kcenon/pacs/network/pdu_encoder.h"
#include "kcenon/pacs/network/pdu_decoder.h"

#include <span>
#include <sstream>
#include <stdexcept>

// KCENON_HAS_COMMON_SYSTEM is defined by CMake when common_system is available
#ifndef KCENON_HAS_COMMON_SYSTEM
#define KCENON_HAS_COMMON_SYSTEM 0
#endif

#ifdef PACS_WITH_NETWORK_SYSTEM
#include <kcenon/network/interfaces/i_session.h>
#endif

#if KCENON_HAS_COMMON_SYSTEM
using kcenon::common::error_info;
#endif

namespace kcenon::pacs::network::v2 {

// =============================================================================
// Construction / Destruction
// =============================================================================

dicom_association_handler::dicom_association_handler(
    session_ptr session,
    const server_config& config,
    const service_map& services,
    std::string session_id)
    : session_(std::move(session))
    , session_id_(std::move(session_id))
    , config_(config)
    , services_(services)
    , receive_limit_(detail::max_accepted_pdu_length)
    , last_activity_(clock::now()) {
#ifdef PACS_WITH_NETWORK_SYSTEM
    if (session_id_.empty() && session_) {
        session_id_ = std::string(session_->id());
    }
#endif
}

dicom_association_handler::~dicom_association_handler() {
    try {
        // Ensure we're stopped
        if (!is_closed()) {
            stop(false);  // Force abort if still running
        }
    } catch (...) {
        // Suppress exceptions in destructor to prevent std::terminate
    }
}

// =============================================================================
// Lifecycle Management
// =============================================================================

void dicom_association_handler::start() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ != handler_state::idle) {
        report_error("Cannot start: handler not in idle state");
        return;
    }

    if (!session_) {
        report_error("Cannot start: no session available");
        return;
    }

#ifdef PACS_WITH_NETWORK_SYSTEM
    // In the i_protocol_server architecture, the server forwards events
    // to the handler via feed_data(), handle_disconnect(), and handle_error().
    // No session-level callback setup needed.
#endif

    touch();
}

void dicom_association_handler::stop(bool graceful) {
    handler_state expected = state_.load();
    if (expected == handler_state::closed) {
        return;  // Already closed
    }

    if (graceful && expected == handler_state::established) {
        // Try graceful release first
        transition_to(handler_state::releasing);

        // Send A-RELEASE-RQ would go here if we were initiating release
        // For now, just close
    }

    close_handler(graceful);
}

// =============================================================================
// State Queries
// =============================================================================

handler_state dicom_association_handler::state() const noexcept {
    return state_.load(std::memory_order_acquire);
}

bool dicom_association_handler::is_established() const noexcept {
    return state_.load(std::memory_order_acquire) == handler_state::established;
}

bool dicom_association_handler::is_closed() const noexcept {
    return state_.load(std::memory_order_acquire) == handler_state::closed;
}

std::string dicom_association_handler::session_id() const {
    // No lock: the PDU processing path calls this while it holds mutex_
    return session_id_;
}

std::string dicom_association_handler::calling_ae() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::string(association_.calling_ae());
}

std::string dicom_association_handler::called_ae() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::string(association_.called_ae());
}

Result<std::reference_wrapper<association>> dicom_association_handler::get_association() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!is_established()) {
        return kcenon::pacs::error_info{kcenon::pacs::error_codes::invalid_association_state,
            "Association not established", "network"};
    }
    return std::ref(association_);
}

Result<std::reference_wrapper<const association>> dicom_association_handler::get_association() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!is_established()) {
        return kcenon::pacs::error_info{kcenon::pacs::error_codes::invalid_association_state,
            "Association not established", "network"};
    }
    return std::cref(association_);
}

dicom_association_handler::time_point dicom_association_handler::last_activity() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_activity_;
}

// =============================================================================
// Callbacks
// =============================================================================

void dicom_association_handler::set_established_callback(
    association_established_callback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    established_callback_ = std::move(callback);
}

void dicom_association_handler::set_closed_callback(
    association_closed_callback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    closed_callback_ = std::move(callback);
}

void dicom_association_handler::set_error_callback(handler_error_callback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    error_callback_ = std::move(callback);
}

void dicom_association_handler::set_session_closer(session_closer closer) {
    session_closer_ = std::move(closer);
}

// =============================================================================
// Statistics
// =============================================================================

uint64_t dicom_association_handler::pdus_received() const noexcept {
    return pdus_received_.load(std::memory_order_relaxed);
}

uint64_t dicom_association_handler::pdus_sent() const noexcept {
    return pdus_sent_.load(std::memory_order_relaxed);
}

uint64_t dicom_association_handler::messages_processed() const noexcept {
    return messages_processed_.load(std::memory_order_relaxed);
}

// =============================================================================
// Security / Access Control
// =============================================================================

void dicom_association_handler::set_access_control(
    std::shared_ptr<security::access_control_manager> acm) {
    std::lock_guard<std::mutex> lock(mutex_);
    access_control_ = std::move(acm);
}

void dicom_association_handler::set_access_control_enabled(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    access_control_enabled_ = enabled;
}

// =============================================================================
// Server-Level Event Forwarding
// =============================================================================

void dicom_association_handler::feed_data(const std::vector<uint8_t>& data) {
    on_data_received(data);
}

void dicom_association_handler::handle_disconnect() {
    on_disconnected(session_id());
}

void dicom_association_handler::handle_error(std::error_code ec) {
    on_error(ec);
}

// =============================================================================
// Network Callbacks
// =============================================================================

void dicom_association_handler::on_data_received(const std::vector<uint8_t>& data) {
    if (is_closed()) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        // Append to receive buffer
        receive_buffer_.insert(receive_buffer_.end(), data.begin(), data.end());
        touch();
    }

    // Process any complete PDUs
    process_buffer();
}

void dicom_association_handler::on_disconnected(const std::string& /*session_id*/) {
    close_handler(true);
}

void dicom_association_handler::on_error(std::error_code ec) {
    // After a release or abort, the peer closing the connection is expected
    if (!is_closed()) {
        std::ostringstream oss;
        oss << "Network error: " << ec.message() << " (" << ec.value() << ")";
        report_error(oss.str());
    }

    close_handler(false);
}

// =============================================================================
// PDU Processing
// =============================================================================

void dicom_association_handler::process_buffer() {
    std::lock_guard<std::mutex> lock(mutex_);

    while (receive_buffer_.size() >= pdu_header_size) {
        // Refuse an unknown type or an oversized length from the header,
        // before the body is buffered
        auto header_type = pdu_decoder::peek_pdu_type(receive_buffer_);
        if (!header_type) {
            report_error("Invalid PDU type received");
            send_abort(abort_source::service_provider, abort_reason::unrecognized_pdu);
            close_handler(false);
            return;
        }
        const uint32_t declared_length =
            (static_cast<uint32_t>(receive_buffer_[2]) << 24) |
            (static_cast<uint32_t>(receive_buffer_[3]) << 16) |
            (static_cast<uint32_t>(receive_buffer_[4]) << 8) |
            static_cast<uint32_t>(receive_buffer_[5]);
        const uint32_t length_limit = *header_type == pdu_type::p_data_tf
            ? receive_limit_
            : detail::max_accepted_pdu_length;
        if (declared_length > length_limit) {
            report_error("PDU length " + std::to_string(declared_length) +
                         " exceeds the limit " + std::to_string(length_limit));
            send_abort(abort_source::service_provider, abort_reason::invalid_pdu_parameter);
            close_handler(false);
            return;
        }

        // Check if we have a complete PDU
        auto length_opt = pdu_decoder::pdu_length(receive_buffer_);
        if (!length_opt) {
            // Not enough data yet
            break;
        }

        size_t pdu_total_length = *length_opt;
        if (receive_buffer_.size() < pdu_total_length) {
            // Not enough data for complete PDU
            break;
        }

        // Extract the complete PDU
        std::vector<uint8_t> pdu_data(
            receive_buffer_.begin(),
            receive_buffer_.begin() + static_cast<std::ptrdiff_t>(pdu_total_length));

        // Remove from buffer
        receive_buffer_.erase(
            receive_buffer_.begin(),
            receive_buffer_.begin() + static_cast<std::ptrdiff_t>(pdu_total_length));

        // Parse PDU type
        auto type_opt = pdu_decoder::peek_pdu_type(pdu_data);
        if (!type_opt) {
            report_error("Invalid PDU type received");
            send_abort(abort_source::service_provider, abort_reason::unrecognized_pdu);
            close_handler(false);
            return;
        }

        pdus_received_.fetch_add(1, std::memory_order_relaxed);

        // Extract payload (skip 6-byte header)
        std::vector<uint8_t> payload(
            pdu_data.begin() + pdu_header_size,
            pdu_data.end());

        // Create pdu_data structure for processing
        integration::pdu_data pdu(*type_opt, std::move(payload));
        process_pdu(pdu);

        // Bytes after a release or abort belong to no association
        if (is_closed()) {
            return;
        }
    }
}

void dicom_association_handler::process_pdu(const integration::pdu_data& pdu) {
    // Note: mutex_ is already held by caller (process_buffer)

    switch (pdu.type) {
        case pdu_type::associate_rq:
            if (state_ == handler_state::idle) {
                handle_associate_rq(pdu.payload);
            } else {
                report_error("Unexpected A-ASSOCIATE-RQ in current state");
                send_abort(abort_source::service_provider, abort_reason::unexpected_pdu);
                close_handler(false);
            }
            break;

        case pdu_type::p_data_tf:
            if (state_ == handler_state::established) {
                handle_p_data_tf(pdu.payload);
            } else {
                report_error("Unexpected P-DATA-TF in current state");
                send_abort(abort_source::service_provider, abort_reason::unexpected_pdu);
                close_handler(false);
            }
            break;

        case pdu_type::release_rq:
            if (state_ == handler_state::established) {
                handle_release_rq();
            } else {
                report_error("Unexpected A-RELEASE-RQ in current state");
                send_abort(abort_source::service_provider, abort_reason::unexpected_pdu);
                close_handler(false);
            }
            break;

        case pdu_type::abort:
            handle_abort(pdu.payload);
            break;

        case pdu_type::associate_ac:
        case pdu_type::associate_rj:
        case pdu_type::release_rp:
            // These are responses, SCP should not receive them in normal flow
            report_error("Unexpected PDU type for SCP: " + std::string(to_string(pdu.type)));
            send_abort(abort_source::service_provider, abort_reason::unexpected_pdu);
            close_handler(false);
            break;

        default:
            report_error("Unknown PDU type");
            send_abort(abort_source::service_provider, abort_reason::unrecognized_pdu);
            close_handler(false);
            break;
    }
}

void dicom_association_handler::handle_associate_rq(const std::vector<uint8_t>& payload) {
    // Build full PDU for decoder (add header back)
    std::vector<uint8_t> full_pdu;
    full_pdu.reserve(pdu_header_size + payload.size());

    // PDU type
    full_pdu.push_back(static_cast<uint8_t>(pdu_type::associate_rq));
    // Reserved
    full_pdu.push_back(0x00);
    // Length (big-endian)
    uint32_t length = static_cast<uint32_t>(payload.size());
    full_pdu.push_back(static_cast<uint8_t>((length >> 24) & 0xFF));
    full_pdu.push_back(static_cast<uint8_t>((length >> 16) & 0xFF));
    full_pdu.push_back(static_cast<uint8_t>((length >> 8) & 0xFF));
    full_pdu.push_back(static_cast<uint8_t>(length & 0xFF));
    // Payload
    full_pdu.insert(full_pdu.end(), payload.begin(), payload.end());

    // Decode the A-ASSOCIATE-RQ
    auto result = pdu_decoder::decode_associate_rq(full_pdu);
    if (!result.is_ok()) {
        report_error("Failed to decode A-ASSOCIATE-RQ");
        send_associate_rj(reject_result::rejected_permanent,
                          static_cast<uint8_t>(reject_source::service_provider_acse),
                          static_cast<uint8_t>(reject_reason_provider_acse::no_reason));
        close_handler(false);
        return;
    }

    const associate_rq& rq = result.value();

    // Validate called AE title
    if (rq.called_ae_title != config_.ae_title) {
        report_error("Called AE title mismatch: expected '" + config_.ae_title +
                     "', got '" + rq.called_ae_title + "'");
        send_associate_rj(reject_result::rejected_permanent,
                          static_cast<uint8_t>(reject_source::service_user),
                          static_cast<uint8_t>(reject_reason_user::called_ae_not_recognized));
        close_handler(false);
        return;
    }

    // Validate calling AE title against whitelist
    if (!config_.ae_whitelist.empty() && !config_.accept_unknown_calling_ae) {
        bool found = false;
        for (const auto& allowed : config_.ae_whitelist) {
            if (allowed == rq.calling_ae_title) {
                found = true;
                break;
            }
        }
        if (!found) {
            report_error("Calling AE title not in whitelist: " + rq.calling_ae_title);
            send_associate_rj(reject_result::rejected_permanent,
                              static_cast<uint8_t>(reject_source::service_user),
                              static_cast<uint8_t>(reject_reason_user::calling_ae_not_recognized));
            close_handler(false);
            return;
        }
    }

    // Build SCP config for association negotiation
    scp_config scp_cfg;
    scp_cfg.ae_title = config_.ae_title;
    scp_cfg.max_pdu_length = config_.max_pdu_size;
    scp_cfg.implementation_class_uid = config_.implementation_class_uid;
    scp_cfg.implementation_version_name = config_.implementation_version_name;

    // Collect supported abstract syntaxes from services
    for (const auto& [uid, _] : services_) {
        scp_cfg.supported_abstract_syntaxes.push_back(uid);
    }

    // Accept compressed and uncompressed transfer syntaxes
    scp_cfg.supported_transfer_syntaxes = detail::scp_transfer_syntax_list();

    // Perform association negotiation
    association_ = association::accept(rq, scp_cfg);

    // Check if any presentation contexts were accepted
    if (association_.accepted_contexts().empty()) {
        report_error("No presentation contexts accepted");
        send_associate_rj(reject_result::rejected_permanent,
                          static_cast<uint8_t>(reject_source::service_user),
                          static_cast<uint8_t>(reject_reason_user::no_reason));
        close_handler(false);
        return;
    }

    // Send A-ASSOCIATE-AC
    send_associate_ac();

    // Transition to established state
    association_.set_state(association_state::established);

    // Responses that services send with association_.send_dimse() go to
    // this session as P-DATA-TF
    association_.set_pdu_writer(
        [this, session = session_](std::vector<uint8_t> pdus) {
#ifdef PACS_WITH_NETWORK_SYSTEM
            if (!session || !session->is_connected() ||
                session->send(std::move(pdus)).is_err()) {
                return false;
            }
            pdus_sent_.fetch_add(1, std::memory_order_relaxed);
            return true;
#else
            (void)session;
            (void)pdus;
            return false;
#endif
        });

    // The A-ASSOCIATE-AC announced max_pdu_size() as the most the SCU may
    // put in one P-DATA-TF PDU
    receive_limit_ = detail::receive_pdu_limit(association_.max_pdu_size());
    assembler_ = std::make_unique<detail::dimse_assembler>(
        [this](uint8_t context_id) -> std::optional<encoding::transfer_syntax> {
            auto ts = association_.context_transfer_syntax(context_id);
            if (ts.is_err()) {
                return std::nullopt;
            }
            return ts.value();
        });

    transition_to(handler_state::established);

    // Set up user context for access control
    if (access_control_enabled_ && access_control_) {
        user_context_ = access_control_->get_context_for_ae(
            rq.calling_ae_title, session_id());
    }

    // Notify callback
    {
        std::lock_guard<std::mutex> cb_lock(callback_mutex_);
        if (established_callback_) {
            established_callback_(session_id(),
                                  rq.calling_ae_title,
                                  rq.called_ae_title);
        }
    }
}

void dicom_association_handler::handle_p_data_tf(const std::vector<uint8_t>& payload) {
    // Build full PDU for decoder
    std::vector<uint8_t> full_pdu;
    full_pdu.reserve(pdu_header_size + payload.size());

    full_pdu.push_back(static_cast<uint8_t>(pdu_type::p_data_tf));
    full_pdu.push_back(0x00);
    uint32_t length = static_cast<uint32_t>(payload.size());
    full_pdu.push_back(static_cast<uint8_t>((length >> 24) & 0xFF));
    full_pdu.push_back(static_cast<uint8_t>((length >> 16) & 0xFF));
    full_pdu.push_back(static_cast<uint8_t>((length >> 8) & 0xFF));
    full_pdu.push_back(static_cast<uint8_t>(length & 0xFF));
    full_pdu.insert(full_pdu.end(), payload.begin(), payload.end());

    // Decode P-DATA-TF
    auto result = pdu_decoder::decode_p_data_tf(full_pdu);
    if (!result.is_ok()) {
        report_error("Failed to decode P-DATA-TF");
        send_abort(abort_source::service_provider, abort_reason::invalid_pdu_parameter);
        close_handler(false);
        return;
    }

    // Command and data set fragments may span several PDUs; the assembler
    // returns each message once its last fragment arrived
    for (auto& pdv : result.value().pdvs) {
        auto added = assembler_->add(std::move(pdv));
        if (added.is_err()) {
            report_error("Invalid P-DATA-TF fragment: " + added.error().message);
            send_abort(abort_source::service_provider, abort_reason::unexpected_pdu_parameter);
            close_handler(false);
            return;
        }
        if (!added.value()) {
            continue;
        }

        const auto& [context_id, message] = *added.value();
        messages_processed_.fetch_add(1, std::memory_order_relaxed);

        // Dispatch to service
        auto dispatch_result = dispatch_to_service(context_id, message);
        if (dispatch_result.is_err()) {
            report_error("Service dispatch failed: " + dispatch_result.error().message);
        }
    }
}

void dicom_association_handler::handle_release_rq() {
    transition_to(handler_state::releasing);

    // Process release on the association
    association_.process_release_rq();

    // Send A-RELEASE-RP
    send_release_rp();

    // Close gracefully
    close_handler(true);
}

void dicom_association_handler::handle_abort(const std::vector<uint8_t>& payload) {
    // Parse abort source and reason if present
    abort_source source = abort_source::service_user;
    abort_reason reason = abort_reason::not_specified;

    if (payload.size() >= 4) {
        // Reserved, Reserved, Source, Reason
        source = static_cast<abort_source>(payload[2]);
        reason = static_cast<abort_reason>(payload[3]);
    }

    association_.process_abort(source, reason);

    // Close immediately
    close_handler(false);
}

// =============================================================================
// Response Sending
// =============================================================================

void dicom_association_handler::send_associate_ac() {
    associate_ac ac = association_.build_associate_ac();
    auto encoded = pdu_encoder::encode_associate_ac(ac);
    send_pdu(pdu_type::associate_ac, encoded);
}

void dicom_association_handler::send_associate_rj(
    reject_result result, uint8_t source, uint8_t reason) {
    associate_rj rj(result, source, reason);
    auto encoded = pdu_encoder::encode_associate_rj(rj);
    send_pdu(pdu_type::associate_rj, encoded);
}

void dicom_association_handler::send_p_data_tf(const std::vector<uint8_t>& payload) {
    // Payload should already be a complete P-DATA-TF PDU
    send_pdu(pdu_type::p_data_tf, payload);
}

void dicom_association_handler::send_release_rp() {
    auto encoded = pdu_encoder::encode_release_rp();
    send_pdu(pdu_type::release_rp, encoded);
}

void dicom_association_handler::send_abort(abort_source source, abort_reason reason) {
    auto encoded = pdu_encoder::encode_abort(source, reason);
    send_pdu(pdu_type::abort, encoded);
}

void dicom_association_handler::send_pdu(pdu_type /*type*/, const std::vector<uint8_t>& encoded_pdu) {
#ifdef PACS_WITH_NETWORK_SYSTEM
    if (session_ && session_->is_connected()) {
        // encoded_pdu already includes header for most PDU types
        // For encode_associate_ac, encode_associate_rj, etc., the encoder returns full PDU

        // We need to make a copy for the async send
        std::vector<uint8_t> data_copy = encoded_pdu;
        (void)session_->send(std::move(data_copy));
        pdus_sent_.fetch_add(1, std::memory_order_relaxed);
    }
#else
    (void)encoded_pdu;
#endif
}

// =============================================================================
// Service Dispatching
// =============================================================================

Result<std::monostate> dicom_association_handler::dispatch_to_service(
    uint8_t context_id,
    const dimse::dimse_message& msg) {

    // Find the abstract syntax for this context
    std::string abstract_syntax;
    for (const auto& ctx : association_.accepted_contexts()) {
        if (ctx.id == context_id) {
            abstract_syntax = ctx.abstract_syntax;
            break;
        }
    }

    if (abstract_syntax.empty()) {
        return error_info("Unknown presentation context ID");
    }

    // Find service for this SOP class
    auto* service = find_service(abstract_syntax);
    if (!service) {
        return error_info("No service registered for SOP Class: " + abstract_syntax);
    }

    // Perform access control check if enabled
    if (access_control_enabled_ && access_control_ && !user_context_) {
        return error_info("Access denied: unregistered AE title");
    }
    if (access_control_enabled_ && access_control_ && user_context_) {
        // Map DIMSE command to DICOM operation
        security::DicomOperation op = security::DicomOperation::CEcho;
        switch (msg.command()) {
            case dimse::command_field::c_store_rq:
                op = security::DicomOperation::CStore;
                break;
            case dimse::command_field::c_find_rq:
                op = security::DicomOperation::CFind;
                break;
            case dimse::command_field::c_move_rq:
                op = security::DicomOperation::CMove;
                break;
            case dimse::command_field::c_get_rq:
                op = security::DicomOperation::CGet;
                break;
            case dimse::command_field::c_echo_rq:
                op = security::DicomOperation::CEcho;
                break;
            case dimse::command_field::n_create_rq:
                op = security::DicomOperation::NCreate;
                break;
            case dimse::command_field::n_set_rq:
                op = security::DicomOperation::NSet;
                break;
            case dimse::command_field::n_get_rq:
                op = security::DicomOperation::NGet;
                break;
            case dimse::command_field::n_delete_rq:
                op = security::DicomOperation::NDelete;
                break;
            case dimse::command_field::n_action_rq:
                op = security::DicomOperation::NAction;
                break;
            case dimse::command_field::n_event_report_rq:
                op = security::DicomOperation::NEventReport;
                break;
            default:
                // Response messages don't need access control
                break;
        }

        // Check permission
        auto check_result = access_control_->check_dicom_operation(*user_context_, op);
        if (!check_result) {
            return error_info("Access denied: " + check_result.reason);
        }
    }

    // Dispatch to service
    return service->handle_message(association_, context_id, msg);
}

services::scp_service* dicom_association_handler::find_service(
    const std::string& sop_class_uid) const {
    auto it = services_.find(sop_class_uid);
    if (it != services_.end()) {
        return it->second;
    }
    return nullptr;
}

// =============================================================================
// State Management
// =============================================================================

void dicom_association_handler::transition_to(handler_state new_state) {
    handler_state old_state = state_.exchange(new_state, std::memory_order_acq_rel);

    // Log state transition if needed
    (void)old_state;
}

void dicom_association_handler::touch() {
    // Note: mutex_ should be held by caller
    last_activity_ = clock::now();
}

void dicom_association_handler::report_error(const std::string& error) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (error_callback_) {
        error_callback_(session_id(), error);
    }
}

void dicom_association_handler::close_handler(bool graceful) {
    // exchange() so that the network thread and stop() close only once
    if (state_.exchange(handler_state::closed, std::memory_order_acq_rel) ==
        handler_state::closed) {
        return;  // Already closed
    }

#ifdef PACS_WITH_NETWORK_SYSTEM
    // Close the session. session_ is set once at construction, so no lock
    // is taken: this runs inside PDU processing, which holds mutex_.
    try {
        if (session_) {
            if (session_closer_) {
                session_closer_(session_);
            } else {
                session_->close();
            }
        }
    } catch (...) {
        // Suppress exceptions during session cleanup
    }
#endif

    // Notify closed callback
    try {
        std::lock_guard<std::mutex> cb_lock(callback_mutex_);
        if (closed_callback_) {
            closed_callback_(session_id(), graceful);
        }
    } catch (...) {
        // Suppress exceptions during callback invocation
    }
}

}  // namespace kcenon::pacs::network::v2
