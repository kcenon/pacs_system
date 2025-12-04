/**
 * @file association_benchmark.cpp
 * @brief Association establishment latency benchmarks
 *
 * Measures the time required to establish DICOM associations,
 * including TCP connection, A-ASSOCIATE negotiation, and release.
 *
 * Key metrics:
 * - Single association establishment latency
 * - Association with release latency (full round-trip)
 * - Repeated association establishment (warm-up effects)
 *
 * @see Issue #154 - Establish performance baseline benchmarks for thread migration
 */

#include "benchmark_common.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

#include <iostream>
#include <numeric>
#include <vector>

using namespace pacs::benchmark;
using namespace pacs::network;
using namespace pacs::network::dimse;
using namespace pacs::services;

// =============================================================================
// Association Establishment Benchmarks
// =============================================================================

TEST_CASE("Association establishment latency baseline", "[benchmark][association]") {
    auto port = find_available_port();
    benchmark_server server(port);

    REQUIRE(server.initialize_echo_only());
    REQUIRE(server.start());

    SECTION("Single association establishment") {
        // Warmup
        for (int i = 0; i < 3; ++i) {
            auto config = make_echo_config("BENCH_SCU", server.ae_title());
            auto result = association::connect("localhost", port, config, default_timeout);
            if (result.is_ok()) {
                (void)result.value().release(std::chrono::milliseconds{1000});
            }
        }

        // Benchmark
        constexpr int iterations = 50;
        benchmark_stats stats;

        for (int i = 0; i < iterations; ++i) {
            auto config = make_echo_config("BENCH_SCU_" + std::to_string(i), server.ae_title());

            high_resolution_timer timer;
            timer.start();

            auto result = association::connect("localhost", port, config, default_timeout);

            timer.stop();

            REQUIRE(result.is_ok());
            stats.record(static_cast<double>(timer.elapsed_us().count()) / 1000.0);

            (void)result.value().release(std::chrono::milliseconds{1000});
        }

        // Output results
        std::cout << "\n=== Association Establishment Latency ===" << std::endl;
        std::cout << "  Iterations: " << stats.count << std::endl;
        std::cout << "  Mean: " << stats.mean_ms() << " ms" << std::endl;
        std::cout << "  Std Dev: " << stats.stddev_ms() << " ms" << std::endl;
        std::cout << "  Min: " << stats.min_ms << " ms" << std::endl;
        std::cout << "  Max: " << stats.max_ms << " ms" << std::endl;
        std::cout << "  Rate: " << stats.throughput_per_second() << " assoc/s" << std::endl;

        // Record baseline requirement: < 100ms mean
        REQUIRE(stats.mean_ms() < 100.0);
    }

    SECTION("Full association round-trip (connect + release)") {
        constexpr int iterations = 30;
        benchmark_stats stats;

        for (int i = 0; i < iterations; ++i) {
            auto config = make_echo_config("BENCH_RT_" + std::to_string(i), server.ae_title());

            high_resolution_timer timer;
            timer.start();

            auto result = association::connect("localhost", port, config, default_timeout);
            REQUIRE(result.is_ok());

            auto release_result = result.value().release(std::chrono::milliseconds{2000});
            REQUIRE(release_result.is_ok());

            timer.stop();
            stats.record(static_cast<double>(timer.elapsed_us().count()) / 1000.0);
        }

        std::cout << "\n=== Full Association Round-Trip ===" << std::endl;
        std::cout << "  Iterations: " << stats.count << std::endl;
        std::cout << "  Mean: " << stats.mean_ms() << " ms" << std::endl;
        std::cout << "  Std Dev: " << stats.stddev_ms() << " ms" << std::endl;
        std::cout << "  Min: " << stats.min_ms << " ms" << std::endl;
        std::cout << "  Max: " << stats.max_ms << " ms" << std::endl;

        // Record baseline requirement: < 200ms mean for full round-trip
        REQUIRE(stats.mean_ms() < 200.0);
    }

    server.stop();
}

TEST_CASE("Association establishment with C-ECHO", "[benchmark][association][echo]") {
    auto port = find_available_port();
    benchmark_server server(port);

    REQUIRE(server.initialize_echo_only());
    REQUIRE(server.start());

    SECTION("Connect + Single C-ECHO + Release") {
        constexpr int iterations = 30;
        benchmark_stats connect_stats;
        benchmark_stats echo_stats;
        benchmark_stats release_stats;
        benchmark_stats total_stats;

        for (int i = 0; i < iterations; ++i) {
            auto config = make_echo_config("BENCH_ECHO_" + std::to_string(i), server.ae_title());

            high_resolution_timer total_timer;
            high_resolution_timer step_timer;

            total_timer.start();

            // Connect
            step_timer.start();
            auto connect_result = association::connect("localhost", port, config, default_timeout);
            step_timer.stop();
            REQUIRE(connect_result.is_ok());
            connect_stats.record(static_cast<double>(step_timer.elapsed_us().count()) / 1000.0);

            auto& assoc = connect_result.value();

            // C-ECHO
            step_timer.start();
            auto ctx = assoc.accepted_context_id(verification_sop_class_uid);
            REQUIRE(ctx.has_value());

            auto echo_rq = make_c_echo_rq(1, verification_sop_class_uid);
            auto send_result = assoc.send_dimse(*ctx, echo_rq);
            REQUIRE(send_result.is_ok());

            auto recv_result = assoc.receive_dimse(default_timeout);
            step_timer.stop();
            REQUIRE(recv_result.is_ok());
            REQUIRE(recv_result.value().second.status() == status_success);
            echo_stats.record(static_cast<double>(step_timer.elapsed_us().count()) / 1000.0);

            // Release
            step_timer.start();
            auto release_result = assoc.release(std::chrono::milliseconds{2000});
            step_timer.stop();
            REQUIRE(release_result.is_ok());
            release_stats.record(static_cast<double>(step_timer.elapsed_us().count()) / 1000.0);

            total_timer.stop();
            total_stats.record(static_cast<double>(total_timer.elapsed_us().count()) / 1000.0);
        }

        std::cout << "\n=== Association + C-ECHO + Release ===" << std::endl;
        std::cout << "  Iterations: " << iterations << std::endl;
        std::cout << "\n  Connect:" << std::endl;
        std::cout << "    Mean: " << connect_stats.mean_ms() << " ms" << std::endl;
        std::cout << "    Std Dev: " << connect_stats.stddev_ms() << " ms" << std::endl;
        std::cout << "\n  C-ECHO:" << std::endl;
        std::cout << "    Mean: " << echo_stats.mean_ms() << " ms" << std::endl;
        std::cout << "    Std Dev: " << echo_stats.stddev_ms() << " ms" << std::endl;
        std::cout << "\n  Release:" << std::endl;
        std::cout << "    Mean: " << release_stats.mean_ms() << " ms" << std::endl;
        std::cout << "    Std Dev: " << release_stats.stddev_ms() << " ms" << std::endl;
        std::cout << "\n  Total:" << std::endl;
        std::cout << "    Mean: " << total_stats.mean_ms() << " ms" << std::endl;
        std::cout << "    Min: " << total_stats.min_ms << " ms" << std::endl;
        std::cout << "    Max: " << total_stats.max_ms << " ms" << std::endl;

        // Baseline requirements
        REQUIRE(connect_stats.mean_ms() < 100.0);
        REQUIRE(echo_stats.mean_ms() < 50.0);
        REQUIRE(release_stats.mean_ms() < 100.0);
    }

    server.stop();
}

TEST_CASE("Sequential association establishment", "[benchmark][association][sequential]") {
    auto port = find_available_port();
    benchmark_server server(port);

    REQUIRE(server.initialize_echo_only());
    REQUIRE(server.start());

    SECTION("Rapid sequential connections") {
        constexpr int iterations = 100;
        benchmark_stats stats;

        high_resolution_timer total_timer;
        total_timer.start();

        for (int i = 0; i < iterations; ++i) {
            auto config = make_echo_config("BENCH_SEQ_" + std::to_string(i), server.ae_title());

            high_resolution_timer timer;
            timer.start();

            auto result = association::connect("localhost", port, config, default_timeout);
            timer.stop();

            if (result.is_ok()) {
                stats.record(static_cast<double>(timer.elapsed_us().count()) / 1000.0);
                (void)result.value().release(std::chrono::milliseconds{500});
            }
        }

        total_timer.stop();

        double total_seconds = total_timer.elapsed_seconds();
        double connections_per_second = static_cast<double>(stats.count) / total_seconds;

        std::cout << "\n=== Sequential Connection Rate ===" << std::endl;
        std::cout << "  Successful connections: " << stats.count << "/" << iterations << std::endl;
        std::cout << "  Total time: " << total_seconds << " s" << std::endl;
        std::cout << "  Rate: " << connections_per_second << " conn/s" << std::endl;
        std::cout << "  Mean latency: " << stats.mean_ms() << " ms" << std::endl;
        std::cout << "  Min latency: " << stats.min_ms << " ms" << std::endl;
        std::cout << "  Max latency: " << stats.max_ms << " ms" << std::endl;

        // Baseline: at least 90% success rate
        REQUIRE(stats.count >= iterations * 0.9);
        // Baseline: at least 10 connections per second
        REQUIRE(connections_per_second >= 10.0);
    }

    server.stop();
}

// =============================================================================
// Catch2 BENCHMARK macros for micro-benchmarks
// =============================================================================

TEST_CASE("Association micro-benchmarks", "[.benchmark][association][micro]") {
    auto port = find_available_port();
    benchmark_server server(port);

    REQUIRE(server.initialize_echo_only());
    REQUIRE(server.start());

    BENCHMARK("Association connect only") {
        auto config = make_echo_config("BENCH_MICRO", server.ae_title());
        auto result = association::connect("localhost", port, config, default_timeout);
        if (result.is_ok()) {
            // source=0 (service-user), reason=0 (not-specified)
            (void)result.value().abort(0, 0);
        }
        return result.is_ok();
    };

    BENCHMARK("Association connect + release") {
        auto config = make_echo_config("BENCH_MICRO", server.ae_title());
        auto result = association::connect("localhost", port, config, default_timeout);
        if (result.is_ok()) {
            (void)result.value().release(std::chrono::milliseconds{1000});
        }
        return result.is_ok();
    };

    server.stop();
}
