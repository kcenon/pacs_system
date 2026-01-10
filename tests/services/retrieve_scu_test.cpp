/**
 * @file retrieve_scu_test.cpp
 * @brief Unit tests for Retrieve SCU service (C-MOVE/C-GET)
 *
 * @see Issue #532 - Implement retrieve_scu Library (C-MOVE/C-GET SCU)
 */

#include <pacs/services/retrieve_scu.hpp>
#include <pacs/services/retrieve_scp.hpp>
#include <pacs/network/dimse/command_field.hpp>
#include <pacs/network/dimse/dimse_message.hpp>
#include <pacs/network/dimse/status_codes.hpp>
#include <pacs/core/dicom_dataset.hpp>
#include <pacs/core/dicom_tag_constants.hpp>
#include <pacs/encoding/vr_type.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace pacs::services;
using namespace pacs::network;
using namespace pacs::network::dimse;
using namespace pacs::core;

// =============================================================================
// retrieve_mode Tests
// =============================================================================

TEST_CASE("retrieve_mode enum", "[services][retrieve_scu]") {
    SECTION("to_string conversion") {
        CHECK(to_string(retrieve_mode::c_move) == "C-MOVE");
        CHECK(to_string(retrieve_mode::c_get) == "C-GET");
    }

    SECTION("enum values") {
        CHECK(static_cast<int>(retrieve_mode::c_move) == 0);
        CHECK(static_cast<int>(retrieve_mode::c_get) == 1);
    }
}

// =============================================================================
// retrieve_progress Tests
// =============================================================================

TEST_CASE("retrieve_progress structure", "[services][retrieve_scu]") {
    SECTION("default values") {
        retrieve_progress progress;
        CHECK(progress.remaining == 0);
        CHECK(progress.completed == 0);
        CHECK(progress.failed == 0);
        CHECK(progress.warning == 0);
    }

    SECTION("total calculation") {
        retrieve_progress progress;
        progress.remaining = 10;
        progress.completed = 5;
        progress.failed = 2;
        progress.warning = 1;

        CHECK(progress.total() == 18);
    }

    SECTION("percent calculation - empty") {
        retrieve_progress progress;
        CHECK(progress.percent() == 0.0f);
    }

    SECTION("percent calculation - partial") {
        retrieve_progress progress;
        progress.remaining = 50;
        progress.completed = 40;
        progress.failed = 5;
        progress.warning = 5;

        // (40 + 5 + 5) / 100 * 100 = 50%
        CHECK(progress.percent() == 50.0f);
    }

    SECTION("percent calculation - complete") {
        retrieve_progress progress;
        progress.remaining = 0;
        progress.completed = 100;
        progress.failed = 0;
        progress.warning = 0;

        CHECK(progress.percent() == 100.0f);
    }

    SECTION("elapsed time") {
        retrieve_progress progress;
        progress.start_time = std::chrono::steady_clock::now();

        // Just check that elapsed returns a value >= 0
        auto elapsed = progress.elapsed();
        CHECK(elapsed.count() >= 0);
    }
}

// =============================================================================
// retrieve_result Tests
// =============================================================================

TEST_CASE("retrieve_result status checks", "[services][retrieve_scu]") {
    SECTION("success status") {
        retrieve_result result;
        result.completed = 10;
        result.failed = 0;
        result.final_status = 0x0000;

        CHECK(result.is_success());
        CHECK_FALSE(result.is_cancelled());
        CHECK_FALSE(result.has_failures());
        CHECK_FALSE(result.has_warnings());
    }

    SECTION("success with some failures") {
        retrieve_result result;
        result.completed = 8;
        result.failed = 2;
        result.final_status = 0x0000;

        CHECK_FALSE(result.is_success());  // has_failures prevents success
        CHECK(result.has_failures());
    }

    SECTION("cancelled status") {
        retrieve_result result;
        result.final_status = 0xFE00;

        CHECK(result.is_cancelled());
        CHECK_FALSE(result.is_success());
    }

    SECTION("warning status") {
        retrieve_result result;
        result.warning = 3;
        result.final_status = 0x0000;

        CHECK(result.has_warnings());
    }

    SECTION("received instances for C-GET") {
        retrieve_result result;
        result.completed = 2;
        result.final_status = 0x0000;

        dicom_dataset ds1, ds2;
        result.received_instances.push_back(ds1);
        result.received_instances.push_back(ds2);

        CHECK(result.received_instances.size() == 2);
    }
}

// =============================================================================
// retrieve_scu_config Tests
// =============================================================================

TEST_CASE("retrieve_scu_config defaults", "[services][retrieve_scu]") {
    retrieve_scu_config config;

    CHECK(config.mode == retrieve_mode::c_move);
    CHECK(config.model == query_model::study_root);
    CHECK(config.level == query_level::study);
    CHECK(config.move_destination.empty());
    CHECK(config.timeout == std::chrono::milliseconds{120000});
    CHECK(config.priority == 0);
}

TEST_CASE("retrieve_scu_config customization", "[services][retrieve_scu]") {
    retrieve_scu_config config;
    config.mode = retrieve_mode::c_get;
    config.model = query_model::patient_root;
    config.level = query_level::series;
    config.move_destination = "WORKSTATION";
    config.timeout = std::chrono::milliseconds{60000};
    config.priority = priority_high;

    CHECK(config.mode == retrieve_mode::c_get);
    CHECK(config.model == query_model::patient_root);
    CHECK(config.level == query_level::series);
    CHECK(config.move_destination == "WORKSTATION");
    CHECK(config.timeout == std::chrono::milliseconds{60000});
    CHECK(config.priority == priority_high);
}

// =============================================================================
// retrieve_scu Construction Tests
// =============================================================================

TEST_CASE("retrieve_scu default construction", "[services][retrieve_scu]") {
    retrieve_scu scu;

    SECTION("initial statistics are zero") {
        CHECK(scu.retrieves_performed() == 0);
        CHECK(scu.instances_retrieved() == 0);
        CHECK(scu.bytes_retrieved() == 0);
    }

    SECTION("default config") {
        const auto& config = scu.config();
        CHECK(config.mode == retrieve_mode::c_move);
        CHECK(config.model == query_model::study_root);
    }
}

TEST_CASE("retrieve_scu construction with config", "[services][retrieve_scu]") {
    retrieve_scu_config config;
    config.mode = retrieve_mode::c_get;
    config.model = query_model::patient_root;
    config.move_destination = "ARCHIVE";

    retrieve_scu scu{config};

    SECTION("config is applied") {
        const auto& actual_config = scu.config();
        CHECK(actual_config.mode == retrieve_mode::c_get);
        CHECK(actual_config.model == query_model::patient_root);
        CHECK(actual_config.move_destination == "ARCHIVE");
    }

    SECTION("initial statistics are still zero") {
        CHECK(scu.retrieves_performed() == 0);
        CHECK(scu.instances_retrieved() == 0);
        CHECK(scu.bytes_retrieved() == 0);
    }
}

// =============================================================================
// retrieve_scu Configuration Methods Tests
// =============================================================================

TEST_CASE("retrieve_scu set_config", "[services][retrieve_scu]") {
    retrieve_scu scu;

    retrieve_scu_config new_config;
    new_config.mode = retrieve_mode::c_get;
    new_config.timeout = std::chrono::milliseconds{30000};

    scu.set_config(new_config);

    const auto& actual_config = scu.config();
    CHECK(actual_config.mode == retrieve_mode::c_get);
    CHECK(actual_config.timeout == std::chrono::milliseconds{30000});
}

TEST_CASE("retrieve_scu set_move_destination", "[services][retrieve_scu]") {
    retrieve_scu scu;

    scu.set_move_destination("MY_WORKSTATION");

    CHECK(scu.config().move_destination == "MY_WORKSTATION");
}

// =============================================================================
// retrieve_scu Statistics Tests
// =============================================================================

TEST_CASE("retrieve_scu statistics", "[services][retrieve_scu]") {
    retrieve_scu scu;

    SECTION("initial values are zero") {
        CHECK(scu.retrieves_performed() == 0);
        CHECK(scu.instances_retrieved() == 0);
        CHECK(scu.bytes_retrieved() == 0);
    }

    SECTION("reset clears statistics") {
        scu.reset_statistics();
        CHECK(scu.retrieves_performed() == 0);
        CHECK(scu.instances_retrieved() == 0);
        CHECK(scu.bytes_retrieved() == 0);
    }
}

// =============================================================================
// retrieve_scu Non-copyable/Non-movable Tests
// =============================================================================

TEST_CASE("retrieve_scu is non-copyable and non-movable", "[services][retrieve_scu]") {
    CHECK_FALSE(std::is_copy_constructible_v<retrieve_scu>);
    CHECK_FALSE(std::is_copy_assignable_v<retrieve_scu>);
    CHECK_FALSE(std::is_move_constructible_v<retrieve_scu>);
    CHECK_FALSE(std::is_move_assignable_v<retrieve_scu>);
}

// =============================================================================
// Multiple Instance Independence Tests
// =============================================================================

TEST_CASE("multiple retrieve_scu instances are independent", "[services][retrieve_scu]") {
    retrieve_scu scu1;
    retrieve_scu scu2;

    scu1.set_move_destination("DEST1");
    scu2.set_move_destination("DEST2");

    CHECK(scu1.config().move_destination == "DEST1");
    CHECK(scu2.config().move_destination == "DEST2");

    CHECK(scu1.retrieves_performed() == 0);
    CHECK(scu2.retrieves_performed() == 0);
}

// =============================================================================
// SOP Class UID Constants Tests
// =============================================================================

TEST_CASE("retrieve SOP Class UIDs are accessible", "[services][retrieve_scu]") {
    // Move SOP Class UIDs
    CHECK(std::string(patient_root_move_sop_class_uid) == "1.2.840.10008.5.1.4.1.2.1.2");
    CHECK(std::string(study_root_move_sop_class_uid) == "1.2.840.10008.5.1.4.1.2.2.2");

    // Get SOP Class UIDs
    CHECK(std::string(patient_root_get_sop_class_uid) == "1.2.840.10008.5.1.4.1.2.1.3");
    CHECK(std::string(study_root_get_sop_class_uid) == "1.2.840.10008.5.1.4.1.2.2.3");
}

// =============================================================================
// Progress Callback Tests
// =============================================================================

TEST_CASE("retrieve_progress_callback type", "[services][retrieve_scu]") {
    SECTION("lambda callback") {
        std::vector<float> progress_percentages;

        retrieve_progress_callback callback = [&](const retrieve_progress& p) {
            progress_percentages.push_back(p.percent());
        };

        // Simulate progress
        retrieve_progress p1;
        p1.remaining = 80;
        p1.completed = 20;
        callback(p1);

        retrieve_progress p2;
        p2.remaining = 0;
        p2.completed = 100;
        callback(p2);

        REQUIRE(progress_percentages.size() == 2);
        CHECK(progress_percentages[0] == 20.0f);
        CHECK(progress_percentages[1] == 100.0f);
    }

    SECTION("null callback is valid") {
        retrieve_progress_callback callback = nullptr;
        CHECK(callback == nullptr);
    }
}

// =============================================================================
// make_c_store_rsp Helper Tests
// =============================================================================

TEST_CASE("make_c_store_rsp helper function", "[services][retrieve_scu]") {
    SECTION("success response") {
        auto rsp = dimse::make_c_store_rsp(
            1,
            "1.2.840.10008.5.1.4.1.1.2",
            "1.2.3.4.5.6.7.8.9",
            status_success
        );

        CHECK(rsp.command() == command_field::c_store_rsp);
        CHECK(rsp.message_id_responded_to() == 1);  // RSP stores the responded-to message ID
        CHECK(rsp.affected_sop_class_uid() == "1.2.840.10008.5.1.4.1.1.2");
        CHECK(rsp.affected_sop_instance_uid() == "1.2.3.4.5.6.7.8.9");
        CHECK(rsp.status() == status_success);
    }

    SECTION("error response") {
        auto rsp = dimse::make_c_store_rsp(
            2,
            "1.2.840.10008.5.1.4.1.1.4",
            "9.8.7.6.5.4.3.2.1",
            static_cast<status_code>(0xA700)  // Out of resources
        );

        CHECK(rsp.command() == command_field::c_store_rsp);
        CHECK(rsp.message_id_responded_to() == 2);  // RSP stores the responded-to message ID
        CHECK(static_cast<uint16_t>(rsp.status()) == 0xA700);
    }
}

// =============================================================================
// Query Level String Conversion Tests
// =============================================================================

TEST_CASE("query_level string conversion for retrieve", "[services][retrieve_scu]") {
    CHECK(to_string(query_level::patient) == "PATIENT");
    CHECK(to_string(query_level::study) == "STUDY");
    CHECK(to_string(query_level::series) == "SERIES");
    CHECK(to_string(query_level::image) == "IMAGE");
}

// =============================================================================
// Query Model String Conversion Tests
// =============================================================================

TEST_CASE("query_model string conversion for retrieve", "[services][retrieve_scu]") {
    CHECK(to_string(query_model::patient_root) == "Patient Root");
    CHECK(to_string(query_model::study_root) == "Study Root");
}

// =============================================================================
// Association Not Established Error Tests
// =============================================================================

TEST_CASE("retrieve_scu operations require established association", "[services][retrieve_scu]") {
    retrieve_scu scu;
    scu.set_move_destination("DEST");
    association assoc;

    SECTION("move fails without association") {
        dicom_dataset query;
        query.set_string(tags::query_retrieve_level, pacs::encoding::vr_type::CS, "STUDY");
        query.set_string(tags::study_instance_uid, pacs::encoding::vr_type::UI, "1.2.3");

        auto result = scu.move(assoc, query, "DEST");
        CHECK(result.is_err());
    }

    SECTION("get fails without association") {
        dicom_dataset query;
        query.set_string(tags::query_retrieve_level, pacs::encoding::vr_type::CS, "STUDY");
        query.set_string(tags::study_instance_uid, pacs::encoding::vr_type::UI, "1.2.3");

        auto result = scu.get(assoc, query);
        CHECK(result.is_err());
    }

    SECTION("retrieve_study fails without association") {
        auto result = scu.retrieve_study(assoc, "1.2.3.4.5");
        CHECK(result.is_err());
    }

    SECTION("retrieve_series fails without association") {
        auto result = scu.retrieve_series(assoc, "1.2.3.4.5.6");
        CHECK(result.is_err());
    }

    SECTION("retrieve_instance fails without association") {
        auto result = scu.retrieve_instance(assoc, "1.2.3.4.5.6.7");
        CHECK(result.is_err());
    }
}

// =============================================================================
// Move Destination Required Tests
// =============================================================================

TEST_CASE("C-MOVE requires move destination", "[services][retrieve_scu]") {
    retrieve_scu scu;
    // Don't set move destination
    association assoc;

    SECTION("retrieve_study in C-MOVE mode without destination") {
        auto result = scu.retrieve_study(assoc, "1.2.3.4.5");
        CHECK(result.is_err());
    }

    SECTION("move with empty destination") {
        dicom_dataset query;
        query.set_string(tags::query_retrieve_level, pacs::encoding::vr_type::CS, "STUDY");

        auto result = scu.move(assoc, query, "");
        CHECK(result.is_err());
    }
}

// =============================================================================
// C-GET Mode Tests
// =============================================================================

TEST_CASE("C-GET mode does not require move destination", "[services][retrieve_scu]") {
    retrieve_scu_config config;
    config.mode = retrieve_mode::c_get;

    retrieve_scu scu{config};
    association assoc;

    // Should fail due to association not established, not missing destination
    auto result = scu.retrieve_study(assoc, "1.2.3.4.5");
    CHECK(result.is_err());
    // Error should be about association, not destination
}

// =============================================================================
// Batch Result Analysis Tests
// =============================================================================

TEST_CASE("analyzing retrieve results", "[services][retrieve_scu]") {
    SECTION("fully successful retrieve") {
        retrieve_result result;
        result.completed = 50;
        result.failed = 0;
        result.warning = 0;
        result.final_status = 0x0000;

        CHECK(result.is_success());
        CHECK_FALSE(result.has_failures());
        CHECK_FALSE(result.has_warnings());
    }

    SECTION("partial success retrieve") {
        retrieve_result result;
        result.completed = 45;
        result.failed = 5;
        result.warning = 0;
        result.final_status = 0x0000;

        CHECK_FALSE(result.is_success());
        CHECK(result.has_failures());
    }

    SECTION("retrieve with warnings") {
        retrieve_result result;
        result.completed = 48;
        result.failed = 0;
        result.warning = 2;
        result.final_status = 0x0000;

        CHECK(result.is_success());  // No failures
        CHECK(result.has_warnings());
    }
}

// =============================================================================
// Default Result Tests
// =============================================================================

TEST_CASE("retrieve_result default construction", "[services][retrieve_scu]") {
    retrieve_result result;

    CHECK(result.completed == 0);
    CHECK(result.failed == 0);
    CHECK(result.warning == 0);
    CHECK(result.final_status == 0);
    CHECK(result.elapsed.count() == 0);
    CHECK(result.received_instances.empty());

    // Default status 0 with no failures means success
    CHECK(result.is_success());
}

// =============================================================================
// Elapsed Time Tests
// =============================================================================

TEST_CASE("retrieve_result elapsed time", "[services][retrieve_scu]") {
    retrieve_result result;
    result.elapsed = std::chrono::milliseconds{5000};

    CHECK(result.elapsed.count() == 5000);
}
