/**
 * @file worklist_scu.hpp
 * @brief DICOM Modality Worklist SCU service (MWL C-FIND sender)
 *
 * This file provides the worklist_scu class for performing Modality Worklist
 * C-FIND queries to RIS/HIS systems. It supports typed query keys and
 * convenience methods for common worklist queries.
 *
 * @see DICOM PS3.4 Section K - Basic Worklist Management Service Class
 * @see DICOM PS3.7 Section 9.1.2 - C-FIND Service
 * @see IHE Radiology Technical Framework - Scheduled Workflow
 * @see Issue #533 - Implement worklist_scu Library (MWL SCU)
 */

#ifndef PACS_SERVICES_WORKLIST_SCU_HPP
#define PACS_SERVICES_WORKLIST_SCU_HPP

#include "worklist_scp.hpp"
#include "pacs/core/dicom_dataset.hpp"
#include "pacs/network/association.hpp"
#include "pacs/network/dimse/dimse_message.hpp"
#include "pacs/di/ilogger.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace pacs::services {

// =============================================================================
// Worklist Query Keys Structure
// =============================================================================

/**
 * @brief Typed query keys for Modality Worklist queries
 *
 * This structure provides named fields for common MWL query attributes,
 * making it easier to construct worklist queries without dealing with
 * DICOM tags directly.
 */
struct worklist_query_keys {
    // =========================================================================
    // Scheduled Procedure Step Attributes
    // =========================================================================

    /// Scheduled Station AE Title (0040,0001)
    std::string scheduled_station_ae;

    /// Modality (0008,0060) - e.g., CT, MR, US, XR
    std::string modality;

    /// Scheduled Procedure Step Start Date (0040,0002) - YYYYMMDD or range
    std::string scheduled_date;

    /// Scheduled Procedure Step Start Time (0040,0003) - HHMMSS or range
    std::string scheduled_time;

    /// Scheduled Performing Physician's Name (0040,0006)
    std::string scheduled_physician;

    /// Scheduled Procedure Step ID (0040,0009)
    std::string scheduled_procedure_step_id;

    // =========================================================================
    // Requested Procedure Attributes
    // =========================================================================

    /// Requested Procedure ID (0040,1001)
    std::string requested_procedure_id;

    /// Requested Procedure Description (0032,1060)
    std::string requested_procedure_description;

    // =========================================================================
    // Patient Attributes
    // =========================================================================

    /// Patient's Name (0010,0010) - supports wildcards (* ?)
    std::string patient_name;

    /// Patient ID (0010,0020)
    std::string patient_id;

    /// Patient's Birth Date (0010,0030)
    std::string patient_birth_date;

    /// Patient's Sex (0010,0040) - M, F, O
    std::string patient_sex;

    // =========================================================================
    // Visit Attributes
    // =========================================================================

    /// Accession Number (0008,0050)
    std::string accession_number;

    /// Referring Physician's Name (0008,0090)
    std::string referring_physician;

    /// Institution Name (0008,0080)
    std::string institution;
};

// =============================================================================
// Worklist Item Structure
// =============================================================================

/**
 * @brief Parsed worklist item from MWL query response
 *
 * Contains all relevant fields from a worklist response, properly parsed
 * and organized for application use.
 */
struct worklist_item {
    // =========================================================================
    // Patient Demographics
    // =========================================================================

    /// Patient's Name (0010,0010)
    std::string patient_name;

    /// Patient ID (0010,0020)
    std::string patient_id;

    /// Patient's Birth Date (0010,0030)
    std::string patient_birth_date;

    /// Patient's Sex (0010,0040)
    std::string patient_sex;

    // =========================================================================
    // Scheduled Procedure Step
    // =========================================================================

    /// Scheduled Station AE Title (0040,0001)
    std::string scheduled_station_ae;

    /// Modality (0008,0060)
    std::string modality;

    /// Scheduled Procedure Step Start Date (0040,0002)
    std::string scheduled_date;

    /// Scheduled Procedure Step Start Time (0040,0003)
    std::string scheduled_time;

    /// Scheduled Procedure Step ID (0040,0009)
    std::string scheduled_procedure_step_id;

    /// Scheduled Procedure Step Description (0040,0007)
    std::string scheduled_procedure_step_description;

    // =========================================================================
    // Requested Procedure
    // =========================================================================

    /// Study Instance UID (0020,000D) - Pre-assigned Study UID
    std::string study_instance_uid;

    /// Accession Number (0008,0050)
    std::string accession_number;

    /// Requested Procedure ID (0040,1001)
    std::string requested_procedure_id;

    /// Requested Procedure Description (0032,1060)
    std::string requested_procedure_description;

    // =========================================================================
    // Visit Information
    // =========================================================================

    /// Referring Physician's Name (0008,0090)
    std::string referring_physician;

    /// Institution Name (0008,0080)
    std::string institution;

    // =========================================================================
    // Original Dataset
    // =========================================================================

    /// Original dataset for full access to all attributes
    core::dicom_dataset dataset;
};

// =============================================================================
// Worklist Result Structure
// =============================================================================

/**
 * @brief Result of a Modality Worklist query operation
 *
 * Contains parsed worklist items and metadata about the query execution.
 */
struct worklist_result {
    /// Parsed worklist items from the query
    std::vector<worklist_item> items;

    /// Final DIMSE status code (0x0000 = success)
    uint16_t status{0};

    /// Query execution time
    std::chrono::milliseconds elapsed{0};

    /// Total pending responses received (may differ from items.size()
    /// if max_results was enforced)
    size_t total_pending{0};

    /// Check if the query was successful
    [[nodiscard]] bool is_success() const noexcept {
        return status == 0x0000;
    }

    /// Check if the query was cancelled
    [[nodiscard]] bool is_cancelled() const noexcept {
        return status == 0xFE00;
    }
};

// =============================================================================
// Worklist SCU Configuration
// =============================================================================

/**
 * @brief Configuration for Worklist SCU service
 */
struct worklist_scu_config {
    /// Timeout for receiving query responses (milliseconds)
    std::chrono::milliseconds timeout{30000};

    /// Maximum number of results to return (0 = unlimited)
    size_t max_results{0};

    /// Send C-CANCEL when max_results is reached
    bool cancel_on_max{true};
};

// =============================================================================
// Streaming Callback Type
// =============================================================================

/**
 * @brief Callback type for streaming worklist query results
 *
 * Called for each pending response received from the SCP.
 *
 * @param item The parsed worklist item from the SCP
 * @return true to continue receiving, false to cancel the query
 */
using worklist_streaming_callback = std::function<bool(const worklist_item&)>;

// =============================================================================
// Worklist SCU Class
// =============================================================================

/**
 * @brief Worklist SCU service for performing Modality Worklist queries
 *
 * The Worklist SCU (Service Class User) sends C-FIND requests to RIS/HIS
 * servers to retrieve scheduled procedure information for modalities.
 *
 * ## MWL C-FIND Message Flow
 *
 * ```
 * This Application (SCU)                   RIS/HIS (SCP)
 *  |                                        |
 *  |  C-FIND-RQ                             |
 *  |  +--------------------------------+    |
 *  |  | SOPClass: MWL Find             |    |
 *  |  | ScheduledStationAETitle: CT_01 |    |
 *  |  | ScheduledProcedureStepStartDate|    |
 *  |  | Modality: CT                   |    |
 *  |  +--------------------------------+    |
 *  |--------------------------------------->|
 *  |                                        |
 *  |                             Query RIS  |
 *  |                             (N items)  |
 *  |                                        |
 *  |  C-FIND-RSP (Pending)                  |
 *  |  +--------------------------------+    |
 *  |  | Status: 0xFF00 (Pending)       |    |
 *  |  | PatientName: DOE^JOHN          |    |
 *  |  | PatientID: 12345               |    |
 *  |  | StudyInstanceUID: 1.2.3...     |    |
 *  |  | AccessionNumber: ACC001        |    |
 *  |  +--------------------------------+    |
 *  |<---------------------------------------|
 *  |                                        |
 *  |  ... (repeat for each scheduled item)  |
 *  |                                        |
 *  |  C-FIND-RSP (Success)                  |
 *  |  +--------------------------------+    |
 *  |  | Status: 0x0000 (Success)       |    |
 *  |  +--------------------------------+    |
 *  |<---------------------------------------|
 * ```
 *
 * @example Basic Usage
 * @code
 * // Establish association with MWL presentation context
 * association_config config;
 * config.calling_ae_title = "MY_MODALITY";
 * config.called_ae_title = "RIS_SCP";
 * config.proposed_contexts.push_back({
 *     1,
 *     std::string(worklist_find_sop_class_uid),
 *     {"1.2.840.10008.1.2.1"}  // Explicit VR LE
 * });
 *
 * auto assoc_result = association::connect("192.168.1.100", 104, config);
 * if (assoc_result.is_err()) {
 *     return;
 * }
 * auto& assoc = assoc_result.value();
 *
 * // Query today's worklist for a CT scanner
 * worklist_scu scu;
 * auto result = scu.query_today(assoc, "CT_SCANNER_01", "CT");
 *
 * if (result.is_ok() && result.value().is_success()) {
 *     for (const auto& item : result.value().items) {
 *         std::cout << "Patient: " << item.patient_name
 *                   << " (" << item.patient_id << ")\n";
 *         std::cout << "Scheduled: " << item.scheduled_date
 *                   << " " << item.scheduled_time << "\n";
 *     }
 * }
 *
 * assoc.release();
 * @endcode
 *
 * @example Streaming Query for Large Worklists
 * @code
 * worklist_scu scu;
 * worklist_query_keys keys;
 * keys.scheduled_date = "20240101-20241231";
 *
 * size_t count = 0;
 * auto result = scu.query_streaming(assoc, keys,
 *     [&count](const worklist_item& item) {
 *         ++count;
 *         // Process each item as it arrives
 *         process_worklist_item(item);
 *         return count < 1000;  // Stop after 1000 items
 *     });
 * @endcode
 */
class worklist_scu {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct a Worklist SCU with default configuration
     *
     * @param logger Logger instance for service logging (nullptr uses null_logger)
     */
    explicit worklist_scu(std::shared_ptr<di::ILogger> logger = nullptr);

    /**
     * @brief Construct a Worklist SCU with custom configuration
     *
     * @param config Configuration options
     * @param logger Logger instance for service logging (nullptr uses null_logger)
     */
    explicit worklist_scu(const worklist_scu_config& config,
                          std::shared_ptr<di::ILogger> logger = nullptr);

    ~worklist_scu() = default;

    // Non-copyable, non-movable (due to atomic members)
    worklist_scu(const worklist_scu&) = delete;
    worklist_scu& operator=(const worklist_scu&) = delete;
    worklist_scu(worklist_scu&&) = delete;
    worklist_scu& operator=(worklist_scu&&) = delete;

    // =========================================================================
    // Generic Query Operations
    // =========================================================================

    /**
     * @brief Perform a MWL C-FIND query with typed keys
     *
     * Sends a C-FIND request with the provided query keys and collects
     * all matching worklist items from the SCP.
     *
     * @param assoc The established association to use
     * @param keys The typed query keys for filtering
     * @return Result containing worklist_result on success, or error message
     */
    [[nodiscard]] network::Result<worklist_result> query(
        network::association& assoc,
        const worklist_query_keys& keys);

    /**
     * @brief Perform a MWL C-FIND query with raw dataset
     *
     * Sends a C-FIND request with the provided raw query dataset.
     * Use this when you need full control over the query attributes.
     *
     * @param assoc The established association to use
     * @param query_keys The DICOM dataset containing query keys
     * @return Result containing worklist_result on success, or error message
     */
    [[nodiscard]] network::Result<worklist_result> query(
        network::association& assoc,
        const core::dicom_dataset& query_keys);

    // =========================================================================
    // Convenience Query Methods
    // =========================================================================

    /**
     * @brief Query today's worklist for a station
     *
     * Convenience method to query scheduled procedures for today,
     * optionally filtered by station AE title and modality.
     *
     * @param assoc The established association to use
     * @param station_ae Scheduled Station AE Title (empty = any)
     * @param modality Modality filter (empty = any)
     * @return Result containing worklist_result on success
     */
    [[nodiscard]] network::Result<worklist_result> query_today(
        network::association& assoc,
        std::string_view station_ae,
        std::string_view modality = "");

    /**
     * @brief Query worklist by date range
     *
     * Query scheduled procedures within a date range,
     * optionally filtered by modality.
     *
     * @param assoc The established association to use
     * @param start_date Start date in YYYYMMDD format
     * @param end_date End date in YYYYMMDD format
     * @param modality Modality filter (empty = any)
     * @return Result containing worklist_result on success
     */
    [[nodiscard]] network::Result<worklist_result> query_date_range(
        network::association& assoc,
        std::string_view start_date,
        std::string_view end_date,
        std::string_view modality = "");

    /**
     * @brief Query worklist by patient ID
     *
     * Query all scheduled procedures for a specific patient.
     *
     * @param assoc The established association to use
     * @param patient_id The patient ID to query
     * @return Result containing worklist_result on success
     */
    [[nodiscard]] network::Result<worklist_result> query_patient(
        network::association& assoc,
        std::string_view patient_id);

    // =========================================================================
    // Streaming Query
    // =========================================================================

    /**
     * @brief Perform a streaming MWL query for large worklists
     *
     * Sends a C-FIND request and calls the callback for each pending
     * response. This is more memory-efficient for large worklists.
     *
     * @param assoc The established association to use
     * @param keys The typed query keys for filtering
     * @param callback Called for each worklist item; return false to cancel
     * @return Result containing the number of items processed, or error
     */
    [[nodiscard]] network::Result<size_t> query_streaming(
        network::association& assoc,
        const worklist_query_keys& keys,
        worklist_streaming_callback callback);

    // =========================================================================
    // C-CANCEL Support
    // =========================================================================

    /**
     * @brief Send a C-CANCEL request to stop an ongoing query
     *
     * @param assoc The association on which the query is running
     * @param message_id The message ID of the query to cancel
     * @return Success or error
     */
    [[nodiscard]] network::Result<std::monostate> cancel(
        network::association& assoc,
        uint16_t message_id);

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Update the SCU configuration
     *
     * @param config New configuration options
     */
    void set_config(const worklist_scu_config& config);

    /**
     * @brief Get the current configuration
     *
     * @return Reference to the current configuration
     */
    [[nodiscard]] const worklist_scu_config& config() const noexcept;

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get the number of queries performed since construction
     * @return Count of C-FIND requests sent
     */
    [[nodiscard]] size_t queries_performed() const noexcept;

    /**
     * @brief Get the total number of items received since construction
     * @return Total count of worklist items received
     */
    [[nodiscard]] size_t total_items() const noexcept;

    /**
     * @brief Reset statistics counters to zero
     */
    void reset_statistics() noexcept;

private:
    // =========================================================================
    // Private Implementation
    // =========================================================================

    /**
     * @brief Parse a worklist item from a response dataset
     */
    [[nodiscard]] worklist_item parse_worklist_item(
        const core::dicom_dataset& ds) const;

    /**
     * @brief Build query dataset from typed keys
     */
    [[nodiscard]] core::dicom_dataset build_query_dataset(
        const worklist_query_keys& keys) const;

    /**
     * @brief Get the next message ID for DIMSE operations
     */
    [[nodiscard]] uint16_t next_message_id() noexcept;

    /**
     * @brief Internal query implementation
     */
    [[nodiscard]] network::Result<worklist_result> query_impl(
        network::association& assoc,
        const core::dicom_dataset& query_keys,
        uint16_t message_id);

    /**
     * @brief Get today's date in DICOM format (YYYYMMDD)
     */
    [[nodiscard]] static std::string get_today_date();

    // =========================================================================
    // Private Members
    // =========================================================================

    /// Logger instance for service logging
    std::shared_ptr<di::ILogger> logger_;

    /// Configuration
    worklist_scu_config config_;

    /// Message ID counter
    std::atomic<uint16_t> message_id_counter_{1};

    /// Statistics: number of queries performed
    std::atomic<size_t> queries_performed_{0};

    /// Statistics: total number of items received
    std::atomic<size_t> total_items_{0};
};

}  // namespace pacs::services

#endif  // PACS_SERVICES_WORKLIST_SCU_HPP
