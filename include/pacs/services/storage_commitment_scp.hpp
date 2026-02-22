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
 * @file storage_commitment_scp.hpp
 * @brief DICOM Storage Commitment Push Model SCP service
 *
 * Handles N-ACTION requests from modalities to commit stored SOP Instances,
 * verifies each instance against the storage backend, and reports results
 * via N-EVENT-REPORT.
 *
 * @see DICOM PS3.4 Annex J - Storage Commitment Push Model Service Class
 * @see DICOM PS3.7 Section 10.1.4 - N-ACTION Service
 * @see DICOM PS3.7 Section 10.1.1 - N-EVENT-REPORT Service
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_SERVICES_STORAGE_COMMITMENT_SCP_HPP
#define PACS_SERVICES_STORAGE_COMMITMENT_SCP_HPP

#include "scp_service.hpp"
#include "storage_commitment_types.hpp"

#include <pacs/storage/storage_interface.hpp>

#include <atomic>
#include <memory>
#include <string>

namespace pacs::services {

/**
 * @brief Storage Commitment Push Model SCP
 *
 * Processes N-ACTION requests containing Referenced SOP Sequences,
 * verifies each instance exists in storage, and sends an N-EVENT-REPORT
 * with per-instance success/failure status.
 *
 * ## Protocol Flow
 *
 * ```
 * SCU (Modality)                      SCP (this)
 *  │                                   │
 *  │── N-ACTION-RQ ──────────────────►│
 *  │   Transaction UID                 │
 *  │   Referenced SOP Sequence         │
 *  │◄── N-ACTION-RSP (Success) ──────│
 *  │                                   │
 *  │   ... SCP verifies storage ...    │
 *  │                                   │
 *  │◄── N-EVENT-REPORT-RQ ──────────│
 *  │   Transaction UID                 │
 *  │   Success/Failed Sequences        │
 *  │── N-EVENT-REPORT-RSP ──────────►│
 * ```
 */
class storage_commitment_scp final : public scp_service {
public:
    /**
     * @brief Construct Storage Commitment SCP with storage backend
     *
     * @param storage The storage interface for verifying instance existence
     * @param logger Logger instance for service logging
     */
    explicit storage_commitment_scp(
        std::shared_ptr<storage::storage_interface> storage,
        std::shared_ptr<di::ILogger> logger = nullptr);

    ~storage_commitment_scp() override = default;

    // =========================================================================
    // scp_service Interface
    // =========================================================================

    [[nodiscard]] std::vector<std::string> supported_sop_classes() const override;

    [[nodiscard]] network::Result<std::monostate> handle_message(
        network::association& assoc,
        uint8_t context_id,
        const network::dimse::dimse_message& request) override;

    [[nodiscard]] std::string_view service_name() const noexcept override;

    // =========================================================================
    // Statistics
    // =========================================================================

    [[nodiscard]] size_t actions_processed() const noexcept;
    [[nodiscard]] size_t instances_committed() const noexcept;
    [[nodiscard]] size_t instances_failed() const noexcept;
    void reset_statistics() noexcept;

private:
    // =========================================================================
    // N-ACTION Handler
    // =========================================================================

    [[nodiscard]] network::Result<std::monostate> handle_n_action(
        network::association& assoc,
        uint8_t context_id,
        const network::dimse::dimse_message& request);

    // =========================================================================
    // Instance Verification
    // =========================================================================

    [[nodiscard]] commitment_result verify_instances(
        const std::string& transaction_uid,
        const std::vector<sop_reference>& references);

    // =========================================================================
    // N-EVENT-REPORT Sender
    // =========================================================================

    [[nodiscard]] network::Result<std::monostate> send_event_report(
        network::association& assoc,
        uint8_t context_id,
        const commitment_result& result);

    // =========================================================================
    // Response Helpers
    // =========================================================================

    [[nodiscard]] network::Result<std::monostate> send_n_action_response(
        network::association& assoc,
        uint8_t context_id,
        uint16_t message_id,
        network::dimse::status_code status);

    // =========================================================================
    // Dataset Parsing Helpers
    // =========================================================================

    [[nodiscard]] static std::vector<sop_reference> parse_referenced_sop_sequence(
        const core::dicom_dataset& dataset);

    [[nodiscard]] static core::dicom_dataset build_event_report_dataset(
        const commitment_result& result);

    // =========================================================================
    // Member Variables
    // =========================================================================

    std::shared_ptr<storage::storage_interface> storage_;

    std::atomic<size_t> actions_processed_{0};
    std::atomic<size_t> instances_committed_{0};
    std::atomic<size_t> instances_failed_{0};
};

}  // namespace pacs::services

#endif  // PACS_SERVICES_STORAGE_COMMITMENT_SCP_HPP
