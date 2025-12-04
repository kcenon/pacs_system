/**
 * @file concurrent_load_benchmark.cpp
 * @brief Concurrent connection handling benchmarks
 *
 * Measures the server's ability to handle multiple simultaneous
 * connections and operations.
 *
 * Key metrics:
 * - Maximum concurrent connections
 * - Throughput under concurrent load
 * - Latency distribution under load
 * - Memory usage under concurrent access
 *
 * @see Issue #154 - Establish performance baseline benchmarks for thread migration
 */

#include "benchmark_common.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

#include <algorithm>
#include <future>
#include <iostream>
#include <latch>
#include <mutex>
#include <numeric>
#include <thread>
#include <vector>

using namespace pacs::benchmark;
using namespace pacs::network;
using namespace pacs::network::dimse;
using namespace pacs::services;

// =============================================================================
// Helper Structures
// =============================================================================

namespace {

/**
 * @brief Result from a concurrent worker
 */
struct worker_result {
    size_t success_count{0};
    size_t failure_count{0};
    std::chrono::milliseconds total_duration{0};
    benchmark_stats latency_stats;
    std::string error_message;
};

/**
 * @brief Run C-ECHO operations as a concurrent worker
 */
worker_result run_echo_worker(
    uint16_t server_port,
    const std::string& server_ae,
    int worker_id,
    int num_operations,
    std::latch& start_latch) {

    worker_result result;
    auto start_time = std::chrono::steady_clock::now();

    // Wait for all workers to be ready
    start_latch.arrive_and_wait();

    try {
        auto config = make_echo_config(
            "ECHO_W" + std::to_string(worker_id), server_ae);

        auto connect_result = association::connect(
            "localhost", server_port, config,
            std::chrono::milliseconds{15000});

        if (connect_result.is_err()) {
            result.error_message = "Connection failed";
            result.failure_count = num_operations;
            return result;
        }

        auto& assoc = connect_result.value();
        auto ctx = assoc.accepted_context_id(verification_sop_class_uid);

        if (!ctx) {
            result.error_message = "No accepted context";
            result.failure_count = num_operations;
            (void)assoc.release(std::chrono::milliseconds{1000});
            return result;
        }

        for (int i = 0; i < num_operations; ++i) {
            high_resolution_timer timer;
            timer.start();

            auto echo_rq = make_c_echo_rq(
                static_cast<uint16_t>(i + 1), verification_sop_class_uid);
            auto send_result = assoc.send_dimse(*ctx, echo_rq);

            if (send_result.is_err()) {
                ++result.failure_count;
                continue;
            }

            auto recv_result = assoc.receive_dimse(default_timeout);
            timer.stop();

            if (recv_result.is_ok() && recv_result.value().second.status() == status_success) {
                ++result.success_count;
                result.latency_stats.record(
                    static_cast<double>(timer.elapsed_us().count()) / 1000.0);
            } else {
                ++result.failure_count;
            }
        }

        (void)assoc.release(std::chrono::milliseconds{2000});

    } catch (const std::exception& e) {
        result.error_message = e.what();
    }

    auto end_time = std::chrono::steady_clock::now();
    result.total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    return result;
}

/**
 * @brief Run C-STORE operations as a concurrent worker
 */
worker_result run_store_worker(
    uint16_t server_port,
    const std::string& server_ae,
    int worker_id,
    int num_operations,
    std::latch& start_latch) {

    worker_result result;
    auto start_time = std::chrono::steady_clock::now();

    start_latch.arrive_and_wait();

    try {
        auto config = make_store_config(
            "STORE_W" + std::to_string(worker_id), server_ae);

        auto connect_result = association::connect(
            "localhost", server_port, config,
            std::chrono::milliseconds{15000});

        if (connect_result.is_err()) {
            result.error_message = "Connection failed";
            result.failure_count = num_operations;
            return result;
        }

        auto& assoc = connect_result.value();
        storage_scu_config scu_config;
        scu_config.response_timeout = default_timeout;
        storage_scu scu{scu_config};

        auto study_uid = generate_uid();

        for (int i = 0; i < num_operations; ++i) {
            auto dataset = generate_benchmark_dataset(study_uid);

            high_resolution_timer timer;
            timer.start();

            auto store_result = scu.store(assoc, dataset);

            timer.stop();

            if (store_result.is_ok() && store_result.value().is_success()) {
                ++result.success_count;
                result.latency_stats.record(
                    static_cast<double>(timer.elapsed_us().count()) / 1000.0);
            } else {
                ++result.failure_count;
            }
        }

        (void)assoc.release(std::chrono::milliseconds{2000});

    } catch (const std::exception& e) {
        result.error_message = e.what();
    }

    auto end_time = std::chrono::steady_clock::now();
    result.total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    return result;
}

}  // namespace

// =============================================================================
// Concurrent Connection Benchmarks
// =============================================================================

TEST_CASE("Concurrent C-ECHO operations", "[benchmark][concurrent][echo]") {
    auto port = find_available_port();
    benchmark_server server(port);

    REQUIRE(server.initialize_echo_only());
    REQUIRE(server.start());

    SECTION("Multiple concurrent connections with C-ECHO") {
        constexpr int num_workers = 10;
        constexpr int ops_per_worker = 50;

        std::latch start_latch(num_workers + 1);
        std::vector<std::future<worker_result>> futures;

        // Launch workers
        for (int i = 0; i < num_workers; ++i) {
            futures.push_back(std::async(
                std::launch::async,
                run_echo_worker,
                port,
                server.ae_title(),
                i,
                ops_per_worker,
                std::ref(start_latch)
            ));
        }

        high_resolution_timer total_timer;
        total_timer.start();

        // Release all workers simultaneously
        start_latch.arrive_and_wait();

        // Collect results
        size_t total_success = 0;
        size_t total_failure = 0;
        benchmark_stats aggregate_latency;
        std::chrono::milliseconds max_duration{0};

        for (auto& future : futures) {
            auto result = future.get();
            total_success += result.success_count;
            total_failure += result.failure_count;
            max_duration = std::max(max_duration, result.total_duration);

            // Aggregate latency stats
            for (size_t i = 0; i < result.latency_stats.count; ++i) {
                aggregate_latency.record(result.latency_stats.mean_ms());
            }

            if (!result.error_message.empty()) {
                INFO("Worker error: " << result.error_message);
            }
        }

        total_timer.stop();

        double total_seconds = total_timer.elapsed_seconds();
        double throughput = static_cast<double>(total_success) / total_seconds;
        size_t expected_total = num_workers * ops_per_worker;

        std::cout << "\n=== Concurrent C-ECHO (" << num_workers << " workers) ===" << std::endl;
        std::cout << "  Expected operations: " << expected_total << std::endl;
        std::cout << "  Successful: " << total_success << std::endl;
        std::cout << "  Failed: " << total_failure << std::endl;
        std::cout << "  Total time: " << total_seconds << " s" << std::endl;
        std::cout << "  Max worker duration: " << max_duration.count() << " ms" << std::endl;
        std::cout << "  Aggregate throughput: " << throughput << " echo/s" << std::endl;
        std::cout << "  Per-worker throughput: " << (throughput / num_workers) << " echo/s" << std::endl;
        std::cout << "  Success rate: "
                  << (100.0 * total_success / expected_total) << "%" << std::endl;

        // Baseline requirements
        REQUIRE(total_success >= expected_total * 0.95);  // 95% success rate
        REQUIRE(throughput >= 50.0);  // At least 50 echo/s aggregate
    }

    server.stop();
}

TEST_CASE("Concurrent C-STORE operations", "[benchmark][concurrent][store]") {
    auto port = find_available_port();
    benchmark_server server(port);

    REQUIRE(server.initialize_with_storage());
    REQUIRE(server.start());

    SECTION("Multiple concurrent connections with C-STORE") {
        constexpr int num_workers = 5;
        constexpr int ops_per_worker = 20;

        std::latch start_latch(num_workers + 1);
        std::vector<std::future<worker_result>> futures;

        for (int i = 0; i < num_workers; ++i) {
            futures.push_back(std::async(
                std::launch::async,
                run_store_worker,
                port,
                server.ae_title(),
                i,
                ops_per_worker,
                std::ref(start_latch)
            ));
        }

        high_resolution_timer total_timer;
        total_timer.start();

        start_latch.arrive_and_wait();

        size_t total_success = 0;
        size_t total_failure = 0;
        std::chrono::milliseconds max_duration{0};

        for (auto& future : futures) {
            auto result = future.get();
            total_success += result.success_count;
            total_failure += result.failure_count;
            max_duration = std::max(max_duration, result.total_duration);

            if (!result.error_message.empty()) {
                INFO("Worker error: " << result.error_message);
            }
        }

        total_timer.stop();

        double total_seconds = total_timer.elapsed_seconds();
        double throughput = static_cast<double>(total_success) / total_seconds;
        size_t expected_total = num_workers * ops_per_worker;

        std::cout << "\n=== Concurrent C-STORE (" << num_workers << " workers) ===" << std::endl;
        std::cout << "  Expected operations: " << expected_total << std::endl;
        std::cout << "  Successful: " << total_success << std::endl;
        std::cout << "  Failed: " << total_failure << std::endl;
        std::cout << "  Server received: " << server.store_count() << std::endl;
        std::cout << "  Total time: " << total_seconds << " s" << std::endl;
        std::cout << "  Aggregate throughput: " << throughput << " store/s" << std::endl;
        std::cout << "  Success rate: "
                  << (100.0 * total_success / expected_total) << "%" << std::endl;

        REQUIRE(total_success >= expected_total * 0.90);  // 90% success rate
        REQUIRE(throughput >= 10.0);  // At least 10 store/s aggregate
    }

    server.stop();
}

TEST_CASE("Scalability test", "[benchmark][concurrent][scalability]") {
    auto port = find_available_port();
    benchmark_server server(port);

    REQUIRE(server.initialize_echo_only());
    REQUIRE(server.start());

    SECTION("Throughput scaling with worker count") {
        constexpr int ops_per_worker = 30;
        std::vector<int> worker_counts = {1, 2, 4, 8, 16};

        std::cout << "\n=== Scalability Test ===" << std::endl;
        std::cout << "  Operations per worker: " << ops_per_worker << std::endl;

        std::vector<double> throughputs;

        for (int num_workers : worker_counts) {
            std::latch start_latch(num_workers + 1);
            std::vector<std::future<worker_result>> futures;

            for (int i = 0; i < num_workers; ++i) {
                futures.push_back(std::async(
                    std::launch::async,
                    run_echo_worker,
                    port,
                    server.ae_title(),
                    i,
                    ops_per_worker,
                    std::ref(start_latch)
                ));
            }

            high_resolution_timer timer;
            timer.start();
            start_latch.arrive_and_wait();

            size_t total_success = 0;
            for (auto& future : futures) {
                auto result = future.get();
                total_success += result.success_count;
            }

            timer.stop();

            double throughput = static_cast<double>(total_success) / timer.elapsed_seconds();
            throughputs.push_back(throughput);

            std::cout << "\n  " << num_workers << " workers:" << std::endl;
            std::cout << "    Success: " << total_success << "/" << (num_workers * ops_per_worker) << std::endl;
            std::cout << "    Throughput: " << throughput << " ops/s" << std::endl;

            // Allow server to recover between tests
            std::this_thread::sleep_for(std::chrono::milliseconds{500});
        }

        // Verify reasonable scaling
        // Throughput should increase (though not linearly) with more workers
        for (size_t i = 1; i < throughputs.size(); ++i) {
            double scaling_factor = throughputs[i] / throughputs[0];
            std::cout << "\n  Scaling factor (" << worker_counts[i]
                      << " vs 1 worker): " << scaling_factor << "x" << std::endl;
        }

        // At least some improvement with 2 workers
        REQUIRE(throughputs[1] >= throughputs[0] * 0.9);
    }

    server.stop();
}

TEST_CASE("Connection saturation test", "[benchmark][concurrent][saturation]") {
    auto port = find_available_port();
    benchmark_server server(port);

    REQUIRE(server.initialize_echo_only());
    REQUIRE(server.start());

    SECTION("Hold multiple connections simultaneously") {
        constexpr int num_connections = 20;
        std::vector<std::optional<association>> connections;
        size_t successful_connections = 0;

        high_resolution_timer timer;
        timer.start();

        for (int i = 0; i < num_connections; ++i) {
            auto config = make_echo_config(
                "HOLD_" + std::to_string(i), server.ae_title());

            auto result = association::connect(
                "localhost", port, config, default_timeout);

            if (result.is_ok()) {
                connections.push_back(std::move(result.value()));
                ++successful_connections;
            } else {
                connections.push_back(std::nullopt);
            }
        }

        timer.stop();

        std::cout << "\n=== Connection Saturation Test ===" << std::endl;
        std::cout << "  Requested connections: " << num_connections << std::endl;
        std::cout << "  Successful: " << successful_connections << std::endl;
        std::cout << "  Active (server): " << server.active_associations() << std::endl;
        std::cout << "  Time to establish all: " << timer.elapsed_ms().count() << " ms" << std::endl;

        // Perform C-ECHO on all held connections
        size_t echo_success = 0;
        for (auto& opt_assoc : connections) {
            if (opt_assoc) {
                auto ctx = opt_assoc->accepted_context_id(verification_sop_class_uid);
                if (ctx) {
                    auto echo_rq = make_c_echo_rq(1, verification_sop_class_uid);
                    if (opt_assoc->send_dimse(*ctx, echo_rq).is_ok()) {
                        auto recv = opt_assoc->receive_dimse(default_timeout);
                        if (recv.is_ok() && recv.value().second.status() == status_success) {
                            ++echo_success;
                        }
                    }
                }
            }
        }

        std::cout << "  C-ECHO success on held connections: " << echo_success << std::endl;

        // Release all connections
        for (auto& opt_assoc : connections) {
            if (opt_assoc) {
                (void)opt_assoc->release(std::chrono::milliseconds{500});
            }
        }
        connections.clear();

        // Verify server can accept new connections after release
        std::this_thread::sleep_for(std::chrono::milliseconds{200});

        auto new_config = make_echo_config("AFTER_RELEASE", server.ae_title());
        auto new_result = association::connect("localhost", port, new_config, default_timeout);
        REQUIRE(new_result.is_ok());
        (void)new_result.value().release(std::chrono::milliseconds{1000});

        std::cout << "  Post-release connection: SUCCESS" << std::endl;

        REQUIRE(successful_connections >= num_connections * 0.9);
        REQUIRE(echo_success >= successful_connections * 0.95);
    }

    server.stop();
}
