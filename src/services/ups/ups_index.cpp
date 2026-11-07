/**
 * @file ups_index.cpp
 * @brief UPS workitems held by the index database, for the UPS services
 */

#include "kcenon/pacs/services/ups/ups_index.h"

#include "kcenon/pacs/core/dicom_file.h"
#include "kcenon/pacs/encoding/transfer_syntax.h"
#include "kcenon/pacs/encoding/vr_type.h"
#include "kcenon/pacs/services/qr_matching.h"
#include "kcenon/pacs/services/ups/ups_rules.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <ctime>
#include <set>

namespace kcenon::pacs::services {

namespace {

using network::Result;
using namespace network::dimse;
using encoding::vr_type;

constexpr std::string_view state_scheduled = "SCHEDULED";
constexpr std::string_view state_in_progress = "IN PROGRESS";
constexpr std::string_view state_canceled = "CANCELED";

constexpr core::dicom_tag tag_sop_class_uid{0x0008, 0x0016};
constexpr core::dicom_tag tag_sop_instance_uid{0x0008, 0x0018};
constexpr core::dicom_tag tag_specific_character_set{0x0008, 0x0005};

[[nodiscard]] Result<std::monostate> refuse(status_code status, const std::string& message) {
    return kcenon::common::make_error<std::monostate>(status, message, "ups");
}

/// The UID names a file, and arrives from the network: digits and dots only (PS3.5 9.1).
[[nodiscard]] bool usable_uid(const std::string& uid) {
    return !uid.empty() && uid.size() <= 64 && uid.front() != '.' &&
           std::all_of(uid.begin(), uid.end(), [](char c) { return (c >= '0' && c <= '9') || c == '.'; });
}

[[nodiscard]] std::string current_datetime() {
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm parts{};
#ifdef _WIN32
    gmtime_s(&parts, &now);
#else
    gmtime_r(&now, &parts);
#endif
    char text[16];
    std::strftime(text, sizeof(text), "%Y%m%d%H%M%S", &parts);
    return text;
}

/// DT range matching is lexical once both sides have the same precision, and these keys do.
[[nodiscard]] bool datetime_matches(std::string_view key, std::string_view value) {
    const auto dash = key.find('-');
    if (dash == std::string_view::npos) {
        return value.substr(0, key.size()) == key;
    }
    const auto low = key.substr(0, dash);
    const auto high = key.substr(dash + 1);
    return (low.empty() || value.substr(0, low.size()) >= low) &&
           (high.empty() || value.substr(0, high.size()) <= high);
}

[[nodiscard]] bool key_holds(const core::dicom_element& key, const core::dicom_dataset& attributes) {
    const auto wanted = attributes.get_string(key.tag());
    auto value = key.as_string();
    if (value.is_err() || value.value().empty()) {
        return true;
    }
    switch (key.vr()) {
        case vr_type::UI:
            return key_matches(key_matching::uid, value.value(), wanted);
        case vr_type::DA:
            return key_matches(key_matching::date, value.value(), wanted);
        case vr_type::TM:
            return key_matches(key_matching::time, value.value(), wanted);
        case vr_type::IS:
            return key_matches(key_matching::number, value.value(), wanted);
        case vr_type::PN:
            return key_matches(key_matching::person_name, value.value(), wanted);
        case vr_type::DT:
            return datetime_matches(value.value(), wanted);
        default:
            return key_matches(key_matching::text, value.value(), wanted);
    }
}

}  // namespace

ups_index::ups_index(storage::index_database& database, std::filesystem::path dataset_directory,
                     std::shared_ptr<event_sender> events, std::shared_ptr<di::ILogger> logger)
    : database_(database)
    , dataset_directory_(std::move(dataset_directory))
    , events_(std::move(events))
    , logger_(logger ? std::move(logger) : di::null_logger()) {
    std::error_code ignored;
    std::filesystem::create_directories(dataset_directory_, ignored);
}

std::shared_ptr<ups_scp> ups_index::make_service() {
    auto push = std::make_shared<ups_push_scp>(logger_);
    push->set_create_handler([this](const storage::ups_workitem& workitem) { return create(workitem); });
    push->set_set_handler([this](const std::string& uid, const core::dicom_dataset& changes) { return set(uid, changes); });
    push->set_get_handler([this](const std::string& uid) { return get(uid); });
    push->set_change_state_handler([this](const std::string& uid, const std::string& state, const std::string& transaction) {
        return change_state(uid, state, transaction);
    });
    push->set_cancel_request_handler([this](const std::string& uid, const core::dicom_dataset& request, const std::string& requesting_ae) {
        return request_cancel(uid, request, requesting_ae);
    });

    auto watch = std::make_shared<ups_watch_scp>(logger_);
    watch->set_subscribe_handler([this](const std::string& ae, const std::string& uid, bool lock) { return subscribe(ae, uid, lock); });
    watch->set_unsubscribe_handler([this](const std::string& ae, const std::string& uid) { return unsubscribe(ae, uid); });
    watch->set_suspend_handler([this](const std::string& ae) { return suspend(ae); });

    auto query = std::make_shared<ups_query_scp>(logger_);
    query->set_handler([this](const core::dicom_dataset& keys, const std::string&) { return find(keys); });

    return std::make_shared<ups_scp>(std::move(push), std::move(watch), std::move(query), logger_);
}

std::filesystem::path ups_index::dataset_path(const std::string& uid) const {
    return dataset_directory_ / (uid + ".dcm");
}

bool ups_index::write_dataset(const std::string& uid, const core::dicom_dataset& attributes) const {
    const auto path = dataset_path(uid);
    auto file = core::dicom_file::create(attributes, encoding::transfer_syntax::explicit_vr_little_endian);
    auto temporary = path;
    temporary += ".tmp";
    if (auto saved = file.save(temporary); saved.is_err()) {
        logger_->error("UPS " + uid + ": cannot write the instance: " + saved.error().message);
        return false;
    }
    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (error) {
        logger_->error("UPS " + uid + ": cannot replace the instance: " + error.message());
        std::filesystem::remove(temporary, error);
        return false;
    }
    return true;
}

core::dicom_dataset ups_index::read_dataset(const std::string& uid) const {
    auto file = core::dicom_file::open(dataset_path(uid));
    if (file.is_err()) {
        logger_->error("UPS " + uid + ": cannot read the instance: " + file.error().message);
        return {};
    }
    return file.value().dataset();
}

std::vector<std::string> ups_index::subscribers(const std::string& uid) const {
    auto rows = database_.get_ups_subscribers(uid);
    if (rows.is_err()) {
        logger_->error("UPS " + uid + ": cannot read subscribers: " + rows.error().message);
        return {};
    }
    // A subscriber can match twice: its row for this instance and its global row.
    std::set<std::string> distinct(rows.value().begin(), rows.value().end());
    return {distinct.begin(), distinct.end()};
}

bool ups_index::has_subscription(const std::string& receiving_ae, const std::string& uid) const {
    auto rows = database_.get_ups_subscriptions(receiving_ae);
    if (rows.is_err()) {
        return false;
    }
    return std::any_of(rows.value().begin(), rows.value().end(),
                       [&](const storage::ups_subscription& row) { return row.workitem_uid == uid; });
}

void ups_index::notify(const std::vector<std::string>& receivers, const std::string& uid, uint16_t event_type,
                       const core::dicom_dataset& information) {
    for (const auto& receiver : receivers) {
        event_report report;
        report.receiving_ae = receiver;
        report.abstract_syntax = ups_event_sop_class_uid;
        report.sop_class_uid = ups_push_sop_class_uid;
        report.sop_instance_uid = uid;
        report.event_type_id = event_type;
        report.information = information;
        // CC.2.4.3: nothing obliges a retry, and an unreachable AE keeps its subscription.
        if (!events_ || !events_->send(std::move(report))) {
            logger_->warn("UPS " + uid + ": no address for subscriber " + receiver);
        }
    }
}

void ups_index::report_state(const std::vector<std::string>& receivers, const std::string& uid,
                             const std::string& state, const core::dicom_dataset& attributes) {
    core::dicom_dataset report;
    report.set_string(ups_tags::procedure_step_state, vr_type::CS, state);
    const auto readiness = attributes.get_string(ups_tags::input_readiness_state);
    report.set_string(ups_tags::input_readiness_state, vr_type::CS, readiness.empty() ? "READY" : readiness);
    if (state == state_canceled && attributes.contains(ups_tags::reason_for_cancellation)) {
        report.set_string(ups_tags::reason_for_cancellation, vr_type::LT,
                          attributes.get_string(ups_tags::reason_for_cancellation));
    }
    notify(receivers, uid, ups_event_state_report, report);
}

void ups_index::report_restart() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::set<std::string> known;
    for (const auto& receiver : subscribers("")) {
        known.insert(receiver);
    }
    auto items = database_.search_ups_workitems(storage::ups_workitem_query{});
    if (items.is_ok()) {
        for (const auto& item : items.value()) {
            for (const auto& receiver : subscribers(item.workitem_uid)) {
                known.insert(receiver);
            }
        }
    }
    // Subscriptions and workitems are in the database, so both lists survived: a warm start.
    core::dicom_dataset report;
    report.set_string(ups_tags::scp_status, vr_type::CS, "RESTARTED");
    report.set_string(ups_tags::subscription_list_status, vr_type::CS, "WARM START");
    report.set_string(ups_tags::unified_procedure_step_list_status, vr_type::CS, "WARM START");
    notify({known.begin(), known.end()}, std::string(ups_global_subscription_instance_uid),
           ups_event_scp_status_change, report);
}

Result<std::monostate> ups_index::create(const storage::ups_workitem& workitem) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto& uid = workitem.workitem_uid;
    if (!usable_uid(uid)) {
        return refuse(status_error_invalid_attribute_value, "not a UID: " + uid);
    }

    auto attributes = workitem.attributes;
    attributes.remove(ups_tags::transaction_uid);
    attributes.set_string(tag_sop_class_uid, vr_type::UI, std::string(ups_push_sop_class_uid));
    attributes.set_string(tag_sop_instance_uid, vr_type::UI, uid);
    attributes.set_string(ups_tags::procedure_step_state, vr_type::CS, std::string(state_scheduled));
    attributes.set_string(ups_tags::scheduled_procedure_step_modification_datetime, vr_type::DT, current_datetime());
    // CC.2.5.1.3: the SCP fills in a Worklist Label the SCU did not give.
    if (attributes.get_string(ups_tags::worklist_label).empty()) {
        attributes.set_string(ups_tags::worklist_label, vr_type::LO, "DEFAULT");
    }

    storage::ups_workitem record;
    record.workitem_uid = uid;
    record.state = std::string(state_scheduled);
    record.procedure_step_label = attributes.get_string(ups_tags::procedure_step_label);
    record.worklist_label = attributes.get_string(ups_tags::worklist_label);
    record.priority = attributes.get_string(ups_tags::scheduled_procedure_step_priority);
    record.scheduled_start_datetime = attributes.get_string(ups_tags::scheduled_procedure_step_start_datetime);
    if (!storage::parse_ups_priority(record.priority).has_value()) {
        return refuse(status_error_invalid_attribute_value, "priority: " + record.priority);
    }

    if (!write_dataset(uid, attributes)) {
        return refuse(status_error_unable_to_process, "the instance could not be stored");
    }
    auto created = database_.create_ups_workitem(record);
    if (created.is_err()) {
        std::error_code ignored;
        std::filesystem::remove(dataset_path(uid), ignored);
        return refuse(status_error_unable_to_process, created.error().message);
    }

    // Table CC.2.3-2: a new step joins each global subscription with its lock, and gets its initial report.
    auto global = subscribers(uid);
    for (const auto& receiver : global) {
        bool deletion_lock = false;
        if (auto rows = database_.get_ups_subscriptions(receiver); rows.is_ok()) {
            for (const auto& row : rows.value()) {
                if (row.workitem_uid.empty()) {
                    deletion_lock = row.deletion_lock;
                }
            }
        }
        storage::ups_subscription subscription;
        subscription.subscriber_ae = receiver;
        subscription.workitem_uid = uid;
        subscription.deletion_lock = deletion_lock;
        if (auto stored = database_.subscribe_ups(subscription); stored.is_err()) {
            logger_->error("UPS " + uid + ": subscription of " + receiver + ": " + stored.error().message);
        }
    }
    report_state(global, uid, record.state, attributes);
    return std::monostate{};
}

Result<std::monostate> ups_index::set(const std::string& uid, const core::dicom_dataset& changes) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto record = database_.find_ups_workitem(uid);
    if (!record) {
        return refuse(status_ups_no_such_instance, "no such workitem");
    }
    auto attributes = read_dataset(uid);
    for (const auto& [tag, element] : changes) {
        attributes.insert(element);
    }

    constexpr std::array scheduling{
        ups_tags::procedure_step_label, ups_tags::worklist_label, ups_tags::scheduled_procedure_step_priority,
        ups_tags::scheduled_procedure_step_start_datetime, ups_tags::scheduled_station_name_code_sequence,
        ups_tags::scheduled_human_performers_sequence};
    const bool rescheduled = std::any_of(scheduling.begin(), scheduling.end(),
                                         [&](const auto tag) { return changes.contains(tag); });
    if (rescheduled) {
        attributes.set_string(ups_tags::scheduled_procedure_step_modification_datetime, vr_type::DT, current_datetime());
    }

    if (!write_dataset(uid, attributes)) {
        return refuse(status_error_unable_to_process, "the instance could not be stored");
    }
    if (rescheduled) {
        record->procedure_step_label = attributes.get_string(ups_tags::procedure_step_label);
        record->worklist_label = attributes.get_string(ups_tags::worklist_label);
        record->priority = attributes.get_string(ups_tags::scheduled_procedure_step_priority);
        record->scheduled_start_datetime = attributes.get_string(ups_tags::scheduled_procedure_step_start_datetime);
        if (auto updated = database_.update_ups_workitem(*record); updated.is_err()) {
            return refuse(status_error_unable_to_process, updated.error().message);
        }
    }

    const auto receivers = subscribers(uid);
    if (changes.contains(ups_tags::scheduled_station_name_code_sequence) ||
        changes.contains(ups_tags::scheduled_human_performers_sequence)) {
        core::dicom_dataset report;
        for (const auto tag : {ups_tags::scheduled_station_name_code_sequence, ups_tags::scheduled_human_performers_sequence}) {
            if (const auto* element = attributes.get(tag)) {
                report.insert(*element);
            }
        }
        notify(receivers, uid, ups_event_assigned, report);
    }
    if (const auto* progress = changes.get(ups_tags::procedure_step_progress_information_sequence)) {
        core::dicom_dataset report;
        report.insert(*progress);
        notify(receivers, uid, ups_event_progress_report, report);
    }
    return std::monostate{};
}

Result<storage::ups_workitem> ups_index::get(const std::string& uid) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto record = database_.find_ups_workitem(uid);
    if (!record) {
        return kcenon::common::make_error<storage::ups_workitem>(status_ups_no_such_instance, "no such workitem", "ups");
    }
    if (usable_uid(uid)) {
        record->attributes = read_dataset(uid);
    }
    return *record;
}

Result<std::monostate> ups_index::change_state(
    const std::string& uid, const std::string& state, const std::string& transaction) {

    std::lock_guard<std::mutex> lock(mutex_);
    auto attributes = read_dataset(uid);
    attributes.set_string(ups_tags::procedure_step_state, vr_type::CS, state);
    if (!write_dataset(uid, attributes)) {
        return refuse(status_error_unable_to_process, "the instance could not be stored");
    }
    if (auto changed = database_.change_ups_state(uid, state, transaction); changed.is_err()) {
        return refuse(status_error_unable_to_process, changed.error().message);
    }
    report_state(subscribers(uid), uid, state, attributes);
    return std::monostate{};
}

Result<std::monostate> ups_index::request_cancel(
    const std::string& uid, const core::dicom_dataset& request, const std::string& requesting_ae) {

    std::lock_guard<std::mutex> lock(mutex_);
    auto record = database_.find_ups_workitem(uid);
    if (!record) {
        return refuse(status_ups_no_such_instance, "no such workitem");
    }
    const auto receivers = subscribers(uid);

    if (record->state == state_in_progress) {
        // CC.2.2.3: the SCP tells the subscribers, or reports that it cannot.
        if (receivers.empty()) {
            return refuse(status_ups_performer_unreachable, "no subscriber to tell of the request");
        }
        core::dicom_dataset report;
        report.set_string(ups_tags::requesting_ae, vr_type::AE, requesting_ae);
        for (const auto tag : {ups_tags::reason_for_cancellation, ups_tags::discontinuation_reason_code_sequence,
                               ups_tags::contact_uri, ups_tags::contact_display_name}) {
            if (const auto* element = request.get(tag)) {
                report.insert(*element);
            }
        }
        notify(receivers, uid, ups_event_cancel_requested, report);
        return std::monostate{};
    }

    // CC.2.2.3: a SCHEDULED step goes first to IN PROGRESS and then to CANCELED.
    auto attributes = read_dataset(uid);
    core::dicom_dataset progress;
    progress.set_string(ups_tags::procedure_step_cancellation_datetime, vr_type::DT, current_datetime());
    auto& sequence = attributes.get_or_create_sequence(ups_tags::procedure_step_progress_information_sequence);
    sequence.clear();
    sequence.push_back(std::move(progress));
    for (const auto tag : {ups_tags::reason_for_cancellation, ups_tags::discontinuation_reason_code_sequence}) {
        if (const auto* element = request.get(tag)) {
            attributes.insert(*element);
        }
    }
    attributes.set_string(ups_tags::procedure_step_state, vr_type::CS, std::string(state_canceled));
    if (!write_dataset(uid, attributes)) {
        return refuse(status_error_unable_to_process, "the instance could not be stored");
    }
    if (auto changed = database_.change_ups_state(uid, state_canceled, ""); changed.is_err()) {
        return refuse(status_error_unable_to_process, changed.error().message);
    }
    report_state(receivers, uid, std::string(state_in_progress), attributes);
    report_state(receivers, uid, std::string(state_canceled), attributes);
    return std::monostate{};
}

Result<std::monostate> ups_index::subscribe(const std::string& receiving_ae, const std::string& uid, bool deletion_lock) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!events_ || !events_->knows(receiving_ae)) {
        return refuse(status_ups_unknown_receiving_ae, "no address for " + receiving_ae);
    }

    storage::ups_subscription subscription;
    subscription.subscriber_ae = receiving_ae;
    subscription.deletion_lock = deletion_lock;

    if (!uid.empty()) {
        auto record = database_.find_ups_workitem(uid);
        if (!record) {
            return refuse(status_ups_no_such_instance, "no such workitem");
        }
        subscription.workitem_uid = uid;
        if (auto stored = database_.subscribe_ups(subscription); stored.is_err()) {
            return refuse(status_error_unable_to_process, stored.error().message);
        }
        report_state({receiving_ae}, uid, record->state, read_dataset(uid));
        return std::monostate{};
    }

    // Table CC.2.3-2: every step this AE was not subscribed to joins the global subscription,
    // and with a lock each of those gets its initial report. Steps it already watches stay as they are.
    auto items = database_.search_ups_workitems(storage::ups_workitem_query{});
    if (items.is_err()) {
        return refuse(status_error_unable_to_process, items.error().message);
    }
    for (const auto& item : items.value()) {
        if (has_subscription(receiving_ae, item.workitem_uid)) {
            continue;
        }
        subscription.workitem_uid = item.workitem_uid;
        if (auto stored = database_.subscribe_ups(subscription); stored.is_err()) {
            return refuse(status_error_unable_to_process, stored.error().message);
        }
        if (deletion_lock) {
            report_state({receiving_ae}, item.workitem_uid, item.state, read_dataset(item.workitem_uid));
        }
    }
    subscription.workitem_uid.clear();
    if (auto stored = database_.subscribe_ups(subscription); stored.is_err()) {
        return refuse(status_error_unable_to_process, stored.error().message);
    }
    return std::monostate{};
}

Result<std::monostate> ups_index::unsubscribe(const std::string& receiving_ae, const std::string& uid) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!uid.empty() && !database_.find_ups_workitem(uid)) {
        return refuse(status_ups_no_such_instance, "no such workitem");
    }
    // An empty UID removes every row of this AE, global and per step: Table CC.2.3-2.
    if (auto removed = database_.unsubscribe_ups(receiving_ae, uid); removed.is_err()) {
        return refuse(status_error_unable_to_process, removed.error().message);
    }
    return std::monostate{};
}

Result<std::monostate> ups_index::suspend(const std::string& receiving_ae) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (auto removed = database_.suspend_ups_global_subscription(receiving_ae); removed.is_err()) {
        return refuse(status_error_unable_to_process, removed.error().message);
    }
    return std::monostate{};
}

std::vector<core::dicom_dataset> ups_index::find(const core::dicom_dataset& keys) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<core::dicom_dataset> rows;
    auto items = database_.search_ups_workitems(storage::ups_workitem_query{});
    if (items.is_err()) {
        logger_->error("UPS C-FIND: " + items.error().message);
        return rows;
    }
    for (const auto& item : items.value()) {
        auto attributes = read_dataset(item.workitem_uid);
        attributes.set_string(ups_tags::procedure_step_state, vr_type::CS, item.state);

        const bool matched = std::all_of(keys.begin(), keys.end(), [&](const auto& entry) {
            const auto& [tag, key] = entry;
            // Sequences and the lock cannot be matched here; a sequence key only asks for its value back.
            return key.is_sequence() || tag == ups_tags::transaction_uid || tag == tag_specific_character_set ||
                   key_holds(key, attributes);
        });
        if (!matched) {
            continue;
        }

        core::dicom_dataset row;
        for (const auto& [tag, key] : keys) {
            if (tag == ups_tags::transaction_uid) {
                continue;  // Table CC.2.5-3: cannot be queried.
            }
            if (const auto* element = attributes.get(tag)) {
                row.insert(*element);
            } else {
                row.insert(core::dicom_element(tag, key.vr()));
            }
        }
        row.set_string(tag_sop_class_uid, vr_type::UI, std::string(ups_push_sop_class_uid));
        row.set_string(tag_sop_instance_uid, vr_type::UI, item.workitem_uid);
        if (const auto* charset = attributes.get(tag_specific_character_set)) {
            row.insert(*charset);
        }
        rows.push_back(std::move(row));
    }
    return rows;
}

}  // namespace kcenon::pacs::services
