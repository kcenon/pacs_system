/**
 * @file ups_rules.cpp
 * @brief What PS3.4 Annex CC says each UPS request is answered with
 */

#include "kcenon/pacs/services/ups/ups_rules.h"

#include "kcenon/pacs/services/ups/ups_push_scp.h"

#include <array>

namespace kcenon::pacs::services {

namespace {

using network::dimse::status_code;

constexpr std::string_view state_scheduled = "SCHEDULED";
constexpr std::string_view state_in_progress = "IN PROGRESS";
constexpr std::string_view state_completed = "COMPLETED";
constexpr std::string_view state_canceled = "CANCELED";

[[nodiscard]] bool is_final(std::string_view state) {
    return state == state_completed || state == state_canceled;
}

[[nodiscard]] bool has_value(const core::dicom_dataset& attributes, core::dicom_tag tag) {
    return !attributes.get_string(tag).empty();
}

[[nodiscard]] bool has_items(const core::dicom_dataset& attributes, core::dicom_tag tag) {
    const auto* sequence = attributes.get_sequence(tag);
    return sequence != nullptr && !sequence->empty();
}

}  // namespace

status_code ups_change_state_status(
    std::string_view current_state,
    std::string_view recorded_transaction,
    std::string_view wanted_state,
    std::string_view offered_transaction) {

    using namespace network::dimse;

    if (wanted_state == state_scheduled) {
        return status_ups_scheduled_only_by_create;
    }
    if (wanted_state != state_in_progress && !is_final(wanted_state)) {
        return status_error_invalid_attribute_value;
    }
    if (offered_transaction.empty() ||
        (!recorded_transaction.empty() && offered_transaction != recorded_transaction)) {
        return status_ups_wrong_transaction;
    }
    if (wanted_state == state_in_progress) {
        if (current_state == state_scheduled) {
            return status_success;
        }
        return current_state == state_in_progress ? status_ups_already_in_progress
                                                  : status_ups_no_longer_updatable;
    }
    if (current_state == state_scheduled) {
        return status_ups_not_in_progress;
    }
    if (is_final(current_state)) {
        if (current_state != wanted_state) {
            return status_ups_no_longer_updatable;
        }
        return wanted_state == state_completed ? status_ups_already_completed
                                               : status_ups_already_canceled;
    }
    return status_success;
}

status_code ups_set_status(
    std::string_view current_state,
    std::string_view recorded_transaction,
    std::string_view offered_transaction) {

    using namespace network::dimse;

    if (is_final(current_state)) {
        return status_ups_no_longer_updatable;
    }
    if (current_state == state_in_progress) {
        return offered_transaction == recorded_transaction ? status_success
                                                           : status_ups_wrong_transaction;
    }
    // CC.2.6.2: to set a SCHEDULED instance the Transaction UID shall not be present.
    return offered_transaction.empty() ? status_success : status_ups_not_in_progress;
}

status_code ups_create_status(const core::dicom_dataset& attributes) {
    using namespace network::dimse;

    if (attributes.get_string(ups_tags::procedure_step_state) != state_scheduled) {
        return status_ups_not_scheduled;
    }
    if (has_value(attributes, ups_tags::transaction_uid)) {
        return status_error_invalid_attribute_value;
    }
    constexpr std::array required{
        ups_tags::scheduled_procedure_step_priority,
        ups_tags::procedure_step_label,
        ups_tags::scheduled_procedure_step_start_datetime,
        ups_tags::input_readiness_state,
    };
    for (const auto tag : required) {
        if (!has_value(attributes, tag)) {
            return status_error_missing_attribute;
        }
    }
    return status_success;
}

bool ups_final_state_met(const core::dicom_dataset& attributes, std::string_view wanted_state) {
    if (wanted_state == state_completed) {
        const auto* performed = attributes.get_sequence(ups_tags::performed_procedure_sequence);
        if (performed == nullptr || performed->empty()) {
            return false;
        }
        const auto& item = performed->front();
        // Output Information is Type 2: a step with no outputs still states so with an empty sequence.
        return has_items(item, ups_tags::performed_station_name_code_sequence) &&
               has_items(item, ups_tags::performed_workitem_code_sequence) &&
               has_value(item, ups_tags::performed_procedure_step_start_datetime) &&
               has_value(item, ups_tags::performed_procedure_step_end_datetime) &&
               item.contains(ups_tags::output_information_sequence);
    }
    if (wanted_state == state_canceled) {
        const auto* progress = attributes.get_sequence(ups_tags::procedure_step_progress_information_sequence);
        return progress != nullptr && !progress->empty() &&
               has_value(progress->front(), ups_tags::procedure_step_cancellation_datetime);
    }
    return true;
}

status_code ups_status_from_error_code(int code) noexcept {
    using namespace network::dimse;

    switch (code) {
        case status_error_duplicate_sop_instance:
        case status_error_missing_attribute:
        case status_error_invalid_attribute_value:
        case status_ups_already_canceled:
        case status_ups_already_completed:
        case status_ups_no_longer_updatable:
        case status_ups_wrong_transaction:
        case status_ups_already_in_progress:
        case status_ups_scheduled_only_by_create:
        case status_ups_final_state_unmet:
        case status_ups_no_such_instance:
        case status_ups_unknown_receiving_ae:
        case status_ups_not_scheduled:
        case status_ups_not_in_progress:
        case status_ups_cannot_cancel_completed:
        case status_ups_performer_unreachable:
        case status_ups_action_not_appropriate:
            return static_cast<status_code>(code);
        default:
            return status_error_unable_to_process;
    }
}

}  // namespace kcenon::pacs::services
