/**
 * @file event_sender.h
 * @brief N-EVENT-REPORT delivery on associations this AE opens in the SCP role
 *
 * PS3.4 J.3.3 requires a Storage Commitment SCP to be able to report on a different
 * association, and UPS events always go to a Receiving AE that need not be connected.
 * The association requestor takes the SCP role through Role Selection (PS3.7 D.3.3.4).
 */

#ifndef PACS_SERVICES_EVENT_SENDER_HPP
#define PACS_SERVICES_EVENT_SENDER_HPP

#include "kcenon/pacs/core/dicom_dataset.h"
#include "kcenon/pacs/di/ilogger.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>

namespace kcenon::pacs::services {

/// One report for one AE.
struct event_report {
    std::string receiving_ae;
    /// SOP Class negotiated for the association; a UPS event names UPS Push on the UPS Event context.
    std::string abstract_syntax;
    std::string sop_class_uid;
    std::string sop_instance_uid;
    uint16_t event_type_id{0};
    core::dicom_dataset information;
};

/// Network address of an AE Title, when this AE knows one.
using ae_address_resolver =
    std::function<std::optional<std::pair<std::string, uint16_t>>(const std::string& ae_title)>;

/// Final outcome of a report: the peer's status, or nothing when no attempt got an answer.
using event_delivery_observer =
    std::function<void(const event_report& report, std::optional<uint16_t> status, int attempts)>;

struct event_sender_config {
    std::string local_ae_title;
    /// Attempts per report. A peer that answers, with any status, ends them.
    int attempts{3};
    std::chrono::milliseconds retry_delay{5000};
    std::chrono::milliseconds timeout{10000};
};

/**
 * @brief Delivers reports in order, per AE, from one worker thread.
 *
 * A report waiting for its retry holds back later reports for the same AE only, so a
 * subscriber sees states in the order they happened and a dead AE does not delay the
 * others. Reports are not kept across restarts; CC.2.4.3 sets no obligation to queue
 * or retry, and a failed delivery leaves subscriptions and locks as they were.
 */
class event_sender {
public:
    event_sender(event_sender_config config, ae_address_resolver resolver,
                 std::shared_ptr<di::ILogger> logger = nullptr);
    ~event_sender();

    event_sender(const event_sender&) = delete;
    event_sender& operator=(const event_sender&) = delete;

    /// Whether the report has somewhere to go: the resolver knows the AE Title.
    [[nodiscard]] bool knows(const std::string& ae_title) const;

    /// Queue a report. Returns false, queueing nothing, for an AE with no known address.
    bool send(event_report report);

    void set_observer(event_delivery_observer observer);

    /// Reports the peer answered, and those that used up their attempts.
    [[nodiscard]] size_t delivered() const;
    [[nodiscard]] size_t abandoned() const;

private:
    struct pending_report {
        event_report report;
        int attempts_made{0};
        std::chrono::steady_clock::time_point not_before{};
    };

    void run();
    /// Opens the association and exchanges the report; nothing when no answer came.
    std::optional<uint16_t> attempt(const event_report& report, const std::pair<std::string, uint16_t>& address);

    event_sender_config config_;
    ae_address_resolver resolver_;
    std::shared_ptr<di::ILogger> logger_;
    event_delivery_observer observer_;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::map<std::string, std::deque<pending_report>> queues_;
    bool stopping_{false};
    size_t delivered_{0};
    size_t abandoned_{0};
    std::thread worker_;
};

}  // namespace kcenon::pacs::services

#endif  // PACS_SERVICES_EVENT_SENDER_HPP
