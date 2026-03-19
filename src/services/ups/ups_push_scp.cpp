/**
 * @file ups_push_scp.cpp
 * @brief Implementation of the UPS (Unified Procedure Step) Push SCP service
 */

#include "pacs/services/ups/ups_push_scp.hpp"

#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/core/result.hpp"
#include "pacs/encoding/vr_type.hpp"
#include "pacs/network/dimse/command_field.hpp"
#include "pacs/network/dimse/dimse_message.hpp"
#include "pacs/network/dimse/status_codes.hpp"

namespace kcenon::pacs::services {

// =============================================================================
// Construction
// =============================================================================

ups_push_scp::ups_push_scp(std::shared_ptr<di::ILogger> logger)
    : scp_service(std::move(logger)) {}

// =============================================================================
// Configuration
// =============================================================================

void ups_push_scp::set_create_handler(ups_create_handler handler) {
    create_handler_ = std::move(handler);
}

void ups_push_scp::set_set_handler(ups_set_handler handler) {
    set_handler_ = std::move(handler);
}

void ups_push_scp::set_get_handler(ups_get_handler handler) {
    get_handler_ = std::move(handler);
}

void ups_push_scp::set_change_state_handler(ups_change_state_handler handler) {
    change_state_handler_ = std::move(handler);
}

void ups_push_scp::set_request_cancel_handler(ups_request_cancel_handler handler) {
    request_cancel_handler_ = std::move(handler);
}

// =============================================================================
// scp_service Interface Implementation
// =============================================================================

std::vector<std::string> ups_push_scp::supported_sop_classes() const {
    return {std::string(ups_push_sop_class_uid)};
}

network::Result<std::monostate> ups_push_scp::handle_message(
    network::association& assoc,
    uint8_t context_id,
    const network::dimse::dimse_message& request) {

    using namespace network::dimse;

    switch (request.command()) {
        case command_field::n_create_rq:
            return handle_n_create(assoc, context_id, request);

        case command_field::n_set_rq:
            return handle_n_set(assoc, context_id, request);

        case command_field::n_get_rq:
            return handle_n_get(assoc, context_id, request);

        case command_field::n_action_rq:
            return handle_n_action(assoc, context_id, request);

        default:
            return kcenon::pacs::pacs_void_error(
                kcenon::pacs::error_codes::ups_unexpected_command,
                "Unexpected command for UPS Push SCP: " +
                std::string(to_string(request.command())));
    }
}

std::string_view ups_push_scp::service_name() const noexcept {
    return "UPS Push SCP";
}

// =============================================================================
// Statistics
// =============================================================================

size_t ups_push_scp::creates_processed() const noexcept {
    return creates_processed_.load();
}

size_t ups_push_scp::sets_processed() const noexcept {
    return sets_processed_.load();
}

size_t ups_push_scp::gets_processed() const noexcept {
    return gets_processed_.load();
}

size_t ups_push_scp::actions_processed() const noexcept {
    return actions_processed_.load();
}

size_t ups_push_scp::state_changes() const noexcept {
    return state_changes_.load();
}

size_t ups_push_scp::cancel_requests() const noexcept {
    return cancel_requests_.load();
}

void ups_push_scp::reset_statistics() noexcept {
    creates_processed_ = 0;
    sets_processed_ = 0;
    gets_processed_ = 0;
    actions_processed_ = 0;
    state_changes_ = 0;
    cancel_requests_ = 0;
}

// =============================================================================
// Private Implementation - N-CREATE Handler
// =============================================================================

network::Result<std::monostate> ups_push_scp::handle_n_create(
    network::association& assoc,
    uint8_t context_id,
    const network::dimse::dimse_message& request) {

    using namespace network::dimse;

    // Verify we have a handler configured
    if (!create_handler_) {
        return kcenon::pacs::pacs_void_error(
            kcenon::pacs::error_codes::ups_handler_not_set,
            "No N-CREATE handler configured for UPS Push SCP");
    }

    // Verify the SOP Class is UPS Push
    auto sop_class_uid = request.affected_sop_class_uid();
    if (sop_class_uid != ups_push_sop_class_uid) {
        return send_n_create_response(
            assoc, context_id, request.message_id(),
            "", status_refused_sop_class_not_supported);
    }

    // Get the SOP Instance UID (workitem UID)
    auto sop_instance_uid = request.affected_sop_instance_uid();
    if (sop_instance_uid.empty()) {
        return send_n_create_response(
            assoc, context_id, request.message_id(),
            "", status_error_missing_attribute);
    }

    // Verify we have a dataset
    if (!request.has_dataset()) {
        return send_n_create_response(
            assoc, context_id, request.message_id(),
            sop_instance_uid, status_error_cannot_understand);
    }

    const auto& dataset = request.dataset().value().get();

    // Validate initial state is SCHEDULED (if present in dataset)
    if (dataset.contains(ups_tags::procedure_step_state)) {
        auto state_str = dataset.get_string(ups_tags::procedure_step_state);
        auto parsed_state = storage::parse_ups_state(state_str);

        if (!parsed_state.has_value() ||
            parsed_state.value() != storage::ups_state::scheduled) {
            return send_n_create_response(
                assoc, context_id, request.message_id(),
                sop_instance_uid, status_error_cannot_understand);
        }
    }

    // Build UPS workitem from request data
    storage::ups_workitem workitem;
    workitem.workitem_uid = sop_instance_uid;
    workitem.state = "SCHEDULED";

    // Extract optional fields from dataset
    if (dataset.contains(ups_tags::procedure_step_label)) {
        workitem.procedure_step_label =
            dataset.get_string(ups_tags::procedure_step_label);
    }
    if (dataset.contains(ups_tags::worklist_label)) {
        workitem.worklist_label =
            dataset.get_string(ups_tags::worklist_label);
    }
    if (dataset.contains(ups_tags::scheduled_procedure_step_priority)) {
        workitem.priority =
            dataset.get_string(ups_tags::scheduled_procedure_step_priority);
    }

    // Call the handler to create the workitem
    auto result = create_handler_(workitem);
    if (result.is_err()) {
        return send_n_create_response(
            assoc, context_id, request.message_id(),
            sop_instance_uid, status_error_unable_to_process);
    }

    ++creates_processed_;

    return send_n_create_response(
        assoc, context_id, request.message_id(),
        sop_instance_uid, status_success);
}

// =============================================================================
// Private Implementation - N-SET Handler
// =============================================================================

network::Result<std::monostate> ups_push_scp::handle_n_set(
    network::association& assoc,
    uint8_t context_id,
    const network::dimse::dimse_message& request) {

    using namespace network::dimse;

    // Verify we have a handler configured
    if (!set_handler_) {
        return kcenon::pacs::pacs_void_error(
            kcenon::pacs::error_codes::ups_handler_not_set,
            "No N-SET handler configured for UPS Push SCP");
    }

    // Get SOP Instance UID (from Requested SOP Instance UID for N-SET)
    auto sop_instance_uid = request.command_set().get_string(
        tag_requested_sop_instance_uid);

    if (sop_instance_uid.empty()) {
        sop_instance_uid = request.affected_sop_instance_uid();
    }

    if (sop_instance_uid.empty()) {
        return send_n_set_response(
            assoc, context_id, request.message_id(),
            "", status_error_missing_attribute);
    }

    // Verify we have a dataset with modifications
    if (!request.has_dataset()) {
        return send_n_set_response(
            assoc, context_id, request.message_id(),
            sop_instance_uid, status_error_cannot_understand);
    }

    const auto& dataset = request.dataset().value().get();

    // Reject if dataset tries to set a final state directly via N-SET
    // (state changes must go through N-ACTION)
    if (dataset.contains(ups_tags::procedure_step_state)) {
        auto state_str = dataset.get_string(ups_tags::procedure_step_state);
        auto parsed_state = storage::parse_ups_state(state_str);
        if (parsed_state.has_value() &&
            (parsed_state.value() == storage::ups_state::completed ||
             parsed_state.value() == storage::ups_state::canceled)) {
            return send_n_set_response(
                assoc, context_id, request.message_id(),
                sop_instance_uid, status_error_cannot_understand);
        }
    }

    // Call the handler to update the workitem
    auto result = set_handler_(sop_instance_uid, dataset);
    if (result.is_err()) {
        return send_n_set_response(
            assoc, context_id, request.message_id(),
            sop_instance_uid, status_error_unable_to_process);
    }

    ++sets_processed_;

    return send_n_set_response(
        assoc, context_id, request.message_id(),
        sop_instance_uid, status_success);
}

// =============================================================================
// Private Implementation - N-GET Handler
// =============================================================================

network::Result<std::monostate> ups_push_scp::handle_n_get(
    network::association& assoc,
    uint8_t context_id,
    const network::dimse::dimse_message& request) {

    using namespace network::dimse;

    // Verify we have a handler configured
    if (!get_handler_) {
        return kcenon::pacs::pacs_void_error(
            kcenon::pacs::error_codes::ups_handler_not_set,
            "No N-GET handler configured for UPS Push SCP");
    }

    // Extract Requested SOP Instance UID
    auto sop_instance_uid = request.requested_sop_instance_uid();
    if (sop_instance_uid.empty()) {
        return send_n_get_response(
            assoc, context_id, request.message_id(),
            "", status_error_missing_attribute);
    }

    // Call the handler to retrieve the workitem
    auto result = get_handler_(sop_instance_uid);
    if (result.is_err()) {
        return send_n_get_response(
            assoc, context_id, request.message_id(),
            sop_instance_uid, status_error_invalid_object_instance);
    }

    ++gets_processed_;

    // Build response dataset from workitem
    const auto& workitem = result.value();
    core::dicom_dataset response_dataset;

    response_dataset.set_string(
        ups_tags::procedure_step_state,
        encoding::vr_type::CS, workitem.state);

    if (!workitem.procedure_step_label.empty()) {
        response_dataset.set_string(
            ups_tags::procedure_step_label,
            encoding::vr_type::LO, workitem.procedure_step_label);
    }
    if (!workitem.worklist_label.empty()) {
        response_dataset.set_string(
            ups_tags::worklist_label,
            encoding::vr_type::LO, workitem.worklist_label);
    }
    if (!workitem.priority.empty()) {
        response_dataset.set_string(
            ups_tags::scheduled_procedure_step_priority,
            encoding::vr_type::CS, workitem.priority);
    }
    if (!workitem.progress_description.empty()) {
        response_dataset.set_string(
            ups_tags::procedure_step_progress,
            encoding::vr_type::DS,
            std::to_string(workitem.progress_percent));
    }
    if (!workitem.transaction_uid.empty()) {
        response_dataset.set_string(
            ups_tags::transaction_uid,
            encoding::vr_type::UI, workitem.transaction_uid);
    }

    return send_n_get_response(
        assoc, context_id, request.message_id(),
        sop_instance_uid, status_success, &response_dataset);
}

// =============================================================================
// Private Implementation - N-ACTION Handler
// =============================================================================

network::Result<std::monostate> ups_push_scp::handle_n_action(
    network::association& assoc,
    uint8_t context_id,
    const network::dimse::dimse_message& request) {

    using namespace network::dimse;

    // Get Action Type ID
    auto action_type = request.action_type_id();
    if (!action_type.has_value()) {
        return kcenon::pacs::pacs_void_error(
            kcenon::pacs::error_codes::ups_invalid_action_type,
            "Missing Action Type ID in N-ACTION request");
    }

    // Get SOP Instance UID
    auto sop_instance_uid = request.requested_sop_instance_uid();
    if (sop_instance_uid.empty()) {
        sop_instance_uid = request.affected_sop_instance_uid();
    }

    if (sop_instance_uid.empty()) {
        return send_n_action_response(
            assoc, context_id, request.message_id(),
            "", action_type.value(), status_error_missing_attribute);
    }

    uint16_t action_id = action_type.value();

    if (action_id == ups_action_change_state) {
        // N-ACTION Type 1: Change UPS State
        if (!change_state_handler_) {
            return kcenon::pacs::pacs_void_error(
                kcenon::pacs::error_codes::ups_handler_not_set,
                "No Change State handler configured for UPS Push SCP");
        }

        if (!request.has_dataset()) {
            return send_n_action_response(
                assoc, context_id, request.message_id(),
                sop_instance_uid, action_id, status_error_cannot_understand);
        }

        const auto& dataset = request.dataset().value().get();

        // Extract new state
        if (!dataset.contains(ups_tags::procedure_step_state)) {
            return send_n_action_response(
                assoc, context_id, request.message_id(),
                sop_instance_uid, action_id, status_error_missing_attribute);
        }

        auto new_state = dataset.get_string(ups_tags::procedure_step_state);

        // Validate the state value
        auto parsed_state = storage::parse_ups_state(new_state);
        if (!parsed_state.has_value()) {
            return send_n_action_response(
                assoc, context_id, request.message_id(),
                sop_instance_uid, action_id, status_error_cannot_understand);
        }

        // Extract Transaction UID (required when transitioning to IN PROGRESS)
        std::string txn_uid;
        if (dataset.contains(ups_tags::transaction_uid)) {
            txn_uid = dataset.get_string(ups_tags::transaction_uid);
        }

        if (parsed_state.value() == storage::ups_state::in_progress &&
            txn_uid.empty()) {
            return send_n_action_response(
                assoc, context_id, request.message_id(),
                sop_instance_uid, action_id, status_error_missing_attribute);
        }

        auto result = change_state_handler_(sop_instance_uid, new_state, txn_uid);
        if (result.is_err()) {
            return send_n_action_response(
                assoc, context_id, request.message_id(),
                sop_instance_uid, action_id, status_error_unable_to_process);
        }

        ++actions_processed_;
        ++state_changes_;

        return send_n_action_response(
            assoc, context_id, request.message_id(),
            sop_instance_uid, action_id, status_success);

    } else if (action_id == ups_action_request_cancel) {
        // N-ACTION Type 3: Request Cancellation
        if (!request_cancel_handler_) {
            return kcenon::pacs::pacs_void_error(
                kcenon::pacs::error_codes::ups_handler_not_set,
                "No Request Cancel handler configured for UPS Push SCP");
        }

        // Extract cancellation reason (optional)
        std::string reason;
        if (request.has_dataset()) {
            const auto& dataset = request.dataset().value().get();
            if (dataset.contains(ups_tags::reason_for_cancellation)) {
                reason = dataset.get_string(ups_tags::reason_for_cancellation);
            }
        }

        auto result = request_cancel_handler_(sop_instance_uid, reason);
        if (result.is_err()) {
            return send_n_action_response(
                assoc, context_id, request.message_id(),
                sop_instance_uid, action_id, status_error_unable_to_process);
        }

        ++actions_processed_;
        ++cancel_requests_;

        return send_n_action_response(
            assoc, context_id, request.message_id(),
            sop_instance_uid, action_id, status_success);

    } else {
        // Unknown action type
        return send_n_action_response(
            assoc, context_id, request.message_id(),
            sop_instance_uid, action_id, status_error_no_such_action_type);
    }
}

// =============================================================================
// Private Implementation - Response Helpers
// =============================================================================

network::Result<std::monostate> ups_push_scp::send_n_create_response(
    network::association& assoc,
    uint8_t context_id,
    uint16_t message_id,
    const std::string& sop_instance_uid,
    network::dimse::status_code status) {

    using namespace network::dimse;

    dimse_message response{command_field::n_create_rsp, 0};
    response.set_message_id_responded_to(message_id);
    response.set_affected_sop_class_uid(ups_push_sop_class_uid);
    response.set_status(status);

    if (!sop_instance_uid.empty()) {
        response.set_affected_sop_instance_uid(sop_instance_uid);
    }

    return assoc.send_dimse(context_id, response);
}

network::Result<std::monostate> ups_push_scp::send_n_set_response(
    network::association& assoc,
    uint8_t context_id,
    uint16_t message_id,
    const std::string& sop_instance_uid,
    network::dimse::status_code status) {

    using namespace network::dimse;

    dimse_message response{command_field::n_set_rsp, 0};
    response.set_message_id_responded_to(message_id);
    response.set_affected_sop_class_uid(ups_push_sop_class_uid);
    response.set_status(status);

    if (!sop_instance_uid.empty()) {
        response.set_affected_sop_instance_uid(sop_instance_uid);
    }

    return assoc.send_dimse(context_id, response);
}

network::Result<std::monostate> ups_push_scp::send_n_get_response(
    network::association& assoc,
    uint8_t context_id,
    uint16_t message_id,
    const std::string& sop_instance_uid,
    network::dimse::status_code status,
    core::dicom_dataset* dataset) {

    using namespace network::dimse;

    auto response = make_n_get_rsp(
        message_id, ups_push_sop_class_uid, sop_instance_uid, status);

    if (dataset != nullptr) {
        response.set_dataset(std::move(*dataset));
    }

    return assoc.send_dimse(context_id, response);
}

network::Result<std::monostate> ups_push_scp::send_n_action_response(
    network::association& assoc,
    uint8_t context_id,
    uint16_t message_id,
    const std::string& sop_instance_uid,
    uint16_t action_type_id,
    network::dimse::status_code status) {

    using namespace network::dimse;

    auto response = make_n_action_rsp(
        message_id, ups_push_sop_class_uid, sop_instance_uid,
        action_type_id, status);

    return assoc.send_dimse(context_id, response);
}

}  // namespace kcenon::pacs::services
