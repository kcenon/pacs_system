/**
 * @file ups_push_scu.hpp
 * @brief DICOM UPS (Unified Procedure Step) Push SCU service
 *
 * This file provides the ups_push_scu class for creating and managing
 * UPS workitems on remote SCP servers via N-CREATE, N-SET, N-GET,
 * and N-ACTION operations.
 *
 * @see DICOM PS3.4 Annex CC - Unified Procedure Step Service
 * @see DICOM PS3.7 Section 10 - DIMSE-N Services
 * @see Issue #809 - UPS Push SCU Implementation
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_SERVICES_UPS_PUSH_SCU_HPP
#define PACS_SERVICES_UPS_PUSH_SCU_HPP

#include "ups_push_scp.hpp"

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/di/ilogger.hpp"
#include "pacs/network/association.hpp"
#include "pacs/network/dimse/dimse_message.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace kcenon::pacs::services {

// =============================================================================
// UPS SCU Data Structures
// =============================================================================

/**
 * @brief Data for N-CREATE operation (create new workitem)
 *
 * Contains attributes required to create a new UPS workitem with
 * SCHEDULED state on a remote SCP.
 */
struct ups_create_data {
    /// Workitem SOP Instance UID (generated if empty)
    std::string workitem_uid;

    /// Procedure Step Label (required)
    std::string procedure_step_label;

    /// Worklist Label
    std::string worklist_label;

    /// Priority: LOW, MEDIUM, HIGH
    std::string priority{"MEDIUM"};

    /// Scheduled start date/time (DICOM DT format)
    std::string scheduled_start_datetime;

    /// Expected completion date/time (DICOM DT format)
    std::string expected_completion_datetime;

    /// Scheduled Station Name AE
    std::string scheduled_station_name;

    /// Input Information (JSON serialized sequence data)
    std::string input_information;
};

/**
 * @brief Data for N-SET operation (modify workitem attributes)
 *
 * Contains modifications to apply to an existing UPS workitem.
 */
struct ups_set_data {
    /// Workitem SOP Instance UID (required)
    std::string workitem_uid;

    /// Modification dataset
    core::dicom_dataset modifications;
};

/**
 * @brief Data for N-GET operation (retrieve workitem)
 */
struct ups_get_data {
    /// Workitem SOP Instance UID (required)
    std::string workitem_uid;

    /// Specific attribute tags to retrieve (empty = all)
    std::vector<core::dicom_tag> attribute_tags;
};

/**
 * @brief Data for N-ACTION Type 1 (change UPS state)
 */
struct ups_change_state_data {
    /// Workitem SOP Instance UID (required)
    std::string workitem_uid;

    /// Requested new state: "IN PROGRESS", "COMPLETED", "CANCELED"
    std::string requested_state;

    /// Transaction UID (required for claiming/completing/canceling)
    std::string transaction_uid;
};

/**
 * @brief Data for N-ACTION Type 3 (request cancellation)
 */
struct ups_request_cancel_data {
    /// Workitem SOP Instance UID (required)
    std::string workitem_uid;

    /// Reason for cancellation request (optional)
    std::string reason;
};

/**
 * @brief Result of a UPS SCU operation
 *
 * Contains information about the outcome of any UPS DIMSE-N operation.
 */
struct ups_result {
    /// Workitem SOP Instance UID
    std::string workitem_uid;

    /// DIMSE status code (0x0000 = success)
    uint16_t status{0};

    /// Error comment from the SCP (if any)
    std::string error_comment;

    /// Response dataset (for N-GET operations)
    core::dicom_dataset attributes;

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
 * @brief Configuration for UPS Push SCU service
 */
struct ups_push_scu_config {
    /// Timeout for receiving DIMSE response
    std::chrono::milliseconds timeout{30000};

    /// Auto-generate workitem UID if not provided
    bool auto_generate_uid{true};
};

// =============================================================================
// UPS Push SCU Class
// =============================================================================

/**
 * @brief UPS Push SCU service for managing workitems on remote systems
 *
 * The UPS Push SCU (Service Class User) sends N-CREATE, N-SET, N-GET,
 * and N-ACTION requests to remote UPS Push SCP servers to create and
 * manage UPS workitems.
 *
 * ## UPS Push SCU Message Flow
 *
 * ```
 * UPS Push SCU (This PACS)                  Remote UPS Push SCP
 *  |                                        |
 *  |  N-CREATE-RQ (Create workitem)         |
 *  |  state = "SCHEDULED"                   |
 *  |--------------------------------------->|
 *  |  N-CREATE-RSP                          |
 *  |<---------------------------------------|
 *  |                                        |
 *  |  N-ACTION-RQ (Claim: Type 1)           |
 *  |  state = "IN PROGRESS"                 |
 *  |--------------------------------------->|
 *  |  N-ACTION-RSP                          |
 *  |<---------------------------------------|
 *  |                                        |
 *  |  N-GET-RQ (Query progress)             |
 *  |--------------------------------------->|
 *  |  N-GET-RSP + attributes                |
 *  |<---------------------------------------|
 *  |                                        |
 *  |  N-ACTION-RQ (Complete: Type 1)        |
 *  |  state = "COMPLETED"                   |
 *  |--------------------------------------->|
 *  |  N-ACTION-RSP                          |
 *  |<---------------------------------------|
 * ```
 *
 * @example Basic Usage
 * @code
 * ups_push_scu scu;
 *
 * // Create a workitem
 * ups_create_data create_data;
 * create_data.procedure_step_label = "AI Analysis";
 * create_data.priority = "HIGH";
 *
 * auto result = scu.create(assoc, create_data);
 * if (result.is_ok() && result.value().is_success()) {
 *     auto uid = result.value().workitem_uid;
 *
 *     // Claim the workitem
 *     ups_change_state_data claim;
 *     claim.workitem_uid = uid;
 *     claim.requested_state = "IN PROGRESS";
 *     claim.transaction_uid = "1.2.3.4.5";
 *     scu.change_state(assoc, claim);
 * }
 * @endcode
 */
class ups_push_scu {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct UPS Push SCU with default configuration
     *
     * @param logger Logger instance (nullptr uses null_logger)
     */
    explicit ups_push_scu(std::shared_ptr<di::ILogger> logger = nullptr);

    /**
     * @brief Construct UPS Push SCU with custom configuration
     *
     * @param config Configuration options
     * @param logger Logger instance (nullptr uses null_logger)
     */
    explicit ups_push_scu(const ups_push_scu_config& config,
                          std::shared_ptr<di::ILogger> logger = nullptr);

    ~ups_push_scu() = default;

    // Non-copyable, non-movable (due to atomic members)
    ups_push_scu(const ups_push_scu&) = delete;
    ups_push_scu& operator=(const ups_push_scu&) = delete;
    ups_push_scu(ups_push_scu&&) = delete;
    ups_push_scu& operator=(ups_push_scu&&) = delete;

    // =========================================================================
    // N-CREATE Operation
    // =========================================================================

    /**
     * @brief Create a new UPS workitem on the remote SCP (N-CREATE)
     *
     * Creates a new workitem with SCHEDULED state. If workitem_uid is
     * empty and auto_generate_uid is true, a unique UID will be generated.
     *
     * @param assoc The established association to use
     * @param data The workitem creation data
     * @return Result containing ups_result on success, or error
     */
    [[nodiscard]] network::Result<ups_result> create(
        network::association& assoc,
        const ups_create_data& data);

    // =========================================================================
    // N-SET Operation
    // =========================================================================

    /**
     * @brief Modify an existing UPS workitem (N-SET)
     *
     * @param assoc The established association to use
     * @param data The modification data including workitem UID and dataset
     * @return Result containing ups_result on success, or error
     */
    [[nodiscard]] network::Result<ups_result> set(
        network::association& assoc,
        const ups_set_data& data);

    // =========================================================================
    // N-GET Operation
    // =========================================================================

    /**
     * @brief Retrieve UPS workitem attributes from remote SCP (N-GET)
     *
     * @param assoc The established association to use
     * @param data The query data including workitem UID and optional tags
     * @return Result containing ups_result with attributes on success
     */
    [[nodiscard]] network::Result<ups_result> get(
        network::association& assoc,
        const ups_get_data& data);

    // =========================================================================
    // N-ACTION Operations
    // =========================================================================

    /**
     * @brief Change UPS workitem state (N-ACTION Type 1)
     *
     * Requests a state transition for the specified workitem.
     * Valid transitions: SCHEDULED→IN PROGRESS, IN PROGRESS→COMPLETED,
     * IN PROGRESS→CANCELED, SCHEDULED→CANCELED.
     *
     * @param assoc The established association to use
     * @param data The state change data including Transaction UID
     * @return Result containing ups_result on success, or error
     */
    [[nodiscard]] network::Result<ups_result> change_state(
        network::association& assoc,
        const ups_change_state_data& data);

    /**
     * @brief Request cancellation of a UPS workitem (N-ACTION Type 3)
     *
     * @param assoc The established association to use
     * @param data The cancellation request data
     * @return Result containing ups_result on success, or error
     */
    [[nodiscard]] network::Result<ups_result> request_cancel(
        network::association& assoc,
        const ups_request_cancel_data& data);

    // =========================================================================
    // Statistics
    // =========================================================================

    [[nodiscard]] size_t creates_performed() const noexcept;
    [[nodiscard]] size_t sets_performed() const noexcept;
    [[nodiscard]] size_t gets_performed() const noexcept;
    [[nodiscard]] size_t actions_performed() const noexcept;

    void reset_statistics() noexcept;

private:
    // =========================================================================
    // Private Implementation
    // =========================================================================

    [[nodiscard]] core::dicom_dataset build_create_dataset(
        const ups_create_data& data) const;

    [[nodiscard]] core::dicom_dataset build_change_state_dataset(
        const ups_change_state_data& data) const;

    [[nodiscard]] core::dicom_dataset build_request_cancel_dataset(
        const ups_request_cancel_data& data) const;

    [[nodiscard]] std::string generate_workitem_uid() const;

    [[nodiscard]] uint16_t next_message_id() noexcept;

    // =========================================================================
    // Private Members
    // =========================================================================

    std::shared_ptr<di::ILogger> logger_;
    ups_push_scu_config config_;
    std::atomic<uint16_t> message_id_counter_{1};
    std::atomic<size_t> creates_performed_{0};
    std::atomic<size_t> sets_performed_{0};
    std::atomic<size_t> gets_performed_{0};
    std::atomic<size_t> actions_performed_{0};
};

}  // namespace kcenon::pacs::services

#endif  // PACS_SERVICES_UPS_PUSH_SCU_HPP
