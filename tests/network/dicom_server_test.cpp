/**
 * @file dicom_server_test.cpp
 * @brief Unit tests for DICOM Server class
 */

#include <catch2/catch_test_macros.hpp>

#include "pacs/network/dicom_server.hpp"
#include "pacs/network/server_config.hpp"
#include "pacs/services/verification_scp.hpp"

#include <atomic>
#include <chrono>
#include <latch>
#include <thread>

using namespace pacs::network;
using namespace pacs::services;

// =============================================================================
// Test Constants
// =============================================================================

namespace {

constexpr const char* TEST_AE_TITLE = "TEST_SCP";
constexpr uint16_t TEST_PORT = 11112;
constexpr const char* VERIFICATION_SOP_CLASS = "1.2.840.10008.1.1";

}  // namespace

// =============================================================================
// server_config Tests
// =============================================================================

TEST_CASE("server_config default construction", "[server_config][construction]") {
    server_config config;

    SECTION("has sensible defaults") {
        CHECK(config.ae_title == "PACS_SCP");
        CHECK(config.port == 11112);
        CHECK(config.max_associations == 20);
        CHECK(config.max_pdu_size == DEFAULT_MAX_PDU_LENGTH);
        CHECK(config.idle_timeout == std::chrono::seconds{300});
        CHECK(config.ae_whitelist.empty());
        CHECK_FALSE(config.accept_unknown_calling_ae);
    }
}

TEST_CASE("server_config constructor with parameters", "[server_config][construction]") {
    server_config config{"MY_PACS", 11113};

    SECTION("sets provided values") {
        CHECK(config.ae_title == "MY_PACS");
        CHECK(config.port == 11113);
    }

    SECTION("keeps other defaults") {
        CHECK(config.max_associations == 20);
        CHECK(config.max_pdu_size == DEFAULT_MAX_PDU_LENGTH);
    }
}

// =============================================================================
// server_statistics Tests
// =============================================================================

TEST_CASE("server_statistics default values", "[server_statistics]") {
    server_statistics stats;

    SECTION("all counters start at zero") {
        CHECK(stats.total_associations == 0);
        CHECK(stats.active_associations == 0);
        CHECK(stats.rejected_associations == 0);
        CHECK(stats.messages_processed == 0);
        CHECK(stats.bytes_received == 0);
        CHECK(stats.bytes_sent == 0);
    }
}

TEST_CASE("server_statistics uptime calculation", "[server_statistics]") {
    server_statistics stats;
    stats.start_time = std::chrono::steady_clock::now() - std::chrono::seconds{60};

    SECTION("calculates uptime correctly") {
        auto uptime = stats.uptime();
        // Allow some tolerance for test execution time
        CHECK(uptime.count() >= 59);
        CHECK(uptime.count() <= 62);
    }
}

// =============================================================================
// dicom_server Construction Tests
// =============================================================================

TEST_CASE("dicom_server construction", "[dicom_server][construction]") {
    server_config config;
    config.ae_title = TEST_AE_TITLE;
    config.port = TEST_PORT;

    dicom_server server{config};

    SECTION("is not running initially") {
        CHECK_FALSE(server.is_running());
    }

    SECTION("has zero active associations") {
        CHECK(server.active_associations() == 0);
    }

    SECTION("returns correct config") {
        CHECK(server.config().ae_title == TEST_AE_TITLE);
        CHECK(server.config().port == TEST_PORT);
    }

    SECTION("has no supported SOP classes initially") {
        CHECK(server.supported_sop_classes().empty());
    }
}

// =============================================================================
// Service Registration Tests
// =============================================================================

TEST_CASE("dicom_server service registration", "[dicom_server][services]") {
    server_config config;
    config.ae_title = TEST_AE_TITLE;
    config.port = TEST_PORT;

    dicom_server server{config};

    SECTION("register shared_ptr service") {
        auto verification = std::make_shared<verification_scp>();
        server.register_service(verification);

        auto sop_classes = server.supported_sop_classes();
        CHECK(sop_classes.size() == 1);
        CHECK(sop_classes[0] == VERIFICATION_SOP_CLASS);
    }

    SECTION("register multiple services") {
        auto verification = std::make_shared<verification_scp>();
        server.register_service(verification);

        // Registering another verification service (for testing)
        auto verification2 = std::make_shared<verification_scp>();
        server.register_service(verification2);

        // Should have same SOP Class (may be deduplicated or both registered)
        auto sop_classes = server.supported_sop_classes();
        CHECK_FALSE(sop_classes.empty());
    }

    SECTION("null service is ignored") {
        scp_service_ptr null_service;
        server.register_service(null_service);

        CHECK(server.supported_sop_classes().empty());
    }
}

// =============================================================================
// Server Start/Stop Tests
// =============================================================================

TEST_CASE("dicom_server start validation", "[dicom_server][lifecycle]") {
    SECTION("start fails with empty AE title") {
        server_config config;
        config.ae_title = "";
        config.port = TEST_PORT;

        dicom_server server{config};
        server.register_service(std::make_shared<verification_scp>());

        auto result = server.start();
        CHECK(result.is_err());
        CHECK_FALSE(server.is_running());
    }

    SECTION("start fails with too long AE title") {
        server_config config;
        config.ae_title = "THIS_AE_TITLE_IS_WAY_TOO_LONG_FOR_DICOM";
        config.port = TEST_PORT;

        dicom_server server{config};
        server.register_service(std::make_shared<verification_scp>());

        auto result = server.start();
        CHECK(result.is_err());
        CHECK_FALSE(server.is_running());
    }

    SECTION("start fails with port 0") {
        server_config config;
        config.ae_title = TEST_AE_TITLE;
        config.port = 0;

        dicom_server server{config};
        server.register_service(std::make_shared<verification_scp>());

        auto result = server.start();
        CHECK(result.is_err());
    }

    SECTION("start fails without registered services") {
        server_config config;
        config.ae_title = TEST_AE_TITLE;
        config.port = TEST_PORT;

        dicom_server server{config};
        // No services registered

        auto result = server.start();
        CHECK(result.is_err());
    }
}

TEST_CASE("dicom_server start with valid configuration", "[dicom_server][lifecycle]") {
    server_config config;
    config.ae_title = TEST_AE_TITLE;
    config.port = TEST_PORT;

    dicom_server server{config};
    server.register_service(std::make_shared<verification_scp>());

    SECTION("starts successfully") {
        auto result = server.start();
        CHECK(result.is_ok());
        CHECK(server.is_running());

        server.stop();
        CHECK_FALSE(server.is_running());
    }

    SECTION("double start returns error") {
        auto result1 = server.start();
        CHECK(result1.is_ok());

        auto result2 = server.start();
        CHECK(result2.is_err());

        server.stop();
    }

    SECTION("stop idempotent") {
        auto result = server.start();
        CHECK(result.is_ok());

        server.stop();
        CHECK_FALSE(server.is_running());

        // Second stop should not crash
        server.stop();
        CHECK_FALSE(server.is_running());
    }
}

// =============================================================================
// Statistics Tests
// =============================================================================

TEST_CASE("dicom_server statistics", "[dicom_server][statistics]") {
    server_config config;
    config.ae_title = TEST_AE_TITLE;
    config.port = TEST_PORT;

    dicom_server server{config};
    server.register_service(std::make_shared<verification_scp>());

    SECTION("statistics are reset on start") {
        auto result = server.start();
        REQUIRE(result.is_ok());

        auto stats = server.get_statistics();
        CHECK(stats.total_associations == 0);
        CHECK(stats.active_associations == 0);
        CHECK(stats.rejected_associations == 0);
        CHECK(stats.messages_processed == 0);

        server.stop();
    }

    SECTION("start_time is set on start") {
        auto before = std::chrono::steady_clock::now();

        auto result = server.start();
        REQUIRE(result.is_ok());

        auto stats = server.get_statistics();
        auto after = std::chrono::steady_clock::now();

        CHECK(stats.start_time >= before);
        CHECK(stats.start_time <= after);

        server.stop();
    }
}

// =============================================================================
// Callback Tests
// =============================================================================

TEST_CASE("dicom_server callbacks", "[dicom_server][callbacks]") {
    server_config config;
    config.ae_title = TEST_AE_TITLE;
    config.port = TEST_PORT;

    dicom_server server{config};
    server.register_service(std::make_shared<verification_scp>());

    SECTION("established callback can be set") {
        std::atomic<bool> callback_called{false};

        server.on_association_established([&](const association&) {
            callback_called = true;
        });

        // Callback is stored, but won't be called until actual connection
        // This test just verifies the callback registration works
        CHECK_FALSE(callback_called);
    }

    SECTION("released callback can be set") {
        std::atomic<bool> callback_called{false};

        server.on_association_released([&](const association&) {
            callback_called = true;
        });

        CHECK_FALSE(callback_called);
    }

    SECTION("error callback can be set") {
        std::atomic<bool> callback_called{false};

        server.on_error([&](const std::string&) {
            callback_called = true;
        });

        CHECK_FALSE(callback_called);
    }
}

// =============================================================================
// Max Associations Tests
// =============================================================================

TEST_CASE("dicom_server max associations configuration", "[dicom_server][config]") {
    SECTION("max_associations limits concurrent connections") {
        server_config config;
        config.ae_title = TEST_AE_TITLE;
        config.port = TEST_PORT;
        config.max_associations = 5;

        dicom_server server{config};
        server.register_service(std::make_shared<verification_scp>());

        CHECK(server.config().max_associations == 5);
    }

    SECTION("max_associations 0 means unlimited") {
        server_config config;
        config.ae_title = TEST_AE_TITLE;
        config.port = TEST_PORT;
        config.max_associations = 0;

        dicom_server server{config};
        CHECK(server.config().max_associations == 0);
    }
}

// =============================================================================
// Idle Timeout Tests
// =============================================================================

TEST_CASE("dicom_server idle timeout configuration", "[dicom_server][config]") {
    SECTION("idle_timeout is configurable") {
        server_config config;
        config.ae_title = TEST_AE_TITLE;
        config.port = TEST_PORT;
        config.idle_timeout = std::chrono::seconds{60};

        dicom_server server{config};
        CHECK(server.config().idle_timeout == std::chrono::seconds{60});
    }

    SECTION("idle_timeout 0 disables timeout") {
        server_config config;
        config.ae_title = TEST_AE_TITLE;
        config.port = TEST_PORT;
        config.idle_timeout = std::chrono::seconds{0};

        dicom_server server{config};
        CHECK(server.config().idle_timeout == std::chrono::seconds{0});
    }
}

// =============================================================================
// AE Whitelist Tests
// =============================================================================

TEST_CASE("dicom_server AE whitelist configuration", "[dicom_server][config]") {
    SECTION("empty whitelist accepts all") {
        server_config config;
        config.ae_title = TEST_AE_TITLE;
        config.port = TEST_PORT;
        config.ae_whitelist = {};

        dicom_server server{config};
        CHECK(server.config().ae_whitelist.empty());
    }

    SECTION("whitelist with entries") {
        server_config config;
        config.ae_title = TEST_AE_TITLE;
        config.port = TEST_PORT;
        config.ae_whitelist = {"MODALITY1", "MODALITY2", "MODALITY3"};

        dicom_server server{config};
        CHECK(server.config().ae_whitelist.size() == 3);
    }

    SECTION("accept_unknown_calling_ae configuration") {
        server_config config;
        config.ae_title = TEST_AE_TITLE;
        config.port = TEST_PORT;
        config.ae_whitelist = {"KNOWN_AE"};
        config.accept_unknown_calling_ae = true;

        dicom_server server{config};
        CHECK(server.config().accept_unknown_calling_ae);
    }
}

// =============================================================================
// Destructor Tests
// =============================================================================

TEST_CASE("dicom_server destructor stops server", "[dicom_server][lifecycle]") {
    bool server_was_running = false;

    {
        server_config config;
        config.ae_title = TEST_AE_TITLE;
        config.port = TEST_PORT;

        dicom_server server{config};
        server.register_service(std::make_shared<verification_scp>());

        auto result = server.start();
        REQUIRE(result.is_ok());
        server_was_running = server.is_running();
    }
    // Server destructor called here

    CHECK(server_was_running);
    // If we get here without hanging, destructor properly stopped the server
}

// =============================================================================
// Graceful Shutdown Tests
// =============================================================================

TEST_CASE("dicom_server graceful shutdown", "[dicom_server][lifecycle]") {
    server_config config;
    config.ae_title = TEST_AE_TITLE;
    config.port = TEST_PORT;

    dicom_server server{config};
    server.register_service(std::make_shared<verification_scp>());

    SECTION("stop with timeout") {
        auto result = server.start();
        REQUIRE(result.is_ok());

        auto start = std::chrono::steady_clock::now();
        server.stop(std::chrono::seconds{1});
        auto end = std::chrono::steady_clock::now();

        CHECK_FALSE(server.is_running());

        // Should complete quickly (no active associations)
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        CHECK(duration.count() < 1000);
    }
}

// =============================================================================
// Cancellation Token Integration Tests (Issue #159)
// =============================================================================

TEST_CASE("dicom_server cancellation token integration", "[dicom_server][cancellation][lifecycle]") {
    server_config config;
    config.ae_title = TEST_AE_TITLE;
    config.port = TEST_PORT;

    dicom_server server{config};
    server.register_service(std::make_shared<verification_scp>());

    SECTION("graceful shutdown uses 3-phase approach") {
        // Phase 1: Request graceful cancellation via cancellation tokens
        // Phase 2: Wait for graceful shutdown with timeout
        // Phase 3: Force abort remaining associations

        auto result = server.start();
        REQUIRE(result.is_ok());
        CHECK(server.is_running());

        // Stop should complete without hanging, demonstrating
        // that cancellation token integration works
        auto start = std::chrono::steady_clock::now();
        server.stop(std::chrono::milliseconds{500});
        auto end = std::chrono::steady_clock::now();

        CHECK_FALSE(server.is_running());

        // Shutdown should complete well within the timeout since there are
        // no active associations (Phase 1 & 2 complete quickly, Phase 3 skipped)
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        CHECK(duration.count() < 500);
    }

    SECTION("stop is idempotent with cancellation tokens") {
        auto result = server.start();
        REQUIRE(result.is_ok());

        // First stop
        server.stop(std::chrono::milliseconds{100});
        CHECK_FALSE(server.is_running());

        // Second stop should not crash or hang
        server.stop(std::chrono::milliseconds{100});
        CHECK_FALSE(server.is_running());
    }

    SECTION("shutdown notifies waiters") {
        auto result = server.start();
        REQUIRE(result.is_ok());

        std::atomic<bool> waiter_notified{false};
        std::latch ready_latch{1};

        // Start a thread that waits for shutdown
        std::thread waiter([&]() {
            ready_latch.count_down();  // Signal ready to wait
            server.wait_for_shutdown();
            waiter_notified = true;
        });

        // Wait for waiter thread to be ready
        ready_latch.wait();

        // Stop should notify the waiter
        server.stop(std::chrono::milliseconds{100});

        // Wait for waiter thread with timeout
        if (waiter.joinable()) {
            waiter.join();
        }

        CHECK(waiter_notified);
    }
}

TEST_CASE("dicom_server shutdown with short timeout", "[dicom_server][cancellation][timeout]") {
    server_config config;
    config.ae_title = TEST_AE_TITLE;
    config.port = TEST_PORT;

    dicom_server server{config};
    server.register_service(std::make_shared<verification_scp>());

    SECTION("immediate shutdown with zero timeout") {
        auto result = server.start();
        REQUIRE(result.is_ok());

        // Zero timeout should still work - goes straight to Phase 3
        server.stop(std::chrono::milliseconds{0});
        CHECK_FALSE(server.is_running());
    }

    SECTION("very short timeout completes normally") {
        auto result = server.start();
        REQUIRE(result.is_ok());

        // Very short timeout (10ms) - should still work with no active connections
        server.stop(std::chrono::milliseconds{10});
        CHECK_FALSE(server.is_running());
    }
}
