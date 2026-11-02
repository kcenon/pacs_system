// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file storage_commitment_scp.h
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

#include "event_sender.h"
#include "scp_service.h"
#include "storage_commitment_types.h"

#include <kcenon/pacs/storage/storage_interface.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace kcenon::pacs::services {

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
/// Why one referenced instance cannot be committed, or nothing when it can.
using commitment_verifier =
    std::function<std::optional<commitment_failure_reason>(const sop_reference& reference)>;

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

    /// Decide per instance instead of asking the storage backend whether the UID exists.
    void set_instance_verifier(commitment_verifier verifier);

    /**
     * @brief Report on a new association to requestors whose address is known
     *
     * PS3.4 J.3.3: the SCU may release as soon as it has the N-ACTION response. An AE the
     * sender does not know can only be answered on the association that asked, while it lasts.
     */
    void set_event_sender(std::shared_ptr<event_sender> sender);

    /// How long a report on the requesting association waits for its N-EVENT-REPORT-RSP.
    void set_event_response_timeout(std::chrono::milliseconds timeout);

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

    /// Nothing when the sequence is absent, empty, or names an item without both UIDs.
    [[nodiscard]] static std::optional<std::vector<sop_reference>> parse_referenced_sop_sequence(
        const core::dicom_dataset& dataset);

    [[nodiscard]] static core::dicom_dataset build_event_report_dataset(
        const commitment_result& result);

    // =========================================================================
    // Member Variables
    // =========================================================================

    std::shared_ptr<storage::storage_interface> storage_;
    commitment_verifier verifier_;
    std::shared_ptr<event_sender> event_sender_;
    std::chrono::milliseconds event_response_timeout_{30000};

    std::atomic<size_t> actions_processed_{0};
    std::atomic<size_t> instances_committed_{0};
    std::atomic<size_t> instances_failed_{0};
};

}  // namespace kcenon::pacs::services

#endif  // PACS_SERVICES_STORAGE_COMMITMENT_SCP_HPP
