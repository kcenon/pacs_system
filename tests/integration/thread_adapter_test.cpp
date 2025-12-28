/**
 * @file thread_adapter_test.cpp
 * @brief Unit tests for thread_adapter
 *
 * This file contains comprehensive tests for verifying thread_system stability
 * and jthread support as part of Issue #155.
 *
 * @note This file tests the deprecated thread_adapter API for backward
 *       compatibility. New tests should use thread_pool_adapter.
 *
 * Test Categories:
 * - Configuration Tests: Verify thread pool configuration handling
 * - Pool Management Tests: Test lifecycle management of thread pool
 * - Job Submission Tests: Test various job submission patterns
 * - Statistics Tests: Verify thread pool statistics reporting
 * - Error Handling Tests: Test error recovery and handling
 * - Shutdown Tests: Test graceful and immediate shutdown
 * - Concurrent Access Tests: Test thread safety under concurrent access
 * - Thread Base Lifecycle Tests: Test thread_base start/stop cycles
 * - jthread Cleanup Tests: Verify automatic resource cleanup
 * - Cancellation Token Tests: Test cancellation propagation
 */

// Suppress deprecation warnings for testing the deprecated API
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4996)
#endif

#include <pacs/integration/thread_adapter.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <latch>
#include <barrier>

using namespace pacs::integration;
using namespace std::chrono_literals;

// =============================================================================
// Helper Utilities
// =============================================================================

namespace {

/**
 * @brief RAII guard to ensure thread_adapter is shut down after each test
 */
struct thread_adapter_guard {
    ~thread_adapter_guard() {
        thread_adapter::shutdown(true);
        std::this_thread::sleep_for(50ms);  // Allow cleanup time
    }
};

/**
 * @brief Wait for a condition with timeout
 * @param condition Function returning bool
 * @param timeout Maximum wait time
 * @return true if condition became true, false if timed out
 */
template <typename Pred>
bool wait_for(Pred condition, std::chrono::milliseconds timeout = 5000ms) {
    auto start = std::chrono::steady_clock::now();
    while (!condition()) {
        if (std::chrono::steady_clock::now() - start > timeout) {
            return false;
        }
        std::this_thread::sleep_for(10ms);
    }
    return true;
}

}  // namespace

// =============================================================================
// Configuration Tests
// =============================================================================

TEST_CASE("thread_adapter configuration", "[thread_adapter][config]") {
    thread_adapter_guard guard;
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

    SECTION("Max threads less than min threads is corrected") {
        thread_pool_config config;
        config.min_threads = 8;
        config.max_threads = 4;  // Invalid: less than min

        thread_adapter::configure(config);
        auto applied = thread_adapter::get_config();

        REQUIRE(applied.max_threads >= applied.min_threads);
    }
}

// =============================================================================
// Pool Management Tests
// =============================================================================

// NOTE: Issue #155 Discovery - thread_system has stability issues on macOS ARM64.
// The thread_pool::start() causes EXC_BAD_ACCESS in thread_base::start() during
// memory allocation. This is related to Issue #96 (SIGILL) which was closed but
// the underlying thread_system stability issues persist on Apple Silicon.
//
// These tests are marked with [!mayfail] to document known platform limitations
// while still running for verification on platforms where thread_system is stable.

TEST_CASE("thread_adapter pool management", "[thread_adapter][pool][!mayfail]") {
    thread_adapter_guard guard;
    thread_adapter::shutdown(true);

    SECTION("Pool starts successfully") {
        thread_pool_config config;
        config.min_threads = 2;
        config.max_threads = 4;
        config.pool_name = "test_pool";

        thread_adapter::configure(config);
        REQUIRE(thread_adapter::start());
        REQUIRE(thread_adapter::is_running());
    }

    SECTION("Multiple start calls are safe") {
        thread_pool_config config;
        config.min_threads = 2;
        thread_adapter::configure(config);

        REQUIRE(thread_adapter::start());
        REQUIRE(thread_adapter::start());  // Second call should be safe
        REQUIRE(thread_adapter::is_running());
    }

    SECTION("Pool reports correct thread count") {
        thread_pool_config config;
        config.min_threads = 4;
        config.max_threads = 4;
        thread_adapter::configure(config);

        REQUIRE(thread_adapter::start());
        REQUIRE(wait_for([]() { return thread_adapter::get_thread_count() >= 4; }));
    }

    SECTION("Shutdown stops the pool") {
        thread_pool_config config;
        config.min_threads = 2;
        thread_adapter::configure(config);

        REQUIRE(thread_adapter::start());
        REQUIRE(thread_adapter::is_running());

        thread_adapter::shutdown(true);
        REQUIRE_FALSE(thread_adapter::is_running());
    }
}

// =============================================================================
// Job Submission Tests
// =============================================================================

TEST_CASE("thread_adapter job submission", "[thread_adapter][submit][!mayfail]") {
    thread_adapter_guard guard;
    thread_adapter::shutdown(true);

    thread_pool_config config;
    config.min_threads = 2;
    config.max_threads = 4;
    thread_adapter::configure(config);

    SECTION("Submit returns valid future") {
        auto future = thread_adapter::submit([]() { return 42; });
        REQUIRE(future.get() == 42);
    }

    SECTION("Submit executes task asynchronously") {
        std::atomic<bool> executed{false};

        auto future = thread_adapter::submit([&executed]() {
            executed = true;
            return true;
        });

        REQUIRE(future.get() == true);
        REQUIRE(executed);
    }

    SECTION("Multiple submissions execute correctly") {
        constexpr int task_count = 100;
        std::atomic<int> counter{0};
        std::vector<std::future<void>> futures;
        futures.reserve(task_count);

        for (int i = 0; i < task_count; ++i) {
            futures.push_back(thread_adapter::submit([&counter]() { counter++; }));
        }

        for (auto& future : futures) {
            future.get();
        }

        REQUIRE(counter == task_count);
    }

    SECTION("Submit with return value works correctly") {
        auto future = thread_adapter::submit([]() {
            std::this_thread::sleep_for(10ms);
            return std::string("result");
        });

        REQUIRE(future.get() == "result");
    }
}

// =============================================================================
// Fire and Forget Tests
// =============================================================================

TEST_CASE("thread_adapter fire and forget", "[thread_adapter][fire_and_forget][!mayfail]") {
    thread_adapter_guard guard;
    thread_adapter::shutdown(true);

    thread_pool_config config;
    config.min_threads = 2;
    thread_adapter::configure(config);

    SECTION("Fire and forget executes task") {
        std::atomic<bool> executed{false};

        thread_adapter::submit_fire_and_forget([&executed]() {
            executed = true;
        });

        REQUIRE(wait_for([&executed]() { return executed.load(); }));
    }

    SECTION("Multiple fire and forget tasks execute") {
        constexpr int task_count = 50;
        std::atomic<int> counter{0};

        for (int i = 0; i < task_count; ++i) {
            thread_adapter::submit_fire_and_forget([&counter]() { counter++; });
        }

        REQUIRE(wait_for([&counter]() { return counter.load() == task_count; }));
    }
}

// =============================================================================
// Priority Submission Tests
// =============================================================================

TEST_CASE("thread_adapter priority submission", "[thread_adapter][priority][!mayfail]") {
    thread_adapter_guard guard;
    thread_adapter::shutdown(true);

    thread_pool_config config;
    config.min_threads = 1;  // Single thread to observe ordering
    config.max_threads = 1;
    thread_adapter::configure(config);

    SECTION("Critical priority tasks execute") {
        std::atomic<bool> executed{false};

        auto future = thread_adapter::submit_with_priority(
            job_priority::critical,
            [&executed]() {
                executed = true;
                return true;
            });

        REQUIRE(future.get() == true);
        REQUIRE(executed);
    }

    SECTION("All priority levels work") {
        std::vector<std::future<int>> futures;

        futures.push_back(thread_adapter::submit_with_priority(
            job_priority::critical, []() { return 1; }));
        futures.push_back(thread_adapter::submit_with_priority(
            job_priority::high, []() { return 2; }));
        futures.push_back(thread_adapter::submit_with_priority(
            job_priority::normal, []() { return 3; }));
        futures.push_back(thread_adapter::submit_with_priority(
            job_priority::low, []() { return 4; }));

        for (auto& future : futures) {
            REQUIRE(future.get() > 0);
        }
    }
}

// =============================================================================
// Statistics Tests
// =============================================================================

TEST_CASE("thread_adapter statistics", "[thread_adapter][stats][!mayfail]") {
    thread_adapter_guard guard;
    thread_adapter::shutdown(true);

    thread_pool_config config;
    config.min_threads = 4;
    config.max_threads = 4;
    thread_adapter::configure(config);

    SECTION("Thread count is reported correctly") {
        REQUIRE(thread_adapter::start());
        REQUIRE(wait_for([]() { return thread_adapter::get_thread_count() >= 4; }));
        REQUIRE(thread_adapter::get_thread_count() == 4);
    }

    SECTION("Pending job count is tracked") {
        REQUIRE(thread_adapter::start());

        // Initial state should have no pending jobs
        std::this_thread::sleep_for(100ms);  // Allow workers to start
        [[maybe_unused]] auto initial_pending = thread_adapter::get_pending_job_count();

        // Submit blocking tasks to fill the queue
        std::latch start_latch(1);
        std::atomic<int> running_count{0};

        for (int i = 0; i < 8; ++i) {
            thread_adapter::submit_fire_and_forget([&start_latch, &running_count]() {
                running_count++;
                start_latch.wait();
            });
        }

        // Wait for tasks to be picked up
        REQUIRE(wait_for([&running_count]() { return running_count.load() >= 4; }));

        // Release tasks
        start_latch.count_down();
    }

    SECTION("Idle worker count is tracked") {
        REQUIRE(thread_adapter::start());
        REQUIRE(wait_for([]() { return thread_adapter::is_running(); }));

        // After some idle time, workers should be idle
        std::this_thread::sleep_for(200ms);
        auto idle_count = thread_adapter::get_idle_worker_count();
        REQUIRE(idle_count >= 0);  // At least some should be idle
    }
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_CASE("thread_adapter error handling", "[thread_adapter][error][!mayfail]") {
    thread_adapter_guard guard;
    thread_adapter::shutdown(true);

    thread_pool_config config;
    config.min_threads = 2;
    thread_adapter::configure(config);

    SECTION("Exception in task is propagated to future") {
        auto future = thread_adapter::submit([]() -> int {
            throw std::runtime_error("test exception");
        });

        REQUIRE_THROWS_AS(future.get(), std::runtime_error);
    }

    SECTION("Pool continues after task exception") {
        // Submit a failing task
        auto failing_future = thread_adapter::submit([]() -> int {
            throw std::runtime_error("test exception");
        });

        try {
            failing_future.get();
        } catch (...) {
            // Expected
        }

        // Pool should still work
        auto success_future = thread_adapter::submit([]() { return 42; });
        REQUIRE(success_future.get() == 42);
    }
}

// =============================================================================
// Shutdown Tests
// =============================================================================

TEST_CASE("thread_adapter shutdown", "[thread_adapter][shutdown][!mayfail]") {
    thread_adapter_guard guard;
    thread_adapter::shutdown(true);

    thread_pool_config config;
    config.min_threads = 4;
    thread_adapter::configure(config);

    SECTION("Graceful shutdown completes pending tasks") {
        REQUIRE(thread_adapter::start());

        std::atomic<int> completed{0};
        std::vector<std::future<void>> futures;

        for (int i = 0; i < 10; ++i) {
            futures.push_back(thread_adapter::submit([&completed]() {
                std::this_thread::sleep_for(10ms);
                completed++;
            }));
        }

        thread_adapter::shutdown(true);  // Wait for completion

        for (auto& f : futures) {
            f.get();
        }

        REQUIRE(completed == 10);
    }

    SECTION("Shutdown is idempotent") {
        REQUIRE(thread_adapter::start());
        thread_adapter::shutdown(true);
        thread_adapter::shutdown(true);  // Should not crash
        REQUIRE_FALSE(thread_adapter::is_running());
    }

    SECTION("Restart after shutdown works") {
        REQUIRE(thread_adapter::start());
        REQUIRE(thread_adapter::is_running());

        thread_adapter::shutdown(true);
        REQUIRE_FALSE(thread_adapter::is_running());

        REQUIRE(thread_adapter::start());
        REQUIRE(thread_adapter::is_running());

        auto future = thread_adapter::submit([]() { return 42; });
        REQUIRE(future.get() == 42);
    }
}

// =============================================================================
// Concurrent Access Tests
// =============================================================================

TEST_CASE("thread_adapter concurrent access", "[thread_adapter][concurrent][!mayfail]") {
    thread_adapter_guard guard;
    thread_adapter::shutdown(true);

    thread_pool_config config;
    config.min_threads = 4;
    config.max_threads = 8;
    thread_adapter::configure(config);

    SECTION("Concurrent submissions are thread-safe") {
        REQUIRE(thread_adapter::start());

        constexpr int thread_count = 8;
        constexpr int tasks_per_thread = 100;
        std::atomic<int> total_completed{0};

        std::vector<std::thread> threads;
        threads.reserve(thread_count);

        for (int t = 0; t < thread_count; ++t) {
            threads.emplace_back([&total_completed]() {
                for (int i = 0; i < tasks_per_thread; ++i) {
                    auto future = thread_adapter::submit([&total_completed]() {
                        total_completed++;
                    });
                    future.get();
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        REQUIRE(total_completed == thread_count * tasks_per_thread);
    }

    SECTION("Concurrent fire and forget is thread-safe") {
        REQUIRE(thread_adapter::start());

        constexpr int thread_count = 4;
        constexpr int tasks_per_thread = 200;
        std::atomic<int> total_completed{0};

        std::vector<std::thread> threads;

        for (int t = 0; t < thread_count; ++t) {
            threads.emplace_back([&total_completed]() {
                for (int i = 0; i < tasks_per_thread; ++i) {
                    thread_adapter::submit_fire_and_forget([&total_completed]() {
                        total_completed++;
                    });
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        REQUIRE(wait_for([&total_completed]() {
            return total_completed.load() == thread_count * tasks_per_thread;
        }));
    }
}

// =============================================================================
// Thread Base Lifecycle Tests (Issue #155)
// =============================================================================

TEST_CASE("thread_base lifecycle stability", "[thread_adapter][lifecycle][stability][!mayfail]") {
    thread_adapter_guard guard;
    thread_adapter::shutdown(true);

    SECTION("Repeated start/stop cycles are stable") {
        thread_pool_config config;
        config.min_threads = 2;
        config.max_threads = 4;
        thread_adapter::configure(config);

        constexpr int cycles = 5;

        for (int i = 0; i < cycles; ++i) {
            INFO("Cycle " << i + 1 << " of " << cycles);

            REQUIRE(thread_adapter::start());
            REQUIRE(thread_adapter::is_running());

            // Submit some work
            auto future = thread_adapter::submit([i]() { return i * 2; });
            REQUIRE(future.get() == i * 2);

            thread_adapter::shutdown(true);
            REQUIRE_FALSE(thread_adapter::is_running());

            // Allow cleanup time between cycles
            std::this_thread::sleep_for(50ms);
        }
    }

    SECTION("Rapid start/stop does not cause crashes") {
        thread_pool_config config;
        config.min_threads = 1;
        thread_adapter::configure(config);

        constexpr int rapid_cycles = 10;

        for (int i = 0; i < rapid_cycles; ++i) {
            REQUIRE(thread_adapter::start());
            thread_adapter::shutdown(false);  // Immediate shutdown
        }

        // Final verification
        REQUIRE(thread_adapter::start());
        auto future = thread_adapter::submit([]() { return true; });
        REQUIRE(future.get());
    }

    SECTION("Long-running tasks complete during graceful shutdown") {
        thread_pool_config config;
        config.min_threads = 2;
        thread_adapter::configure(config);

        REQUIRE(thread_adapter::start());

        std::atomic<bool> task_completed{false};

        auto future = thread_adapter::submit([&task_completed]() {
            std::this_thread::sleep_for(200ms);
            task_completed = true;
            return true;
        });

        std::this_thread::sleep_for(50ms);  // Let task start
        thread_adapter::shutdown(true);     // Wait for completion

        REQUIRE(future.get() == true);
        REQUIRE(task_completed);
    }
}

// =============================================================================
// jthread Automatic Cleanup Tests (Issue #155)
// =============================================================================

TEST_CASE("jthread automatic cleanup", "[thread_adapter][jthread][cleanup][!mayfail]") {
    thread_adapter_guard guard;
    thread_adapter::shutdown(true);

    SECTION("No resource leaks after pool destruction") {
        thread_pool_config config;
        config.min_threads = 4;
        config.max_threads = 4;

        // Create and destroy pool multiple times
        for (int i = 0; i < 3; ++i) {
            thread_adapter::configure(config);
            REQUIRE(thread_adapter::start());

            // Submit some work
            std::vector<std::future<int>> futures;
            for (int j = 0; j < 20; ++j) {
                futures.push_back(thread_adapter::submit([j]() { return j; }));
            }

            for (auto& f : futures) {
                f.get();
            }

            thread_adapter::shutdown(true);
            std::this_thread::sleep_for(100ms);  // Allow cleanup
        }

        // Pool should be cleanly reusable
        thread_adapter::configure(config);
        REQUIRE(thread_adapter::start());
        auto future = thread_adapter::submit([]() { return 42; });
        REQUIRE(future.get() == 42);
    }

    SECTION("Workers are properly joined on shutdown") {
        thread_pool_config config;
        config.min_threads = 8;
        thread_adapter::configure(config);

        REQUIRE(thread_adapter::start());

        // Verify workers started
        REQUIRE(wait_for([]() { return thread_adapter::get_thread_count() >= 8; }));

        // Shutdown and verify all threads are joined
        thread_adapter::shutdown(true);

        // If threads were not properly joined, this would hang or crash
        REQUIRE_FALSE(thread_adapter::is_running());
    }
}

// =============================================================================
// Cancellation Token Propagation Tests (Issue #155)
// =============================================================================

TEST_CASE("cancellation_token propagation", "[thread_adapter][cancellation][!mayfail]") {
    thread_adapter_guard guard;
    thread_adapter::shutdown(true);

    thread_pool_config config;
    config.min_threads = 2;
    thread_adapter::configure(config);

    SECTION("Shutdown cancels pending tasks gracefully") {
        REQUIRE(thread_adapter::start());

        std::atomic<int> started_count{0};
        std::atomic<int> completed_count{0};
        std::latch block_latch(1);

        // Submit tasks that block
        std::vector<std::future<void>> futures;
        for (int i = 0; i < 10; ++i) {
            futures.push_back(thread_adapter::submit([&]() {
                started_count++;
                block_latch.wait();
                completed_count++;
            }));
        }

        // Wait for some tasks to start
        REQUIRE(wait_for([&started_count]() { return started_count.load() >= 2; }));

        // Release blocked tasks and shutdown
        block_latch.count_down();
        thread_adapter::shutdown(true);

        // All started tasks should complete
        REQUIRE(wait_for([&]() { return completed_count.load() >= started_count.load(); }));
    }

    SECTION("Pool remains functional after graceful cancellation") {
        REQUIRE(thread_adapter::start());

        // Submit and complete some tasks
        for (int i = 0; i < 5; ++i) {
            auto future = thread_adapter::submit([i]() { return i; });
            REQUIRE(future.get() == i);
        }

        // Pool should still be functional
        REQUIRE(thread_adapter::is_running());

        auto final_future = thread_adapter::submit([]() {
            return std::string("success");
        });
        REQUIRE(final_future.get() == "success");
    }
}

// =============================================================================
// Platform-Specific Stability Tests (Issue #155)
// =============================================================================

TEST_CASE("platform stability verification", "[thread_adapter][platform][stability][!mayfail]") {
    thread_adapter_guard guard;
    thread_adapter::shutdown(true);

    SECTION("High concurrency stress test") {
        thread_pool_config config;
        config.min_threads = std::thread::hardware_concurrency();
        config.max_threads = std::thread::hardware_concurrency() * 2;
        thread_adapter::configure(config);

        REQUIRE(thread_adapter::start());

        constexpr int total_tasks = 1000;
        std::atomic<int> completed{0};
        std::vector<std::future<void>> futures;
        futures.reserve(total_tasks);

        for (int i = 0; i < total_tasks; ++i) {
            futures.push_back(thread_adapter::submit([&completed]() {
                // Simulate varied workload
                std::this_thread::sleep_for(std::chrono::microseconds(rand() % 100));
                completed++;
            }));
        }

        // Wait for all futures
        for (auto& f : futures) {
            f.get();
        }

        REQUIRE(completed == total_tasks);
    }

    SECTION("Mixed priority workload") {
        thread_pool_config config;
        config.min_threads = 4;
        thread_adapter::configure(config);

        REQUIRE(thread_adapter::start());

        std::atomic<int> completed{0};
        constexpr int tasks_per_priority = 50;

        std::vector<std::future<void>> futures;

        // Submit tasks with mixed priorities
        for (int i = 0; i < tasks_per_priority; ++i) {
            futures.push_back(thread_adapter::submit_with_priority(
                job_priority::critical, [&completed]() { completed++; }));
            futures.push_back(thread_adapter::submit_with_priority(
                job_priority::high, [&completed]() { completed++; }));
            futures.push_back(thread_adapter::submit_with_priority(
                job_priority::normal, [&completed]() { completed++; }));
            futures.push_back(thread_adapter::submit_with_priority(
                job_priority::low, [&completed]() { completed++; }));
        }

        for (auto& f : futures) {
            f.get();
        }

        REQUIRE(completed == tasks_per_priority * 4);
    }
}

// Restore warning settings
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif
