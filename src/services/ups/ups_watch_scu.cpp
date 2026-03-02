/**
 * @file ups_watch_scu.cpp
 * @brief Implementation of the UPS Watch SCU service
 */

#include "pacs/services/ups/ups_watch_scu.hpp"

#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/core/result.hpp"
#include "pacs/encoding/vr_type.hpp"
#include "pacs/network/dimse/command_field.hpp"
#include "pacs/network/dimse/status_codes.hpp"

#include <chrono>

namespace pacs::services {

// =============================================================================
// Construction
// =============================================================================

ups_watch_scu::ups_watch_scu(std::shared_ptr<di::ILogger> logger)
    : logger_(logger ? std::move(logger) : di::null_logger()) {}

ups_watch_scu::ups_watch_scu(const ups_watch_scu_config& config,
                             std::shared_ptr<di::ILogger> logger)
    : logger_(logger ? std::move(logger) : di::null_logger()),
      config_(config) {}

// =============================================================================
// Callback Configuration
// =============================================================================

void ups_watch_scu::set_event_received_callback(event_received_callback cb) {
    event_callback_ = std::move(cb);
}

// =============================================================================
// N-ACTION Operations
// =============================================================================

network::Result<ups_result> ups_watch_scu::subscribe(
    network::association& assoc,
    const ups_subscribe_data& data) {

    // Determine the SOP Instance UID: workitem-specific or global
    std::string sop_instance_uid = data.workitem_uid.empty()
        ? std::string(ups_global_subscription_instance_uid)
        : data.workitem_uid;

    logger_->debug("UPS Watch SCU: Subscribe request" +
                   (data.workitem_uid.empty() ? std::string(" (global)") :
                    " for workitem " + data.workitem_uid));

    auto result = send_watch_action(
        assoc, sop_instance_uid, ups_watch_action_subscribe);

    if (result.is_ok() && result.value().is_success()) {
        subscribes_performed_.fetch_add(1, std::memory_order_relaxed);
        logger_->info("UPS Watch SCU: Subscribe successful" +
                      (data.workitem_uid.empty() ? std::string(" (global)") :
                       " for workitem " + data.workitem_uid));
    }

    return result;
}

network::Result<ups_result> ups_watch_scu::unsubscribe(
    network::association& assoc,
    const ups_unsubscribe_data& data) {

    std::string sop_instance_uid = data.workitem_uid.empty()
        ? std::string(ups_global_subscription_instance_uid)
        : data.workitem_uid;

    logger_->debug("UPS Watch SCU: Unsubscribe request" +
                   (data.workitem_uid.empty() ? std::string(" (global)") :
                    " for workitem " + data.workitem_uid));

    auto result = send_watch_action(
        assoc, sop_instance_uid, ups_watch_action_unsubscribe);

    if (result.is_ok() && result.value().is_success()) {
        unsubscribes_performed_.fetch_add(1, std::memory_order_relaxed);
        logger_->info("UPS Watch SCU: Unsubscribe successful" +
                      (data.workitem_uid.empty() ? std::string(" (global)") :
                       " for workitem " + data.workitem_uid));
    }

    return result;
}

network::Result<ups_result> ups_watch_scu::suspend_global(
    network::association& assoc) {

    logger_->debug("UPS Watch SCU: Suspend global subscription");

    auto result = send_watch_action(
        assoc,
        std::string(ups_global_subscription_instance_uid),
        ups_watch_action_suspend_global);

    if (result.is_ok() && result.value().is_success()) {
        unsubscribes_performed_.fetch_add(1, std::memory_order_relaxed);
        logger_->info("UPS Watch SCU: Global subscription suspended");
    }

    return result;
}

// =============================================================================
// N-EVENT-REPORT Handler
// =============================================================================

network::Result<ups_watch_event> ups_watch_scu::handle_event_report(
    const network::dimse::dimse_message& event_rq) {

    using namespace network::dimse;

    // Verify command is N-EVENT-REPORT-RQ
    if (event_rq.command() != command_field::n_event_report_rq) {
        return pacs::pacs_error<ups_watch_event>(
            pacs::error_codes::ups_unexpected_command,
            "Expected N-EVENT-REPORT-RQ, got: " +
            std::string(to_string(event_rq.command())));
    }

    // Get event type ID
    auto event_type = event_rq.event_type_id();
    if (!event_type.has_value()) {
        return pacs::pacs_error<ups_watch_event>(
            pacs::error_codes::ups_invalid_action_type,
            "Missing Event Type ID in N-EVENT-REPORT");
    }

    // Get workitem UID from the affected SOP Instance UID
    std::string workitem_uid{event_rq.affected_sop_instance_uid()};

    // Parse dataset if present
    ups_watch_event event;
    if (event_rq.has_dataset()) {
        auto dataset_result = event_rq.dataset();
        if (dataset_result.is_ok()) {
            event = parse_event_dataset(
                event_type.value(), workitem_uid,
                dataset_result.value().get());
        } else {
            event.event_type_id = event_type.value();
            event.workitem_uid = workitem_uid;
            event.timestamp = std::chrono::system_clock::now();
        }
    } else {
        event.event_type_id = event_type.value();
        event.workitem_uid = workitem_uid;
        event.timestamp = std::chrono::system_clock::now();
    }

    events_received_.fetch_add(1, std::memory_order_relaxed);

    logger_->info("UPS Watch SCU: Received event type " +
                  std::to_string(event.event_type_id) +
                  " for workitem " + event.workitem_uid);

    // Invoke callback if registered
    if (event_callback_) {
        event_callback_(event);
    }

    return pacs::Result<ups_watch_event>::ok(std::move(event));
}

// =============================================================================
// Statistics
// =============================================================================

size_t ups_watch_scu::subscribes_performed() const noexcept {
    return subscribes_performed_.load(std::memory_order_relaxed);
}

size_t ups_watch_scu::unsubscribes_performed() const noexcept {
    return unsubscribes_performed_.load(std::memory_order_relaxed);
}

size_t ups_watch_scu::events_received() const noexcept {
    return events_received_.load(std::memory_order_relaxed);
}

void ups_watch_scu::reset_statistics() noexcept {
    subscribes_performed_.store(0, std::memory_order_relaxed);
    unsubscribes_performed_.store(0, std::memory_order_relaxed);
    events_received_.store(0, std::memory_order_relaxed);
}

// =============================================================================
// Private Implementation - Send Watch Action
// =============================================================================

network::Result<ups_result> ups_watch_scu::send_watch_action(
    network::association& assoc,
    const std::string& sop_instance_uid,
    uint16_t action_type_id) {

    using namespace network::dimse;

    auto start_time = std::chrono::steady_clock::now();

    // Verify association is established
    if (!assoc.is_established()) {
        return pacs::pacs_error<ups_result>(
            pacs::error_codes::association_not_established,
            "Association not established");
    }

    // Get accepted presentation context for UPS Watch
    auto context_id = assoc.accepted_context_id(ups_watch_sop_class_uid);
    if (!context_id) {
        return pacs::pacs_error<ups_result>(
            pacs::error_codes::ups_context_not_accepted,
            "No accepted presentation context for UPS Watch SOP Class");
    }

    // Build the N-ACTION request
    auto request = make_n_action_rq(
        next_message_id(),
        ups_watch_sop_class_uid,
        sop_instance_uid,
        action_type_id);

    // Send the request
    auto send_result = assoc.send_dimse(*context_id, request);
    if (send_result.is_err()) {
        logger_->error("UPS Watch SCU: Failed to send N-ACTION: " +
                       send_result.error().message);
        return send_result.error();
    }

    // Receive the response
    auto recv_result = assoc.receive_dimse(config_.timeout);
    if (recv_result.is_err()) {
        logger_->error("UPS Watch SCU: Failed to receive N-ACTION response: " +
                       recv_result.error().message);
        return recv_result.error();
    }

    const auto& [recv_context_id, response] = recv_result.value();

    // Verify it's an N-ACTION response
    if (response.command() != command_field::n_action_rsp) {
        return pacs::pacs_error<ups_result>(
            pacs::error_codes::ups_unexpected_command,
            "Expected N-ACTION-RSP but received " +
            std::string(to_string(response.command())));
    }

    auto end_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    // Build result
    ups_result result;
    result.workitem_uid = sop_instance_uid;
    result.status = static_cast<uint16_t>(response.status());
    result.elapsed = elapsed;

    if (response.command_set().contains(tag_error_comment)) {
        result.error_comment = response.command_set().get_string(tag_error_comment);
    }

    if (!result.is_success()) {
        logger_->warn("UPS Watch SCU: N-ACTION returned status 0x" +
                      std::to_string(result.status));
    }

    return result;
}

// =============================================================================
// Private Implementation - Event Parsing
// =============================================================================

ups_watch_event ups_watch_scu::parse_event_dataset(
    uint16_t event_type_id,
    const std::string& workitem_uid,
    const core::dicom_dataset& dataset) {

    ups_watch_event event;
    event.event_type_id = event_type_id;
    event.workitem_uid = workitem_uid;
    event.event_info = dataset;
    event.timestamp = std::chrono::system_clock::now();

    // Extract fields based on event type
    switch (event_type_id) {
        case ups_event_state_report:
            event.procedure_step_state = dataset.get_string(
                ups_tags::procedure_step_state);
            break;

        case ups_event_cancel_requested:
            event.cancellation_reason = dataset.get_string(
                ups_tags::reason_for_cancellation);
            break;

        case ups_event_progress_report:
            event.progress = dataset.get_string(
                ups_tags::procedure_step_progress);
            break;

        default:
            break;
    }

    return event;
}

// =============================================================================
// Private Implementation - Utility Functions
// =============================================================================

uint16_t ups_watch_scu::next_message_id() noexcept {
    uint16_t id = message_id_counter_.fetch_add(1, std::memory_order_relaxed);
    if (id == 0) {
        id = message_id_counter_.fetch_add(1, std::memory_order_relaxed);
    }
    return id;
}

}  // namespace pacs::services
