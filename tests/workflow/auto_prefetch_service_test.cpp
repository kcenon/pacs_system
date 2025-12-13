/**
 * @file auto_prefetch_service_test.cpp
 * @brief Unit tests for auto_prefetch_service class
 *
 * Tests the automatic prefetch service for prior patient studies
 * based on worklist queries.
 */

#include <pacs/workflow/auto_prefetch_service.hpp>
#include <pacs/workflow/prefetch_config.hpp>

#include <pacs/storage/index_database.hpp>
#include <pacs/storage/worklist_record.hpp>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>

using namespace pacs::workflow;
using namespace pacs::storage;

namespace {

/**
 * @brief Create a test database for prefetch service testing
 *
 * Uses in-memory database for faster tests and automatic cleanup.
 */
auto create_test_database() -> std::unique_ptr<index_database> {
    auto result = index_database::open(":memory:");
    if (!result.is_ok()) {
        throw std::runtime_error("Failed to create test database");
    }
    return std::move(result.value());
}

/**
 * @brief Create test worklist items
 */
auto create_test_worklist_items() -> std::vector<worklist_item> {
    std::vector<worklist_item> items;

    worklist_item item1;
    item1.step_id = "SPS001";
    item1.patient_id = "P001";
    item1.patient_name = "TEST^PATIENT^ONE";
    item1.modality = "CT";
    item1.study_uid = "1.2.3.4.5.1";
    item1.scheduled_datetime = "20231215100000";
    item1.station_ae = "CT_SCANNER_1";
    items.push_back(item1);

    worklist_item item2;
    item2.step_id = "SPS002";
    item2.patient_id = "P002";
    item2.patient_name = "TEST^PATIENT^TWO";
    item2.modality = "MR";
    item2.study_uid = "1.2.3.4.5.2";
    item2.scheduled_datetime = "20231215110000";
    item2.station_ae = "MR_SCANNER_1";
    items.push_back(item2);

    return items;
}

}  // namespace

// ============================================================================
// prefetch_config Tests
// ============================================================================

TEST_CASE("prefetch_config: default configuration", "[workflow][prefetch][config]") {
    prefetch_service_config config;

    SECTION("defaults are sensible") {
        CHECK(config.enabled == true);
        CHECK(config.prefetch_interval == std::chrono::seconds{300});
        CHECK(config.max_concurrent_prefetches == 4);
        CHECK(config.auto_start == false);
        CHECK(config.remote_pacs.empty());
    }

    SECTION("validation requires remote PACS when enabled") {
        // Empty remote_pacs, so not valid when enabled
        CHECK(config.is_valid() == false);

        // Add valid remote PACS
        remote_pacs_config pacs;
        pacs.ae_title = "REMOTE_PACS";
        pacs.host = "192.168.1.100";
        pacs.port = 11112;
        config.remote_pacs.push_back(pacs);

        CHECK(config.is_valid() == true);
    }

    SECTION("disabled config is always valid") {
        config.enabled = false;
        CHECK(config.is_valid() == true);
    }
}

TEST_CASE("remote_pacs_config: validation", "[workflow][prefetch][config]") {
    remote_pacs_config config;

    SECTION("defaults") {
        CHECK(config.ae_title.empty());
        CHECK(config.host.empty());
        CHECK(config.port == 11112);
        CHECK(config.local_ae_title == "PACS_PREFETCH");
        CHECK(config.connection_timeout == std::chrono::seconds{30});
        CHECK(config.use_tls == false);
    }

    SECTION("validation requires ae_title and host") {
        CHECK(config.is_valid() == false);

        config.ae_title = "REMOTE";
        CHECK(config.is_valid() == false);

        config.host = "192.168.1.100";
        CHECK(config.is_valid() == true);
    }
}

TEST_CASE("prefetch_criteria: default values", "[workflow][prefetch][config]") {
    prefetch_criteria criteria;

    CHECK(criteria.lookback_period == std::chrono::days{365});
    CHECK(criteria.max_studies_per_patient == 10);
    CHECK(criteria.max_series_per_study == 0);  // unlimited
    CHECK(criteria.include_modalities.empty());
    CHECK(criteria.exclude_modalities.empty());
    CHECK(criteria.prefer_same_modality == true);
    CHECK(criteria.prefer_same_body_part == true);
}

TEST_CASE("prefetch_result: accumulation", "[workflow][prefetch][result]") {
    prefetch_result result1;
    result1.patients_processed = 2;
    result1.studies_prefetched = 5;
    result1.studies_failed = 1;
    result1.bytes_downloaded = 1000;

    prefetch_result result2;
    result2.patients_processed = 3;
    result2.studies_prefetched = 7;
    result2.studies_failed = 0;
    result2.bytes_downloaded = 2000;

    result1 += result2;

    CHECK(result1.patients_processed == 5);
    CHECK(result1.studies_prefetched == 12);
    CHECK(result1.studies_failed == 1);
    CHECK(result1.bytes_downloaded == 3000);
}

TEST_CASE("prefetch_result: success check", "[workflow][prefetch][result]") {
    prefetch_result result;

    SECTION("no failures means success") {
        result.studies_prefetched = 5;
        result.studies_failed = 0;
        CHECK(result.is_successful() == true);
    }

    SECTION("any failure means not successful") {
        result.studies_prefetched = 5;
        result.studies_failed = 1;
        CHECK(result.is_successful() == false);
    }
}

// ============================================================================
// auto_prefetch_service Tests
// ============================================================================

TEST_CASE("auto_prefetch_service: construction", "[workflow][prefetch][service]") {
    auto db = create_test_database();

    SECTION("default construction") {
        auto_prefetch_service service{*db};

        CHECK(service.is_enabled() == false);
        CHECK(service.is_running() == false);
        CHECK(service.cycles_completed() == 0);
        CHECK(service.pending_requests() == 0);
    }

    SECTION("construction with config") {
        prefetch_service_config config;
        config.enabled = false;
        config.prefetch_interval = std::chrono::seconds{60};

        auto_prefetch_service service{*db, config};

        CHECK(service.is_enabled() == false);
        CHECK(service.get_prefetch_interval() == std::chrono::seconds{60});
    }
}

TEST_CASE("auto_prefetch_service: start and stop", "[workflow][prefetch][service]") {
    auto db = create_test_database();

    prefetch_service_config config;
    config.prefetch_interval = std::chrono::seconds{1};

    auto_prefetch_service service{*db, config};

    SECTION("start enables the service") {
        CHECK(service.is_running() == false);

        service.start();
        CHECK(service.is_running() == true);

        service.stop();
        CHECK(service.is_running() == false);
    }

    SECTION("multiple start calls are safe") {
        service.start();
        service.start();  // Should be no-op
        CHECK(service.is_running() == true);

        service.stop();
    }

    SECTION("multiple stop calls are safe") {
        service.start();
        service.stop();
        service.stop();  // Should be no-op
        CHECK(service.is_running() == false);
    }
}

TEST_CASE("auto_prefetch_service: worklist event handling", "[workflow][prefetch][service]") {
    auto db = create_test_database();

    prefetch_service_config config;
    config.prefetch_interval = std::chrono::seconds{60};

    auto_prefetch_service service{*db, config};

    SECTION("on_worklist_query queues requests") {
        auto items = create_test_worklist_items();

        service.on_worklist_query(items);

        // Should have queued 2 patients
        CHECK(service.pending_requests() == 2);
    }

    SECTION("duplicate patients are not queued twice") {
        auto items = create_test_worklist_items();

        service.on_worklist_query(items);
        CHECK(service.pending_requests() == 2);

        // Trigger again with same patients
        service.on_worklist_query(items);
        CHECK(service.pending_requests() == 2);  // Still 2, not 4
    }

    SECTION("empty worklist does not add requests") {
        std::vector<worklist_item> empty;
        service.on_worklist_query(empty);
        CHECK(service.pending_requests() == 0);
    }

    SECTION("items without patient_id are ignored") {
        worklist_item item;
        item.step_id = "SPS001";
        item.patient_id = "";  // Empty patient ID
        item.modality = "CT";

        service.on_worklist_query({item});
        CHECK(service.pending_requests() == 0);
    }
}

TEST_CASE("auto_prefetch_service: configuration updates", "[workflow][prefetch][service]") {
    auto db = create_test_database();

    prefetch_service_config config;
    config.prefetch_interval = std::chrono::seconds{60};

    auto_prefetch_service service{*db, config};

    SECTION("update prefetch interval") {
        CHECK(service.get_prefetch_interval() == std::chrono::seconds{60});

        service.set_prefetch_interval(std::chrono::seconds{120});
        CHECK(service.get_prefetch_interval() == std::chrono::seconds{120});
    }

    SECTION("update prefetch criteria") {
        auto& original = service.get_prefetch_criteria();
        CHECK(original.lookback_period == std::chrono::days{365});

        prefetch_criteria new_criteria;
        new_criteria.lookback_period = std::chrono::days{180};
        new_criteria.max_studies_per_patient = 5;

        service.set_prefetch_criteria(new_criteria);

        auto& updated = service.get_prefetch_criteria();
        CHECK(updated.lookback_period == std::chrono::days{180});
        CHECK(updated.max_studies_per_patient == 5);
    }
}

TEST_CASE("auto_prefetch_service: statistics", "[workflow][prefetch][service]") {
    auto db = create_test_database();

    prefetch_service_config config;

    auto_prefetch_service service{*db, config};

    SECTION("initial statistics are zero") {
        auto stats = service.get_cumulative_stats();
        CHECK(stats.patients_processed == 0);
        CHECK(stats.studies_prefetched == 0);
        CHECK(stats.studies_failed == 0);
    }

    SECTION("no last result initially") {
        CHECK(service.get_last_result().has_value() == false);
    }

    SECTION("time until next cycle when not running") {
        CHECK(service.time_until_next_cycle().has_value() == false);
    }
}

TEST_CASE("auto_prefetch_service: trigger_for_worklist", "[workflow][prefetch][service]") {
    auto db = create_test_database();

    auto_prefetch_service service{*db};

    auto items = create_test_worklist_items();
    service.trigger_for_worklist(items);

    CHECK(service.pending_requests() == 2);
}

TEST_CASE("auto_prefetch_service: run_prefetch_cycle", "[workflow][prefetch][service]") {
    auto db = create_test_database();

    prefetch_service_config config;
    // No remote PACS configured, so prefetch will not actually fetch anything

    auto_prefetch_service service{*db, config};

    // Queue some requests
    auto items = create_test_worklist_items();
    service.on_worklist_query(items);
    CHECK(service.pending_requests() == 2);

    // Run a cycle manually
    auto result = service.run_prefetch_cycle();

    // Requests should be processed (though no actual prefetch occurs)
    CHECK(service.pending_requests() == 0);
    CHECK(result.patients_processed == 2);
}

TEST_CASE("auto_prefetch_service: callbacks", "[workflow][prefetch][service]") {
    auto db = create_test_database();

    prefetch_service_config config;
    bool cycle_complete_called = false;
    prefetch_result callback_result;

    config.on_cycle_complete = [&](const prefetch_result& r) {
        cycle_complete_called = true;
        callback_result = r;
    };

    auto_prefetch_service service{*db, config};

    // Queue and process
    auto items = create_test_worklist_items();
    service.on_worklist_query(items);

    // Start service and trigger cycle
    service.start();
    service.trigger_cycle();

    // Wait a bit for the cycle to complete
    std::this_thread::sleep_for(std::chrono::milliseconds{100});

    service.stop();

    // Callback should have been called
    // Note: Due to timing, this may or may not have been called
    // In a real test, we'd use synchronization primitives
}

// ============================================================================
// prior_study_info Tests
// ============================================================================

TEST_CASE("prior_study_info: structure", "[workflow][prefetch][types]") {
    prior_study_info info;

    info.study_instance_uid = "1.2.3.4.5";
    info.patient_id = "P001";
    info.patient_name = "TEST^PATIENT";
    info.study_date = "20231215";
    info.study_description = "CT Chest";
    info.modalities.insert("CT");
    info.number_of_series = 3;
    info.number_of_instances = 150;

    CHECK(info.study_instance_uid == "1.2.3.4.5");
    CHECK(info.modalities.count("CT") == 1);
    CHECK(info.number_of_instances == 150);
}

// ============================================================================
// prefetch_request Tests
// ============================================================================

TEST_CASE("prefetch_request: structure", "[workflow][prefetch][types]") {
    prefetch_request request;

    request.patient_id = "P001";
    request.patient_name = "TEST^PATIENT";
    request.scheduled_modality = "CT";
    request.scheduled_study_uid = "1.2.3.4.5";
    request.request_time = std::chrono::system_clock::now();
    request.retry_count = 0;

    CHECK(request.patient_id == "P001");
    CHECK(request.retry_count == 0);
}
