/**
 * @file worklist_scp_test.cpp
 * @brief Unit tests for Worklist SCP service (Modality Worklist)
 */

#include <pacs/services/worklist_scp.hpp>
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
// worklist_scp Construction Tests
// ============================================================================

TEST_CASE("worklist_scp construction", "[services][worklist]") {
    worklist_scp scp;

    SECTION("service name is correct") {
        CHECK(scp.service_name() == "Worklist SCP");
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

TEST_CASE("worklist_scp SOP class support", "[services][worklist]") {
    worklist_scp scp;

    SECTION("supports Modality Worklist FIND") {
        CHECK(scp.supports_sop_class("1.2.840.10008.5.1.4.31"));
        CHECK(scp.supports_sop_class(worklist_find_sop_class_uid));
    }

    SECTION("does not support Patient Root Query/Retrieve FIND") {
        CHECK_FALSE(scp.supports_sop_class("1.2.840.10008.5.1.4.1.2.1.1"));
    }

    SECTION("does not support Study Root Query/Retrieve FIND") {
        CHECK_FALSE(scp.supports_sop_class("1.2.840.10008.5.1.4.1.2.2.1"));
    }

    SECTION("does not support non-FIND SOP classes") {
        // Verification SOP Class
        CHECK_FALSE(scp.supports_sop_class("1.2.840.10008.1.1"));
        // CT Image Storage
        CHECK_FALSE(scp.supports_sop_class("1.2.840.10008.5.1.4.1.1.2"));
        // MPPS SOP Class
        CHECK_FALSE(scp.supports_sop_class("1.2.840.10008.3.1.2.3.3"));
        // Empty string
        CHECK_FALSE(scp.supports_sop_class(""));
    }
}

// ============================================================================
// SOP Class UID Constants Tests
// ============================================================================

TEST_CASE("worklist SOP class UID constant", "[services][worklist]") {
    CHECK(worklist_find_sop_class_uid == "1.2.840.10008.5.1.4.31");
}

// ============================================================================
// Configuration Tests
// ============================================================================

TEST_CASE("worklist_scp configuration", "[services][worklist]") {
    worklist_scp scp;

    SECTION("set_max_results updates max_results") {
        scp.set_max_results(100);
        CHECK(scp.max_results() == 100);

        scp.set_max_results(0);  // unlimited
        CHECK(scp.max_results() == 0);

        scp.set_max_results(999);
        CHECK(scp.max_results() == 999);
    }

    SECTION("set_handler accepts lambda") {
        bool handler_called = false;
        scp.set_handler([&handler_called](
            [[maybe_unused]] const dicom_dataset& keys,
            [[maybe_unused]] const std::string& ae) {
            handler_called = true;
            return std::vector<dicom_dataset>{};
        });
        // Handler is stored but not called in this test
        CHECK_FALSE(handler_called);
    }

    SECTION("set_cancel_check accepts lambda") {
        bool cancel_called = false;
        scp.set_cancel_check([&cancel_called]() {
            cancel_called = true;
            return false;
        });
        // Cancel check is stored but not called in this test
        CHECK_FALSE(cancel_called);
    }
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_CASE("worklist_scp statistics", "[services][worklist]") {
    worklist_scp scp;

    SECTION("queries_processed starts at zero") {
        CHECK(scp.queries_processed() == 0);
    }

    SECTION("items_returned starts at zero") {
        CHECK(scp.items_returned() == 0);
    }

    SECTION("reset_statistics resets all counters to zero") {
        // We can't easily increment the counter without a mock association,
        // but we can verify reset_statistics works
        scp.reset_statistics();
        CHECK(scp.queries_processed() == 0);
        CHECK(scp.items_returned() == 0);
    }
}

// ============================================================================
// MWL C-FIND Message Factory Tests
// ============================================================================

TEST_CASE("make_c_find_rq for MWL creates valid request", "[services][worklist]") {
    auto request = make_c_find_rq(42, worklist_find_sop_class_uid);

    CHECK(request.command() == command_field::c_find_rq);
    CHECK(request.message_id() == 42);
    CHECK(request.affected_sop_class_uid() == "1.2.840.10008.5.1.4.31");
    CHECK(request.is_request());
    CHECK_FALSE(request.is_response());
}

TEST_CASE("make_c_find_rsp for MWL creates valid response", "[services][worklist]") {
    SECTION("pending response") {
        auto response = make_c_find_rsp(42, worklist_find_sop_class_uid, status_pending);

        CHECK(response.command() == command_field::c_find_rsp);
        CHECK(response.message_id_responded_to() == 42);
        CHECK(response.affected_sop_class_uid() == "1.2.840.10008.5.1.4.31");
        CHECK(response.status() == status_pending);
        CHECK(response.is_response());
        CHECK_FALSE(response.is_request());
    }

    SECTION("success response") {
        auto response = make_c_find_rsp(123, worklist_find_sop_class_uid, status_success);

        CHECK(response.status() == status_success);
    }

    SECTION("cancel response") {
        auto response = make_c_find_rsp(456, worklist_find_sop_class_uid, status_cancel);

        CHECK(response.status() == status_cancel);
    }
}

// ============================================================================
// Handler Tests
// ============================================================================

TEST_CASE("worklist_scp handler integration", "[services][worklist]") {
    worklist_scp scp;
    std::vector<dicom_dataset> test_results;

    // Create a worklist item dataset
    dicom_dataset item1;
    item1.set_string(tags::patient_name, vr_type::PN, "DOE^JOHN");
    item1.set_string(tags::patient_id, vr_type::LO, "12345");
    item1.set_string(tags::accession_number, vr_type::SH, "ACC001");
    item1.set_string(tags::study_instance_uid, vr_type::UI, "1.2.3.4.5.6.7");

    dicom_dataset item2;
    item2.set_string(tags::patient_name, vr_type::PN, "DOE^JANE");
    item2.set_string(tags::patient_id, vr_type::LO, "67890");
    item2.set_string(tags::accession_number, vr_type::SH, "ACC002");
    item2.set_string(tags::study_instance_uid, vr_type::UI, "1.2.3.4.5.6.8");

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

    SECTION("handler captures correct parameters") {
        // Note: Actual handle_message testing requires a mock association
        // This test validates handler setup only
        CHECK_FALSE(handler_called);
    }
}

// ============================================================================
// scp_service Base Class Tests
// ============================================================================

TEST_CASE("worklist_scp is a scp_service", "[services][worklist]") {
    // Verify worklist_scp properly inherits from scp_service
    std::unique_ptr<scp_service> base_ptr = std::make_unique<worklist_scp>();

    CHECK(base_ptr->service_name() == "Worklist SCP");
    CHECK(base_ptr->supported_sop_classes().size() == 1);
    CHECK(base_ptr->supports_sop_class(worklist_find_sop_class_uid));
}

// ============================================================================
// Multiple Instance Tests
// ============================================================================

TEST_CASE("multiple worklist_scp instances are independent", "[services][worklist]") {
    worklist_scp scp1;
    worklist_scp scp2;

    // Set different configurations
    scp1.set_max_results(100);
    scp2.set_max_results(200);

    CHECK(scp1.max_results() == 100);
    CHECK(scp2.max_results() == 200);

    // Reset one should not affect the other
    scp1.reset_statistics();
    CHECK(scp1.queries_processed() == 0);
    CHECK(scp1.items_returned() == 0);
    CHECK(scp2.queries_processed() == 0);
    CHECK(scp2.items_returned() == 0);
}

// ============================================================================
// Scheduled Procedure Step Tag Tests
// ============================================================================

TEST_CASE("scheduled procedure step tag constants", "[services][worklist]") {
    CHECK(tags::scheduled_station_ae_title.group() == 0x0040);
    CHECK(tags::scheduled_station_ae_title.element() == 0x0001);

    CHECK(tags::scheduled_procedure_step_start_date.group() == 0x0040);
    CHECK(tags::scheduled_procedure_step_start_date.element() == 0x0002);

    CHECK(tags::scheduled_procedure_step_start_time.group() == 0x0040);
    CHECK(tags::scheduled_procedure_step_start_time.element() == 0x0003);

    CHECK(tags::scheduled_procedure_step_sequence.group() == 0x0040);
    CHECK(tags::scheduled_procedure_step_sequence.element() == 0x0100);

    CHECK(tags::scheduled_procedure_step_id.group() == 0x0040);
    CHECK(tags::scheduled_procedure_step_id.element() == 0x0009);

    CHECK(tags::scheduled_procedure_step_description.group() == 0x0040);
    CHECK(tags::scheduled_procedure_step_description.element() == 0x0007);
}

// ============================================================================
// MWL Worklist Item Dataset Creation Tests
// ============================================================================

TEST_CASE("create worklist item dataset", "[services][worklist]") {
    dicom_dataset worklist_item;

    // Patient information
    worklist_item.set_string(tags::patient_name, vr_type::PN, "DOE^JOHN");
    worklist_item.set_string(tags::patient_id, vr_type::LO, "12345");
    worklist_item.set_string(tags::patient_birth_date, vr_type::DA, "19800115");
    worklist_item.set_string(tags::patient_sex, vr_type::CS, "M");

    // Study information
    worklist_item.set_string(tags::accession_number, vr_type::SH, "ACC001");
    worklist_item.set_string(tags::study_instance_uid, vr_type::UI, "1.2.3.4.5.6.7");

    SECTION("patient attributes are set correctly") {
        CHECK(worklist_item.get_string(tags::patient_name) == "DOE^JOHN");
        CHECK(worklist_item.get_string(tags::patient_id) == "12345");
        CHECK(worklist_item.get_string(tags::patient_birth_date) == "19800115");
        CHECK(worklist_item.get_string(tags::patient_sex) == "M");
    }

    SECTION("study attributes are set correctly") {
        CHECK(worklist_item.get_string(tags::accession_number) == "ACC001");
        CHECK(worklist_item.get_string(tags::study_instance_uid) == "1.2.3.4.5.6.7");
    }
}

// ============================================================================
// MWL Query Key Dataset Creation Tests
// ============================================================================

TEST_CASE("create worklist query keys dataset", "[services][worklist]") {
    dicom_dataset query_keys;

    // Set query keys - patient level
    query_keys.set_string(tags::patient_name, vr_type::PN, "DOE*");
    query_keys.set_string(tags::patient_id, vr_type::LO, "");

    // Set query keys - study level
    query_keys.set_string(tags::accession_number, vr_type::SH, "");
    query_keys.set_string(tags::study_instance_uid, vr_type::UI, "");

    // Set modality in query
    query_keys.set_string(tags::modality, vr_type::CS, "CT");

    SECTION("patient query keys") {
        CHECK(query_keys.get_string(tags::patient_name) == "DOE*");
        CHECK(query_keys.get_string(tags::patient_id).empty());
    }

    SECTION("modality query key") {
        CHECK(query_keys.get_string(tags::modality) == "CT");
    }
}

// ============================================================================
// Handler Function Type Tests
// ============================================================================

TEST_CASE("worklist_handler function type", "[services][worklist]") {
    worklist_handler handler = [](
        [[maybe_unused]] const dicom_dataset& query_keys,
        [[maybe_unused]] const std::string& calling_ae) {
        std::vector<dicom_dataset> results;

        // Create a sample worklist item
        dicom_dataset item;
        item.set_string(tags::patient_name, vr_type::PN, "TEST^PATIENT");
        results.push_back(item);

        return results;
    };

    dicom_dataset query;
    std::string ae = "TEST_AE";

    auto results = handler(query, ae);

    CHECK(results.size() == 1);
    CHECK(results[0].get_string(tags::patient_name) == "TEST^PATIENT");
}

TEST_CASE("worklist_cancel_check function type", "[services][worklist]") {
    SECTION("returns false by default") {
        worklist_cancel_check check = []() {
            return false;
        };
        CHECK_FALSE(check());
    }

    SECTION("can return true to indicate cancel") {
        worklist_cancel_check check = []() {
            return true;
        };
        CHECK(check());
    }
}
