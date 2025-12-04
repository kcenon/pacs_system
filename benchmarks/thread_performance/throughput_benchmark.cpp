/**
 * @file throughput_benchmark.cpp
 * @brief Message throughput benchmarks
 *
 * Measures the message processing rate of the DICOM server,
 * including C-ECHO and C-STORE operations.
 *
 * Key metrics:
 * - C-ECHO messages per second (single connection)
 * - C-STORE messages per second (single connection)
 * - Sustained throughput over time
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
// C-ECHO Throughput Benchmarks
// =============================================================================

TEST_CASE("C-ECHO throughput baseline", "[benchmark][throughput][echo]") {
    auto port = find_available_port();
    benchmark_server server(port);

    REQUIRE(server.initialize_echo_only());
    REQUIRE(server.start());

    SECTION("Single connection C-ECHO throughput") {
        auto config = make_echo_config("BENCH_ECHO", server.ae_title());
        auto connect_result = association::connect("localhost", port, config, default_timeout);
        REQUIRE(connect_result.is_ok());

        auto& assoc = connect_result.value();
        auto ctx = assoc.accepted_context_id(verification_sop_class_uid);
        REQUIRE(ctx.has_value());

        // Warmup
        for (int i = 0; i < 10; ++i) {
            auto echo_rq = make_c_echo_rq(static_cast<uint16_t>(i + 1), verification_sop_class_uid);
            (void)assoc.send_dimse(*ctx, echo_rq);
            (void)assoc.receive_dimse(default_timeout);
        }

        // Benchmark
        constexpr int iterations = 500;
        benchmark_stats stats;
        size_t success_count = 0;

        high_resolution_timer total_timer;
        total_timer.start();

        for (int i = 0; i < iterations; ++i) {
            high_resolution_timer timer;
            timer.start();

            auto echo_rq = make_c_echo_rq(static_cast<uint16_t>(i + 100), verification_sop_class_uid);
            auto send_result = assoc.send_dimse(*ctx, echo_rq);
            if (send_result.is_err()) continue;

            auto recv_result = assoc.receive_dimse(default_timeout);
            timer.stop();

            if (recv_result.is_ok() && recv_result.value().second.status() == status_success) {
                ++success_count;
                stats.record(static_cast<double>(timer.elapsed_us().count()) / 1000.0);
            }
        }

        total_timer.stop();

        double total_seconds = total_timer.elapsed_seconds();
        double echo_per_second = static_cast<double>(success_count) / total_seconds;

        std::cout << "\n=== C-ECHO Throughput (Single Connection) ===" << std::endl;
        std::cout << "  Iterations: " << iterations << std::endl;
        std::cout << "  Successful: " << success_count << std::endl;
        std::cout << "  Total time: " << total_seconds << " s" << std::endl;
        std::cout << "  Throughput: " << echo_per_second << " echo/s" << std::endl;
        std::cout << "  Mean latency: " << stats.mean_ms() << " ms" << std::endl;
        std::cout << "  Std Dev: " << stats.stddev_ms() << " ms" << std::endl;
        std::cout << "  Min latency: " << stats.min_ms << " ms" << std::endl;
        std::cout << "  Max latency: " << stats.max_ms << " ms" << std::endl;

        (void)assoc.release(std::chrono::milliseconds{2000});

        // Baseline requirements
        REQUIRE(success_count >= iterations * 0.99);  // 99% success rate
        REQUIRE(echo_per_second >= 100.0);  // At least 100 echo/s
    }

    server.stop();
}

TEST_CASE("C-ECHO sustained throughput", "[benchmark][throughput][echo][sustained]") {
    auto port = find_available_port();
    benchmark_server server(port);

    REQUIRE(server.initialize_echo_only());
    REQUIRE(server.start());

    SECTION("10-second sustained C-ECHO") {
        auto config = make_echo_config("BENCH_SUSTAINED", server.ae_title());
        auto connect_result = association::connect("localhost", port, config, default_timeout);
        REQUIRE(connect_result.is_ok());

        auto& assoc = connect_result.value();
        auto ctx = assoc.accepted_context_id(verification_sop_class_uid);
        REQUIRE(ctx.has_value());

        constexpr auto duration = std::chrono::seconds{10};
        size_t total_messages = 0;
        size_t failed_messages = 0;

        auto start = std::chrono::steady_clock::now();
        auto end_time = start + duration;

        while (std::chrono::steady_clock::now() < end_time) {
            auto echo_rq = make_c_echo_rq(
                static_cast<uint16_t>((total_messages % 65535) + 1),
                verification_sop_class_uid);

            auto send_result = assoc.send_dimse(*ctx, echo_rq);
            if (send_result.is_err()) {
                ++failed_messages;
                continue;
            }

            auto recv_result = assoc.receive_dimse(std::chrono::milliseconds{5000});
            if (recv_result.is_ok() && recv_result.value().second.status() == status_success) {
                ++total_messages;
            } else {
                ++failed_messages;
            }
        }

        auto actual_duration = std::chrono::steady_clock::now() - start;
        double seconds = std::chrono::duration<double>(actual_duration).count();
        double throughput = static_cast<double>(total_messages) / seconds;

        std::cout << "\n=== Sustained C-ECHO Throughput (10s) ===" << std::endl;
        std::cout << "  Duration: " << seconds << " s" << std::endl;
        std::cout << "  Total messages: " << total_messages << std::endl;
        std::cout << "  Failed messages: " << failed_messages << std::endl;
        std::cout << "  Throughput: " << throughput << " msg/s" << std::endl;
        std::cout << "  Success rate: "
                  << (100.0 * total_messages / (total_messages + failed_messages)) << "%" << std::endl;

        (void)assoc.release(std::chrono::milliseconds{2000});

        // Baseline: sustained throughput should be at least 80% of burst
        REQUIRE(throughput >= 80.0);
    }

    server.stop();
}

// =============================================================================
// C-STORE Throughput Benchmarks
// =============================================================================

TEST_CASE("C-STORE throughput baseline", "[benchmark][throughput][store]") {
    auto port = find_available_port();
    benchmark_server server(port);

    REQUIRE(server.initialize_with_storage());
    REQUIRE(server.start());

    SECTION("Single connection C-STORE throughput") {
        auto config = make_store_config("BENCH_STORE", server.ae_title());
        auto connect_result = association::connect("localhost", port, config, default_timeout);
        REQUIRE(connect_result.is_ok());

        auto& assoc = connect_result.value();
        storage_scu_config scu_config;
        scu_config.response_timeout = default_timeout;
        storage_scu scu{scu_config};

        // Generate a study UID for all images
        auto study_uid = generate_uid();

        // Warmup
        for (int i = 0; i < 5; ++i) {
            auto dataset = generate_benchmark_dataset(study_uid);
            (void)scu.store(assoc, dataset);
        }

        // Reset server counter
        size_t initial_count = server.store_count();

        // Benchmark
        constexpr int iterations = 100;
        benchmark_stats stats;
        size_t success_count = 0;

        high_resolution_timer total_timer;
        total_timer.start();

        for (int i = 0; i < iterations; ++i) {
            auto dataset = generate_benchmark_dataset(study_uid);

            high_resolution_timer timer;
            timer.start();

            auto result = scu.store(assoc, dataset);

            timer.stop();

            if (result.is_ok() && result.value().is_success()) {
                ++success_count;
                stats.record(static_cast<double>(timer.elapsed_us().count()) / 1000.0);
            }
        }

        total_timer.stop();

        double total_seconds = total_timer.elapsed_seconds();
        double store_per_second = static_cast<double>(success_count) / total_seconds;

        std::cout << "\n=== C-STORE Throughput (Single Connection) ===" << std::endl;
        std::cout << "  Iterations: " << iterations << std::endl;
        std::cout << "  Successful: " << success_count << std::endl;
        std::cout << "  Server received: " << (server.store_count() - initial_count) << std::endl;
        std::cout << "  Total time: " << total_seconds << " s" << std::endl;
        std::cout << "  Throughput: " << store_per_second << " store/s" << std::endl;
        std::cout << "  Mean latency: " << stats.mean_ms() << " ms" << std::endl;
        std::cout << "  Std Dev: " << stats.stddev_ms() << " ms" << std::endl;
        std::cout << "  Min latency: " << stats.min_ms << " ms" << std::endl;
        std::cout << "  Max latency: " << stats.max_ms << " ms" << std::endl;

        // Calculate data throughput (64x64 16-bit = 8KB per image)
        double bytes_per_image = 64 * 64 * 2;  // 8192 bytes
        double mb_per_second = (static_cast<double>(success_count) * bytes_per_image)
                              / (total_seconds * 1024.0 * 1024.0);
        std::cout << "  Data rate: " << mb_per_second << " MB/s" << std::endl;

        (void)assoc.release(std::chrono::milliseconds{2000});

        // Baseline requirements
        REQUIRE(success_count >= iterations * 0.95);  // 95% success rate
        REQUIRE(store_per_second >= 20.0);  // At least 20 store/s for small images
    }

    server.stop();
}

TEST_CASE("C-STORE with varying image sizes", "[benchmark][throughput][store][sizes]") {
    auto port = find_available_port();
    benchmark_server server(port);

    REQUIRE(server.initialize_with_storage());
    REQUIRE(server.start());

    SECTION("Image size impact on throughput") {
        auto config = make_store_config("BENCH_SIZE", server.ae_title());
        auto connect_result = association::connect("localhost", port, config, default_timeout);
        REQUIRE(connect_result.is_ok());

        auto& assoc = connect_result.value();
        storage_scu_config scu_config;
        scu_config.response_timeout = std::chrono::milliseconds{30000};
        storage_scu scu{scu_config};

        // Test different image sizes
        struct size_test {
            int rows;
            int cols;
            const char* name;
        };

        std::vector<size_test> sizes = {
            {64, 64, "64x64 (8KB)"},
            {128, 128, "128x128 (32KB)"},
            {256, 256, "256x256 (128KB)"},
            {512, 512, "512x512 (512KB)"}
        };

        std::cout << "\n=== C-STORE Throughput by Image Size ===" << std::endl;

        for (const auto& size : sizes) {
            constexpr int iterations = 20;
            benchmark_stats stats;
            size_t success_count = 0;

            high_resolution_timer total_timer;
            total_timer.start();

            for (int i = 0; i < iterations; ++i) {
                // Generate dataset with specific size
                auto dataset = generate_benchmark_dataset();

                // Update image dimensions
                dataset.set_numeric<uint16_t>(
                    pacs::core::tags::rows, pacs::encoding::vr_type::US,
                    static_cast<uint16_t>(size.rows));
                dataset.set_numeric<uint16_t>(
                    pacs::core::tags::columns, pacs::encoding::vr_type::US,
                    static_cast<uint16_t>(size.cols));

                // Generate new pixel data
                std::vector<uint16_t> pixel_data(size.rows * size.cols, 512);
                pacs::core::dicom_element pixel_elem(
                    pacs::core::tags::pixel_data, pacs::encoding::vr_type::OW);
                pixel_elem.set_value(std::span<const uint8_t>(
                    reinterpret_cast<const uint8_t*>(pixel_data.data()),
                    pixel_data.size() * sizeof(uint16_t)));

                // Remove old pixel data and add new
                dataset.remove(pacs::core::tags::pixel_data);
                dataset.insert(std::move(pixel_elem));

                // Generate new SOP Instance UID
                dataset.set_string(pacs::core::tags::sop_instance_uid,
                                   pacs::encoding::vr_type::UI, generate_uid());

                high_resolution_timer timer;
                timer.start();

                auto result = scu.store(assoc, dataset);

                timer.stop();

                if (result.is_ok() && result.value().is_success()) {
                    ++success_count;
                    stats.record(static_cast<double>(timer.elapsed_us().count()) / 1000.0);
                }
            }

            total_timer.stop();

            double total_seconds = total_timer.elapsed_seconds();
            double store_per_second = static_cast<double>(success_count) / total_seconds;
            double bytes_per_image = size.rows * size.cols * 2;
            double mb_per_second = (static_cast<double>(success_count) * bytes_per_image)
                                  / (total_seconds * 1024.0 * 1024.0);

            std::cout << "\n  " << size.name << ":" << std::endl;
            std::cout << "    Success: " << success_count << "/" << iterations << std::endl;
            std::cout << "    Throughput: " << store_per_second << " store/s" << std::endl;
            std::cout << "    Data rate: " << mb_per_second << " MB/s" << std::endl;
            std::cout << "    Mean latency: " << stats.mean_ms() << " ms" << std::endl;
        }

        (void)assoc.release(std::chrono::milliseconds{2000});
    }

    server.stop();
}

// =============================================================================
// Catch2 BENCHMARK macros
// =============================================================================

TEST_CASE("Throughput micro-benchmarks", "[.benchmark][throughput][micro]") {
    auto port = find_available_port();
    benchmark_server server(port);

    REQUIRE(server.initialize_echo_only());
    REQUIRE(server.start());

    auto config = make_echo_config("BENCH_MICRO", server.ae_title());
    auto connect_result = association::connect("localhost", port, config, default_timeout);
    REQUIRE(connect_result.is_ok());

    auto& assoc = connect_result.value();
    auto ctx = assoc.accepted_context_id(verification_sop_class_uid);
    REQUIRE(ctx.has_value());

    static uint16_t msg_id = 0;

    BENCHMARK("Single C-ECHO round-trip") {
        auto echo_rq = make_c_echo_rq(++msg_id, verification_sop_class_uid);
        (void)assoc.send_dimse(*ctx, echo_rq);
        auto result = assoc.receive_dimse(default_timeout);
        return result.is_ok();
    };

    (void)assoc.release(std::chrono::milliseconds{2000});
    server.stop();
}
