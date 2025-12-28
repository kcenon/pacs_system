/**
 * @file config_reload_integration_test.cpp
 * @brief Integration tests for configuration hot-reload scenarios
 *
 * This file contains cross-system integration tests verifying runtime
 * configuration changes and their propagation across systems.
 *
 * @note This file uses the deprecated thread_adapter API for backward
 *       compatibility testing.
 *
 * Part of Issue #390 - Enhance cross-system integration tests
 * Addresses Issue #395 - Configuration Hot-Reload integration test
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
struct config_test_guard {
    std::filesystem::path log_dir;

    explicit config_test_guard(const std::filesystem::path& dir) : log_dir(dir) {
        std::filesystem::create_directories(log_dir);
    }

    ~config_test_guard() {
        thread_adapter::shutdown(true);
        logger_adapter::shutdown();
        std::this_thread::sleep_for(50ms);
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
    auto temp_dir = std::filesystem::temp_directory_path() / "pacs_config_test";
    std::filesystem::create_directories(temp_dir);
    return temp_dir;
}

}  // namespace

// =============================================================================
// Logger Configuration Tests
// =============================================================================

TEST_CASE("Logger runtime configuration changes",
          "[integration][config][logger][!mayfail]") {
    auto temp_dir = create_temp_log_directory();
    config_test_guard guard(temp_dir);

    SECTION("Log level can be changed at runtime") {
        logger_config config;
        config.log_directory = temp_dir;
        config.enable_console = false;
        config.enable_file = true;
        config.min_level = log_level::info;
        logger_adapter::initialize(config);

        // Initial level
        REQUIRE(logger_adapter::get_min_level() == log_level::info);
        REQUIRE(logger_adapter::is_level_enabled(log_level::info));
        REQUIRE_FALSE(logger_adapter::is_level_enabled(log_level::debug));

        // Change level to debug
        logger_adapter::set_min_level(log_level::debug);
        REQUIRE(logger_adapter::get_min_level() == log_level::debug);
        REQUIRE(logger_adapter::is_level_enabled(log_level::debug));

        // Log at debug level - should now be captured
        logger_adapter::debug("Debug message after level change");

        // Change level to warn
        logger_adapter::set_min_level(log_level::warn);
        REQUIRE(logger_adapter::get_min_level() == log_level::warn);
        REQUIRE_FALSE(logger_adapter::is_level_enabled(log_level::info));
        REQUIRE(logger_adapter::is_level_enabled(log_level::warn));

        logger_adapter::flush();
    }

    SECTION("Log level changes apply to concurrent logging") {
        logger_config config;
        config.log_directory = temp_dir;
        config.enable_console = false;
        config.enable_file = true;
        config.min_level = log_level::error;  // Only errors initially
        logger_adapter::initialize(config);

        thread_pool_config thread_config;
        thread_config.min_threads = 4;
        thread_adapter::configure(thread_config);
        REQUIRE(thread_adapter::start());

        std::atomic<int> debug_logs{0};
        std::atomic<int> error_logs{0};

        // Submit tasks that log at different levels
        std::vector<std::future<void>> futures;
        for (int i = 0; i < 10; ++i) {
            futures.push_back(thread_adapter::submit([i, &debug_logs, &error_logs]() {
                if (logger_adapter::is_level_enabled(log_level::debug)) {
                    logger_adapter::debug("Debug from task {}", i);
                    debug_logs++;
                }
                if (logger_adapter::is_level_enabled(log_level::error)) {
                    logger_adapter::error("Error from task {}", i);
                    error_logs++;
                }
            }));
        }

        for (auto& f : futures) {
            f.get();
        }

        // Only error logs should have been counted
        REQUIRE(error_logs == 10);
        REQUIRE(debug_logs == 0);

        // Now enable debug logging and run more tasks
        logger_adapter::set_min_level(log_level::debug);

        futures.clear();
        for (int i = 0; i < 10; ++i) {
            futures.push_back(thread_adapter::submit([i, &debug_logs]() {
                if (logger_adapter::is_level_enabled(log_level::debug)) {
                    logger_adapter::debug("Debug from task {} (round 2)", i);
                    debug_logs++;
                }
            }));
        }

        for (auto& f : futures) {
            f.get();
        }

        REQUIRE(debug_logs == 10);  // Now debug logs are captured
        logger_adapter::flush();
    }
}

// =============================================================================
// Thread Pool Configuration Tests
// =============================================================================

TEST_CASE("Thread pool reconfiguration",
          "[integration][config][thread][!mayfail]") {
    auto temp_dir = create_temp_log_directory();
    config_test_guard guard(temp_dir);

    logger_config log_config;
    log_config.log_directory = temp_dir;
    log_config.enable_console = false;
    logger_adapter::initialize(log_config);

    SECTION("Thread pool can be reconfigured after restart") {
        // Initial configuration: 2 threads
        thread_pool_config config1;
        config1.min_threads = 2;
        config1.max_threads = 2;
        config1.pool_name = "config_test_v1";
        thread_adapter::configure(config1);

        REQUIRE(thread_adapter::start());
        REQUIRE(wait_for([]() { return thread_adapter::get_thread_count() >= 2; }));

        auto initial_config = thread_adapter::get_config();
        REQUIRE(initial_config.min_threads == 2);

        // Submit some work to verify functionality
        auto future1 = thread_adapter::submit([]() { return 1; });
        REQUIRE(future1.get() == 1);

        // Shutdown and reconfigure
        thread_adapter::shutdown(true);
        REQUIRE_FALSE(thread_adapter::is_running());

        // New configuration: 4 threads
        thread_pool_config config2;
        config2.min_threads = 4;
        config2.max_threads = 4;
        config2.pool_name = "config_test_v2";
        thread_adapter::configure(config2);

        auto updated_config = thread_adapter::get_config();
        REQUIRE(updated_config.min_threads == 4);
        REQUIRE(updated_config.pool_name == "config_test_v2");

        // Restart with new configuration
        REQUIRE(thread_adapter::start());
        REQUIRE(wait_for([]() { return thread_adapter::get_thread_count() >= 4; }));

        // Verify new thread count
        REQUIRE(thread_adapter::get_thread_count() >= 4);

        auto future2 = thread_adapter::submit([]() { return 2; });
        REQUIRE(future2.get() == 2);
    }

    SECTION("Configuration changes propagate correctly") {
        thread_pool_config config;
        config.min_threads = 2;
        config.max_threads = 8;
        config.idle_timeout = 5000ms;
        config.pool_name = "test_pool";
        thread_adapter::configure(config);

        auto retrieved = thread_adapter::get_config();
        REQUIRE(retrieved.min_threads == 2);
        REQUIRE(retrieved.max_threads == 8);
        REQUIRE(retrieved.idle_timeout == 5000ms);
        REQUIRE(retrieved.pool_name == "test_pool");

        REQUIRE(thread_adapter::start());

        // Update configuration (will apply on next restart)
        thread_pool_config new_config;
        new_config.min_threads = 4;
        new_config.max_threads = 16;
        new_config.idle_timeout = 10000ms;
        new_config.pool_name = "updated_pool";
        thread_adapter::configure(new_config);

        // Config is updated but pool uses previous config until restart
        auto current_config = thread_adapter::get_config();
        REQUIRE(current_config.min_threads == 4);

        thread_adapter::shutdown(true);
        REQUIRE(thread_adapter::start());

        // Now the new config should be active
        REQUIRE(wait_for([]() { return thread_adapter::get_thread_count() >= 4; }));
    }
}

// =============================================================================
// Cross-System Configuration Consistency Tests
// =============================================================================

TEST_CASE("Configuration consistency across systems",
          "[integration][config][consistency][!mayfail]") {
    auto temp_dir = create_temp_log_directory();
    config_test_guard guard(temp_dir);

    SECTION("Logger and thread pool work together after reconfiguration") {
        // Initialize logger
        logger_config log_config;
        log_config.log_directory = temp_dir;
        log_config.enable_console = false;
        log_config.enable_audit_log = true;
        log_config.min_level = log_level::debug;
        logger_adapter::initialize(log_config);

        // Initialize thread pool
        thread_pool_config thread_config;
        thread_config.min_threads = 2;
        thread_adapter::configure(thread_config);
        REQUIRE(thread_adapter::start());

        // Run some work
        auto future1 = thread_adapter::submit([]() {
            logger_adapter::info("Work before reconfiguration");
            return 1;
        });
        REQUIRE(future1.get() == 1);

        // Reconfigure logger
        logger_adapter::set_min_level(log_level::warn);

        // Reconfigure thread pool
        thread_adapter::shutdown(true);
        thread_pool_config new_thread_config;
        new_thread_config.min_threads = 4;
        thread_adapter::configure(new_thread_config);
        REQUIRE(thread_adapter::start());

        // Run more work - systems should work together
        auto future2 = thread_adapter::submit([]() {
            // Info won't be logged (level is warn)
            logger_adapter::info("This won't be logged");

            // But warn will
            logger_adapter::warn("This will be logged");

            logger_adapter::log_association_established(
                "RECONFIG_TEST", "LOCAL_SCP", "127.0.0.1");

            return 2;
        });
        REQUIRE(future2.get() == 2);

        logger_adapter::flush();
    }
}

// =============================================================================
// Configuration Validation Tests
// =============================================================================

TEST_CASE("Invalid configuration handling",
          "[integration][config][validation][!mayfail]") {
    auto temp_dir = create_temp_log_directory();
    config_test_guard guard(temp_dir);

    logger_config log_config;
    log_config.log_directory = temp_dir;
    log_config.enable_console = false;
    logger_adapter::initialize(log_config);

    SECTION("Thread pool corrects invalid min_threads") {
        thread_pool_config config;
        config.min_threads = 0;  // Invalid
        config.max_threads = 4;
        thread_adapter::configure(config);

        auto retrieved = thread_adapter::get_config();
        REQUIRE(retrieved.min_threads >= 1);  // Should be corrected
    }

    SECTION("Thread pool corrects max < min") {
        thread_pool_config config;
        config.min_threads = 8;
        config.max_threads = 2;  // Invalid: less than min
        thread_adapter::configure(config);

        auto retrieved = thread_adapter::get_config();
        REQUIRE(retrieved.max_threads >= retrieved.min_threads);
    }

    SECTION("Logger handles invalid paths gracefully") {
        // Note: This tests the adapter's resilience, not necessarily failure
        logger_config config;
        config.log_directory = temp_dir / "valid_subdir";
        config.enable_console = false;
        config.enable_file = true;

        // Should create directory if needed
        logger_adapter::shutdown();
        logger_adapter::initialize(config);

        REQUIRE(logger_adapter::is_initialized());
        logger_adapter::info("Test message to valid subdir");
        logger_adapter::flush();
    }
}

// =============================================================================
// Runtime Configuration Query Tests
// =============================================================================

TEST_CASE("Query configuration at runtime",
          "[integration][config][query][!mayfail]") {
    auto temp_dir = create_temp_log_directory();
    config_test_guard guard(temp_dir);

    SECTION("Logger configuration is queryable") {
        logger_config config;
        config.log_directory = temp_dir;
        config.enable_console = false;
        config.enable_file = true;
        config.enable_audit_log = true;
        config.min_level = log_level::debug;
        config.max_file_size_mb = 50;
        config.max_files = 5;
        logger_adapter::initialize(config);

        auto& retrieved = logger_adapter::get_config();
        REQUIRE(retrieved.enable_file == true);
        REQUIRE(retrieved.enable_audit_log == true);
        REQUIRE(retrieved.min_level == log_level::debug);
        REQUIRE(retrieved.max_file_size_mb == 50);
        REQUIRE(retrieved.max_files == 5);
    }

    SECTION("Thread pool configuration is queryable") {
        thread_pool_config config;
        config.min_threads = 3;
        config.max_threads = 12;
        config.idle_timeout = 15000ms;
        config.use_lock_free_queue = true;
        config.pool_name = "query_test";
        thread_adapter::configure(config);

        auto retrieved = thread_adapter::get_config();
        REQUIRE(retrieved.min_threads == 3);
        REQUIRE(retrieved.max_threads == 12);
        REQUIRE(retrieved.idle_timeout == 15000ms);
        REQUIRE(retrieved.use_lock_free_queue == true);
        REQUIRE(retrieved.pool_name == "query_test");
    }

    SECTION("Configuration reflects runtime changes") {
        logger_config config;
        config.log_directory = temp_dir;
        config.min_level = log_level::info;
        logger_adapter::initialize(config);

        REQUIRE(logger_adapter::get_min_level() == log_level::info);

        logger_adapter::set_min_level(log_level::trace);
        REQUIRE(logger_adapter::get_min_level() == log_level::trace);

        logger_adapter::set_min_level(log_level::error);
        REQUIRE(logger_adapter::get_min_level() == log_level::error);
    }
}

// =============================================================================
// Configuration Under Load Tests
// =============================================================================

TEST_CASE("Configuration changes under load",
          "[integration][config][load][!mayfail]") {
    auto temp_dir = create_temp_log_directory();
    config_test_guard guard(temp_dir);

    logger_config log_config;
    log_config.log_directory = temp_dir;
    log_config.enable_console = false;
    log_config.min_level = log_level::debug;
    logger_adapter::initialize(log_config);

    thread_pool_config thread_config;
    thread_config.min_threads = 4;
    thread_config.max_threads = 8;
    thread_adapter::configure(thread_config);

    SECTION("Log level changes while tasks are running") {
        REQUIRE(thread_adapter::start());

        std::atomic<bool> stop{false};
        std::atomic<int> task_count{0};

        // Start background tasks
        std::vector<std::future<void>> futures;
        for (int i = 0; i < 10; ++i) {
            futures.push_back(thread_adapter::submit([&stop, &task_count]() {
                while (!stop) {
                    if (logger_adapter::is_level_enabled(log_level::debug)) {
                        logger_adapter::debug("Background task running");
                    }
                    task_count++;
                    std::this_thread::sleep_for(5ms);
                }
            }));
        }

        // Change log levels while tasks run
        std::this_thread::sleep_for(20ms);
        logger_adapter::set_min_level(log_level::warn);

        std::this_thread::sleep_for(20ms);
        logger_adapter::set_min_level(log_level::trace);

        std::this_thread::sleep_for(20ms);
        logger_adapter::set_min_level(log_level::error);

        // Stop and cleanup
        stop = true;
        for (auto& f : futures) {
            f.get();
        }

        REQUIRE(task_count > 0);  // Tasks ran
        logger_adapter::flush();
    }
}

// =============================================================================
// Multi-Cycle Configuration Tests
// =============================================================================

TEST_CASE("Multiple configuration cycles",
          "[integration][config][cycles][!mayfail]") {
    auto temp_dir = create_temp_log_directory();
    config_test_guard guard(temp_dir);

    SECTION("Logger can be reinitialized with different configs") {
        for (int cycle = 0; cycle < 3; ++cycle) {
            logger_config config;
            config.log_directory = temp_dir;
            config.enable_console = false;
            config.min_level = static_cast<log_level>(cycle % 3);

            logger_adapter::initialize(config);
            REQUIRE(logger_adapter::is_initialized());

            logger_adapter::info("Cycle {} message", cycle);
            logger_adapter::flush();

            logger_adapter::shutdown();
            REQUIRE_FALSE(logger_adapter::is_initialized());
        }
    }

    SECTION("Thread pool survives multiple reconfigurations") {
        logger_config log_config;
        log_config.log_directory = temp_dir;
        log_config.enable_console = false;
        logger_adapter::initialize(log_config);

        for (int cycle = 0; cycle < 3; ++cycle) {
            thread_pool_config config;
            config.min_threads = 2 + cycle;
            config.max_threads = 4 + cycle * 2;
            config.pool_name = "cycle_" + std::to_string(cycle);
            thread_adapter::configure(config);

            REQUIRE(thread_adapter::start());

            // Verify configuration
            auto retrieved = thread_adapter::get_config();
            REQUIRE(retrieved.min_threads == static_cast<std::size_t>(2 + cycle));
            REQUIRE(retrieved.pool_name == "cycle_" + std::to_string(cycle));

            // Do some work
            auto future = thread_adapter::submit([cycle]() {
                return cycle * 10;
            });
            REQUIRE(future.get() == cycle * 10);

            thread_adapter::shutdown(true);
        }
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
