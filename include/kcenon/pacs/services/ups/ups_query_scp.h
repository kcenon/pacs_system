/**
 * @file ups_query_scp.h
 * @brief DICOM UPS Query SCP service (C-FIND handler for UPS workitems)
 *
 * This file provides the ups_query_scp class for handling C-FIND requests
 * to search UPS workitems by state, priority, label, and other criteria.
 *
 * @see DICOM PS3.4 Annex CC - Unified Procedure Step Service
 * @see DICOM PS3.7 Section 9.1 - C-FIND Service
 * @see Issue #811 - UPS Query SCP Implementation
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_SERVICES_UPS_QUERY_SCP_HPP
#define PACS_SERVICES_UPS_QUERY_SCP_HPP

#include "../scp_service.h"

#include <atomic>
#include <functional>

namespace kcenon::pacs::services {

// =============================================================================
// SOP Class UID
// =============================================================================

/// UPS Query Information Model - FIND SOP Class UID (PS3.4 Table CC.2-1)
inline constexpr std::string_view ups_query_find_sop_class_uid =
    "1.2.840.10008.5.1.4.34.6.4";

// =============================================================================
// Handler Types
// =============================================================================

/**
 * @brief UPS query handler function type
 *
 * Called by ups_query_scp to retrieve matching UPS workitems for a
 * C-FIND query. The handler should query the database using the
 * provided query keys and return matching workitem datasets.
 *
 * @param query_keys The query dataset containing search criteria:
 *   - Procedure Step State (0074,1000)
 *   - Scheduled Procedure Step Priority (0074,1200)
 *   - Procedure Step Label (0074,1204)
 *   - Worklist Label (0074,1202)
 *   - SOP Instance UID (0008,0018) for unique key match
 * @param calling_ae The calling AE title of the requesting system
 * @return Vector of matching workitem datasets (empty if no matches)
 */
using ups_query_handler = std::function<std::vector<core::dicom_dataset>(
    const core::dicom_dataset& query_keys,
    const std::string& calling_ae)>;

/**
 * @brief Cancel check function type for UPS queries
 *
 * Called periodically during query processing to check if a C-CANCEL
 * request has been received.
 *
 * @return true if cancel has been requested
 */
using ups_query_cancel_check = std::function<bool()>;

// =============================================================================
// UPS Query SCP Class
// =============================================================================

/**
 * @brief UPS Query SCP service for handling workitem C-FIND requests
 *
 * The UPS Query SCP (Service Class Provider) responds to C-FIND
 * requests to search for UPS workitems. It follows the same
 * pending/final response pattern as the Modality Worklist SCP.
 *
 * ## UPS C-FIND Message Flow
 *
 * ```
 * SCU (Scheduler/Performer)              UPS Query SCP
 *  |                                     |
 *  |  C-FIND-RQ                          |
 *  |  +-------------------------------+  |
 *  |  | SOPClass: ...34.6.4           |  |
 *  |  | ProcedureStepState: SCHEDULED |  |
 *  |  | Priority: HIGH               |  |
 *  |  +-------------------------------+  |
 *  |------------------------------------>|
 *  |                                     |
 *  |  C-FIND-RSP (Pending, 0xFF00)       |
 *  |  +-------------------------------+  |
 *  |  | WorkitemUID: 1.2.3.4...       |  |
 *  |  | Label: "AI Analysis"         |  |
 *  |  | State: SCHEDULED             |  |
 *  |  +-------------------------------+  |
 *  |<------------------------------------|
 *  |                                     |
 *  |  ... (repeat for each match)        |
 *  |                                     |
 *  |  C-FIND-RSP (Success, 0x0000)       |
 *  |  (No dataset)                       |
 *  |<------------------------------------|
 * ```
 */
class ups_query_scp final : public scp_service {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct UPS Query SCP with optional logger
     *
     * @param logger Logger instance (nullptr uses null_logger)
     */
    explicit ups_query_scp(std::shared_ptr<di::ILogger> logger = nullptr);

    ~ups_query_scp() override = default;

    // =========================================================================
    // Configuration
    // =========================================================================

    void set_handler(ups_query_handler handler);
    void set_max_results(size_t max) noexcept;
    [[nodiscard]] size_t max_results() const noexcept;
    void set_cancel_check(ups_query_cancel_check check);

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

    [[nodiscard]] size_t queries_processed() const noexcept;
    [[nodiscard]] size_t items_returned() const noexcept;

    void reset_statistics() noexcept;

private:
    // =========================================================================
    // Private Implementation
    // =========================================================================

    [[nodiscard]] network::Result<std::monostate> send_pending_response(
        network::association& assoc,
        uint8_t context_id,
        uint16_t message_id,
        const core::dicom_dataset& result);

    [[nodiscard]] network::Result<std::monostate> send_final_response(
        network::association& assoc,
        uint8_t context_id,
        uint16_t message_id,
        network::dimse::status_code status);

    // =========================================================================
    // Member Variables
    // =========================================================================

    ups_query_handler handler_;
    ups_query_cancel_check cancel_check_;
    size_t max_results_{0};  // 0 = unlimited
    std::atomic<size_t> queries_processed_{0};
    std::atomic<size_t> items_returned_{0};
};

}  // namespace kcenon::pacs::services

#endif  // PACS_SERVICES_UPS_QUERY_SCP_HPP
