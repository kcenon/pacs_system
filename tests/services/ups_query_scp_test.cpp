/**
 * @file ups_query_scp_test.cpp
 * @brief Unit tests for UPS Query SCP service (C-FIND for workitems)
 */

#include <pacs/services/ups/ups_query_scp.hpp>
#include <pacs/services/ups/ups_push_scp.hpp>
#include <pacs/network/dimse/command_field.hpp>
#include <pacs/network/dimse/dimse_message.hpp>
#include <pacs/network/dimse/status_codes.hpp>
#include <pacs/core/dicom_tag_constants.hpp>
#include <pacs/encoding/vr_type.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace pacs::services;
using namespace pacs::network;
using namespace pacs::network::dimse;
using namespace pacs::core;
using namespace pacs::encoding;

// ============================================================================
// ups_query_scp Construction Tests
// ============================================================================

TEST_CASE("ups_query_scp construction", "[services][ups][query]") {
    ups_query_scp scp;

    SECTION("service name is correct") {
        CHECK(scp.service_name() == "UPS Query SCP");
    }

    SECTION("supports one SOP class") {
        auto classes = scp.supported_sop_classes();
        CHECK(classes.size() == 1);
    }

    SECTION("default max_results is unlimited (0)") {
        CHECK(scp.max_results() == 0);
    }

    SECTION("initial queries_processed is zero") {
        CHECK(scp.queries_processed() == 0);
    }

    SECTION("initial items_returned is zero") {
        CHECK(scp.items_returned() == 0);
    }
}

// ============================================================================
// SOP Class Support Tests
// ============================================================================

TEST_CASE("ups_query_scp SOP class support", "[services][ups][query]") {
    ups_query_scp scp;

    SECTION("supports UPS Query FIND") {
        CHECK(scp.supports_sop_class("1.2.840.10008.5.1.4.34.6.4"));
        CHECK(scp.supports_sop_class(ups_query_find_sop_class_uid));
    }

    SECTION("does not support UPS Push SOP Class") {
        CHECK_FALSE(scp.supports_sop_class("1.2.840.10008.5.1.4.34.6.1"));
    }

    SECTION("does not support Modality Worklist FIND") {
        CHECK_FALSE(scp.supports_sop_class("1.2.840.10008.5.1.4.31"));
    }

    SECTION("does not support non-UPS SOP classes") {
        CHECK_FALSE(scp.supports_sop_class("1.2.840.10008.1.1"));
        CHECK_FALSE(scp.supports_sop_class("1.2.840.10008.5.1.4.1.1.2"));
        CHECK_FALSE(scp.supports_sop_class(""));
    }
}

// ============================================================================
// SOP Class UID Constant Tests
// ============================================================================

TEST_CASE("ups_query_find_sop_class_uid constant", "[services][ups][query]") {
    CHECK(ups_query_find_sop_class_uid == "1.2.840.10008.5.1.4.34.6.4");
}

// ============================================================================
// Configuration Tests
// ============================================================================

TEST_CASE("ups_query_scp configuration", "[services][ups][query]") {
    ups_query_scp scp;

    SECTION("set_max_results updates max_results") {
        scp.set_max_results(50);
        CHECK(scp.max_results() == 50);

        scp.set_max_results(0);
        CHECK(scp.max_results() == 0);

        scp.set_max_results(1000);
        CHECK(scp.max_results() == 1000);
    }

    SECTION("set_handler accepts lambda") {
        bool handler_called = false;
        scp.set_handler([&handler_called](
            [[maybe_unused]] const dicom_dataset& keys,
            [[maybe_unused]] const std::string& ae) {
            handler_called = true;
            return std::vector<dicom_dataset>{};
        });
        CHECK_FALSE(handler_called);
    }

    SECTION("set_cancel_check accepts lambda") {
        bool cancel_called = false;
        scp.set_cancel_check([&cancel_called]() {
            cancel_called = true;
            return false;
        });
        CHECK_FALSE(cancel_called);
    }
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_CASE("ups_query_scp statistics", "[services][ups][query]") {
    ups_query_scp scp;

    SECTION("queries_processed starts at zero") {
        CHECK(scp.queries_processed() == 0);
    }

    SECTION("items_returned starts at zero") {
        CHECK(scp.items_returned() == 0);
    }

    SECTION("reset_statistics resets all counters") {
        scp.reset_statistics();
        CHECK(scp.queries_processed() == 0);
        CHECK(scp.items_returned() == 0);
    }
}

// ============================================================================
// C-FIND Message Factory Tests (with UPS Query SOP Class)
// ============================================================================

TEST_CASE("make_c_find_rq for UPS Query creates valid request", "[services][ups][query]") {
    auto request = make_c_find_rq(42, ups_query_find_sop_class_uid);

    CHECK(request.command() == command_field::c_find_rq);
    CHECK(request.message_id() == 42);
    CHECK(request.affected_sop_class_uid() == "1.2.840.10008.5.1.4.34.6.4");
    CHECK(request.is_request());
    CHECK_FALSE(request.is_response());
}

TEST_CASE("make_c_find_rsp for UPS Query creates valid response", "[services][ups][query]") {
    SECTION("pending response") {
        auto response = make_c_find_rsp(42, ups_query_find_sop_class_uid, status_pending);

        CHECK(response.command() == command_field::c_find_rsp);
        CHECK(response.message_id_responded_to() == 42);
        CHECK(response.affected_sop_class_uid() == "1.2.840.10008.5.1.4.34.6.4");
        CHECK(response.status() == status_pending);
        CHECK(response.is_response());
    }

    SECTION("success response") {
        auto response = make_c_find_rsp(123, ups_query_find_sop_class_uid, status_success);

        CHECK(response.status() == status_success);
    }

    SECTION("cancel response") {
        auto response = make_c_find_rsp(456, ups_query_find_sop_class_uid, status_cancel);

        CHECK(response.status() == status_cancel);
    }
}

// ============================================================================
// Handler Tests
// ============================================================================

TEST_CASE("ups_query_handler function type", "[services][ups][query]") {
    ups_query_handler handler = [](
        [[maybe_unused]] const dicom_dataset& query_keys,
        [[maybe_unused]] const std::string& calling_ae) {
        std::vector<dicom_dataset> results;

        dicom_dataset item;
        item.set_string(ups_tags::procedure_step_state, vr_type::CS, "SCHEDULED");
        item.set_string(ups_tags::procedure_step_label, vr_type::LO, "AI Analysis");
        item.set_string(ups_tags::scheduled_procedure_step_priority, vr_type::CS, "HIGH");
        results.push_back(item);

        return results;
    };

    dicom_dataset query;
    std::string ae = "AI_ENGINE";

    auto results = handler(query, ae);

    CHECK(results.size() == 1);
    CHECK(results[0].get_string(ups_tags::procedure_step_state) == "SCHEDULED");
    CHECK(results[0].get_string(ups_tags::procedure_step_label) == "AI Analysis");
    CHECK(results[0].get_string(ups_tags::scheduled_procedure_step_priority) == "HIGH");
}

TEST_CASE("ups_query_cancel_check function type", "[services][ups][query]") {
    SECTION("returns false by default") {
        ups_query_cancel_check check = []() {
            return false;
        };
        CHECK_FALSE(check());
    }

    SECTION("can return true to indicate cancel") {
        ups_query_cancel_check check = []() {
            return true;
        };
        CHECK(check());
    }
}

TEST_CASE("ups_query_scp handler integration", "[services][ups][query]") {
    ups_query_scp scp;
    std::vector<dicom_dataset> test_results;

    dicom_dataset item1;
    item1.set_string(ups_tags::procedure_step_state, vr_type::CS, "SCHEDULED");
    item1.set_string(ups_tags::procedure_step_label, vr_type::LO, "CT Review");
    item1.set_string(ups_tags::scheduled_procedure_step_priority, vr_type::CS, "MEDIUM");

    dicom_dataset item2;
    item2.set_string(ups_tags::procedure_step_state, vr_type::CS, "SCHEDULED");
    item2.set_string(ups_tags::procedure_step_label, vr_type::LO, "MR Analysis");
    item2.set_string(ups_tags::scheduled_procedure_step_priority, vr_type::CS, "HIGH");

    test_results.push_back(item1);
    test_results.push_back(item2);

    std::string captured_ae;
    bool handler_called = false;

    scp.set_handler([&](
                        [[maybe_unused]] const dicom_dataset& keys,
                        const std::string& ae) {
        handler_called = true;
        captured_ae = ae;
        return test_results;
    });

    SECTION("handler is stored but not called until handle_message") {
        CHECK_FALSE(handler_called);
    }
}

// ============================================================================
// scp_service Base Class Tests
// ============================================================================

TEST_CASE("ups_query_scp is a scp_service", "[services][ups][query]") {
    std::unique_ptr<scp_service> base_ptr = std::make_unique<ups_query_scp>();

    CHECK(base_ptr->service_name() == "UPS Query SCP");
    CHECK(base_ptr->supported_sop_classes().size() == 1);
    CHECK(base_ptr->supports_sop_class(ups_query_find_sop_class_uid));
}

// ============================================================================
// Multiple Instance Tests
// ============================================================================

TEST_CASE("multiple ups_query_scp instances are independent", "[services][ups][query]") {
    ups_query_scp scp1;
    ups_query_scp scp2;

    scp1.set_max_results(50);
    scp2.set_max_results(200);

    CHECK(scp1.max_results() == 50);
    CHECK(scp2.max_results() == 200);

    scp1.reset_statistics();
    CHECK(scp1.queries_processed() == 0);
    CHECK(scp1.items_returned() == 0);
    CHECK(scp2.queries_processed() == 0);
    CHECK(scp2.items_returned() == 0);
}

// ============================================================================
// UPS Query Key Dataset Creation Tests
// ============================================================================

TEST_CASE("create UPS query keys dataset", "[services][ups][query]") {
    dicom_dataset query_keys;

    SECTION("query by state") {
        query_keys.set_string(ups_tags::procedure_step_state, vr_type::CS, "SCHEDULED");

        CHECK(query_keys.get_string(ups_tags::procedure_step_state) == "SCHEDULED");
    }

    SECTION("query by priority") {
        query_keys.set_string(ups_tags::scheduled_procedure_step_priority, vr_type::CS, "HIGH");

        CHECK(query_keys.get_string(ups_tags::scheduled_procedure_step_priority) == "HIGH");
    }

    SECTION("query by label with wildcard") {
        query_keys.set_string(ups_tags::procedure_step_label, vr_type::LO, "AI*");

        CHECK(query_keys.get_string(ups_tags::procedure_step_label) == "AI*");
    }

    SECTION("query with multiple criteria") {
        query_keys.set_string(ups_tags::procedure_step_state, vr_type::CS, "SCHEDULED");
        query_keys.set_string(ups_tags::scheduled_procedure_step_priority, vr_type::CS, "HIGH");
        query_keys.set_string(ups_tags::worklist_label, vr_type::LO, "");

        CHECK(query_keys.get_string(ups_tags::procedure_step_state) == "SCHEDULED");
        CHECK(query_keys.get_string(ups_tags::scheduled_procedure_step_priority) == "HIGH");
        CHECK(query_keys.get_string(ups_tags::worklist_label).empty());
    }
}

// ============================================================================
// UPS Workitem Result Dataset Tests
// ============================================================================

TEST_CASE("create UPS workitem result dataset", "[services][ups][query]") {
    dicom_dataset workitem;

    workitem.set_string(tags::sop_instance_uid, vr_type::UI, "1.2.3.4.5.6.7.8");
    workitem.set_string(ups_tags::procedure_step_state, vr_type::CS, "SCHEDULED");
    workitem.set_string(ups_tags::procedure_step_label, vr_type::LO, "CT Chest Review");
    workitem.set_string(ups_tags::scheduled_procedure_step_priority, vr_type::CS, "MEDIUM");
    workitem.set_string(ups_tags::worklist_label, vr_type::LO, "Radiology");

    SECTION("workitem attributes are set correctly") {
        CHECK(workitem.get_string(tags::sop_instance_uid) == "1.2.3.4.5.6.7.8");
        CHECK(workitem.get_string(ups_tags::procedure_step_state) == "SCHEDULED");
        CHECK(workitem.get_string(ups_tags::procedure_step_label) == "CT Chest Review");
        CHECK(workitem.get_string(ups_tags::scheduled_procedure_step_priority) == "MEDIUM");
        CHECK(workitem.get_string(ups_tags::worklist_label) == "Radiology");
    }
}
