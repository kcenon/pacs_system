/**
 * @file ups_watch_scp.cpp
 * @brief Implementation of the UPS Watch SCP service
 */

#include "pacs/services/ups/ups_watch_scp.hpp"

#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/core/result.hpp"
#include "pacs/encoding/vr_type.hpp"
#include "pacs/network/dimse/command_field.hpp"
#include "pacs/network/dimse/status_codes.hpp"
#include "pacs/services/ups/ups_push_scp.hpp"

namespace pacs::services {

// =============================================================================
// Construction
// =============================================================================

ups_watch_scp::ups_watch_scp(std::shared_ptr<di::ILogger> logger)
    : scp_service(std::move(logger)) {}

// =============================================================================
// Configuration
// =============================================================================

void ups_watch_scp::set_subscribe_handler(ups_subscribe_handler handler) {
    subscribe_handler_ = std::move(handler);
}

void ups_watch_scp::set_unsubscribe_handler(ups_unsubscribe_handler handler) {
    unsubscribe_handler_ = std::move(handler);
}

void ups_watch_scp::set_get_subscribers_handler(
    ups_get_subscribers_handler handler) {
    get_subscribers_handler_ = std::move(handler);
}

void ups_watch_scp::set_event_callback(ups_event_callback callback) {
    event_callback_ = std::move(callback);
}

// =============================================================================
// scp_service Interface Implementation
// =============================================================================

std::vector<std::string> ups_watch_scp::supported_sop_classes() const {
    return {
        std::string(ups_watch_sop_class_uid)
    };
}

network::Result<std::monostate> ups_watch_scp::handle_message(
    network::association& assoc,
    uint8_t context_id,
    const network::dimse::dimse_message& request) {

    using namespace network::dimse;

    if (request.command() != command_field::n_action_rq) {
        return pacs::pacs_void_error(
            pacs::error_codes::ups_unexpected_command,
            "Expected N-ACTION-RQ but received " +
            std::string(to_string(request.command())));
    }

    return handle_n_action(assoc, context_id, request);
}

std::string_view ups_watch_scp::service_name() const noexcept {
    return "UPS Watch SCP";
}

// =============================================================================
// Event Notification
// =============================================================================

void ups_watch_scp::notify_state_change(
    const std::string& workitem_uid,
    const std::string& new_state) {

    core::dicom_dataset event_info;
    event_info.set_string(
        ups_tags::procedure_step_state,
        encoding::vr_type::CS, new_state);

    dispatch_event(ups_event_state_report, workitem_uid, event_info);
}

void ups_watch_scp::notify_cancel_requested(
    const std::string& workitem_uid,
    const std::string& reason) {

    core::dicom_dataset event_info;
    if (!reason.empty()) {
        event_info.set_string(
            ups_tags::reason_for_cancellation,
            encoding::vr_type::LT, reason);
    }

    dispatch_event(ups_event_cancel_requested, workitem_uid, event_info);
}

void ups_watch_scp::notify_progress(
    const std::string& workitem_uid,
    int progress_percent) {

    core::dicom_dataset event_info;
    event_info.set_string(
        ups_tags::procedure_step_progress,
        encoding::vr_type::DS, std::to_string(progress_percent));

    dispatch_event(ups_event_progress_report, workitem_uid, event_info);
}

// =============================================================================
// Statistics
// =============================================================================

size_t ups_watch_scp::subscriptions_created() const noexcept {
    return subscriptions_created_.load();
}

size_t ups_watch_scp::subscriptions_removed() const noexcept {
    return subscriptions_removed_.load();
}

size_t ups_watch_scp::events_sent() const noexcept {
    return events_sent_.load();
}

void ups_watch_scp::reset_statistics() noexcept {
    subscriptions_created_ = 0;
    subscriptions_removed_ = 0;
    events_sent_ = 0;
}

// =============================================================================
// Private Implementation - N-ACTION Dispatch
// =============================================================================

network::Result<std::monostate> ups_watch_scp::handle_n_action(
    network::association& assoc,
    uint8_t context_id,
    const network::dimse::dimse_message& request) {

    using namespace network::dimse;

    // Get Action Type ID
    auto action_type = request.action_type_id();
    if (!action_type.has_value()) {
        return pacs::pacs_void_error(
            pacs::error_codes::ups_invalid_action_type,
            "Missing Action Type ID in N-ACTION request");
    }

    // Get SOP Instance UID (workitem UID or global subscription UID)
    auto sop_instance_uid = request.requested_sop_instance_uid();
    if (sop_instance_uid.empty()) {
        sop_instance_uid = request.affected_sop_instance_uid();
    }

    uint16_t action_id = action_type.value();
    uint16_t message_id = request.message_id();

    switch (action_id) {
        case ups_watch_action_subscribe:
            return handle_subscribe(
                assoc, context_id, message_id, sop_instance_uid);

        case ups_watch_action_unsubscribe:
            return handle_unsubscribe(
                assoc, context_id, message_id, sop_instance_uid);

        case ups_watch_action_suspend_global:
            return handle_suspend_global(
                assoc, context_id, message_id);

        default:
            return send_n_action_response(
                assoc, context_id, message_id,
                sop_instance_uid, action_id,
                status_error_no_such_action_type);
    }
}

// =============================================================================
// Private Implementation - Subscribe
// =============================================================================

network::Result<std::monostate> ups_watch_scp::handle_subscribe(
    network::association& assoc,
    uint8_t context_id,
    uint16_t message_id,
    const std::string& sop_instance_uid) {

    using namespace network::dimse;

    if (!subscribe_handler_) {
        return pacs::pacs_void_error(
            pacs::error_codes::ups_handler_not_set,
            "No subscribe handler configured for UPS Watch SCP");
    }

    // Determine subscriber AE from the association
    std::string subscriber_ae{assoc.calling_ae()};

    // Determine if this is a global or workitem-specific subscription
    std::string workitem_uid;
    if (sop_instance_uid != ups_global_subscription_instance_uid) {
        workitem_uid = sop_instance_uid;
    }

    // Deletion lock defaults to false (could be extracted from dataset)
    bool deletion_lock = false;

    logger_->debug("UPS Watch: Subscribe request from " + subscriber_ae +
                   (workitem_uid.empty() ? " (global)" :
                    " for workitem " + workitem_uid));

    auto result = subscribe_handler_(subscriber_ae, workitem_uid, deletion_lock);
    if (result.is_err()) {
        return send_n_action_response(
            assoc, context_id, message_id,
            sop_instance_uid, ups_watch_action_subscribe,
            status_error_unable_to_process);
    }

    ++subscriptions_created_;

    logger_->info("UPS Watch: Subscription created for " + subscriber_ae);

    return send_n_action_response(
        assoc, context_id, message_id,
        sop_instance_uid, ups_watch_action_subscribe,
        status_success);
}

// =============================================================================
// Private Implementation - Unsubscribe
// =============================================================================

network::Result<std::monostate> ups_watch_scp::handle_unsubscribe(
    network::association& assoc,
    uint8_t context_id,
    uint16_t message_id,
    const std::string& sop_instance_uid) {

    using namespace network::dimse;

    if (!unsubscribe_handler_) {
        return pacs::pacs_void_error(
            pacs::error_codes::ups_handler_not_set,
            "No unsubscribe handler configured for UPS Watch SCP");
    }

    std::string subscriber_ae{assoc.calling_ae()};

    // Determine if this is a global or workitem-specific unsubscription
    std::string workitem_uid;
    if (sop_instance_uid != ups_global_subscription_instance_uid) {
        workitem_uid = sop_instance_uid;
    }

    logger_->debug("UPS Watch: Unsubscribe request from " + subscriber_ae +
                   (workitem_uid.empty() ? " (global)" :
                    " for workitem " + workitem_uid));

    auto result = unsubscribe_handler_(subscriber_ae, workitem_uid);
    if (result.is_err()) {
        return send_n_action_response(
            assoc, context_id, message_id,
            sop_instance_uid, ups_watch_action_unsubscribe,
            status_error_unable_to_process);
    }

    ++subscriptions_removed_;

    logger_->info("UPS Watch: Subscription removed for " + subscriber_ae);

    return send_n_action_response(
        assoc, context_id, message_id,
        sop_instance_uid, ups_watch_action_unsubscribe,
        status_success);
}

// =============================================================================
// Private Implementation - Suspend Global
// =============================================================================

network::Result<std::monostate> ups_watch_scp::handle_suspend_global(
    network::association& assoc,
    uint8_t context_id,
    uint16_t message_id) {

    using namespace network::dimse;

    if (!unsubscribe_handler_) {
        return pacs::pacs_void_error(
            pacs::error_codes::ups_handler_not_set,
            "No unsubscribe handler configured for UPS Watch SCP");
    }

    std::string subscriber_ae{assoc.calling_ae()};

    logger_->debug("UPS Watch: Suspend global subscription from " +
                   subscriber_ae);

    // Suspend global = unsubscribe from global (empty workitem_uid)
    auto result = unsubscribe_handler_(subscriber_ae, "");
    if (result.is_err()) {
        return send_n_action_response(
            assoc, context_id, message_id,
            std::string(ups_global_subscription_instance_uid),
            ups_watch_action_suspend_global,
            status_error_unable_to_process);
    }

    ++subscriptions_removed_;

    logger_->info("UPS Watch: Global subscription suspended for " +
                  subscriber_ae);

    return send_n_action_response(
        assoc, context_id, message_id,
        std::string(ups_global_subscription_instance_uid),
        ups_watch_action_suspend_global,
        status_success);
}

// =============================================================================
// Private Implementation - Response Helper
// =============================================================================

network::Result<std::monostate> ups_watch_scp::send_n_action_response(
    network::association& assoc,
    uint8_t context_id,
    uint16_t message_id,
    const std::string& sop_instance_uid,
    uint16_t action_type_id,
    network::dimse::status_code status) {

    using namespace network::dimse;

    auto response = make_n_action_rsp(
        message_id,
        ups_watch_sop_class_uid,
        sop_instance_uid,
        action_type_id,
        status);

    return assoc.send_dimse(context_id, response);
}

// =============================================================================
// Private Implementation - Event Dispatch
// =============================================================================

void ups_watch_scp::dispatch_event(
    uint16_t event_type_id,
    const std::string& workitem_uid,
    const core::dicom_dataset& event_info) {

    if (!get_subscribers_handler_ || !event_callback_) {
        return;
    }

    // Get subscribers for this workitem
    auto subscribers_result = get_subscribers_handler_(workitem_uid);
    if (subscribers_result.is_err()) {
        logger_->warn("UPS Watch: Failed to get subscribers for " +
                         workitem_uid);
        return;
    }

    const auto& subscribers = subscribers_result.value();

    for (const auto& subscriber_ae : subscribers) {
        event_callback_(subscriber_ae, event_type_id,
                        workitem_uid, event_info);
        ++events_sent_;
    }

    if (!subscribers.empty()) {
        logger_->info("UPS Watch: Dispatched event type " +
                      std::to_string(event_type_id) + " to " +
                      std::to_string(subscribers.size()) +
                      " subscribers for workitem " + workitem_uid);
    }
}

}  // namespace pacs::services
