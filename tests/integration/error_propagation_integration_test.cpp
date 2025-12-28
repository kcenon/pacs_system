/**
 * @file error_propagation_integration_test.cpp
 * @brief Integration tests for error propagation across systems
 *
 * This file contains cross-system integration tests verifying error
 * propagation and recovery patterns involving Result<T>, RAII cleanup,
 * and monitoring system interactions.
 *
 * @note This file uses the deprecated thread_adapter API for backward
 *       compatibility testing.
 *
 * Part of Issue #390 - Enhance cross-system integration tests
 * Addresses Issue #392 - Error Propagation Chain integration test
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
#include <pacs/integration/network_adapter.hpp>
#include <pacs/integration/thread_adapter.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <exception>
#include <filesystem>
#include <future>
#include <memory>
#include <stdexcept>
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
struct error_test_guard {
    std::filesystem::path log_dir;

    explicit error_test_guard(const std::filesystem::path& dir) : log_dir(dir) {
        std::filesystem::create_directories(log_dir);
    }

    ~error_test_guard() {
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
    auto temp_dir = std::filesystem::temp_directory_path() / "pacs_error_test";
    std::filesystem::create_directories(temp_dir);
    return temp_dir;
}

/**
 * @brief Simulated error types for testing
 */
enum class simulated_error {
    none,
    network_timeout,
    connection_refused,
    protocol_error,
    storage_full,
    invalid_data
};

/**
 * @brief RAII resource that tracks cleanup
 */
class tracked_resource {
public:
    explicit tracked_resource(std::atomic<int>& cleanup_counter)
        : cleanup_counter_(cleanup_counter), active_(true) {}

    ~tracked_resource() {
        if (active_) {
            cleanup_counter_++;
        }
    }

    tracked_resource(const tracked_resource&) = delete;
    tracked_resource& operator=(const tracked_resource&) = delete;

    tracked_resource(tracked_resource&& other) noexcept
        : cleanup_counter_(other.cleanup_counter_), active_(other.active_) {
        other.active_ = false;
    }

    void release() { active_ = false; }

private:
    std::atomic<int>& cleanup_counter_;
    bool active_;
};

/**
 * @brief Simulate an operation that may fail
 */
auto simulate_operation(simulated_error error_type) -> Result<std::string> {
    switch (error_type) {
        case simulated_error::none:
            return std::string("success");
        case simulated_error::network_timeout:
            return error_info(-1, "Network timeout", "network");
        case simulated_error::connection_refused:
            return error_info(-2, "Connection refused", "network");
        case simulated_error::protocol_error:
            return error_info(-3, "Protocol error", "dicom");
        case simulated_error::storage_full:
            return error_info(-4, "Storage full", "storage");
        case simulated_error::invalid_data:
            return error_info(-5, "Invalid DICOM data", "parser");
    }
    return error_info(-99, "Unknown error", "unknown");
}

}  // namespace

// =============================================================================
// Result<T> Error Propagation Tests
// =============================================================================

TEST_CASE("Result<T> error propagation across thread pool",
          "[integration][error][result][!mayfail]") {
    auto temp_dir = create_temp_log_directory();
    error_test_guard guard(temp_dir);

    logger_config log_config;
    log_config.log_directory = temp_dir;
    log_config.enable_console = false;
    log_config.enable_audit_log = true;
    logger_adapter::initialize(log_config);

    thread_pool_config thread_config;
    thread_config.min_threads = 2;
    thread_config.max_threads = 4;
    thread_adapter::configure(thread_config);

    SECTION("Error result propagates through future") {
        REQUIRE(thread_adapter::start());

        auto future = thread_adapter::submit([]() {
            auto result = simulate_operation(simulated_error::network_timeout);
            return result;
        });

        auto result = future.get();
        REQUIRE(result.is_err());
        REQUIRE(result.error().message == "Network timeout");
        REQUIRE(result.error().module == "network");
    }

    SECTION("Success result propagates through future") {
        REQUIRE(thread_adapter::start());

        auto future = thread_adapter::submit([]() {
            auto result = simulate_operation(simulated_error::none);
            return result;
        });

        auto result = future.get();
        REQUIRE(result.is_ok());
        REQUIRE(result.value() == "success");
    }

    SECTION("Mixed results with error logging") {
        REQUIRE(thread_adapter::start());

        std::atomic<int> success_count{0};
        std::atomic<int> error_count{0};

        std::vector<std::future<void>> futures;

        // Submit tasks with various outcomes
        simulated_error errors[] = {
            simulated_error::none,
            simulated_error::network_timeout,
            simulated_error::none,
            simulated_error::storage_full,
            simulated_error::none,
            simulated_error::protocol_error};

        for (auto error : errors) {
            futures.push_back(thread_adapter::submit(
                [error, &success_count, &error_count]() {
                    auto result = simulate_operation(error);

                    if (result.is_ok()) {
                        success_count++;
                    } else {
                        error_count++;
                        logger_adapter::error("Operation failed: {} ({})",
                                              result.error().message,
                                              result.error().module);
                    }
                }));
        }

        for (auto& f : futures) {
            f.get();
        }

        REQUIRE(success_count == 3);
        REQUIRE(error_count == 3);
        logger_adapter::flush();
    }
}

// =============================================================================
// RAII Cleanup Tests
// =============================================================================

TEST_CASE("RAII cleanup during error scenarios",
          "[integration][error][raii][!mayfail]") {
    auto temp_dir = create_temp_log_directory();
    error_test_guard guard(temp_dir);

    logger_config log_config;
    log_config.log_directory = temp_dir;
    log_config.enable_console = false;
    logger_adapter::initialize(log_config);

    thread_pool_config thread_config;
    thread_config.min_threads = 2;
    thread_adapter::configure(thread_config);

    SECTION("Resources cleaned up on exception") {
        REQUIRE(thread_adapter::start());

        std::atomic<int> cleanup_count{0};
        std::atomic<int> exception_caught{0};

        auto future = thread_adapter::submit([&cleanup_count, &exception_caught]()
                                                 -> int {
            tracked_resource resource(cleanup_count);

            try {
                // Simulate work that throws
                throw std::runtime_error("Simulated failure");
            } catch (const std::exception& e) {
                exception_caught++;
                logger_adapter::error("Exception caught: {}", e.what());
                throw;  // Re-throw to test RAII cleanup
            }

            return 0;  // Never reached
        });

        REQUIRE_THROWS_AS(future.get(), std::runtime_error);
        REQUIRE(exception_caught == 1);
        REQUIRE(cleanup_count == 1);  // RAII cleanup occurred
    }

    SECTION("Multiple resources cleaned up in reverse order") {
        REQUIRE(thread_adapter::start());

        std::atomic<int> total_cleanup{0};

        auto future = thread_adapter::submit([&total_cleanup]() {
            tracked_resource r1(total_cleanup);
            tracked_resource r2(total_cleanup);
            tracked_resource r3(total_cleanup);

            throw std::runtime_error("Simulated failure");
        });

        REQUIRE_THROWS(future.get());
        REQUIRE(total_cleanup == 3);  // All resources cleaned up
    }

    SECTION("Resources cleaned up on normal exit") {
        REQUIRE(thread_adapter::start());

        std::atomic<int> cleanup_count{0};

        auto future = thread_adapter::submit([&cleanup_count]() {
            tracked_resource resource(cleanup_count);
            // Normal completion
            return 42;
        });

        REQUIRE(future.get() == 42);
        REQUIRE(cleanup_count == 1);
    }
}

// =============================================================================
// Exception Propagation Tests
// =============================================================================

TEST_CASE("Exception propagation through thread pool",
          "[integration][error][exception][!mayfail]") {
    auto temp_dir = create_temp_log_directory();
    error_test_guard guard(temp_dir);

    logger_config log_config;
    log_config.log_directory = temp_dir;
    log_config.enable_console = false;
    logger_adapter::initialize(log_config);

    thread_pool_config thread_config;
    thread_config.min_threads = 2;
    thread_adapter::configure(thread_config);

    SECTION("std::runtime_error propagates") {
        REQUIRE(thread_adapter::start());

        auto future = thread_adapter::submit([]() -> int {
            throw std::runtime_error("Test runtime error");
        });

        REQUIRE_THROWS_AS(future.get(), std::runtime_error);
    }

    SECTION("std::logic_error propagates") {
        REQUIRE(thread_adapter::start());

        auto future = thread_adapter::submit([]() -> int {
            throw std::logic_error("Test logic error");
        });

        REQUIRE_THROWS_AS(future.get(), std::logic_error);
    }

    SECTION("Custom exception propagates") {
        REQUIRE(thread_adapter::start());

        class custom_exception : public std::exception {
        public:
            const char* what() const noexcept override {
                return "Custom exception";
            }
        };

        auto future = thread_adapter::submit([]() -> int {
            throw custom_exception();
        });

        REQUIRE_THROWS_AS(future.get(), custom_exception);
    }

    SECTION("Thread pool continues after exception") {
        REQUIRE(thread_adapter::start());

        // First task throws
        auto failing_future = thread_adapter::submit([]() -> int {
            throw std::runtime_error("Expected failure");
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
// Error Recovery Tests
// =============================================================================

TEST_CASE("Error recovery and retry patterns",
          "[integration][error][recovery][!mayfail]") {
    auto temp_dir = create_temp_log_directory();
    error_test_guard guard(temp_dir);

    logger_config log_config;
    log_config.log_directory = temp_dir;
    log_config.enable_console = false;
    logger_adapter::initialize(log_config);

    thread_pool_config thread_config;
    thread_config.min_threads = 2;
    thread_adapter::configure(thread_config);

    SECTION("Retry on transient failure") {
        REQUIRE(thread_adapter::start());

        std::atomic<int> attempt_count{0};
        constexpr int succeed_on_attempt = 2;

        auto future = thread_adapter::submit(
            [&attempt_count]() {
                constexpr int max_retries = 3;
                constexpr int succeed_threshold = 2;
                Result<std::string> result = error_info("transient error");

                for (int retry = 0; retry < max_retries && result.is_err(); ++retry) {
                    attempt_count++;

                    if (attempt_count >= succeed_threshold) {
                        result = std::string("success after retry");
                    } else {
                        logger_adapter::warn("Attempt {} failed, retrying...",
                                             attempt_count.load());
                        std::this_thread::sleep_for(10ms);
                    }
                }

                return result;
            });

        auto result = future.get();
        REQUIRE(result.is_ok());
        REQUIRE(attempt_count == succeed_on_attempt);
        logger_adapter::flush();
    }

    SECTION("Graceful degradation on persistent failure") {
        REQUIRE(thread_adapter::start());

        std::atomic<bool> used_fallback{false};

        auto future = thread_adapter::submit([&used_fallback]() {
            // Primary operation fails
            auto primary_result =
                simulate_operation(simulated_error::connection_refused);

            if (primary_result.is_err()) {
                logger_adapter::warn("Primary operation failed: {}",
                                     primary_result.error().message);

                // Use fallback
                used_fallback = true;
                return std::string("fallback_result");
            }

            return primary_result.value();
        });

        auto result = future.get();
        REQUIRE(result == "fallback_result");
        REQUIRE(used_fallback);
        logger_adapter::flush();
    }
}

// =============================================================================
// Error Logging and Audit Tests
// =============================================================================

TEST_CASE("Error events logged to audit trail",
          "[integration][error][audit][!mayfail]") {
    auto temp_dir = create_temp_log_directory();
    error_test_guard guard(temp_dir);

    logger_config log_config;
    log_config.log_directory = temp_dir;
    log_config.enable_console = false;
    log_config.enable_audit_log = true;
    log_config.audit_log_format = "json";
    logger_adapter::initialize(log_config);

    thread_pool_config thread_config;
    thread_config.min_threads = 2;
    thread_adapter::configure(thread_config);

    SECTION("Security events logged on access errors") {
        REQUIRE(thread_adapter::start());

        auto future = thread_adapter::submit([]() {
            // Simulate access denied error
            logger_adapter::log_security_event(
                security_event_type::access_denied,
                "Access to protected study denied",
                "unauthorized_user");

            return true;
        });

        REQUIRE(future.get());
        logger_adapter::flush();

        auto audit_path = temp_dir / "audit.json";
        REQUIRE(wait_for([&audit_path]() {
            return std::filesystem::exists(audit_path);
        }));
    }

    SECTION("C-STORE failure logged with status") {
        REQUIRE(thread_adapter::start());

        auto future = thread_adapter::submit([]() {
            logger_adapter::log_c_store_received(
                "FAILED_MODALITY", "PATIENT001", "1.2.3.4", "1.2.3.4.5.6",
                storage_status::out_of_resources);
        });

        future.get();
        logger_adapter::flush();

        auto audit_path = temp_dir / "audit.json";
        REQUIRE(wait_for([&audit_path]() {
            return std::filesystem::exists(audit_path);
        }));
    }

    SECTION("C-MOVE failure logged with status") {
        REQUIRE(thread_adapter::start());

        auto future = thread_adapter::submit([]() {
            logger_adapter::log_c_move_executed(
                "REQUESTING_AE", "UNKNOWN_DEST", "1.2.3.4", 0,
                move_status::refused_move_destination_unknown);
        });

        future.get();
        logger_adapter::flush();

        auto audit_path = temp_dir / "audit.json";
        REQUIRE(wait_for([&audit_path]() {
            return std::filesystem::exists(audit_path);
        }));
    }
}

// =============================================================================
// Nested Error Handling Tests
// =============================================================================

TEST_CASE("Nested operations with error handling",
          "[integration][error][nested][!mayfail]") {
    auto temp_dir = create_temp_log_directory();
    error_test_guard guard(temp_dir);

    logger_config log_config;
    log_config.log_directory = temp_dir;
    log_config.enable_console = false;
    logger_adapter::initialize(log_config);

    thread_pool_config thread_config;
    thread_config.min_threads = 4;
    thread_adapter::configure(thread_config);

    SECTION("Parent handles child task error") {
        REQUIRE(thread_adapter::start());

        std::atomic<int> child_errors{0};

        auto parent_future = thread_adapter::submit([&child_errors]() {
            std::vector<std::future<Result<int>>> child_futures;

            for (int i = 0; i < 5; ++i) {
                child_futures.push_back(thread_adapter::submit([i]() -> Result<int> {
                    if (i % 2 == 0) {
                        return Result<int>(error_info("child error"));
                    }
                    return Result<int>(i * 10);
                }));
            }

            int total = 0;
            for (auto& f : child_futures) {
                auto result = f.get();
                if (result.is_ok()) {
                    total += result.value();
                } else {
                    child_errors++;
                }
            }

            return total;
        });

        auto result = parent_future.get();
        REQUIRE(child_errors == 3);    // 0, 2, 4 failed
        REQUIRE(result == 10 + 30);    // 1*10 + 3*10
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
