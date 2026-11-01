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

#include "dcmtk_tool.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <thread>

using namespace kcenon::pacs::integration_test;

// =============================================================================
// DCMTK Availability Tests
// =============================================================================

TEST_CASE("process launcher reports a missing executable immediately", "[process][utility]") {
    const auto started = std::chrono::steady_clock::now();
    auto result = process_launcher::run("pacs-nonexistent-test-executable-1311", {},
                                        std::chrono::seconds{5});
    REQUIRE(result.exit_code != 0);
    REQUIRE_FALSE(result.timed_out);
    REQUIRE_FALSE(result.stderr_output.empty());
    REQUIRE(std::chrono::steady_clock::now() - started < std::chrono::seconds{3});
    REQUIRE(process_launcher::start_background("pacs-nonexistent-test-executable-1311")
            == process_launcher::invalid_pid);
}

#ifndef _WIN32
TEST_CASE("process launcher preserves exits signals and captured bytes", "[process][utility]") {
    SECTION("PATH lookup and nonzero exit") {
        auto result = process_launcher::run("sh", {"-c", "printf 'out'; printf 'err' >&2; exit 7"});
        REQUIRE(result.exit_code == 7);
        REQUIRE(result.stdout_output == "out");
        REQUIRE(result.stderr_output == "err");
    }
    SECTION("signal termination does not run the parent's Catch2 handlers") {
        auto result = process_launcher::run("sh", {"-c", "ulimit -c 0; kill -ABRT $$"});
        REQUIRE(result.exit_code == -SIGABRT);
        REQUIRE(result.stdout_output.empty());
        REQUIRE(result.stderr_output.find("Catch") == std::string::npos);
    }
    SECTION("timeout is bounded and distinguished from an exit") {
        auto result = process_launcher::run("sh", {"-c", "exec sleep 10"}, std::chrono::seconds{1});
        REQUIRE(result.timed_out);
        REQUIRE(result.exit_code == 124);
        REQUIRE(result.duration < std::chrono::seconds{3});
    }
    SECTION("capture drains output while the child runs") {
        auto result = process_launcher::run("sh", {"-c", "head -c 262144 /dev/zero; head -c 262144 /dev/zero >&2"});
        REQUIRE(result.exit_code == 0);
        REQUIRE(result.stdout_output.size() == 262144);
        REQUIRE(result.stderr_output.size() == 262144);
    }
}

TEST_CASE("background process ownership reaps exited children", "[process][utility]") {
    auto pid = process_launcher::start_background("sleep", {"10"});
    REQUIRE(pid != process_launcher::invalid_pid);
    REQUIRE(process_launcher::is_running(pid));
    REQUIRE(process_launcher::stop_background(pid));
    REQUIRE_FALSE(process_launcher::is_running(pid));
    int status = 0;
    REQUIRE(waitpid(pid, &status, WNOHANG) == -1);
    REQUIRE(errno == ECHILD);
    REQUIRE(process_launcher::stop_background(pid));
}
#endif

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
            REQUIRE(server.pid() != process_launcher::invalid_pid);

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

            REQUIRE(server.pid() != process_launcher::invalid_pid);

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

    dcmtk_server_guard server("storescp", port, {
        "-aet", "TEST_SCP",
        std::to_string(port)
    });

    REQUIRE(server.is_running());

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
        dcmtk_server_guard server1("storescp", port, {
            "-aet", "TEST_SCP",
            std::to_string(port)
        });

        REQUIRE(server1.is_running());

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
            REQUIRE(temp_guard.pid() != process_launcher::invalid_pid);

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
