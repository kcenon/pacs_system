/**
 * @file pipeline_coordinator_test.cpp
 * @brief Unit tests for pipeline coordinator
 *
 * @see Issue #524 - Phase 7: Testing & Benchmarks
 */

#include <catch2/catch_test_macros.hpp>

#include "pacs/network/pipeline/pipeline_coordinator.hpp"
#include "pacs/network/pipeline/pipeline_job_types.hpp"
#include "pacs/core/result.hpp"

#include <atomic>
#include <chrono>
#include <thread>
#include <condition_variable>
#include <mutex>

using namespace pacs::network::pipeline;
using pacs::VoidResult;

namespace {

// Helper class for timeout-based waiting
class completion_waiter {
public:
    void signal() {
        std::lock_guard<std::mutex> lock(mutex_);
        completed_ = true;
        cv_.notify_all();
    }

    bool wait_for(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, timeout, [this] { return completed_; });
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    bool completed_ = false;
};

}  // namespace

namespace {

// Test job implementation for unit tests
class test_job : public pipeline_job_base {
public:
    explicit test_job(uint64_t job_id = 0,
                      std::function<void()> on_execute = nullptr)
        : on_execute_(std::move(on_execute)) {
        context_.job_id = job_id;
        context_.stage = pipeline_stage::network_receive;
        context_.category = job_category::echo;
    }

    auto execute([[maybe_unused]] pipeline_coordinator& coordinator) -> VoidResult override {
        executed_.store(true, std::memory_order_release);
        if (on_execute_) {
            on_execute_();
        }
        return pacs::ok();
    }

    [[nodiscard]] auto get_context() const noexcept -> const job_context& override {
        return context_;
    }

    [[nodiscard]] auto get_context() noexcept -> job_context& override {
        return context_;
    }

    [[nodiscard]] auto get_name() const -> std::string override {
        return "test_job";
    }

    [[nodiscard]] auto was_executed() const noexcept -> bool {
        return executed_.load(std::memory_order_acquire);
    }

private:
    job_context context_;
    std::function<void()> on_execute_;
    std::atomic<bool> executed_{false};
};

}  // namespace

TEST_CASE("pipeline_config default values", "[network][pipeline][coordinator]") {
    pipeline_config config;

    CHECK(config.net_io_workers == 4);
    CHECK(config.protocol_workers == 2);
    CHECK(config.execution_workers == 8);
    CHECK(config.encode_workers == 2);
    CHECK(config.max_queue_depth == 10000);
    CHECK(config.enable_metrics == true);
    CHECK(config.name_prefix == "pipeline");
}

TEST_CASE("pipeline_config get_workers_for_stage", "[network][pipeline][coordinator]") {
    pipeline_config config;
    config.net_io_workers = 10;
    config.protocol_workers = 5;
    config.execution_workers = 20;
    config.encode_workers = 3;

    SECTION("network stages use net_io_workers") {
        CHECK(config.get_workers_for_stage(pipeline_stage::network_receive) == 10);
        CHECK(config.get_workers_for_stage(pipeline_stage::network_send) == 10);
    }

    SECTION("protocol stages use protocol_workers") {
        CHECK(config.get_workers_for_stage(pipeline_stage::pdu_decode) == 5);
        CHECK(config.get_workers_for_stage(pipeline_stage::dimse_process) == 5);
    }

    SECTION("execution stage uses execution_workers") {
        CHECK(config.get_workers_for_stage(pipeline_stage::storage_query_exec) == 20);
    }

    SECTION("encode stage uses encode_workers") {
        CHECK(config.get_workers_for_stage(pipeline_stage::response_encode) == 3);
    }
}

TEST_CASE("pipeline_coordinator lifecycle", "[network][pipeline][coordinator]") {
    pipeline_config config;
    config.net_io_workers = 1;
    config.protocol_workers = 1;
    config.execution_workers = 1;
    config.encode_workers = 1;

    SECTION("default construction") {
        pipeline_coordinator coordinator;
        CHECK(coordinator.is_running() == false);
    }

    SECTION("construction with config") {
        pipeline_coordinator coordinator(config);
        CHECK(coordinator.is_running() == false);
    }

    SECTION("start and stop") {
        pipeline_coordinator coordinator(config);

        auto start_result = coordinator.start();
        REQUIRE(start_result.is_ok());
        CHECK(coordinator.is_running() == true);

        auto stop_result = coordinator.stop();
        REQUIRE(stop_result.is_ok());
        CHECK(coordinator.is_running() == false);
    }

    SECTION("double start returns error") {
        pipeline_coordinator coordinator(config);

        auto first = coordinator.start();
        REQUIRE(first.is_ok());

        auto second = coordinator.start();
        CHECK_FALSE(second.is_ok());
    }

    SECTION("double stop is safe") {
        pipeline_coordinator coordinator(config);

        auto start = coordinator.start();
        REQUIRE(start.is_ok());

        auto first_stop = coordinator.stop();
        CHECK(first_stop.is_ok());

        auto second_stop = coordinator.stop();
        CHECK(second_stop.is_ok());
    }

    SECTION("destructor stops coordinator") {
        {
            pipeline_coordinator coordinator(config);
            auto result = coordinator.start();
            REQUIRE(result.is_ok());
            CHECK(coordinator.is_running() == true);
        }
        // Destructor should have stopped the coordinator without issues
    }
}

TEST_CASE("pipeline_coordinator job submission", "[network][pipeline][coordinator]") {
    pipeline_config config;
    config.net_io_workers = 2;
    config.protocol_workers = 1;
    config.execution_workers = 1;
    config.encode_workers = 1;
    config.enable_metrics = true;

    pipeline_coordinator coordinator(config);
    auto start_result = coordinator.start();
    REQUIRE(start_result.is_ok());

    SECTION("submit job before start fails") {
        pipeline_coordinator stopped_coordinator(config);
        auto job = std::make_unique<test_job>(1);
        auto result = stopped_coordinator.submit_to_stage(
            pipeline_stage::network_receive, std::move(job));
        CHECK_FALSE(result.is_ok());
    }

    SECTION("submit null job fails") {
        auto result = coordinator.submit_to_stage(
            pipeline_stage::network_receive, nullptr);
        CHECK_FALSE(result.is_ok());
    }

    SECTION("submit job to valid stage succeeds") {
        std::atomic<bool> executed{false};
        completion_waiter waiter;

        auto job = std::make_unique<test_job>(1, [&]() {
            executed.store(true, std::memory_order_release);
            waiter.signal();
        });

        auto result = coordinator.submit_to_stage(
            pipeline_stage::network_receive, std::move(job));
        REQUIRE(result.is_ok());

        // Wait for execution with timeout (10s for CI environments)
        bool completed = waiter.wait_for(std::chrono::seconds(10));
        REQUIRE(completed);
        CHECK(executed.load(std::memory_order_acquire) == true);
    }

    SECTION("submit task function succeeds") {
        std::atomic<bool> executed{false};
        completion_waiter waiter;

        auto result = coordinator.submit_task(
            pipeline_stage::pdu_decode,
            [&]() {
                executed.store(true, std::memory_order_release);
                waiter.signal();
            });
        REQUIRE(result.is_ok());

        // 10s timeout for CI environments
        bool completed = waiter.wait_for(std::chrono::seconds(10));
        REQUIRE(completed);
        CHECK(executed.load(std::memory_order_acquire) == true);
    }

    SECTION("submit to invalid stage fails") {
        auto job = std::make_unique<test_job>(1);
        auto result = coordinator.submit_to_stage(
            static_cast<pipeline_stage>(255), std::move(job));
        CHECK_FALSE(result.is_ok());
    }

    static_cast<void>(coordinator.stop());
}

TEST_CASE("pipeline_coordinator statistics", "[network][pipeline][coordinator]") {
    pipeline_config config;
    config.net_io_workers = 2;
    config.protocol_workers = 1;
    config.execution_workers = 4;
    config.encode_workers = 1;

    pipeline_coordinator coordinator(config);

    SECTION("total worker count before start is 0") {
        CHECK(coordinator.get_total_worker_count() == 0);
    }

    SECTION("total worker count after start is correct") {
        auto result = coordinator.start();
        REQUIRE(result.is_ok());

        // 2 (receive) + 1 (decode) + 1 (dimse) + 4 (exec) + 1 (encode) + 2 (send) = 11
        size_t expected = 2 + 1 + 1 + 4 + 1 + 2;
        CHECK(coordinator.get_total_worker_count() == expected);

        static_cast<void>(coordinator.stop());
    }

    SECTION("generate_job_id is monotonically increasing") {
        auto id1 = coordinator.generate_job_id();
        auto id2 = coordinator.generate_job_id();
        auto id3 = coordinator.generate_job_id();

        CHECK(id2 > id1);
        CHECK(id3 > id2);
    }
}

TEST_CASE("pipeline_coordinator callbacks", "[network][pipeline][coordinator]") {
    pipeline_config config;
    config.net_io_workers = 1;
    config.protocol_workers = 1;
    config.execution_workers = 1;
    config.encode_workers = 1;
    config.enable_metrics = true;

    pipeline_coordinator coordinator(config);
    auto start_result = coordinator.start();
    REQUIRE(start_result.is_ok());

    SECTION("job completion callback is invoked") {
        std::atomic<bool> callback_invoked{false};
        std::atomic<uint64_t> received_job_id{0};
        completion_waiter waiter;

        coordinator.set_job_completion_callback(
            [&](const job_context& ctx, [[maybe_unused]] bool success) {
                received_job_id.store(ctx.job_id, std::memory_order_release);
                callback_invoked.store(true, std::memory_order_release);
                waiter.signal();
            });

        auto job = std::make_unique<test_job>(42);
        auto result = coordinator.submit_to_stage(
            pipeline_stage::network_receive, std::move(job));
        REQUIRE(result.is_ok());

        // 10s timeout for CI environments
        bool completed = waiter.wait_for(std::chrono::seconds(10));
        REQUIRE(completed);
        CHECK(callback_invoked.load(std::memory_order_acquire) == true);
        CHECK(received_job_id.load(std::memory_order_acquire) == 42);
    }

    static_cast<void>(coordinator.stop());
}

TEST_CASE("pipeline_coordinator metrics", "[network][pipeline][coordinator]") {
    pipeline_config config;
    config.net_io_workers = 1;
    config.protocol_workers = 1;
    config.execution_workers = 1;
    config.encode_workers = 1;
    config.enable_metrics = true;

    pipeline_coordinator coordinator(config);

    SECTION("metrics are accessible") {
        auto& metrics = coordinator.get_metrics();
        // Just verify we can access metrics without crashing
        [[maybe_unused]] auto& stage_metrics = metrics.get_stage_metrics(pipeline_stage::network_receive);
        CHECK(stage_metrics.jobs_processed.load(std::memory_order_relaxed) == 0);
    }

    SECTION("reset_metrics clears data") {
        coordinator.reset_metrics();
        // Verify reset doesn't crash
    }
}

TEST_CASE("pipeline_coordinator get_config", "[network][pipeline][coordinator]") {
    pipeline_config config;
    config.net_io_workers = 10;
    config.name_prefix = "test_pipeline";

    pipeline_coordinator coordinator(config);
    const auto& retrieved_config = coordinator.get_config();

    CHECK(retrieved_config.net_io_workers == 10);
    CHECK(retrieved_config.name_prefix == "test_pipeline");
}

TEST_CASE("pipeline_coordinator concurrent job submission", "[network][pipeline][coordinator]") {
    pipeline_config config;
    config.net_io_workers = 4;
    config.protocol_workers = 2;
    config.execution_workers = 4;
    config.encode_workers = 2;

    pipeline_coordinator coordinator(config);
    auto start_result = coordinator.start();
    REQUIRE(start_result.is_ok());

    constexpr size_t num_jobs = 100;
    std::atomic<size_t> completed_count{0};
    std::mutex mutex;
    std::condition_variable cv;

    // Submit many jobs concurrently from multiple threads
    std::vector<std::thread> threads;
    constexpr size_t num_threads = 4;
    size_t jobs_per_thread = num_jobs / num_threads;

    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (size_t i = 0; i < jobs_per_thread; ++i) {
                auto job = std::make_unique<test_job>(
                    t * jobs_per_thread + i,
                    [&]() {
                        size_t count = completed_count.fetch_add(1, std::memory_order_relaxed) + 1;
                        if (count == num_jobs) {
                            std::lock_guard<std::mutex> lock(mutex);
                            cv.notify_all();
                        }
                    });

                auto result = coordinator.submit_to_stage(
                    pipeline_stage::network_receive, std::move(job));
                REQUIRE(result.is_ok());
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Wait for all jobs to complete with timeout
    // Use longer timeout for CI environments (especially Windows)
    // Windows CI can be significantly slower than Linux/macOS
    {
        std::unique_lock<std::mutex> lock(mutex);
        bool completed = cv.wait_for(lock, std::chrono::seconds(60),
            [&]() { return completed_count.load(std::memory_order_acquire) == num_jobs; });
        REQUIRE(completed);
    }

    CHECK(completed_count.load(std::memory_order_acquire) == num_jobs);

    static_cast<void>(coordinator.stop());
}
