/**
 * @file ups_watch_scp.hpp
 * @brief DICOM UPS Watch SCP service (subscription and event notification)
 *
 * This file provides the ups_watch_scp class for handling N-ACTION
 * subscribe/unsubscribe requests and dispatching N-EVENT-REPORT
 * notifications when UPS workitems change state.
 *
 * @see DICOM PS3.4 Annex CC.2.3 - UPS Watch SOP Class
 * @see DICOM PS3.4 CC.2.4 - N-EVENT-REPORT for UPS
 * @see Issue #813 - UPS Watch SCP Implementation
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_SERVICES_UPS_WATCH_SCP_HPP
#define PACS_SERVICES_UPS_WATCH_SCP_HPP

#include "../scp_service.hpp"

#include <atomic>
#include <functional>

namespace kcenon::pacs::services {

// =============================================================================
// SOP Class UID
// =============================================================================

/// UPS Watch SOP Class UID (PS3.4 Table CC.2-1)
inline constexpr std::string_view ups_watch_sop_class_uid =
    "1.2.840.10008.5.1.4.34.6.2";

// =============================================================================
// Well-Known SOP Instance UID
// =============================================================================

/// UPS Global Subscription SOP Instance UID
/// Used as the SOP Instance UID for global (non-workitem-specific) subscriptions
inline constexpr std::string_view ups_global_subscription_instance_uid =
    "1.2.840.10008.5.1.4.34.5";

// =============================================================================
// N-ACTION Type Constants for UPS Watch
// =============================================================================

/// N-ACTION Type 3: Subscribe to Receive UPS Event Reports (PS3.4 CC.2.3.3)
inline constexpr uint16_t ups_watch_action_subscribe = 3;

/// N-ACTION Type 4: Unsubscribe from Receiving UPS Event Reports (PS3.4 CC.2.3.4)
inline constexpr uint16_t ups_watch_action_unsubscribe = 4;

/// N-ACTION Type 5: Suspend Global Subscription (PS3.4 CC.2.3.5)
inline constexpr uint16_t ups_watch_action_suspend_global = 5;

// =============================================================================
// N-EVENT-REPORT Event Type Constants
// =============================================================================

/// Event Type 1: UPS State Report — workitem state changed
inline constexpr uint16_t ups_event_state_report = 1;

/// Event Type 2: UPS Cancel Requested — performer should stop processing
inline constexpr uint16_t ups_event_cancel_requested = 2;

/// Event Type 3: UPS Progress Report — progress percentage update
inline constexpr uint16_t ups_event_progress_report = 3;

/// Event Type 4: SCP Status Change — SCP going down/restarting
inline constexpr uint16_t ups_event_scp_status_change = 4;

// =============================================================================
// Handler Types
// =============================================================================

/**
 * @brief Subscribe handler function type
 *
 * Called when an N-ACTION subscribe request is received. The handler should
 * persist the subscription (e.g., to the ups_subscriptions database table).
 *
 * @param subscriber_ae The AE Title of the subscribing system
 * @param workitem_uid The specific workitem UID (empty for global subscription)
 * @param deletion_lock Whether deletion lock is requested
 * @return Success if the subscription was created, error otherwise
 */
using ups_subscribe_handler = std::function<network::Result<std::monostate>(
    const std::string& subscriber_ae,
    const std::string& workitem_uid,
    bool deletion_lock)>;

/**
 * @brief Unsubscribe handler function type
 *
 * Called when an N-ACTION unsubscribe request is received. The handler should
 * remove the subscription from persistence.
 *
 * @param subscriber_ae The AE Title of the unsubscribing system
 * @param workitem_uid The specific workitem UID (empty for global unsubscription)
 * @return Success if the subscription was removed, error otherwise
 */
using ups_unsubscribe_handler = std::function<network::Result<std::monostate>(
    const std::string& subscriber_ae,
    const std::string& workitem_uid)>;

/**
 * @brief Get subscribers handler function type
 *
 * Called to retrieve subscriber AE titles for a specific workitem.
 * Used when sending N-EVENT-REPORT notifications.
 *
 * @param workitem_uid The workitem UID to find subscribers for
 * @return Vector of subscriber AE titles
 */
using ups_get_subscribers_handler = std::function<
    network::Result<std::vector<std::string>>(
        const std::string& workitem_uid)>;

/**
 * @brief Event notification callback type
 *
 * Called by the Watch SCP when an event needs to be dispatched to subscribers.
 * The application layer is responsible for establishing the network connection
 * and sending the N-EVENT-REPORT to each subscriber.
 *
 * @param subscriber_ae The AE Title of the subscriber to notify
 * @param event_type_id The event type (ups_event_state_report, etc.)
 * @param workitem_uid The workitem UID that triggered the event
 * @param event_info Dataset containing event details
 */
using ups_event_callback = std::function<void(
    const std::string& subscriber_ae,
    uint16_t event_type_id,
    const std::string& workitem_uid,
    const core::dicom_dataset& event_info)>;

// =============================================================================
// UPS Watch SCP Class
// =============================================================================

/**
 * @brief UPS Watch SCP service for subscription and event notification
 *
 * The UPS Watch SCP handles subscribe/unsubscribe N-ACTION requests and
 * provides event notification infrastructure. When workitem state changes
 * occur (triggered externally, e.g., by UPS Push SCP), the Watch SCP
 * notifies all relevant subscribers via the event callback.
 *
 * ## UPS Watch Message Flow
 *
 * ```
 * Subscriber (SCU)                     UPS Watch SCP
 *  |                                   |
 *  |  N-ACTION-RQ (Subscribe, Type 3)  |
 *  |  WorkitemUID: 1.2.3.4...         |
 *  |---------------------------------->|
 *  |  N-ACTION-RSP (Success)           |
 *  |<----------------------------------|
 *  |                                   |
 *  |  [Workitem state changes]         |
 *  |                                   |
 *  |  N-EVENT-REPORT-RQ (Type 1)       |
 *  |  State: COMPLETED                |
 *  |<----------------------------------|
 *  |  N-EVENT-REPORT-RSP               |
 *  |---------------------------------->|
 *  |                                   |
 *  |  N-ACTION-RQ (Unsubscribe, Type 4)|
 *  |---------------------------------->|
 *  |  N-ACTION-RSP (Success)           |
 *  |<----------------------------------|
 * ```
 */
class ups_watch_scp final : public scp_service {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct UPS Watch SCP with optional logger
     *
     * @param logger Logger instance (nullptr uses null_logger)
     */
    explicit ups_watch_scp(std::shared_ptr<di::ILogger> logger = nullptr);

    ~ups_watch_scp() override = default;

    // =========================================================================
    // Configuration
    // =========================================================================

    void set_subscribe_handler(ups_subscribe_handler handler);
    void set_unsubscribe_handler(ups_unsubscribe_handler handler);
    void set_get_subscribers_handler(ups_get_subscribers_handler handler);
    void set_event_callback(ups_event_callback callback);

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
    // Event Notification
    // =========================================================================

    /**
     * @brief Notify subscribers of a workitem state change
     *
     * Looks up subscribers for the given workitem and dispatches
     * UPS State Report events via the event callback.
     *
     * @param workitem_uid The workitem that changed state
     * @param new_state The new state string
     */
    void notify_state_change(const std::string& workitem_uid,
                             const std::string& new_state);

    /**
     * @brief Notify subscribers of a cancel request
     *
     * @param workitem_uid The workitem for which cancel was requested
     * @param reason The cancellation reason (may be empty)
     */
    void notify_cancel_requested(const std::string& workitem_uid,
                                 const std::string& reason);

    /**
     * @brief Notify subscribers of progress update
     *
     * @param workitem_uid The workitem that reported progress
     * @param progress_percent The progress percentage (0-100)
     */
    void notify_progress(const std::string& workitem_uid,
                         int progress_percent);

    // =========================================================================
    // Statistics
    // =========================================================================

    [[nodiscard]] size_t subscriptions_created() const noexcept;
    [[nodiscard]] size_t subscriptions_removed() const noexcept;
    [[nodiscard]] size_t events_sent() const noexcept;

    void reset_statistics() noexcept;

private:
    // =========================================================================
    // Private Implementation
    // =========================================================================

    [[nodiscard]] network::Result<std::monostate> handle_n_action(
        network::association& assoc,
        uint8_t context_id,
        const network::dimse::dimse_message& request);

    [[nodiscard]] network::Result<std::monostate> handle_subscribe(
        network::association& assoc,
        uint8_t context_id,
        uint16_t message_id,
        const std::string& sop_instance_uid);

    [[nodiscard]] network::Result<std::monostate> handle_unsubscribe(
        network::association& assoc,
        uint8_t context_id,
        uint16_t message_id,
        const std::string& sop_instance_uid);

    [[nodiscard]] network::Result<std::monostate> handle_suspend_global(
        network::association& assoc,
        uint8_t context_id,
        uint16_t message_id);

    [[nodiscard]] network::Result<std::monostate> send_n_action_response(
        network::association& assoc,
        uint8_t context_id,
        uint16_t message_id,
        const std::string& sop_instance_uid,
        uint16_t action_type_id,
        network::dimse::status_code status);

    void dispatch_event(uint16_t event_type_id,
                        const std::string& workitem_uid,
                        const core::dicom_dataset& event_info);

    // =========================================================================
    // Member Variables
    // =========================================================================

    ups_subscribe_handler subscribe_handler_;
    ups_unsubscribe_handler unsubscribe_handler_;
    ups_get_subscribers_handler get_subscribers_handler_;
    ups_event_callback event_callback_;

    std::atomic<size_t> subscriptions_created_{0};
    std::atomic<size_t> subscriptions_removed_{0};
    std::atomic<size_t> events_sent_{0};
};

}  // namespace kcenon::pacs::services

#endif  // PACS_SERVICES_UPS_WATCH_SCP_HPP
