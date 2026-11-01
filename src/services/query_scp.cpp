// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file query_scp.cpp
 * @brief Implementation of the Query SCP service
 */

#include "kcenon/pacs/services/query_scp.h"

#include "kcenon/pacs/core/dicom_tag_constants.h"
#include "kcenon/pacs/core/events.h"
#include "kcenon/pacs/core/result.h"
#include "kcenon/pacs/network/dimse/command_field.h"
#include "kcenon/pacs/network/dimse/status_codes.h"
#include "kcenon/pacs/security/atna_service_auditor.h"

#include <kcenon/common/patterns/event_bus.h>

namespace kcenon::pacs::services {

namespace {
// Convert services::query_level to events::query_level
kcenon::pacs::events::query_level to_event_level(query_level level) {
    switch (level) {
        case query_level::patient: return kcenon::pacs::events::query_level::patient;
        case query_level::study: return kcenon::pacs::events::query_level::study;
        case query_level::series: return kcenon::pacs::events::query_level::series;
        case query_level::image: return kcenon::pacs::events::query_level::image;
        default: return kcenon::pacs::events::query_level::patient;
    }
}

[[nodiscard]] std::optional<qr_model> model_of(std::string_view sop_class_uid) noexcept {
    if (sop_class_uid == patient_root_find_sop_class_uid) return qr_model::patient_root;
    if (sop_class_uid == study_root_find_sop_class_uid) return qr_model::study_root;
    if (sop_class_uid == patient_study_only_find_sop_class_uid) return qr_model::patient_study_only;
    return std::nullopt;
}

}  // namespace

// =============================================================================
// Construction
// =============================================================================

query_scp::query_scp(std::shared_ptr<di::ILogger> logger)
    : scp_service(std::move(logger)) {}

// =============================================================================
// Configuration
// =============================================================================

void query_scp::set_handler(query_handler handler) {
    handler_ = std::move(handler);
}

void query_scp::set_find_handler(find_handler handler) {
    find_handler_ = std::move(handler);
}

void query_scp::set_max_results(size_t max) noexcept {
    max_results_ = max;
}

size_t query_scp::max_results() const noexcept {
    return max_results_;
}

void query_scp::set_cancel_check(cancel_check check) {
    cancel_check_ = std::move(check);
}

void query_scp::set_audit_handler(
    std::shared_ptr<kcenon::pacs::security::atna_service_auditor> auditor) {
    auditor_ = std::move(auditor);
}

// =============================================================================
// scp_service Interface Implementation
// =============================================================================

std::vector<std::string> query_scp::supported_sop_classes() const {
    return {
        std::string(patient_root_find_sop_class_uid),
        std::string(study_root_find_sop_class_uid),
        std::string(patient_study_only_find_sop_class_uid)
    };
}

network::Result<std::monostate> query_scp::handle_message(
    network::association& assoc,
    uint8_t context_id,
    const network::dimse::dimse_message& request) {

    using namespace network::dimse;
    auto start_time = std::chrono::steady_clock::now();
    std::string calling_ae{assoc.calling_ae()};

    // Verify the message is a C-FIND request
    if (request.command() != command_field::c_find_rq) {
        return kcenon::pacs::pacs_void_error(
            kcenon::pacs::error_codes::find_unexpected_command,
            "Expected C-FIND-RQ but received " +
            std::string(to_string(request.command())));
    }

    // Verify we have a handler
    if (!handler_ && !find_handler_) {
        kcenon::common::get_event_bus().publish(
            kcenon::pacs::events::query_failed_event{
                calling_ae,
                static_cast<int>(kcenon::pacs::error_codes::find_handler_not_set),
                "No query handler configured"
            }
        );
        return kcenon::pacs::pacs_void_error(
            kcenon::pacs::error_codes::find_handler_not_set,
            "No query handler configured");
    }

    // Get the SOP Class UID from the request
    auto sop_class_uid = request.affected_sop_class_uid();

    // Verify we have a dataset
    if (!request.has_dataset()) {
        return send_final_response(
            assoc, context_id, request.message_id(),
            sop_class_uid, status_error_dataset_mismatch);
    }

    // The level must exist in the model the SOP Class selects (PS3.4 Annex C.6)
    const auto& query_keys = request.dataset().value().get();
    auto level = extract_query_level(query_keys);
    const auto model = model_of(sop_class_uid);
    if (!level.has_value() || (model.has_value() && !level_in_model(*model, *level))) {
        return send_final_response(
            assoc, context_id, request.message_id(),
            sop_class_uid, status_error_dataset_mismatch);
    }

    // Call the handler to get matching results. A failure must not read as no matches.
    std::vector<core::dicom_dataset> results;
    status_code pending_status = status_pending;
    if (find_handler_ && model.has_value()) {
        find_outcome outcome;
        try {
            outcome = find_handler_(find_request{*model, *level, query_keys, calling_ae, assoc.called_ae()});
        } catch (const std::exception&) {
            outcome.failure = qr_failure::unable_to_process;
        }
        if (outcome.failure != qr_failure::none) {
            return send_final_response(
                assoc, context_id, request.message_id(), sop_class_uid,
                outcome.failure == qr_failure::identifier_mismatch ? status_error_dataset_mismatch
                                                                   : status_error_unable_to_process);
        }
        results = std::move(outcome.matches);
        if (outcome.optional_keys_ignored) {
            pending_status = status_pending_warning;
        }
    } else if (handler_) {
        results = handler_(level.value(), query_keys, calling_ae);
    } else {
        return send_final_response(assoc, context_id, request.message_id(), sop_class_uid,
                                   status_error_unable_to_process);
    }

    // Apply max results limit if configured
    if (max_results_ > 0 && results.size() > max_results_) {
        results.resize(max_results_);
    }

    // The association registers each C-FIND before dispatch, so a C-CANCEL naming this
    // message sets this flag from the receive thread while this loop is sending.
    const auto cancelled = [&] {
        return assoc.cancellation_requested(context_id, request.message_id()) ||
               (cancel_check_ && cancel_check_());
    };

    // Send pending responses for each result
    for (const auto& result : results) {
        // Check for cancel request
        if (cancelled()) {
            // Increment statistics
            ++queries_processed_;

            // Send cancel response
            return send_final_response(
                assoc, context_id, request.message_id(),
                sop_class_uid, status_cancel);
        }

        // Send pending response with matching dataset
        auto send_result = send_pending_response(
            assoc, context_id, request.message_id(),
            sop_class_uid, result, pending_status);

        if (send_result.is_err()) {
            return send_result;
        }
    }

    // Increment statistics
    ++queries_processed_;

    // A cancel that arrives after the last match still ends the operation as cancelled.
    if (cancelled()) {
        return send_final_response(
            assoc, context_id, request.message_id(),
            sop_class_uid, status_cancel);
    }

    // Calculate execution time
    auto end_time = std::chrono::steady_clock::now();
    auto execution_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    // Publish query executed event
    kcenon::common::get_event_bus().publish(
        kcenon::pacs::events::query_executed_event{
            to_event_level(level.value()),
            calling_ae,
            results.size(),
            static_cast<uint64_t>(execution_time_ms)
        }
    );

    // Emit ATNA audit event
    if (auditor_) {
        auditor_->audit_query(
            calling_ae,
            std::string(assoc.called_ae()),
            std::string(to_string(level.value())),
            true);
    }

    // Send final success response (no dataset)
    return send_final_response(
        assoc, context_id, request.message_id(),
        sop_class_uid, status_success);
}

std::string_view query_scp::service_name() const noexcept {
    return "Query SCP";
}

// =============================================================================
// Statistics
// =============================================================================

size_t query_scp::queries_processed() const noexcept {
    return queries_processed_.load();
}

void query_scp::reset_statistics() noexcept {
    queries_processed_ = 0;
}

// =============================================================================
// Private Implementation
// =============================================================================

std::optional<query_level> query_scp::extract_query_level(
    const core::dicom_dataset& dataset) const {

    // Get Query/Retrieve Level tag (0008,0052)
    auto level_str = dataset.get_string(core::tags::query_retrieve_level);

    if (level_str.empty()) {
        return std::nullopt;
    }

    return parse_query_level(level_str);
}

network::Result<std::monostate> query_scp::send_pending_response(
    network::association& assoc,
    uint8_t context_id,
    uint16_t message_id,
    std::string_view sop_class_uid,
    const core::dicom_dataset& result,
    network::dimse::status_code status) {

    using namespace network::dimse;

    // Create C-FIND response with pending status
    auto response = make_c_find_rsp(
        message_id,
        sop_class_uid,
        status
    );

    // Attach the matching dataset
    response.set_dataset(result);

    // Send the response
    return assoc.send_dimse(context_id, response);
}

network::Result<std::monostate> query_scp::send_final_response(
    network::association& assoc,
    uint8_t context_id,
    uint16_t message_id,
    std::string_view sop_class_uid,
    network::dimse::status_code status) {

    using namespace network::dimse;

    // Create C-FIND response with final status (no dataset)
    auto response = make_c_find_rsp(
        message_id,
        sop_class_uid,
        status
    );

    // Send the response
    return assoc.send_dimse(context_id, response);
}

}  // namespace kcenon::pacs::services
