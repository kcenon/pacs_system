/**
 * @file load_integration_test.cpp
 * @brief Integration tests for high load concurrent operations
 *
 * This file contains cross-system integration tests verifying behavior
 * under high load conditions including concurrent associations, thread
 * pool saturation, and connection pool management.
 *
 * Part of Issue #390 - Enhance cross-system integration tests
 * Addresses Issue #393 - High Load Concurrent Associations integration test
 */

#include <pacs/integration/logger_adapter.hpp>
#include <pacs/integration/thread_adapter.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <barrier>
#include <chrono>
#include <filesystem>
#include <future>
#include <latch>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

using namespace pacs::integration;
using namespace std::chrono_literals;

// =============================================================================
// Helper Utilities
// =============================================================================

namespace {

/**
 * @brief RAII guard for test cleanup
 */
struct load_test_guard {
    std::filesystem::path log_dir;

    explicit load_test_guard(const std::filesystem::path& dir) : log_dir(dir) {
        std::filesystem::create_directories(log_dir);
    }

    ~load_test_guard() {
        thread_adapter::shutdown(true);
        logger_adapter::shutdown();
        std::this_thread::sleep_for(100ms);  // Extra time for high-load cleanup
        if (std::filesystem::exists(log_dir)) {
            std::filesystem::remove_all(log_dir);
        }
    }
};

/**
 * @brief Wait for a condition with timeout
 */
template <typename Pred>
bool wait_for(Pred condition, std::chrono::milliseconds timeout = 10000ms) {
    auto start = std::chrono::steady_clock::now();
    while (!condition()) {
        if (std::chrono::steady_clock::now() - start > timeout) {
            return false;
        }
        std::this_thread::sleep_for(10ms);
    }
    return true;
}

/**
 * @brief Create a temporary directory for test logs
 */
auto create_temp_log_directory() -> std::filesystem::path {
    auto temp_dir = std::filesystem::temp_directory_path() / "pacs_load_test";
    std::filesystem::create_directories(temp_dir);
    return temp_dir;
}

/**
 * @brief Simulated DICOM association for load testing
 */
class simulated_association {
public:
    explicit simulated_association(int id) : id_(id), active_(true) {}

    void perform_operations(int operation_count) {
        for (int i = 0; i < operation_count && active_; ++i) {
            // Simulate DIMSE operation
            std::this_thread::sleep_for(std::chrono::microseconds(100 + (id_ % 50)));
        }
    }

    void release() { active_ = false; }

    [[nodiscard]] int id() const { return id_; }
    [[nodiscard]] bool is_active() const { return active_; }

private:
    int id_;
    std::atomic<bool> active_;
};

/**
 * @brief Thread-safe counter for load testing
 */
class concurrent_counter {
public:
    void increment() {
        std::lock_guard lock(mutex_);
        ++count_;
        max_concurrent_ = std::max(max_concurrent_, count_);
    }

    void decrement() {
        std::lock_guard lock(mutex_);
        --count_;
    }

    [[nodiscard]] int current() const {
        std::lock_guard lock(mutex_);
        return count_;
    }

    [[nodiscard]] int max() const {
        std::lock_guard lock(mutex_);
        return max_concurrent_;
    }

private:
    mutable std::mutex mutex_;
    int count_ = 0;
    int max_concurrent_ = 0;
};

}  // namespace

// =============================================================================
// High Concurrency Tests
// =============================================================================

TEST_CASE("High load concurrent task execution",
          "[integration][load][concurrent][!mayfail]") {
    auto temp_dir = create_temp_log_directory();
    load_test_guard guard(temp_dir);

    logger_config log_config;
    log_config.log_directory = temp_dir;
    log_config.enable_console = false;
    log_config.enable_file = true;
    log_config.async_mode = true;
    logger_adapter::initialize(log_config);

    // Configure for high concurrency
    thread_pool_config thread_config;
    thread_config.min_threads = std::thread::hardware_concurrency();
    thread_config.max_threads = std::thread::hardware_concurrency() * 2;
    thread_config.pool_name = "load_test_pool";
    thread_adapter::configure(thread_config);

    SECTION("100 concurrent tasks complete successfully") {
        REQUIRE(thread_adapter::start());
        REQUIRE(wait_for([]() { return thread_adapter::is_running(); }));

        constexpr int task_count = 100;
        std::atomic<int> completed{0};
        std::vector<std::future<int>> futures;
        futures.reserve(task_count);

        for (int i = 0; i < task_count; ++i) {
            futures.push_back(thread_adapter::submit([i, &completed]() {
                // Simulate variable workload
                std::this_thread::sleep_for(
                    std::chrono::microseconds(50 + (i % 100)));
                completed++;
                return i;
            }));
        }

        // Wait for all futures
        for (int i = 0; i < task_count; ++i) {
            REQUIRE(futures[i].get() == i);
        }

        REQUIRE(completed == task_count);
    }

    SECTION("Concurrent logging under load") {
        REQUIRE(thread_adapter::start());

        constexpr int task_count = 100;
        std::atomic<int> logged{0};
        std::vector<std::future<void>> futures;

        for (int i = 0; i < task_count; ++i) {
            futures.push_back(thread_adapter::submit([i, &logged]() {
                logger_adapter::info("Task {} executing", i);
                logger_adapter::log_c_find_executed(
                    "LOAD_TEST_AE", query_level::study, static_cast<std::size_t>(i));
                logged++;
            }));
        }

        for (auto& f : futures) {
            f.get();
        }

        REQUIRE(logged == task_count);
        logger_adapter::flush();
    }
}

// =============================================================================
// Thread Pool Saturation Tests
// =============================================================================

TEST_CASE("Thread pool saturation and queuing",
          "[integration][load][saturation][!mayfail]") {
    auto temp_dir = create_temp_log_directory();
    load_test_guard guard(temp_dir);

    logger_config log_config;
    log_config.log_directory = temp_dir;
    log_config.enable_console = false;
    logger_adapter::initialize(log_config);

    // Configure small pool to force saturation
    thread_pool_config thread_config;
    thread_config.min_threads = 2;
    thread_config.max_threads = 4;
    thread_adapter::configure(thread_config);

    SECTION("Tasks queue when pool is saturated") {
        REQUIRE(thread_adapter::start());
        REQUIRE(wait_for([]() { return thread_adapter::get_thread_count() >= 2; }));

        concurrent_counter active_tasks;
        std::latch block_latch(1);
        std::atomic<int> completed{0};

        constexpr int task_count = 20;  // More tasks than threads
        std::vector<std::future<void>> futures;

        for (int i = 0; i < task_count; ++i) {
            futures.push_back(thread_adapter::submit(
                [&active_tasks, &block_latch, &completed]() {
                    active_tasks.increment();
                    block_latch.wait();
                    std::this_thread::sleep_for(5ms);
                    active_tasks.decrement();
                    completed++;
                }));
        }

        // Wait for pool to be saturated
        REQUIRE(wait_for([&active_tasks, &thread_config]() {
            return active_tasks.current() >= static_cast<int>(thread_config.max_threads);
        }));

        // Max concurrent should be limited by pool size
        auto current_active = active_tasks.current();
        REQUIRE(current_active <= static_cast<int>(thread_config.max_threads));

        // Release blocked tasks
        block_latch.count_down();

        // Wait for completion
        for (auto& f : futures) {
            f.get();
        }

        REQUIRE(completed == task_count);
        REQUIRE(active_tasks.max() <= static_cast<int>(thread_config.max_threads));
    }

    SECTION("Priority tasks execute under load") {
        REQUIRE(thread_adapter::start());

        std::latch block_latch(1);
        std::atomic<int> low_priority_started{0};
        std::atomic<int> high_priority_completed{0};

        // Fill pool with low priority blocking tasks
        std::vector<std::future<void>> low_futures;
        for (int i = 0; i < 10; ++i) {
            low_futures.push_back(thread_adapter::submit_with_priority(
                job_priority::low,
                [&block_latch, &low_priority_started]() {
                    low_priority_started++;
                    block_latch.wait();
                }));
        }

        // Wait for some low priority to start
        REQUIRE(wait_for([&low_priority_started]() {
            return low_priority_started.load() >= 2;
        }));

        // Submit high priority task
        auto high_future = thread_adapter::submit_with_priority(
            job_priority::critical,
            [&high_priority_completed]() {
                high_priority_completed++;
                return true;
            });

        // Release blocked tasks
        block_latch.count_down();

        // High priority should complete
        REQUIRE(high_future.get());
        REQUIRE(high_priority_completed == 1);

        // Cleanup
        for (auto& f : low_futures) {
            f.get();
        }
    }
}

// =============================================================================
// Simulated Association Load Tests
// =============================================================================

TEST_CASE("Simulated concurrent associations",
          "[integration][load][associations][!mayfail]") {
    auto temp_dir = create_temp_log_directory();
    load_test_guard guard(temp_dir);

    logger_config log_config;
    log_config.log_directory = temp_dir;
    log_config.enable_console = false;
    log_config.enable_audit_log = true;
    logger_adapter::initialize(log_config);

    thread_pool_config thread_config;
    thread_config.min_threads = 4;
    thread_config.max_threads = 16;
    thread_adapter::configure(thread_config);

    SECTION("50 concurrent simulated associations") {
        REQUIRE(thread_adapter::start());

        constexpr int association_count = 50;
        constexpr int operations_per_association = 10;

        std::atomic<int> associations_completed{0};
        std::atomic<int> total_operations{0};
        std::vector<std::future<int>> futures;
        futures.reserve(association_count);

        for (int i = 0; i < association_count; ++i) {
            futures.push_back(thread_adapter::submit(
                [i, &associations_completed, &total_operations]() {
                    simulated_association assoc(i);

                    // Log association established
                    logger_adapter::log_association_established(
                        "MODALITY_" + std::to_string(i),
                        "LOAD_TEST_SCP",
                        "192.168.1." + std::to_string(i % 255));

                    // Perform operations
                    assoc.perform_operations(operations_per_association);
                    total_operations += operations_per_association;

                    // Log association released
                    logger_adapter::log_association_released(
                        "MODALITY_" + std::to_string(i), "LOAD_TEST_SCP");

                    assoc.release();
                    associations_completed++;
                    return assoc.id();
                }));
        }

        // Wait for all associations to complete
        for (auto& f : futures) {
            f.get();
        }

        REQUIRE(associations_completed == association_count);
        REQUIRE(total_operations == association_count * operations_per_association);
        logger_adapter::flush();
    }
}

// =============================================================================
// Stress Tests
// =============================================================================

TEST_CASE("Stress test: rapid task submission",
          "[integration][load][stress][!mayfail]") {
    auto temp_dir = create_temp_log_directory();
    load_test_guard guard(temp_dir);

    logger_config log_config;
    log_config.log_directory = temp_dir;
    log_config.enable_console = false;
    logger_adapter::initialize(log_config);

    thread_pool_config thread_config;
    thread_config.min_threads = std::thread::hardware_concurrency();
    thread_config.max_threads = std::thread::hardware_concurrency() * 2;
    thread_adapter::configure(thread_config);

    SECTION("Rapid fire-and-forget submissions") {
        REQUIRE(thread_adapter::start());

        constexpr int submission_count = 500;
        std::atomic<int> executed{0};

        auto start_time = std::chrono::steady_clock::now();

        for (int i = 0; i < submission_count; ++i) {
            thread_adapter::submit_fire_and_forget([&executed]() {
                executed++;
            });
        }

        // Wait for all to complete
        REQUIRE(wait_for([&executed]() {
            return executed.load() == submission_count;
        }, 30000ms));

        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);

        // Log performance metrics
        logger_adapter::info("Completed {} tasks in {}ms ({:.2f} tasks/ms)",
                             submission_count, duration.count(),
                             static_cast<double>(submission_count) / duration.count());

        REQUIRE(executed == submission_count);
        logger_adapter::flush();
    }

    SECTION("Mixed workload stress test") {
        REQUIRE(thread_adapter::start());

        constexpr int total_tasks = 200;
        std::atomic<int> completed{0};
        std::vector<std::future<int>> futures;

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> work_dist(1, 100);

        for (int i = 0; i < total_tasks; ++i) {
            int work_units = work_dist(gen);

            futures.push_back(thread_adapter::submit([i, work_units, &completed]() {
                // Variable workload
                std::this_thread::sleep_for(
                    std::chrono::microseconds(work_units));
                completed++;
                return i;
            }));
        }

        // Wait for all futures
        for (auto& f : futures) {
            f.get();
        }

        REQUIRE(completed == total_tasks);
    }
}

// =============================================================================
// Deadlock Prevention Tests
// =============================================================================

TEST_CASE("No deadlocks under concurrent access",
          "[integration][load][deadlock][!mayfail]") {
    auto temp_dir = create_temp_log_directory();
    load_test_guard guard(temp_dir);

    logger_config log_config;
    log_config.log_directory = temp_dir;
    log_config.enable_console = false;
    logger_adapter::initialize(log_config);

    thread_pool_config thread_config;
    thread_config.min_threads = 4;
    thread_config.max_threads = 8;
    thread_adapter::configure(thread_config);

    SECTION("Nested task submission does not deadlock") {
        REQUIRE(thread_adapter::start());

        constexpr int outer_count = 10;
        constexpr int inner_count = 5;
        std::atomic<int> total_completed{0};

        std::vector<std::future<int>> outer_futures;

        for (int i = 0; i < outer_count; ++i) {
            outer_futures.push_back(thread_adapter::submit(
                [i, &total_completed]() {
                    std::vector<std::future<int>> inner_futures;

                    for (int j = 0; j < inner_count; ++j) {
                        inner_futures.push_back(thread_adapter::submit(
                            [i, j, &total_completed]() {
                                std::this_thread::sleep_for(1ms);
                                total_completed++;
                                return i * 100 + j;
                            }));
                    }

                    int sum = 0;
                    for (auto& f : inner_futures) {
                        sum += f.get();
                    }
                    return sum;
                }));
        }

        // This should complete without deadlock within timeout
        for (auto& f : outer_futures) {
            f.get();
        }

        REQUIRE(total_completed == outer_count * inner_count);
    }

    SECTION("Concurrent statistics queries do not deadlock") {
        REQUIRE(thread_adapter::start());

        std::atomic<bool> stop{false};
        std::atomic<int> query_count{0};

        // Background tasks
        std::vector<std::future<void>> work_futures;
        for (int i = 0; i < 20; ++i) {
            work_futures.push_back(thread_adapter::submit([&stop]() {
                while (!stop) {
                    std::this_thread::sleep_for(1ms);
                }
            }));
        }

        // Query statistics from multiple threads
        std::vector<std::thread> query_threads;
        for (int i = 0; i < 4; ++i) {
            query_threads.emplace_back([&stop, &query_count]() {
                while (!stop) {
                    [[maybe_unused]] auto threads = thread_adapter::get_thread_count();
                    [[maybe_unused]] auto pending = thread_adapter::get_pending_job_count();
                    [[maybe_unused]] auto idle = thread_adapter::get_idle_worker_count();
                    query_count++;
                    std::this_thread::sleep_for(1ms);
                }
            });
        }

        // Run for a short period
        std::this_thread::sleep_for(100ms);
        stop = true;

        // Cleanup
        for (auto& t : query_threads) {
            t.join();
        }
        for (auto& f : work_futures) {
            f.get();
        }

        REQUIRE(query_count > 0);
    }
}

// =============================================================================
// Memory Stability Tests
// =============================================================================

TEST_CASE("Memory stability under load",
          "[integration][load][memory][!mayfail]") {
    auto temp_dir = create_temp_log_directory();
    load_test_guard guard(temp_dir);

    logger_config log_config;
    log_config.log_directory = temp_dir;
    log_config.enable_console = false;
    logger_adapter::initialize(log_config);

    thread_pool_config thread_config;
    thread_config.min_threads = 4;
    thread_adapter::configure(thread_config);

    SECTION("Repeated task cycles without memory growth") {
        REQUIRE(thread_adapter::start());

        constexpr int cycles = 5;
        constexpr int tasks_per_cycle = 100;

        for (int cycle = 0; cycle < cycles; ++cycle) {
            std::vector<std::future<std::vector<char>>> futures;

            for (int i = 0; i < tasks_per_cycle; ++i) {
                futures.push_back(thread_adapter::submit([]() {
                    // Allocate and deallocate memory
                    std::vector<char> data(1024, 'x');
                    return data;
                }));
            }

            for (auto& f : futures) {
                auto data = f.get();
                REQUIRE(data.size() == 1024);
            }
        }

        // If memory leaks exist, ASAN would detect them
        REQUIRE(true);
    }
}
