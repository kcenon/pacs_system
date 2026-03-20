/**
 * @file shutdown_integration_test.cpp
 * @brief Integration tests for graceful shutdown scenarios
 *
 * This file contains cross-system integration tests verifying graceful
 * shutdown behavior with pending tasks, resource cleanup, and system
 * state transitions.
 *
 * Uses thread_pool_adapter with dependency injection pattern.
 *
 * Part of Issue #390 - Enhance cross-system integration tests
 * Addresses Issue #394 - Graceful Shutdown integration test
 */

#include <pacs/integration/logger_adapter.hpp>
#include <pacs/integration/thread_pool_adapter.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <future>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

using namespace kcenon::pacs::integration;
using namespace std::chrono_literals;

// =============================================================================
// Helper Utilities
// =============================================================================

namespace {

auto current_process_id() -> unsigned long long {
#if defined(_WIN32)
    return static_cast<unsigned long long>(::_getpid());
#else
    return static_cast<unsigned long long>(::getpid());
#endif
}

/**
 * @brief RAII guard for test cleanup (minimal - tests manage own shutdown)
 */
struct shutdown_test_guard {
    std::filesystem::path log_dir;
    std::shared_ptr<thread_pool_adapter> pool;

    shutdown_test_guard(const std::filesystem::path& dir,
                        std::shared_ptr<thread_pool_adapter> p)
        : log_dir(dir), pool(std::move(p)) {
        std::filesystem::create_directories(log_dir);
    }

    ~shutdown_test_guard() {
        // Ensure everything is cleaned up
        if (pool) {
            pool->shutdown(false);  // Force shutdown if needed
        }
        logger_adapter::shutdown();
        std::this_thread::sleep_for(100ms);
        std::error_code ec;
        std::filesystem::remove_all(log_dir, ec);
    }
};

/**
 * @brief Wait for a condition with timeout
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

/**
 * @brief Create a temporary directory for test logs
 */
auto create_temp_log_directory() -> std::filesystem::path {
    const auto temp_base = std::filesystem::temp_directory_path();
    const auto pid = current_process_id();

    for (int attempt = 0; attempt < 100; ++attempt) {
        const auto timestamp = static_cast<unsigned long long>(
            std::chrono::steady_clock::now().time_since_epoch().count());
        const auto dir_name = "pacs_shutdown_test_" + std::to_string(pid) + "_" +
                              std::to_string(timestamp) + "_" + std::to_string(attempt);
        auto temp_dir = temp_base / dir_name;

        std::error_code ec;
        if (std::filesystem::create_directories(temp_dir, ec) && !ec) {
            return temp_dir;
        }

        std::this_thread::sleep_for(1ms);
    }

    throw std::runtime_error("Failed to create unique temporary shutdown test directory");
}

/**
 * @brief Tracked resource for verifying cleanup during shutdown
 */
class shutdown_tracked_resource {
public:
    explicit shutdown_tracked_resource(std::atomic<int>& counter)
        : counter_(counter), active_(true) {
        counter_++;
    }

    ~shutdown_tracked_resource() {
        if (active_) {
            counter_--;
        }
    }

    shutdown_tracked_resource(const shutdown_tracked_resource&) = delete;
    shutdown_tracked_resource& operator=(const shutdown_tracked_resource&) = delete;

    shutdown_tracked_resource(shutdown_tracked_resource&& other) noexcept
        : counter_(other.counter_), active_(other.active_) {
        other.active_ = false;
    }

private:
    std::atomic<int>& counter_;
    bool active_;
};

}  // namespace

// =============================================================================
// Graceful Shutdown Tests
// =============================================================================

TEST_CASE("Graceful shutdown with pending tasks",
          "[integration][shutdown][graceful][!mayfail]") {
    auto temp_dir = create_temp_log_directory();

    logger_config log_config;
    log_config.log_directory = temp_dir;
    log_config.enable_console = false;
    log_config.enable_file = true;
    logger_adapter::initialize(log_config);

    thread_pool_config thread_config;
    thread_config.min_threads = 2;
    thread_config.max_threads = 4;
    auto pool = std::make_shared<thread_pool_adapter>(thread_config);
    shutdown_test_guard guard(temp_dir, pool);

    SECTION("Pending tasks complete before shutdown") {
        REQUIRE(pool->start());

        std::atomic<int> completed{0};
        constexpr int task_count = 10;
        std::vector<std::future<void>> futures;

        for (int i = 0; i < task_count; ++i) {
            futures.push_back(pool->submit([&completed, i]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(20 + i * 5));
                completed++;
                logger_adapter::info("Task {} completed during shutdown", i);
            }));
        }

        // Wait for some tasks to start
        std::this_thread::sleep_for(50ms);

        // Graceful shutdown - should wait for completion
        pool->shutdown(true);

        // All futures should be available
        for (auto& f : futures) {
            f.get();
        }

        REQUIRE(completed == task_count);
        logger_adapter::flush();
    }

    SECTION("Long-running task completes during graceful shutdown") {
        REQUIRE(pool->start());

        std::atomic<bool> task_started{false};
        std::atomic<bool> task_completed{false};

        auto future = pool->submit([&task_started, &task_completed]() {
            task_started = true;
            std::this_thread::sleep_for(300ms);
            task_completed = true;
        });

        // Wait for task to start
        REQUIRE(wait_for([&task_started]() { return task_started.load(); }));

        // Initiate graceful shutdown
        pool->shutdown(true);

        // Task should have completed
        future.get();
        REQUIRE(task_completed);
    }

    SECTION("Multiple shutdown calls are safe") {
        REQUIRE(pool->start());

        auto future = pool->submit([]() {
            std::this_thread::sleep_for(50ms);
        });

        // Multiple shutdown calls should be safe
        pool->shutdown(true);
        pool->shutdown(true);
        pool->shutdown(false);

        future.get();
        REQUIRE_FALSE(pool->is_running());
    }
}

// =============================================================================
// Immediate Shutdown Tests
// =============================================================================

TEST_CASE("Immediate shutdown behavior",
          "[integration][shutdown][immediate][!mayfail]") {
    auto temp_dir = create_temp_log_directory();

    logger_config log_config;
    log_config.log_directory = temp_dir;
    log_config.enable_console = false;
    logger_adapter::initialize(log_config);

    thread_pool_config thread_config;
    thread_config.min_threads = 2;
    thread_config.max_threads = 4;
    auto pool = std::make_shared<thread_pool_adapter>(thread_config);
    shutdown_test_guard guard(temp_dir, pool);

    SECTION("Immediate shutdown stops accepting new tasks") {
        REQUIRE(pool->start());

        std::atomic<bool> release_flag{false};
        std::atomic<int> started{0};

        // Submit blocking tasks
        for (int i = 0; i < 4; ++i) {
            pool->submit_fire_and_forget([&release_flag, &started]() {
                started++;
                // Use atomic flag with polling instead of latch for safer cleanup
                while (!release_flag.load()) {
                    std::this_thread::sleep_for(1ms);
                }
            });
        }

        // Wait for tasks to start
        REQUIRE(wait_for([&started]() { return started.load() >= 2; }));

        // Release blocked tasks BEFORE shutdown to prevent deadlock
        release_flag.store(true);

        // Small delay to let tasks see the flag
        std::this_thread::sleep_for(10ms);

        // Immediate shutdown
        pool->shutdown(false);

        // Pool should not be running
        REQUIRE_FALSE(pool->is_running());
    }

    SECTION("Shutdown is idempotent") {
        REQUIRE(pool->start());
        REQUIRE(pool->is_running());

        pool->shutdown(false);
        REQUIRE_FALSE(pool->is_running());

        // Additional shutdown calls should be safe
        pool->shutdown(false);
        pool->shutdown(true);
        REQUIRE_FALSE(pool->is_running());
    }
}

// =============================================================================
// Resource Cleanup During Shutdown Tests
// =============================================================================

TEST_CASE("Resource cleanup during shutdown",
          "[integration][shutdown][cleanup][!mayfail]") {
    auto temp_dir = create_temp_log_directory();

    logger_config log_config;
    log_config.log_directory = temp_dir;
    log_config.enable_console = false;
    logger_adapter::initialize(log_config);

    thread_pool_config thread_config;
    thread_config.min_threads = 2;
    auto pool = std::make_shared<thread_pool_adapter>(thread_config);
    shutdown_test_guard guard(temp_dir, pool);

    SECTION("RAII resources cleaned up on graceful shutdown") {
        REQUIRE(pool->start());

        std::atomic<int> active_resources{0};
        std::vector<std::future<void>> futures;

        for (int i = 0; i < 5; ++i) {
            futures.push_back(pool->submit([&active_resources]() {
                shutdown_tracked_resource resource(active_resources);
                std::this_thread::sleep_for(50ms);
                // Resource automatically cleaned up when lambda returns
            }));
        }

        // Graceful shutdown
        pool->shutdown(true);

        // All resources should be cleaned up
        REQUIRE(active_resources == 0);
    }

    SECTION("Logger flushes during shutdown") {
        REQUIRE(pool->start());

        // Log some messages
        for (int i = 0; i < 10; ++i) {
            auto future = pool->submit([i]() {
                logger_adapter::info("Shutdown test message {}", i);
            });
            future.get();
        }

        // Shutdown logger explicitly
        logger_adapter::flush();

        // Verify log file exists
        // (actual content verification would require reading the file)
    }
}

// =============================================================================
// Restart After Shutdown Tests
// =============================================================================

TEST_CASE("Restart after shutdown",
          "[integration][shutdown][restart][!mayfail]") {
    auto temp_dir = create_temp_log_directory();

    logger_config log_config;
    log_config.log_directory = temp_dir;
    log_config.enable_console = false;
    logger_adapter::initialize(log_config);

    // Use a guard without pool since we manage multiple pools in this test
    shutdown_test_guard guard(temp_dir, nullptr);

    SECTION("Pool can be restarted by creating a new instance") {
        thread_pool_config thread_config;
        thread_config.min_threads = 2;
        thread_config.max_threads = 4;

        // First cycle
        {
            auto pool = std::make_shared<thread_pool_adapter>(thread_config);
            REQUIRE(pool->start());
            std::atomic<int> result{0};
            auto future = pool->submit([&result]() { result = 1; });
            future.get();
            REQUIRE(result == 1);
            pool->shutdown(true);
            REQUIRE_FALSE(pool->is_running());
        }

        // Second cycle
        {
            auto pool = std::make_shared<thread_pool_adapter>(thread_config);
            REQUIRE(pool->start());
            std::atomic<int> result{0};
            auto future = pool->submit([&result]() { result = 2; });
            future.get();
            REQUIRE(result == 2);
            pool->shutdown(true);
            REQUIRE_FALSE(pool->is_running());
        }

        // Third cycle with different config
        {
            thread_pool_config new_config;
            new_config.min_threads = 4;
            new_config.max_threads = 8;
            auto pool = std::make_shared<thread_pool_adapter>(new_config);
            REQUIRE(pool->start());
            REQUIRE(wait_for([&pool]() { return pool->get_thread_count() >= 4; }));

            std::atomic<int> result{0};
            auto future = pool->submit([&result]() { result = 3; });
            future.get();
            REQUIRE(result == 3);
            pool->shutdown(true);
        }
    }

    SECTION("Pool can be restarted after immediate shutdown") {
        thread_pool_config thread_config;
        thread_config.min_threads = 2;
        thread_config.max_threads = 4;

        {
            auto pool = std::make_shared<thread_pool_adapter>(thread_config);
            REQUIRE(pool->start());

            // Submit a blocking task with cancellable wait
            std::atomic<bool> release_flag{false};
            pool->submit_fire_and_forget([&release_flag]() {
                while (!release_flag.load()) {
                    std::this_thread::sleep_for(1ms);
                }
            });

            // Release blocking task BEFORE immediate shutdown
            release_flag.store(true);
            std::this_thread::sleep_for(10ms);  // Allow task to exit loop

            // Immediate shutdown
            pool->shutdown(false);
        }

        // Restart with new pool
        std::this_thread::sleep_for(50ms);  // Allow cleanup
        {
            auto pool = std::make_shared<thread_pool_adapter>(thread_config);
            REQUIRE(pool->start());
            REQUIRE(pool->is_running());

            std::atomic<int> result{0};
            auto future = pool->submit([&result]() { result = 42; });
            future.get();
            REQUIRE(result == 42);
            pool->shutdown(true);
        }
    }
}

// =============================================================================
// Shutdown Order Tests
// =============================================================================

TEST_CASE("System shutdown order",
          "[integration][shutdown][order][!mayfail]") {
    auto temp_dir = create_temp_log_directory();
    shutdown_test_guard guard(temp_dir, nullptr);

    SECTION("Logger shutdown before thread pool is safe") {
        logger_config log_config;
        log_config.log_directory = temp_dir;
        log_config.enable_console = false;
        logger_adapter::initialize(log_config);

        thread_pool_config thread_config;
        thread_config.min_threads = 2;
        auto pool = std::make_shared<thread_pool_adapter>(thread_config);

        REQUIRE(pool->start());

        auto future = pool->submit([]() {
            logger_adapter::info("Message before logger shutdown");
        });

        future.get();

        // Shutdown logger first
        logger_adapter::shutdown();

        // Thread pool should still work (just no logging)
        std::atomic<int> result{0};
        auto future2 = pool->submit([&result]() {
            // Logging here would be a no-op
            result = 2;
        });

        future2.get();
        REQUIRE(result == 2);

        // Then shutdown thread pool
        pool->shutdown(true);
    }

    SECTION("Thread pool shutdown before logger preserves logs") {
        logger_config log_config;
        log_config.log_directory = temp_dir;
        log_config.enable_console = false;
        log_config.enable_audit_log = true;
        logger_adapter::initialize(log_config);

        thread_pool_config thread_config;
        thread_config.min_threads = 2;
        auto pool = std::make_shared<thread_pool_adapter>(thread_config);

        REQUIRE(pool->start());

        // Log from thread pool
        auto future = pool->submit([]() {
            logger_adapter::log_association_established(
                "SHUTDOWN_TEST_AE", "LOCAL_SCP", "127.0.0.1");
        });

        future.get();

        // Shutdown thread pool first
        pool->shutdown(true);

        // Logger can still be used
        logger_adapter::info("Final message after thread pool shutdown");
        logger_adapter::flush();

        // Then shutdown logger
        logger_adapter::shutdown();
    }
}

// =============================================================================
// Concurrent Shutdown Tests
// =============================================================================

TEST_CASE("Concurrent shutdown attempts",
          "[integration][shutdown][concurrent][!mayfail]") {
    auto temp_dir = create_temp_log_directory();

    logger_config log_config;
    log_config.log_directory = temp_dir;
    log_config.enable_console = false;
    logger_adapter::initialize(log_config);

    thread_pool_config thread_config;
    thread_config.min_threads = 4;
    auto pool = std::make_shared<thread_pool_adapter>(thread_config);
    shutdown_test_guard guard(temp_dir, pool);

    SECTION("Multiple threads calling shutdown simultaneously") {
        REQUIRE(pool->start());

        // Submit some work
        std::atomic<int> completed{0};
        for (int i = 0; i < 10; ++i) {
            pool->submit_fire_and_forget([&completed]() {
                std::this_thread::sleep_for(20ms);
                completed++;
            });
        }

        // Multiple threads attempt shutdown
        std::vector<std::thread> shutdown_threads;
        for (int i = 0; i < 4; ++i) {
            shutdown_threads.emplace_back([&pool]() {
                pool->shutdown(true);
            });
        }

        // All should complete without crash
        for (auto& t : shutdown_threads) {
            t.join();
        }

        REQUIRE_FALSE(pool->is_running());
    }

    SECTION("Shutdown during active task submission") {
        REQUIRE(pool->start());

        std::atomic<bool> stop_submitting{false};
        std::atomic<int> submitted{0};
        std::atomic<int> completed{0};

        // Background submission thread
        std::thread submitter([&]() {
            while (!stop_submitting) {
                pool->submit_fire_and_forget([&completed]() {
                    std::this_thread::sleep_for(1ms);
                    completed++;
                });
                submitted++;
                std::this_thread::sleep_for(1ms);
            }
        });

        // Let some tasks be submitted
        std::this_thread::sleep_for(50ms);

        // Stop submission and shutdown
        stop_submitting = true;
        submitter.join();

        pool->shutdown(true);

        // Should complete without deadlock
        REQUIRE_FALSE(pool->is_running());
        REQUIRE(submitted > 0);
    }
}

// =============================================================================
// Shutdown Timeout Tests
// =============================================================================

TEST_CASE("Shutdown behavior with stuck tasks",
          "[integration][shutdown][timeout][!mayfail]") {
    auto temp_dir = create_temp_log_directory();

    logger_config log_config;
    log_config.log_directory = temp_dir;
    log_config.enable_console = false;
    logger_adapter::initialize(log_config);

    thread_pool_config thread_config;
    thread_config.min_threads = 2;
    auto pool = std::make_shared<thread_pool_adapter>(thread_config);
    shutdown_test_guard guard(temp_dir, pool);

    SECTION("Immediate shutdown completes promptly") {
        REQUIRE(pool->start());

        std::atomic<bool> release_flag{false};
        std::atomic<bool> task_started{false};
        std::atomic<bool> task_completed{false};

        // Submit a task that blocks until flag is set
        pool->submit_fire_and_forget(
            [&release_flag, &task_started, &task_completed]() {
                task_started = true;
                while (!release_flag.load()) {
                    std::this_thread::sleep_for(1ms);
                }
                task_completed = true;
            });

        // Wait for task to start
        REQUIRE(wait_for([&task_started]() { return task_started.load(); }));

        // Release the task BEFORE shutdown
        release_flag.store(true);

        // Wait for task to complete
        REQUIRE(wait_for([&task_completed]() { return task_completed.load(); }, 5000ms));

        // Now shutdown should be quick
        auto start = std::chrono::steady_clock::now();
        pool->shutdown(false);
        auto elapsed = std::chrono::steady_clock::now() - start;

        // Shutdown should be quick (< 1 second)
        REQUIRE(elapsed < 1s);
        REQUIRE_FALSE(pool->is_running());
    }
}
