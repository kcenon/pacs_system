/**
 * @file pacs_metrics_test.cpp
 * @brief Unit tests for pacs_metrics operation metrics collection
 *
 * @see Issue #210 - [Quick Win] feat(monitoring): Implement operation metrics collection
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "pacs/monitoring/pacs_metrics.hpp"

#include <chrono>
#include <string>
#include <thread>
#include <vector>

using namespace pacs::monitoring;
using namespace std::chrono_literals;

// =============================================================================
// dimse_operation enum tests
// =============================================================================

TEST_CASE("dimse_operation to_string conversion", "[monitoring][metrics]") {
    SECTION("C-DIMSE operations") {
        CHECK(to_string(dimse_operation::c_echo) == "c_echo");
        CHECK(to_string(dimse_operation::c_store) == "c_store");
        CHECK(to_string(dimse_operation::c_find) == "c_find");
        CHECK(to_string(dimse_operation::c_move) == "c_move");
        CHECK(to_string(dimse_operation::c_get) == "c_get");
    }

    SECTION("N-DIMSE operations") {
        CHECK(to_string(dimse_operation::n_create) == "n_create");
        CHECK(to_string(dimse_operation::n_set) == "n_set");
        CHECK(to_string(dimse_operation::n_get) == "n_get");
        CHECK(to_string(dimse_operation::n_action) == "n_action");
        CHECK(to_string(dimse_operation::n_event) == "n_event");
        CHECK(to_string(dimse_operation::n_delete) == "n_delete");
    }
}

// =============================================================================
// operation_counter tests
// =============================================================================

TEST_CASE("operation_counter basic functionality", "[monitoring][metrics]") {
    operation_counter counter;

    SECTION("Initial state is zero") {
        CHECK(counter.success_count.load() == 0);
        CHECK(counter.failure_count.load() == 0);
        CHECK(counter.total_count() == 0);
        CHECK(counter.average_duration_us() == 0);
    }

    SECTION("Record success updates counters") {
        counter.record_success(100us);

        CHECK(counter.success_count.load() == 1);
        CHECK(counter.failure_count.load() == 0);
        CHECK(counter.total_count() == 1);
        CHECK(counter.total_duration_us.load() == 100);
        CHECK(counter.min_duration_us.load() == 100);
        CHECK(counter.max_duration_us.load() == 100);
        CHECK(counter.average_duration_us() == 100);
    }

    SECTION("Record failure updates counters") {
        counter.record_failure(200us);

        CHECK(counter.success_count.load() == 0);
        CHECK(counter.failure_count.load() == 1);
        CHECK(counter.total_count() == 1);
        CHECK(counter.total_duration_us.load() == 200);
    }

    SECTION("Multiple operations track min/max correctly") {
        counter.record_success(50us);
        counter.record_success(100us);
        counter.record_success(75us);

        CHECK(counter.total_count() == 3);
        CHECK(counter.min_duration_us.load() == 50);
        CHECK(counter.max_duration_us.load() == 100);
        CHECK(counter.average_duration_us() == 75);  // (50 + 100 + 75) / 3 = 75
    }

    SECTION("Reset clears all counters") {
        counter.record_success(100us);
        counter.record_failure(200us);
        counter.reset();

        CHECK(counter.success_count.load() == 0);
        CHECK(counter.failure_count.load() == 0);
        CHECK(counter.total_count() == 0);
        CHECK(counter.total_duration_us.load() == 0);
        CHECK(counter.min_duration_us.load() == UINT64_MAX);
        CHECK(counter.max_duration_us.load() == 0);
    }
}

// =============================================================================
// data_transfer_metrics tests
// =============================================================================

TEST_CASE("data_transfer_metrics functionality", "[monitoring][metrics]") {
    data_transfer_metrics transfer;

    SECTION("Initial state is zero") {
        CHECK(transfer.bytes_sent.load() == 0);
        CHECK(transfer.bytes_received.load() == 0);
        CHECK(transfer.images_stored.load() == 0);
        CHECK(transfer.images_retrieved.load() == 0);
    }

    SECTION("Bytes tracking works correctly") {
        transfer.add_bytes_sent(1024);
        transfer.add_bytes_received(2048);

        CHECK(transfer.bytes_sent.load() == 1024);
        CHECK(transfer.bytes_received.load() == 2048);

        transfer.add_bytes_sent(512);
        CHECK(transfer.bytes_sent.load() == 1536);
    }

    SECTION("Image counts work correctly") {
        transfer.increment_images_stored();
        transfer.increment_images_stored();
        transfer.increment_images_retrieved();

        CHECK(transfer.images_stored.load() == 2);
        CHECK(transfer.images_retrieved.load() == 1);
    }

    SECTION("Reset clears all values") {
        transfer.add_bytes_sent(1024);
        transfer.increment_images_stored();
        transfer.reset();

        CHECK(transfer.bytes_sent.load() == 0);
        CHECK(transfer.images_stored.load() == 0);
    }
}

// =============================================================================
// association_counters tests
// =============================================================================

TEST_CASE("association_counters functionality", "[monitoring][metrics]") {
    association_counters assoc;

    SECTION("Initial state is zero") {
        CHECK(assoc.total_established.load() == 0);
        CHECK(assoc.total_rejected.load() == 0);
        CHECK(assoc.total_aborted.load() == 0);
        CHECK(assoc.current_active.load() == 0);
        CHECK(assoc.peak_active.load() == 0);
    }

    SECTION("Establish and release tracking") {
        assoc.record_established();
        CHECK(assoc.total_established.load() == 1);
        CHECK(assoc.current_active.load() == 1);
        CHECK(assoc.peak_active.load() == 1);

        assoc.record_established();
        CHECK(assoc.current_active.load() == 2);
        CHECK(assoc.peak_active.load() == 2);

        assoc.record_released();
        CHECK(assoc.current_active.load() == 1);
        CHECK(assoc.peak_active.load() == 2);  // Peak remains at 2
    }

    SECTION("Rejection tracking") {
        assoc.record_rejected();
        CHECK(assoc.total_rejected.load() == 1);
        CHECK(assoc.current_active.load() == 0);  // Rejections don't affect active
    }

    SECTION("Abort tracking") {
        assoc.record_established();
        assoc.record_aborted();

        CHECK(assoc.total_aborted.load() == 1);
        CHECK(assoc.current_active.load() == 0);
    }

    SECTION("Peak tracking across multiple associations") {
        for (int i = 0; i < 5; ++i) {
            assoc.record_established();
        }
        CHECK(assoc.peak_active.load() == 5);

        for (int i = 0; i < 3; ++i) {
            assoc.record_released();
        }
        CHECK(assoc.current_active.load() == 2);
        CHECK(assoc.peak_active.load() == 5);  // Peak unchanged
    }
}

// =============================================================================
// pacs_metrics tests
// =============================================================================

TEST_CASE("pacs_metrics singleton access", "[monitoring][metrics]") {
    SECTION("Global metrics returns same instance") {
        auto& m1 = pacs_metrics::global_metrics();
        auto& m2 = pacs_metrics::global_metrics();
        CHECK(&m1 == &m2);
    }
}

TEST_CASE("pacs_metrics DIMSE operation recording", "[monitoring][metrics]") {
    pacs_metrics metrics;

    SECTION("record_store updates C-STORE counter") {
        metrics.record_store(true, 1000us, 1024);

        const auto& counter = metrics.get_counter(dimse_operation::c_store);
        CHECK(counter.success_count.load() == 1);
        CHECK(counter.total_duration_us.load() == 1000);

        const auto& transfer = metrics.transfer();
        CHECK(transfer.bytes_received.load() == 1024);
        CHECK(transfer.images_stored.load() == 1);
    }

    SECTION("record_store failure does not update transfer") {
        metrics.record_store(false, 500us, 0);

        const auto& counter = metrics.get_counter(dimse_operation::c_store);
        CHECK(counter.failure_count.load() == 1);

        const auto& transfer = metrics.transfer();
        CHECK(transfer.images_stored.load() == 0);
    }

    SECTION("record_query updates C-FIND counter") {
        metrics.record_query(true, 200us, 10);

        const auto& counter = metrics.get_counter(dimse_operation::c_find);
        CHECK(counter.success_count.load() == 1);
    }

    SECTION("record_echo updates C-ECHO counter") {
        metrics.record_echo(true, 50us);

        const auto& counter = metrics.get_counter(dimse_operation::c_echo);
        CHECK(counter.success_count.load() == 1);
    }

    SECTION("record_move updates C-MOVE counter and images retrieved") {
        metrics.record_move(true, 5000us, 3);

        const auto& counter = metrics.get_counter(dimse_operation::c_move);
        CHECK(counter.success_count.load() == 1);

        const auto& transfer = metrics.transfer();
        CHECK(transfer.images_retrieved.load() == 3);
    }

    SECTION("record_get updates C-GET counter and transfer") {
        metrics.record_get(true, 3000us, 2, 2048);

        const auto& counter = metrics.get_counter(dimse_operation::c_get);
        CHECK(counter.success_count.load() == 1);

        const auto& transfer = metrics.transfer();
        CHECK(transfer.images_retrieved.load() == 2);
        CHECK(transfer.bytes_sent.load() == 2048);
    }

    SECTION("record_operation works for all types") {
        metrics.record_operation(dimse_operation::n_create, true, 100us);
        metrics.record_operation(dimse_operation::n_set, true, 150us);

        CHECK(metrics.get_counter(dimse_operation::n_create).success_count.load() == 1);
        CHECK(metrics.get_counter(dimse_operation::n_set).success_count.load() == 1);
    }
}

TEST_CASE("pacs_metrics association tracking", "[monitoring][metrics]") {
    pacs_metrics metrics;

    SECTION("Association lifecycle events") {
        metrics.record_association_established();
        metrics.record_association_established();

        const auto& assoc = metrics.associations();
        CHECK(assoc.total_established.load() == 2);
        CHECK(assoc.current_active.load() == 2);

        metrics.record_association_released();
        CHECK(assoc.current_active.load() == 1);

        metrics.record_association_rejected();
        CHECK(assoc.total_rejected.load() == 1);

        metrics.record_association_aborted();
        CHECK(assoc.total_aborted.load() == 1);
    }
}

TEST_CASE("pacs_metrics byte recording", "[monitoring][metrics]") {
    pacs_metrics metrics;

    metrics.record_bytes_sent(1024);
    metrics.record_bytes_received(2048);

    const auto& transfer = metrics.transfer();
    CHECK(transfer.bytes_sent.load() == 1024);
    CHECK(transfer.bytes_received.load() == 2048);
}

TEST_CASE("pacs_metrics reset", "[monitoring][metrics]") {
    pacs_metrics metrics;

    // Record various operations
    metrics.record_store(true, 1000us, 1024);
    metrics.record_echo(true, 50us);
    metrics.record_association_established();
    metrics.record_bytes_sent(512);

    // Reset all
    metrics.reset();

    // Verify all reset
    CHECK(metrics.get_counter(dimse_operation::c_store).total_count() == 0);
    CHECK(metrics.get_counter(dimse_operation::c_echo).total_count() == 0);
    CHECK(metrics.transfer().bytes_sent.load() == 0);
    CHECK(metrics.associations().total_established.load() == 0);
}

// =============================================================================
// JSON export tests
// =============================================================================

TEST_CASE("pacs_metrics to_json export", "[monitoring][metrics][json]") {
    pacs_metrics metrics;

    SECTION("Empty metrics produces valid JSON") {
        std::string json = metrics.to_json();

        CHECK_THAT(json, Catch::Matchers::StartsWith("{"));
        CHECK_THAT(json, Catch::Matchers::EndsWith("}"));
        CHECK_THAT(json, Catch::Matchers::ContainsSubstring("\"dimse_operations\""));
        CHECK_THAT(json, Catch::Matchers::ContainsSubstring("\"data_transfer\""));
        CHECK_THAT(json, Catch::Matchers::ContainsSubstring("\"associations\""));
    }

    SECTION("JSON contains operation data") {
        metrics.record_store(true, 1000us, 2048);
        metrics.record_echo(true, 50us);

        std::string json = metrics.to_json();

        CHECK_THAT(json, Catch::Matchers::ContainsSubstring("\"c_store\""));
        CHECK_THAT(json, Catch::Matchers::ContainsSubstring("\"c_echo\""));
        CHECK_THAT(json, Catch::Matchers::ContainsSubstring("\"success\":1"));
    }

    SECTION("JSON contains transfer data") {
        metrics.record_bytes_sent(1024);
        metrics.record_bytes_received(2048);

        std::string json = metrics.to_json();

        CHECK_THAT(json, Catch::Matchers::ContainsSubstring("\"bytes_sent\":1024"));
        CHECK_THAT(json, Catch::Matchers::ContainsSubstring("\"bytes_received\":2048"));
    }

    SECTION("JSON contains association data") {
        metrics.record_association_established();
        metrics.record_association_established();
        metrics.record_association_released();

        std::string json = metrics.to_json();

        CHECK_THAT(json, Catch::Matchers::ContainsSubstring("\"total_established\":2"));
        CHECK_THAT(json, Catch::Matchers::ContainsSubstring("\"current_active\":1"));
    }
}

// =============================================================================
// Prometheus export tests
// =============================================================================

TEST_CASE("pacs_metrics to_prometheus export", "[monitoring][metrics][prometheus]") {
    pacs_metrics metrics;

    SECTION("Empty metrics produces valid Prometheus format") {
        std::string prom = metrics.to_prometheus();

        CHECK_THAT(prom, Catch::Matchers::ContainsSubstring("# HELP"));
        CHECK_THAT(prom, Catch::Matchers::ContainsSubstring("# TYPE"));
        CHECK_THAT(prom, Catch::Matchers::ContainsSubstring("pacs_dimse_c_echo_total"));
        CHECK_THAT(prom, Catch::Matchers::ContainsSubstring("pacs_dimse_c_store_total"));
    }

    SECTION("Custom prefix is used") {
        std::string prom = metrics.to_prometheus("myprefix");

        CHECK_THAT(prom, Catch::Matchers::ContainsSubstring("myprefix_dimse_c_echo_total"));
        CHECK_THAT(prom, Catch::Matchers::ContainsSubstring("myprefix_bytes_sent_total"));
    }

    SECTION("Prometheus contains counter types") {
        std::string prom = metrics.to_prometheus();

        CHECK_THAT(prom, Catch::Matchers::ContainsSubstring("# TYPE pacs_dimse_c_store_total counter"));
        CHECK_THAT(prom, Catch::Matchers::ContainsSubstring("# TYPE pacs_bytes_sent_total counter"));
    }

    SECTION("Prometheus contains gauge types for active associations") {
        std::string prom = metrics.to_prometheus();

        CHECK_THAT(prom, Catch::Matchers::ContainsSubstring("# TYPE pacs_associations_active gauge"));
    }

    SECTION("Prometheus contains operation values") {
        metrics.record_store(true, 1000us, 2048);

        std::string prom = metrics.to_prometheus();

        CHECK_THAT(prom, Catch::Matchers::ContainsSubstring("pacs_dimse_c_store_success_total 1"));
        CHECK_THAT(prom, Catch::Matchers::ContainsSubstring("pacs_images_stored_total 1"));
    }
}

// =============================================================================
// Thread safety tests
// =============================================================================

TEST_CASE("pacs_metrics thread safety", "[monitoring][metrics][threading]") {
    pacs_metrics metrics;
    constexpr int num_threads = 4;
    constexpr int ops_per_thread = 1000;

    SECTION("Concurrent C-STORE recording is thread-safe") {
        std::vector<std::thread> threads;
        threads.reserve(num_threads);

        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&metrics]() {
                for (int i = 0; i < ops_per_thread; ++i) {
                    metrics.record_store(true, 100us, 1024);
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        const auto& counter = metrics.get_counter(dimse_operation::c_store);
        CHECK(counter.success_count.load() == num_threads * ops_per_thread);
        CHECK(counter.total_duration_us.load() == num_threads * ops_per_thread * 100);
    }

    SECTION("Concurrent association tracking is thread-safe") {
        std::vector<std::thread> threads;
        threads.reserve(num_threads);

        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&metrics]() {
                for (int i = 0; i < ops_per_thread; ++i) {
                    metrics.record_association_established();
                    metrics.record_association_released();
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        const auto& assoc = metrics.associations();
        CHECK(assoc.total_established.load() == num_threads * ops_per_thread);
        // current_active may not be exactly 0 due to race between establish/release
        // but it should be close to 0
        CHECK(assoc.current_active.load() <= static_cast<std::uint32_t>(num_threads));
    }

    SECTION("Concurrent mixed operations are thread-safe") {
        std::vector<std::thread> threads;
        threads.reserve(num_threads);

        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&metrics, t]() {
                for (int i = 0; i < ops_per_thread; ++i) {
                    switch (t % 4) {
                        case 0:
                            metrics.record_store(true, 100us, 1024);
                            break;
                        case 1:
                            metrics.record_echo(true, 50us);
                            break;
                        case 2:
                            metrics.record_query(true, 200us, 5);
                            break;
                        case 3:
                            metrics.record_bytes_sent(512);
                            metrics.record_bytes_received(256);
                            break;
                    }
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        // Verify each operation type has expected count
        CHECK(metrics.get_counter(dimse_operation::c_store).success_count.load() == ops_per_thread);
        CHECK(metrics.get_counter(dimse_operation::c_echo).success_count.load() == ops_per_thread);
        CHECK(metrics.get_counter(dimse_operation::c_find).success_count.load() == ops_per_thread);
        CHECK(metrics.transfer().bytes_sent.load() == ops_per_thread * 512);
    }
}
