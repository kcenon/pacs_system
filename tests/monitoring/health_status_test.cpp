/**
 * @file health_status_test.cpp
 * @brief Unit tests for health_status and related structures
 *
 * @see Issue #211 - Implement health check endpoint
 */

#include <pacs/monitoring/health_status.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <thread>

using namespace pacs::monitoring;
using Catch::Matchers::WithinRel;

// =============================================================================
// health_level Tests
// =============================================================================

TEST_CASE("health_level to_string conversion", "[monitoring][health_status]") {
    SECTION("healthy converts to 'healthy'") {
        CHECK(to_string(health_level::healthy) == "healthy");
    }

    SECTION("degraded converts to 'degraded'") {
        CHECK(to_string(health_level::degraded) == "degraded");
    }

    SECTION("unhealthy converts to 'unhealthy'") {
        CHECK(to_string(health_level::unhealthy) == "unhealthy");
    }
}

// =============================================================================
// database_status Tests
// =============================================================================

TEST_CASE("database_status default construction",
          "[monitoring][health_status]") {
    database_status status;

    CHECK_FALSE(status.connected);
    CHECK_FALSE(status.last_connected.has_value());
    CHECK(status.active_connections == 0);
    CHECK_FALSE(status.response_time.has_value());
    CHECK_FALSE(status.error_message.has_value());
}

TEST_CASE("database_status with values", "[monitoring][health_status]") {
    database_status status;
    status.connected = true;
    status.last_connected = std::chrono::system_clock::now();
    status.active_connections = 5;
    status.response_time = std::chrono::milliseconds{42};
    status.error_message = std::nullopt;

    CHECK(status.connected);
    CHECK(status.last_connected.has_value());
    CHECK(status.active_connections == 5);
    CHECK(status.response_time.has_value());
    CHECK(status.response_time->count() == 42);
    CHECK_FALSE(status.error_message.has_value());
}

// =============================================================================
// storage_status Tests
// =============================================================================

TEST_CASE("storage_status default construction", "[monitoring][health_status]") {
    storage_status status;

    CHECK_FALSE(status.writable);
    CHECK_FALSE(status.readable);
    CHECK(status.total_bytes == 0);
    CHECK(status.used_bytes == 0);
    CHECK(status.available_bytes == 0);
    CHECK_FALSE(status.error_message.has_value());
}

TEST_CASE("storage_status usage_percent calculation",
          "[monitoring][health_status]") {
    storage_status status;

    SECTION("zero total returns 0%") {
        status.total_bytes = 0;
        status.used_bytes = 0;
        CHECK(status.usage_percent() == 0.0);
    }

    SECTION("50% usage") {
        status.total_bytes = 1000;
        status.used_bytes = 500;
        CHECK_THAT(status.usage_percent(), WithinRel(50.0, 0.01));
    }

    SECTION("100% usage") {
        status.total_bytes = 1000;
        status.used_bytes = 1000;
        CHECK_THAT(status.usage_percent(), WithinRel(100.0, 0.01));
    }

    SECTION("25% usage with large numbers") {
        status.total_bytes = 1'000'000'000'000;  // 1 TB
        status.used_bytes = 250'000'000'000;     // 250 GB
        CHECK_THAT(status.usage_percent(), WithinRel(25.0, 0.01));
    }
}

// =============================================================================
// association_metrics Tests
// =============================================================================

TEST_CASE("association_metrics default values", "[monitoring][health_status]") {
    association_metrics metrics;

    CHECK(metrics.active_associations == 0);
    CHECK(metrics.max_associations == 100);
    CHECK(metrics.total_associations == 0);
    CHECK(metrics.failed_associations == 0);
}

// =============================================================================
// storage_metrics Tests
// =============================================================================

TEST_CASE("storage_metrics default values", "[monitoring][health_status]") {
    storage_metrics metrics;

    CHECK(metrics.total_instances == 0);
    CHECK(metrics.total_studies == 0);
    CHECK(metrics.total_series == 0);
    CHECK(metrics.successful_stores == 0);
    CHECK(metrics.failed_stores == 0);
}

// =============================================================================
// version_info Tests
// =============================================================================

TEST_CASE("version_info default values", "[monitoring][health_status]") {
    version_info info;

    CHECK(info.major == 1);
    CHECK(info.minor == 0);
    CHECK(info.patch == 0);
    CHECK(info.build_id.empty());
}

TEST_CASE("version_info version_string", "[monitoring][health_status]") {
    version_info info;
    info.major = 2;
    info.minor = 3;
    info.patch = 4;

    CHECK(info.version_string() == "2.3.4");
}

TEST_CASE("version_info uptime calculation", "[monitoring][health_status]") {
    version_info info;
    info.startup_time =
        std::chrono::system_clock::now() - std::chrono::seconds{60};

    auto uptime = info.uptime();
    CHECK(uptime.count() >= 59);  // Allow for some timing variance
    CHECK(uptime.count() <= 61);
}

// =============================================================================
// health_status Tests
// =============================================================================

TEST_CASE("health_status default construction", "[monitoring][health_status]") {
    health_status status;

    CHECK(status.level == health_level::unhealthy);
    CHECK_FALSE(status.message.has_value());
    CHECK_FALSE(status.is_healthy());
    CHECK_FALSE(status.is_operational());
}

TEST_CASE("health_status update_level logic", "[monitoring][health_status]") {
    health_status status;

    SECTION("unhealthy when database disconnected") {
        status.database.connected = false;
        status.storage.readable = true;
        status.storage.writable = true;
        status.update_level();

        CHECK(status.level == health_level::unhealthy);
        CHECK_FALSE(status.is_healthy());
        CHECK_FALSE(status.is_operational());
    }

    SECTION("unhealthy when storage not readable") {
        status.database.connected = true;
        status.storage.readable = false;
        status.storage.writable = true;
        status.update_level();

        CHECK(status.level == health_level::unhealthy);
    }

    SECTION("unhealthy when storage not writable") {
        status.database.connected = true;
        status.storage.readable = true;
        status.storage.writable = false;
        status.update_level();

        CHECK(status.level == health_level::unhealthy);
    }

    SECTION("degraded when storage usage high") {
        status.database.connected = true;
        status.storage.readable = true;
        status.storage.writable = true;
        status.storage.total_bytes = 1000;
        status.storage.used_bytes = 910;  // 91%
        status.update_level();

        CHECK(status.level == health_level::degraded);
        CHECK_FALSE(status.is_healthy());
        CHECK(status.is_operational());
    }

    SECTION("degraded when associations near max") {
        status.database.connected = true;
        status.storage.readable = true;
        status.storage.writable = true;
        status.associations.active_associations = 95;
        status.associations.max_associations = 100;
        status.update_level();

        CHECK(status.level == health_level::degraded);
    }

    SECTION("healthy when all conditions met") {
        status.database.connected = true;
        status.storage.readable = true;
        status.storage.writable = true;
        status.storage.total_bytes = 1000;
        status.storage.used_bytes = 500;  // 50%
        status.associations.active_associations = 10;
        status.associations.max_associations = 100;
        status.update_level();

        CHECK(status.level == health_level::healthy);
        CHECK(status.is_healthy());
        CHECK(status.is_operational());
    }
}

TEST_CASE("health_status is_operational", "[monitoring][health_status]") {
    health_status status;

    SECTION("healthy is operational") {
        status.level = health_level::healthy;
        CHECK(status.is_operational());
    }

    SECTION("degraded is operational") {
        status.level = health_level::degraded;
        CHECK(status.is_operational());
    }

    SECTION("unhealthy is not operational") {
        status.level = health_level::unhealthy;
        CHECK_FALSE(status.is_operational());
    }
}
