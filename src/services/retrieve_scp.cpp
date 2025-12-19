/**
 * @file retrieve_scp.cpp
 * @brief Implementation of the Retrieve SCP service (C-MOVE/C-GET)
 */

#include "pacs/services/retrieve_scp.hpp"

#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/core/events.hpp"
#include "pacs/core/result.hpp"
#include "pacs/network/dimse/command_field.hpp"
#include "pacs/network/dimse/status_codes.hpp"

#include <kcenon/common/patterns/event_bus.h>

namespace pacs::services {

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
            return pacs::pacs_void_error(
                pacs::error_codes::retrieve_unexpected_command,
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

    // Verify we have a retrieve handler
    if (!retrieve_handler_) {
        return pacs::pacs_void_error(
            pacs::error_codes::retrieve_handler_not_set,
            "No retrieve handler configured");
    }

    // Verify we have a destination resolver for C-MOVE
    if (!destination_resolver_) {
        return pacs::pacs_void_error(
            pacs::error_codes::retrieve_missing_destination,
            "No destination resolver configured for C-MOVE");
    }

    auto sop_class_uid = request.affected_sop_class_uid();
    auto message_id = request.message_id();

    // Verify we have a dataset (query keys)
    if (!request.has_dataset()) {
        sub_operation_stats stats;
        return send_final_response(
            assoc, context_id, message_id,
            sop_class_uid, true, stats, false);
    }

    // Get move destination AE title
    auto dest_ae = get_move_destination(request);
    if (dest_ae.empty()) {
        // Missing Move Destination - return error status
        ++move_operations_;

        dimse_message response{command_field::c_move_rsp, 0};
        response.set_affected_sop_class_uid(sop_class_uid);
        response.set_message_id_responded_to(message_id);
        response.set_status(status_refused_move_destination_unknown);

        return assoc.send_dimse(context_id, response);
    }

    // Resolve destination to network address
    auto dest_addr = destination_resolver_(dest_ae);
    if (!dest_addr.has_value()) {
        // Unknown destination AE - return error status
        ++move_operations_;

        dimse_message response{command_field::c_move_rsp, 0};
        response.set_affected_sop_class_uid(sop_class_uid);
        response.set_message_id_responded_to(message_id);
        response.set_status(status_refused_move_destination_unknown);

        return assoc.send_dimse(context_id, response);
    }

    // Get calling AE for move originator information
    std::string calling_ae{assoc.calling_ae()};

    // Retrieve matching files
    const auto& query_keys = request.dataset();
    auto files = retrieve_handler_(query_keys);
    auto start_time = std::chrono::steady_clock::now();

    // Get study UID for event
    std::string study_uid = query_keys.get_string(core::tags::study_instance_uid);

    // Publish retrieve started event
    kcenon::common::get_event_bus().publish(
        pacs::events::retrieve_started_event{
            pacs::events::retrieve_operation::c_move,
            calling_ae,
            dest_ae,
            study_uid,
            static_cast<uint16_t>(files.size())
        }
    );

    // Initialize sub-operation statistics
    sub_operation_stats stats;
    stats.remaining = static_cast<uint16_t>(files.size());
    bool was_cancelled = false;

    // Process each file (C-STORE sub-operations)
    for (const auto& file : files) {
        // Check for cancel request
        if (cancel_check_ && cancel_check_()) {
            was_cancelled = true;
            break;
        }

        // Send pending response with current progress
        auto pending_result = send_pending_response(
            assoc, context_id, message_id,
            sop_class_uid, true, stats);

        if (pending_result.is_err()) {
            return pending_result;
        }

        // Perform C-STORE sub-operation
        // Note: In a full implementation, this would establish a sub-association
        // to the destination and perform C-STORE. For now, we simulate success
        // or use the provided store handler.
        status_code store_status = status_success;

        if (store_handler_) {
            // Use custom store handler
            // The handler should establish connection to dest_addr and perform C-STORE
            // For now, we pass the current association (which isn't correct for C-MOVE)
            // A real implementation would create a separate association to destination
            store_status = store_handler_(
                assoc, context_id, file,
                calling_ae, message_id);
        }

        // Update statistics based on store result
        stats.remaining--;
        if (is_success(store_status)) {
            stats.completed++;
            ++images_transferred_;
        } else if (is_warning(store_status)) {
            stats.warning++;
            ++images_transferred_;
        } else {
            stats.failed++;
        }
    }

    // Update operation count
    ++move_operations_;

    // Calculate duration and publish completed event
    auto end_time = std::chrono::steady_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    kcenon::common::get_event_bus().publish(
        pacs::events::retrieve_completed_event{
            pacs::events::retrieve_operation::c_move,
            calling_ae,
            dest_ae,
            stats.completed,
            stats.failed,
            stats.warning,
            static_cast<uint64_t>(duration_ms)
        }
    );

    // Send final response
    return send_final_response(
        assoc, context_id, message_id,
        sop_class_uid, true, stats, was_cancelled);
}

// =============================================================================
// Private Implementation - C-GET
// =============================================================================

network::Result<std::monostate> retrieve_scp::handle_c_get(
    network::association& assoc,
    uint8_t context_id,
    const network::dimse::dimse_message& request) {

    using namespace network::dimse;

    // Verify we have a retrieve handler
    if (!retrieve_handler_) {
        return pacs::pacs_void_error(
            pacs::error_codes::retrieve_handler_not_set,
            "No retrieve handler configured");
    }

    auto sop_class_uid = request.affected_sop_class_uid();
    auto message_id = request.message_id();

    // Verify we have a dataset (query keys)
    if (!request.has_dataset()) {
        sub_operation_stats stats;
        return send_final_response(
            assoc, context_id, message_id,
            sop_class_uid, false, stats, false);
    }

    // Get calling AE for logging
    std::string calling_ae{assoc.calling_ae()};

    // Retrieve matching files
    const auto& query_keys = request.dataset();
    auto files = retrieve_handler_(query_keys);
    auto start_time = std::chrono::steady_clock::now();

    // Get study UID for event
    std::string study_uid = query_keys.get_string(core::tags::study_instance_uid);

    // Publish retrieve started event (C-GET has no destination AE)
    kcenon::common::get_event_bus().publish(
        pacs::events::retrieve_started_event{
            pacs::events::retrieve_operation::c_get,
            calling_ae,
            "",  // No destination for C-GET
            study_uid,
            static_cast<uint16_t>(files.size())
        }
    );

    // Initialize sub-operation statistics
    sub_operation_stats stats;
    stats.remaining = static_cast<uint16_t>(files.size());
    bool was_cancelled = false;

    // Process each file (C-STORE sub-operations on same association)
    for (const auto& file : files) {
        // Check for cancel request
        if (cancel_check_ && cancel_check_()) {
            was_cancelled = true;
            break;
        }

        // Send pending response with current progress
        auto pending_result = send_pending_response(
            assoc, context_id, message_id,
            sop_class_uid, false, stats);

        if (pending_result.is_err()) {
            return pending_result;
        }

        // Perform C-STORE sub-operation on the same association
        // For C-GET, images are sent back on the same association
        status_code store_status = status_success;

        if (store_handler_) {
            // Use custom store handler
            store_status = store_handler_(
                assoc, context_id, file,
                calling_ae, message_id);
        } else {
            // Default implementation: build and send C-STORE-RQ
            // Get the SOP Class and Instance UIDs from the file
            auto file_sop_class = file.sop_class_uid();
            auto file_sop_instance = file.sop_instance_uid();

            // Create C-STORE request
            dimse_message store_rq{command_field::c_store_rq, message_id};
            store_rq.set_affected_sop_class_uid(file_sop_class);
            store_rq.set_affected_sop_instance_uid(file_sop_instance);
            store_rq.set_priority(priority_medium);

            // For C-GET, include Move Originator information
            // (0000,1030) Move Originator Application Entity Title
            // (0000,1031) Move Originator Message ID
            store_rq.command_set().set_string(
                tag_move_originator_aet,
                encoding::vr_type::AE,
                calling_ae);
            store_rq.command_set().set_numeric<uint16_t>(
                tag_move_originator_message_id,
                encoding::vr_type::US,
                message_id);

            // Attach the dataset
            store_rq.set_dataset(file.dataset());

            // Find the presentation context for the SOP Class
            auto store_context_id = assoc.accepted_context_id(file_sop_class);
            if (!store_context_id.has_value()) {
                // No accepted context for this SOP Class
                stats.remaining--;
                stats.failed++;
                continue;
            }

            // Send the C-STORE request
            auto send_result = assoc.send_dimse(store_context_id.value(), store_rq);
            if (send_result.is_err()) {
                stats.remaining--;
                stats.failed++;
                continue;
            }

            // Wait for C-STORE response
            auto recv_result = assoc.receive_dimse();
            if (recv_result.is_err()) {
                stats.remaining--;
                stats.failed++;
                continue;
            }

            auto& [recv_ctx, store_rsp] = recv_result.value();
            store_status = store_rsp.status();
        }

        // Update statistics based on store result
        stats.remaining--;
        if (is_success(store_status)) {
            stats.completed++;
            ++images_transferred_;
        } else if (is_warning(store_status)) {
            stats.warning++;
            ++images_transferred_;
        } else {
            stats.failed++;
        }
    }

    // Update operation count
    ++get_operations_;

    // Calculate duration and publish completed event
    auto end_time = std::chrono::steady_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    kcenon::common::get_event_bus().publish(
        pacs::events::retrieve_completed_event{
            pacs::events::retrieve_operation::c_get,
            calling_ae,
            "",  // No destination for C-GET
            stats.completed,
            stats.failed,
            stats.warning,
            static_cast<uint64_t>(duration_ms)
        }
    );

    // Send final response
    return send_final_response(
        assoc, context_id, message_id,
        sop_class_uid, false, stats, was_cancelled);
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
    bool was_cancelled) {

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

    // Set final sub-operation counts
    response.set_remaining_subops(stats.remaining);
    response.set_completed_subops(stats.completed);
    response.set_failed_subops(stats.failed);
    response.set_warning_subops(stats.warning);

    // Send the response
    return assoc.send_dimse(context_id, response);
}

std::string retrieve_scp::get_move_destination(
    const network::dimse::dimse_message& request) const {

    // Move Destination is in the command set at tag (0000,0600)
    return request.command_set().get_string(network::dimse::tag_move_destination);
}

}  // namespace pacs::services
