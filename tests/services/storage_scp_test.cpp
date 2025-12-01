/**
 * @file storage_scp_test.cpp
 * @brief Unit tests for Storage SCP service
 */

#include <pacs/services/storage_scp.hpp>
#include <pacs/services/storage_status.hpp>
#include <pacs/network/dimse/command_field.hpp>
#include <pacs/network/dimse/dimse_message.hpp>
#include <pacs/network/dimse/status_codes.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace pacs::services;
using namespace pacs::network;
using namespace pacs::network::dimse;
using namespace pacs::core;

// ============================================================================
// storage_status Tests
// ============================================================================

TEST_CASE("storage_status enum values", "[services][storage]") {
    SECTION("success status") {
        CHECK(static_cast<uint16_t>(storage_status::success) == 0x0000);
        CHECK(is_success(storage_status::success));
        CHECK_FALSE(is_warning(storage_status::success));
        CHECK_FALSE(is_failure(storage_status::success));
    }

    SECTION("warning statuses") {
        CHECK(static_cast<uint16_t>(storage_status::coercion_of_data_elements) == 0xB000);
        CHECK(static_cast<uint16_t>(storage_status::elements_discarded) == 0xB006);
        CHECK(static_cast<uint16_t>(storage_status::data_set_does_not_match_sop_class_warning) == 0xB007);

        CHECK(is_warning(storage_status::coercion_of_data_elements));
        CHECK(is_warning(storage_status::elements_discarded));
        CHECK(is_warning(storage_status::data_set_does_not_match_sop_class_warning));

        CHECK_FALSE(is_success(storage_status::coercion_of_data_elements));
        CHECK_FALSE(is_failure(storage_status::coercion_of_data_elements));
    }

    SECTION("failure statuses") {
        CHECK(static_cast<uint16_t>(storage_status::duplicate_sop_instance) == 0x0111);
        CHECK(static_cast<uint16_t>(storage_status::out_of_resources) == 0xA700);
        CHECK(static_cast<uint16_t>(storage_status::data_set_does_not_match_sop_class) == 0xA900);
        CHECK(static_cast<uint16_t>(storage_status::cannot_understand) == 0xC000);
        CHECK(static_cast<uint16_t>(storage_status::storage_error) == 0xC001);

        CHECK(is_failure(storage_status::duplicate_sop_instance));
        CHECK(is_failure(storage_status::out_of_resources));
        CHECK(is_failure(storage_status::data_set_does_not_match_sop_class));
        CHECK(is_failure(storage_status::cannot_understand));
        CHECK(is_failure(storage_status::storage_error));

        CHECK_FALSE(is_success(storage_status::storage_error));
        CHECK_FALSE(is_warning(storage_status::storage_error));
    }
}

TEST_CASE("storage_status to_string", "[services][storage]") {
    CHECK(to_string(storage_status::success) == "Success");
    CHECK(to_string(storage_status::coercion_of_data_elements) == "Warning: Coercion of data elements");
    CHECK(to_string(storage_status::duplicate_sop_instance) == "Failure: Duplicate SOP instance");
    CHECK(to_string(storage_status::storage_error) == "Failure: Storage error");
}

TEST_CASE("storage_status to_status_code conversion", "[services][storage]") {
    CHECK(to_status_code(storage_status::success) == 0x0000);
    CHECK(to_status_code(storage_status::duplicate_sop_instance) == 0x0111);
    CHECK(to_status_code(storage_status::out_of_resources) == 0xA700);
    CHECK(to_status_code(storage_status::storage_error) == 0xC001);
}

// ============================================================================
// storage_scp Construction Tests
// ============================================================================

TEST_CASE("storage_scp default construction", "[services][storage]") {
    storage_scp scp;

    SECTION("service name is correct") {
        CHECK(scp.service_name() == "Storage SCP");
    }

    SECTION("supports standard storage SOP classes") {
        auto classes = scp.supported_sop_classes();
        CHECK(classes.size() > 0);
    }

    SECTION("initial statistics are zero") {
        CHECK(scp.images_received() == 0);
        CHECK(scp.bytes_received() == 0);
    }
}

TEST_CASE("storage_scp construction with config", "[services][storage]") {
    storage_scp_config config;
    config.accepted_sop_classes = {"1.2.840.10008.5.1.4.1.1.2"};  // CT only
    config.duplicate_policy = duplicate_policy::reject;

    storage_scp scp{config};

    SECTION("uses configured SOP classes") {
        auto classes = scp.supported_sop_classes();
        REQUIRE(classes.size() == 1);
        CHECK(classes[0] == "1.2.840.10008.5.1.4.1.1.2");
    }
}

// ============================================================================
// SOP Class Support Tests
// ============================================================================

TEST_CASE("storage_scp SOP class support", "[services][storage]") {
    storage_scp scp;

    SECTION("supports CT Image Storage") {
        CHECK(scp.supports_sop_class(ct_image_storage_uid));
        CHECK(scp.supports_sop_class("1.2.840.10008.5.1.4.1.1.2"));
    }

    SECTION("supports MR Image Storage") {
        CHECK(scp.supports_sop_class(mr_image_storage_uid));
    }

    SECTION("supports US Image Storage") {
        CHECK(scp.supports_sop_class(us_image_storage_uid));
    }

    SECTION("supports Secondary Capture") {
        CHECK(scp.supports_sop_class(secondary_capture_image_storage_uid));
    }

    SECTION("does not support Verification SOP Class") {
        CHECK_FALSE(scp.supports_sop_class("1.2.840.10008.1.1"));
    }

    SECTION("does not support empty string") {
        CHECK_FALSE(scp.supports_sop_class(""));
    }

    SECTION("does not support random UID") {
        CHECK_FALSE(scp.supports_sop_class("1.2.3.4.5.6.7.8.9"));
    }
}

TEST_CASE("storage_scp configured SOP classes", "[services][storage]") {
    storage_scp_config config;
    config.accepted_sop_classes = {
        "1.2.840.10008.5.1.4.1.1.2",   // CT
        "1.2.840.10008.5.1.4.1.1.4"    // MR
    };

    storage_scp scp{config};

    SECTION("supports only configured classes") {
        CHECK(scp.supports_sop_class("1.2.840.10008.5.1.4.1.1.2"));
        CHECK(scp.supports_sop_class("1.2.840.10008.5.1.4.1.1.4"));
        CHECK_FALSE(scp.supports_sop_class("1.2.840.10008.5.1.4.1.1.6.1"));  // US
    }
}

// ============================================================================
// Handler Registration Tests
// ============================================================================

TEST_CASE("storage_scp handler registration", "[services][storage]") {
    storage_scp scp;

    SECTION("can set storage handler") {
        bool handler_called = false;
        scp.set_handler([&](const pacs::core::dicom_dataset&,
                           const std::string&,
                           const std::string&,
                           const std::string&) {
            handler_called = true;
            return storage_status::success;
        });
        // Handler is set but not called yet
        CHECK_FALSE(handler_called);
    }

    SECTION("can set pre-store handler") {
        bool handler_called = false;
        scp.set_pre_store_handler([&](const pacs::core::dicom_dataset&) {
            handler_called = true;
            return true;
        });
        CHECK_FALSE(handler_called);
    }
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_CASE("storage_scp statistics", "[services][storage]") {
    storage_scp scp;

    SECTION("initial values are zero") {
        CHECK(scp.images_received() == 0);
        CHECK(scp.bytes_received() == 0);
    }

    SECTION("reset clears statistics") {
        // Note: In a real test, we would trigger handle_message to increment
        // For now, just verify reset works
        scp.reset_statistics();
        CHECK(scp.images_received() == 0);
        CHECK(scp.bytes_received() == 0);
    }
}

// ============================================================================
// Storage SOP Class UID Constants Tests
// ============================================================================

TEST_CASE("Storage SOP Class UID constants", "[services][storage]") {
    CHECK(ct_image_storage_uid == "1.2.840.10008.5.1.4.1.1.2");
    CHECK(enhanced_ct_image_storage_uid == "1.2.840.10008.5.1.4.1.1.2.1");
    CHECK(mr_image_storage_uid == "1.2.840.10008.5.1.4.1.1.4");
    CHECK(enhanced_mr_image_storage_uid == "1.2.840.10008.5.1.4.1.1.4.1");
    CHECK(cr_image_storage_uid == "1.2.840.10008.5.1.4.1.1.1");
    CHECK(us_image_storage_uid == "1.2.840.10008.5.1.4.1.1.6.1");
    CHECK(secondary_capture_image_storage_uid == "1.2.840.10008.5.1.4.1.1.7");
}

// ============================================================================
// get_standard_storage_sop_classes Tests
// ============================================================================

TEST_CASE("get_standard_storage_sop_classes", "[services][storage]") {
    auto classes = get_standard_storage_sop_classes();

    SECTION("returns non-empty list") {
        CHECK(classes.size() > 0);
    }

    SECTION("includes common modality types") {
        // Use a helper to check if a UID is in the list
        auto contains = [&classes](const std::string& uid) {
            return std::find(classes.begin(), classes.end(), uid) != classes.end();
        };

        CHECK(contains(std::string(ct_image_storage_uid)));
        CHECK(contains(std::string(mr_image_storage_uid)));
        CHECK(contains(std::string(us_image_storage_uid)));
        CHECK(contains(std::string(secondary_capture_image_storage_uid)));
    }

    SECTION("does not include non-storage SOP classes") {
        auto contains = [&classes](const std::string& uid) {
            return std::find(classes.begin(), classes.end(), uid) != classes.end();
        };

        // Verification SOP Class should not be included
        CHECK_FALSE(contains("1.2.840.10008.1.1"));
        // Query/Retrieve SOP Classes should not be included
        CHECK_FALSE(contains("1.2.840.10008.5.1.4.1.2.1.1"));
    }
}

// ============================================================================
// scp_service Base Class Tests
// ============================================================================

TEST_CASE("storage_scp is a scp_service", "[services][storage]") {
    std::unique_ptr<scp_service> base_ptr = std::make_unique<storage_scp>();

    CHECK(base_ptr->service_name() == "Storage SCP");
    CHECK(base_ptr->supported_sop_classes().size() > 0);
    CHECK(base_ptr->supports_sop_class("1.2.840.10008.5.1.4.1.1.2"));
}

// ============================================================================
// Multiple Instance Tests
// ============================================================================

TEST_CASE("multiple storage_scp instances are independent", "[services][storage]") {
    storage_scp scp1;
    storage_scp scp2;

    bool handler1_called = false;
    bool handler2_called = false;

    scp1.set_handler([&](auto&&...) {
        handler1_called = true;
        return storage_status::success;
    });

    scp2.set_handler([&](auto&&...) {
        handler2_called = true;
        return storage_status::storage_error;
    });

    // Handlers are independent
    CHECK_FALSE(handler1_called);
    CHECK_FALSE(handler2_called);

    // Statistics are independent
    CHECK(scp1.images_received() == 0);
    CHECK(scp2.images_received() == 0);
}

// ============================================================================
// C-STORE Message Factory Tests
// ============================================================================

TEST_CASE("make_c_store_rq creates valid request", "[services][storage]") {
    auto request = make_c_store_rq(
        42,
        "1.2.840.10008.5.1.4.1.1.2",
        "1.2.3.4.5.6.7.8.9.10"
    );

    CHECK(request.command() == command_field::c_store_rq);
    CHECK(request.message_id() == 42);
    CHECK(request.affected_sop_class_uid() == "1.2.840.10008.5.1.4.1.1.2");
    CHECK(request.affected_sop_instance_uid() == "1.2.3.4.5.6.7.8.9.10");
    CHECK(request.is_request());
    CHECK_FALSE(request.is_response());
}

TEST_CASE("make_c_store_rsp creates valid response", "[services][storage]") {
    auto response = make_c_store_rsp(
        42,
        "1.2.840.10008.5.1.4.1.1.2",
        "1.2.3.4.5.6.7.8.9.10",
        status_success
    );

    CHECK(response.command() == command_field::c_store_rsp);
    CHECK(response.message_id_responded_to() == 42);
    CHECK(response.affected_sop_class_uid() == "1.2.840.10008.5.1.4.1.1.2");
    CHECK(response.affected_sop_instance_uid() == "1.2.3.4.5.6.7.8.9.10");
    CHECK(response.status() == status_success);
    CHECK(response.is_response());
    CHECK_FALSE(response.is_request());
}

TEST_CASE("make_c_store_rsp with error status", "[services][storage]") {
    auto response = make_c_store_rsp(
        42,
        "1.2.840.10008.5.1.4.1.1.2",
        "1.2.3.4.5.6.7.8.9.10",
        static_cast<status_code>(storage_status::storage_error)
    );

    CHECK(response.status() == 0xC001);
}

// ============================================================================
// duplicate_policy Tests
// ============================================================================

TEST_CASE("duplicate_policy enum", "[services][storage]") {
    // Just verify enum values exist and can be used
    storage_scp_config config;

    config.duplicate_policy = duplicate_policy::reject;
    CHECK(config.duplicate_policy == duplicate_policy::reject);

    config.duplicate_policy = duplicate_policy::replace;
    CHECK(config.duplicate_policy == duplicate_policy::replace);

    config.duplicate_policy = duplicate_policy::ignore;
    CHECK(config.duplicate_policy == duplicate_policy::ignore);
}
