/**
 * @file event_sender.cpp
 * @brief N-EVENT-REPORT delivery on associations this AE opens in the SCP role
 */

#include "kcenon/pacs/services/event_sender.h"

#include "kcenon/pacs/network/association.h"
#include "kcenon/pacs/network/dimse/dimse_message.h"

#include <algorithm>

namespace kcenon::pacs::services {

namespace {

constexpr const char* implementation_class_uid = "1.2.826.0.1.3680043.2.1545.1";
constexpr const char* implementation_version = "PACS_EVENT_100";

}  // namespace

event_sender::event_sender(event_sender_config config, ae_address_resolver resolver,
                           std::shared_ptr<di::ILogger> logger)
    : config_(std::move(config))
    , resolver_(std::move(resolver))
    , logger_(logger ? std::move(logger) : di::null_logger())
    , worker_([this] { run(); }) {}

event_sender::~event_sender() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }
    wake_.notify_all();
    worker_.join();
    size_t waiting = 0;
    for (const auto& [ae, queue] : queues_) {
        waiting += queue.size();
    }
    if (waiting > 0) {
        logger_->warn("Stopping with " + std::to_string(waiting) + " N-EVENT-REPORT(s) undelivered");
    }
}

bool event_sender::knows(const std::string& ae_title) const {
    return resolver_ && resolver_(ae_title).has_value();
}

bool event_sender::send(event_report report) {
    if (!knows(report.receiving_ae)) {
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& queue = queues_[report.receiving_ae];
        queue.push_back(pending_report{std::move(report), 0, {}});
    }
    wake_.notify_all();
    return true;
}

void event_sender::set_observer(event_delivery_observer observer) {
    std::lock_guard<std::mutex> lock(mutex_);
    observer_ = std::move(observer);
}

size_t event_sender::delivered() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return delivered_;
}

size_t event_sender::abandoned() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return abandoned_;
}

std::optional<uint16_t> event_sender::attempt(const event_report& report,
                                              const std::pair<std::string, uint16_t>& address) {
    using namespace network;

    association_config config;
    config.calling_ae_title = config_.local_ae_title;
    config.called_ae_title = report.receiving_ae;
    config.implementation_class_uid = implementation_class_uid;
    config.implementation_version_name = implementation_version;
    config.proposed_contexts.push_back({1, report.abstract_syntax, {"1.2.840.10008.1.2.1", "1.2.840.10008.1.2"}});
    // The default roles would make this requestor the SCU, which cannot send an event report.
    config.role_selections.emplace_back(report.abstract_syntax, false, true);

    auto connected = association::connect(address.first, address.second, config, config_.timeout);
    if (connected.is_err()) {
        logger_->warn("N-EVENT-REPORT to " + report.receiving_ae + ": " + connected.error().message);
        return std::nullopt;
    }
    auto& assoc = connected.value();
    if (!assoc.requestor_role(report.abstract_syntax).scp_role) {
        logger_->warn("N-EVENT-REPORT to " + report.receiving_ae + ": the SCP role was not granted");
        assoc.abort(0, 0);
        return std::nullopt;
    }
    const auto context_id = assoc.accepted_context_id(report.abstract_syntax);
    if (!context_id) {
        logger_->warn("N-EVENT-REPORT to " + report.receiving_ae + ": no accepted presentation context");
        assoc.abort(0, 0);
        return std::nullopt;
    }

    auto request = dimse::make_n_event_report_rq(1, report.sop_class_uid, report.sop_instance_uid, report.event_type_id);
    request.set_dataset(report.information);
    auto sent = assoc.send_dimse(*context_id, request);
    if (sent.is_err()) {
        logger_->warn("N-EVENT-REPORT to " + report.receiving_ae + ": " + sent.error().message);
        assoc.abort(0, 0);
        return std::nullopt;
    }
    auto received = assoc.receive_dimse(config_.timeout);
    if (received.is_err()) {
        logger_->warn("N-EVENT-REPORT to " + report.receiving_ae + ": no response: " + received.error().message);
        assoc.abort(0, 0);
        return std::nullopt;
    }
    const auto& [_, response] = received.value();
    if (response.command() != dimse::command_field::n_event_report_rsp) {
        logger_->warn("N-EVENT-REPORT to " + report.receiving_ae + ": unexpected response");
        assoc.abort(0, 0);
        return std::nullopt;
    }
    const auto status = response.status();
    if (assoc.release(config_.timeout).is_err()) {
        // The report was answered; only the tidy ending failed.
        logger_->debug("N-EVENT-REPORT to " + report.receiving_ae + ": association did not release cleanly");
    }
    return status;
}

void event_sender::run() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (!stopping_) {
        const auto now = std::chrono::steady_clock::now();
        std::string ready;
        std::optional<std::chrono::steady_clock::time_point> next;
        for (const auto& [ae, queue] : queues_) {
            if (queue.empty()) {
                continue;
            }
            if (queue.front().not_before <= now) {
                ready = ae;
                break;
            }
            next = next ? std::min(*next, queue.front().not_before) : queue.front().not_before;
        }
        if (ready.empty()) {
            if (next) {
                wake_.wait_until(lock, *next);
            } else {
                wake_.wait(lock);
            }
            continue;
        }

        auto current = queues_[ready].front();
        auto address = resolver_(ready);
        lock.unlock();
        std::optional<uint16_t> status;
        if (address) {
            status = attempt(current.report, *address);
        }
        lock.lock();

        auto& queue = queues_[ready];
        ++current.attempts_made;
        if (!status && address && current.attempts_made < config_.attempts) {
            queue.front().attempts_made = current.attempts_made;
            queue.front().not_before = std::chrono::steady_clock::now() + config_.retry_delay;
            continue;
        }
        queue.pop_front();
        if (status) {
            ++delivered_;
            logger_->info("N-EVENT-REPORT type " + std::to_string(current.report.event_type_id) + " to " + ready +
                          " answered with status " + std::to_string(*status));
        } else {
            ++abandoned_;
            logger_->error("N-EVENT-REPORT type " + std::to_string(current.report.event_type_id) + " to " + ready +
                           " abandoned after " + std::to_string(current.attempts_made) + " attempts");
        }
        if (observer_) {
            auto observer = observer_;
            lock.unlock();
            observer(current.report, status, current.attempts_made);
            lock.lock();
        }
    }
}

}  // namespace kcenon::pacs::services
