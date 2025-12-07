/**
 * @file health_checker_test.cpp
 * @brief Unit tests for health_checker class
 *
 * @see Issue #211 - Implement health check endpoint
 */

#include <pacs/monitoring/health_checker.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <thread>

using namespace pacs::monitoring;

// =============================================================================
// Construction Tests
// =============================================================================

TEST_CASE("health_checker default construction", "[monitoring][health_checker]") {
    health_checker checker;

    // Should be constructible without errors
    CHECK(checker.is_alive());
}

TEST_CASE("health_checker construction with config",
          "[monitoring][health_checker]") {
    health_checker_config config;
    config.check_interval = std::chrono::seconds{60};
    config.database_timeout = std::chrono::milliseconds{10000};
    config.storage_warning_threshold = 75.0;

    health_checker checker{config};

    CHECK(checker.config().check_interval == std::chrono::seconds{60});
    CHECK(checker.config().database_timeout ==
          std::chrono::milliseconds{10000});
    CHECK(checker.config().storage_warning_threshold == 75.0);
}

TEST_CASE("health_checker move construction", "[monitoring][health_checker]") {
    health_checker original;
    original.set_version(2, 0, 1, "test-build");

    health_checker moved{std::move(original)};

    auto status = moved.check();
    CHECK(status.version.major == 2);
    CHECK(status.version.minor == 0);
    CHECK(status.version.patch == 1);
    CHECK(status.version.build_id == "test-build");
}

TEST_CASE("health_checker move assignment", "[monitoring][health_checker]") {
    health_checker original;
    original.set_version(3, 1, 0);

    health_checker target;
    target = std::move(original);

    auto status = target.check();
    CHECK(status.version.major == 3);
}

// =============================================================================
// Basic Health Check Tests
// =============================================================================

TEST_CASE("health_checker is_alive always returns true",
          "[monitoring][health_checker]") {
    health_checker checker;
    CHECK(checker.is_alive());
}

TEST_CASE("health_checker check returns valid status",
          "[monitoring][health_checker]") {
    health_checker checker;
    auto status = checker.check();

    // Without storage/database configured, should be healthy
    CHECK(status.database.connected);
    CHECK(status.storage.readable);
    CHECK(status.storage.writable);
}

TEST_CASE("health_checker cached status", "[monitoring][health_checker]") {
    health_checker_config config;
    config.cache_duration = std::chrono::seconds{5};

    health_checker checker{config};

    // First check
    auto status1 = checker.check();

    // Cached check should return same timestamp
    auto status2 = checker.get_cached_status();
    CHECK(status2.timestamp == status1.timestamp);
}

TEST_CASE("health_checker get_status uses cache", "[monitoring][health_checker]") {
    health_checker_config config;
    config.cache_duration = std::chrono::seconds{60};

    health_checker checker{config};

    // Perform initial check
    auto status1 = checker.check();

    // get_status should return cached result
    auto status2 = checker.get_status();
    CHECK(status2.timestamp == status1.timestamp);
}

// =============================================================================
// Metrics Update Tests
// =============================================================================

TEST_CASE("health_checker update_association_metrics",
          "[monitoring][health_checker]") {
    health_checker checker;

    checker.update_association_metrics(10, 100, 500, 5);

    auto status = checker.check();
    CHECK(status.associations.active_associations == 10);
    CHECK(status.associations.max_associations == 100);
    CHECK(status.associations.total_associations == 500);
    CHECK(status.associations.failed_associations == 5);
}

TEST_CASE("health_checker update_storage_metrics",
          "[monitoring][health_checker]") {
    health_checker checker;

    checker.update_storage_metrics(1000, 50, 200, 950, 50);

    auto status = checker.check();
    CHECK(status.metrics.total_instances == 1000);
    CHECK(status.metrics.total_studies == 50);
    CHECK(status.metrics.total_series == 200);
    CHECK(status.metrics.successful_stores == 950);
    CHECK(status.metrics.failed_stores == 50);
}

TEST_CASE("health_checker set_version", "[monitoring][health_checker]") {
    health_checker checker;

    checker.set_version(2, 5, 3, "abc123");

    auto status = checker.check();
    CHECK(status.version.major == 2);
    CHECK(status.version.minor == 5);
    CHECK(status.version.patch == 3);
    CHECK(status.version.build_id == "abc123");
    CHECK(status.version.version_string() == "2.5.3");
}

// =============================================================================
// Custom Check Tests
// =============================================================================

TEST_CASE("health_checker register custom check",
          "[monitoring][health_checker]") {
    health_checker checker;

    bool check_called = false;
    checker.register_check("test_check",
                           [&check_called](std::string& /*error*/) {
                               check_called = true;
                               return true;
                           });

    [[maybe_unused]] auto status = checker.check();
    CHECK(check_called);
}

TEST_CASE("health_checker custom check failure",
          "[monitoring][health_checker]") {
    health_checker checker;

    checker.register_check("failing_check", [](std::string& error) {
        error = "Custom check failed";
        return false;
    });

    auto status = checker.check();
    CHECK(status.message.has_value());
    CHECK(status.message->find("failing_check") != std::string::npos);
}

TEST_CASE("health_checker unregister custom check",
          "[monitoring][health_checker]") {
    health_checker checker;

    std::atomic<int> call_count{0};
    checker.register_check("temp_check", [&call_count](std::string& /*error*/) {
        ++call_count;
        return true;
    });

    [[maybe_unused]] auto s1 = checker.check();
    CHECK(call_count == 1);

    checker.unregister_check("temp_check");
    [[maybe_unused]] auto s2 = checker.check();
    CHECK(call_count == 1);  // Should not increment
}

TEST_CASE("health_checker custom check exception handling",
          "[monitoring][health_checker]") {
    health_checker checker;

    checker.register_check(
        "throwing_check",
        [](std::string& /*error*/) -> bool { throw std::runtime_error("Test exception"); });

    // Should not throw, but capture error in message
    auto status = checker.check();
    CHECK(status.message.has_value());
    CHECK(status.message->find("throwing_check") != std::string::npos);
    CHECK(status.message->find("exception") != std::string::npos);
}

// =============================================================================
// Configuration Tests
// =============================================================================

TEST_CASE("health_checker set_config", "[monitoring][health_checker]") {
    health_checker checker;

    health_checker_config new_config;
    new_config.check_interval = std::chrono::seconds{120};
    new_config.storage_warning_threshold = 85.0;

    checker.set_config(new_config);

    CHECK(checker.config().check_interval == std::chrono::seconds{120});
    CHECK(checker.config().storage_warning_threshold == 85.0);
}

// =============================================================================
// Thread Safety Tests
// =============================================================================

TEST_CASE("health_checker thread safety", "[monitoring][health_checker]") {
    health_checker checker;
    std::atomic<int> check_count{0};
    std::atomic<bool> error_occurred{false};

    // Spawn multiple threads performing health checks
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&checker, &check_count, &error_occurred]() {
            try {
                for (int j = 0; j < 10; ++j) {
                    [[maybe_unused]] auto s = checker.check();
                    [[maybe_unused]] auto c = checker.get_cached_status();
                    [[maybe_unused]] bool r = checker.is_ready();
                    ++check_count;
                }
            } catch (...) {
                error_occurred = true;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    CHECK_FALSE(error_occurred);
    CHECK(check_count == 40);
}

TEST_CASE("health_checker concurrent metrics updates",
          "[monitoring][health_checker]") {
    health_checker checker;
    std::atomic<bool> error_occurred{false};

    std::vector<std::thread> threads;

    // Thread updating associations
    threads.emplace_back([&checker, &error_occurred]() {
        try {
            for (int i = 0; i < 100; ++i) {
                checker.update_association_metrics(
                    static_cast<std::uint32_t>(i), 100, static_cast<std::uint64_t>(i * 10), 0);
            }
        } catch (...) {
            error_occurred = true;
        }
    });

    // Thread updating storage metrics
    threads.emplace_back([&checker, &error_occurred]() {
        try {
            for (int i = 0; i < 100; ++i) {
                checker.update_storage_metrics(static_cast<std::uint64_t>(i * 100), static_cast<std::uint64_t>(i), static_cast<std::uint64_t>(i * 2), static_cast<std::uint64_t>(i * 99), static_cast<std::uint64_t>(i));
            }
        } catch (...) {
            error_occurred = true;
        }
    });

    // Thread performing health checks
    threads.emplace_back([&checker, &error_occurred]() {
        try {
            for (int i = 0; i < 100; ++i) {
                [[maybe_unused]] auto s = checker.check();
            }
        } catch (...) {
            error_occurred = true;
        }
    });

    for (auto& t : threads) {
        t.join();
    }

    CHECK_FALSE(error_occurred);
}

// =============================================================================
// is_ready Tests
// =============================================================================

TEST_CASE("health_checker is_ready without storage/database",
          "[monitoring][health_checker]") {
    health_checker checker;

    // Without configured storage/database, should be ready
    CHECK(checker.is_ready());
}
