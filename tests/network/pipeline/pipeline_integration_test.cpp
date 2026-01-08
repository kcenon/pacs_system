/**
 * @file pipeline_integration_test.cpp
 * @brief Integration tests for the complete DICOM I/O pipeline
 *
 * Tests basic pipeline functionality without complex job chains.
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
#include <vector>
#include <algorithm>

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

// Helper for waiting for multiple completions
class multi_completion_waiter {
public:
    explicit multi_completion_waiter(size_t target) : target_(target) {}

    void signal() {
        size_t count = completed_.fetch_add(1, std::memory_order_relaxed) + 1;
        if (count >= target_) {
            std::lock_guard<std::mutex> lock(mutex_);
            cv_.notify_all();
        }
    }

    bool wait_for(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, timeout,
            [this] { return completed_.load(std::memory_order_acquire) >= target_; });
    }

    size_t count() const {
        return completed_.load(std::memory_order_acquire);
    }

private:
    size_t target_;
    std::atomic<size_t> completed_{0};
    std::mutex mutex_;
    std::condition_variable cv_;
};

// Simple test job that doesn't chain to other stages
class simple_test_job : public pipeline_job_base {
public:
    explicit simple_test_job(uint64_t job_id = 0,
                             std::function<void()> on_execute = nullptr)
        : on_execute_(std::move(on_execute)) {
        context_.job_id = job_id;
        context_.stage = pipeline_stage::network_receive;
        context_.category = job_category::echo;
    }

    auto execute([[maybe_unused]] pipeline_coordinator& coordinator) -> VoidResult override {
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
        return "simple_test_job";
    }

private:
    job_context context_;
    std::function<void()> on_execute_;
};

}  // namespace

TEST_CASE("pipeline integration - basic functionality", "[network][pipeline][integration]") {
    pipeline_config config;
    config.net_io_workers = 2;
    config.protocol_workers = 2;
    config.execution_workers = 2;
    config.encode_workers = 2;
    config.enable_metrics = true;

    pipeline_coordinator coordinator(config);
    auto start_result = coordinator.start();
    REQUIRE(start_result.is_ok());

    SECTION("job execution triggers completion callback") {
        std::atomic<bool> executed{false};
        std::atomic<bool> callback_invoked{false};
        completion_waiter waiter;

        coordinator.set_job_completion_callback(
            [&]([[maybe_unused]] const job_context& ctx,
                [[maybe_unused]] bool success) {
                callback_invoked.store(true, std::memory_order_release);
                waiter.signal();
            });

        auto job = std::make_unique<simple_test_job>(1, [&]() {
            executed.store(true, std::memory_order_release);
        });

        auto result = coordinator.submit_to_stage(
            pipeline_stage::network_receive, std::move(job));
        REQUIRE(result.is_ok());

        bool completed = waiter.wait_for(std::chrono::seconds(5));
        REQUIRE(completed);
        CHECK(executed.load(std::memory_order_acquire) == true);
        CHECK(callback_invoked.load(std::memory_order_acquire) == true);
    }

    static_cast<void>(coordinator.stop());
}

TEST_CASE("pipeline integration - multiple jobs", "[network][pipeline][integration]") {
    pipeline_config config;
    config.net_io_workers = 4;
    config.protocol_workers = 2;
    config.execution_workers = 4;
    config.encode_workers = 2;
    config.enable_metrics = true;

    pipeline_coordinator coordinator(config);
    auto start_result = coordinator.start();
    REQUIRE(start_result.is_ok());

    SECTION("multiple jobs execute and complete") {
        constexpr size_t num_jobs = 50;
        std::atomic<size_t> execute_count{0};
        multi_completion_waiter waiter(num_jobs);

        coordinator.set_job_completion_callback(
            [&]([[maybe_unused]] const job_context& ctx,
                [[maybe_unused]] bool success) {
                waiter.signal();
            });

        for (size_t i = 0; i < num_jobs; ++i) {
            auto job = std::make_unique<simple_test_job>(i + 1, [&]() {
                execute_count.fetch_add(1, std::memory_order_relaxed);
            });

            auto result = coordinator.submit_to_stage(
                pipeline_stage::network_receive, std::move(job));
            REQUIRE(result.is_ok());
        }

        bool completed = waiter.wait_for(std::chrono::seconds(10));
        REQUIRE(completed);
        CHECK(execute_count.load(std::memory_order_acquire) == num_jobs);
    }

    static_cast<void>(coordinator.stop());
}

TEST_CASE("pipeline integration - metrics update", "[network][pipeline][integration]") {
    pipeline_config config;
    config.net_io_workers = 2;
    config.protocol_workers = 1;
    config.execution_workers = 2;
    config.encode_workers = 1;
    config.enable_metrics = true;

    pipeline_coordinator coordinator(config);
    auto start_result = coordinator.start();
    REQUIRE(start_result.is_ok());

    SECTION("metrics track job completions") {
        completion_waiter waiter;

        coordinator.set_job_completion_callback(
            [&]([[maybe_unused]] const job_context& ctx,
                [[maybe_unused]] bool success) {
                waiter.signal();
            });

        auto job = std::make_unique<simple_test_job>(1);
        auto result = coordinator.submit_to_stage(
            pipeline_stage::network_receive, std::move(job));
        REQUIRE(result.is_ok());

        bool completed = waiter.wait_for(std::chrono::seconds(5));
        REQUIRE(completed);

        auto& metrics = coordinator.get_metrics();
        auto& stage_metrics = metrics.get_stage_metrics(pipeline_stage::network_receive);
        CHECK(stage_metrics.jobs_processed.load(std::memory_order_relaxed) >= 1);
    }

    static_cast<void>(coordinator.stop());
}

TEST_CASE("pipeline integration - graceful shutdown", "[network][pipeline][integration]") {
    pipeline_config config;
    config.net_io_workers = 2;
    config.protocol_workers = 1;
    config.execution_workers = 2;
    config.encode_workers = 1;
    config.shutdown_timeout = std::chrono::milliseconds{1000};

    SECTION("coordinator stops cleanly after jobs complete") {
        pipeline_coordinator coordinator(config);
        auto start_result = coordinator.start();
        REQUIRE(start_result.is_ok());

        constexpr size_t num_jobs = 10;
        multi_completion_waiter waiter(num_jobs);

        coordinator.set_job_completion_callback(
            [&]([[maybe_unused]] const job_context& ctx,
                [[maybe_unused]] bool success) {
                waiter.signal();
            });

        for (size_t i = 0; i < num_jobs; ++i) {
            auto job = std::make_unique<simple_test_job>(i + 1);
            auto submit_result = coordinator.submit_to_stage(
                pipeline_stage::network_receive, std::move(job));
            REQUIRE(submit_result.is_ok());
        }

        // Wait for jobs to complete
        bool all_completed = waiter.wait_for(std::chrono::seconds(5));
        REQUIRE(all_completed);

        // Stop should succeed
        auto stop_result = coordinator.stop();
        CHECK(stop_result.is_ok());
    }
}

TEST_CASE("pipeline integration - job ID uniqueness", "[network][pipeline][integration]") {
    pipeline_config config;
    config.net_io_workers = 2;
    config.protocol_workers = 1;
    config.execution_workers = 1;
    config.encode_workers = 1;

    pipeline_coordinator coordinator(config);
    auto start_result = coordinator.start();
    REQUIRE(start_result.is_ok());

    std::vector<uint64_t> job_ids;
    std::mutex ids_mutex;
    constexpr size_t num_jobs = 100;
    multi_completion_waiter waiter(num_jobs);

    coordinator.set_job_completion_callback(
        [&](const job_context& ctx, [[maybe_unused]] bool success) {
            {
                std::lock_guard<std::mutex> lock(ids_mutex);
                job_ids.push_back(ctx.job_id);
            }
            waiter.signal();
        });

    // Submit jobs from multiple threads
    std::vector<std::thread> threads;
    for (size_t t = 0; t < 4; ++t) {
        threads.emplace_back([&, t]() {
            for (size_t i = 0; i < num_jobs / 4; ++i) {
                auto job = std::make_unique<simple_test_job>(t * 1000 + i);
                auto result = coordinator.submit_to_stage(
                    pipeline_stage::network_receive, std::move(job));
                REQUIRE(result.is_ok());
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    bool completed = waiter.wait_for(std::chrono::seconds(10));
    REQUIRE(completed);

    // Check uniqueness
    std::sort(job_ids.begin(), job_ids.end());
    auto unique_end = std::unique(job_ids.begin(), job_ids.end());
    CHECK(unique_end == job_ids.end());

    static_cast<void>(coordinator.stop());
}
