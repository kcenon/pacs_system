/**
 * @file ups_push_scu.cpp
 * @brief Implementation of the UPS Push SCU service
 */

#include "kcenon/pacs/services/ups/ups_push_scu.h"

#include "kcenon/pacs/core/dicom_tag_constants.h"
#include "kcenon/pacs/core/result.h"
#include "kcenon/pacs/encoding/vr_type.h"
#include "kcenon/pacs/network/dimse/command_field.h"
#include "kcenon/pacs/network/dimse/status_codes.h"

#include <chrono>
#include <random>
#include <sstream>

namespace kcenon::pacs::services {

// =============================================================================
// Local Helper Functions
// =============================================================================

namespace {

/// UID root for auto-generated UPS workitem UIDs
constexpr const char* uid_root = "1.2.826.0.1.3680043.2.1545.3";

/// Requested SOP Instance UID tag (used in N-SET/N-GET/N-ACTION command set)
constexpr core::dicom_tag tag_requested_sop_instance_uid{0x0000, 0x1001};

/**
 * @brief Create N-CREATE request message for UPS
 */
network::dimse::dimse_message make_ups_n_create_rq(
    uint16_t message_id,
    std::string_view sop_instance_uid,
    core::dicom_dataset dataset) {

    using namespace network::dimse;

    dimse_message msg{command_field::n_create_rq, message_id};
    msg.set_affected_sop_class_uid(ups_push_sop_class_uid);
    msg.set_affected_sop_instance_uid(sop_instance_uid);
    msg.set_dataset(std::move(dataset));

    return msg;
}

/**
 * @brief Create N-SET request message for UPS
 */
network::dimse::dimse_message make_ups_n_set_rq(
    uint16_t message_id,
    std::string_view sop_instance_uid,
    core::dicom_dataset modifications) {

    using namespace network::dimse;
    using namespace encoding;

    dimse_message msg{command_field::n_set_rq, message_id};
    msg.set_affected_sop_class_uid(ups_push_sop_class_uid);
    msg.command_set().set_string(
        tag_requested_sop_instance_uid,
        vr_type::UI,
        std::string(sop_instance_uid));
    msg.set_dataset(std::move(modifications));

    return msg;
}

}  // namespace

// =============================================================================
// Construction
// =============================================================================

ups_push_scu::ups_push_scu(std::shared_ptr<di::ILogger> logger)
    : logger_(logger ? std::move(logger) : di::null_logger()) {}

ups_push_scu::ups_push_scu(const ups_push_scu_config& config,
                           std::shared_ptr<di::ILogger> logger)
    : logger_(logger ? std::move(logger) : di::null_logger()),
      config_(config) {}

// =============================================================================
// N-CREATE Operation
// =============================================================================

network::Result<ups_result> ups_push_scu::create(
    network::association& assoc,
    const ups_create_data& data) {

    using namespace network::dimse;

    auto start_time = std::chrono::steady_clock::now();

    // Verify association is established
    if (!assoc.is_established()) {
        return kcenon::pacs::pacs_error<ups_result>(
            kcenon::pacs::error_codes::association_not_established,
            "Association not established");
    }

    // Get accepted presentation context for UPS Push
    auto context_id = assoc.accepted_context_id(ups_push_sop_class_uid);
    if (!context_id) {
        return kcenon::pacs::pacs_error<ups_result>(
            kcenon::pacs::error_codes::ups_context_not_accepted,
            "No accepted presentation context for UPS Push SOP Class");
    }

    // Generate or use provided workitem UID
    std::string uid = data.workitem_uid;
    if (uid.empty() && config_.auto_generate_uid) {
        uid = generate_workitem_uid();
    }

    if (uid.empty()) {
        return kcenon::pacs::pacs_error<ups_result>(
            kcenon::pacs::error_codes::ups_missing_uid,
            "Workitem UID is required");
    }

    // Build the creation dataset
    auto dataset = build_create_dataset(data);

    // Create the N-CREATE request
    auto request = make_ups_n_create_rq(next_message_id(), uid, std::move(dataset));

    logger_->debug("Sending N-CREATE request for UPS workitem: " + uid);

    // Send the request
    auto send_result = assoc.send_dimse(*context_id, request);
    if (send_result.is_err()) {
        logger_->error("Failed to send N-CREATE: " + send_result.error().message);
        return send_result.error();
    }

    // Receive the response
    auto recv_result = assoc.receive_dimse(config_.timeout);
    if (recv_result.is_err()) {
        logger_->error("Failed to receive N-CREATE response: " +
                       recv_result.error().message);
        return recv_result.error();
    }

    const auto& [recv_context_id, response] = recv_result.value();

    // Verify it's an N-CREATE response
    if (response.command() != command_field::n_create_rsp) {
        return kcenon::pacs::pacs_error<ups_result>(
            kcenon::pacs::error_codes::ups_unexpected_command,
            "Expected N-CREATE-RSP but received " +
            std::string(to_string(response.command())));
    }

    auto end_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    // Build result
    ups_result result;
    result.workitem_uid = uid;
    result.status = static_cast<uint16_t>(response.status());
    result.elapsed = elapsed;

    if (response.command_set().contains(tag_error_comment)) {
        result.error_comment = response.command_set().get_string(tag_error_comment);
    }

    creates_performed_.fetch_add(1, std::memory_order_relaxed);

    if (result.is_success()) {
        logger_->info("N-CREATE successful for UPS workitem: " + uid);
    } else {
        logger_->warn("N-CREATE returned status 0x" +
                      std::to_string(result.status) +
                      " for UPS workitem: " + uid);
    }

    return result;
}

// =============================================================================
// N-SET Operation
// =============================================================================

network::Result<ups_result> ups_push_scu::set(
    network::association& assoc,
    const ups_set_data& data) {

    using namespace network::dimse;

    auto start_time = std::chrono::steady_clock::now();

    if (!assoc.is_established()) {
        return kcenon::pacs::pacs_error<ups_result>(
            kcenon::pacs::error_codes::association_not_established,
            "Association not established");
    }

    if (data.workitem_uid.empty()) {
        return kcenon::pacs::pacs_error<ups_result>(
            kcenon::pacs::error_codes::ups_missing_uid,
            "Workitem UID is required for N-SET");
    }

    auto context_id = assoc.accepted_context_id(ups_push_sop_class_uid);
    if (!context_id) {
        return kcenon::pacs::pacs_error<ups_result>(
            kcenon::pacs::error_codes::ups_context_not_accepted,
            "No accepted presentation context for UPS Push SOP Class");
    }

    // Create the N-SET request with modification dataset
    auto request = make_ups_n_set_rq(
        next_message_id(),
        data.workitem_uid,
        core::dicom_dataset(data.modifications));

    logger_->debug("Sending N-SET request for UPS workitem: " + data.workitem_uid);

    auto send_result = assoc.send_dimse(*context_id, request);
    if (send_result.is_err()) {
        logger_->error("Failed to send N-SET: " + send_result.error().message);
        return send_result.error();
    }

    auto recv_result = assoc.receive_dimse(config_.timeout);
    if (recv_result.is_err()) {
        logger_->error("Failed to receive N-SET response: " +
                       recv_result.error().message);
        return recv_result.error();
    }

    const auto& [recv_context_id, response] = recv_result.value();

    if (response.command() != command_field::n_set_rsp) {
        return kcenon::pacs::pacs_error<ups_result>(
            kcenon::pacs::error_codes::ups_unexpected_command,
            "Expected N-SET-RSP but received " +
            std::string(to_string(response.command())));
    }

    auto end_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    ups_result result;
    result.workitem_uid = data.workitem_uid;
    result.status = static_cast<uint16_t>(response.status());
    result.elapsed = elapsed;

    if (response.command_set().contains(tag_error_comment)) {
        result.error_comment = response.command_set().get_string(tag_error_comment);
    }

    sets_performed_.fetch_add(1, std::memory_order_relaxed);

    if (result.is_success()) {
        logger_->info("N-SET successful for UPS workitem: " + data.workitem_uid);
    } else {
        logger_->warn("N-SET returned status 0x" +
                      std::to_string(result.status) +
                      " for UPS workitem: " + data.workitem_uid);
    }

    return result;
}

// =============================================================================
// N-GET Operation
// =============================================================================

network::Result<ups_result> ups_push_scu::get(
    network::association& assoc,
    const ups_get_data& data) {

    using namespace network::dimse;

    auto start_time = std::chrono::steady_clock::now();

    if (!assoc.is_established()) {
        return kcenon::pacs::pacs_error<ups_result>(
            kcenon::pacs::error_codes::association_not_established,
            "Association not established");
    }

    if (data.workitem_uid.empty()) {
        return kcenon::pacs::pacs_error<ups_result>(
            kcenon::pacs::error_codes::ups_missing_uid,
            "Workitem UID is required for N-GET");
    }

    auto context_id = assoc.accepted_context_id(ups_push_sop_class_uid);
    if (!context_id) {
        return kcenon::pacs::pacs_error<ups_result>(
            kcenon::pacs::error_codes::ups_context_not_accepted,
            "No accepted presentation context for UPS Push SOP Class");
    }

    // Use the global factory function for N-GET
    auto request = make_n_get_rq(
        next_message_id(),
        ups_push_sop_class_uid,
        data.workitem_uid,
        data.attribute_tags);

    logger_->debug("Sending N-GET request for UPS workitem: " + data.workitem_uid +
                   " (attributes: " +
                   (data.attribute_tags.empty() ? "all" :
                    std::to_string(data.attribute_tags.size())) + ")");

    auto send_result = assoc.send_dimse(*context_id, request);
    if (send_result.is_err()) {
        logger_->error("Failed to send N-GET: " + send_result.error().message);
        return send_result.error();
    }

    auto recv_result = assoc.receive_dimse(config_.timeout);
    if (recv_result.is_err()) {
        logger_->error("Failed to receive N-GET response: " +
                       recv_result.error().message);
        return recv_result.error();
    }

    const auto& [recv_context_id, response] = recv_result.value();

    if (response.command() != command_field::n_get_rsp) {
        return kcenon::pacs::pacs_error<ups_result>(
            kcenon::pacs::error_codes::ups_unexpected_command,
            "Expected N-GET-RSP but received " +
            std::string(to_string(response.command())));
    }

    auto end_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    ups_result result;
    result.workitem_uid = data.workitem_uid;
    result.status = static_cast<uint16_t>(response.status());
    result.elapsed = elapsed;

    if (response.command_set().contains(tag_error_comment)) {
        result.error_comment = response.command_set().get_string(tag_error_comment);
    }

    // Extract returned attributes from response dataset
    if (response.has_dataset()) {
        auto dataset_result = response.dataset();
        if (dataset_result.is_ok()) {
            result.attributes = dataset_result.value().get();
        }
    }

    gets_performed_.fetch_add(1, std::memory_order_relaxed);

    if (result.is_success()) {
        logger_->info("N-GET successful for UPS workitem: " + data.workitem_uid);
    } else {
        logger_->warn("N-GET returned status 0x" +
                      std::to_string(result.status) +
                      " for UPS workitem: " + data.workitem_uid);
    }

    return result;
}

// =============================================================================
// N-ACTION Operations
// =============================================================================

network::Result<ups_result> ups_push_scu::change_state(
    network::association& assoc,
    const ups_change_state_data& data) {

    using namespace network::dimse;

    auto start_time = std::chrono::steady_clock::now();

    if (!assoc.is_established()) {
        return kcenon::pacs::pacs_error<ups_result>(
            kcenon::pacs::error_codes::association_not_established,
            "Association not established");
    }

    if (data.workitem_uid.empty()) {
        return kcenon::pacs::pacs_error<ups_result>(
            kcenon::pacs::error_codes::ups_missing_uid,
            "Workitem UID is required for state change");
    }

    if (data.transaction_uid.empty()) {
        return kcenon::pacs::pacs_error<ups_result>(
            kcenon::pacs::error_codes::ups_missing_transaction_uid,
            "Transaction UID is required for state change");
    }

    auto context_id = assoc.accepted_context_id(ups_push_sop_class_uid);
    if (!context_id) {
        return kcenon::pacs::pacs_error<ups_result>(
            kcenon::pacs::error_codes::ups_context_not_accepted,
            "No accepted presentation context for UPS Push SOP Class");
    }

    // Build the N-ACTION request with action dataset
    auto request = make_n_action_rq(
        next_message_id(),
        ups_push_sop_class_uid,
        data.workitem_uid,
        ups_action_change_state);

    // Attach dataset with state and transaction UID
    auto action_dataset = build_change_state_dataset(data);
    request.set_dataset(std::move(action_dataset));

    logger_->debug("Sending N-ACTION (Change State) for UPS workitem: " +
                   data.workitem_uid + " -> " + data.requested_state);

    auto send_result = assoc.send_dimse(*context_id, request);
    if (send_result.is_err()) {
        logger_->error("Failed to send N-ACTION: " + send_result.error().message);
        return send_result.error();
    }

    auto recv_result = assoc.receive_dimse(config_.timeout);
    if (recv_result.is_err()) {
        logger_->error("Failed to receive N-ACTION response: " +
                       recv_result.error().message);
        return recv_result.error();
    }

    const auto& [recv_context_id, response] = recv_result.value();

    if (response.command() != command_field::n_action_rsp) {
        return kcenon::pacs::pacs_error<ups_result>(
            kcenon::pacs::error_codes::ups_unexpected_command,
            "Expected N-ACTION-RSP but received " +
            std::string(to_string(response.command())));
    }

    auto end_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    ups_result result;
    result.workitem_uid = data.workitem_uid;
    result.status = static_cast<uint16_t>(response.status());
    result.elapsed = elapsed;

    if (response.command_set().contains(tag_error_comment)) {
        result.error_comment = response.command_set().get_string(tag_error_comment);
    }

    actions_performed_.fetch_add(1, std::memory_order_relaxed);

    if (result.is_success()) {
        logger_->info("N-ACTION (Change State) successful for UPS workitem: " +
                      data.workitem_uid + " -> " + data.requested_state);
    } else {
        logger_->warn("N-ACTION returned status 0x" +
                      std::to_string(result.status) +
                      " for UPS workitem: " + data.workitem_uid);
    }

    return result;
}

network::Result<ups_result> ups_push_scu::request_cancel(
    network::association& assoc,
    const ups_request_cancel_data& data) {

    using namespace network::dimse;

    auto start_time = std::chrono::steady_clock::now();

    if (!assoc.is_established()) {
        return kcenon::pacs::pacs_error<ups_result>(
            kcenon::pacs::error_codes::association_not_established,
            "Association not established");
    }

    if (data.workitem_uid.empty()) {
        return kcenon::pacs::pacs_error<ups_result>(
            kcenon::pacs::error_codes::ups_missing_uid,
            "Workitem UID is required for cancel request");
    }

    auto context_id = assoc.accepted_context_id(ups_push_sop_class_uid);
    if (!context_id) {
        return kcenon::pacs::pacs_error<ups_result>(
            kcenon::pacs::error_codes::ups_context_not_accepted,
            "No accepted presentation context for UPS Push SOP Class");
    }

    // Build the N-ACTION request for cancel
    auto request = make_n_action_rq(
        next_message_id(),
        ups_push_sop_class_uid,
        data.workitem_uid,
        ups_action_request_cancel);

    // Attach dataset with cancellation reason if provided
    if (!data.reason.empty()) {
        auto cancel_dataset = build_request_cancel_dataset(data);
        request.set_dataset(std::move(cancel_dataset));
    }

    logger_->debug("Sending N-ACTION (Request Cancel) for UPS workitem: " +
                   data.workitem_uid);

    auto send_result = assoc.send_dimse(*context_id, request);
    if (send_result.is_err()) {
        logger_->error("Failed to send N-ACTION: " + send_result.error().message);
        return send_result.error();
    }

    auto recv_result = assoc.receive_dimse(config_.timeout);
    if (recv_result.is_err()) {
        logger_->error("Failed to receive N-ACTION response: " +
                       recv_result.error().message);
        return recv_result.error();
    }

    const auto& [recv_context_id, response] = recv_result.value();

    if (response.command() != command_field::n_action_rsp) {
        return kcenon::pacs::pacs_error<ups_result>(
            kcenon::pacs::error_codes::ups_unexpected_command,
            "Expected N-ACTION-RSP but received " +
            std::string(to_string(response.command())));
    }

    auto end_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    ups_result result;
    result.workitem_uid = data.workitem_uid;
    result.status = static_cast<uint16_t>(response.status());
    result.elapsed = elapsed;

    if (response.command_set().contains(tag_error_comment)) {
        result.error_comment = response.command_set().get_string(tag_error_comment);
    }

    actions_performed_.fetch_add(1, std::memory_order_relaxed);

    if (result.is_success()) {
        logger_->info("N-ACTION (Request Cancel) successful for UPS workitem: " +
                      data.workitem_uid);
    } else {
        logger_->warn("N-ACTION (Request Cancel) returned status 0x" +
                      std::to_string(result.status) +
                      " for UPS workitem: " + data.workitem_uid);
    }

    return result;
}

// =============================================================================
// Statistics
// =============================================================================

size_t ups_push_scu::creates_performed() const noexcept {
    return creates_performed_.load(std::memory_order_relaxed);
}

size_t ups_push_scu::sets_performed() const noexcept {
    return sets_performed_.load(std::memory_order_relaxed);
}

size_t ups_push_scu::gets_performed() const noexcept {
    return gets_performed_.load(std::memory_order_relaxed);
}

size_t ups_push_scu::actions_performed() const noexcept {
    return actions_performed_.load(std::memory_order_relaxed);
}

void ups_push_scu::reset_statistics() noexcept {
    creates_performed_.store(0, std::memory_order_relaxed);
    sets_performed_.store(0, std::memory_order_relaxed);
    gets_performed_.store(0, std::memory_order_relaxed);
    actions_performed_.store(0, std::memory_order_relaxed);
}

// =============================================================================
// Private Implementation - Dataset Building
// =============================================================================

core::dicom_dataset ups_push_scu::build_create_dataset(
    const ups_create_data& data) const {

    using namespace core;
    using namespace encoding;

    dicom_dataset ds;

    // Procedure Step State - always SCHEDULED for N-CREATE
    ds.set_string(ups_tags::procedure_step_state, vr_type::CS, "SCHEDULED");

    // Procedure Step Label (required)
    if (!data.procedure_step_label.empty()) {
        ds.set_string(ups_tags::procedure_step_label, vr_type::LO,
                      data.procedure_step_label);
    }

    // Worklist Label
    if (!data.worklist_label.empty()) {
        ds.set_string(ups_tags::worklist_label, vr_type::LO,
                      data.worklist_label);
    }

    // Priority
    if (!data.priority.empty()) {
        ds.set_string(ups_tags::scheduled_procedure_step_priority, vr_type::CS,
                      data.priority);
    }

    // Scheduled start datetime
    if (!data.scheduled_start_datetime.empty()) {
        ds.set_string(
            dicom_tag{0x0040, 0x4005}, vr_type::DT,
            data.scheduled_start_datetime);
    }

    // Expected completion datetime
    if (!data.expected_completion_datetime.empty()) {
        ds.set_string(
            dicom_tag{0x0040, 0x4011}, vr_type::DT,
            data.expected_completion_datetime);
    }

    // Scheduled Station Name
    if (!data.scheduled_station_name.empty()) {
        ds.set_string(
            dicom_tag{0x0040, 0x4001}, vr_type::CS,
            data.scheduled_station_name);
    }

    return ds;
}

core::dicom_dataset ups_push_scu::build_change_state_dataset(
    const ups_change_state_data& data) const {

    using namespace core;
    using namespace encoding;

    dicom_dataset ds;

    // Requested state
    ds.set_string(ups_tags::procedure_step_state, vr_type::CS,
                  data.requested_state);

    // Transaction UID
    ds.set_string(ups_tags::transaction_uid, vr_type::UI,
                  data.transaction_uid);

    return ds;
}

core::dicom_dataset ups_push_scu::build_request_cancel_dataset(
    const ups_request_cancel_data& data) const {

    using namespace core;
    using namespace encoding;

    dicom_dataset ds;

    if (!data.reason.empty()) {
        ds.set_string(ups_tags::reason_for_cancellation, vr_type::LT,
                      data.reason);
    }

    return ds;
}

// =============================================================================
// Private Implementation - Utility Functions
// =============================================================================

std::string ups_push_scu::generate_workitem_uid() const {
    static std::mt19937_64 gen{std::random_device{}()};
    static std::uniform_int_distribution<uint64_t> dist;

    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    return std::string(uid_root) + "." + std::to_string(timestamp) +
           "." + std::to_string(dist(gen) % 100000);
}

uint16_t ups_push_scu::next_message_id() noexcept {
    uint16_t id = message_id_counter_.fetch_add(1, std::memory_order_relaxed);
    if (id == 0) {
        id = message_id_counter_.fetch_add(1, std::memory_order_relaxed);
    }
    return id;
}

}  // namespace kcenon::pacs::services
