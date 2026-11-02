// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file storage_commitment_scp.cpp
 * @brief Implementation of Storage Commitment Push Model SCP service
 */

#include "kcenon/pacs/services/storage_commitment_scp.h"

#include "kcenon/pacs/core/dicom_tag_constants.h"
#include "kcenon/pacs/core/result.h"
#include "kcenon/pacs/network/dimse/command_field.h"
#include "kcenon/pacs/network/dimse/status_codes.h"

namespace kcenon::pacs::services {

// =============================================================================
// Construction
// =============================================================================

storage_commitment_scp::storage_commitment_scp(
    std::shared_ptr<storage::storage_interface> storage,
    std::shared_ptr<di::ILogger> logger)
    : scp_service(std::move(logger))
    , storage_(std::move(storage)) {}

void storage_commitment_scp::set_instance_verifier(commitment_verifier verifier) {
    verifier_ = std::move(verifier);
}

void storage_commitment_scp::set_event_sender(std::shared_ptr<event_sender> sender) {
    event_sender_ = std::move(sender);
}

void storage_commitment_scp::set_event_response_timeout(std::chrono::milliseconds timeout) {
    event_response_timeout_ = timeout;
}

// =============================================================================
// scp_service Interface
// =============================================================================

std::vector<std::string> storage_commitment_scp::supported_sop_classes() const {
    return {std::string(storage_commitment_push_model_sop_class_uid)};
}

network::Result<std::monostate> storage_commitment_scp::handle_message(
    network::association& assoc,
    uint8_t context_id,
    const network::dimse::dimse_message& request) {

    using namespace network::dimse;

    switch (request.command()) {
        case command_field::n_action_rq:
            return handle_n_action(assoc, context_id, request);

        default:
            return kcenon::pacs::pacs_void_error(
                kcenon::pacs::error_codes::storage_commitment_unexpected_command,
                "Unexpected command for Storage Commitment SCP: " +
                std::string(to_string(request.command())));
    }
}

std::string_view storage_commitment_scp::service_name() const noexcept {
    return "Storage Commitment SCP";
}

// =============================================================================
// Statistics
// =============================================================================

size_t storage_commitment_scp::actions_processed() const noexcept {
    return actions_processed_.load();
}

size_t storage_commitment_scp::instances_committed() const noexcept {
    return instances_committed_.load();
}

size_t storage_commitment_scp::instances_failed() const noexcept {
    return instances_failed_.load();
}

void storage_commitment_scp::reset_statistics() noexcept {
    actions_processed_ = 0;
    instances_committed_ = 0;
    instances_failed_ = 0;
}

// =============================================================================
// N-ACTION Handler
// =============================================================================

network::Result<std::monostate> storage_commitment_scp::handle_n_action(
    network::association& assoc,
    uint8_t context_id,
    const network::dimse::dimse_message& request) {

    using namespace network::dimse;

    // Validate Requested SOP Class UID
    auto sop_class_uid = request.command_set().get_string(
        tag_requested_sop_class_uid);
    if (sop_class_uid.empty()) {
        sop_class_uid = request.affected_sop_class_uid();
    }

    if (sop_class_uid != storage_commitment_push_model_sop_class_uid) {
        return send_n_action_response(
            assoc, context_id, request.message_id(),
            status_refused_sop_class_not_supported);
    }

    // Validate Action Type ID = 1 (Request Storage Commitment)
    auto action_type = request.action_type_id();
    if (!action_type.has_value() ||
        action_type.value() != storage_commitment_action_type_request) {
        return send_n_action_response(
            assoc, context_id, request.message_id(),
            status_error_no_such_action_type);
    }

    // PS3.4 J.3: the request goes to the well-known instance, and no other exists.
    if (request.requested_sop_instance_uid() != storage_commitment_push_model_sop_instance_uid) {
        return send_n_action_response(
            assoc, context_id, request.message_id(),
            status_error_invalid_object_instance);
    }

    // Verify we have a dataset
    if (!request.has_dataset()) {
        return send_n_action_response(
            assoc, context_id, request.message_id(),
            status_error_missing_attribute);
    }

    const auto& dataset = request.dataset().value().get();

    // Extract Transaction UID (0008,1195)
    auto transaction_uid = dataset.get_string(core::tags::transaction_uid);
    if (transaction_uid.empty()) {
        return send_n_action_response(
            assoc, context_id, request.message_id(),
            status_error_missing_attribute);
    }

    // Parse Referenced SOP Sequence (0008,1199)
    auto references = parse_referenced_sop_sequence(dataset);
    if (!references) {
        return send_n_action_response(
            assoc, context_id, request.message_id(),
            status_error_missing_attribute);
    }

    // Send N-ACTION-RSP with Success (acknowledge the request)
    auto rsp_result = send_n_action_response(
        assoc, context_id, request.message_id(),
        status_success);
    if (rsp_result.is_err()) {
        return rsp_result;
    }

    // Update statistics
    ++actions_processed_;

    // Verify instances against storage
    auto result = verify_instances(transaction_uid, *references);

    // Update instance statistics
    instances_committed_ += result.success_references.size();
    instances_failed_ += result.failed_references.size();

    // Send N-EVENT-REPORT with verification results
    return send_event_report(assoc, context_id, result);
}

// =============================================================================
// Instance Verification
// =============================================================================

commitment_result storage_commitment_scp::verify_instances(
    const std::string& transaction_uid,
    const std::vector<sop_reference>& references) {

    commitment_result result;
    result.transaction_uid = transaction_uid;
    result.timestamp = std::chrono::system_clock::now();

    for (const auto& ref : references) {
        std::optional<commitment_failure_reason> failure;
        if (verifier_) {
            failure = verifier_(ref);
        } else if (!storage_ || !storage_->exists(ref.sop_instance_uid)) {
            failure = commitment_failure_reason::no_such_object_instance;
        }
        if (failure) {
            result.failed_references.emplace_back(ref, *failure);
        } else {
            result.success_references.push_back(ref);
        }
    }

    return result;
}

// =============================================================================
// N-EVENT-REPORT Sender
// =============================================================================

network::Result<std::monostate> storage_commitment_scp::send_event_report(
    network::association& assoc,
    uint8_t context_id,
    const commitment_result& result) {

    using namespace network::dimse;

    // Determine event type: 1 = all success, 2 = failures exist
    uint16_t event_type = result.failed_references.empty()
        ? storage_commitment_event_type_success
        : storage_commitment_event_type_failure;

    auto event_dataset = build_event_report_dataset(result);

    // PS3.4 J.3.3: same AE Title, and an association of its own when the requestor can be reached.
    const std::string requestor{assoc.calling_ae()};
    if (event_sender_ && event_sender_->knows(requestor)) {
        event_report report;
        report.receiving_ae = requestor;
        report.abstract_syntax = storage_commitment_push_model_sop_class_uid;
        report.sop_class_uid = storage_commitment_push_model_sop_class_uid;
        report.sop_instance_uid = storage_commitment_push_model_sop_instance_uid;
        report.event_type_id = event_type;
        report.information = std::move(event_dataset);
        event_sender_->send(std::move(report));
        return kcenon::pacs::ok();
    }

    auto message_id = assoc.next_request_message_id();
    if (message_id.is_err()) {
        return message_id.error();
    }
    auto event_rq = make_n_event_report_rq(
        message_id.value(),
        storage_commitment_push_model_sop_class_uid,
        storage_commitment_push_model_sop_instance_uid,
        event_type);
    event_rq.set_dataset(std::move(event_dataset));

    auto sent = assoc.send_request(context_id, event_rq);
    if (sent.is_err()) {
        logger_->error("Storage Commitment: N-EVENT-REPORT to " + requestor + " for transaction " +
                       result.transaction_uid + " not sent: " + sent.error().message);
        return sent;
    }
    auto response = assoc.receive_response(event_response_timeout_);
    if (response.is_err()) {
        assoc.cancel_response_wait();
        logger_->error("Storage Commitment: N-EVENT-REPORT to " + requestor + " for transaction " +
                       result.transaction_uid + " not answered: " + response.error().message);
        return response.error();
    }
    logger_->info("Storage Commitment: N-EVENT-REPORT answered with status " +
                  std::to_string(response.value().status()));
    return kcenon::pacs::ok();
}

// =============================================================================
// Response Helpers
// =============================================================================

network::Result<std::monostate> storage_commitment_scp::send_n_action_response(
    network::association& assoc,
    uint8_t context_id,
    uint16_t message_id,
    network::dimse::status_code status) {

    using namespace network::dimse;

    auto response = make_n_action_rsp(
        message_id,
        storage_commitment_push_model_sop_class_uid,
        storage_commitment_push_model_sop_instance_uid,
        storage_commitment_action_type_request,
        status);

    return assoc.send_dimse(context_id, response);
}

// =============================================================================
// Dataset Parsing Helpers
// =============================================================================

std::optional<std::vector<sop_reference>> storage_commitment_scp::parse_referenced_sop_sequence(
    const core::dicom_dataset& dataset) {

    const auto* seq = dataset.get_sequence(core::tags::referenced_sop_sequence);
    if (seq == nullptr || seq->empty()) {
        return std::nullopt;
    }

    std::vector<sop_reference> references;
    references.reserve(seq->size());
    for (const auto& item : *seq) {
        sop_reference ref;
        ref.sop_class_uid = item.get_string(core::tags::referenced_sop_class_uid);
        ref.sop_instance_uid = item.get_string(core::tags::referenced_sop_instance_uid);

        // Dropping an item would leave it out of both result lists, which J.3.3 forbids.
        if (ref.sop_class_uid.empty() || ref.sop_instance_uid.empty()) {
            return std::nullopt;
        }
        references.push_back(std::move(ref));
    }

    return references;
}

core::dicom_dataset storage_commitment_scp::build_event_report_dataset(
    const commitment_result& result) {

    core::dicom_dataset ds;

    // Transaction UID (0008,1195)
    ds.set_string(core::tags::transaction_uid, encoding::vr_type::UI,
                  result.transaction_uid);

    // Referenced SOP Sequence (0008,1199) — successful instances
    if (!result.success_references.empty()) {
        auto& success_seq = ds.get_or_create_sequence(
            core::tags::referenced_sop_sequence);

        for (const auto& ref : result.success_references) {
            core::dicom_dataset item;
            item.set_string(core::tags::referenced_sop_class_uid,
                            encoding::vr_type::UI, ref.sop_class_uid);
            item.set_string(core::tags::referenced_sop_instance_uid,
                            encoding::vr_type::UI, ref.sop_instance_uid);
            success_seq.push_back(std::move(item));
        }
    }

    // Failed SOP Sequence (0008,1198) — failed instances
    if (!result.failed_references.empty()) {
        auto& failed_seq = ds.get_or_create_sequence(
            core::tags::failed_sop_sequence);

        for (const auto& [ref, reason] : result.failed_references) {
            core::dicom_dataset item;
            item.set_string(core::tags::referenced_sop_class_uid,
                            encoding::vr_type::UI, ref.sop_class_uid);
            item.set_string(core::tags::referenced_sop_instance_uid,
                            encoding::vr_type::UI, ref.sop_instance_uid);
            item.set_numeric<uint16_t>(core::tags::failure_reason,
                                       encoding::vr_type::US,
                                       static_cast<uint16_t>(reason));
            failed_seq.push_back(std::move(item));
        }
    }

    return ds;
}

}  // namespace kcenon::pacs::services
