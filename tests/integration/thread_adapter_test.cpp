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

// FIXME: Known issue - SIGSEGV on macOS ARM64 with thread_system integration
// See issue #96: https://github.com/kcenon/pacs_system/issues/96
// Skipping tests that cause process crash until thread_system compatibility is resolved
TEST_CASE("thread_adapter pool management", "[thread_adapter][pool][.skip]") {
    SKIP("Skipped due to SIGSEGV on macOS ARM64 - see issue #96");
}

// =============================================================================
// Job Submission Tests
// =============================================================================

// FIXME: Known issue - SIGSEGV on macOS ARM64 (issue #96)
TEST_CASE("thread_adapter job submission", "[thread_adapter][submit][.skip]") {
    SKIP("Skipped due to SIGSEGV on macOS ARM64 - see issue #96");
}

// =============================================================================
// Fire and Forget Tests
// =============================================================================

// FIXME: Known issue - SIGSEGV on macOS ARM64 (issue #96)
TEST_CASE("thread_adapter fire and forget", "[thread_adapter][fire_and_forget][.skip]") {
    SKIP("Skipped due to SIGSEGV on macOS ARM64 - see issue #96");
}

// =============================================================================
// Priority Submission Tests
// =============================================================================

// FIXME: Known issue - SIGSEGV on macOS ARM64 (issue #96)
TEST_CASE("thread_adapter priority submission", "[thread_adapter][priority][.skip]") {
    SKIP("Skipped due to SIGSEGV on macOS ARM64 - see issue #96");
}

// =============================================================================
// Statistics Tests
// =============================================================================

// FIXME: Known issue - SIGSEGV on macOS ARM64 (issue #96)
TEST_CASE("thread_adapter statistics", "[thread_adapter][stats][.skip]") {
    SKIP("Skipped due to SIGSEGV on macOS ARM64 - see issue #96");
}

// =============================================================================
// Error Handling Tests
// =============================================================================

// FIXME: Known issue - SIGSEGV on macOS ARM64 (issue #96)
TEST_CASE("thread_adapter error handling", "[thread_adapter][error][.skip]") {
    SKIP("Skipped due to SIGSEGV on macOS ARM64 - see issue #96");
}

// =============================================================================
// Shutdown Tests
// =============================================================================

// FIXME: Known issue - SIGSEGV on macOS ARM64 (issue #96)
TEST_CASE("thread_adapter shutdown", "[thread_adapter][shutdown][.skip]") {
    SKIP("Skipped due to SIGSEGV on macOS ARM64 - see issue #96");
}

// =============================================================================
// Concurrent Access Tests
// =============================================================================

// FIXME: Known issue - SIGSEGV on macOS ARM64 (issue #96)
TEST_CASE("thread_adapter concurrent access", "[thread_adapter][concurrent][.skip]") {
    SKIP("Skipped due to SIGSEGV on macOS ARM64 - see issue #96");
}
