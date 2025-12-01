/**
 * @file mpps_scp.hpp
 * @brief DICOM MPPS (Modality Performed Procedure Step) SCP service
 *
 * This file provides the mpps_scp class for handling N-CREATE and N-SET
 * requests to track exam progress from modality devices.
 *
 * @see DICOM PS3.4 Section F - MPPS SOP Class
 * @see DICOM PS3.7 Section 10 - DIMSE-N Services
 * @see DES-SVC-007 - MPPS SCP Design Specification
 */

#ifndef PACS_SERVICES_MPPS_SCP_HPP
#define PACS_SERVICES_MPPS_SCP_HPP

#include "scp_service.hpp"

#include <atomic>
#include <functional>
#include <optional>
#include <string>

namespace pacs::services {

// =============================================================================
// SOP Class UID
// =============================================================================

/// MPPS (Modality Performed Procedure Step) SOP Class UID
inline constexpr std::string_view mpps_sop_class_uid = "1.2.840.10008.3.1.2.3.3";

// =============================================================================
// MPPS Types
// =============================================================================

/**
 * @brief MPPS status enumeration
 *
 * Defines valid states for a Modality Performed Procedure Step.
 * COMPLETED and DISCONTINUED are final (terminal) states.
 */
enum class mpps_status {
    in_progress,   ///< Procedure is currently being performed
    completed,     ///< Procedure completed successfully
    discontinued   ///< Procedure was stopped/cancelled
};

/**
 * @brief Convert mpps_status to DICOM string representation
 *
 * @param status The status enum value
 * @return DICOM-compliant string ("IN PROGRESS", "COMPLETED", "DISCONTINUED")
 */
[[nodiscard]] inline auto to_string(mpps_status status) -> std::string_view {
    switch (status) {
        case mpps_status::in_progress:
            return "IN PROGRESS";
        case mpps_status::completed:
            return "COMPLETED";
        case mpps_status::discontinued:
            return "DISCONTINUED";
        default:
            return "IN PROGRESS";
    }
}

/**
 * @brief Parse DICOM string to mpps_status enum
 *
 * @param str The DICOM string representation
 * @return Optional containing the status if valid, nullopt otherwise
 */
[[nodiscard]] inline auto parse_mpps_status(std::string_view str)
    -> std::optional<mpps_status> {
    if (str == "IN PROGRESS") {
        return mpps_status::in_progress;
    }
    if (str == "COMPLETED") {
        return mpps_status::completed;
    }
    if (str == "DISCONTINUED") {
        return mpps_status::discontinued;
    }
    return std::nullopt;
}

/**
 * @brief MPPS instance data structure
 *
 * Contains information extracted from N-CREATE requests.
 */
struct mpps_instance {
    /// SOP Instance UID - unique identifier for this MPPS
    std::string sop_instance_uid;

    /// Current status (always IN PROGRESS for N-CREATE)
    mpps_status status{mpps_status::in_progress};

    /// Performing station AE Title
    std::string station_ae;

    /// Complete MPPS dataset from the request
    core::dicom_dataset data;
};

// =============================================================================
// Handler Types
// =============================================================================

/**
 * @brief N-CREATE handler function type
 *
 * Called when an N-CREATE request is received to create a new MPPS instance.
 *
 * @param instance The MPPS instance data from the N-CREATE request
 * @return Success if the instance was created, error description otherwise
 */
using mpps_create_handler = std::function<network::Result<std::monostate>(
    const mpps_instance& instance)>;

/**
 * @brief N-SET handler function type
 *
 * Called when an N-SET request is received to update an existing MPPS instance.
 *
 * @param sop_instance_uid The SOP Instance UID of the MPPS to update
 * @param modifications The dataset containing the modifications
 * @param new_status The new status (COMPLETED or DISCONTINUED)
 * @return Success if the instance was updated, error description otherwise
 */
using mpps_set_handler = std::function<network::Result<std::monostate>(
    const std::string& sop_instance_uid,
    const core::dicom_dataset& modifications,
    mpps_status new_status)>;

// =============================================================================
// MPPS SCP Class
// =============================================================================

/**
 * @brief MPPS SCP service for handling N-CREATE and N-SET requests
 *
 * The MPPS SCP (Service Class Provider) responds to MPPS N-CREATE and N-SET
 * requests from modality devices. It tracks the progress of performed
 * procedure steps and enables workflow integration with RIS/HIS systems.
 *
 * ## MPPS Message Flow
 *
 * ```
 * Modality (CT/MR/etc)                    PACS/RIS (MPPS SCP)
 *  │                                       │
 *  │  [Exam Started]                       │
 *  │                                       │
 *  │  N-CREATE-RQ                          │
 *  │  ┌───────────────────────────────────┐│
 *  │  │ AffectedSOPClass: MPPS            ││
 *  │  │ AffectedSOPInstance: 1.2.3...     ││
 *  │  │ Status: "IN PROGRESS"             ││
 *  │  │ PerformedStationAETitle: CT_01    ││
 *  │  │ PerformedProcedureStepStartDate   ││
 *  │  │ ScheduledStepAttributesSequence   ││
 *  │  └───────────────────────────────────┘│
 *  │──────────────────────────────────────►│
 *  │                                       │
 *  │                               Store   │
 *  │                               instance│
 *  │                                       │
 *  │  N-CREATE-RSP (Success)               │
 *  │◄──────────────────────────────────────│
 *  │                                       │
 *  │  [Exam Completed]                     │
 *  │                                       │
 *  │  N-SET-RQ                             │
 *  │  ┌───────────────────────────────────┐│
 *  │  │ RequestedSOPInstance: 1.2.3...    ││
 *  │  │ Status: "COMPLETED"               ││
 *  │  │ PerformedProcedureStepEndDate     ││
 *  │  │ PerformedSeriesSequence           ││
 *  │  └───────────────────────────────────┘│
 *  │──────────────────────────────────────►│
 *  │                                       │
 *  │  N-SET-RSP (Success)                  │
 *  │◄──────────────────────────────────────│
 * ```
 *
 * ## MPPS State Machine
 *
 * ```
 *     N-CREATE (status = "IN PROGRESS")
 *                   │
 *                   ▼
 *          ┌─────────────────┐
 *          │   IN PROGRESS   │
 *          └────────┬────────┘
 *                   │
 *       ┌───────────┼───────────┐
 *       │ N-SET     │     N-SET │
 *       │ COMPLETED │ DISCONTINUED
 *       ▼                       ▼
 *  ┌───────────┐       ┌──────────────┐
 *  │ COMPLETED │       │ DISCONTINUED │
 *  └───────────┘       └──────────────┘
 *
 *  Note: COMPLETED and DISCONTINUED are final states
 * ```
 *
 * @example Usage
 * @code
 * mpps_scp scp;
 *
 * // Set up N-CREATE handler
 * scp.set_create_handler([&database](const mpps_instance& inst) {
 *     return database.create_mpps(inst);
 * });
 *
 * // Set up N-SET handler
 * scp.set_set_handler([&database](const auto& uid, const auto& mods, auto status) {
 *     return database.update_mpps(uid, mods, status);
 * });
 *
 * // Handle incoming MPPS request
 * auto result = scp.handle_message(association, context_id, request);
 * @endcode
 */
class mpps_scp final : public scp_service {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    mpps_scp();
    ~mpps_scp() override = default;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Set the N-CREATE handler function
     *
     * The handler is called for each N-CREATE request to create a new
     * MPPS instance in the database or forward to RIS/HIS.
     *
     * @param handler The N-CREATE handler function
     */
    void set_create_handler(mpps_create_handler handler);

    /**
     * @brief Set the N-SET handler function
     *
     * The handler is called for each N-SET request to update an existing
     * MPPS instance with COMPLETED or DISCONTINUED status.
     *
     * @param handler The N-SET handler function
     */
    void set_set_handler(mpps_set_handler handler);

    // =========================================================================
    // scp_service Interface Implementation
    // =========================================================================

    /**
     * @brief Get supported SOP Class UIDs
     *
     * @return Vector containing MPPS SOP Class UID
     */
    [[nodiscard]] std::vector<std::string> supported_sop_classes() const override;

    /**
     * @brief Handle an incoming DIMSE message (N-CREATE-RQ or N-SET-RQ)
     *
     * Processes N-CREATE and N-SET requests for MPPS management.
     *
     * @param assoc The association on which the message was received
     * @param context_id The presentation context ID for the message
     * @param request The incoming N-CREATE-RQ or N-SET-RQ message
     * @return Success or error if the message cannot be processed
     */
    [[nodiscard]] network::Result<std::monostate> handle_message(
        network::association& assoc,
        uint8_t context_id,
        const network::dimse::dimse_message& request) override;

    /**
     * @brief Get the service name
     *
     * @return "MPPS SCP"
     */
    [[nodiscard]] std::string_view service_name() const noexcept override;

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get total number of N-CREATE requests processed
     *
     * @return Number of MPPS instances created
     */
    [[nodiscard]] size_t creates_processed() const noexcept;

    /**
     * @brief Get total number of N-SET requests processed
     *
     * @return Number of MPPS instances updated
     */
    [[nodiscard]] size_t sets_processed() const noexcept;

    /**
     * @brief Get number of MPPS completed successfully
     *
     * @return Number of MPPS with COMPLETED status
     */
    [[nodiscard]] size_t mpps_completed() const noexcept;

    /**
     * @brief Get number of MPPS discontinued
     *
     * @return Number of MPPS with DISCONTINUED status
     */
    [[nodiscard]] size_t mpps_discontinued() const noexcept;

    /**
     * @brief Reset statistics counters
     */
    void reset_statistics() noexcept;

private:
    // =========================================================================
    // Private Implementation
    // =========================================================================

    /**
     * @brief Handle N-CREATE request
     *
     * Creates a new MPPS instance with IN PROGRESS status.
     *
     * @param assoc The association
     * @param context_id The presentation context ID
     * @param request The N-CREATE-RQ message
     * @return Success or error
     */
    [[nodiscard]] network::Result<std::monostate> handle_n_create(
        network::association& assoc,
        uint8_t context_id,
        const network::dimse::dimse_message& request);

    /**
     * @brief Handle N-SET request
     *
     * Updates an existing MPPS instance to COMPLETED or DISCONTINUED.
     *
     * @param assoc The association
     * @param context_id The presentation context ID
     * @param request The N-SET-RQ message
     * @return Success or error
     */
    [[nodiscard]] network::Result<std::monostate> handle_n_set(
        network::association& assoc,
        uint8_t context_id,
        const network::dimse::dimse_message& request);

    /**
     * @brief Send N-CREATE response
     *
     * @param assoc The association
     * @param context_id The presentation context ID
     * @param message_id The original request message ID
     * @param sop_instance_uid The SOP Instance UID
     * @param status The response status code
     * @return Success or error
     */
    [[nodiscard]] network::Result<std::monostate> send_n_create_response(
        network::association& assoc,
        uint8_t context_id,
        uint16_t message_id,
        const std::string& sop_instance_uid,
        network::dimse::status_code status);

    /**
     * @brief Send N-SET response
     *
     * @param assoc The association
     * @param context_id The presentation context ID
     * @param message_id The original request message ID
     * @param sop_instance_uid The SOP Instance UID
     * @param status The response status code
     * @return Success or error
     */
    [[nodiscard]] network::Result<std::monostate> send_n_set_response(
        network::association& assoc,
        uint8_t context_id,
        uint16_t message_id,
        const std::string& sop_instance_uid,
        network::dimse::status_code status);

    // =========================================================================
    // Member Variables
    // =========================================================================

    mpps_create_handler create_handler_;
    mpps_set_handler set_handler_;

    std::atomic<size_t> creates_processed_{0};
    std::atomic<size_t> sets_processed_{0};
    std::atomic<size_t> mpps_completed_{0};
    std::atomic<size_t> mpps_discontinued_{0};
};

// =============================================================================
// MPPS DICOM Tags (Group 0x0040)
// =============================================================================

namespace mpps_tags {

/// Performed Station AE Title (0040,0241)
inline constexpr core::dicom_tag performed_station_ae_title{0x0040, 0x0241};

/// Performed Station Name (0040,0242)
inline constexpr core::dicom_tag performed_station_name{0x0040, 0x0242};

/// Performed Location (0040,0243)
inline constexpr core::dicom_tag performed_location{0x0040, 0x0243};

/// Performed Procedure Step End Date (0040,0250)
inline constexpr core::dicom_tag performed_procedure_step_end_date{0x0040, 0x0250};

/// Performed Procedure Step End Time (0040,0251)
inline constexpr core::dicom_tag performed_procedure_step_end_time{0x0040, 0x0251};

/// Performed Procedure Step Status (0040,0252)
inline constexpr core::dicom_tag performed_procedure_step_status{0x0040, 0x0252};

/// Performed Procedure Step ID (0040,0253)
inline constexpr core::dicom_tag performed_procedure_step_id{0x0040, 0x0253};

/// Performed Series Sequence (0040,0340)
inline constexpr core::dicom_tag performed_series_sequence{0x0040, 0x0340};

/// Scheduled Step Attributes Sequence (0040,0270)
inline constexpr core::dicom_tag scheduled_step_attributes_sequence{0x0040, 0x0270};

/// Referenced Study Sequence (0008,1110)
inline constexpr core::dicom_tag referenced_study_sequence{0x0008, 0x1110};

}  // namespace mpps_tags

}  // namespace pacs::services

#endif  // PACS_SERVICES_MPPS_SCP_HPP
