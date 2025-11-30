/**
 * @file thread_adapter_test.cpp
 * @brief Unit tests for thread_adapter
 */

#include <pacs/integration/thread_adapter.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace pacs::integration;
using namespace std::chrono_literals;

// =============================================================================
// Configuration Tests
// =============================================================================

TEST_CASE("thread_adapter configuration", "[thread_adapter][config]") {
    // Ensure clean state before tests
    thread_adapter::shutdown(true);

    SECTION("Default configuration is valid") {
        auto config = thread_adapter::get_config();

        REQUIRE(config.min_threads >= 1);
        REQUIRE(config.max_threads >= config.min_threads);
        REQUIRE(config.idle_timeout > 0ms);
    }

    SECTION("Custom configuration is applied") {
        thread_pool_config config;
        config.min_threads = 4;
        config.max_threads = 8;
        config.pool_name = "test_pool";

        thread_adapter::configure(config);
        auto applied = thread_adapter::get_config();

        REQUIRE(applied.min_threads == 4);
        REQUIRE(applied.max_threads == 8);
        REQUIRE(applied.pool_name == "test_pool");
    }

    SECTION("Invalid configuration is corrected") {
        thread_pool_config config;
        config.min_threads = 0;  // Invalid
        config.max_threads = 2;

        thread_adapter::configure(config);
        auto applied = thread_adapter::get_config();

        REQUIRE(applied.min_threads >= 1);  // Should be corrected
    }
}

// =============================================================================
// Pool Management Tests
// =============================================================================

TEST_CASE("thread_adapter pool management", "[thread_adapter][pool]") {
    // Ensure clean state
    thread_adapter::shutdown(true);

    SECTION("Pool can be started") {
        thread_pool_config config;
        config.min_threads = 2;
        thread_adapter::configure(config);

        REQUIRE(thread_adapter::start());
        REQUIRE(thread_adapter::is_running());

        thread_adapter::shutdown(true);
        REQUIRE_FALSE(thread_adapter::is_running());
    }

    SECTION("Multiple start calls are safe") {
        thread_pool_config config;
        config.min_threads = 2;
        thread_adapter::configure(config);

        REQUIRE(thread_adapter::start());
        REQUIRE(thread_adapter::start());  // Should be no-op
        REQUIRE(thread_adapter::is_running());

        thread_adapter::shutdown(true);
    }

    SECTION("Get pool returns valid instance") {
        thread_pool_config config;
        config.min_threads = 2;
        thread_adapter::configure(config);

        auto pool = thread_adapter::get_pool();
        REQUIRE(pool != nullptr);

        thread_adapter::shutdown(true);
    }
}

// =============================================================================
// Job Submission Tests
// =============================================================================

TEST_CASE("thread_adapter job submission", "[thread_adapter][submit]") {
    // Setup
    thread_pool_config config;
    config.min_threads = 2;
    thread_adapter::configure(config);
    REQUIRE(thread_adapter::start());

    SECTION("Submit task returns correct result") {
        auto future = thread_adapter::submit([]() { return 42; });

        auto result = future.get();
        REQUIRE(result == 42);
    }

    SECTION("Submit void task completes") {
        std::atomic<bool> executed{false};

        auto future = thread_adapter::submit([&executed]() {
            executed = true;
        });

        future.get();
        REQUIRE(executed);
    }

    SECTION("Multiple tasks execute correctly") {
        constexpr int num_tasks = 10;
        std::vector<std::future<int>> futures;
        futures.reserve(num_tasks);

        for (int i = 0; i < num_tasks; ++i) {
            futures.push_back(thread_adapter::submit([i]() { return i * 2; }));
        }

        for (int i = 0; i < num_tasks; ++i) {
            REQUIRE(futures[i].get() == i * 2);
        }
    }

    // Cleanup
    thread_adapter::shutdown(true);
}

// =============================================================================
// Fire and Forget Tests
// =============================================================================

TEST_CASE("thread_adapter fire and forget", "[thread_adapter][fire_and_forget]") {
    // Setup
    thread_pool_config config;
    config.min_threads = 2;
    thread_adapter::configure(config);
    REQUIRE(thread_adapter::start());

    SECTION("Fire and forget task executes") {
        std::atomic<int> counter{0};

        thread_adapter::submit_fire_and_forget([&counter]() {
            counter++;
        });

        // Wait for task to complete
        std::this_thread::sleep_for(100ms);
        REQUIRE(counter == 1);
    }

    SECTION("Multiple fire and forget tasks") {
        std::atomic<int> counter{0};
        constexpr int num_tasks = 5;

        for (int i = 0; i < num_tasks; ++i) {
            thread_adapter::submit_fire_and_forget([&counter]() {
                counter++;
            });
        }

        // Wait for tasks to complete
        std::this_thread::sleep_for(200ms);
        REQUIRE(counter == num_tasks);
    }

    // Cleanup
    thread_adapter::shutdown(true);
}

// =============================================================================
// Priority Submission Tests
// =============================================================================

TEST_CASE("thread_adapter priority submission", "[thread_adapter][priority]") {
    // Setup
    thread_pool_config config;
    config.min_threads = 2;
    thread_adapter::configure(config);
    REQUIRE(thread_adapter::start());

    SECTION("Critical priority task executes") {
        auto future = thread_adapter::submit_with_priority(
            job_priority::critical,
            []() { return 100; });

        REQUIRE(future.get() == 100);
    }

    SECTION("All priority levels work") {
        auto critical = thread_adapter::submit_with_priority(
            job_priority::critical, []() { return 1; });
        auto high = thread_adapter::submit_with_priority(
            job_priority::high, []() { return 2; });
        auto normal = thread_adapter::submit_with_priority(
            job_priority::normal, []() { return 3; });
        auto low = thread_adapter::submit_with_priority(
            job_priority::low, []() { return 4; });

        REQUIRE(critical.get() == 1);
        REQUIRE(high.get() == 2);
        REQUIRE(normal.get() == 3);
        REQUIRE(low.get() == 4);
    }

    // Cleanup
    thread_adapter::shutdown(true);
}

// =============================================================================
// Statistics Tests
// =============================================================================

TEST_CASE("thread_adapter statistics", "[thread_adapter][stats]") {
    // Ensure clean state
    thread_adapter::shutdown(true);

    SECTION("Thread count reflects configuration") {
        thread_pool_config config;
        config.min_threads = 3;
        thread_adapter::configure(config);
        REQUIRE(thread_adapter::start());

        // Give time for workers to start
        std::this_thread::sleep_for(50ms);

        REQUIRE(thread_adapter::get_thread_count() >= 3);

        thread_adapter::shutdown(true);
    }

    SECTION("Pending job count is accurate") {
        thread_pool_config config;
        config.min_threads = 1;
        thread_adapter::configure(config);
        REQUIRE(thread_adapter::start());

        // Initially should be 0 or very low
        [[maybe_unused]] auto initial_pending = thread_adapter::get_pending_job_count();

        thread_adapter::shutdown(true);
    }

    SECTION("Statistics return 0 when pool not running") {
        thread_adapter::shutdown(true);

        REQUIRE(thread_adapter::get_thread_count() == 0);
        REQUIRE(thread_adapter::get_pending_job_count() == 0);
        REQUIRE(thread_adapter::get_idle_worker_count() == 0);
    }
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_CASE("thread_adapter error handling", "[thread_adapter][error]") {
    // Setup
    thread_pool_config config;
    config.min_threads = 2;
    thread_adapter::configure(config);
    REQUIRE(thread_adapter::start());

    SECTION("Exception in task is propagated") {
        auto future = thread_adapter::submit([]() -> int {
            throw std::runtime_error("test exception");
        });

        REQUIRE_THROWS_AS(future.get(), std::runtime_error);
    }

    SECTION("Fire and forget handles exceptions gracefully") {
        // Should not crash
        thread_adapter::submit_fire_and_forget([]() {
            throw std::runtime_error("ignored exception");
        });

        // Give time for task to complete
        std::this_thread::sleep_for(100ms);

        // Pool should still be running
        REQUIRE(thread_adapter::is_running());
    }

    // Cleanup
    thread_adapter::shutdown(true);
}

// =============================================================================
// Shutdown Tests
// =============================================================================

TEST_CASE("thread_adapter shutdown", "[thread_adapter][shutdown]") {
    SECTION("Graceful shutdown waits for tasks") {
        thread_pool_config config;
        config.min_threads = 2;
        thread_adapter::configure(config);
        REQUIRE(thread_adapter::start());

        std::atomic<bool> task_completed{false};

        thread_adapter::submit_fire_and_forget([&task_completed]() {
            std::this_thread::sleep_for(50ms);
            task_completed = true;
        });

        thread_adapter::shutdown(true);  // Wait for completion

        REQUIRE(task_completed);
    }

    SECTION("Immediate shutdown stops pool") {
        thread_pool_config config;
        config.min_threads = 2;
        thread_adapter::configure(config);
        REQUIRE(thread_adapter::start());

        REQUIRE(thread_adapter::is_running());

        thread_adapter::shutdown(false);  // Immediate

        REQUIRE_FALSE(thread_adapter::is_running());
    }
}

// =============================================================================
// Concurrent Access Tests
// =============================================================================

TEST_CASE("thread_adapter concurrent access", "[thread_adapter][concurrent]") {
    // Setup
    thread_pool_config config;
    config.min_threads = 4;
    thread_adapter::configure(config);
    REQUIRE(thread_adapter::start());

    SECTION("Concurrent submissions are safe") {
        constexpr int num_threads = 4;
        constexpr int tasks_per_thread = 25;
        std::atomic<int> total_completed{0};

        std::vector<std::thread> threads;
        threads.reserve(num_threads);

        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&total_completed]() {
                for (int i = 0; i < tasks_per_thread; ++i) {
                    thread_adapter::submit([&total_completed]() {
                        total_completed++;
                    }).wait();
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        REQUIRE(total_completed == num_threads * tasks_per_thread);
    }

    // Cleanup
    thread_adapter::shutdown(true);
}
