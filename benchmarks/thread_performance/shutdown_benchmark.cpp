/**
 * @file shutdown_benchmark.cpp
 * @brief Server shutdown time benchmarks
 *
 * Measures the time required to gracefully shut down the DICOM server
 * under various conditions (idle, with active connections, under load).
 *
 * Key metrics:
 * - Idle server shutdown time
 * - Shutdown with active associations
 * - Shutdown under load
 * - Resource cleanup verification
 *
 * @see Issue #154 - Establish performance baseline benchmarks for thread migration
 */

#include "benchmark_common.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

#include <atomic>
#include <future>
#include <iostream>
#include <latch>
#include <thread>
#include <vector>

using namespace pacs::benchmark;
using namespace pacs::network;
using namespace pacs::network::dimse;
using namespace pacs::services;

// =============================================================================
// Shutdown Benchmarks
// =============================================================================

TEST_CASE("Idle server shutdown time", "[benchmark][shutdown][idle]") {
    SECTION("Immediate shutdown after start") {
        constexpr int iterations = 10;
        benchmark_stats stats;

        for (int i = 0; i < iterations; ++i) {
            auto port = find_available_port();

            server_config config;
            config.ae_title = "SHUTDOWN_TEST";
            config.port = port;
            config.max_associations = 50;
            config.idle_timeout = std::chrono::seconds{30};
            config.implementation_class_uid = "1.2.826.0.1.3680043.9.8888.5";

            auto server = std::make_unique<dicom_server>(config);
            server->register_service(std::make_shared<verification_scp>());

            REQUIRE(server->start().is_ok());
            std::this_thread::sleep_for(std::chrono::milliseconds{100});

            high_resolution_timer timer;
            timer.start();

            server->stop();

            timer.stop();
            stats.record(static_cast<double>(timer.elapsed_us().count()) / 1000.0);

            server.reset();
        }

        std::cout << "\n=== Idle Server Shutdown ===" << std::endl;
        std::cout << "  Iterations: " << stats.count << std::endl;
        std::cout << "  Mean: " << stats.mean_ms() << " ms" << std::endl;
        std::cout << "  Std Dev: " << stats.stddev_ms() << " ms" << std::endl;
        std::cout << "  Min: " << stats.min_ms << " ms" << std::endl;
        std::cout << "  Max: " << stats.max_ms << " ms" << std::endl;

        // Baseline: idle shutdown should complete within 1 second
        REQUIRE(stats.mean_ms() < 1000.0);
        REQUIRE(stats.max_ms < 2000.0);
    }

    SECTION("Shutdown after warmup traffic") {
        constexpr int iterations = 5;
        benchmark_stats stats;

        for (int i = 0; i < iterations; ++i) {
            auto port = find_available_port();

            server_config config;
            config.ae_title = "WARMUP_TEST";
            config.port = port;
            config.max_associations = 50;
            config.implementation_class_uid = "1.2.826.0.1.3680043.9.8888.6";

            auto server = std::make_unique<dicom_server>(config);
            server->register_service(std::make_shared<verification_scp>());

            REQUIRE(server->start().is_ok());
            std::this_thread::sleep_for(std::chrono::milliseconds{100});

            // Perform some warmup operations
            for (int j = 0; j < 10; ++j) {
                auto assoc_config = make_echo_config("WARMUP_SCU", "WARMUP_TEST");
                auto result = association::connect("localhost", port, assoc_config, default_timeout);
                if (result.is_ok()) {
                    auto& assoc = result.value();
                    auto ctx = assoc.accepted_context_id(verification_sop_class_uid);
                    if (ctx) {
                        auto echo_rq = make_c_echo_rq(1, verification_sop_class_uid);
                        (void)assoc.send_dimse(*ctx, echo_rq);
                        (void)assoc.receive_dimse(default_timeout);
                    }
                    (void)assoc.release(std::chrono::milliseconds{500});
                }
            }

            // Wait for all connections to close
            std::this_thread::sleep_for(std::chrono::milliseconds{200});

            high_resolution_timer timer;
            timer.start();

            server->stop();

            timer.stop();
            stats.record(static_cast<double>(timer.elapsed_us().count()) / 1000.0);

            server.reset();
        }

        std::cout << "\n=== Shutdown After Warmup ===" << std::endl;
        std::cout << "  Iterations: " << stats.count << std::endl;
        std::cout << "  Mean: " << stats.mean_ms() << " ms" << std::endl;
        std::cout << "  Std Dev: " << stats.stddev_ms() << " ms" << std::endl;
        std::cout << "  Min: " << stats.min_ms << " ms" << std::endl;
        std::cout << "  Max: " << stats.max_ms << " ms" << std::endl;

        REQUIRE(stats.mean_ms() < 2000.0);
    }
}

TEST_CASE("Shutdown with active connections", "[benchmark][shutdown][active]") {
    SECTION("Shutdown with held connections") {
        auto port = find_available_port();

        server_config config;
        config.ae_title = "ACTIVE_TEST";
        config.port = port;
        config.max_associations = 50;
        config.implementation_class_uid = "1.2.826.0.1.3680043.9.8888.7";

        auto server = std::make_unique<dicom_server>(config);
        server->register_service(std::make_shared<verification_scp>());

        REQUIRE(server->start().is_ok());
        std::this_thread::sleep_for(std::chrono::milliseconds{100});

        // Establish and hold multiple connections
        constexpr int num_connections = 5;
        std::vector<std::optional<association>> connections;

        for (int i = 0; i < num_connections; ++i) {
            auto assoc_config = make_echo_config(
                "HOLD_" + std::to_string(i), "ACTIVE_TEST");
            auto result = association::connect("localhost", port, assoc_config, default_timeout);
            if (result.is_ok()) {
                connections.push_back(std::move(result.value()));
            }
        }

        size_t active_before = server->active_associations();
        std::cout << "\n=== Shutdown With Active Connections ===" << std::endl;
        std::cout << "  Held connections: " << connections.size() << std::endl;
        std::cout << "  Active associations (before): " << active_before << std::endl;

        high_resolution_timer timer;
        timer.start();

        // Shutdown should handle active connections gracefully
        server->stop();

        timer.stop();

        std::cout << "  Shutdown time: " << timer.elapsed_ms().count() << " ms" << std::endl;

        // Clean up client-side connections (they should be disconnected by now)
        connections.clear();

        // Baseline: shutdown with active connections should complete within 5 seconds
        REQUIRE(timer.elapsed_ms().count() < 5000);

        server.reset();
    }
}

TEST_CASE("Shutdown under load", "[benchmark][shutdown][load]") {
    SECTION("Shutdown during active operations") {
        auto port = find_available_port();

        server_config config;
        config.ae_title = "LOAD_TEST";
        config.port = port;
        config.max_associations = 50;
        config.implementation_class_uid = "1.2.826.0.1.3680043.9.8888.8";

        auto server = std::make_unique<dicom_server>(config);
        server->register_service(std::make_shared<verification_scp>());

        auto storage_service = std::make_shared<storage_scp>();
        storage_service->set_handler([](
            const pacs::core::dicom_dataset& /* dataset */,
            const std::string& /* calling_ae */,
            const std::string& /* sop_class_uid */,
            const std::string& /* sop_instance_uid */) {
            return storage_status::success;
        });
        server->register_service(storage_service);

        REQUIRE(server->start().is_ok());
        std::this_thread::sleep_for(std::chrono::milliseconds{100});

        // Start background workers that generate load
        std::atomic<bool> stop_workers{false};
        std::atomic<size_t> operations_completed{0};

        auto worker = [&]() {
            while (!stop_workers.load()) {
                auto assoc_config = make_echo_config("LOAD_SCU", "LOAD_TEST");
                auto result = association::connect(
                    "localhost", port, assoc_config,
                    std::chrono::milliseconds{2000});

                if (result.is_ok()) {
                    auto& assoc = result.value();
                    auto ctx = assoc.accepted_context_id(verification_sop_class_uid);
                    if (ctx) {
                        for (int i = 0; i < 5 && !stop_workers.load(); ++i) {
                            auto echo_rq = make_c_echo_rq(
                                static_cast<uint16_t>(i + 1), verification_sop_class_uid);
                            if (assoc.send_dimse(*ctx, echo_rq).is_ok()) {
                                auto recv = assoc.receive_dimse(std::chrono::milliseconds{2000});
                                if (recv.is_ok()) {
                                    ++operations_completed;
                                }
                            }
                        }
                    }
                    (void)assoc.release(std::chrono::milliseconds{500});
                }
            }
        };

        constexpr int num_workers = 3;
        std::vector<std::future<void>> workers;
        for (int i = 0; i < num_workers; ++i) {
            workers.push_back(std::async(std::launch::async, worker));
        }

        // Let workers run for a bit
        std::this_thread::sleep_for(std::chrono::seconds{2});

        std::cout << "\n=== Shutdown Under Load ===" << std::endl;
        std::cout << "  Workers: " << num_workers << std::endl;
        std::cout << "  Operations before shutdown: " << operations_completed.load() << std::endl;
        std::cout << "  Active associations: " << server->active_associations() << std::endl;

        // Signal workers to stop
        stop_workers.store(true);

        high_resolution_timer timer;
        timer.start();

        // Shutdown while workers are still running
        server->stop();

        timer.stop();

        // Wait for workers to complete
        for (auto& w : workers) {
            w.wait();
        }

        std::cout << "  Operations at shutdown: " << operations_completed.load() << std::endl;
        std::cout << "  Shutdown time: " << timer.elapsed_ms().count() << " ms" << std::endl;

        // Baseline: shutdown under load should complete within 10 seconds
        REQUIRE(timer.elapsed_ms().count() < 10000);

        server.reset();
    }
}

TEST_CASE("Repeated start-stop cycles", "[benchmark][shutdown][cycles]") {
    SECTION("Multiple start-stop cycles") {
        constexpr int cycles = 10;
        benchmark_stats start_stats;
        benchmark_stats stop_stats;

        auto port = find_available_port();

        server_config config;
        config.ae_title = "CYCLE_TEST";
        config.port = port;
        config.max_associations = 50;
        config.implementation_class_uid = "1.2.826.0.1.3680043.9.8888.9";

        for (int i = 0; i < cycles; ++i) {
            auto server = std::make_unique<dicom_server>(config);
            server->register_service(std::make_shared<verification_scp>());

            high_resolution_timer start_timer;
            start_timer.start();

            auto result = server->start();
            REQUIRE(result.is_ok());

            start_timer.stop();
            start_stats.record(static_cast<double>(start_timer.elapsed_us().count()) / 1000.0);

            // Brief operation
            std::this_thread::sleep_for(std::chrono::milliseconds{50});

            high_resolution_timer stop_timer;
            stop_timer.start();

            server->stop();

            stop_timer.stop();
            stop_stats.record(static_cast<double>(stop_timer.elapsed_us().count()) / 1000.0);

            server.reset();

            // Small delay between cycles
            std::this_thread::sleep_for(std::chrono::milliseconds{100});
        }

        std::cout << "\n=== Start-Stop Cycles ===" << std::endl;
        std::cout << "  Cycles: " << cycles << std::endl;
        std::cout << "\n  Start time:" << std::endl;
        std::cout << "    Mean: " << start_stats.mean_ms() << " ms" << std::endl;
        std::cout << "    Std Dev: " << start_stats.stddev_ms() << " ms" << std::endl;
        std::cout << "    Min: " << start_stats.min_ms << " ms" << std::endl;
        std::cout << "    Max: " << start_stats.max_ms << " ms" << std::endl;
        std::cout << "\n  Stop time:" << std::endl;
        std::cout << "    Mean: " << stop_stats.mean_ms() << " ms" << std::endl;
        std::cout << "    Std Dev: " << stop_stats.stddev_ms() << " ms" << std::endl;
        std::cout << "    Min: " << stop_stats.min_ms << " ms" << std::endl;
        std::cout << "    Max: " << stop_stats.max_ms << " ms" << std::endl;

        // Baseline requirements
        REQUIRE(start_stats.mean_ms() < 500.0);
        REQUIRE(stop_stats.mean_ms() < 1000.0);
    }
}

TEST_CASE("Graceful vs forced shutdown", "[benchmark][shutdown][graceful]") {
    SECTION("Compare graceful and abort shutdown") {
        // This test demonstrates the expected behavior for graceful shutdown
        // After thread_system migration, shutdown should be more controlled

        auto port = find_available_port();

        server_config config;
        config.ae_title = "GRACEFUL_TEST";
        config.port = port;
        config.max_associations = 50;
        config.implementation_class_uid = "1.2.826.0.1.3680043.9.8888.10";

        // Test 1: Graceful shutdown (no active connections)
        {
            auto server = std::make_unique<dicom_server>(config);
            server->register_service(std::make_shared<verification_scp>());
            REQUIRE(server->start().is_ok());
            std::this_thread::sleep_for(std::chrono::milliseconds{100});

            high_resolution_timer timer;
            timer.start();
            server->stop();  // Default graceful shutdown
            timer.stop();

            std::cout << "\n=== Graceful Shutdown (no connections) ===" << std::endl;
            std::cout << "  Time: " << timer.elapsed_ms().count() << " ms" << std::endl;

            REQUIRE(timer.elapsed_ms().count() < 2000);
        }

        // Allow port to be released
        std::this_thread::sleep_for(std::chrono::milliseconds{200});
        port = find_available_port();
        config.port = port;

        // Test 2: Shutdown with pending connection
        {
            auto server = std::make_unique<dicom_server>(config);
            server->register_service(std::make_shared<verification_scp>());
            REQUIRE(server->start().is_ok());
            std::this_thread::sleep_for(std::chrono::milliseconds{100});

            // Establish a connection
            auto assoc_config = make_echo_config("PENDING_SCU", "GRACEFUL_TEST");
            auto result = association::connect("localhost", port, assoc_config, default_timeout);
            REQUIRE(result.is_ok());

            auto& assoc = result.value();
            // Don't release - leave it pending

            high_resolution_timer timer;
            timer.start();
            server->stop();
            timer.stop();

            std::cout << "\n=== Shutdown (pending connection) ===" << std::endl;
            std::cout << "  Time: " << timer.elapsed_ms().count() << " ms" << std::endl;

            // Connection should be terminated by server shutdown
            // Client side cleanup
            // Note: assoc might already be disconnected, so ignore result
            // source=0 (service-user), reason=0 (not-specified)
            (void)assoc.abort(0, 0);

            REQUIRE(timer.elapsed_ms().count() < 5000);
        }

        std::cout << "\n  [Note: After thread_system migration, graceful shutdown" << std::endl;
        std::cout << "   should use cancellation_token for cooperative cancellation]" << std::endl;
    }
}
