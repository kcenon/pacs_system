/**
 * @file accept_worker_test.cpp
 * @brief Unit tests for accept_worker class
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "pacs/network/detail/accept_worker.hpp"

#include <atomic>
#include <chrono>
#include <thread>

using namespace pacs::network::detail;
using namespace std::chrono_literals;

TEST_CASE("accept_worker construction", "[accept_worker][construction]") {
    SECTION("constructs with valid parameters") {
        bool callback_invoked = false;
        accept_worker worker(
            11112,
            [&](uint64_t) { callback_invoked = true; }
        );

        REQUIRE(worker.port() == 11112);
        REQUIRE(worker.max_pending_connections() == 128);  // default
        REQUIRE_FALSE(worker.is_running());
        REQUIRE_FALSE(worker.is_accepting());
    }

    SECTION("constructs with maintenance callback") {
        bool maintenance_invoked = false;
        accept_worker worker(
            11112,
            [](uint64_t) {},
            [&]() { maintenance_invoked = true; }
        );

        REQUIRE(worker.port() == 11112);
        REQUIRE_FALSE(worker.is_running());
    }
}

TEST_CASE("accept_worker configuration", "[accept_worker][config]") {
    accept_worker worker(
        11113,
        [](uint64_t) {}
    );

    SECTION("set_max_pending_connections") {
        worker.set_max_pending_connections(256);
        REQUIRE(worker.max_pending_connections() == 256);
    }

    SECTION("port accessor") {
        REQUIRE(worker.port() == 11113);
    }
}

TEST_CASE("accept_worker lifecycle", "[accept_worker][lifecycle]") {
    accept_worker worker(
        11114,
        [](uint64_t) {}
    );

    SECTION("start and stop") {
        worker.set_wake_interval(50ms);

        auto result = worker.start();
        REQUIRE_FALSE(result.has_error());

        // Give the worker time to start and set running state
        std::this_thread::sleep_for(50ms);

        // Check accepting state (set in before_start)
        REQUIRE(worker.is_accepting());

        // Give the worker time to run at least one iteration
        std::this_thread::sleep_for(100ms);

        auto stop_result = worker.stop();
        REQUIRE_FALSE(stop_result.has_error());
        REQUIRE_FALSE(worker.is_accepting());
    }

    SECTION("stop without start is safe") {
        auto result = worker.stop();
        // thread_base returns error when stop is called without start,
        // but this is safe behavior - no crash or undefined behavior
        REQUIRE(result.has_error());
        // Verify it's the expected "not running" error
        REQUIRE(result.get_error().to_string().find("not running") != std::string::npos);
    }

    SECTION("double start returns error") {
        worker.set_wake_interval(50ms);

        auto first = worker.start();
        REQUIRE_FALSE(first.has_error());

        // Wait for thread to fully start
        std::this_thread::sleep_for(50ms);

        auto second = worker.start();
        REQUIRE(second.has_error());

        worker.stop();
    }
}

TEST_CASE("accept_worker maintenance callback", "[accept_worker][callback]") {
    std::atomic<int> maintenance_count{0};

    accept_worker worker(
        11115,
        [](uint64_t) {},
        [&]() { maintenance_count++; }
    );

    worker.set_wake_interval(50ms);

    auto result = worker.start();
    REQUIRE_FALSE(result.has_error());

    // Wait for at least 2 maintenance callbacks with generous timeout
    // Using polling with timeout instead of fixed sleep for CI stability
    constexpr int expected_callbacks = 2;
    constexpr auto max_wait = 500ms;
    constexpr auto poll_interval = 25ms;

    auto start = std::chrono::steady_clock::now();
    while (maintenance_count.load() < expected_callbacks) {
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed >= max_wait) {
            break;
        }
        std::this_thread::sleep_for(poll_interval);
    }

    worker.stop();

    // Should have been called at least twice within generous timeout
    REQUIRE(maintenance_count.load() >= expected_callbacks);
}

TEST_CASE("accept_worker graceful shutdown", "[accept_worker][shutdown]") {
    std::atomic<bool> in_work{false};

    accept_worker worker(
        11116,
        [](uint64_t) {},
        [&]() {
            in_work = true;
            std::this_thread::sleep_for(50ms);
            in_work = false;
        }
    );

    worker.set_wake_interval(10ms);

    auto result = worker.start();
    REQUIRE_FALSE(result.has_error());

    // Wait for work to start
    while (!in_work) {
        std::this_thread::sleep_for(5ms);
    }

    // Stop should wait for current work to complete
    worker.stop();

    REQUIRE_FALSE(worker.is_running());
    // Stop should have waited for work completion gracefully
}

TEST_CASE("accept_worker to_string", "[accept_worker][debug]") {
    accept_worker worker(
        11117,
        [](uint64_t) {}
    );

    worker.set_max_pending_connections(64);

    auto str = worker.to_string();

    REQUIRE_THAT(str, Catch::Matchers::ContainsSubstring("accept_worker"));
    REQUIRE_THAT(str, Catch::Matchers::ContainsSubstring("port=11117"));
    REQUIRE_THAT(str, Catch::Matchers::ContainsSubstring("backlog=64"));
}

TEST_CASE("accept_worker destructor stops thread", "[accept_worker][raii]") {
    std::atomic<bool> was_running{false};

    {
        accept_worker worker(
            11118,
            [](uint64_t) {}
        );

        worker.set_wake_interval(50ms);
        auto result = worker.start();
        REQUIRE_FALSE(result.has_error());

        // Give the thread time to transition to running state
        // The thread needs to execute before_start() and enter the wait loop
        std::this_thread::sleep_for(50ms);

        was_running = worker.is_running();
        // Destructor should be called here
    }

    REQUIRE(was_running);
    // If we get here without hanging, the destructor properly stopped the thread
}
