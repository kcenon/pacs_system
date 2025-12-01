/**
 * @file monitoring_adapter_test.cpp
 * @brief Unit tests for monitoring_adapter
 */

#include <pacs/integration/monitoring_adapter.hpp>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <thread>

using namespace pacs::integration;

// =============================================================================
// Test Helpers
// =============================================================================

namespace {

/**
 * @brief RAII wrapper for monitoring adapter initialization/shutdown
 */
class monitoring_test_fixture {
public:
    explicit monitoring_test_fixture(const monitoring_config& config = {}) {
        monitoring_adapter::initialize(config);
    }

    ~monitoring_test_fixture() { monitoring_adapter::shutdown(); }

    monitoring_test_fixture(const monitoring_test_fixture&) = delete;
    monitoring_test_fixture& operator=(const monitoring_test_fixture&) = delete;
};

}  // namespace

// =============================================================================
// Initialization Tests
// =============================================================================

TEST_CASE("monitoring_adapter initialization and shutdown",
          "[monitoring_adapter][init]") {
    SECTION("Basic initialization") {
        monitoring_config config;
        config.enable_metrics = true;
        config.enable_tracing = true;

        monitoring_adapter::initialize(config);
        REQUIRE(monitoring_adapter::is_initialized());

        monitoring_adapter::shutdown();
        REQUIRE_FALSE(monitoring_adapter::is_initialized());
    }

    SECTION("Multiple initialization calls are safe") {
        monitoring_config config;
        config.enable_metrics = true;

        monitoring_adapter::initialize(config);
        REQUIRE(monitoring_adapter::is_initialized());

        // Second initialization should be ignored
        monitoring_adapter::initialize(config);
        REQUIRE(monitoring_adapter::is_initialized());

        monitoring_adapter::shutdown();
        REQUIRE_FALSE(monitoring_adapter::is_initialized());
    }

    SECTION("Shutdown without initialization is safe") {
        monitoring_adapter::shutdown();
        REQUIRE_FALSE(monitoring_adapter::is_initialized());
    }

    SECTION("Configuration is preserved") {
        monitoring_config config;
        config.enable_metrics = true;
        config.enable_tracing = false;
        config.export_interval = std::chrono::seconds(60);
        config.service_name = "test_service";
        config.max_samples_per_operation = 5000;

        monitoring_adapter::initialize(config);

        auto& stored_config = monitoring_adapter::get_config();
        REQUIRE(stored_config.enable_metrics == true);
        REQUIRE(stored_config.enable_tracing == false);
        REQUIRE(stored_config.export_interval == std::chrono::seconds(60));
        REQUIRE(stored_config.service_name == "test_service");
        REQUIRE(stored_config.max_samples_per_operation == 5000);

        monitoring_adapter::shutdown();
    }
}

// =============================================================================
// Counter Metrics Tests
// =============================================================================

TEST_CASE("monitoring_adapter counter metrics", "[monitoring_adapter][metrics]") {
    monitoring_test_fixture fixture;

    SECTION("Increment counter by default value") {
        monitoring_adapter::increment_counter("test_counter");
        monitoring_adapter::increment_counter("test_counter");
        monitoring_adapter::increment_counter("test_counter");

        // Counter is incremented (no direct read API, but should not throw)
        REQUIRE(monitoring_adapter::is_initialized());
    }

    SECTION("Increment counter by custom value") {
        monitoring_adapter::increment_counter("test_counter", 5);
        monitoring_adapter::increment_counter("test_counter", 3);

        REQUIRE(monitoring_adapter::is_initialized());
    }

    SECTION("Multiple counters are independent") {
        monitoring_adapter::increment_counter("counter_a", 10);
        monitoring_adapter::increment_counter("counter_b", 20);

        REQUIRE(monitoring_adapter::is_initialized());
    }
}

// =============================================================================
// Gauge Metrics Tests
// =============================================================================

TEST_CASE("monitoring_adapter gauge metrics", "[monitoring_adapter][metrics]") {
    monitoring_test_fixture fixture;

    SECTION("Set gauge value") {
        monitoring_adapter::set_gauge("test_gauge", 42.5);
        monitoring_adapter::set_gauge("test_gauge", 100.0);

        REQUIRE(monitoring_adapter::is_initialized());
    }

    SECTION("Multiple gauges are independent") {
        monitoring_adapter::set_gauge("gauge_a", 10.0);
        monitoring_adapter::set_gauge("gauge_b", 20.0);

        REQUIRE(monitoring_adapter::is_initialized());
    }
}

// =============================================================================
// Histogram Metrics Tests
// =============================================================================

TEST_CASE("monitoring_adapter histogram metrics", "[monitoring_adapter][metrics]") {
    monitoring_test_fixture fixture;

    SECTION("Record histogram samples") {
        for (int i = 0; i < 100; i++) {
            monitoring_adapter::record_histogram("test_histogram", i * 0.1);
        }

        REQUIRE(monitoring_adapter::is_initialized());
    }

    SECTION("Record timing measurements") {
        monitoring_adapter::record_timing("test_timing",
                                          std::chrono::milliseconds(150));
        monitoring_adapter::record_timing("test_timing",
                                          std::chrono::milliseconds(200));

        REQUIRE(monitoring_adapter::is_initialized());
    }
}

// =============================================================================
// DICOM-Specific Metrics Tests
// =============================================================================

TEST_CASE("monitoring_adapter DICOM metrics", "[monitoring_adapter][dicom]") {
    monitoring_test_fixture fixture;

    SECTION("Record C-STORE success") {
        monitoring_adapter::record_c_store(std::chrono::milliseconds(150),
                                           1024 * 1024,  // 1MB
                                           true);

        REQUIRE(monitoring_adapter::is_initialized());
    }

    SECTION("Record C-STORE failure") {
        monitoring_adapter::record_c_store(std::chrono::milliseconds(50),
                                           0,
                                           false);

        REQUIRE(monitoring_adapter::is_initialized());
    }

    SECTION("Record multiple C-STORE operations") {
        for (int i = 0; i < 10; i++) {
            monitoring_adapter::record_c_store(
                std::chrono::milliseconds(100 + i * 10),
                static_cast<std::size_t>(1024 * i),
                i % 2 == 0);
        }

        REQUIRE(monitoring_adapter::is_initialized());
    }

    SECTION("Record C-FIND at different levels") {
        monitoring_adapter::record_c_find(std::chrono::milliseconds(50), 10,
                                          query_level::patient);
        monitoring_adapter::record_c_find(std::chrono::milliseconds(30), 5,
                                          query_level::study);
        monitoring_adapter::record_c_find(std::chrono::milliseconds(20), 15,
                                          query_level::series);
        monitoring_adapter::record_c_find(std::chrono::milliseconds(10), 100,
                                          query_level::image);

        REQUIRE(monitoring_adapter::is_initialized());
    }

    SECTION("Record association establishment and release") {
        monitoring_adapter::record_association("MODALITY1", true);
        monitoring_adapter::record_association("MODALITY2", true);
        monitoring_adapter::record_association("MODALITY1", false);
        monitoring_adapter::record_association("MODALITY2", false);

        REQUIRE(monitoring_adapter::is_initialized());
    }

    SECTION("Update storage statistics") {
        monitoring_adapter::update_storage_stats(1000, 1024 * 1024 * 100);  // 100MB
        monitoring_adapter::update_storage_stats(2000, 1024 * 1024 * 200);  // 200MB

        REQUIRE(monitoring_adapter::is_initialized());
    }
}

// =============================================================================
// Distributed Tracing Tests
// =============================================================================

TEST_CASE("monitoring_adapter distributed tracing", "[monitoring_adapter][tracing]") {
    monitoring_config config;
    config.enable_tracing = true;
    monitoring_test_fixture fixture(config);

    SECTION("Create and finish span") {
        {
            auto span = monitoring_adapter::start_span("test_operation");
            REQUIRE(span.is_valid());
            REQUIRE_FALSE(span.trace_id().empty());
            REQUIRE_FALSE(span.span_id().empty());
        }  // span automatically finished

        REQUIRE(monitoring_adapter::is_initialized());
    }

    SECTION("Span with tags") {
        {
            auto span = monitoring_adapter::start_span("c_store");
            span.set_tag("calling_ae", "MODALITY1");
            span.set_tag("sop_class", "1.2.840.10008.5.1.4.1.1.2");
        }

        REQUIRE(monitoring_adapter::is_initialized());
    }

    SECTION("Span with events") {
        {
            auto span = monitoring_adapter::start_span("query_operation");
            span.add_event("query_started");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            span.add_event("query_completed");
        }

        REQUIRE(monitoring_adapter::is_initialized());
    }

    SECTION("Span with error") {
        {
            auto span = monitoring_adapter::start_span("failing_operation");
            try {
                throw std::runtime_error("Test error");
            } catch (const std::exception& e) {
                span.set_error(e);
            }
        }

        REQUIRE(monitoring_adapter::is_initialized());
    }

    SECTION("Move semantics for span") {
        auto span1 = monitoring_adapter::start_span("operation1");
        auto trace_id = span1.trace_id();
        auto span_id = span1.span_id();

        auto span2 = std::move(span1);
        REQUIRE(span2.is_valid());
        REQUIRE(span2.trace_id() == trace_id);
        REQUIRE(span2.span_id() == span_id);
    }
}

// =============================================================================
// Health Check Tests
// =============================================================================

TEST_CASE("monitoring_adapter health checks", "[monitoring_adapter][health]") {
    monitoring_test_fixture fixture;

    SECTION("Get health with no registered checks") {
        auto health = monitoring_adapter::get_health();
        REQUIRE(health.healthy);
        REQUIRE(health.status == "healthy");
        REQUIRE(health.components.empty());
    }

    SECTION("Register healthy component") {
        monitoring_adapter::register_health_check("database", []() {
            return true;  // DB is healthy
        });

        auto health = monitoring_adapter::get_health();
        REQUIRE(health.healthy);
        REQUIRE(health.components.at("database") == "healthy");

        monitoring_adapter::unregister_health_check("database");
    }

    SECTION("Register unhealthy component") {
        monitoring_adapter::register_health_check("storage", []() {
            return false;  // Storage is unhealthy
        });

        auto health = monitoring_adapter::get_health();
        REQUIRE_FALSE(health.healthy);
        REQUIRE(health.status == "degraded");
        REQUIRE(health.components.at("storage") == "unhealthy");

        monitoring_adapter::unregister_health_check("storage");
    }

    SECTION("Multiple health checks") {
        monitoring_adapter::register_health_check("database", []() { return true; });
        monitoring_adapter::register_health_check("storage", []() { return true; });
        monitoring_adapter::register_health_check("network", []() { return true; });

        auto health = monitoring_adapter::get_health();
        REQUIRE(health.healthy);
        REQUIRE(health.components.size() == 3);

        monitoring_adapter::unregister_health_check("database");
        monitoring_adapter::unregister_health_check("storage");
        monitoring_adapter::unregister_health_check("network");
    }

    SECTION("Health check with exception") {
        monitoring_adapter::register_health_check("failing_component", []() -> bool {
            throw std::runtime_error("Check failed");
        });

        auto health = monitoring_adapter::get_health();
        REQUIRE_FALSE(health.healthy);
        REQUIRE(health.status == "degraded");
        REQUIRE(health.components.at("failing_component").find("error") !=
                std::string::npos);

        monitoring_adapter::unregister_health_check("failing_component");
    }

    SECTION("Unregister non-existent component is safe") {
        monitoring_adapter::unregister_health_check("non_existent");
        REQUIRE(monitoring_adapter::is_initialized());
    }
}

// =============================================================================
// Thread Safety Tests
// =============================================================================

TEST_CASE("monitoring_adapter thread safety", "[monitoring_adapter][threading]") {
    monitoring_test_fixture fixture;

    SECTION("Concurrent counter increments") {
        constexpr int num_threads = 4;
        constexpr int increments_per_thread = 1000;

        std::vector<std::thread> threads;
        for (int i = 0; i < num_threads; i++) {
            threads.emplace_back([&]() {
                for (int j = 0; j < increments_per_thread; j++) {
                    monitoring_adapter::increment_counter("concurrent_counter");
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        REQUIRE(monitoring_adapter::is_initialized());
    }

    SECTION("Concurrent DICOM metrics recording") {
        constexpr int num_threads = 4;
        constexpr int operations_per_thread = 100;

        std::vector<std::thread> threads;
        for (int i = 0; i < num_threads; i++) {
            threads.emplace_back([&]() {
                for (int j = 0; j < operations_per_thread; j++) {
                    monitoring_adapter::record_c_store(
                        std::chrono::milliseconds(100),
                        1024,
                        true);
                    monitoring_adapter::record_association("MODALITY", true);
                    monitoring_adapter::record_association("MODALITY", false);
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        REQUIRE(monitoring_adapter::is_initialized());
    }

    SECTION("Concurrent span creation") {
        constexpr int num_threads = 4;
        constexpr int spans_per_thread = 50;

        std::vector<std::thread> threads;
        for (int i = 0; i < num_threads; i++) {
            threads.emplace_back([&]() {
                for (int j = 0; j < spans_per_thread; j++) {
                    auto span = monitoring_adapter::start_span("concurrent_op");
                    span.set_tag("thread", std::to_string(i));
                    span.add_event("processing");
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        REQUIRE(monitoring_adapter::is_initialized());
    }

    SECTION("Concurrent health check registration and queries") {
        constexpr int num_threads = 4;

        std::vector<std::thread> threads;
        for (int i = 0; i < num_threads; i++) {
            threads.emplace_back([i]() {
                std::string component = "component_" + std::to_string(i);
                monitoring_adapter::register_health_check(component, []() {
                    return true;
                });

                for (int j = 0; j < 10; j++) {
                    auto health = monitoring_adapter::get_health();
                    (void)health;
                }

                monitoring_adapter::unregister_health_check(component);
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        REQUIRE(monitoring_adapter::is_initialized());
    }
}

// =============================================================================
// Integration Pattern Tests
// =============================================================================

TEST_CASE("monitoring_adapter usage patterns", "[monitoring_adapter][integration]") {
    monitoring_test_fixture fixture;

    SECTION("C-STORE operation pattern with tracing") {
        // Start trace span
        auto span = monitoring_adapter::start_span("c_store");
        span.set_tag("calling_ae", "MODALITY1");
        span.set_tag("sop_class", "1.2.840.10008.5.1.4.1.1.2");

        auto start = std::chrono::steady_clock::now();

        // Simulate operation
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        auto duration = std::chrono::steady_clock::now() - start;
        monitoring_adapter::record_c_store(
            std::chrono::duration_cast<std::chrono::nanoseconds>(duration),
            1024 * 1024,
            true);

        span.add_event("store_complete");

        REQUIRE(monitoring_adapter::is_initialized());
    }

    SECTION("Query operation with level-specific metrics") {
        auto span = monitoring_adapter::start_span("c_find");
        span.set_tag("query_level", "STUDY");

        auto start = std::chrono::steady_clock::now();

        // Simulate query
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        auto duration = std::chrono::steady_clock::now() - start;
        monitoring_adapter::record_c_find(
            std::chrono::duration_cast<std::chrono::nanoseconds>(duration),
            25,  // matches
            query_level::study);

        REQUIRE(monitoring_adapter::is_initialized());
    }

    SECTION("Full PACS operation monitoring") {
        // Register health checks for all components
        monitoring_adapter::register_health_check("database", []() { return true; });
        monitoring_adapter::register_health_check("storage", []() { return true; });
        monitoring_adapter::register_health_check("network", []() { return true; });

        // Update storage stats
        monitoring_adapter::update_storage_stats(1000, 1024 * 1024 * 1024);  // 1GB

        // Record some associations
        monitoring_adapter::record_association("MODALITY1", true);
        monitoring_adapter::record_association("MODALITY2", true);

        // Record operations
        for (int i = 0; i < 5; i++) {
            monitoring_adapter::record_c_store(
                std::chrono::milliseconds(100 + i * 20),
                1024 * 1024,
                true);
        }

        // Check overall health
        auto health = monitoring_adapter::get_health();
        REQUIRE(health.healthy);
        REQUIRE(health.components.size() == 3);

        // Cleanup
        monitoring_adapter::unregister_health_check("database");
        monitoring_adapter::unregister_health_check("storage");
        monitoring_adapter::unregister_health_check("network");
    }
}
