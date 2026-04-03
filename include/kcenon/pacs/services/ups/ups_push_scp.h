/**
 * @file ups_push_scp.h
 * @brief DICOM UPS (Unified Procedure Step) Push SCP service
 *
 * This file provides the ups_push_scp class for handling N-CREATE, N-SET,
 * N-GET, and N-ACTION requests for UPS workitem management.
 *
 * @see DICOM PS3.4 Annex CC - Unified Procedure Step Service
 * @see DICOM PS3.7 Section 10 - DIMSE-N Services
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_SERVICES_UPS_PUSH_SCP_HPP
#define PACS_SERVICES_UPS_PUSH_SCP_HPP

#include "../scp_service.h"

#include <atomic>
#include <functional>
#include <optional>
#include <string>
#include <variant>

#include "kcenon/pacs/storage/ups_workitem.h"

namespace kcenon::pacs::services {

// =============================================================================
// SOP Class UID
// =============================================================================

/// UPS Push SOP Class UID (PS3.4 Table CC.2-1)
inline constexpr std::string_view ups_push_sop_class_uid =
    "1.2.840.10008.5.1.4.34.6.1";

// =============================================================================
// N-ACTION Type Constants
// =============================================================================

/// N-ACTION Type 1: Change UPS State (PS3.4 CC.2.4)
inline constexpr uint16_t ups_action_change_state = 1;

/// N-ACTION Type 3: Request Cancellation (PS3.4 CC.2.5)
inline constexpr uint16_t ups_action_request_cancel = 3;

// =============================================================================
// Handler Types
// =============================================================================

/**
 * @brief N-CREATE handler function type
 *
 * Called when an N-CREATE request is received to create a new UPS workitem.
 * The workitem will always have state=SCHEDULED.
 *
 * @param workitem The UPS workitem data from the N-CREATE request
 * @return Success if the workitem was created, error description otherwise
 */
using ups_create_handler = std::function<network::Result<std::monostate>(
    const storage::ups_workitem& workitem)>;

/**
 * @brief N-SET handler function type
 *
 * Called when an N-SET request is received to update an existing UPS workitem.
 *
 * @param workitem_uid The SOP Instance UID of the workitem to update
 * @param modifications The dataset containing the modifications
 * @return Success if the workitem was updated, error description otherwise
 */
using ups_set_handler = std::function<network::Result<std::monostate>(
    const std::string& workitem_uid,
    const core::dicom_dataset& modifications)>;

/**
 * @brief N-GET handler function type
 *
 * Called when an N-GET request is received to retrieve a UPS workitem.
 *
 * @param workitem_uid The SOP Instance UID of the workitem to retrieve
 * @return The workitem if found, error otherwise
 */
using ups_get_handler = std::function<network::Result<storage::ups_workitem>(
    const std::string& workitem_uid)>;

/**
 * @brief N-ACTION Type 1 handler: Change UPS State
 *
 * Called when an N-ACTION request with Action Type ID 1 is received.
 *
 * @param workitem_uid The SOP Instance UID of the workitem
 * @param new_state The requested new state
 * @param transaction_uid The transaction UID for claiming
 * @return Success if the state was changed, error otherwise
 */
using ups_change_state_handler = std::function<network::Result<std::monostate>(
    const std::string& workitem_uid,
    const std::string& new_state,
    const std::string& transaction_uid)>;

/**
 * @brief N-ACTION Type 3 handler: Request Cancellation
 *
 * Called when an N-ACTION request with Action Type ID 3 is received.
 *
 * @param workitem_uid The SOP Instance UID of the workitem
 * @param reason The cancellation reason (may be empty)
 * @return Success if the cancellation request was accepted, error otherwise
 */
using ups_request_cancel_handler = std::function<network::Result<std::monostate>(
    const std::string& workitem_uid,
    const std::string& reason)>;

// =============================================================================
// UPS Push SCP Class
// =============================================================================

/**
 * @brief UPS Push SCP service for handling workitem management requests
 *
 * The UPS Push SCP (Service Class Provider) responds to N-CREATE, N-SET,
 * N-GET, and N-ACTION requests from SCU peers. It manages UPS workitems
 * following the DICOM Unified Procedure Step Push model (PS3.4 Annex CC).
 *
 * ## UPS Push Message Flow
 *
 * ```
 * SCU (Performer)                          UPS Push SCP
 *  │                                       │
 *  │  N-CREATE-RQ (Create workitem)        │
 *  │  state = "SCHEDULED"                  │
 *  │──────────────────────────────────────►│
 *  │  N-CREATE-RSP                         │
 *  │◄──────────────────────────────────────│
 *  │                                       │
 *  │  N-ACTION-RQ (Claim: Type 1)          │
 *  │  new_state = "IN PROGRESS"            │
 *  │──────────────────────────────────────►│
 *  │  N-ACTION-RSP                         │
 *  │◄──────────────────────────────────────│
 *  │                                       │
 *  │  N-SET-RQ (Update progress)           │
 *  │──────────────────────────────────────►│
 *  │  N-SET-RSP                            │
 *  │◄──────────────────────────────────────│
 *  │                                       │
 *  │  N-ACTION-RQ (Complete: Type 1)       │
 *  │  new_state = "COMPLETED"              │
 *  │──────────────────────────────────────►│
 *  │  N-ACTION-RSP                         │
 *  │◄──────────────────────────────────────│
 * ```
 *
 * @example Usage
 * @code
 * ups_push_scp scp;
 *
 * scp.set_create_handler([&db](const ups_workitem& wi) {
 *     return db.create_ups_workitem(wi);
 * });
 *
 * scp.set_change_state_handler([&db](const auto& uid, const auto& state,
 *                                     const auto& txn_uid) {
 *     return db.change_ups_state(uid, state, txn_uid);
 * });
 *
 * auto result = scp.handle_message(association, context_id, request);
 * @endcode
 */
class ups_push_scp final : public scp_service {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct UPS Push SCP with optional logger
     *
     * @param logger Logger instance for service logging (nullptr uses null_logger)
     */
    explicit ups_push_scp(std::shared_ptr<di::ILogger> logger = nullptr);

    ~ups_push_scp() override = default;

    // =========================================================================
    // Configuration
    // =========================================================================

    void set_create_handler(ups_create_handler handler);
    void set_set_handler(ups_set_handler handler);
    void set_get_handler(ups_get_handler handler);
    void set_change_state_handler(ups_change_state_handler handler);
    void set_request_cancel_handler(ups_request_cancel_handler handler);

    // =========================================================================
    // scp_service Interface Implementation
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

    [[nodiscard]] size_t creates_processed() const noexcept;
    [[nodiscard]] size_t sets_processed() const noexcept;
    [[nodiscard]] size_t gets_processed() const noexcept;
    [[nodiscard]] size_t actions_processed() const noexcept;
    [[nodiscard]] size_t state_changes() const noexcept;
    [[nodiscard]] size_t cancel_requests() const noexcept;

    void reset_statistics() noexcept;

private:
    // =========================================================================
    // Private Implementation
    // =========================================================================

    [[nodiscard]] network::Result<std::monostate> handle_n_create(
        network::association& assoc,
        uint8_t context_id,
        const network::dimse::dimse_message& request);

    [[nodiscard]] network::Result<std::monostate> handle_n_set(
        network::association& assoc,
        uint8_t context_id,
        const network::dimse::dimse_message& request);

    [[nodiscard]] network::Result<std::monostate> handle_n_get(
        network::association& assoc,
        uint8_t context_id,
        const network::dimse::dimse_message& request);

    [[nodiscard]] network::Result<std::monostate> handle_n_action(
        network::association& assoc,
        uint8_t context_id,
        const network::dimse::dimse_message& request);

    [[nodiscard]] network::Result<std::monostate> send_n_create_response(
        network::association& assoc,
        uint8_t context_id,
        uint16_t message_id,
        const std::string& sop_instance_uid,
        network::dimse::status_code status);

    [[nodiscard]] network::Result<std::monostate> send_n_set_response(
        network::association& assoc,
        uint8_t context_id,
        uint16_t message_id,
        const std::string& sop_instance_uid,
        network::dimse::status_code status);

    [[nodiscard]] network::Result<std::monostate> send_n_get_response(
        network::association& assoc,
        uint8_t context_id,
        uint16_t message_id,
        const std::string& sop_instance_uid,
        network::dimse::status_code status,
        core::dicom_dataset* dataset = nullptr);

    [[nodiscard]] network::Result<std::monostate> send_n_action_response(
        network::association& assoc,
        uint8_t context_id,
        uint16_t message_id,
        const std::string& sop_instance_uid,
        uint16_t action_type_id,
        network::dimse::status_code status);

    // =========================================================================
    // Member Variables
    // =========================================================================

    ups_create_handler create_handler_;
    ups_set_handler set_handler_;
    ups_get_handler get_handler_;
    ups_change_state_handler change_state_handler_;
    ups_request_cancel_handler request_cancel_handler_;

    std::atomic<size_t> creates_processed_{0};
    std::atomic<size_t> sets_processed_{0};
    std::atomic<size_t> gets_processed_{0};
    std::atomic<size_t> actions_processed_{0};
    std::atomic<size_t> state_changes_{0};
    std::atomic<size_t> cancel_requests_{0};
};

// =============================================================================
// UPS DICOM Tags
// =============================================================================

namespace ups_tags {

/// Procedure Step State (0074,1000)
inline constexpr core::dicom_tag procedure_step_state{0x0074, 0x1000};

/// Procedure Step Progress (0074,1004)
inline constexpr core::dicom_tag procedure_step_progress{0x0074, 0x1004};

/// Scheduled Procedure Step Priority (0074,1200)
inline constexpr core::dicom_tag scheduled_procedure_step_priority{0x0074, 0x1200};

/// Worklist Label (0074,1202)
inline constexpr core::dicom_tag worklist_label{0x0074, 0x1202};

/// Procedure Step Label (0074,1204)
inline constexpr core::dicom_tag procedure_step_label{0x0074, 0x1204};

/// Transaction UID (0008,1195)
inline constexpr core::dicom_tag transaction_uid{0x0008, 0x1195};

/// Input Information Sequence (0040,4021)
inline constexpr core::dicom_tag input_information_sequence{0x0040, 0x4021};

/// Reason for Cancellation (0074,1238)
inline constexpr core::dicom_tag reason_for_cancellation{0x0074, 0x1238};

}  // namespace ups_tags

}  // namespace kcenon::pacs::services

#endif  // PACS_SERVICES_UPS_PUSH_SCP_HPP
