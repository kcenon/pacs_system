/**
 * @file collectors_test.cpp
 * @brief Unit tests for DICOM metric collectors
 *
 * @see Issue #310 - IMonitor Integration and DICOM Metric Collector
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "pacs/monitoring/collectors/dicom_association_collector.hpp"
#include "pacs/monitoring/collectors/dicom_metrics_collector.hpp"
#include "pacs/monitoring/collectors/dicom_service_collector.hpp"
#include "pacs/monitoring/collectors/dicom_storage_collector.hpp"
#include "pacs/monitoring/pacs_monitor.hpp"

#include <chrono>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

using namespace pacs::monitoring;
using namespace std::chrono_literals;

// =============================================================================
// dicom_association_collector tests
// =============================================================================

TEST_CASE("dicom_association_collector initialization", "[monitoring][collectors]") {
    dicom_association_collector collector("TEST_AE");

    SECTION("Default state is not initialized") {
        CHECK_FALSE(collector.is_healthy());
    }

    SECTION("Initialize with empty config") {
        auto result = collector.initialize({});
        CHECK(result == true);
        CHECK(collector.is_healthy());
    }

    SECTION("Initialize with ae_title in config") {
        std::unordered_map<std::string, std::string> config;
        config["ae_title"] = "CONFIG_AE";

        auto result = collector.initialize(config);
        CHECK(result == true);
        CHECK(collector.get_ae_title() == "CONFIG_AE");
    }

    SECTION("Get name returns correct value") {
        CHECK(collector.get_name() == "dicom_association_collector");
    }
}

TEST_CASE("dicom_association_collector metrics collection", "[monitoring][collectors]") {
    dicom_association_collector collector("TEST_AE");
    (void)collector.initialize({});

    // Reset metrics before test
    pacs_metrics::global_metrics().reset();

    SECTION("Collect returns empty metrics initially") {
        auto metrics = collector.collect();
        CHECK_FALSE(metrics.empty());

        // Should have basic association metrics
        bool found_active = false;
        bool found_total = false;
        for (const auto& m : metrics) {
            if (m.name == "dicom_associations_active") {
                found_active = true;
                CHECK(m.value == 0.0);
                CHECK(m.type == "gauge");
            }
            if (m.name == "dicom_associations_total") {
                found_total = true;
                CHECK(m.value == 0.0);
                CHECK(m.type == "counter");
            }
        }
        CHECK(found_active);
        CHECK(found_total);
    }

    SECTION("Collect reflects recorded associations") {
        auto& metrics_instance = pacs_metrics::global_metrics();

        // Simulate associations
        metrics_instance.record_association_established();
        metrics_instance.record_association_established();
        metrics_instance.record_association_rejected();

        auto metrics = collector.collect();

        for (const auto& m : metrics) {
            if (m.name == "dicom_associations_active") {
                CHECK(m.value == 2.0);
            }
            if (m.name == "dicom_associations_total") {
                CHECK(m.value == 2.0);
            }
            if (m.name == "dicom_associations_rejected_total") {
                CHECK(m.value == 1.0);
            }
        }
    }

    SECTION("Metric types are correct") {
        auto types = collector.get_metric_types();
        CHECK(types.size() >= 5);

        bool has_active = false;
        bool has_total = false;
        for (const auto& t : types) {
            if (t == "dicom_associations_active") has_active = true;
            if (t == "dicom_associations_total") has_total = true;
        }
        CHECK(has_active);
        CHECK(has_total);
    }

    SECTION("Statistics track collection count") {
        (void)collector.collect();
        (void)collector.collect();
        (void)collector.collect();

        auto stats = collector.get_statistics();
        CHECK(stats["collection_count"] == 3.0);
    }
}

// =============================================================================
// dicom_service_collector tests
// =============================================================================

TEST_CASE("dicom_service_collector initialization", "[monitoring][collectors]") {
    dicom_service_collector collector("TEST_AE");

    SECTION("Default state is not initialized") {
        CHECK_FALSE(collector.is_healthy());
    }

    SECTION("Initialize succeeds") {
        auto result = collector.initialize({});
        CHECK(result == true);
        CHECK(collector.is_healthy());
    }

    SECTION("All operations enabled by default") {
        (void)collector.initialize({});
        CHECK(collector.is_operation_enabled(dimse_operation::c_echo));
        CHECK(collector.is_operation_enabled(dimse_operation::c_store));
        CHECK(collector.is_operation_enabled(dimse_operation::c_find));
        CHECK(collector.is_operation_enabled(dimse_operation::c_move));
        CHECK(collector.is_operation_enabled(dimse_operation::c_get));
    }

    SECTION("Operation can be disabled") {
        (void)collector.initialize({});
        collector.set_operation_enabled(dimse_operation::c_echo, false);
        CHECK_FALSE(collector.is_operation_enabled(dimse_operation::c_echo));
        CHECK(collector.is_operation_enabled(dimse_operation::c_store));
    }
}

TEST_CASE("dicom_service_collector metrics collection", "[monitoring][collectors]") {
    dicom_service_collector collector("TEST_AE");
    (void)collector.initialize({});

    // Reset metrics before test
    pacs_metrics::global_metrics().reset();

    SECTION("Collect returns metrics for all operations") {
        auto metrics = collector.collect();
        CHECK_FALSE(metrics.empty());

        // Should have metrics for each enabled DIMSE operation
        bool found_echo = false;
        bool found_store = false;
        for (const auto& m : metrics) {
            if (m.name == "dicom_c_echo_requests_total") found_echo = true;
            if (m.name == "dicom_c_store_requests_total") found_store = true;
        }
        CHECK(found_echo);
        CHECK(found_store);
    }

    SECTION("Collect reflects recorded operations") {
        auto& metrics_instance = pacs_metrics::global_metrics();

        // Simulate DIMSE operations
        metrics_instance.record_echo(true, 100us);
        metrics_instance.record_echo(true, 150us);
        metrics_instance.record_echo(false, 200us);
        metrics_instance.record_store(true, 1000us, 1024);

        auto metrics = collector.collect();

        for (const auto& m : metrics) {
            if (m.name == "dicom_c_echo_requests_total") {
                CHECK(m.value == 3.0);
            }
            if (m.name == "dicom_c_echo_success_total") {
                CHECK(m.value == 2.0);
            }
            if (m.name == "dicom_c_echo_failure_total") {
                CHECK(m.value == 1.0);
            }
            if (m.name == "dicom_c_store_requests_total") {
                CHECK(m.value == 1.0);
            }
        }
    }

    SECTION("Disabled operations are not collected") {
        collector.set_operation_enabled(dimse_operation::c_echo, false);

        auto metrics = collector.collect();

        bool found_echo = false;
        for (const auto& m : metrics) {
            if (m.name.find("c_echo") != std::string::npos) {
                found_echo = true;
            }
        }
        CHECK_FALSE(found_echo);
    }

    SECTION("Duration metrics are in seconds") {
        auto& metrics_instance = pacs_metrics::global_metrics();
        metrics_instance.reset();

        // Record a 1-second operation
        metrics_instance.record_store(true, 1000000us, 1024);  // 1 second

        auto metrics = collector.collect();

        for (const auto& m : metrics) {
            if (m.name == "dicom_c_store_duration_seconds_avg") {
                CHECK_THAT(m.value, Catch::Matchers::WithinRel(1.0, 0.01));
            }
        }
    }
}

// =============================================================================
// dicom_storage_collector tests
// =============================================================================

TEST_CASE("dicom_storage_collector initialization", "[monitoring][collectors]") {
    dicom_storage_collector collector("TEST_AE");

    SECTION("Default state is not initialized") {
        CHECK_FALSE(collector.is_healthy());
    }

    SECTION("Initialize succeeds") {
        auto result = collector.initialize({});
        CHECK(result == true);
        CHECK(collector.is_healthy());
    }

    SECTION("Pool metrics enabled by default") {
        (void)collector.initialize({});
        CHECK(collector.is_pool_metrics_enabled());
    }

    SECTION("Pool metrics can be disabled via config") {
        std::unordered_map<std::string, std::string> config;
        config["collect_pool_metrics"] = "false";

        (void)collector.initialize(config);
        CHECK_FALSE(collector.is_pool_metrics_enabled());
    }
}

TEST_CASE("dicom_storage_collector metrics collection", "[monitoring][collectors]") {
    dicom_storage_collector collector("TEST_AE");
    (void)collector.initialize({});

    // Reset metrics before test
    pacs_metrics::global_metrics().reset();

    SECTION("Collect returns transfer metrics") {
        auto metrics = collector.collect();
        CHECK_FALSE(metrics.empty());

        bool found_sent = false;
        bool found_received = false;
        bool found_images_stored = false;
        for (const auto& m : metrics) {
            if (m.name == "dicom_bytes_sent_total") found_sent = true;
            if (m.name == "dicom_bytes_received_total") found_received = true;
            if (m.name == "dicom_images_stored_total") found_images_stored = true;
        }
        CHECK(found_sent);
        CHECK(found_received);
        CHECK(found_images_stored);
    }

    SECTION("Collect reflects recorded transfers") {
        auto& metrics_instance = pacs_metrics::global_metrics();

        // Simulate storage operations
        metrics_instance.record_store(true, 1000us, 1024);
        metrics_instance.record_store(true, 1000us, 2048);
        metrics_instance.record_bytes_sent(512);

        auto metrics = collector.collect();

        for (const auto& m : metrics) {
            if (m.name == "dicom_bytes_received_total") {
                CHECK(m.value == 3072.0);  // 1024 + 2048
            }
            if (m.name == "dicom_bytes_sent_total") {
                CHECK(m.value == 512.0);
            }
            if (m.name == "dicom_images_stored_total") {
                CHECK(m.value == 2.0);
            }
        }
    }

    SECTION("Pool metrics included when enabled") {
        collector.set_pool_metrics_enabled(true);
        auto metrics = collector.collect();

        bool found_element_pool = false;
        bool found_dataset_pool = false;
        for (const auto& m : metrics) {
            if (m.name.find("element_pool") != std::string::npos) found_element_pool = true;
            if (m.name.find("dataset_pool") != std::string::npos) found_dataset_pool = true;
        }
        CHECK(found_element_pool);
        CHECK(found_dataset_pool);
    }

    SECTION("Pool metrics excluded when disabled") {
        collector.set_pool_metrics_enabled(false);
        auto metrics = collector.collect();

        bool found_pool_metric = false;
        for (const auto& m : metrics) {
            if (m.name.find("_pool_") != std::string::npos) {
                found_pool_metric = true;
            }
        }
        CHECK_FALSE(found_pool_metric);
    }

    SECTION("Metric units are correct") {
        auto metrics = collector.collect();

        for (const auto& m : metrics) {
            if (m.name.find("bytes") != std::string::npos && m.name.find("rate") == std::string::npos) {
                CHECK(m.unit == "bytes");
            }
            if (m.name.find("rate") != std::string::npos) {
                CHECK(m.unit == "bytes_per_second");
            }
        }
    }
}

// =============================================================================
// pacs_monitor tests
// =============================================================================

TEST_CASE("pacs_monitor initialization", "[monitoring][pacs_monitor]") {
    SECTION("Default construction") {
        pacs_monitor_config config;
        pacs_monitor monitor(config);

        CHECK(monitor.get_config().ae_title == "PACS_SCP");
        CHECK(monitor.get_config().enable_association_metrics);
        CHECK(monitor.get_config().enable_service_metrics);
        CHECK(monitor.get_config().enable_storage_metrics);
    }

    SECTION("Custom configuration") {
        pacs_monitor_config config;
        config.ae_title = "MY_PACS";
        config.enable_pool_metrics = false;

        pacs_monitor monitor(config);

        CHECK(monitor.get_config().ae_title == "MY_PACS");
        CHECK_FALSE(monitor.get_config().enable_pool_metrics);
    }
}

TEST_CASE("pacs_monitor metrics collection", "[monitoring][pacs_monitor]") {
    pacs_monitor_config config;
    config.ae_title = "TEST_PACS";
    pacs_monitor monitor(config);

    // Reset metrics before test
    pacs_metrics::global_metrics().reset();

    SECTION("Get metrics returns snapshot") {
        auto snapshot = monitor.get_metrics();
        CHECK(snapshot.source_id == "TEST_PACS");
        CHECK_FALSE(snapshot.metrics.empty());
    }

    SECTION("Custom metrics can be recorded") {
        monitor.record_metric("custom_gauge", 42.0);

        auto snapshot = monitor.get_metrics();

        bool found_custom = false;
        for (const auto& m : snapshot.metrics) {
            if (m.name == "custom_gauge") {
                found_custom = true;
                CHECK(m.value == 42.0);
            }
        }
        CHECK(found_custom);
    }

    SECTION("Custom metrics with tags") {
        std::unordered_map<std::string, std::string> tags;
        tags["component"] = "storage";
        tags["tier"] = "hot";

        monitor.record_metric("custom_tagged", 100.0, tags);

        auto snapshot = monitor.get_metrics();

        bool found_custom = false;
        for (const auto& m : snapshot.metrics) {
            if (m.name == "custom_tagged") {
                found_custom = true;
                CHECK(m.value == 100.0);
                CHECK(m.tags.at("component") == "storage");
                CHECK(m.tags.at("tier") == "hot");
            }
        }
        CHECK(found_custom);
    }

    SECTION("Reset clears metrics") {
        auto& metrics_instance = pacs_metrics::global_metrics();
        metrics_instance.record_echo(true, 100us);

        monitor.reset();

        auto snapshot = monitor.get_metrics();
        for (const auto& m : snapshot.metrics) {
            if (m.name == "dicom_c_echo_requests_total") {
                CHECK(m.value == 0.0);
            }
        }
    }
}

TEST_CASE("pacs_monitor health check", "[monitoring][pacs_monitor]") {
    pacs_monitor_config config;
    pacs_monitor monitor(config);

    SECTION("Default health check is healthy") {
        auto result = monitor.check_health();
        CHECK(result.is_healthy());
    }

    SECTION("Register and check custom health check") {
        bool component_healthy = true;

        monitor.register_health_check("test_component", [&]() {
            return component_healthy;
        });

        auto result = monitor.check_health();
        CHECK(result.is_healthy());
        CHECK(result.metadata.at("test_component") == "healthy");

        // Make component unhealthy
        component_healthy = false;
        result = monitor.check_health();
        CHECK_FALSE(result.is_healthy());
        CHECK(result.metadata.at("test_component") == "unhealthy");
    }

    SECTION("Unregister health check") {
        monitor.register_health_check("temp_component", []() { return true; });
        monitor.unregister_health_check("temp_component");

        auto result = monitor.check_health();
        CHECK(result.metadata.find("temp_component") == result.metadata.end());
    }

    SECTION("Health check measures duration") {
        monitor.register_health_check("slow_component", []() {
            std::this_thread::sleep_for(10ms);
            return true;
        });

        auto result = monitor.check_health();
        CHECK(result.check_duration.count() >= 10);
    }
}

TEST_CASE("pacs_monitor Prometheus export", "[monitoring][pacs_monitor]") {
    pacs_monitor_config config;
    config.metric_prefix = "test_pacs";
    pacs_monitor monitor(config);

    // Reset and add some data
    pacs_metrics::global_metrics().reset();
    pacs_metrics::global_metrics().record_echo(true, 100us);

    SECTION("to_prometheus returns valid format") {
        std::string output = monitor.to_prometheus();

        CHECK_FALSE(output.empty());
        CHECK(output.find("# HELP") != std::string::npos);
        CHECK(output.find("# TYPE") != std::string::npos);
        CHECK(output.find("test_pacs_") != std::string::npos);
    }
}

TEST_CASE("pacs_monitor configuration update", "[monitoring][pacs_monitor]") {
    pacs_monitor_config config;
    config.ae_title = "INITIAL_AE";
    pacs_monitor monitor(config);

    SECTION("Update configuration") {
        pacs_monitor_config new_config;
        new_config.ae_title = "UPDATED_AE";
        new_config.enable_pool_metrics = false;

        monitor.update_config(new_config);

        CHECK(monitor.get_config().ae_title == "UPDATED_AE");
        CHECK_FALSE(monitor.get_config().enable_pool_metrics);
    }
}

TEST_CASE("pacs_monitor collector access", "[monitoring][pacs_monitor]") {
    pacs_monitor_config config;
    pacs_monitor monitor(config);

    SECTION("Access association collector") {
        auto& collector = monitor.association_collector();
        CHECK(collector.is_healthy());
        CHECK(collector.get_name() == "dicom_association_collector");
    }

    SECTION("Access service collector") {
        auto& collector = monitor.service_collector();
        CHECK(collector.is_healthy());
        CHECK(collector.get_name() == "dicom_service_collector");
    }

    SECTION("Access storage collector") {
        auto& collector = monitor.storage_collector();
        CHECK(collector.is_healthy());
        CHECK(collector.get_name() == "dicom_storage_collector");
    }

    SECTION("Access unified CRTP-based collector") {
        auto& collector = monitor.unified_collector();
        CHECK(collector.is_healthy());
        CHECK(collector.get_name() == "dicom_metrics_collector");
    }
}

// =============================================================================
// dicom_metrics_collector tests (CRTP-based)
// Issue #490 - Implement CRTP-based DICOM metrics collector
// =============================================================================

TEST_CASE("dicom_metrics_collector initialization", "[monitoring][collectors][crtp]") {
    dicom_metrics_collector collector("TEST_AE");

    SECTION("Initialize with default config") {
        config_map config;
        config["ae_title"] = "TEST_AE";

        auto result = collector.initialize(config);
        CHECK(result == true);
        CHECK(collector.is_healthy());
        CHECK(collector.is_available());
    }

    SECTION("Get name returns correct value") {
        CHECK(collector.get_name() == "dicom_metrics_collector");
    }

    SECTION("Initialize with custom collection flags") {
        config_map config;
        config["collect_associations"] = "true";
        config["collect_transfers"] = "false";
        config["collect_storage"] = "true";
        config["collect_queries"] = "false";
        config["collect_pools"] = "true";

        auto result = collector.initialize(config);
        CHECK(result == true);
    }
}

TEST_CASE("dicom_metrics_collector metrics collection", "[monitoring][collectors][crtp]") {
    dicom_metrics_collector collector("TEST_AE");
    config_map config;
    config["ae_title"] = "TEST_AE";
    (void)collector.initialize(config);

    // Reset metrics before test
    pacs_metrics::global_metrics().reset();

    SECTION("Collect returns comprehensive metrics") {
        auto metrics = collector.collect();
        CHECK_FALSE(metrics.empty());

        // Should have association metrics
        bool found_associations = false;
        bool found_transfer = false;
        bool found_pool = false;

        for (const auto& m : metrics) {
            if (m.name.find("dicom_associations") != std::string::npos) {
                found_associations = true;
            }
            if (m.name.find("dicom_bytes") != std::string::npos ||
                m.name.find("dicom_images") != std::string::npos) {
                found_transfer = true;
            }
            if (m.name.find("pool") != std::string::npos) {
                found_pool = true;
            }
        }

        CHECK(found_associations);
        CHECK(found_transfer);
        CHECK(found_pool);
    }

    SECTION("Metrics include correct tags") {
        auto metrics = collector.collect();

        for (const auto& m : metrics) {
            // All metrics should have collector tag
            CHECK(m.tags.find("collector") != m.tags.end());
            CHECK(m.tags.at("collector") == "dicom_metrics_collector");

            // All metrics should have ae_title tag
            CHECK(m.tags.find("ae_title") != m.tags.end());
            CHECK(m.tags.at("ae_title") == "TEST_AE");
        }
    }

    SECTION("Collect reflects recorded operations") {
        auto& pacs = pacs_metrics::global_metrics();

        // Record some operations
        pacs.record_echo(true, 100us);
        pacs.record_store(true, 500us, 1024);
        pacs.record_association_established();

        auto metrics = collector.collect();

        // Check that metrics reflect the recorded data
        for (const auto& m : metrics) {
            if (m.name == "dicom_c_echo_total") {
                CHECK(m.value == 1.0);
            }
            if (m.name == "dicom_c_store_total") {
                CHECK(m.value == 1.0);
            }
            if (m.name == "dicom_associations_active") {
                CHECK(m.value == 1.0);
            }
        }
    }
}

TEST_CASE("dicom_metrics_collector snapshot", "[monitoring][collectors][crtp]") {
    dicom_metrics_collector collector("SNAPSHOT_AE");
    config_map config;
    (void)collector.initialize(config);

    // Reset metrics before test
    pacs_metrics::global_metrics().reset();

    SECTION("Get snapshot returns current state") {
        auto& pacs = pacs_metrics::global_metrics();

        // Record some data
        pacs.record_association_established();
        pacs.record_association_established();
        pacs.record_store(true, 1000us, 2048);
        pacs.record_query(true, 500us, 10);

        auto snapshot = collector.get_snapshot();

        CHECK(snapshot.active_associations == 2);
        CHECK(snapshot.total_associations == 2);
        CHECK(snapshot.store_operations == 1);
        CHECK(snapshot.successful_stores == 1);
        CHECK(snapshot.query_operations == 1);
        CHECK(snapshot.successful_queries == 1);
    }

    SECTION("Snapshot has timestamp") {
        auto snapshot = collector.get_snapshot();
        auto now = std::chrono::system_clock::now();

        // Snapshot timestamp should be recent (within 1 second)
        auto diff = std::chrono::duration_cast<std::chrono::seconds>(
            now - snapshot.timestamp).count();
        CHECK(diff <= 1);
    }
}

TEST_CASE("dicom_metrics_collector configuration", "[monitoring][collectors][crtp]") {
    dicom_metrics_collector collector;
    config_map config;
    (void)collector.initialize(config);

    pacs_metrics::global_metrics().reset();

    SECTION("Set and get AE title") {
        collector.set_ae_title("UPDATED_AE");
        CHECK(collector.get_ae_title() == "UPDATED_AE");
    }

    SECTION("Disable specific metric collections") {
        collector.set_collect_associations(false);

        auto metrics = collector.collect();

        // Should not contain association metrics
        for (const auto& m : metrics) {
            CHECK(m.name.find("dicom_associations") == std::string::npos);
        }
    }

    SECTION("Statistics tracking") {
        // Collect several times
        (void)collector.collect();
        (void)collector.collect();
        (void)collector.collect();

        auto stats = collector.get_statistics();
        CHECK(stats["collection_count"] == 3.0);
        CHECK(stats["enabled"] == 1.0);
    }
}

TEST_CASE("dicom_metrics_collector CRTP inheritance", "[monitoring][collectors][crtp]") {
    SECTION("Collector base methods work correctly") {
        dicom_metrics_collector collector("CRTP_AE");
        config_map config;
        config["enabled"] = "false";
        (void)collector.initialize(config);

        // When disabled, collect should return empty
        auto metrics = collector.collect();
        CHECK(metrics.empty());

        // But is_healthy should still return true (disabled is healthy)
        CHECK(collector.is_healthy());
    }

    SECTION("Get metric types returns expected list") {
        dicom_metrics_collector collector;
        auto types = collector.get_metric_types();

        CHECK_FALSE(types.empty());

        // Should include various metric types
        bool has_association = false;
        bool has_transfer = false;
        bool has_dimse = false;

        for (const auto& t : types) {
            if (t.find("associations") != std::string::npos) has_association = true;
            if (t.find("bytes") != std::string::npos) has_transfer = true;
            if (t.find("c_store") != std::string::npos || t.find("c_echo") != std::string::npos) has_dimse = true;
        }

        CHECK(has_association);
        CHECK(has_transfer);
        CHECK(has_dimse);
    }
}

TEST_CASE("pacs_monitor unified collector integration", "[monitoring][pacs_monitor][crtp]") {
    pacs_monitor_config config;
    config.enable_unified_collector = true;
    pacs_monitor monitor(config);

    pacs_metrics::global_metrics().reset();

    SECTION("Unified collector is accessible") {
        auto& collector = monitor.unified_collector();
        CHECK(collector.is_healthy());
    }

    SECTION("Get unified snapshot") {
        auto& pacs = pacs_metrics::global_metrics();
        pacs.record_association_established();
        pacs.record_store(true, 100us, 1024);

        auto snapshot = monitor.get_unified_snapshot();
        CHECK(snapshot.active_associations == 1);
        CHECK(snapshot.store_operations == 1);
    }

    SECTION("Unified collector appears in health check") {
        auto health = monitor.check_health();
        CHECK(health.is_healthy());
        // unified_collector should be checked
    }
}
