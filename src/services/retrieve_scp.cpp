// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file retrieve_scp.cpp
 * @brief Implementation of the Retrieve SCP service (C-MOVE/C-GET)
 */

#include "kcenon/pacs/services/retrieve_scp.h"
#include "kcenon/pacs/services/storage_scp.h"

#include "kcenon/pacs/core/dicom_tag_constants.h"
#include "kcenon/pacs/core/events.h"
#include "kcenon/pacs/core/result.h"
#include "kcenon/pacs/network/dimse/command_field.h"
#include "kcenon/pacs/network/dimse/status_codes.h"

#include <kcenon/common/patterns/event_bus.h>

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace kcenon::pacs::services {

// =============================================================================
// Construction
// =============================================================================

retrieve_scp::retrieve_scp(std::shared_ptr<di::ILogger> logger)
    : scp_service(std::move(logger)) {}

// =============================================================================
// Configuration
// =============================================================================

void retrieve_scp::set_retrieve_handler(retrieve_handler handler) {
    retrieve_handler_ = std::move(handler);
}

void retrieve_scp::set_destination_resolver(destination_resolver resolver) {
    destination_resolver_ = std::move(resolver);
}

void retrieve_scp::set_store_sub_operation(store_sub_operation handler) {
    store_handler_ = std::move(handler);
}

void retrieve_scp::set_cancel_check(retrieve_cancel_check check) {
    cancel_check_ = std::move(check);
}

void retrieve_scp::set_move_timeouts(std::chrono::milliseconds connect,
                                     std::chrono::milliseconds response,
                                     std::chrono::milliseconds release) {
    if (connect.count() <= 0 || response.count() <= 0 || release.count() <= 0) {
        throw std::invalid_argument("C-MOVE timeouts must be positive");
    }
    move_connect_timeout_ = connect;
    move_response_timeout_ = response;
    move_release_timeout_ = release;
}

// =============================================================================
// scp_service Interface Implementation
// =============================================================================

std::vector<std::string> retrieve_scp::supported_sop_classes() const {
    return {
        std::string(patient_root_move_sop_class_uid),
        std::string(study_root_move_sop_class_uid),
        std::string(patient_root_get_sop_class_uid),
        std::string(study_root_get_sop_class_uid)
    };
}

std::vector<std::string> retrieve_scp::outbound_sop_classes() const {
    return get_standard_storage_sop_classes();
}

void retrieve_scp::set_get_response_timeout(std::chrono::milliseconds timeout) {
    if (timeout.count() <= 0) {
        throw std::invalid_argument("C-GET response timeout must be positive");
    }
    get_response_timeout_ = timeout;
}

network::Result<std::monostate> retrieve_scp::handle_message(
    network::association& assoc,
    uint8_t context_id,
    const network::dimse::dimse_message& request) {

    using namespace network::dimse;

    // Route to appropriate handler based on command type
    switch (request.command()) {
        case command_field::c_move_rq:
            return handle_c_move(assoc, context_id, request);

        case command_field::c_get_rq:
            return handle_c_get(assoc, context_id, request);

        default:
            return kcenon::pacs::pacs_void_error(
                kcenon::pacs::error_codes::retrieve_unexpected_command,
                "Expected C-MOVE-RQ or C-GET-RQ but received " +
                std::string(to_string(request.command())));
    }
}

std::string_view retrieve_scp::service_name() const noexcept {
    return "Retrieve SCP";
}

// =============================================================================
// Statistics
// =============================================================================

size_t retrieve_scp::move_operations() const noexcept {
    return move_operations_.load();
}

size_t retrieve_scp::get_operations() const noexcept {
    return get_operations_.load();
}

size_t retrieve_scp::images_transferred() const noexcept {
    return images_transferred_.load();
}

void retrieve_scp::reset_statistics() noexcept {
    move_operations_ = 0;
    get_operations_ = 0;
    images_transferred_ = 0;
}

// =============================================================================
// Private Implementation - C-MOVE
// =============================================================================

network::Result<std::monostate> retrieve_scp::handle_c_move(
    network::association& assoc,
    uint8_t context_id,
    const network::dimse::dimse_message& request) {

    using namespace network::dimse;
    using namespace std::chrono;
    const auto sop_class_uid = request.affected_sop_class_uid();
    const auto message_id = request.message_id();
    ++move_operations_;
    assoc.register_cancellable_operation(context_id, message_id);
    struct operation_guard {
        network::association& assoc;
        uint8_t context;
        uint16_t message;
        bool active{true};
        void finish() {
            if (active) {
                assoc.finish_cancellable_operation(context, message);
                active = false;
            }
        }
        ~operation_guard() { finish(); }
    } operation{assoc, context_id, message_id};
    auto cancelled = [&] {
        return assoc.cancellation_requested(context_id, message_id) ||
               (cancel_check_ && cancel_check_());
    };
    auto refuse = [&](status_code status) {
        dimse_message response{command_field::c_move_rsp, 0};
        response.set_affected_sop_class_uid(sop_class_uid);
        response.set_message_id_responded_to(message_id);
        response.set_status(status);
        operation.finish();
        return assoc.send_dimse(context_id, response);
    };
    if (!request.has_dataset()) {
        return refuse(status_error_dataset_mismatch);
    }
    const auto dest_ae = get_move_destination(request);
    const auto dest_addr = destination_resolver_ && !dest_ae.empty()
        ? destination_resolver_(dest_ae) : std::nullopt;
    if (!dest_addr) {
        return refuse(status_refused_move_destination_unknown);
    }
    if (!retrieve_handler_) {
        return refuse(status_error_unable_to_process);
    }
    const auto& query_keys = request.dataset().value().get();
    std::vector<core::dicom_file> files;
    try {
        files = retrieve_handler_(query_keys);
    } catch (const std::exception&) {
        return refuse(status_error_unable_to_process);
    }
    // DIMSE counters are US. Never wrap a larger match set into a success.
    if (files.size() > (std::numeric_limits<uint16_t>::max)()) {
        return refuse(status_refused_out_of_resources_matches);
    }
    const std::string calling_ae{assoc.calling_ae()};
    const auto start_time = steady_clock::now();
    kcenon::common::get_event_bus().publish(
        kcenon::pacs::events::retrieve_started_event{
            kcenon::pacs::events::retrieve_operation::c_move, calling_ae, dest_ae,
            query_keys.get_string(core::tags::study_instance_uid),
            static_cast<uint16_t>(files.size())});

    sub_operation_stats stats;
    stats.remaining = static_cast<uint16_t>(files.size());
    std::vector<std::string> failed_uids;
    bool was_cancelled = false;
    using syntax_key = std::pair<std::string, std::string>;
    std::vector<syntax_key> syntaxes;
    std::vector<syntax_key> file_syntaxes;
    for (const auto& file : files) {
        syntax_key key{file.dataset().get_string(core::tags::sop_class_uid),
                       std::string(file.transfer_syntax().uid())};
        file_syntaxes.push_back(key);
        if (std::find(syntaxes.begin(), syntaxes.end(), key) == syntaxes.end()) {
            syntaxes.push_back(std::move(key));
        }
    }
    // At most 128 presentation contexts fit in one association. Reuse each
    // association for all matching instances, then negotiate the next batch.
    for (size_t first = 0; first < syntaxes.size(); first += 128) {
        if (cancelled() || !assoc.is_established()) {
            was_cancelled = cancelled();
            break;
        }
        network::association_config config;
        config.calling_ae_title = std::string(assoc.called_ae());
        config.called_ae_title = dest_ae;
        const auto end = (std::min)(first + 128, syntaxes.size());
        for (size_t i = first; i < end; ++i) {
            std::vector<std::string> transfers{syntaxes[i].second};
            if (syntaxes[i].second == "1.2.840.10008.1.2.1") {
                transfers.push_back("1.2.840.10008.1.2");
            } else if (syntaxes[i].second == "1.2.840.10008.1.2") {
                transfers.push_back("1.2.840.10008.1.2.1");
            }
            config.proposed_contexts.emplace_back(
                static_cast<uint8_t>((i-first)*2+1), syntaxes[i].first, std::move(transfers));
        }
        auto connected = network::association::connect(
            dest_addr->first, dest_addr->second, config, move_connect_timeout_);
        for (size_t i = 0; i < files.size(); ++i) {
            const auto key_it = std::find(syntaxes.begin()+first, syntaxes.begin()+end,
                                          file_syntaxes[i]);
            if (key_it == syntaxes.begin()+end) {
                continue;
            }
            if (cancelled() || !assoc.is_established()) {
                was_cancelled = cancelled();
                break;
            }
            auto pending = send_pending_response(assoc, context_id, message_id,
                                                  sop_class_uid, true, stats);
            if (pending.is_err()) {
                return pending;
            }
            const auto& file = files[i];
            const auto uid = file.dataset().get_string(core::tags::sop_instance_uid);
            const uint8_t store_context = static_cast<uint8_t>((key_it-syntaxes.begin()-first)*2+1);
            status_code store_status = status_error_unable_to_process;
            if (connected.is_ok() && connected.value().is_established()) {
                auto& destination = connected.value();
                const auto syntax = destination.context_transfer_syntax(store_context);
                // A rejected context has no transfer syntax; never borrow a
                // context for a different compressed representation.
                if (syntax.is_ok() && core::dicom_file::validate_pixel_data_representation(
                        file.dataset(), syntax.value()).is_ok()) {
                    if (store_handler_) {
                        store_status = store_handler_(destination, store_context, file,
                                                       calling_ae, message_id);
                    } else {
                        const auto store_id = static_cast<uint16_t>(i+1);
                        auto store = make_c_store_rq(store_id, file_syntaxes[i].first,
                                                     uid, request.priority());
                        store.command_set().set_string(tag_move_originator_aet,
                                                       encoding::vr_type::AE, calling_ae);
                        store.command_set().set_numeric<uint16_t>(tag_move_originator_message_id,
                                                                 encoding::vr_type::US, message_id);
                        store.set_dataset(file.dataset());
                        bool acknowledged = false;
                        auto sent = destination.send_dimse(store_context, store);
                        if (sent.is_ok()) {
                            const auto deadline = steady_clock::now() + move_response_timeout_;
                            while (steady_clock::now() < deadline && !cancelled() && assoc.is_established()) {
                                const auto left = duration_cast<milliseconds>(deadline-steady_clock::now());
                                auto received = destination.receive_dimse((std::min)(left, milliseconds{50}));
                                if (received.is_err()) {
                                    if (received.error().code == error_codes::receive_timeout) {
                                        continue;
                                    }
                                    break;
                                }
                                const auto& [response_context, response] = received.value();
                                if (response_context == store_context &&
                                    response.command() == command_field::c_store_rsp &&
                                    response.message_id_responded_to() == store_id &&
                                    response.command_set().contains(tag_status) &&
                                    (!response.command_set().contains(tag_affected_sop_class_uid) ||
                                     response.affected_sop_class_uid() == file_syntaxes[i].first) &&
                                    (!response.command_set().contains(tag_affected_sop_instance_uid) ||
                                     response.affected_sop_instance_uid() == uid) &&
                                    !response.has_dataset() && !is_pending(response.status())) {
                                    store_status = response.status();
                                    acknowledged = true;
                                } else {
                                    destination.abort();
                                }
                                break;
                            }
                        }
                        // No trustworthy response: abort so a late acknowledgement
                        // cannot be mistaken for the following sub-operation.
                        if (!acknowledged) {
                            destination.abort();
                        }
                    }
                }
            }
            --stats.remaining;
            if (network::dimse::is_success(store_status)) {
                ++stats.completed;
                ++images_transferred_;
            } else if (network::dimse::is_warning(store_status)) {
                ++stats.warning;
                ++images_transferred_;
            } else {
                ++stats.failed;
                failed_uids.push_back(uid);
            }
        }
        if (connected.is_ok() && connected.value().is_established()) {
            if (cancelled() || !assoc.is_established()) {
                connected.value().abort();
            } else if (connected.value().release(move_release_timeout_).is_err()) {
                connected.value().abort();
            }
        }
        if (cancelled() || !assoc.is_established()) {
            was_cancelled = cancelled();
            break;
        }
    }
    kcenon::common::get_event_bus().publish(
        kcenon::pacs::events::retrieve_completed_event{
            kcenon::pacs::events::retrieve_operation::c_move, calling_ae, dest_ae,
            stats.completed, stats.failed, stats.warning,
            static_cast<uint64_t>(duration_cast<milliseconds>(steady_clock::now()-start_time).count())});
    // Retire cancellation before exposing the terminal response; the requester
    // may immediately reuse its MessageID for the next operation.
    operation.finish();
    return send_final_response(assoc, context_id, message_id, sop_class_uid,
                               true, stats, was_cancelled, failed_uids);
}

// =============================================================================
// Private Implementation - C-GET
// =============================================================================

network::Result<std::monostate> retrieve_scp::handle_c_get(
    network::association& assoc,
    uint8_t context_id,
    const network::dimse::dimse_message& request) {

    using namespace network::dimse;
    using namespace std::chrono;
    const auto sop_class_uid = request.affected_sop_class_uid();
    const auto message_id = request.message_id();
    ++get_operations_;
    assoc.register_cancellable_operation(context_id, message_id);
    struct operation_guard {
        network::association& assoc;
        uint8_t context;
        uint16_t message;
        bool active{true};
        void finish() {
            if (active) {
                assoc.cancel_response_wait();
                assoc.finish_cancellable_operation(context, message);
                active = false;
            }
        }
        ~operation_guard() { finish(); }
    } operation{assoc, context_id, message_id};
    auto cancelled = [&] {
        return assoc.cancellation_requested(context_id, message_id) ||
               (cancel_check_ && cancel_check_());
    };
    auto refuse = [&](status_code status) {
        dimse_message response{command_field::c_get_rsp, 0};
        response.set_affected_sop_class_uid(sop_class_uid);
        response.set_message_id_responded_to(message_id);
        response.set_status(status);
        operation.finish();
        return assoc.send_dimse(context_id, response);
    };
    if (!request.has_dataset()) {
        return refuse(status_error_dataset_mismatch);
    }
    if (!retrieve_handler_) {
        return refuse(status_error_unable_to_process);
    }
    const auto& query_keys = request.dataset().value().get();
    std::vector<core::dicom_file> files;
    try {
        files = retrieve_handler_(query_keys);
    } catch (const std::exception&) {
        return refuse(status_error_unable_to_process);
    }
    if (files.size() > (std::numeric_limits<uint16_t>::max)()) {
        return refuse(status_refused_out_of_resources_matches);
    }
    const std::string calling_ae{assoc.calling_ae()};
    const auto start_time = steady_clock::now();
    kcenon::common::get_event_bus().publish(
        kcenon::pacs::events::retrieve_started_event{
            kcenon::pacs::events::retrieve_operation::c_get, calling_ae, "",
            query_keys.get_string(core::tags::study_instance_uid),
            static_cast<uint16_t>(files.size())});

    sub_operation_stats stats;
    stats.remaining = static_cast<uint16_t>(files.size());
    std::vector<std::string> failed_uids;
    bool was_cancelled = false;
    bool exchange_failed = false;
    for (const auto& file : files) {
        if (cancelled() || !assoc.is_established()) {
            was_cancelled = cancelled();
            break;
        }
        const auto uid = file.dataset().get_string(core::tags::sop_instance_uid);
        // Once the exchange failed, don't multiply a silent peer's timeout by
        // the number of matches or count an unacknowledged object as completed.
        if (exchange_failed) {
            --stats.remaining;
            ++stats.failed;
            failed_uids.push_back(uid);
            continue;
        }
        auto pending = send_pending_response(assoc, context_id, message_id,
                                              sop_class_uid, false, stats);
        if (pending.is_err()) {
            return pending;
        }
        const auto file_class = file.dataset().get_string(core::tags::sop_class_uid);
        const std::string file_syntax{file.transfer_syntax().uid()};
        std::optional<uint8_t> store_context;
        if (assoc.requestor_role(file_class).scp_role) {
            for (const auto& context : assoc.accepted_contexts()) {
                const auto& syntax = context.transfer_syntax;
                const bool native_pair =
                    (file_syntax == "1.2.840.10008.1.2" || file_syntax == "1.2.840.10008.1.2.1") &&
                    (syntax == "1.2.840.10008.1.2" || syntax == "1.2.840.10008.1.2.1");
                if (context.is_accepted() && context.abstract_syntax == file_class &&
                    (syntax == file_syntax || native_pair) &&
                    core::dicom_file::validate_pixel_data_representation(
                        file.dataset(), encoding::transfer_syntax{syntax}).is_ok()) {
                    store_context = context.id;
                    if (syntax == file_syntax) break;
                }
            }
        }
        status_code store_status = status_error_unable_to_process;
        if (store_context) {
            if (store_handler_) {
                // Custom transports retain responsibility for their own waits.
                store_status = store_handler_(assoc, *store_context, file, "", 0);
            } else {
                auto store_id = assoc.next_request_message_id();
                if (store_id.is_ok()) {
                    auto store = make_c_store_rq(store_id.value(), file_class, uid, request.priority());
                    store.set_dataset(file.dataset());
                    auto sent = assoc.send_request(*store_context, store);
                    bool acknowledged = false;
                    if (sent.is_ok()) {
                        const auto deadline = steady_clock::now() + get_response_timeout_;
                        while (steady_clock::now() < deadline && !cancelled() && assoc.is_established()) {
                            const auto left = duration_cast<milliseconds>(deadline-steady_clock::now());
                            auto received = assoc.receive_response((std::min)(left, milliseconds{50}));
                            if (received.is_err()) {
                                if (received.error().code == error_codes::receive_timeout) continue;
                                break;
                            }
                            store_status = received.value().status();
                            acknowledged = true;
                            break;
                        }
                    }
                    assoc.cancel_response_wait();
                    exchange_failed = !acknowledged;
                } else {
                    exchange_failed = true;
                }
            }
        }
        --stats.remaining;
        if (network::dimse::is_success(store_status)) {
            ++stats.completed;
            ++images_transferred_;
        } else if (network::dimse::is_warning(store_status)) {
            ++stats.warning;
            ++images_transferred_;
        } else {
            ++stats.failed;
            failed_uids.push_back(uid);
        }
    }
    // Cancellation after the last in-flight store still wins over normal final.
    was_cancelled = was_cancelled || cancelled();
    kcenon::common::get_event_bus().publish(
        kcenon::pacs::events::retrieve_completed_event{
            kcenon::pacs::events::retrieve_operation::c_get, calling_ae, "",
            stats.completed, stats.failed, stats.warning,
            static_cast<uint64_t>(duration_cast<milliseconds>(steady_clock::now()-start_time).count())});
    operation.finish();
    return send_final_response(assoc, context_id, message_id, sop_class_uid,
                               false, stats, was_cancelled, failed_uids);
}

// =============================================================================
// Private Implementation - Response Helpers
// =============================================================================

network::Result<std::monostate> retrieve_scp::send_pending_response(
    network::association& assoc,
    uint8_t context_id,
    uint16_t message_id,
    std::string_view sop_class_uid,
    bool is_move,
    const sub_operation_stats& stats) {

    using namespace network::dimse;

    // Create response message
    auto cmd = is_move ? command_field::c_move_rsp : command_field::c_get_rsp;
    dimse_message response{cmd, 0};

    response.set_affected_sop_class_uid(sop_class_uid);
    response.set_message_id_responded_to(message_id);
    response.set_status(status_pending);

    // Set sub-operation counts
    response.set_remaining_subops(stats.remaining);
    response.set_completed_subops(stats.completed);
    response.set_failed_subops(stats.failed);
    response.set_warning_subops(stats.warning);

    // Send the response
    return assoc.send_dimse(context_id, response);
}

network::Result<std::monostate> retrieve_scp::send_final_response(
    network::association& assoc,
    uint8_t context_id,
    uint16_t message_id,
    std::string_view sop_class_uid,
    bool is_move,
    const sub_operation_stats& stats,
    bool was_cancelled,
    const std::vector<std::string>& failed_uids) {

    using namespace network::dimse;

    // Create response message
    auto cmd = is_move ? command_field::c_move_rsp : command_field::c_get_rsp;
    dimse_message response{cmd, 0};

    response.set_affected_sop_class_uid(sop_class_uid);
    response.set_message_id_responded_to(message_id);

    // Determine final status
    status_code final_status;
    if (was_cancelled) {
        final_status = status_cancel;
    } else if (stats.failed > 0 && stats.completed == 0 && stats.warning == 0) {
        // All sub-operations failed
        final_status = status_refused_out_of_resources_subops;
    } else if (stats.failed > 0 || stats.warning > 0) {
        // Some sub-operations had issues
        final_status = status_warning_subops_complete_failures;
    } else {
        // All successful
        final_status = status_success;
    }

    response.set_status(final_status);

    // Remaining is permitted in Cancel, never in Success/Warning/Failure.
    if (was_cancelled) {
        response.set_remaining_subops(stats.remaining);
    }
    response.set_completed_subops(stats.completed);
    response.set_failed_subops(stats.failed);
    response.set_warning_subops(stats.warning);
    if (!failed_uids.empty()) {
        std::string joined;
        for (const auto& uid : failed_uids) {
            if (!joined.empty()) joined += "\\";
            joined += uid;
        }
        core::dicom_dataset identifier;
        identifier.set_string(core::dicom_tag{0x0008, 0x0058}, encoding::vr_type::UI, joined);
        response.set_dataset(std::move(identifier));
    }

    return assoc.send_dimse(context_id, response);
}

std::string retrieve_scp::get_move_destination(
    const network::dimse::dimse_message& request) const {

    // Move Destination is in the command set at tag (0000,0600)
    return request.command_set().get_string(network::dimse::tag_move_destination);
}

}  // namespace kcenon::pacs::services
