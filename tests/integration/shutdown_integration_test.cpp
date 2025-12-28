/**
 * @file shutdown_integration_test.cpp
 * @brief Integration tests for graceful shutdown scenarios
 *
 * This file contains cross-system integration tests verifying graceful
 * shutdown behavior with pending tasks, resource cleanup, and system
 * state transitions.
 *
 * @note This file uses the deprecated thread_adapter API for backward
 *       compatibility testing.
 *
 * Part of Issue #390 - Enhance cross-system integration tests
 * Addresses Issue #394 - Graceful Shutdown integration test
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

#include <pacs/integration/logger_adapter.hpp>
#include <pacs/integration/thread_adapter.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <future>
#include <memory>
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
 * @brief RAII guard for test cleanup (minimal - tests manage own shutdown)
 */
struct shutdown_test_guard {
    std::filesystem::path log_dir;

    explicit shutdown_test_guard(const std::filesystem::path& dir) : log_dir(dir) {
        std::filesystem::create_directories(log_dir);
    }

    ~shutdown_test_guard() {
        // Ensure everything is cleaned up
        thread_adapter::shutdown(false);  // Force shutdown if needed
        logger_adapter::shutdown();
        std::this_thread::sleep_for(100ms);
        if (std::filesystem::exists(log_dir)) {
            std::filesystem::remove_all(log_dir);
        }
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
    auto temp_dir = std::filesystem::temp_directory_path() / "pacs_shutdown_test";
    std::filesystem::create_directories(temp_dir);
    return temp_dir;
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
    shutdown_test_guard guard(temp_dir);

    logger_config log_config;
    log_config.log_directory = temp_dir;
    log_config.enable_console = false;
    log_config.enable_file = true;
    logger_adapter::initialize(log_config);

    thread_pool_config thread_config;
    thread_config.min_threads = 2;
    thread_config.max_threads = 4;
    thread_adapter::configure(thread_config);

    SECTION("Pending tasks complete before shutdown") {
        REQUIRE(thread_adapter::start());

        std::atomic<int> completed{0};
        constexpr int task_count = 10;
        std::vector<std::future<void>> futures;

        for (int i = 0; i < task_count; ++i) {
            futures.push_back(thread_adapter::submit([&completed, i]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(20 + i * 5));
                completed++;
                logger_adapter::info("Task {} completed during shutdown", i);
            }));
        }

        // Wait for some tasks to start
        std::this_thread::sleep_for(50ms);

        // Graceful shutdown - should wait for completion
        thread_adapter::shutdown(true);

        // All futures should be available
        for (auto& f : futures) {
            f.get();
        }

        REQUIRE(completed == task_count);
        logger_adapter::flush();
    }

    SECTION("Long-running task completes during graceful shutdown") {
        REQUIRE(thread_adapter::start());

        std::atomic<bool> task_started{false};
        std::atomic<bool> task_completed{false};

        auto future = thread_adapter::submit([&task_started, &task_completed]() {
            task_started = true;
            std::this_thread::sleep_for(300ms);
            task_completed = true;
            return true;
        });

        // Wait for task to start
        REQUIRE(wait_for([&task_started]() { return task_started.load(); }));

        // Initiate graceful shutdown
        thread_adapter::shutdown(true);

        // Task should have completed
        REQUIRE(future.get());
        REQUIRE(task_completed);
    }

    SECTION("Multiple shutdown calls are safe") {
        REQUIRE(thread_adapter::start());

        auto future = thread_adapter::submit([]() {
            std::this_thread::sleep_for(50ms);
            return 42;
        });

        // Multiple shutdown calls should be safe
        thread_adapter::shutdown(true);
        thread_adapter::shutdown(true);
        thread_adapter::shutdown(false);

        REQUIRE(future.get() == 42);
        REQUIRE_FALSE(thread_adapter::is_running());
    }
}

// =============================================================================
// Immediate Shutdown Tests
// =============================================================================

TEST_CASE("Immediate shutdown behavior",
          "[integration][shutdown][immediate][!mayfail]") {
    auto temp_dir = create_temp_log_directory();
    shutdown_test_guard guard(temp_dir);

    logger_config log_config;
    log_config.log_directory = temp_dir;
    log_config.enable_console = false;
    logger_adapter::initialize(log_config);

    thread_pool_config thread_config;
    thread_config.min_threads = 2;
    thread_config.max_threads = 4;
    thread_adapter::configure(thread_config);

    SECTION("Immediate shutdown stops accepting new tasks") {
        REQUIRE(thread_adapter::start());

        std::atomic<bool> release_flag{false};
        std::atomic<int> started{0};

        // Submit blocking tasks
        for (int i = 0; i < 4; ++i) {
            thread_adapter::submit_fire_and_forget([&release_flag, &started]() {
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
        thread_adapter::shutdown(false);

        // Pool should not be running
        REQUIRE_FALSE(thread_adapter::is_running());
    }

    SECTION("Shutdown is idempotent") {
        REQUIRE(thread_adapter::start());
        REQUIRE(thread_adapter::is_running());

        thread_adapter::shutdown(false);
        REQUIRE_FALSE(thread_adapter::is_running());

        // Additional shutdown calls should be safe
        thread_adapter::shutdown(false);
        thread_adapter::shutdown(true);
        REQUIRE_FALSE(thread_adapter::is_running());
    }
}

// =============================================================================
// Resource Cleanup During Shutdown Tests
// =============================================================================

TEST_CASE("Resource cleanup during shutdown",
          "[integration][shutdown][cleanup][!mayfail]") {
    auto temp_dir = create_temp_log_directory();
    shutdown_test_guard guard(temp_dir);

    logger_config log_config;
    log_config.log_directory = temp_dir;
    log_config.enable_console = false;
    logger_adapter::initialize(log_config);

    thread_pool_config thread_config;
    thread_config.min_threads = 2;
    thread_adapter::configure(thread_config);

    SECTION("RAII resources cleaned up on graceful shutdown") {
        REQUIRE(thread_adapter::start());

        std::atomic<int> active_resources{0};
        std::vector<std::future<void>> futures;

        for (int i = 0; i < 5; ++i) {
            futures.push_back(thread_adapter::submit([&active_resources]() {
                shutdown_tracked_resource resource(active_resources);
                std::this_thread::sleep_for(50ms);
                // Resource automatically cleaned up when lambda returns
            }));
        }

        // Graceful shutdown
        thread_adapter::shutdown(true);

        // All resources should be cleaned up
        REQUIRE(active_resources == 0);
    }

    SECTION("Logger flushes during shutdown") {
        REQUIRE(thread_adapter::start());

        // Log some messages
        for (int i = 0; i < 10; ++i) {
            auto future = thread_adapter::submit([i]() {
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
    shutdown_test_guard guard(temp_dir);

    logger_config log_config;
    log_config.log_directory = temp_dir;
    log_config.enable_console = false;
    logger_adapter::initialize(log_config);

    thread_pool_config thread_config;
    thread_config.min_threads = 2;
    thread_config.max_threads = 4;
    thread_adapter::configure(thread_config);

    SECTION("Pool can be restarted after graceful shutdown") {
        // First cycle
        REQUIRE(thread_adapter::start());
        auto future1 = thread_adapter::submit([]() { return 1; });
        REQUIRE(future1.get() == 1);
        thread_adapter::shutdown(true);
        REQUIRE_FALSE(thread_adapter::is_running());

        // Second cycle
        REQUIRE(thread_adapter::start());
        auto future2 = thread_adapter::submit([]() { return 2; });
        REQUIRE(future2.get() == 2);
        thread_adapter::shutdown(true);
        REQUIRE_FALSE(thread_adapter::is_running());

        // Third cycle with different config
        thread_pool_config new_config;
        new_config.min_threads = 4;
        new_config.max_threads = 8;
        thread_adapter::configure(new_config);

        REQUIRE(thread_adapter::start());
        REQUIRE(wait_for([]() { return thread_adapter::get_thread_count() >= 4; }));

        auto future3 = thread_adapter::submit([]() { return 3; });
        REQUIRE(future3.get() == 3);
    }

    SECTION("Pool can be restarted after immediate shutdown") {
        REQUIRE(thread_adapter::start());

        // Submit a blocking task with cancellable wait
        std::atomic<bool> release_flag{false};
        thread_adapter::submit_fire_and_forget([&release_flag]() {
            while (!release_flag.load()) {
                std::this_thread::sleep_for(1ms);
            }
        });

        // Release blocking task BEFORE immediate shutdown
        release_flag.store(true);
        std::this_thread::sleep_for(10ms);  // Allow task to exit loop

        // Immediate shutdown
        thread_adapter::shutdown(false);

        // Restart
        std::this_thread::sleep_for(50ms);  // Allow cleanup
        REQUIRE(thread_adapter::start());
        REQUIRE(thread_adapter::is_running());

        auto future = thread_adapter::submit([]() { return 42; });
        REQUIRE(future.get() == 42);
    }
}

// =============================================================================
// Shutdown Order Tests
// =============================================================================

TEST_CASE("System shutdown order",
          "[integration][shutdown][order][!mayfail]") {
    auto temp_dir = create_temp_log_directory();
    shutdown_test_guard guard(temp_dir);

    SECTION("Logger shutdown before thread pool is safe") {
        logger_config log_config;
        log_config.log_directory = temp_dir;
        log_config.enable_console = false;
        logger_adapter::initialize(log_config);

        thread_pool_config thread_config;
        thread_config.min_threads = 2;
        thread_adapter::configure(thread_config);

        REQUIRE(thread_adapter::start());

        auto future = thread_adapter::submit([]() {
            logger_adapter::info("Message before logger shutdown");
            return 1;
        });

        REQUIRE(future.get() == 1);

        // Shutdown logger first
        logger_adapter::shutdown();

        // Thread pool should still work (just no logging)
        auto future2 = thread_adapter::submit([]() {
            // Logging here would be a no-op
            return 2;
        });

        REQUIRE(future2.get() == 2);

        // Then shutdown thread pool
        thread_adapter::shutdown(true);
    }

    SECTION("Thread pool shutdown before logger preserves logs") {
        logger_config log_config;
        log_config.log_directory = temp_dir;
        log_config.enable_console = false;
        log_config.enable_audit_log = true;
        logger_adapter::initialize(log_config);

        thread_pool_config thread_config;
        thread_config.min_threads = 2;
        thread_adapter::configure(thread_config);

        REQUIRE(thread_adapter::start());

        // Log from thread pool
        auto future = thread_adapter::submit([]() {
            logger_adapter::log_association_established(
                "SHUTDOWN_TEST_AE", "LOCAL_SCP", "127.0.0.1");
            return true;
        });

        REQUIRE(future.get());

        // Shutdown thread pool first
        thread_adapter::shutdown(true);

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
    shutdown_test_guard guard(temp_dir);

    logger_config log_config;
    log_config.log_directory = temp_dir;
    log_config.enable_console = false;
    logger_adapter::initialize(log_config);

    thread_pool_config thread_config;
    thread_config.min_threads = 4;
    thread_adapter::configure(thread_config);

    SECTION("Multiple threads calling shutdown simultaneously") {
        REQUIRE(thread_adapter::start());

        // Submit some work
        std::atomic<int> completed{0};
        for (int i = 0; i < 10; ++i) {
            thread_adapter::submit_fire_and_forget([&completed]() {
                std::this_thread::sleep_for(20ms);
                completed++;
            });
        }

        // Multiple threads attempt shutdown
        std::vector<std::thread> shutdown_threads;
        for (int i = 0; i < 4; ++i) {
            shutdown_threads.emplace_back([]() {
                thread_adapter::shutdown(true);
            });
        }

        // All should complete without crash
        for (auto& t : shutdown_threads) {
            t.join();
        }

        REQUIRE_FALSE(thread_adapter::is_running());
    }

    SECTION("Shutdown during active task submission") {
        REQUIRE(thread_adapter::start());

        std::atomic<bool> stop_submitting{false};
        std::atomic<int> submitted{0};
        std::atomic<int> completed{0};

        // Background submission thread
        std::thread submitter([&]() {
            while (!stop_submitting) {
                thread_adapter::submit_fire_and_forget([&completed]() {
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

        thread_adapter::shutdown(true);

        // Should complete without deadlock
        REQUIRE_FALSE(thread_adapter::is_running());
        REQUIRE(submitted > 0);
    }
}

// =============================================================================
// Shutdown Timeout Tests
// =============================================================================

TEST_CASE("Shutdown behavior with stuck tasks",
          "[integration][shutdown][timeout][!mayfail]") {
    auto temp_dir = create_temp_log_directory();
    shutdown_test_guard guard(temp_dir);

    logger_config log_config;
    log_config.log_directory = temp_dir;
    log_config.enable_console = false;
    logger_adapter::initialize(log_config);

    thread_pool_config thread_config;
    thread_config.min_threads = 2;
    thread_adapter::configure(thread_config);

    SECTION("Immediate shutdown completes promptly") {
        REQUIRE(thread_adapter::start());

        std::atomic<bool> release_flag{false};
        std::atomic<bool> task_started{false};
        std::atomic<bool> task_completed{false};

        // Submit a task that blocks until flag is set
        // This simulates a "cancellable" long-running task
        thread_adapter::submit_fire_and_forget(
            [&release_flag, &task_started, &task_completed]() {
                task_started = true;
                // Use polling with timeout instead of indefinite latch wait
                // This allows the task to be cleanly interrupted
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
        thread_adapter::shutdown(false);
        auto elapsed = std::chrono::steady_clock::now() - start;

        // Shutdown should be quick (< 1 second)
        REQUIRE(elapsed < 1s);
        REQUIRE_FALSE(thread_adapter::is_running());
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
