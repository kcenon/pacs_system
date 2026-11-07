/**
 * @file ups_rules_test.cpp
 * @brief PS3.4 Annex CC tables as data, and the workitem index behind the UPS services
 */

#include <kcenon/pacs/core/dicom_dictionary.h>
#include <kcenon/pacs/network/dimse/dimse_message.h>
#include <kcenon/pacs/services/event_sender.h>
#include <kcenon/pacs/services/storage_commitment_types.h>
#include <kcenon/pacs/services/ups/ups_index.h>
#include <kcenon/pacs/services/ups/ups_rules.h>
#include <kcenon/pacs/services/ups/ups_scp.h>
#include <kcenon/pacs/storage/index_database.h>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

using namespace kcenon::pacs;
using namespace kcenon::pacs::services;
using encoding::vr_type;
using network::dimse::status_code;
using network::dimse::status_success;

namespace {

constexpr const char* recorded = "1.2.3";
constexpr const char* other = "1.2.4";

core::dicom_dataset create_attributes() {
    core::dicom_dataset attributes;
    attributes.set_string(ups_tags::procedure_step_state, vr_type::CS, "SCHEDULED");
    attributes.set_string(ups_tags::scheduled_procedure_step_priority, vr_type::CS, "MEDIUM");
    attributes.set_string(ups_tags::procedure_step_label, vr_type::LO, "Label");
    attributes.set_string(ups_tags::scheduled_procedure_step_start_datetime, vr_type::DT, "20261102120000");
    attributes.set_string(ups_tags::input_readiness_state, vr_type::CS, "READY");
    return attributes;
}

core::dicom_dataset performed_item() {
    core::dicom_dataset code;
    code.set_string(core::dicom_tag{0x0008, 0x0100}, vr_type::SH, "110001");
    core::dicom_dataset item;
    item.get_or_create_sequence(ups_tags::performed_station_name_code_sequence).push_back(code);
    item.get_or_create_sequence(ups_tags::performed_workitem_code_sequence).push_back(code);
    item.set_string(ups_tags::performed_procedure_step_start_datetime, vr_type::DT, "20261102120100");
    item.set_string(ups_tags::performed_procedure_step_end_datetime, vr_type::DT, "20261102120200");
    (void)item.get_or_create_sequence(ups_tags::output_information_sequence);
    return item;
}

/// A directory for one test, removed with it.
struct scratch_directory {
    std::filesystem::path path;
    scratch_directory() {
        static int counter = 0;
        path = std::filesystem::temp_directory_path() /
               ("ups_index_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "_" +
                std::to_string(++counter));
        std::filesystem::create_directories(path);
    }
    ~scratch_directory() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
};

std::unique_ptr<storage::index_database> open_index() {
    auto opened = storage::index_database::open(":memory:");
    REQUIRE(opened.is_ok());
    return std::move(opened.value());
}

/// Knows one AE at a port nothing listens on, and gives up at once.
std::shared_ptr<event_sender> unreachable_sender() {
    event_sender_config config;
    config.local_ae_title = "TEST_SCP";
    config.attempts = 1;
    config.timeout = std::chrono::milliseconds{200};
    return std::make_shared<event_sender>(config, [](const std::string& ae_title) -> std::optional<std::pair<std::string, uint16_t>> {
        if (ae_title == "WATCHER") {
            return std::make_pair(std::string("127.0.0.1"), uint16_t{1});
        }
        return std::nullopt;
    });
}

storage::ups_workitem workitem_for(const std::string& uid) {
    storage::ups_workitem workitem;
    workitem.workitem_uid = uid;
    workitem.attributes = create_attributes();
    return workitem;
}

int error_of(const network::Result<std::monostate>& result) {
    REQUIRE(result.is_err());
    return result.error().code;
}

}  // namespace

TEST_CASE("Change UPS State follows Table CC.1.1-2", "[services][ups]") {
    const auto status = ups_change_state_status;

    SECTION("to IN PROGRESS") {
        CHECK(status("SCHEDULED", "", "IN PROGRESS", recorded) == status_success);
        CHECK(status("SCHEDULED", "", "IN PROGRESS", "") == status_ups_wrong_transaction);
        CHECK(status("IN PROGRESS", recorded, "IN PROGRESS", recorded) == status_ups_already_in_progress);
        CHECK(status("IN PROGRESS", recorded, "IN PROGRESS", other) == status_ups_wrong_transaction);
        CHECK(status("COMPLETED", recorded, "IN PROGRESS", recorded) == status_ups_no_longer_updatable);
        CHECK(status("COMPLETED", recorded, "IN PROGRESS", other) == status_ups_wrong_transaction);
        CHECK(status("CANCELED", recorded, "IN PROGRESS", recorded) == status_ups_no_longer_updatable);
        CHECK(status("CANCELED", recorded, "IN PROGRESS", other) == status_ups_wrong_transaction);
    }
    SECTION("to SCHEDULED is only by N-CREATE") {
        for (const auto* state : {"SCHEDULED", "IN PROGRESS", "COMPLETED", "CANCELED"}) {
            CHECK(status(state, recorded, "SCHEDULED", recorded) == status_ups_scheduled_only_by_create);
        }
    }
    SECTION("to COMPLETED") {
        CHECK(status("SCHEDULED", "", "COMPLETED", recorded) == status_ups_not_in_progress);
        CHECK(status("SCHEDULED", "", "COMPLETED", "") == status_ups_wrong_transaction);
        CHECK(status("IN PROGRESS", recorded, "COMPLETED", recorded) == status_success);
        CHECK(status("IN PROGRESS", recorded, "COMPLETED", other) == status_ups_wrong_transaction);
        CHECK(status("COMPLETED", recorded, "COMPLETED", recorded) == status_ups_already_completed);
        CHECK(status("COMPLETED", recorded, "COMPLETED", other) == status_ups_wrong_transaction);
        CHECK(status("CANCELED", recorded, "COMPLETED", recorded) == status_ups_no_longer_updatable);
        CHECK(status("CANCELED", recorded, "COMPLETED", other) == status_ups_wrong_transaction);
    }
    SECTION("to CANCELED") {
        CHECK(status("SCHEDULED", "", "CANCELED", recorded) == status_ups_not_in_progress);
        CHECK(status("SCHEDULED", "", "CANCELED", "") == status_ups_wrong_transaction);
        CHECK(status("IN PROGRESS", recorded, "CANCELED", recorded) == status_success);
        CHECK(status("IN PROGRESS", recorded, "CANCELED", other) == status_ups_wrong_transaction);
        CHECK(status("COMPLETED", recorded, "CANCELED", recorded) == status_ups_no_longer_updatable);
        CHECK(status("COMPLETED", recorded, "CANCELED", other) == status_ups_wrong_transaction);
        CHECK(status("CANCELED", recorded, "CANCELED", recorded) == status_ups_already_canceled);
        CHECK(status("CANCELED", recorded, "CANCELED", other) == status_ups_wrong_transaction);
    }
    SECTION("a state the standard does not have") {
        CHECK(status("SCHEDULED", "", "PAUSED", recorded) == network::dimse::status_error_invalid_attribute_value);
    }
}

TEST_CASE("N-SET on a UPS follows CC.2.6", "[services][ups]") {
    CHECK(ups_set_status("SCHEDULED", "", "") == status_success);
    CHECK(ups_set_status("SCHEDULED", "", recorded) == status_ups_not_in_progress);
    CHECK(ups_set_status("IN PROGRESS", recorded, recorded) == status_success);
    CHECK(ups_set_status("IN PROGRESS", recorded, other) == status_ups_wrong_transaction);
    CHECK(ups_set_status("IN PROGRESS", recorded, "") == status_ups_wrong_transaction);
    CHECK(ups_set_status("COMPLETED", recorded, recorded) == status_ups_no_longer_updatable);
    CHECK(ups_set_status("CANCELED", recorded, recorded) == status_ups_no_longer_updatable);
}

TEST_CASE("N-CREATE of a UPS follows Table CC.2.5-3", "[services][ups]") {
    auto attributes = create_attributes();
    CHECK(ups_create_status(attributes) == status_success);

    SECTION("a state other than SCHEDULED") {
        attributes.set_string(ups_tags::procedure_step_state, vr_type::CS, "IN PROGRESS");
        CHECK(ups_create_status(attributes) == status_ups_not_scheduled);
    }
    SECTION("no state at all") {
        attributes.remove(ups_tags::procedure_step_state);
        CHECK(ups_create_status(attributes) == status_ups_not_scheduled);
    }
    SECTION("a Transaction UID, which only IN PROGRESS gets") {
        attributes.set_string(ups_tags::transaction_uid, vr_type::UI, recorded);
        CHECK(ups_create_status(attributes) == network::dimse::status_error_invalid_attribute_value);
    }
    SECTION("each Type 1 attribute missing") {
        for (const auto tag : {ups_tags::scheduled_procedure_step_priority, ups_tags::procedure_step_label,
                               ups_tags::scheduled_procedure_step_start_datetime, ups_tags::input_readiness_state}) {
            auto incomplete = create_attributes();
            incomplete.remove(tag);
            CHECK(ups_create_status(incomplete) == network::dimse::status_error_missing_attribute);
        }
    }
}

TEST_CASE("final state requirements of Table CC.2.5-3", "[services][ups]") {
    core::dicom_dataset attributes;

    SECTION("COMPLETED needs the performed procedure") {
        CHECK_FALSE(ups_final_state_met(attributes, "COMPLETED"));
        attributes.get_or_create_sequence(ups_tags::performed_procedure_sequence).push_back(performed_item());
        CHECK(ups_final_state_met(attributes, "COMPLETED"));
    }
    SECTION("each attribute under it matters") {
        for (const auto tag : {ups_tags::performed_station_name_code_sequence, ups_tags::performed_workitem_code_sequence,
                               ups_tags::performed_procedure_step_start_datetime, ups_tags::performed_procedure_step_end_datetime,
                               ups_tags::output_information_sequence}) {
            auto item = performed_item();
            item.remove(tag);
            core::dicom_dataset incomplete;
            incomplete.get_or_create_sequence(ups_tags::performed_procedure_sequence).push_back(item);
            CHECK_FALSE(ups_final_state_met(incomplete, "COMPLETED"));
        }
    }
    SECTION("CANCELED needs the cancellation time") {
        CHECK_FALSE(ups_final_state_met(attributes, "CANCELED"));
        core::dicom_dataset progress;
        progress.set_string(ups_tags::procedure_step_cancellation_datetime, vr_type::DT, "20261102120300");
        attributes.get_or_create_sequence(ups_tags::procedure_step_progress_information_sequence).push_back(progress);
        CHECK(ups_final_state_met(attributes, "CANCELED"));
        CHECK_FALSE(ups_final_state_met(attributes, "COMPLETED"));
    }
}

TEST_CASE("a handler's status reaches the wire, and nothing else does", "[services][ups]") {
    CHECK(ups_status_from_error_code(status_ups_wrong_transaction) == status_ups_wrong_transaction);
    CHECK(ups_status_from_error_code(status_ups_already_canceled) == status_ups_already_canceled);
    CHECK(ups_status_from_error_code(network::dimse::status_error_duplicate_sop_instance) ==
          network::dimse::status_error_duplicate_sop_instance);
    CHECK(ups_status_from_error_code(-1) == network::dimse::status_error_unable_to_process);
    CHECK(ups_status_from_error_code(0) == network::dimse::status_error_unable_to_process);
}

TEST_CASE("each UPS SOP Class carries the operations CC.2 gives it", "[services][ups]") {
    using namespace network::dimse;
    const auto action = [](uint16_t type) {
        return make_n_action_rq(1, ups_push_sop_class_uid, "1.2.3", type);
    };
    const auto carries = [](std::string_view context, const dimse_message& request) {
        return ups_context_carries(context, request);
    };

    SECTION("N-CREATE only on UPS Push") {
        const dimse_message create{command_field::n_create_rq, 1};
        CHECK(carries(ups_push_sop_class_uid, create));
        CHECK_FALSE(carries(ups_pull_sop_class_uid, create));
        CHECK_FALSE(carries(ups_watch_sop_class_uid, create));
        CHECK_FALSE(carries(ups_query_find_sop_class_uid, create));
    }
    SECTION("N-SET and Change UPS State only on UPS Pull") {
        const dimse_message set{command_field::n_set_rq, 1};
        CHECK(carries(ups_pull_sop_class_uid, set));
        CHECK_FALSE(carries(ups_push_sop_class_uid, set));
        CHECK(carries(ups_pull_sop_class_uid, action(ups_action_change_state)));
        CHECK_FALSE(carries(ups_push_sop_class_uid, action(ups_action_change_state)));
        CHECK_FALSE(carries(ups_watch_sop_class_uid, action(ups_action_change_state)));
    }
    SECTION("Request UPS Cancel on UPS Push and UPS Watch") {
        CHECK(carries(ups_push_sop_class_uid, action(ups_action_request_cancel)));
        CHECK(carries(ups_watch_sop_class_uid, action(ups_action_request_cancel)));
        CHECK_FALSE(carries(ups_pull_sop_class_uid, action(ups_action_request_cancel)));
    }
    SECTION("subscriptions only on UPS Watch") {
        for (const uint16_t type : {ups_watch_action_subscribe, ups_watch_action_unsubscribe, ups_watch_action_suspend_global}) {
            CHECK(carries(ups_watch_sop_class_uid, action(type)));
            CHECK_FALSE(carries(ups_push_sop_class_uid, action(type)));
            CHECK_FALSE(carries(ups_pull_sop_class_uid, action(type)));
        }
    }
    SECTION("C-FIND everywhere but UPS Push; N-GET everywhere") {
        const dimse_message find{command_field::c_find_rq, 1};
        const dimse_message get{command_field::n_get_rq, 1};
        CHECK_FALSE(carries(ups_push_sop_class_uid, find));
        for (const auto context : {ups_pull_sop_class_uid, ups_watch_sop_class_uid, ups_query_find_sop_class_uid}) {
            CHECK(carries(context, find));
        }
        for (const auto context : {ups_push_sop_class_uid, ups_pull_sop_class_uid, ups_watch_sop_class_uid, ups_query_find_sop_class_uid}) {
            CHECK(carries(context, get));
        }
    }
}

TEST_CASE("the action types and UIDs are those of the standard", "[services][ups]") {
    CHECK(ups_action_change_state == 1);
    CHECK(ups_action_request_cancel == 2);
    CHECK(ups_watch_action_subscribe == 3);
    CHECK(ups_watch_action_unsubscribe == 4);
    CHECK(ups_watch_action_suspend_global == 5);
    CHECK(ups_query_find_sop_class_uid == "1.2.840.10008.5.1.4.34.6.5");
    CHECK(ups_event_sop_class_uid == "1.2.840.10008.5.1.4.34.6.4");
}

TEST_CASE("the dictionary reads what these services receive in Implicit VR", "[services][ups]") {
    auto& dictionary = core::dicom_dictionary::instance();
    const auto vr_of = [&](core::dicom_tag tag) {
        const auto info = dictionary.find(tag);
        REQUIRE(info.has_value());
        return static_cast<vr_type>(info->vr);
    };
    CHECK(vr_of(core::dicom_tag{0x0008, 0x1199}) == vr_type::SQ);
    CHECK(vr_of(core::dicom_tag{0x0008, 0x1198}) == vr_type::SQ);
    CHECK(vr_of(core::dicom_tag{0x0008, 0x1197}) == vr_type::US);
    CHECK(vr_of(ups_tags::procedure_step_state) == vr_type::CS);
    CHECK(vr_of(ups_tags::receiving_ae) == vr_type::AE);
    CHECK(vr_of(ups_tags::performed_procedure_sequence) == vr_type::SQ);
    CHECK(vr_of(core::dicom_tag{0x2020, 0x0110}) == vr_type::SQ);
    CHECK(vr_of(core::dicom_tag{0x2010, 0x0500}) == vr_type::SQ);
}

TEST_CASE("ups_index keeps a workitem whole", "[services][ups]") {
    scratch_directory directory;
    auto database = open_index();
    ups_index index(*database, directory.path, unreachable_sender());
    const std::string uid = "1.2.826.0.1.3680043.8.498.77";
    REQUIRE(index.create(workitem_for(uid)).is_ok());

    SECTION("creating adds what the SCP must add and keeps no lock") {
        auto found = index.get(uid);
        REQUIRE(found.is_ok());
        const auto& attributes = found.value().attributes;
        CHECK(found.value().state == "SCHEDULED");
        CHECK(found.value().transaction_uid.empty());
        CHECK(attributes.get_string(core::dicom_tag{0x0008, 0x0016}) == ups_push_sop_class_uid);
        CHECK(attributes.get_string(core::dicom_tag{0x0008, 0x0018}) == uid);
        CHECK(attributes.get_string(ups_tags::worklist_label) == "DEFAULT");
        CHECK_FALSE(attributes.get_string(ups_tags::scheduled_procedure_step_modification_datetime).empty());
        CHECK_FALSE(attributes.contains(ups_tags::transaction_uid));
    }
    SECTION("a UID that is not one names no file") {
        for (const auto* bad : {"../../escape", "1.2/3", "", ".1.2"}) {
            CHECK(error_of(index.create(workitem_for(bad))) == network::dimse::status_error_invalid_attribute_value);
        }
    }
    SECTION("a priority the standard does not have is refused and leaves nothing") {
        auto workitem = workitem_for("1.2.826.0.1.3680043.8.498.78");
        workitem.attributes.set_string(ups_tags::scheduled_procedure_step_priority, vr_type::CS, "URGENT");
        CHECK(error_of(index.create(workitem)) == network::dimse::status_error_invalid_attribute_value);
        CHECK(index.get(workitem.workitem_uid).is_err());
    }
    SECTION("N-SET merges and the index follows") {
        core::dicom_dataset changes;
        changes.set_string(ups_tags::procedure_step_label, vr_type::LO, "Renamed");
        REQUIRE(index.set(uid, changes).is_ok());
        auto found = index.get(uid);
        REQUIRE(found.is_ok());
        CHECK(found.value().procedure_step_label == "Renamed");
        CHECK(found.value().attributes.get_string(ups_tags::procedure_step_label) == "Renamed");
        CHECK(found.value().attributes.get_string(ups_tags::input_readiness_state) == "READY");
    }
    SECTION("state and lock reach the database") {
        REQUIRE(index.change_state(uid, "IN PROGRESS", "9.8.7").is_ok());
        auto found = index.get(uid);
        REQUIRE(found.is_ok());
        CHECK(found.value().state == "IN PROGRESS");
        CHECK(found.value().transaction_uid == "9.8.7");
        CHECK(found.value().attributes.get_string(ups_tags::procedure_step_state) == "IN PROGRESS");
    }
    SECTION("a cancel request on a SCHEDULED step ends it, with the reason and the time") {
        core::dicom_dataset request;
        request.set_string(ups_tags::reason_for_cancellation, vr_type::LT, "Not needed");
        REQUIRE(index.request_cancel(uid, request, "CALLER").is_ok());
        auto found = index.get(uid);
        REQUIRE(found.is_ok());
        CHECK(found.value().state == "CANCELED");
        CHECK(found.value().attributes.get_string(ups_tags::reason_for_cancellation) == "Not needed");
        CHECK(ups_final_state_met(found.value().attributes, "CANCELED"));
    }
    SECTION("a cancel request on a step in progress needs someone to tell") {
        REQUIRE(index.change_state(uid, "IN PROGRESS", "9.8.7").is_ok());
        CHECK(error_of(index.request_cancel(uid, core::dicom_dataset{}, "CALLER")) == status_ups_performer_unreachable);
        CHECK(index.get(uid).value().state == "IN PROGRESS");
        REQUIRE(index.subscribe("WATCHER", uid, false).is_ok());
        CHECK(index.request_cancel(uid, core::dicom_dataset{}, "CALLER").is_ok());
        CHECK(index.get(uid).value().state == "IN PROGRESS");
    }
    SECTION("subscribing names a known AE and an existing step") {
        CHECK(error_of(index.subscribe("NOBODY", uid, false)) == status_ups_unknown_receiving_ae);
        CHECK(error_of(index.subscribe("WATCHER", "1.2.3.4.404", false)) == status_ups_no_such_instance);
        CHECK(error_of(index.unsubscribe("WATCHER", "1.2.3.4.404")) == status_ups_no_such_instance);
        REQUIRE(index.subscribe("WATCHER", uid, true).is_ok());
    }
    SECTION("suspending keeps what the AE subscribed to; unsubscribing globally does not") {
        REQUIRE(index.subscribe("WATCHER", "", false).is_ok());
        auto rows = database->get_ups_subscriptions("WATCHER");
        REQUIRE(rows.is_ok());
        CHECK(rows.value().size() == 2);  // The global row, and the step that existed.

        REQUIRE(index.suspend("WATCHER").is_ok());
        rows = database->get_ups_subscriptions("WATCHER");
        REQUIRE(rows.is_ok());
        REQUIRE(rows.value().size() == 1);
        CHECK(rows.value().front().workitem_uid == uid);

        REQUIRE(index.unsubscribe("WATCHER", "").is_ok());
        rows = database->get_ups_subscriptions("WATCHER");
        REQUIRE(rows.is_ok());
        CHECK(rows.value().empty());
    }
    SECTION("a step created under a global subscription joins it, lock included") {
        REQUIRE(index.subscribe("WATCHER", "", true).is_ok());
        const std::string later = "1.2.826.0.1.3680043.8.498.79";
        REQUIRE(index.create(workitem_for(later)).is_ok());
        auto rows = database->get_ups_subscriptions("WATCHER");
        REQUIRE(rows.is_ok());
        bool joined = false;
        for (const auto& row : rows.value()) {
            if (row.workitem_uid == later) {
                joined = row.deletion_lock;
            }
        }
        CHECK(joined);
    }
    SECTION("C-FIND matches, answers the keys asked, and never returns the lock") {
        REQUIRE(index.change_state(uid, "IN PROGRESS", "9.8.7").is_ok());
        core::dicom_dataset keys;
        keys.set_string(ups_tags::procedure_step_state, vr_type::CS, "IN PROGRESS");
        keys.set_string(ups_tags::transaction_uid, vr_type::UI, "");
        keys.set_string(core::dicom_tag{0x0010, 0x0020}, vr_type::LO, "");
        auto rows = index.find(keys);
        REQUIRE(rows.size() == 1);
        CHECK(rows.front().get_string(core::dicom_tag{0x0008, 0x0018}) == uid);
        CHECK_FALSE(rows.front().contains(ups_tags::transaction_uid));
        CHECK(rows.front().contains(core::dicom_tag{0x0010, 0x0020}));

        keys.set_string(ups_tags::procedure_step_state, vr_type::CS, "SCHEDULED");
        CHECK(index.find(keys).empty());
    }
    SECTION("a date range on a DT key") {
        core::dicom_dataset keys;
        keys.set_string(ups_tags::scheduled_procedure_step_start_datetime, vr_type::DT, "20261101-20261103");
        CHECK(index.find(keys).size() == 1);
        keys.set_string(ups_tags::scheduled_procedure_step_start_datetime, vr_type::DT, "20261201-");
        CHECK(index.find(keys).empty());
    }
}

TEST_CASE("event_sender only takes reports it can address, and keeps trying as told", "[services][ups]") {
    event_sender_config config;
    config.local_ae_title = "TEST_SCP";
    config.attempts = 3;
    config.retry_delay = std::chrono::milliseconds{5};
    config.timeout = std::chrono::milliseconds{200};
    event_sender sender(config, [](const std::string& ae_title) -> std::optional<std::pair<std::string, uint16_t>> {
        if (ae_title == "DEAD") {
            return std::make_pair(std::string("127.0.0.1"), uint16_t{1});
        }
        return std::nullopt;
    });

    std::mutex mutex;
    std::vector<int> attempts;
    sender.set_observer([&](const event_report&, std::optional<uint16_t> status, int made) {
        std::lock_guard<std::mutex> lock(mutex);
        CHECK_FALSE(status.has_value());
        attempts.push_back(made);
    });

    event_report report;
    report.receiving_ae = "NOBODY";
    CHECK_FALSE(sender.knows("NOBODY"));
    CHECK_FALSE(sender.send(report));

    report.receiving_ae = "DEAD";
    report.abstract_syntax = report.sop_class_uid = std::string(storage_commitment_push_model_sop_class_uid);
    report.sop_instance_uid = "1.2.3";
    report.event_type_id = 1;
    CHECK(sender.knows("DEAD"));
    CHECK(sender.send(report));

    for (int waited = 0; waited < 500 && sender.abandoned() == 0; ++waited) {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    CHECK(sender.abandoned() == 1);
    CHECK(sender.delivered() == 0);
    std::lock_guard<std::mutex> lock(mutex);
    CHECK(attempts == std::vector<int>{3});
}
