/**
 * @file mpps_scu.hpp
 * @brief DICOM MPPS (Modality Performed Procedure Step) SCU service
 *
 * This file provides the mpps_scu class for reporting procedure status
 * to MPPS SCP systems via N-CREATE and N-SET operations.
 *
 * @see DICOM PS3.4 Section F - Modality Performed Procedure Step
 * @see DICOM PS3.7 Section 10 - DIMSE-N Services
 * @see Issue #534 - MPPS SCU Library Implementation
 */

#ifndef PACS_SERVICES_MPPS_SCU_HPP
#define PACS_SERVICES_MPPS_SCU_HPP

#include "mpps_scp.hpp"

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/di/ilogger.hpp"
#include "pacs/network/association.hpp"
#include "pacs/network/dimse/dimse_message.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace pacs::services {

// =============================================================================
// MPPS SCU Data Structures
// =============================================================================

/**
 * @brief Information about a performed series for N-SET COMPLETED
 *
 * Contains the details of a series that was performed during the procedure.
 */
struct performed_series_info {
    /// Series Instance UID
    std::string series_uid;

    /// Series Description
    std::string series_description;

    /// Modality type (CT, MR, US, etc.)
    std::string modality;

    /// Name of the performing physician
    std::string performing_physician;

    /// Name of the operator
    std::string operator_name;

    /// List of SOP Instance UIDs in this series
    std::vector<std::string> sop_instance_uids;

    /// Number of instances in the series
    size_t num_instances{0};
};

/**
 * @brief Data for N-CREATE operation (start procedure)
 *
 * Contains all attributes required to create a new MPPS instance
 * with IN PROGRESS status.
 */
struct mpps_create_data {
    // Scheduled Step Reference
    std::string scheduled_procedure_step_id;
    std::string study_instance_uid;
    std::string accession_number;

    // Patient Information
    std::string patient_name;
    std::string patient_id;
    std::string patient_birth_date;
    std::string patient_sex;

    // Performed Procedure Step Information
    std::string mpps_sop_instance_uid;          ///< Generated if empty
    std::string procedure_step_start_date;      ///< DICOM DA format (YYYYMMDD)
    std::string procedure_step_start_time;      ///< DICOM TM format (HHMMSS)
    std::string modality;
    std::string station_ae_title;
    std::string station_name;
    std::string procedure_description;

    // Performing Information
    std::string performing_physician;
    std::string operator_name;
};

/**
 * @brief Data for N-SET operation (update/complete procedure)
 *
 * Contains attributes to update an existing MPPS instance to
 * COMPLETED or DISCONTINUED status.
 */
struct mpps_set_data {
    /// MPPS SOP Instance UID (required)
    std::string mpps_sop_instance_uid;

    /// New status (COMPLETED or DISCONTINUED)
    mpps_status status{mpps_status::completed};

    /// Procedure Step End Date (required for COMPLETED/DISCONTINUED)
    std::string procedure_step_end_date;

    /// Procedure Step End Time (required for COMPLETED/DISCONTINUED)
    std::string procedure_step_end_time;

    /// Performed Series Sequence (for COMPLETED status)
    std::vector<performed_series_info> performed_series;

    /// Discontinuation reason (for DISCONTINUED status)
    std::string discontinuation_reason;
};

/**
 * @brief Result of an MPPS operation
 *
 * Contains information about the outcome of N-CREATE or N-SET operations.
 */
struct mpps_result {
    /// MPPS SOP Instance UID
    std::string mpps_sop_instance_uid;

    /// DIMSE status code (0x0000 = success)
    uint16_t status{0};

    /// Error comment from the SCP (if any)
    std::string error_comment;

    /// Time taken for the operation
    std::chrono::milliseconds elapsed{0};

    /// Check if the operation was successful
    [[nodiscard]] bool is_success() const noexcept {
        return status == 0x0000;
    }

    /// Check if this was a warning status
    [[nodiscard]] bool is_warning() const noexcept {
        return (status & 0xF000) == 0xB000;
    }

    /// Check if this was an error status
    [[nodiscard]] bool is_error() const noexcept {
        return !is_success() && !is_warning();
    }
};

/**
 * @brief Configuration for MPPS SCU service
 */
struct mpps_scu_config {
    /// Timeout for receiving DIMSE response
    std::chrono::milliseconds timeout{30000};

    /// Auto-generate MPPS UID if not provided
    bool auto_generate_uid{true};
};

// =============================================================================
// MPPS SCU Class
// =============================================================================

/**
 * @brief MPPS SCU service for reporting procedure status
 *
 * The MPPS SCU (Service Class User) sends N-CREATE and N-SET requests
 * to remote MPPS SCP systems (PACS, RIS) to report procedure progress.
 *
 * ## MPPS Message Flow
 *
 * ```
 * Modality (MPPS SCU)                    PACS/RIS (MPPS SCP)
 *  |                                     |
 *  |  [Exam Started]                     |
 *  |                                     |
 *  |  N-CREATE-RQ                        |
 *  |  +--------------------------+       |
 *  |  | Status: "IN PROGRESS"   |       |
 *  |  | PatientID, Modality...  |       |
 *  |  +--------------------------+       |
 *  |------------------------------------>|
 *  |                                     |
 *  |  N-CREATE-RSP (Success)             |
 *  |<------------------------------------|
 *  |                                     |
 *  |  [Exam Completed]                   |
 *  |                                     |
 *  |  N-SET-RQ                           |
 *  |  +--------------------------+       |
 *  |  | Status: "COMPLETED"     |       |
 *  |  | PerformedSeriesSequence |       |
 *  |  +--------------------------+       |
 *  |------------------------------------>|
 *  |                                     |
 *  |  N-SET-RSP (Success)                |
 *  |<------------------------------------|
 * ```
 *
 * @example Basic Usage
 * @code
 * // Establish association
 * association_config config;
 * config.calling_ae_title = "CT_SCANNER";
 * config.called_ae_title = "RIS_SCP";
 * config.proposed_contexts.push_back({
 *     1,
 *     std::string(mpps_sop_class_uid),
 *     {"1.2.840.10008.1.2.1", "1.2.840.10008.1.2"}
 * });
 *
 * auto assoc_result = association::connect("192.168.1.100", 11112, config);
 * auto& assoc = assoc_result.value();
 *
 * // Create MPPS SCU and start procedure
 * mpps_scu scu;
 *
 * mpps_create_data create_data;
 * create_data.patient_id = "12345";
 * create_data.patient_name = "Doe^John";
 * create_data.modality = "CT";
 * create_data.station_ae_title = "CT_SCANNER";
 *
 * auto create_result = scu.create(assoc, create_data);
 * if (create_result.is_ok() && create_result.value().is_success()) {
 *     std::string mpps_uid = create_result.value().mpps_sop_instance_uid;
 *
 *     // ... perform exam ...
 *
 *     // Complete the procedure
 *     performed_series_info series;
 *     series.series_uid = "1.2.3.4.5.6";
 *     series.modality = "CT";
 *     series.num_instances = 150;
 *
 *     auto complete_result = scu.complete(assoc, mpps_uid, {series});
 * }
 *
 * assoc.release();
 * @endcode
 */
class mpps_scu {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct MPPS SCU with default configuration
     *
     * @param logger Logger instance for service logging (nullptr uses null_logger)
     */
    explicit mpps_scu(std::shared_ptr<di::ILogger> logger = nullptr);

    /**
     * @brief Construct MPPS SCU with custom configuration
     *
     * @param config Configuration options
     * @param logger Logger instance for service logging (nullptr uses null_logger)
     */
    explicit mpps_scu(const mpps_scu_config& config,
                      std::shared_ptr<di::ILogger> logger = nullptr);

    ~mpps_scu() = default;

    // Non-copyable, non-movable (due to atomic members)
    mpps_scu(const mpps_scu&) = delete;
    mpps_scu& operator=(const mpps_scu&) = delete;
    mpps_scu(mpps_scu&&) = delete;
    mpps_scu& operator=(mpps_scu&&) = delete;

    // =========================================================================
    // N-CREATE Operation
    // =========================================================================

    /**
     * @brief Create a new MPPS instance (N-CREATE)
     *
     * Starts a new Modality Performed Procedure Step with IN PROGRESS status.
     * If mpps_sop_instance_uid is empty, a unique UID will be auto-generated.
     *
     * @param assoc The established association to use
     * @param data The MPPS creation data
     * @return Result containing mpps_result on success, or error message
     */
    [[nodiscard]] network::Result<mpps_result> create(
        network::association& assoc,
        const mpps_create_data& data);

    // =========================================================================
    // N-SET Operations
    // =========================================================================

    /**
     * @brief Update an existing MPPS instance (N-SET)
     *
     * Updates an MPPS instance to COMPLETED or DISCONTINUED status.
     *
     * @param assoc The established association to use
     * @param data The MPPS update data
     * @return Result containing mpps_result on success, or error message
     */
    [[nodiscard]] network::Result<mpps_result> set(
        network::association& assoc,
        const mpps_set_data& data);

    /**
     * @brief Complete an MPPS instance (convenience method)
     *
     * Updates the MPPS to COMPLETED status with performed series information.
     * Automatically fills in current date/time for end timestamps.
     *
     * @param assoc The established association to use
     * @param mpps_uid The MPPS SOP Instance UID
     * @param performed_series Information about performed series
     * @return Result containing mpps_result on success, or error message
     */
    [[nodiscard]] network::Result<mpps_result> complete(
        network::association& assoc,
        std::string_view mpps_uid,
        const std::vector<performed_series_info>& performed_series);

    /**
     * @brief Discontinue an MPPS instance (convenience method)
     *
     * Updates the MPPS to DISCONTINUED status.
     * Automatically fills in current date/time for end timestamps.
     *
     * @param assoc The established association to use
     * @param mpps_uid The MPPS SOP Instance UID
     * @param reason Optional discontinuation reason
     * @return Result containing mpps_result on success, or error message
     */
    [[nodiscard]] network::Result<mpps_result> discontinue(
        network::association& assoc,
        std::string_view mpps_uid,
        std::string_view reason = "");

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get the number of N-CREATE operations performed
     * @return Count of create operations
     */
    [[nodiscard]] size_t creates_performed() const noexcept;

    /**
     * @brief Get the number of N-SET operations performed
     * @return Count of set operations
     */
    [[nodiscard]] size_t sets_performed() const noexcept;

    /**
     * @brief Reset statistics counters to zero
     */
    void reset_statistics() noexcept;

private:
    // =========================================================================
    // Private Implementation
    // =========================================================================

    /**
     * @brief Build DICOM dataset for N-CREATE request
     */
    [[nodiscard]] core::dicom_dataset build_create_dataset(
        const mpps_create_data& data) const;

    /**
     * @brief Build DICOM dataset for N-SET request
     */
    [[nodiscard]] core::dicom_dataset build_set_dataset(
        const mpps_set_data& data) const;

    /**
     * @brief Generate a unique MPPS SOP Instance UID
     */
    [[nodiscard]] std::string generate_mpps_uid() const;

    /**
     * @brief Get current date in DICOM DA format (YYYYMMDD)
     */
    [[nodiscard]] std::string get_current_date() const;

    /**
     * @brief Get current time in DICOM TM format (HHMMSS)
     */
    [[nodiscard]] std::string get_current_time() const;

    /**
     * @brief Get the next message ID for DIMSE operations
     */
    [[nodiscard]] uint16_t next_message_id() noexcept;

    // =========================================================================
    // Private Members
    // =========================================================================

    /// Logger instance
    std::shared_ptr<di::ILogger> logger_;

    /// Configuration
    mpps_scu_config config_;

    /// Message ID counter
    std::atomic<uint16_t> message_id_counter_{1};

    /// Statistics: N-CREATE operations performed
    std::atomic<size_t> creates_performed_{0};

    /// Statistics: N-SET operations performed
    std::atomic<size_t> sets_performed_{0};
};

// =============================================================================
// Additional MPPS Tags (for SCU use)
// =============================================================================

namespace mpps_tags {

/// Performed Procedure Step Start Date (0040,0244)
inline constexpr core::dicom_tag performed_procedure_step_start_date{0x0040, 0x0244};

/// Performed Procedure Step Start Time (0040,0245)
inline constexpr core::dicom_tag performed_procedure_step_start_time{0x0040, 0x0245};

/// Performed Procedure Step Description (0040,0254)
inline constexpr core::dicom_tag performed_procedure_step_description{0x0040, 0x0254};

/// Performed Protocol Code Sequence (0040,0260)
inline constexpr core::dicom_tag performed_protocol_code_sequence{0x0040, 0x0260};

/// Retrieve AE Title (0008,0054)
inline constexpr core::dicom_tag retrieve_ae_title{0x0008, 0x0054};

/// Referenced Image Sequence (0008,1140)
inline constexpr core::dicom_tag referenced_image_sequence{0x0008, 0x1140};

/// Performing Physician's Name (0008,1050)
inline constexpr core::dicom_tag performing_physicians_name{0x0008, 0x1050};

/// Operators' Name (0008,1070)
inline constexpr core::dicom_tag operators_name{0x0008, 0x1070};

/// Series Description (0008,103E)
inline constexpr core::dicom_tag series_description{0x0008, 0x103E};

/// Performed Procedure Step Discontinuation Reason Code Sequence (0040,0281)
inline constexpr core::dicom_tag discontinuation_reason_code_sequence{0x0040, 0x0281};

}  // namespace mpps_tags

}  // namespace pacs::services

#endif  // PACS_SERVICES_MPPS_SCU_HPP
