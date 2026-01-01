/**
 * @file test_dcmtk_tool.cpp
 * @brief Unit tests for DCMTK tool wrapper utilities
 *
 * Tests the dcmtk_tool class functionality including availability checks,
 * version detection, and server lifecycle management.
 *
 * @see Issue #450 - DCMTK Process Launcher and Test Utilities
 * @see Issue #449 - DCMTK Interoperability Test Automation Epic
 */

#include "dcmtk_tool.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <thread>

using namespace pacs::integration_test;

// =============================================================================
// DCMTK Availability Tests
// =============================================================================

TEST_CASE("DCMTK availability check", "[dcmtk][utility]") {
    bool available = dcmtk_tool::is_available();

    if (!available) {
        WARN("DCMTK not installed - some tests will be skipped");
        WARN("Install with: brew install dcmtk (macOS) or apt install dcmtk (Linux)");
    }

    // This test always passes - it just checks the detection mechanism
    SUCCEED("DCMTK availability check completed");
}

TEST_CASE("DCMTK version detection", "[dcmtk][utility]") {
    if (!dcmtk_tool::is_available()) {
        SKIP("DCMTK not installed");
    }

    auto version = dcmtk_tool::version();
    REQUIRE(version.has_value());
    INFO("DCMTK version: " << *version);

    // Version string should not be empty
    REQUIRE_FALSE(version->empty());
}

// =============================================================================
// dcmtk_result Tests
// =============================================================================

TEST_CASE("dcmtk_result success check", "[dcmtk][utility]") {
    dcmtk_result result;

    SECTION("Default result is not success") {
        REQUIRE_FALSE(result.success());
    }

    SECTION("Zero exit code is success") {
        result.exit_code = 0;
        REQUIRE(result.success());
    }

    SECTION("Non-zero exit code is failure") {
        result.exit_code = 1;
        REQUIRE_FALSE(result.success());
    }
}

TEST_CASE("dcmtk_result error check", "[dcmtk][utility]") {
    dcmtk_result result;

    SECTION("Empty stderr has no error") {
        REQUIRE_FALSE(result.has_error());
    }

    SECTION("Non-empty stderr has error") {
        result.stderr_output = "error message";
        REQUIRE(result.has_error());
    }
}

// =============================================================================
// DCMTK Server Lifecycle Tests
// =============================================================================

TEST_CASE("DCMTK storescp lifecycle", "[dcmtk][utility][server]") {
    if (!dcmtk_tool::is_available()) {
        SKIP("DCMTK not installed");
    }

    auto port = find_available_port();
    test_directory output_dir;

    SECTION("Server starts and stops correctly") {
        {
            auto server = dcmtk_tool::storescp(port, "TEST_SCP", output_dir.path());

            // Check if server started successfully
            if (server.pid() == process_launcher::invalid_pid) {
                SKIP("Failed to start storescp - may not be installed correctly");
            }

            REQUIRE(server.is_running());

            // Server should be listening on the port
            REQUIRE(process_launcher::is_port_listening(port));
        }

        // After guard destruction, give the process time to fully terminate
        std::this_thread::sleep_for(std::chrono::milliseconds{200});

        // Server should be stopped after guard destruction
        REQUIRE_FALSE(process_launcher::is_port_listening(port));
    }
}

TEST_CASE("DCMTK echoscp lifecycle", "[dcmtk][utility][server]") {
    if (!dcmtk_tool::is_available()) {
        SKIP("DCMTK not installed");
    }

    auto port = find_available_port();

    SECTION("Server starts and stops correctly") {
        {
            auto server = dcmtk_tool::echoscp(port, "TEST_ECHO_SCP");

            if (server.pid() == process_launcher::invalid_pid) {
                SKIP("Failed to start echoscp - may not be installed correctly");
            }

            REQUIRE(server.is_running());
            REQUIRE(process_launcher::is_port_listening(port));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds{200});
        REQUIRE_FALSE(process_launcher::is_port_listening(port));
    }
}

// =============================================================================
// DCMTK Server Guard Tests
// =============================================================================

TEST_CASE("dcmtk_server_guard wait_for_ready", "[dcmtk][utility][server]") {
    if (!dcmtk_tool::is_available()) {
        SKIP("DCMTK not installed");
    }

    auto port = find_available_port();

    dcmtk_server_guard server("echoscp", port, {
        "-aet", "TEST_SCP",
        std::to_string(port)
    });

    if (!server.is_running()) {
        SKIP("Failed to start echoscp");
    }

    SECTION("wait_for_ready returns true when server is listening") {
        REQUIRE(server.wait_for_ready(std::chrono::seconds{10}));
    }

    SECTION("Port matches") {
        REQUIRE(server.port() == port);
    }
}

TEST_CASE("dcmtk_server_guard move semantics", "[dcmtk][utility][server]") {
    if (!dcmtk_tool::is_available()) {
        SKIP("DCMTK not installed");
    }

    auto port = find_available_port();

    SECTION("Move construction transfers ownership") {
        dcmtk_server_guard server1("echoscp", port, {
            "-aet", "TEST_SCP",
            std::to_string(port)
        });

        if (!server1.is_running()) {
            SKIP("Failed to start echoscp");
        }

        auto pid = server1.pid();
        dcmtk_server_guard server2(std::move(server1));

        REQUIRE(server2.pid() == pid);
        REQUIRE(server2.is_running());
    }
}

// =============================================================================
// Background Process Guard Integration Tests
// =============================================================================

TEST_CASE("background_process_guard with DCMTK", "[dcmtk][utility]") {
    if (!dcmtk_tool::is_available()) {
        SKIP("DCMTK not installed");
    }

    auto port = find_available_port();

    SECTION("Guard manages process lifecycle") {
        background_process_guard guard;

        {
            auto temp_guard = dcmtk_tool::echoscp(port, "TEST_SCP");
            if (temp_guard.pid() == process_launcher::invalid_pid) {
                SKIP("Failed to start echoscp");
            }

            guard = std::move(temp_guard);
        }

        // Guard should still manage the process
        REQUIRE(guard.is_running());

        guard.stop();
        std::this_thread::sleep_for(std::chrono::milliseconds{200});
        REQUIRE_FALSE(guard.is_running());
    }
}

// =============================================================================
// Port Availability Tests
// =============================================================================

TEST_CASE("find_available_port returns valid port", "[dcmtk][utility]") {
    auto port1 = find_available_port();
    auto port2 = find_available_port();

    REQUIRE(port1 > 0);
    REQUIRE(port2 > 0);
    REQUIRE(port1 != port2);  // Should return different ports
}

TEST_CASE("is_port_listening detection", "[dcmtk][utility]") {
    auto port = find_available_port();

    SECTION("Unused port is not listening") {
        REQUIRE_FALSE(process_launcher::is_port_listening(port));
    }
}
