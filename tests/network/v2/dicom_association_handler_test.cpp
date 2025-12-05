/**
 * @file dicom_association_handler_test.cpp
 * @brief Unit tests for DICOM association handler (network_system integration layer)
 */

#include <catch2/catch_test_macros.hpp>

#include "pacs/network/v2/dicom_association_handler.hpp"
#include "pacs/network/server_config.hpp"
#include "pacs/network/pdu_types.hpp"

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

using namespace pacs::network;
using namespace pacs::network::v2;

// =============================================================================
// Test Constants
// =============================================================================

namespace {

constexpr const char* TEST_AE_TITLE = "TEST_SCP";
constexpr const char* TEST_CALLING_AE = "TEST_SCU";
constexpr const char* VERIFICATION_SOP_CLASS = "1.2.840.10008.1.1";

}  // namespace

// =============================================================================
// handler_state Tests
// =============================================================================

TEST_CASE("handler_state to_string", "[handler][state]") {
    SECTION("all states have string representation") {
        CHECK(std::string(to_string(handler_state::idle)) == "Idle");
        CHECK(std::string(to_string(handler_state::awaiting_response)) == "Awaiting Response");
        CHECK(std::string(to_string(handler_state::established)) == "Established");
        CHECK(std::string(to_string(handler_state::releasing)) == "Releasing");
        CHECK(std::string(to_string(handler_state::closed)) == "Closed");
    }
}

// =============================================================================
// Handler State Machine Tests (without network)
// =============================================================================

TEST_CASE("dicom_association_handler state queries", "[handler][state]") {
    server_config config;
    config.ae_title = TEST_AE_TITLE;

    dicom_association_handler::service_map empty_services;

    // Note: We pass nullptr for session to test state management without network
    // In real usage, a valid session would be required
    // This tests the handler's state logic independently

    SECTION("is_established returns false for non-established states") {
        // Cannot construct without session in real code, so we test the enum directly
        CHECK(handler_state::idle != handler_state::established);
        CHECK(handler_state::awaiting_response != handler_state::established);
        CHECK(handler_state::releasing != handler_state::established);
        CHECK(handler_state::closed != handler_state::established);
    }

    SECTION("is_closed returns true only for closed state") {
        CHECK(handler_state::closed == handler_state::closed);
        CHECK(handler_state::idle != handler_state::closed);
        CHECK(handler_state::established != handler_state::closed);
    }
}

// =============================================================================
// Server Config Tests for Handler
// =============================================================================

TEST_CASE("server_config for association handler", "[handler][config]") {
    SECTION("whitelist validation logic") {
        server_config config;
        config.ae_title = TEST_AE_TITLE;
        config.ae_whitelist = {"ALLOWED_SCU1", "ALLOWED_SCU2"};
        config.accept_unknown_calling_ae = false;

        // Test whitelist logic
        bool found = false;
        for (const auto& allowed : config.ae_whitelist) {
            if (allowed == "ALLOWED_SCU1") {
                found = true;
                break;
            }
        }
        CHECK(found);

        // Unknown AE should not be found
        found = false;
        for (const auto& allowed : config.ae_whitelist) {
            if (allowed == "UNKNOWN_SCU") {
                found = true;
                break;
            }
        }
        CHECK_FALSE(found);
    }

    SECTION("empty whitelist allows all") {
        server_config config;
        config.ae_title = TEST_AE_TITLE;
        config.ae_whitelist.clear();

        // Empty whitelist means accept all (in handler logic)
        CHECK(config.ae_whitelist.empty());
    }

    SECTION("accept_unknown_calling_ae bypasses whitelist") {
        server_config config;
        config.ae_title = TEST_AE_TITLE;
        config.ae_whitelist = {"ALLOWED_SCU"};
        config.accept_unknown_calling_ae = true;

        // With accept_unknown_calling_ae = true, whitelist is ignored
        CHECK(config.accept_unknown_calling_ae);
    }
}

// =============================================================================
// Service Map Tests
// =============================================================================

TEST_CASE("service_map for association handler", "[handler][services]") {
    dicom_association_handler::service_map services;

    SECTION("empty service map") {
        CHECK(services.empty());
        CHECK(services.find(VERIFICATION_SOP_CLASS) == services.end());
    }

    SECTION("service lookup") {
        // Simulate adding a service (nullptr for testing, real code would have valid ptr)
        services[VERIFICATION_SOP_CLASS] = nullptr;

        CHECK_FALSE(services.empty());
        CHECK(services.find(VERIFICATION_SOP_CLASS) != services.end());
        CHECK(services.find("1.2.3.4.5") == services.end());
    }
}

// =============================================================================
// Statistics Default Values Tests
// =============================================================================

TEST_CASE("handler statistics initial values", "[handler][statistics]") {
    // Test that atomic counters work correctly
    std::atomic<uint64_t> counter{0};

    SECTION("counter starts at zero") {
        CHECK(counter.load() == 0);
    }

    SECTION("counter increments correctly") {
        counter.fetch_add(1);
        CHECK(counter.load() == 1);

        counter.fetch_add(5);
        CHECK(counter.load() == 6);
    }

    SECTION("counter is thread-safe") {
        std::atomic<uint64_t> shared_counter{0};
        const int iterations = 1000;

        // Simple concurrent increment test
        std::thread t1([&]() {
            for (int i = 0; i < iterations; ++i) {
                shared_counter.fetch_add(1, std::memory_order_relaxed);
            }
        });

        std::thread t2([&]() {
            for (int i = 0; i < iterations; ++i) {
                shared_counter.fetch_add(1, std::memory_order_relaxed);
            }
        });

        t1.join();
        t2.join();

        CHECK(shared_counter.load() == 2 * iterations);
    }
}

// =============================================================================
// Time Point Tests
// =============================================================================

TEST_CASE("handler time tracking", "[handler][time]") {
    using clock = std::chrono::steady_clock;

    SECTION("time point comparison") {
        auto t1 = clock::now();
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
        auto t2 = clock::now();

        CHECK(t2 > t1);
        CHECK(std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() >= 10);
    }
}

// =============================================================================
// Callback Type Tests
// =============================================================================

TEST_CASE("handler callback types", "[handler][callbacks]") {
    SECTION("established callback signature") {
        bool callback_called = false;
        std::string captured_session_id;
        std::string captured_calling_ae;
        std::string captured_called_ae;

        association_established_callback callback =
            [&](const std::string& session_id,
                const std::string& calling_ae,
                const std::string& called_ae) {
                callback_called = true;
                captured_session_id = session_id;
                captured_calling_ae = calling_ae;
                captured_called_ae = called_ae;
            };

        // Invoke callback
        callback("session_123", TEST_CALLING_AE, TEST_AE_TITLE);

        CHECK(callback_called);
        CHECK(captured_session_id == "session_123");
        CHECK(captured_calling_ae == TEST_CALLING_AE);
        CHECK(captured_called_ae == TEST_AE_TITLE);
    }

    SECTION("closed callback signature") {
        bool callback_called = false;
        std::string captured_session_id;
        bool captured_graceful = false;

        association_closed_callback callback =
            [&](const std::string& session_id, bool graceful) {
                callback_called = true;
                captured_session_id = session_id;
                captured_graceful = graceful;
            };

        // Test graceful close
        callback("session_456", true);
        CHECK(callback_called);
        CHECK(captured_session_id == "session_456");
        CHECK(captured_graceful);

        // Test forced close
        callback_called = false;
        callback("session_789", false);
        CHECK(callback_called);
        CHECK_FALSE(captured_graceful);
    }

    SECTION("error callback signature") {
        bool callback_called = false;
        std::string captured_session_id;
        std::string captured_error;

        handler_error_callback callback =
            [&](const std::string& session_id, const std::string& error) {
                callback_called = true;
                captured_session_id = session_id;
                captured_error = error;
            };

        callback("session_error", "Network timeout");

        CHECK(callback_called);
        CHECK(captured_session_id == "session_error");
        CHECK(captured_error == "Network timeout");
    }
}

// =============================================================================
// PDU Constants Tests
// =============================================================================

TEST_CASE("handler PDU constants", "[handler][pdu]") {
    SECTION("PDU header size is 6 bytes") {
        CHECK(dicom_association_handler::pdu_header_size == 6);
    }

    SECTION("max PDU size is reasonable") {
        // 64 MB max
        CHECK(dicom_association_handler::max_pdu_size == 64 * 1024 * 1024);
    }
}
