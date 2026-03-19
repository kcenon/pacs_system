/**
 * @file ups_workitem_test.cpp
 * @brief Unit tests for index_database UPS operations
 *
 * Tests CRUD operations for the ups_workitems and ups_subscriptions tables.
 * Validates UPS state machine transitions per PS3.4 Annex CC.
 */

#include <pacs/storage/index_database.hpp>
#include <pacs/services/sop_class_registry.hpp>

#include <catch2/catch_test_macros.hpp>

#include <memory>

using namespace kcenon::pacs::storage;

namespace {

auto create_test_database() -> std::unique_ptr<index_database> {
    auto result = index_database::open(":memory:");
    REQUIRE(result.is_ok());
    return std::move(result.value());
}

auto create_test_workitem(index_database& db, const std::string& uid,
                           const std::string& label = "Test Procedure",
                           const std::string& priority = "MEDIUM") -> int64_t {
    ups_workitem item;
    item.workitem_uid = uid;
    item.procedure_step_label = label;
    item.priority = priority;
    item.scheduled_start_datetime = "20240115090000";
    auto result = db.create_ups_workitem(item);
    REQUIRE(result.is_ok());
    return result.value();
}

}  // namespace

// ============================================================================
// UPS Data Model Tests
// ============================================================================

TEST_CASE("UPS state enum conversion", "[storage][ups]") {
    SECTION("to_string produces correct values") {
        CHECK(to_string(ups_state::scheduled) == "SCHEDULED");
        CHECK(to_string(ups_state::in_progress) == "IN PROGRESS");
        CHECK(to_string(ups_state::completed) == "COMPLETED");
        CHECK(to_string(ups_state::canceled) == "CANCELED");
    }

    SECTION("parse_ups_state round-trips correctly") {
        CHECK(parse_ups_state("SCHEDULED") == ups_state::scheduled);
        CHECK(parse_ups_state("IN PROGRESS") == ups_state::in_progress);
        CHECK(parse_ups_state("COMPLETED") == ups_state::completed);
        CHECK(parse_ups_state("CANCELED") == ups_state::canceled);
        CHECK_FALSE(parse_ups_state("INVALID").has_value());
    }
}

TEST_CASE("UPS priority enum conversion", "[storage][ups]") {
    SECTION("to_string produces correct values") {
        CHECK(to_string(ups_priority::low) == "LOW");
        CHECK(to_string(ups_priority::medium) == "MEDIUM");
        CHECK(to_string(ups_priority::high) == "HIGH");
    }

    SECTION("parse_ups_priority round-trips correctly") {
        CHECK(parse_ups_priority("LOW") == ups_priority::low);
        CHECK(parse_ups_priority("MEDIUM") == ups_priority::medium);
        CHECK(parse_ups_priority("HIGH") == ups_priority::high);
        CHECK_FALSE(parse_ups_priority("INVALID").has_value());
    }
}

TEST_CASE("UPS workitem struct helpers", "[storage][ups]") {
    ups_workitem item;

    SECTION("Empty workitem is invalid") {
        CHECK_FALSE(item.is_valid());
    }

    SECTION("Workitem with UID is valid") {
        item.workitem_uid = "1.2.3.4.5";
        CHECK(item.is_valid());
    }

    SECTION("Final state detection") {
        item.state = "SCHEDULED";
        CHECK_FALSE(item.is_final());

        item.state = "IN PROGRESS";
        CHECK_FALSE(item.is_final());

        item.state = "COMPLETED";
        CHECK(item.is_final());

        item.state = "CANCELED";
        CHECK(item.is_final());
    }
}

TEST_CASE("UPS subscription struct helpers", "[storage][ups]") {
    ups_subscription sub;

    SECTION("Global subscription detection") {
        sub.subscriber_ae = "SCU_AE";
        CHECK(sub.is_global());
        CHECK_FALSE(sub.is_workitem_specific());
    }

    SECTION("Workitem-specific subscription") {
        sub.subscriber_ae = "SCU_AE";
        sub.workitem_uid = "1.2.3.4.5";
        CHECK_FALSE(sub.is_global());
        CHECK(sub.is_workitem_specific());
    }
}

// ============================================================================
// UPS Workitem CRUD Tests
// ============================================================================

TEST_CASE("UPS: create workitem", "[storage][ups]") {
    auto db = create_test_database();

    ups_workitem item;
    item.workitem_uid = "1.2.3.4.5.100";
    item.procedure_step_label = "CT Head Scan";
    item.worklist_label = "Radiology";
    item.priority = "HIGH";
    item.scheduled_start_datetime = "20240115090000";
    item.scheduled_station_name = "CT_SCANNER_1";

    auto result = db->create_ups_workitem(item);
    REQUIRE(result.is_ok());
    CHECK(result.value() > 0);

    // Verify workitem was created with SCHEDULED state
    auto found = db->find_ups_workitem("1.2.3.4.5.100");
    REQUIRE(found.has_value());
    CHECK(found->workitem_uid == "1.2.3.4.5.100");
    CHECK(found->state == "SCHEDULED");
    CHECK(found->procedure_step_label == "CT Head Scan");
    CHECK(found->worklist_label == "Radiology");
    CHECK(found->priority == "HIGH");
    CHECK(found->scheduled_start_datetime == "20240115090000");
    CHECK(found->scheduled_station_name == "CT_SCANNER_1");
}

TEST_CASE("UPS: create workitem requires UID", "[storage][ups]") {
    auto db = create_test_database();

    ups_workitem item;
    auto result = db->create_ups_workitem(item);
    REQUIRE(result.is_err());
}

TEST_CASE("UPS: create workitem with default priority", "[storage][ups]") {
    auto db = create_test_database();

    ups_workitem item;
    item.workitem_uid = "1.2.3.4.5.101";
    auto result = db->create_ups_workitem(item);
    REQUIRE(result.is_ok());

    auto found = db->find_ups_workitem("1.2.3.4.5.101");
    REQUIRE(found.has_value());
    CHECK(found->priority == "MEDIUM");
}

TEST_CASE("UPS: find workitem not found", "[storage][ups]") {
    auto db = create_test_database();

    auto found = db->find_ups_workitem("nonexistent.uid");
    CHECK_FALSE(found.has_value());
}

TEST_CASE("UPS: update workitem", "[storage][ups]") {
    auto db = create_test_database();
    create_test_workitem(*db, "1.2.3.4.5.200");

    ups_workitem updated;
    updated.workitem_uid = "1.2.3.4.5.200";
    updated.procedure_step_label = "Updated Label";
    updated.priority = "HIGH";
    updated.progress_description = "In preparation";
    updated.progress_percent = 10;

    auto result = db->update_ups_workitem(updated);
    REQUIRE(result.is_ok());

    auto found = db->find_ups_workitem("1.2.3.4.5.200");
    REQUIRE(found.has_value());
    CHECK(found->procedure_step_label == "Updated Label");
    CHECK(found->priority == "HIGH");
    CHECK(found->progress_description == "In preparation");
    CHECK(found->progress_percent == 10);
}

TEST_CASE("UPS: cannot update final state workitem", "[storage][ups]") {
    auto db = create_test_database();
    create_test_workitem(*db, "1.2.3.4.5.201");

    // Transition to IN PROGRESS, then COMPLETED
    auto claim = db->change_ups_state("1.2.3.4.5.201", "IN PROGRESS", "1.2.3.99.1");
    REQUIRE(claim.is_ok());
    auto complete = db->change_ups_state("1.2.3.4.5.201", "COMPLETED");
    REQUIRE(complete.is_ok());

    // Try to update
    ups_workitem updated;
    updated.workitem_uid = "1.2.3.4.5.201";
    updated.procedure_step_label = "Should Fail";
    auto result = db->update_ups_workitem(updated);
    REQUIRE(result.is_err());
}

TEST_CASE("UPS: delete workitem", "[storage][ups]") {
    auto db = create_test_database();
    create_test_workitem(*db, "1.2.3.4.5.300");

    auto result = db->delete_ups_workitem("1.2.3.4.5.300");
    REQUIRE(result.is_ok());

    auto found = db->find_ups_workitem("1.2.3.4.5.300");
    CHECK_FALSE(found.has_value());
}

TEST_CASE("UPS: workitem count", "[storage][ups]") {
    auto db = create_test_database();

    SECTION("Empty database") {
        auto count = db->ups_workitem_count();
        REQUIRE(count.is_ok());
        CHECK(count.value() == 0);
    }

    SECTION("Count after insertion") {
        create_test_workitem(*db, "1.2.3.4.5.400");
        create_test_workitem(*db, "1.2.3.4.5.401");
        create_test_workitem(*db, "1.2.3.4.5.402");

        auto count = db->ups_workitem_count();
        REQUIRE(count.is_ok());
        CHECK(count.value() == 3);
    }

    SECTION("Count by state") {
        create_test_workitem(*db, "1.2.3.4.5.410");
        create_test_workitem(*db, "1.2.3.4.5.411");

        // Move one to IN PROGRESS
        auto claim_result = db->change_ups_state("1.2.3.4.5.411", "IN PROGRESS", "1.2.3.99.2");
        REQUIRE(claim_result.is_ok());

        auto scheduled_count = db->ups_workitem_count("SCHEDULED");
        REQUIRE(scheduled_count.is_ok());
        CHECK(scheduled_count.value() == 1);

        auto progress_count = db->ups_workitem_count("IN PROGRESS");
        REQUIRE(progress_count.is_ok());
        CHECK(progress_count.value() == 1);
    }
}

// ============================================================================
// UPS State Machine Tests
// ============================================================================

TEST_CASE("UPS: state transition SCHEDULED -> IN PROGRESS", "[storage][ups]") {
    auto db = create_test_database();
    create_test_workitem(*db, "1.2.3.4.5.500");

    auto result = db->change_ups_state("1.2.3.4.5.500", "IN PROGRESS", "1.2.3.99.1");
    REQUIRE(result.is_ok());

    auto found = db->find_ups_workitem("1.2.3.4.5.500");
    REQUIRE(found.has_value());
    CHECK(found->state == "IN PROGRESS");
    CHECK(found->transaction_uid == "1.2.3.99.1");
}

TEST_CASE("UPS: state transition SCHEDULED -> CANCELED", "[storage][ups]") {
    auto db = create_test_database();
    create_test_workitem(*db, "1.2.3.4.5.501");

    auto result = db->change_ups_state("1.2.3.4.5.501", "CANCELED");
    REQUIRE(result.is_ok());

    auto found = db->find_ups_workitem("1.2.3.4.5.501");
    REQUIRE(found.has_value());
    CHECK(found->state == "CANCELED");
}

TEST_CASE("UPS: state transition IN PROGRESS -> COMPLETED", "[storage][ups]") {
    auto db = create_test_database();
    create_test_workitem(*db, "1.2.3.4.5.502");

    auto claim_result = db->change_ups_state("1.2.3.4.5.502", "IN PROGRESS", "1.2.3.99.1");
    REQUIRE(claim_result.is_ok());

    auto result = db->change_ups_state("1.2.3.4.5.502", "COMPLETED");
    REQUIRE(result.is_ok());

    auto found = db->find_ups_workitem("1.2.3.4.5.502");
    REQUIRE(found.has_value());
    CHECK(found->state == "COMPLETED");
    CHECK(found->is_final());
}

TEST_CASE("UPS: state transition IN PROGRESS -> CANCELED", "[storage][ups]") {
    auto db = create_test_database();
    create_test_workitem(*db, "1.2.3.4.5.503");

    auto claim_result = db->change_ups_state("1.2.3.4.5.503", "IN PROGRESS", "1.2.3.99.1");
    REQUIRE(claim_result.is_ok());

    auto result = db->change_ups_state("1.2.3.4.5.503", "CANCELED");
    REQUIRE(result.is_ok());

    auto found = db->find_ups_workitem("1.2.3.4.5.503");
    REQUIRE(found.has_value());
    CHECK(found->state == "CANCELED");
}

TEST_CASE("UPS: invalid state transition SCHEDULED -> COMPLETED", "[storage][ups]") {
    auto db = create_test_database();
    create_test_workitem(*db, "1.2.3.4.5.504");

    auto result = db->change_ups_state("1.2.3.4.5.504", "COMPLETED");
    REQUIRE(result.is_err());
}

TEST_CASE("UPS: cannot transition from final state", "[storage][ups]") {
    auto db = create_test_database();
    create_test_workitem(*db, "1.2.3.4.5.505");

    auto claim_result = db->change_ups_state("1.2.3.4.5.505", "IN PROGRESS", "1.2.3.99.1");
    REQUIRE(claim_result.is_ok());
    auto complete_result = db->change_ups_state("1.2.3.4.5.505", "COMPLETED");
    REQUIRE(complete_result.is_ok());

    auto result = db->change_ups_state("1.2.3.4.5.505", "IN PROGRESS", "1.2.3.99.2");
    REQUIRE(result.is_err());
}

TEST_CASE("UPS: IN PROGRESS requires transaction UID", "[storage][ups]") {
    auto db = create_test_database();
    create_test_workitem(*db, "1.2.3.4.5.506");

    auto result = db->change_ups_state("1.2.3.4.5.506", "IN PROGRESS");
    REQUIRE(result.is_err());
}

TEST_CASE("UPS: invalid state string rejected", "[storage][ups]") {
    auto db = create_test_database();
    create_test_workitem(*db, "1.2.3.4.5.507");

    auto result = db->change_ups_state("1.2.3.4.5.507", "INVALID_STATE");
    REQUIRE(result.is_err());
}

TEST_CASE("UPS: state change on nonexistent workitem", "[storage][ups]") {
    auto db = create_test_database();

    auto result = db->change_ups_state("nonexistent.uid", "IN PROGRESS", "1.2.3.99.1");
    REQUIRE(result.is_err());
}

// ============================================================================
// UPS Search Tests
// ============================================================================

TEST_CASE("UPS: search by state", "[storage][ups]") {
    auto db = create_test_database();
    create_test_workitem(*db, "1.2.3.4.5.600");
    create_test_workitem(*db, "1.2.3.4.5.601");
    create_test_workitem(*db, "1.2.3.4.5.602");

    // Move one to IN PROGRESS
    auto claim_result = db->change_ups_state("1.2.3.4.5.602", "IN PROGRESS", "1.2.3.99.1");
    REQUIRE(claim_result.is_ok());

    ups_workitem_query query;
    query.state = "SCHEDULED";
    auto result = db->search_ups_workitems(query);
    REQUIRE(result.is_ok());
    CHECK(result.value().size() == 2);
}

TEST_CASE("UPS: search by priority", "[storage][ups]") {
    auto db = create_test_database();
    create_test_workitem(*db, "1.2.3.4.5.610", "Label A", "HIGH");
    create_test_workitem(*db, "1.2.3.4.5.611", "Label B", "LOW");
    create_test_workitem(*db, "1.2.3.4.5.612", "Label C", "HIGH");

    ups_workitem_query query;
    query.priority = "HIGH";
    auto result = db->search_ups_workitems(query);
    REQUIRE(result.is_ok());
    CHECK(result.value().size() == 2);
}

TEST_CASE("UPS: search with limit and offset", "[storage][ups]") {
    auto db = create_test_database();
    for (int i = 0; i < 5; ++i) {
        create_test_workitem(*db, "1.2.3.4.5.62" + std::to_string(i));
    }

    ups_workitem_query query;
    query.limit = 2;
    auto result = db->search_ups_workitems(query);
    REQUIRE(result.is_ok());
    CHECK(result.value().size() == 2);

    query.offset = 3;
    result = db->search_ups_workitems(query);
    REQUIRE(result.is_ok());
    CHECK(result.value().size() == 2);
}

TEST_CASE("UPS: search with no criteria returns all", "[storage][ups]") {
    auto db = create_test_database();
    create_test_workitem(*db, "1.2.3.4.5.630");
    create_test_workitem(*db, "1.2.3.4.5.631");

    ups_workitem_query query;
    auto result = db->search_ups_workitems(query);
    REQUIRE(result.is_ok());
    CHECK(result.value().size() == 2);
}

// ============================================================================
// UPS Subscription Tests
// ============================================================================

TEST_CASE("UPS: subscribe to workitem", "[storage][ups]") {
    auto db = create_test_database();
    create_test_workitem(*db, "1.2.3.4.5.700");

    ups_subscription sub;
    sub.subscriber_ae = "SUBSCRIBER_AE";
    sub.workitem_uid = "1.2.3.4.5.700";
    sub.deletion_lock = true;

    auto result = db->subscribe_ups(sub);
    REQUIRE(result.is_ok());
    CHECK(result.value() > 0);

    auto subs = db->get_ups_subscriptions("SUBSCRIBER_AE");
    REQUIRE(subs.is_ok());
    REQUIRE(subs.value().size() == 1);
    CHECK(subs.value()[0].subscriber_ae == "SUBSCRIBER_AE");
    CHECK(subs.value()[0].workitem_uid == "1.2.3.4.5.700");
    CHECK(subs.value()[0].deletion_lock == true);
}

TEST_CASE("UPS: global subscription", "[storage][ups]") {
    auto db = create_test_database();

    ups_subscription sub;
    sub.subscriber_ae = "GLOBAL_SUB";

    auto result = db->subscribe_ups(sub);
    REQUIRE(result.is_ok());

    auto subs = db->get_ups_subscriptions("GLOBAL_SUB");
    REQUIRE(subs.is_ok());
    REQUIRE(subs.value().size() == 1);
    CHECK(subs.value()[0].workitem_uid.empty());
}

TEST_CASE("UPS: subscribe requires AE title", "[storage][ups]") {
    auto db = create_test_database();

    ups_subscription sub;
    auto result = db->subscribe_ups(sub);
    REQUIRE(result.is_err());
}

TEST_CASE("UPS: unsubscribe from workitem", "[storage][ups]") {
    auto db = create_test_database();
    create_test_workitem(*db, "1.2.3.4.5.710");

    ups_subscription sub;
    sub.subscriber_ae = "SUB_AE";
    sub.workitem_uid = "1.2.3.4.5.710";
    auto sub_result = db->subscribe_ups(sub);
    REQUIRE(sub_result.is_ok());

    auto unsub_result = db->unsubscribe_ups("SUB_AE", "1.2.3.4.5.710");
    REQUIRE(unsub_result.is_ok());

    auto subs = db->get_ups_subscriptions("SUB_AE");
    REQUIRE(subs.is_ok());
    CHECK(subs.value().empty());
}

TEST_CASE("UPS: unsubscribe all", "[storage][ups]") {
    auto db = create_test_database();

    // Create two subscriptions
    ups_subscription sub1;
    sub1.subscriber_ae = "SUB_AE_ALL";
    sub1.workitem_uid = "1.2.3.4.5.720";
    auto sub1_result = db->subscribe_ups(sub1);
    REQUIRE(sub1_result.is_ok());

    ups_subscription sub2;
    sub2.subscriber_ae = "SUB_AE_ALL";
    sub2.workitem_uid = "1.2.3.4.5.721";
    auto sub2_result = db->subscribe_ups(sub2);
    REQUIRE(sub2_result.is_ok());

    auto unsub_result = db->unsubscribe_ups("SUB_AE_ALL");
    REQUIRE(unsub_result.is_ok());

    auto subs = db->get_ups_subscriptions("SUB_AE_ALL");
    REQUIRE(subs.is_ok());
    CHECK(subs.value().empty());
}

TEST_CASE("UPS: get subscribers for workitem", "[storage][ups]") {
    auto db = create_test_database();
    create_test_workitem(*db, "1.2.3.4.5.730");

    // Workitem-specific subscriber
    ups_subscription sub1;
    sub1.subscriber_ae = "SPECIFIC_SUB";
    sub1.workitem_uid = "1.2.3.4.5.730";
    auto sub1_result = db->subscribe_ups(sub1);
    REQUIRE(sub1_result.is_ok());

    // Global subscriber
    ups_subscription sub2;
    sub2.subscriber_ae = "GLOBAL_SUB";
    auto sub2_result = db->subscribe_ups(sub2);
    REQUIRE(sub2_result.is_ok());

    auto subscribers = db->get_ups_subscribers("1.2.3.4.5.730");
    REQUIRE(subscribers.is_ok());
    CHECK(subscribers.value().size() == 2);
}

// ============================================================================
// SOP Class Registry Tests
// ============================================================================

TEST_CASE("UPS SOP classes registered in registry",
          "[services][sop_class_registry][ups]") {
    // UPS SOP Class UIDs per DICOM PS3.4 Annex CC
    const auto& registry = kcenon::pacs::services::sop_class_registry::instance();

    SECTION("UPS Push SOP Class registered") {
        CHECK(registry.is_supported("1.2.840.10008.5.1.4.34.6.1"));
        auto info = registry.get_info("1.2.840.10008.5.1.4.34.6.1");
        REQUIRE(info != nullptr);
        CHECK(info->category == kcenon::pacs::services::sop_class_category::ups);
    }

    SECTION("UPS Watch SOP Class registered") {
        CHECK(registry.is_supported("1.2.840.10008.5.1.4.34.6.2"));
        auto info = registry.get_info("1.2.840.10008.5.1.4.34.6.2");
        REQUIRE(info != nullptr);
        CHECK(info->category == kcenon::pacs::services::sop_class_category::ups);
    }

    SECTION("UPS Pull SOP Class registered") {
        CHECK(registry.is_supported("1.2.840.10008.5.1.4.34.6.3"));
    }

    SECTION("UPS Event SOP Class registered") {
        CHECK(registry.is_supported("1.2.840.10008.5.1.4.34.6.4"));
    }

    SECTION("UPS Query SOP Class registered") {
        CHECK(registry.is_supported("1.2.840.10008.5.1.4.34.6.5"));
    }

    SECTION("UPS classes are in 'ups' category") {
        auto ups_classes = registry.get_by_category(kcenon::pacs::services::sop_class_category::ups);
        CHECK(ups_classes.size() == 5);
    }
}
