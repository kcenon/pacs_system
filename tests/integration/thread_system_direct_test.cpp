/**
 * @file thread_system_direct_test.cpp
 * @brief Direct tests for thread_system to isolate ARM64 stability issues
 *
 * This file tests thread_system directly (without thread_adapter) to determine
 * if stability issues are in thread_system itself or in pacs_system's usage.
 *
 * Related Issues:
 * - pacs_system #155: Verify thread_system stability
 * - thread_system #223: SIGILL/SIGSEGV on macOS ARM64
 *
 * Test Patterns:
 * 1. Direct thread_pool usage (same as thread_system's own tests)
 * 2. Manual worker management (pattern used by thread_adapter)
 * 3. Batch enqueue pattern (exact pattern that crashes)
 */

// Suppress deprecated warnings from thread_system headers (logger_interface is deprecated)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

#include <catch2/catch_test_macros.hpp>

#include <kcenon/thread/core/thread_pool.h>
#include <kcenon/thread/core/thread_worker.h>
#include <kcenon/thread/interfaces/thread_context.h>

#pragma clang diagnostic pop

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

// =============================================================================
// Test 1: Direct thread_pool usage (same as thread_system's own tests)
// =============================================================================

TEST_CASE("direct thread_pool basic usage", "[thread_system][direct][!mayfail]") {
    SECTION("Create and start pool without manual workers") {
        // This pattern is used in thread_system's own tests
        kcenon::thread::thread_context context;
        auto pool = std::make_shared<kcenon::thread::thread_pool>("test_pool", context);

        // Start pool (should auto-create workers internally)
        auto start_result = pool->start();
        INFO("start() result: " << (start_result.is_err() ? "error" : "success"));

        REQUIRE_FALSE(start_result.is_err());
        REQUIRE(pool->is_running());

        // Submit a simple task
        std::atomic<bool> executed{false};
        auto submit_future = pool->submit([&executed]() { executed = true; });
        REQUIRE(submit_future.valid());

        // Wait for execution
        auto start_time = std::chrono::steady_clock::now();
        while (!executed && std::chrono::steady_clock::now() - start_time < 5s) {
            std::this_thread::sleep_for(10ms);
        }

        REQUIRE(executed);

        // Stop pool
        auto stop_result = pool->stop();
        REQUIRE_FALSE(stop_result.is_err());
    }
}

// =============================================================================
// Test 2: Manual worker management (thread_adapter pattern)
// =============================================================================

TEST_CASE("manual worker batch enqueue - thread_adapter pattern",
          "[thread_system][manual][batch][!mayfail]") {
    SECTION("Create pool then batch enqueue workers") {
        // This is the exact pattern used by thread_adapter::start()
        kcenon::thread::thread_context context;
        auto pool = std::make_shared<kcenon::thread::thread_pool>("adapter_pattern_pool", context);

        // Create workers manually (like thread_adapter does)
        constexpr std::size_t worker_count = 4;
        std::vector<std::unique_ptr<kcenon::thread::thread_worker>> workers;
        workers.reserve(worker_count);

        for (std::size_t i = 0; i < worker_count; ++i) {
            workers.push_back(
                std::make_unique<kcenon::thread::thread_worker>(false, context));
        }

        INFO("Created " << workers.size() << " workers");

        // Batch enqueue (this is where thread_adapter does enqueue_batch)
        auto enqueue_result = pool->enqueue_batch(std::move(workers));
        INFO("enqueue_batch() result: " << (enqueue_result.is_err() ? "error" : "success"));

        REQUIRE_FALSE(enqueue_result.is_err());

        // Start the pool (CRASH POINT on ARM64)
        INFO("About to call pool->start()...");
        auto start_result = pool->start();
        INFO("start() completed");
        INFO("start() result: " << (start_result.is_err() ? "error" : "success"));

        REQUIRE_FALSE(start_result.is_err());
        REQUIRE(pool->is_running());

        // Submit task
        std::atomic<bool> task_executed{false};
        auto submit_future = pool->submit([&task_executed]() { task_executed = true; });
        REQUIRE(submit_future.valid());

        // Wait for execution
        auto start_time = std::chrono::steady_clock::now();
        while (!task_executed && std::chrono::steady_clock::now() - start_time < 5s) {
            std::this_thread::sleep_for(10ms);
        }

        REQUIRE(task_executed);

        // Stop pool
        pool->stop();
    }
}

// =============================================================================
// Test 3: Individual worker enqueue (alternative pattern)
// =============================================================================

TEST_CASE("individual worker enqueue", "[thread_system][manual][individual][!mayfail]") {
    SECTION("Create pool then enqueue workers individually") {
        kcenon::thread::thread_context context;
        auto pool = std::make_shared<kcenon::thread::thread_pool>("individual_enqueue_pool", context);

        // Enqueue workers one by one
        constexpr std::size_t worker_count = 4;
        for (std::size_t i = 0; i < worker_count; ++i) {
            auto worker = std::make_unique<kcenon::thread::thread_worker>(false, context);
            auto enqueue_result = pool->enqueue(std::move(worker));
            INFO("enqueue() worker " << i << " result: "
                                     << (enqueue_result.is_err() ? "error" : "success"));
            REQUIRE_FALSE(enqueue_result.is_err());
        }

        // Start the pool
        auto start_result = pool->start();
        REQUIRE_FALSE(start_result.is_err());
        REQUIRE(pool->is_running());

        // Submit task
        std::atomic<int> counter{0};
        for (int i = 0; i < 10; ++i) {
            (void)pool->submit([&counter]() { counter++; });
        }

        // Wait for all tasks
        auto start_time = std::chrono::steady_clock::now();
        while (counter < 10 && std::chrono::steady_clock::now() - start_time < 5s) {
            std::this_thread::sleep_for(10ms);
        }

        REQUIRE(counter == 10);

        pool->stop();
    }
}

// =============================================================================
// Test 4: Stress test with repeated create/destroy
// =============================================================================

TEST_CASE("repeated pool lifecycle", "[thread_system][lifecycle][stress][!mayfail]") {
    SECTION("Create and destroy pools repeatedly") {
        constexpr int cycles = 5;

        for (int cycle = 0; cycle < cycles; ++cycle) {
            INFO("Cycle " << cycle + 1 << " of " << cycles);

            kcenon::thread::thread_context context;
            auto pool =
                std::make_shared<kcenon::thread::thread_pool>("lifecycle_pool_" + std::to_string(cycle), context);

            // Create and enqueue workers
            std::vector<std::unique_ptr<kcenon::thread::thread_worker>> workers;
            for (int i = 0; i < 2; ++i) {
                workers.push_back(std::make_unique<kcenon::thread::thread_worker>(false, context));
            }

            auto enqueue_result = pool->enqueue_batch(std::move(workers));
            REQUIRE_FALSE(enqueue_result.is_err());

            // Start pool
            auto start_result = pool->start();
            REQUIRE_FALSE(start_result.is_err());

            // Submit a task
            std::atomic<bool> done{false};
            (void)pool->submit([&done]() { done = true; });

            // Wait
            auto start_time = std::chrono::steady_clock::now();
            while (!done && std::chrono::steady_clock::now() - start_time < 2s) {
                std::this_thread::sleep_for(10ms);
            }

            REQUIRE(done);

            // Stop pool
            pool->stop();

            // Allow cleanup
            std::this_thread::sleep_for(50ms);
        }
    }
}

// =============================================================================
// Test 5: Memory alignment verification
// =============================================================================

TEST_CASE("memory alignment verification", "[thread_system][alignment]") {
    SECTION("Critical structures have correct alignment") {
        // These are the static assertions added in thread_system PR #224
        static_assert(alignof(kcenon::thread::thread_worker) >= alignof(void*),
                      "thread_worker alignment too small");
        static_assert(alignof(kcenon::thread::thread_pool) >= alignof(void*),
                      "thread_pool alignment too small");

        // Additional alignment checks
        INFO("thread_worker alignment: " << alignof(kcenon::thread::thread_worker));
        INFO("thread_pool alignment: " << alignof(kcenon::thread::thread_pool));
        INFO("void* alignment: " << alignof(void*));

        REQUIRE(alignof(kcenon::thread::thread_worker) >= alignof(void*));
        REQUIRE(alignof(kcenon::thread::thread_pool) >= alignof(void*));
    }

    SECTION("Object instances are properly aligned") {
        kcenon::thread::thread_context context;
        auto worker = std::make_unique<kcenon::thread::thread_worker>(false, context);
        auto pool = std::make_shared<kcenon::thread::thread_pool>("align_test", context);

        // Check that pointers are aligned
        auto worker_ptr = reinterpret_cast<std::uintptr_t>(worker.get());
        auto pool_ptr = reinterpret_cast<std::uintptr_t>(pool.get());

        INFO("worker pointer: " << std::hex << worker_ptr);
        INFO("pool pointer: " << std::hex << pool_ptr);

        REQUIRE((worker_ptr % alignof(void*)) == 0);
        REQUIRE((pool_ptr % alignof(void*)) == 0);
    }
}

// =============================================================================
// Test 6: Platform info reporting
// =============================================================================

// Platform detection constants
namespace {

#if defined(__APPLE__)
constexpr const char* PLATFORM_NAME = "macOS";
#elif defined(__linux__)
constexpr const char* PLATFORM_NAME = "Linux";
#elif defined(_WIN32)
constexpr const char* PLATFORM_NAME = "Windows";
#else
constexpr const char* PLATFORM_NAME = "Unknown";
#endif

#if defined(__aarch64__) || defined(__arm64__)
constexpr const char* ARCH_NAME = "ARM64";
#elif defined(__x86_64__) || defined(_M_X64)
constexpr const char* ARCH_NAME = "x86_64";
#elif defined(__i386__) || defined(_M_IX86)
constexpr const char* ARCH_NAME = "x86";
#else
constexpr const char* ARCH_NAME = "Unknown";
#endif

#if defined(__clang__)
constexpr const char* COMPILER_NAME = "Clang";
constexpr int COMPILER_MAJOR = __clang_major__;
constexpr int COMPILER_MINOR = __clang_minor__;
#elif defined(__GNUC__)
constexpr const char* COMPILER_NAME = "GCC";
constexpr int COMPILER_MAJOR = __GNUC__;
constexpr int COMPILER_MINOR = __GNUC_MINOR__;
#elif defined(_MSC_VER)
constexpr const char* COMPILER_NAME = "MSVC";
constexpr int COMPILER_MAJOR = _MSC_VER / 100;
constexpr int COMPILER_MINOR = _MSC_VER % 100;
#else
constexpr const char* COMPILER_NAME = "Unknown";
constexpr int COMPILER_MAJOR = 0;
constexpr int COMPILER_MINOR = 0;
#endif

#ifdef USE_STD_JTHREAD
constexpr bool JTHREAD_ENABLED = true;
#else
constexpr bool JTHREAD_ENABLED = false;
#endif

}  // namespace

TEST_CASE("platform information", "[thread_system][platform][info]") {
    SECTION("Report platform details") {
        INFO("Platform: " << PLATFORM_NAME);
        INFO("Architecture: " << ARCH_NAME);
        INFO("Compiler: " << COMPILER_NAME << " " << COMPILER_MAJOR << "." << COMPILER_MINOR);
        INFO("C++ Standard: " << __cplusplus);
        INFO("jthread support: " << (JTHREAD_ENABLED ? "enabled" : "disabled"));
        INFO("Hardware concurrency: " << std::thread::hardware_concurrency());

        SUCCEED("Platform info logged");
    }
}
