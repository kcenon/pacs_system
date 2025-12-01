/**
 * @file query_scp_test.cpp
 * @brief Unit tests for Query SCP service
 */

#include <pacs/services/query_scp.hpp>
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
// query_level Enum Tests
// ============================================================================

TEST_CASE("query_level enum", "[services][query]") {
    SECTION("to_string returns correct DICOM strings") {
        CHECK(to_string(query_level::patient) == "PATIENT");
        CHECK(to_string(query_level::study) == "STUDY");
        CHECK(to_string(query_level::series) == "SERIES");
        CHECK(to_string(query_level::image) == "IMAGE");
    }

    SECTION("parse_query_level parses valid strings") {
        auto patient = parse_query_level("PATIENT");
        REQUIRE(patient.has_value());
        CHECK(patient.value() == query_level::patient);

        auto study = parse_query_level("STUDY");
        REQUIRE(study.has_value());
        CHECK(study.value() == query_level::study);

        auto series = parse_query_level("SERIES");
        REQUIRE(series.has_value());
        CHECK(series.value() == query_level::series);

        auto image = parse_query_level("IMAGE");
        REQUIRE(image.has_value());
        CHECK(image.value() == query_level::image);
    }

    SECTION("parse_query_level returns nullopt for invalid strings") {
        CHECK_FALSE(parse_query_level("INVALID").has_value());
        CHECK_FALSE(parse_query_level("patient").has_value());  // lowercase
        CHECK_FALSE(parse_query_level("").has_value());
        CHECK_FALSE(parse_query_level("INSTANCE").has_value());  // IMAGE, not INSTANCE
    }
}

// ============================================================================
// query_scp Construction Tests
// ============================================================================

TEST_CASE("query_scp construction", "[services][query]") {
    query_scp scp;

    SECTION("service name is correct") {
        CHECK(scp.service_name() == "Query SCP");
    }

    SECTION("supports two SOP classes") {
        auto classes = scp.supported_sop_classes();
        CHECK(classes.size() == 2);
    }

    SECTION("default max_results is unlimited (0)") {
        CHECK(scp.max_results() == 0);
    }

    SECTION("initial queries_processed is zero") {
        CHECK(scp.queries_processed() == 0);
    }
}

// ============================================================================
// SOP Class Support Tests
// ============================================================================

TEST_CASE("query_scp SOP class support", "[services][query]") {
    query_scp scp;

    SECTION("supports Patient Root Query/Retrieve FIND") {
        CHECK(scp.supports_sop_class("1.2.840.10008.5.1.4.1.2.1.1"));
        CHECK(scp.supports_sop_class(patient_root_find_sop_class_uid));
    }

    SECTION("supports Study Root Query/Retrieve FIND") {
        CHECK(scp.supports_sop_class("1.2.840.10008.5.1.4.1.2.2.1"));
        CHECK(scp.supports_sop_class(study_root_find_sop_class_uid));
    }

    SECTION("does not support non-FIND SOP classes") {
        // Verification SOP Class
        CHECK_FALSE(scp.supports_sop_class("1.2.840.10008.1.1"));
        // CT Image Storage
        CHECK_FALSE(scp.supports_sop_class("1.2.840.10008.5.1.4.1.1.2"));
        // Patient Root MOVE
        CHECK_FALSE(scp.supports_sop_class("1.2.840.10008.5.1.4.1.2.1.2"));
        // Study Root GET
        CHECK_FALSE(scp.supports_sop_class("1.2.840.10008.5.1.4.1.2.2.3"));
        // Empty string
        CHECK_FALSE(scp.supports_sop_class(""));
    }
}

// ============================================================================
// SOP Class UID Constants Tests
// ============================================================================

TEST_CASE("query SOP class UID constants", "[services][query]") {
    CHECK(patient_root_find_sop_class_uid == "1.2.840.10008.5.1.4.1.2.1.1");
    CHECK(study_root_find_sop_class_uid == "1.2.840.10008.5.1.4.1.2.2.1");
    CHECK(patient_study_only_find_sop_class_uid == "1.2.840.10008.5.1.4.1.2.3.1");
    CHECK(modality_worklist_find_sop_class_uid == "1.2.840.10008.5.1.4.31");
}

// ============================================================================
// Configuration Tests
// ============================================================================

TEST_CASE("query_scp configuration", "[services][query]") {
    query_scp scp;

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
            [[maybe_unused]] query_level level,
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

TEST_CASE("query_scp statistics", "[services][query]") {
    query_scp scp;

    SECTION("queries_processed starts at zero") {
        CHECK(scp.queries_processed() == 0);
    }

    SECTION("reset_statistics resets counter to zero") {
        // We can't easily increment the counter without a mock association,
        // but we can verify reset_statistics works
        scp.reset_statistics();
        CHECK(scp.queries_processed() == 0);
    }
}

// ============================================================================
// C-FIND Message Factory Tests
// ============================================================================

TEST_CASE("make_c_find_rq creates valid request", "[services][query]") {
    auto request = make_c_find_rq(42, patient_root_find_sop_class_uid);

    CHECK(request.command() == command_field::c_find_rq);
    CHECK(request.message_id() == 42);
    CHECK(request.affected_sop_class_uid() == "1.2.840.10008.5.1.4.1.2.1.1");
    CHECK(request.is_request());
    CHECK_FALSE(request.is_response());
}

TEST_CASE("make_c_find_rsp creates valid response", "[services][query]") {
    SECTION("pending response") {
        auto response = make_c_find_rsp(42, study_root_find_sop_class_uid, status_pending);

        CHECK(response.command() == command_field::c_find_rsp);
        CHECK(response.message_id_responded_to() == 42);
        CHECK(response.affected_sop_class_uid() == "1.2.840.10008.5.1.4.1.2.2.1");
        CHECK(response.status() == status_pending);
        CHECK(response.is_response());
        CHECK_FALSE(response.is_request());
    }

    SECTION("success response") {
        auto response = make_c_find_rsp(123, patient_root_find_sop_class_uid, status_success);

        CHECK(response.status() == status_success);
    }

    SECTION("cancel response") {
        auto response = make_c_find_rsp(456, study_root_find_sop_class_uid, status_cancel);

        CHECK(response.status() == status_cancel);
    }
}

// ============================================================================
// Handler Tests
// ============================================================================

TEST_CASE("query_scp handler integration", "[services][query]") {
    query_scp scp;
    std::vector<dicom_dataset> test_results;

    // Create some test datasets
    dicom_dataset ds1;
    ds1.set_string(tags::patient_name, vr_type::PN, "DOE^JOHN");
    ds1.set_string(tags::patient_id, vr_type::LO, "12345");

    dicom_dataset ds2;
    ds2.set_string(tags::patient_name, vr_type::PN, "DOE^JANE");
    ds2.set_string(tags::patient_id, vr_type::LO, "67890");

    test_results.push_back(ds1);
    test_results.push_back(ds2);

    query_level captured_level{};
    std::string captured_ae;
    bool handler_called = false;

    scp.set_handler([&](query_level level,
                        [[maybe_unused]] const dicom_dataset& keys,
                        const std::string& ae) {
        handler_called = true;
        captured_level = level;
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

TEST_CASE("query_scp is a scp_service", "[services][query]") {
    // Verify query_scp properly inherits from scp_service
    std::unique_ptr<scp_service> base_ptr = std::make_unique<query_scp>();

    CHECK(base_ptr->service_name() == "Query SCP");
    CHECK(base_ptr->supported_sop_classes().size() == 2);
    CHECK(base_ptr->supports_sop_class(patient_root_find_sop_class_uid));
    CHECK(base_ptr->supports_sop_class(study_root_find_sop_class_uid));
}

// ============================================================================
// Multiple Instance Tests
// ============================================================================

TEST_CASE("multiple query_scp instances are independent", "[services][query]") {
    query_scp scp1;
    query_scp scp2;

    // Set different configurations
    scp1.set_max_results(100);
    scp2.set_max_results(200);

    CHECK(scp1.max_results() == 100);
    CHECK(scp2.max_results() == 200);

    // Reset one should not affect the other
    scp1.reset_statistics();
    CHECK(scp1.queries_processed() == 0);
    CHECK(scp2.queries_processed() == 0);
}

// ============================================================================
// Query Level Tag Tests
// ============================================================================

TEST_CASE("query_retrieve_level tag constant", "[services][query]") {
    CHECK(tags::query_retrieve_level.group() == 0x0008);
    CHECK(tags::query_retrieve_level.element() == 0x0052);
}
