/**
 * @file test_dcmtk_echo.cpp
 * @brief C-ECHO (Verification) interoperability tests with DCMTK
 *
 * Tests bidirectional C-ECHO compatibility between pacs_system and DCMTK:
 * - Scenario A: pacs_system SCP <- DCMTK echoscu
 * - Scenario B: DCMTK storescp <- pacs_system SCU (using association)
 *
 * @see Issue #451 - C-ECHO Bidirectional Interoperability Test with DCMTK
 * @see Issue #449 - DCMTK Interoperability Test Automation Epic
 */

#include <catch2/catch_test_macros.hpp>

#include "dcmtk_tool.hpp"
#include "test_fixtures.hpp"

#include "pacs/network/dimse/dimse_message.hpp"
#include "pacs/services/verification_scp.hpp"

#include <future>
#include <thread>
#include <vector>

using namespace pacs::integration_test;
using namespace pacs::network;
using namespace pacs::network::dimse;
using namespace pacs::services;

// =============================================================================
// Test: pacs_system SCP with DCMTK echoscu
// =============================================================================

TEST_CASE("C-ECHO: pacs_system SCP with DCMTK echoscu", "[dcmtk][interop][echo]") {
    if (!dcmtk_tool::is_available()) {
        SKIP("DCMTK not installed - skipping interoperability test");
    }

    // Setup: Start pacs_system echo server
    auto port = find_available_port();
    const std::string ae_title = "PACS_ECHO_SCP";

    test_server server(port, ae_title);
    server.register_service(std::make_shared<verification_scp>());
    REQUIRE(server.start());

    // Wait for server to be ready
    REQUIRE(wait_for([&]() {
        return process_launcher::is_port_listening(port);
    }, server_ready_timeout()));

    SECTION("Basic echo succeeds") {
        auto result = dcmtk_tool::echoscu("localhost", port, ae_title);

        INFO("stdout: " << result.stdout_output);
        INFO("stderr: " << result.stderr_output);

        REQUIRE(result.success());
    }

    SECTION("Echo with custom calling AE title") {
        auto result = dcmtk_tool::echoscu(
            "localhost", port, ae_title, "CUSTOM_SCU");

        INFO("stdout: " << result.stdout_output);
        INFO("stderr: " << result.stderr_output);

        REQUIRE(result.success());
    }

    SECTION("Multiple consecutive echoes") {
        for (int i = 0; i < 5; ++i) {
            auto result = dcmtk_tool::echoscu("localhost", port, ae_title);

            INFO("Iteration: " << i);
            INFO("stdout: " << result.stdout_output);
            INFO("stderr: " << result.stderr_output);

            REQUIRE(result.success());
        }
    }

    SECTION("Echo with short timeout succeeds") {
        auto result = dcmtk_tool::echoscu(
            "localhost", port, ae_title, "ECHOSCU", std::chrono::seconds{5});

        REQUIRE(result.success());
    }
}

// =============================================================================
// Test: DCMTK storescp with pacs_system SCU
// =============================================================================

TEST_CASE("C-ECHO: DCMTK storescp with pacs_system SCU", "[dcmtk][interop][echo]") {
    if (!dcmtk_tool::is_available()) {
        SKIP("DCMTK not installed - skipping interoperability test");
    }

    // Setup: Start DCMTK store server (accepts echo)
    auto port = find_available_port();
    const std::string ae_title = "DCMTK_SCP";
    test_directory temp_dir;

    // Start DCMTK storescp (it also handles C-ECHO)
    auto dcmtk_server = dcmtk_tool::storescp(port, ae_title, temp_dir.path());
    REQUIRE(dcmtk_server.is_running());

    // Wait for DCMTK server to be ready
    REQUIRE(wait_for([&]() {
        return process_launcher::is_port_listening(port);
    }, dcmtk_server_ready_timeout()));

    SECTION("pacs_system SCU sends C-ECHO successfully") {
        // Connect using association
        auto connect_result = test_association::connect(
            "localhost", port, ae_title, "PACS_SCU",
            {std::string(verification_sop_class_uid)});

        REQUIRE(connect_result.is_ok());
        auto& assoc = connect_result.value();

        // Get accepted context
        REQUIRE(assoc.has_accepted_context(verification_sop_class_uid));
        auto context_id = assoc.accepted_context_id(verification_sop_class_uid);
        REQUIRE(context_id.has_value());

        // Send C-ECHO request
        auto echo_rq = make_c_echo_rq(1, verification_sop_class_uid);
        auto send_result = assoc.send_dimse(*context_id, echo_rq);
        REQUIRE(send_result.is_ok());

        // Receive C-ECHO response
        auto recv_result = assoc.receive_dimse();
        REQUIRE(recv_result.is_ok());

        auto& [recv_ctx, echo_rsp] = recv_result.value();
        REQUIRE(echo_rsp.command() == command_field::c_echo_rsp);
        REQUIRE(echo_rsp.status() == status_success);
    }

    SECTION("Multiple consecutive echoes from pacs_system") {
        auto connect_result = test_association::connect(
            "localhost", port, ae_title, "PACS_SCU",
            {std::string(verification_sop_class_uid)});

        REQUIRE(connect_result.is_ok());
        auto& assoc = connect_result.value();

        auto context_id = assoc.accepted_context_id(verification_sop_class_uid);
        REQUIRE(context_id.has_value());

        for (int i = 0; i < 5; ++i) {
            auto echo_rq = make_c_echo_rq(
                static_cast<uint16_t>(i + 1), verification_sop_class_uid);
            auto send_result = assoc.send_dimse(*context_id, echo_rq);
            REQUIRE(send_result.is_ok());

            auto recv_result = assoc.receive_dimse();
            REQUIRE(recv_result.is_ok());

            auto& [recv_ctx, echo_rsp] = recv_result.value();
            INFO("Iteration: " << i);
            REQUIRE(echo_rsp.command() == command_field::c_echo_rsp);
            REQUIRE(echo_rsp.status() == status_success);
        }
    }
}

// =============================================================================
// Test: DCMTK echoscp with pacs_system SCU
// =============================================================================

TEST_CASE("C-ECHO: DCMTK echoscp with pacs_system SCU", "[dcmtk][interop][echo]") {
    if (!dcmtk_tool::is_available()) {
        SKIP("DCMTK not installed - skipping interoperability test");
    }

    // Setup: Start DCMTK echo server
    auto port = find_available_port();
    const std::string ae_title = "DCMTK_ECHO";

    auto dcmtk_server = dcmtk_tool::echoscp(port, ae_title);
    REQUIRE(dcmtk_server.is_running());

    // Wait for DCMTK server to be ready
    REQUIRE(wait_for([&]() {
        return process_launcher::is_port_listening(port);
    }, dcmtk_server_ready_timeout()));

    SECTION("pacs_system SCU succeeds with DCMTK echoscp") {
        auto connect_result = test_association::connect(
            "localhost", port, ae_title, "PACS_SCU",
            {std::string(verification_sop_class_uid)});

        REQUIRE(connect_result.is_ok());
        auto& assoc = connect_result.value();

        auto context_id = assoc.accepted_context_id(verification_sop_class_uid);
        REQUIRE(context_id.has_value());

        auto echo_rq = make_c_echo_rq(1, verification_sop_class_uid);
        auto send_result = assoc.send_dimse(*context_id, echo_rq);
        REQUIRE(send_result.is_ok());

        auto recv_result = assoc.receive_dimse();
        REQUIRE(recv_result.is_ok());

        auto& [recv_ctx, echo_rsp] = recv_result.value();
        REQUIRE(echo_rsp.command() == command_field::c_echo_rsp);
        REQUIRE(echo_rsp.status() == status_success);
    }
}

// =============================================================================
// Test: Concurrent echo operations
// =============================================================================

TEST_CASE("C-ECHO: Concurrent echo operations", "[dcmtk][interop][echo][stress]") {
    if (!dcmtk_tool::is_available()) {
        SKIP("DCMTK not installed");
    }

    auto port = find_available_port();
    const std::string ae_title = "STRESS_SCP";

    test_server server(port, ae_title);
    server.register_service(std::make_shared<verification_scp>());
    REQUIRE(server.start());

    // Wait for server to be ready
    REQUIRE(wait_for([&]() {
        return process_launcher::is_port_listening(port);
    }, server_ready_timeout()));

    SECTION("5 concurrent DCMTK echoscu clients") {
        constexpr int num_clients = 5;
        std::vector<std::future<dcmtk_result>> futures;
        futures.reserve(num_clients);

        for (int i = 0; i < num_clients; ++i) {
            futures.push_back(std::async(std::launch::async, [&, i]() {
                return dcmtk_tool::echoscu(
                    "localhost", port, ae_title,
                    "CLIENT_" + std::to_string(i));
            }));
        }

        // All should succeed
        for (size_t i = 0; i < futures.size(); ++i) {
            auto result = futures[i].get();

            INFO("Client " << i << " stdout: " << result.stdout_output);
            INFO("Client " << i << " stderr: " << result.stderr_output);

            REQUIRE(result.success());
        }
    }

    SECTION("5 concurrent pacs_system SCU clients") {
        constexpr int num_clients = 5;
        std::vector<std::future<bool>> futures;
        futures.reserve(num_clients);

        for (int i = 0; i < num_clients; ++i) {
            futures.push_back(std::async(std::launch::async, [&, i]() {
                auto connect_result = test_association::connect(
                    "localhost", port, ae_title,
                    "PACS_CLIENT_" + std::to_string(i),
                    {std::string(verification_sop_class_uid)});

                if (!connect_result.is_ok()) {
                    return false;
                }

                auto& assoc = connect_result.value();
                auto context_id = assoc.accepted_context_id(verification_sop_class_uid);
                if (!context_id.has_value()) {
                    return false;
                }

                auto echo_rq = make_c_echo_rq(1, verification_sop_class_uid);
                auto send_result = assoc.send_dimse(*context_id, echo_rq);
                if (!send_result.is_ok()) {
                    return false;
                }

                auto recv_result = assoc.receive_dimse();
                if (!recv_result.is_ok()) {
                    return false;
                }

                auto& [recv_ctx, echo_rsp] = recv_result.value();
                return echo_rsp.command() == command_field::c_echo_rsp &&
                       echo_rsp.status() == status_success;
            }));
        }

        // All should succeed
        for (size_t i = 0; i < futures.size(); ++i) {
            bool success = futures[i].get();
            INFO("Client " << i);
            REQUIRE(success);
        }
    }
}

// =============================================================================
// Test: Connection error handling
// =============================================================================

TEST_CASE("C-ECHO: Connection error handling", "[dcmtk][interop][echo][error]") {
    if (!dcmtk_tool::is_available()) {
        SKIP("DCMTK not installed");
    }

    SECTION("echoscu to non-existent server fails gracefully") {
        auto port = find_available_port();

        // Ensure nothing is listening on this port
        REQUIRE_FALSE(process_launcher::is_port_listening(port));

        auto result = dcmtk_tool::echoscu(
            "localhost", port, "NONEXISTENT",
            "ECHOSCU", std::chrono::seconds{5});

        // Should fail - no server listening
        REQUIRE_FALSE(result.success());
    }

    SECTION("pacs_system SCU to non-existent server fails gracefully") {
        // Use a high port range that's less likely to have conflicts
        auto port = find_available_port(59000);

        // Wait briefly and re-verify the port is truly free
        std::this_thread::sleep_for(std::chrono::milliseconds{100});

        // Ensure nothing is listening on this port
        if (process_launcher::is_port_listening(port)) {
            SKIP("Port " + std::to_string(port) + " is unexpectedly in use");
        }

        auto connect_result = test_association::connect(
            "localhost", port, "NONEXISTENT", "PACS_SCU",
            {std::string(verification_sop_class_uid)});

        // Should fail - no server listening
        REQUIRE_FALSE(connect_result.is_ok());
    }
}

// =============================================================================
// Test: Protocol verification
// =============================================================================

TEST_CASE("C-ECHO: Protocol verification", "[dcmtk][interop][echo][protocol]") {
    if (!dcmtk_tool::is_available()) {
        SKIP("DCMTK not installed");
    }

    auto port = find_available_port();
    const std::string ae_title = "PROTOCOL_SCP";

    test_server server(port, ae_title);
    server.register_service(std::make_shared<verification_scp>());
    REQUIRE(server.start());

    REQUIRE(wait_for([&]() {
        return process_launcher::is_port_listening(port);
    }, server_ready_timeout()));

    SECTION("Verification SOP Class negotiation") {
        // echoscu should negotiate Verification SOP Class (1.2.840.10008.1.1)
        auto result = dcmtk_tool::echoscu("localhost", port, ae_title);

        REQUIRE(result.success());

        // The successful echo confirms proper SOP Class negotiation
    }

    SECTION("Association release after echo") {
        auto result = dcmtk_tool::echoscu("localhost", port, ae_title);

        REQUIRE(result.success());

        // Server should still be accepting new connections after release
        auto result2 = dcmtk_tool::echoscu("localhost", port, ae_title);

        REQUIRE(result2.success());
    }
}
