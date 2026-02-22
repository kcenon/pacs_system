// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @file storage_commitment_scu.cpp
 * @brief Implementation of Storage Commitment Push Model SCU service
 */

#include "pacs/services/storage_commitment_scu.hpp"

#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/core/result.hpp"
#include "pacs/network/dimse/command_field.hpp"
#include "pacs/network/dimse/status_codes.hpp"

namespace pacs::services {

// =============================================================================
// Construction
// =============================================================================

storage_commitment_scu::storage_commitment_scu(
    std::shared_ptr<di::ILogger> logger)
    : logger_(logger ? std::move(logger) : di::null_logger()) {}

// =============================================================================
// Callback Configuration
// =============================================================================

void storage_commitment_scu::set_commitment_callback(commitment_callback cb) {
    callback_ = std::move(cb);
}

// =============================================================================
// N-ACTION Request
// =============================================================================

network::Result<std::monostate> storage_commitment_scu::request_commitment(
    network::association& assoc,
    uint8_t context_id,
    const std::string& transaction_uid,
    const std::vector<sop_reference>& references) {

    using namespace network::dimse;

    if (transaction_uid.empty() || references.empty()) {
        return pacs::pacs_void_error(
            pacs::error_codes::storage_commitment_missing_transaction_uid,
            "Transaction UID and references must not be empty");
    }

    // Build N-ACTION-RQ
    auto action_rq = make_n_action_rq(
        1,  // message_id
        storage_commitment_push_model_sop_class_uid,
        storage_commitment_push_model_sop_instance_uid,
        storage_commitment_action_type_request);

    // Build and attach action dataset
    auto action_dataset = build_action_dataset(transaction_uid, references);
    action_rq.set_dataset(std::move(action_dataset));

    // Send N-ACTION-RQ
    auto send_result = assoc.send_dimse(context_id, action_rq);
    if (send_result.is_err()) {
        return send_result;
    }

    ++requests_sent_;
    return pacs::ok();
}

// =============================================================================
// N-EVENT-REPORT Handler
// =============================================================================

network::Result<commitment_result> storage_commitment_scu::handle_event_report(
    const network::dimse::dimse_message& event_rq) {

    using namespace network::dimse;

    // Verify command is N-EVENT-REPORT-RQ
    if (event_rq.command() != command_field::n_event_report_rq) {
        return pacs::pacs_error<commitment_result>(
            pacs::error_codes::storage_commitment_unexpected_command,
            "Expected N-EVENT-REPORT-RQ, got: " +
            std::string(to_string(event_rq.command())));
    }

    // Verify dataset is present
    if (!event_rq.has_dataset()) {
        return pacs::pacs_error<commitment_result>(
            pacs::error_codes::storage_commitment_missing_sequence,
            "N-EVENT-REPORT-RQ has no dataset");
    }

    const auto& dataset = event_rq.dataset().value().get();

    // Parse the event report dataset
    auto result = parse_event_report_dataset(dataset);

    ++event_reports_received_;

    // Invoke callback if registered
    if (callback_) {
        callback_(result.transaction_uid, result);
    }

    return pacs::Result<commitment_result>::ok(std::move(result));
}

// =============================================================================
// Statistics
// =============================================================================

size_t storage_commitment_scu::requests_sent() const noexcept {
    return requests_sent_.load();
}

size_t storage_commitment_scu::event_reports_received() const noexcept {
    return event_reports_received_.load();
}

void storage_commitment_scu::reset_statistics() noexcept {
    requests_sent_ = 0;
    event_reports_received_ = 0;
}

// =============================================================================
// Dataset Builders
// =============================================================================

core::dicom_dataset storage_commitment_scu::build_action_dataset(
    const std::string& transaction_uid,
    const std::vector<sop_reference>& references) {

    core::dicom_dataset ds;

    // Transaction UID (0008,1195)
    ds.set_string(core::tags::transaction_uid, encoding::vr_type::UI,
                  transaction_uid);

    // Referenced SOP Sequence (0008,1199)
    auto& seq = ds.get_or_create_sequence(core::tags::referenced_sop_sequence);

    for (const auto& ref : references) {
        core::dicom_dataset item;
        item.set_string(core::tags::referenced_sop_class_uid,
                        encoding::vr_type::UI, ref.sop_class_uid);
        item.set_string(core::tags::referenced_sop_instance_uid,
                        encoding::vr_type::UI, ref.sop_instance_uid);
        seq.push_back(std::move(item));
    }

    return ds;
}

commitment_result storage_commitment_scu::parse_event_report_dataset(
    const core::dicom_dataset& dataset) {

    commitment_result result;

    // Extract Transaction UID (0008,1195)
    result.transaction_uid = dataset.get_string(core::tags::transaction_uid);
    result.timestamp = std::chrono::system_clock::now();

    // Parse Referenced SOP Sequence (0008,1199) — successful instances
    if (const auto* success_seq =
            dataset.get_sequence(core::tags::referenced_sop_sequence)) {
        for (const auto& item : *success_seq) {
            sop_reference ref;
            ref.sop_class_uid = item.get_string(
                core::tags::referenced_sop_class_uid);
            ref.sop_instance_uid = item.get_string(
                core::tags::referenced_sop_instance_uid);
            if (!ref.sop_class_uid.empty() && !ref.sop_instance_uid.empty()) {
                result.success_references.push_back(std::move(ref));
            }
        }
    }

    // Parse Failed SOP Sequence (0008,1198) — failed instances
    if (const auto* failed_seq =
            dataset.get_sequence(core::tags::failed_sop_sequence)) {
        for (const auto& item : *failed_seq) {
            sop_reference ref;
            ref.sop_class_uid = item.get_string(
                core::tags::referenced_sop_class_uid);
            ref.sop_instance_uid = item.get_string(
                core::tags::referenced_sop_instance_uid);

            auto reason = commitment_failure_reason::processing_failure;
            auto reason_val = item.get_numeric<uint16_t>(
                core::tags::failure_reason);
            if (reason_val.has_value()) {
                reason = static_cast<commitment_failure_reason>(
                    reason_val.value());
            }

            if (!ref.sop_class_uid.empty() && !ref.sop_instance_uid.empty()) {
                result.failed_references.emplace_back(std::move(ref), reason);
            }
        }
    }

    return result;
}

}  // namespace pacs::services
