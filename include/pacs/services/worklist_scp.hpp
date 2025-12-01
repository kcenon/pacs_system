/**
 * @file worklist_scp.hpp
 * @brief DICOM Modality Worklist SCP service (MWL C-FIND handler)
 *
 * This file provides the worklist_scp class for handling Modality Worklist
 * C-FIND requests from modality devices to retrieve scheduled procedure
 * information.
 *
 * @see DICOM PS3.4 Section K - Basic Worklist Management Service Class
 * @see DICOM PS3.7 Section 9.1 - C-FIND Service
 * @see DES-SVC-006 - Worklist SCP Design Specification
 */

#ifndef PACS_SERVICES_WORKLIST_SCP_HPP
#define PACS_SERVICES_WORKLIST_SCP_HPP

#include "scp_service.hpp"

#include <atomic>
#include <functional>

namespace pacs::services {

// =============================================================================
// SOP Class UIDs (re-exported for convenience)
// =============================================================================

/// Modality Worklist Information Model - FIND SOP Class UID
inline constexpr std::string_view worklist_find_sop_class_uid =
    "1.2.840.10008.5.1.4.31";

// =============================================================================
// Worklist Handler Type
// =============================================================================

/**
 * @brief Worklist handler function type
 *
 * Called by worklist_scp to retrieve matching scheduled procedure items
 * for a Modality Worklist C-FIND query.
 *
 * The handler should query the RIS/HIS database or worklist repository
 * and return matching scheduled procedure step items.
 *
 * @param query_keys The query dataset containing search criteria:
 *   - Patient demographics (PatientName, PatientID, etc.)
 *   - Scheduled Procedure Step Sequence with:
 *     - ScheduledStationAETitle (0040,0001)
 *     - ScheduledProcedureStepStartDate (0040,0002)
 *     - ScheduledProcedureStepStartTime (0040,0003)
 *     - Modality (0008,0060)
 *     - ScheduledPerformingPhysicianName (0040,0006)
 * @param calling_ae The calling AE title of the requesting modality
 * @return Vector of matching worklist item datasets (empty if no matches)
 *
 * @example Handler Implementation
 * @code
 * worklist_handler handler = [&database](
 *     const dicom_dataset& query,
 *     const std::string& calling_ae) {
 *
 *     std::vector<dicom_dataset> results;
 *
 *     // Extract filter criteria from Scheduled Procedure Step Sequence
 *     auto sps_seq = query.get_sequence(tags::scheduled_procedure_step_sequence);
 *     std::string station_ae, start_date, modality;
 *
 *     if (!sps_seq.empty()) {
 *         station_ae = sps_seq[0].get_string(tags::scheduled_station_ae_title);
 *         start_date = sps_seq[0].get_string(tags::scheduled_procedure_step_start_date);
 *         modality = sps_seq[0].get_string(tags::modality);
 *     }
 *
 *     // Query database and build worklist items
 *     return database.query_worklist(station_ae, start_date, modality);
 * };
 * @endcode
 */
using worklist_handler = std::function<std::vector<core::dicom_dataset>(
    const core::dicom_dataset& query_keys,
    const std::string& calling_ae)>;

/**
 * @brief Cancel check function type
 *
 * Called periodically during query processing to check if a C-CANCEL
 * request has been received.
 *
 * @return true if cancel has been requested
 */
using worklist_cancel_check = std::function<bool()>;

// =============================================================================
// Worklist SCP Class
// =============================================================================

/**
 * @brief Worklist SCP service for handling Modality Worklist C-FIND requests
 *
 * The Worklist SCP (Service Class Provider) responds to Modality Worklist
 * C-FIND requests from modality devices. It provides scheduled procedure
 * information including patient demographics, study details, and scheduled
 * procedure step attributes.
 *
 * ## MWL C-FIND Message Flow
 *
 * ```
 * Modality (CT/MR/etc)                    PACS/RIS (Worklist SCP)
 *  │                                       │
 *  │  C-FIND-RQ                            │
 *  │  ┌───────────────────────────────────┐│
 *  │  │ SOPClass: 1.2.840.10008.5.1.4.31  ││
 *  │  │ ScheduledProcedureStepSequence:   ││
 *  │  │   ScheduledStationAETitle: CT_01  ││
 *  │  │   ScheduledProcedureStepStartDate ││
 *  │  │   Modality: CT                    ││
 *  │  └───────────────────────────────────┘│
 *  │──────────────────────────────────────►│
 *  │                                       │
 *  │                               Query   │
 *  │                               RIS/HIS │
 *  │                               (N items)│
 *  │                                       │
 *  │  C-FIND-RSP (Pending)                 │
 *  │  ┌───────────────────────────────────┐│
 *  │  │ Status: 0xFF00 (Pending)          ││
 *  │  │ PatientName: DOE^JOHN             ││
 *  │  │ PatientID: 12345                  ││
 *  │  │ StudyInstanceUID: 1.2.3.4...      ││
 *  │  │ AccessionNumber: ACC001           ││
 *  │  │ ScheduledProcedureStepSequence:   ││
 *  │  │   ScheduledStationAETitle: CT_01  ││
 *  │  │   ScheduledProcedureStepStartDate ││
 *  │  │   ScheduledProcedureStepStartTime ││
 *  │  │   Modality: CT                    ││
 *  │  │   ScheduledProcedureStepID        ││
 *  │  └───────────────────────────────────┘│
 *  │◄──────────────────────────────────────│
 *  │                                       │
 *  │  ... (repeat for each scheduled item) │
 *  │                                       │
 *  │  C-FIND-RSP (Success)                 │
 *  │  ┌───────────────────────────────────┐│
 *  │  │ Status: 0x0000 (Success)          ││
 *  │  │ (No dataset)                      ││
 *  │  └───────────────────────────────────┘│
 *  │◄──────────────────────────────────────│
 * ```
 *
 * ## MWL Return Keys
 *
 * The following attributes are typically returned in worklist responses:
 *
 * | Tag | Keyword | Description |
 * |-----|---------|-------------|
 * | (0008,0050) | AccessionNumber | Exam accession number |
 * | (0010,0010) | PatientName | Patient name |
 * | (0010,0020) | PatientID | Patient identifier |
 * | (0010,0030) | PatientBirthDate | Patient birth date |
 * | (0010,0040) | PatientSex | Patient sex |
 * | (0020,000D) | StudyInstanceUID | Pre-assigned Study UID |
 * | (0040,0100) | ScheduledProcedureStepSequence | Procedure details |
 * | >(0008,0060) | Modality | Scheduled modality |
 * | >(0040,0001) | ScheduledStationAETitle | Target station |
 * | >(0040,0002) | ScheduledProcedureStepStartDate | Scheduled date |
 * | >(0040,0003) | ScheduledProcedureStepStartTime | Scheduled time |
 * | >(0040,0007) | ScheduledProcedureStepDescription | Step description |
 * | >(0040,0009) | ScheduledProcedureStepID | Step ID |
 *
 * @example Usage
 * @code
 * worklist_scp scp;
 *
 * // Set up worklist handler
 * scp.set_handler([&ris_database](const auto& query, const auto& ae) {
 *     return ris_database.query_worklist(query);
 * });
 *
 * // Optionally limit results
 * scp.set_max_results(100);
 *
 * // Handle incoming MWL C-FIND request
 * auto result = scp.handle_message(association, context_id, request);
 * @endcode
 */
class worklist_scp final : public scp_service {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    worklist_scp();
    ~worklist_scp() override = default;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Set the worklist handler function
     *
     * The handler is called for each MWL C-FIND request to retrieve matching
     * scheduled procedure items from the RIS/HIS database.
     *
     * @param handler The worklist handler function
     */
    void set_handler(worklist_handler handler);

    /**
     * @brief Set maximum number of results to return
     *
     * @param max Maximum results (0 = unlimited)
     */
    void set_max_results(size_t max) noexcept;

    /**
     * @brief Get maximum number of results
     *
     * @return Maximum results (0 = unlimited)
     */
    [[nodiscard]] size_t max_results() const noexcept;

    /**
     * @brief Set the cancel check function
     *
     * The cancel check is called periodically during query processing
     * to check if a C-CANCEL has been received.
     *
     * @param check The cancel check function
     */
    void set_cancel_check(worklist_cancel_check check);

    // =========================================================================
    // scp_service Interface Implementation
    // =========================================================================

    /**
     * @brief Get supported SOP Class UIDs
     *
     * @return Vector containing Modality Worklist Find SOP Class UID
     */
    [[nodiscard]] std::vector<std::string> supported_sop_classes() const override;

    /**
     * @brief Handle an incoming DIMSE message (MWL C-FIND-RQ)
     *
     * Processes the Modality Worklist C-FIND request, queries the worklist
     * repository through the handler, and sends pending responses for each
     * match followed by a final success.
     *
     * @param assoc The association on which the message was received
     * @param context_id The presentation context ID for the message
     * @param request The incoming C-FIND-RQ message
     * @return Success or error if the message is not a valid MWL C-FIND-RQ
     */
    [[nodiscard]] network::Result<std::monostate> handle_message(
        network::association& assoc,
        uint8_t context_id,
        const network::dimse::dimse_message& request) override;

    /**
     * @brief Get the service name
     *
     * @return "Worklist SCP"
     */
    [[nodiscard]] std::string_view service_name() const noexcept override;

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get total number of worklist queries processed
     *
     * @return Number of MWL C-FIND requests handled
     */
    [[nodiscard]] size_t queries_processed() const noexcept;

    /**
     * @brief Get total number of worklist items returned
     *
     * @return Total count of scheduled procedure items returned
     */
    [[nodiscard]] size_t items_returned() const noexcept;

    /**
     * @brief Reset statistics counters
     */
    void reset_statistics() noexcept;

private:
    // =========================================================================
    // Private Implementation
    // =========================================================================

    /**
     * @brief Send a pending C-FIND response with matching worklist item
     *
     * @param assoc The association
     * @param context_id The presentation context ID
     * @param message_id The original request message ID
     * @param result The matching worklist item dataset
     * @return Success or error
     */
    [[nodiscard]] network::Result<std::monostate> send_pending_response(
        network::association& assoc,
        uint8_t context_id,
        uint16_t message_id,
        const core::dicom_dataset& result);

    /**
     * @brief Send the final C-FIND response (success or cancel)
     *
     * @param assoc The association
     * @param context_id The presentation context ID
     * @param message_id The original request message ID
     * @param status The final status code
     * @return Success or error
     */
    [[nodiscard]] network::Result<std::monostate> send_final_response(
        network::association& assoc,
        uint8_t context_id,
        uint16_t message_id,
        network::dimse::status_code status);

    // =========================================================================
    // Member Variables
    // =========================================================================

    worklist_handler handler_;
    worklist_cancel_check cancel_check_;
    size_t max_results_{0};  // 0 = unlimited
    std::atomic<size_t> queries_processed_{0};
    std::atomic<size_t> items_returned_{0};
};

}  // namespace pacs::services

#endif  // PACS_SERVICES_WORKLIST_SCP_HPP
