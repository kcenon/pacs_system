/**
 * @file test_connectivity.cpp
 * @brief Scenario 1: Basic Connectivity Tests
 *
 * Tests basic DICOM connectivity using C-ECHO service.
 * Validates that Echo SCP and SCU can communicate successfully.
 *
 * @see Issue #111 - Integration Test Suite
 *
 * Test Workflow:
 * 1. Start Echo SCP
 * 2. Run Echo SCU -> Verify success
 * 3. Stop Echo SCP
 */

#include "test_fixtures.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "pacs/network/dimse/dimse_message.hpp"
#include "pacs/services/verification_scp.hpp"

#include <thread>

using namespace pacs::integration_test;
using namespace pacs::network;
using namespace pacs::network::dimse;
using namespace pacs::services;

// =============================================================================
// Scenario 1: Basic Connectivity
// =============================================================================

TEST_CASE("C-ECHO basic connectivity", "[connectivity][echo]") {
    SECTION("Echo SCP accepts connection and responds to C-ECHO") {
        // Step 1: Start Echo SCP
        auto port = find_available_port();
        test_server server(port, "ECHO_SCP");
        server.register_service(std::make_shared<verification_scp>());

        REQUIRE(server.start());
        REQUIRE(server.is_running());

        // Step 2: Connect and send C-ECHO
        auto connect_result = test_association::connect(
            "localhost",
            port,
            server.ae_title(),
            "ECHO_SCU",
            {std::string(verification_sop_class_uid)}
        );

        REQUIRE(connect_result.is_ok());
        auto& assoc = connect_result.value();

        // Verify we have accepted context for verification
        REQUIRE(assoc.has_accepted_context(verification_sop_class_uid));

        auto context_id_opt = assoc.accepted_context_id(verification_sop_class_uid);
        REQUIRE(context_id_opt.has_value());
        uint8_t context_id = *context_id_opt;

        // Create and send C-ECHO request
        auto echo_rq = make_c_echo_rq(1, verification_sop_class_uid);
        auto send_result = assoc.send_dimse(context_id, echo_rq);
        REQUIRE(send_result.is_ok());

        // Receive C-ECHO response
        auto recv_result = assoc.receive_dimse(default_timeout);
        REQUIRE(recv_result.is_ok());

        auto& [recv_context_id, echo_rsp] = recv_result.value();
        REQUIRE(echo_rsp.command() == command_field::c_echo_rsp);
        REQUIRE(echo_rsp.status() == status_success);

        // Release association gracefully
        auto release_result = assoc.release(default_timeout);
        REQUIRE(release_result.is_ok());

        // Step 3: Stop server
        server.stop();
        REQUIRE_FALSE(server.is_running());
    }
}

TEST_CASE("Multiple sequential C-ECHO requests", "[connectivity][echo]") {
    auto port = find_available_port();
    test_server server(port, "ECHO_SCP");
    server.register_service(std::make_shared<verification_scp>());

    REQUIRE(server.start());

    // Connect once
    auto connect_result = test_association::connect(
        "localhost", port, server.ae_title(), "ECHO_SCU",
        {std::string(verification_sop_class_uid)}
    );
    REQUIRE(connect_result.is_ok());
    auto& assoc = connect_result.value();

    auto context_id = *assoc.accepted_context_id(verification_sop_class_uid);

    // Send multiple C-ECHO requests on same association
    constexpr int echo_count = 5;
    for (int i = 0; i < echo_count; ++i) {
        auto echo_rq = make_c_echo_rq(static_cast<uint16_t>(i + 1), verification_sop_class_uid);
        auto send_result = assoc.send_dimse(context_id, echo_rq);
        REQUIRE(send_result.is_ok());

        auto recv_result = assoc.receive_dimse(default_timeout);
        REQUIRE(recv_result.is_ok());

        auto& [recv_ctx, echo_rsp] = recv_result.value();
        REQUIRE(echo_rsp.command() == command_field::c_echo_rsp);
        REQUIRE(echo_rsp.status() == status_success);
    }

    (void)assoc.release(default_timeout);
    server.stop();
}

TEST_CASE("Multiple concurrent associations", "[connectivity][echo]") {
    auto port = find_available_port();
    test_server server(port, "ECHO_SCP");
    server.register_service(std::make_shared<verification_scp>());

    REQUIRE(server.start());

    constexpr int num_associations = 5;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int i = 0; i < num_associations; ++i) {
        threads.emplace_back([&server, &success_count, port, i]() {
            auto connect_result = test_association::connect(
                "localhost", port, server.ae_title(),
                "ECHO_SCU_" + std::to_string(i),
                {std::string(verification_sop_class_uid)}
            );

            if (connect_result.is_err()) {
                return;
            }

            auto& assoc = connect_result.value();
            auto context_id_opt = assoc.accepted_context_id(verification_sop_class_uid);
            if (!context_id_opt) {
                return;
            }

            auto echo_rq = make_c_echo_rq(1, verification_sop_class_uid);
            auto send_result = assoc.send_dimse(*context_id_opt, echo_rq);
            if (send_result.is_err()) {
                return;
            }

            auto recv_result = assoc.receive_dimse(default_timeout);
            if (recv_result.is_ok()) {
                auto& [ctx, rsp] = recv_result.value();
                if (rsp.command() == command_field::c_echo_rsp &&
                    rsp.status() == status_success) {
                    ++success_count;
                }
            }

            (void)assoc.release(default_timeout);
        });
    }

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    server.stop();

    REQUIRE(success_count == num_associations);
}

TEST_CASE("Connection to non-existent server fails gracefully", "[connectivity][error]") {
    // Try to connect to a port that's not listening
    auto connect_result = test_association::connect(
        "localhost",
        find_available_port() + 1000,  // Very unlikely to be in use
        "NONEXISTENT",
        "ECHO_SCU",
        {std::string(verification_sop_class_uid)}
    );

    REQUIRE(connect_result.is_err());
}

TEST_CASE("Wrong AE title handling", "[connectivity][ae_title]") {
    auto port = find_available_port();
    test_server server(port, "CORRECT_AE");
    server.register_service(std::make_shared<verification_scp>());

    REQUIRE(server.start());

    // Note: In DICOM, AE title mismatch is typically handled during
    // association negotiation. The server may accept or reject based
    // on configuration. This test validates the connection behavior.
    auto connect_result = test_association::connect(
        "localhost",
        port,
        "WRONG_AE",  // Wrong AE title
        "ECHO_SCU",
        {std::string(verification_sop_class_uid)}
    );

    // The result depends on server configuration
    // Some servers reject, some accept any AE title
    // We just verify the operation completes without crash
    if (connect_result.is_ok()) {
        auto& assoc = connect_result.value();
        (void)assoc.release(default_timeout);
    }

    server.stop();
}

TEST_CASE("Association timeout handling", "[connectivity][timeout]") {
    auto port = find_available_port();
    test_server server(port, "ECHO_SCP");
    server.register_service(std::make_shared<verification_scp>());

    REQUIRE(server.start());

    auto connect_result = test_association::connect(
        "localhost", port, server.ae_title(), "ECHO_SCU",
        {std::string(verification_sop_class_uid)}
    );
    REQUIRE(connect_result.is_ok());
    auto& assoc = connect_result.value();

    // Try to receive without sending - should timeout
    auto short_timeout = std::chrono::milliseconds{100};
    auto recv_result = assoc.receive_dimse(short_timeout);

    // Should timeout (error result)
    REQUIRE(recv_result.is_err());

    // Abort connection (since we didn't properly communicate)
    assoc.abort();

    server.stop();
}
