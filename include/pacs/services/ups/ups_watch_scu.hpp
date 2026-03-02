/**
 * @file ups_watch_scu.hpp
 * @brief DICOM UPS Watch SCU service (subscription client and event receiver)
 *
 * This file provides the ups_watch_scu class for subscribing to UPS workitem
 * events on remote SCP servers and receiving N-EVENT-REPORT notifications.
 *
 * @see DICOM PS3.4 Annex CC.2.3 - UPS Watch SOP Class
 * @see DICOM PS3.4 CC.2.4 - N-EVENT-REPORT for UPS
 * @see Issue #815 - UPS Watch SCU Implementation
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_SERVICES_UPS_WATCH_SCU_HPP
#define PACS_SERVICES_UPS_WATCH_SCU_HPP

#include "ups_push_scu.hpp"
#include "ups_watch_scp.hpp"

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/di/ilogger.hpp"
#include "pacs/network/association.hpp"
#include "pacs/network/dimse/dimse_message.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <string>

namespace pacs::services {

// =============================================================================
// UPS Watch SCU Data Structures
// =============================================================================

/**
 * @brief Data for N-ACTION subscribe request
 *
 * Contains parameters for subscribing to UPS workitem events on a remote SCP.
 */
struct ups_subscribe_data {
    /// Workitem SOP Instance UID (empty for global subscription)
    std::string workitem_uid;

    /// Whether to request a deletion lock
    bool deletion_lock{false};
};

/**
 * @brief Data for N-ACTION unsubscribe request
 */
struct ups_unsubscribe_data {
    /// Workitem SOP Instance UID (empty for global unsubscription)
    std::string workitem_uid;
};

/**
 * @brief Parsed UPS event notification received from SCP
 *
 * Represents the content of an N-EVENT-REPORT received from the Watch SCP.
 */
struct ups_watch_event {
    /// Event type ID (ups_event_state_report, ups_event_cancel_requested, etc.)
    uint16_t event_type_id{0};

    /// Workitem SOP Instance UID that triggered the event
    std::string workitem_uid;

    /// New procedure step state (for state report events)
    std::string procedure_step_state;

    /// Progress percentage (for progress report events)
    std::string progress;

    /// Cancellation reason (for cancel requested events)
    std::string cancellation_reason;

    /// Full event dataset (for custom processing)
    core::dicom_dataset event_info;

    /// Timestamp when the event was received
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief Configuration for UPS Watch SCU service
 */
struct ups_watch_scu_config {
    /// Timeout for receiving DIMSE response
    std::chrono::milliseconds timeout{30000};
};

// =============================================================================
// UPS Watch SCU Class
// =============================================================================

/**
 * @brief UPS Watch SCU service for subscribing to events on remote systems
 *
 * The UPS Watch SCU (Service Class User) sends N-ACTION requests to remote
 * UPS Watch SCP servers to subscribe/unsubscribe from workitem events, and
 * handles incoming N-EVENT-REPORT notifications.
 *
 * ## UPS Watch SCU Message Flow
 *
 * ```
 * UPS Watch SCU (This PACS)               Remote UPS Watch SCP
 *  |                                       |
 *  |  N-ACTION-RQ (Subscribe, Type 3)      |
 *  |  WorkitemUID: 1.2.3.4...             |
 *  |-------------------------------------->|
 *  |  N-ACTION-RSP (Success)              |
 *  |<--------------------------------------|
 *  |                                       |
 *  |  [Workitem state changes on SCP]      |
 *  |                                       |
 *  |  N-EVENT-REPORT-RQ (Type 1)          |
 *  |  State: COMPLETED                    |
 *  |<--------------------------------------|
 *  |  N-EVENT-REPORT-RSP                  |
 *  |-------------------------------------->|
 *  |                                       |
 *  |  N-ACTION-RQ (Unsubscribe, Type 4)   |
 *  |-------------------------------------->|
 *  |  N-ACTION-RSP (Success)              |
 *  |<--------------------------------------|
 * ```
 */
class ups_watch_scu {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct UPS Watch SCU with default configuration
     *
     * @param logger Logger instance (nullptr uses null_logger)
     */
    explicit ups_watch_scu(std::shared_ptr<di::ILogger> logger = nullptr);

    /**
     * @brief Construct UPS Watch SCU with custom configuration
     *
     * @param config Configuration options
     * @param logger Logger instance (nullptr uses null_logger)
     */
    explicit ups_watch_scu(const ups_watch_scu_config& config,
                           std::shared_ptr<di::ILogger> logger = nullptr);

    ~ups_watch_scu() = default;

    // Non-copyable, non-movable (due to atomic members)
    ups_watch_scu(const ups_watch_scu&) = delete;
    ups_watch_scu& operator=(const ups_watch_scu&) = delete;
    ups_watch_scu(ups_watch_scu&&) = delete;
    ups_watch_scu& operator=(ups_watch_scu&&) = delete;

    // =========================================================================
    // Callback Configuration
    // =========================================================================

    /**
     * @brief Callback type for received event notifications
     *
     * Invoked when an N-EVENT-REPORT is received from the SCP.
     */
    using event_received_callback = std::function<void(
        const ups_watch_event& event)>;

    /**
     * @brief Set callback for received event notifications
     * @param cb The callback function
     */
    void set_event_received_callback(event_received_callback cb);

    // =========================================================================
    // N-ACTION Operations
    // =========================================================================

    /**
     * @brief Subscribe to UPS workitem events (N-ACTION Type 3)
     *
     * Sends a subscribe request to the remote UPS Watch SCP.
     * Use empty workitem_uid for global subscription.
     *
     * @param assoc The established association to use
     * @param data The subscription parameters
     * @return Result containing ups_result on success, or error
     */
    [[nodiscard]] network::Result<ups_result> subscribe(
        network::association& assoc,
        const ups_subscribe_data& data);

    /**
     * @brief Unsubscribe from UPS workitem events (N-ACTION Type 4)
     *
     * @param assoc The established association to use
     * @param data The unsubscription parameters
     * @return Result containing ups_result on success, or error
     */
    [[nodiscard]] network::Result<ups_result> unsubscribe(
        network::association& assoc,
        const ups_unsubscribe_data& data);

    /**
     * @brief Suspend global subscription (N-ACTION Type 5)
     *
     * @param assoc The established association to use
     * @return Result containing ups_result on success, or error
     */
    [[nodiscard]] network::Result<ups_result> suspend_global(
        network::association& assoc);

    // =========================================================================
    // N-EVENT-REPORT Handler
    // =========================================================================

    /**
     * @brief Handle an N-EVENT-REPORT-RQ received from the SCP
     *
     * Parses the event dataset and invokes the registered callback.
     *
     * @param event_rq The N-EVENT-REPORT-RQ message
     * @return The parsed event notification
     */
    [[nodiscard]] network::Result<ups_watch_event> handle_event_report(
        const network::dimse::dimse_message& event_rq);

    // =========================================================================
    // Statistics
    // =========================================================================

    [[nodiscard]] size_t subscribes_performed() const noexcept;
    [[nodiscard]] size_t unsubscribes_performed() const noexcept;
    [[nodiscard]] size_t events_received() const noexcept;

    void reset_statistics() noexcept;

private:
    // =========================================================================
    // Private Implementation
    // =========================================================================

    [[nodiscard]] network::Result<ups_result> send_watch_action(
        network::association& assoc,
        const std::string& sop_instance_uid,
        uint16_t action_type_id);

    [[nodiscard]] static ups_watch_event parse_event_dataset(
        uint16_t event_type_id,
        const std::string& workitem_uid,
        const core::dicom_dataset& dataset);

    [[nodiscard]] uint16_t next_message_id() noexcept;

    // =========================================================================
    // Private Members
    // =========================================================================

    std::shared_ptr<di::ILogger> logger_;
    ups_watch_scu_config config_;
    event_received_callback event_callback_;
    std::atomic<uint16_t> message_id_counter_{1};
    std::atomic<size_t> subscribes_performed_{0};
    std::atomic<size_t> unsubscribes_performed_{0};
    std::atomic<size_t> events_received_{0};
};

}  // namespace pacs::services

#endif  // PACS_SERVICES_UPS_WATCH_SCU_HPP
