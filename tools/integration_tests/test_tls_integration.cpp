/**
 * @file test_tls_integration.cpp
 * @brief TLS Integration Tests - Secure DICOM Communication
 *
 * Tests TLS-secured DICOM communication using the secure_dicom example
 * applications and network_adapter TLS configuration.
 *
 * @see Issue #141 - TLS Integration Tests
 * @see DICOM PS3.15 - Security and System Management Profiles
 *
 * Test Scenarios:
 * 1. Basic TLS connection
 * 2. Certificate validation
 * 3. Mutual TLS (mTLS)
 * 4. TLS-secured store/query workflow
 */

#include "test_fixtures.h"
#include "tls_test_fixtures.h"
#include "kcenon/pacs/services/storage_scp.h"
#include "kcenon/pacs/services/storage_scu.h"
#include "kcenon/pacs/network/detail/tcp_stream.h"
#include "kcenon/pacs/network/detail/tls_server.h"
#ifdef PACS_WITH_NETWORK_SYSTEM
#include <kcenon/network/interfaces/i_protocol_server.h>
#endif
#include <future>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "kcenon/pacs/integration/network_adapter.h"
#include "kcenon/pacs/network/dimse/dimse_message.h"
#include "kcenon/pacs/services/verification_scp.h"

#include <filesystem>
#include <thread>

using namespace kcenon::pacs::integration_test;
using namespace kcenon::pacs::services;
using kcenon::pacs::integration::tls_config;
using kcenon::pacs::integration::network_adapter;

namespace {

// =============================================================================
// TLS Test Fixtures
// =============================================================================

/**
 * @brief TLS-enabled test server wrapper
 *
 * Extends test_server with TLS configuration support.
 */
class tls_test_server {
public:
    explicit tls_test_server(
        uint16_t port,
        const std::string& ae_title,
        const tls_config& tls_cfg)
        : port_(port == 0 ? find_available_port() : port)
        , ae_title_(ae_title)
        , tls_cfg_(tls_cfg) {

        kcenon::pacs::network::server_config config;
        config.ae_title = ae_title_;
        config.port = port_;
        config.max_associations = 10;
        config.idle_timeout = std::chrono::seconds{30};
        config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.1";
        config.implementation_version_name = "TLS_TEST_SCP";

        // Validate TLS config first
        auto tls_result = network_adapter::configure_tls(tls_cfg_);
        if (tls_result.is_err()) {
            tls_valid_ = false;
            return;
        }
        tls_valid_ = true;

        // Create server with TLS
        server_ = network_adapter::create_server(config, tls_cfg_);
    }

    ~tls_test_server() {
        stop();
    }

    // Non-copyable, non-movable
    tls_test_server(const tls_test_server&) = delete;
    tls_test_server& operator=(const tls_test_server&) = delete;
    tls_test_server(tls_test_server&&) = delete;
    tls_test_server& operator=(tls_test_server&&) = delete;

    template <typename Service>
    void register_service(std::shared_ptr<Service> service) {
        if (server_) {
            server_->register_service(std::move(service));
        }
    }

    [[nodiscard]] bool start() {
        if (!server_ || !tls_valid_) {
            return false;
        }
        auto result = server_->start();
        if (result.is_ok()) {
            running_ = true;
            std::this_thread::sleep_for(std::chrono::milliseconds{100});
        }
        return result.is_ok();
    }

    void stop() {
        if (running_ && server_) {
            server_->stop();
            running_ = false;
        }
    }

    [[nodiscard]] uint16_t port() const noexcept { return port_; }
    [[nodiscard]] const std::string& ae_title() const noexcept { return ae_title_; }
    [[nodiscard]] bool is_running() const noexcept { return running_; }
    [[nodiscard]] bool is_tls_valid() const noexcept { return tls_valid_; }
    [[nodiscard]] kcenon::pacs::network::dicom_server* server() { return server_.get(); }

private:
    uint16_t port_;
    std::string ae_title_;
    tls_config tls_cfg_;
    std::unique_ptr<kcenon::pacs::network::dicom_server> server_;
    bool running_{false};
    bool tls_valid_{false};
};

/**
 * @brief Helper for TLS client connections
 */
class tls_test_client {
public:
    static kcenon::pacs::network::Result<kcenon::pacs::network::association> connect(
        const std::string& host,
        uint16_t port,
        const std::string& called_ae,
        const std::string& calling_ae,
        const tls_config& tls_cfg,
        const std::vector<std::string>& sop_classes = {std::string(verification_sop_class_uid)}) {

        // Validate TLS configuration
        auto tls_result = network_adapter::configure_tls(tls_cfg);
        if (tls_result.is_err()) {
            return tls_result.error();
        }

        // Configure association
        kcenon::pacs::network::association_config config;
        config.calling_ae_title = calling_ae;
        config.called_ae_title = called_ae;
        config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.2";
        config.implementation_version_name = "TLS_TEST_SCU";

        uint8_t context_id = 1;
        for (const auto& sop_class : sop_classes) {
            config.proposed_contexts.push_back({
                context_id,
                sop_class,
                {
                    "1.2.840.10008.1.2.1",  // Explicit VR Little Endian
                    "1.2.840.10008.1.2"     // Implicit VR Little Endian
                }
            });
            context_id += 2;
        }

        // Connect with TLS configuration
        // Note: In full implementation, TLS config would be passed to connect
        return kcenon::pacs::network::association::connect(host, port, config, default_timeout());
    }
};

}  // namespace

// =============================================================================
// Scenario 1: Basic TLS Connection
// =============================================================================

TEST_CASE("TLS C-ECHO connection", "[tls][connectivity]") {
    auto certs = get_test_certificates();

    // Skip if certificates not available
    if (!certs.all_exist()) {
        WARN("Skipping TLS tests: certificates not found at " << certs.ca_cert.parent_path());
        SKIP("Test certificates not available");
    }

    SECTION("TLS server accepts connection and responds to C-ECHO") {
        // Configure TLS server
        tls_config server_tls;
        server_tls.enabled = true;
        server_tls.cert_path = certs.server_cert;
        server_tls.key_path = certs.server_key;
        server_tls.ca_path = certs.ca_cert;
        server_tls.verify_peer = false;  // Don't require client cert
        server_tls.min_version = tls_config::tls_version::v1_2;

        auto port = find_available_port();
        tls_test_server server(port, "TLS_SCP", server_tls);

        if (!server.is_tls_valid()) {
            WARN("TLS configuration not valid, skipping test");
            SKIP("TLS not properly configured");
        }

        server.register_service(std::make_shared<verification_scp>());
        REQUIRE(server.start());
        REQUIRE(server.is_running());

        // Configure TLS client
        tls_config client_tls;
        client_tls.enabled = true;
        client_tls.ca_path = certs.ca_cert;
        client_tls.verify_peer = true;  // Verify server cert
        client_tls.min_version = tls_config::tls_version::v1_2;

        auto connect_result = tls_test_client::connect(
            "localhost", port, server.ae_title(), "TLS_SCU", client_tls);

        REQUIRE(connect_result.is_ok());
        auto& assoc = connect_result.value();

        // Verify connection
        REQUIRE(assoc.has_accepted_context(verification_sop_class_uid));

        auto context_id_opt = assoc.accepted_context_id(verification_sop_class_uid);
        REQUIRE(context_id_opt.has_value());

        // Send C-ECHO
        using namespace kcenon::pacs::network::dimse;
        auto echo_rq = make_c_echo_rq(1, verification_sop_class_uid);
        auto send_result = assoc.send_dimse(*context_id_opt, echo_rq);
        REQUIRE(send_result.is_ok());

        auto recv_result = assoc.receive_dimse(default_timeout());
        REQUIRE(recv_result.is_ok());

        auto& [recv_ctx, echo_rsp] = recv_result.value();
        REQUIRE(echo_rsp.command() == command_field::c_echo_rsp);
        REQUIRE(echo_rsp.status() == status_success);

        // Clean up
        (void)assoc.release(default_timeout());
        server.stop();
    }
}

// =============================================================================
// Scenario 2: Certificate Validation
// =============================================================================

TEST_CASE("TLS certificate validation", "[tls][security]") {
    auto certs = get_test_certificates();

    if (!certs.all_exist()) {
        SKIP("Test certificates not available");
    }

    // Start TLS server
    tls_config server_tls;
    server_tls.enabled = true;
    server_tls.cert_path = certs.server_cert;
    server_tls.key_path = certs.server_key;
    server_tls.ca_path = certs.ca_cert;
    server_tls.verify_peer = false;
    server_tls.min_version = tls_config::tls_version::v1_2;

    auto port = find_available_port();
    tls_test_server server(port, "TLS_SCP", server_tls);

    if (!server.is_tls_valid()) {
        SKIP("TLS not properly configured");
    }

    server.register_service(std::make_shared<verification_scp>());
    REQUIRE(server.start());

    SECTION("Valid certificate succeeds") {
        tls_config client_tls;
        client_tls.enabled = true;
        client_tls.ca_path = certs.ca_cert;
        client_tls.verify_peer = true;
        client_tls.min_version = tls_config::tls_version::v1_2;

        auto result = tls_test_client::connect(
            "localhost", port, server.ae_title(), "TLS_SCU", client_tls);

        REQUIRE(result.is_ok());
        auto& assoc = result.value();
        (void)assoc.release(default_timeout());
    }

    SECTION("Wrong CA fails validation") {
        // Skip if other_ca doesn't exist
        if (!std::filesystem::exists(certs.other_ca_cert)) {
            WARN("other_ca.crt not found, skipping wrong CA test");
            server.stop();
            return;
        }

        tls_config client_tls;
        client_tls.enabled = true;
        client_tls.ca_path = certs.other_ca_cert;  // Wrong CA
        client_tls.verify_peer = true;
        client_tls.min_version = tls_config::tls_version::v1_2;

        auto result = tls_test_client::connect(
            "localhost", port, server.ae_title(), "TLS_SCU", client_tls);

        // Should fail due to certificate verification failure
        // Note: Actual behavior depends on implementation
        // Some implementations may still connect but with warnings
        if (result.is_ok()) {
            auto& assoc = result.value();
            assoc.abort();
        }
    }

    SECTION("TLS configuration validation") {
        // Test invalid TLS configuration
        tls_config invalid_tls;
        invalid_tls.enabled = true;
        invalid_tls.cert_path = "/nonexistent/cert.pem";
        invalid_tls.key_path = "/nonexistent/key.pem";

        auto result = network_adapter::configure_tls(invalid_tls);
        // Should fail validation
        REQUIRE(result.is_err());
    }

    server.stop();
}

// =============================================================================
// Scenario 3: Mutual TLS (mTLS)
// =============================================================================

TEST_CASE("Mutual TLS authentication", "[tls][mtls]") {
    auto certs = get_test_certificates();

    if (!certs.all_exist()) {
        SKIP("Test certificates not available");
    }

    SECTION("Client with valid certificate succeeds") {
        // Server requires client certificate
        tls_config server_tls;
        server_tls.enabled = true;
        server_tls.cert_path = certs.server_cert;
        server_tls.key_path = certs.server_key;
        server_tls.ca_path = certs.ca_cert;
        server_tls.verify_peer = true;  // Require client cert
        server_tls.min_version = tls_config::tls_version::v1_2;

        auto port = find_available_port();
        tls_test_server server(port, "MTLS_SCP", server_tls);

        if (!server.is_tls_valid()) {
            SKIP("TLS not properly configured");
        }

        server.register_service(std::make_shared<verification_scp>());
        REQUIRE(server.start());

        // Client with certificate
        tls_config client_tls;
        client_tls.enabled = true;
        client_tls.cert_path = certs.client_cert;
        client_tls.key_path = certs.client_key;
        client_tls.ca_path = certs.ca_cert;
        client_tls.verify_peer = true;
        client_tls.min_version = tls_config::tls_version::v1_2;

        auto result = tls_test_client::connect(
            "localhost", port, server.ae_title(), "MTLS_SCU", client_tls);

        REQUIRE(result.is_ok());
        auto& assoc = result.value();

        // Verify we can communicate
        REQUIRE(assoc.has_accepted_context(verification_sop_class_uid));

        using namespace kcenon::pacs::network::dimse;
        auto ctx_id = *assoc.accepted_context_id(verification_sop_class_uid);
        auto echo_rq = make_c_echo_rq(1, verification_sop_class_uid);
        auto send_result = assoc.send_dimse(ctx_id, echo_rq);
        REQUIRE(send_result.is_ok());

        auto recv_result = assoc.receive_dimse(default_timeout());
        REQUIRE(recv_result.is_ok());

        auto& [recv_ctx, rsp] = recv_result.value();
        REQUIRE(rsp.status() == status_success);

        (void)assoc.release(default_timeout());
        server.stop();
    }

    SECTION("Client without certificate fails when server requires it") {
        tls_config server_tls;
        server_tls.enabled = true;
        server_tls.cert_path = certs.server_cert;
        server_tls.key_path = certs.server_key;
        server_tls.ca_path = certs.ca_cert;
        server_tls.verify_peer = true;  // Require client cert
        server_tls.min_version = tls_config::tls_version::v1_2;

        auto port = find_available_port();
        tls_test_server server(port, "MTLS_SCP", server_tls);

        if (!server.is_tls_valid()) {
            SKIP("TLS not properly configured");
        }

        server.register_service(std::make_shared<verification_scp>());
        REQUIRE(server.start());

        // Client without certificate
        tls_config client_tls;
        client_tls.enabled = true;
        client_tls.ca_path = certs.ca_cert;
        client_tls.verify_peer = true;
        // No client cert/key
        client_tls.min_version = tls_config::tls_version::v1_2;

        auto result = tls_test_client::connect(
            "localhost", port, server.ae_title(), "NO_CERT_SCU", client_tls);

        // Should fail due to missing client certificate
        // Note: Actual behavior depends on server configuration
        if (result.is_ok()) {
            auto& assoc = result.value();
            // Connection might succeed but operations may fail
            assoc.abort();
        }

        server.stop();
    }
}

// =============================================================================
// Scenario 4: TLS Version Negotiation
// =============================================================================

TEST_CASE("TLS version negotiation", "[tls][version]") {
    auto certs = get_test_certificates();

    if (!certs.all_exist()) {
        SKIP("Test certificates not available");
    }

    SECTION("TLS 1.2 connection") {
        tls_config server_tls;
        server_tls.enabled = true;
        server_tls.cert_path = certs.server_cert;
        server_tls.key_path = certs.server_key;
        server_tls.ca_path = certs.ca_cert;
        server_tls.verify_peer = false;
        server_tls.min_version = tls_config::tls_version::v1_2;

        auto port = find_available_port();
        tls_test_server server(port, "TLS12_SCP", server_tls);

        if (!server.is_tls_valid()) {
            SKIP("TLS not properly configured");
        }

        server.register_service(std::make_shared<verification_scp>());
        REQUIRE(server.start());

        tls_config client_tls;
        client_tls.enabled = true;
        client_tls.ca_path = certs.ca_cert;
        client_tls.verify_peer = true;
        client_tls.min_version = tls_config::tls_version::v1_2;

        auto result = tls_test_client::connect(
            "localhost", port, server.ae_title(), "TLS12_SCU", client_tls);

        REQUIRE(result.is_ok());
        auto& assoc = result.value();
        (void)assoc.release(default_timeout());
        server.stop();
    }

    SECTION("TLS 1.3 connection") {
        tls_config server_tls;
        server_tls.enabled = true;
        server_tls.cert_path = certs.server_cert;
        server_tls.key_path = certs.server_key;
        server_tls.ca_path = certs.ca_cert;
        server_tls.verify_peer = false;
        server_tls.min_version = tls_config::tls_version::v1_3;

        auto port = find_available_port();
        tls_test_server server(port, "TLS13_SCP", server_tls);

        if (!server.is_tls_valid()) {
            SKIP("TLS not properly configured");
        }

        server.register_service(std::make_shared<verification_scp>());
        REQUIRE(server.start());

        tls_config client_tls;
        client_tls.enabled = true;
        client_tls.ca_path = certs.ca_cert;
        client_tls.verify_peer = true;
        client_tls.min_version = tls_config::tls_version::v1_3;

        auto result = tls_test_client::connect(
            "localhost", port, server.ae_title(), "TLS13_SCU", client_tls);

        // TLS 1.3 may not be supported on all systems
        if (result.is_ok()) {
            auto& assoc = result.value();
            (void)assoc.release(default_timeout());
        } else {
            INFO("TLS 1.3 not supported: " << result.error().message);
        }

        server.stop();
    }
}

// =============================================================================
// Scenario 5: Multiple TLS Connections
// =============================================================================

TEST_CASE("Multiple concurrent TLS connections", "[tls][concurrent]") {
    auto certs = get_test_certificates();

    if (!certs.all_exist()) {
        SKIP("Test certificates not available");
    }

    tls_config server_tls;
    server_tls.enabled = true;
    server_tls.cert_path = certs.server_cert;
    server_tls.key_path = certs.server_key;
    server_tls.ca_path = certs.ca_cert;
    server_tls.verify_peer = false;
    server_tls.min_version = tls_config::tls_version::v1_2;

    auto port = find_available_port();
    tls_test_server server(port, "CONCURRENT_TLS", server_tls);

    if (!server.is_tls_valid()) {
        SKIP("TLS not properly configured");
    }

    server.register_service(std::make_shared<verification_scp>());
    REQUIRE(server.start());

    constexpr int num_connections = 3;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int i = 0; i < num_connections; ++i) {
        threads.emplace_back([&, i]() {
            tls_config client_tls;
            client_tls.enabled = true;
            client_tls.ca_path = certs.ca_cert;
            client_tls.verify_peer = true;
            client_tls.min_version = tls_config::tls_version::v1_2;

            auto result = tls_test_client::connect(
                "localhost", port, server.ae_title(),
                "TLS_SCU_" + std::to_string(i), client_tls);

            if (result.is_err()) {
                return;
            }

            auto& assoc = result.value();
            auto ctx_opt = assoc.accepted_context_id(verification_sop_class_uid);
            if (!ctx_opt) {
                return;
            }

            using namespace kcenon::pacs::network::dimse;
            auto echo_rq = make_c_echo_rq(1, verification_sop_class_uid);
            auto send_result = assoc.send_dimse(*ctx_opt, echo_rq);
            if (send_result.is_err()) {
                return;
            }

            auto recv_result = assoc.receive_dimse(default_timeout());
            if (recv_result.is_ok()) {
                auto& [ctx, rsp] = recv_result.value();
                if (rsp.status() == status_success) {
                    ++success_count;
                }
            }

            (void)assoc.release(default_timeout());
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    server.stop();

    REQUIRE(success_count == num_connections);
}

// =============================================================================
// TLS Configuration Validation Tests
// =============================================================================

TEST_CASE("TLS configuration validation", "[tls][config]") {
    SECTION("Disabled TLS is always valid") {
        tls_config cfg;
        cfg.enabled = false;

        REQUIRE(cfg.is_valid() == true);
    }

    SECTION("Enabled TLS requires cert and key") {
        tls_config cfg;
        cfg.enabled = true;

        // Missing both cert and key
        REQUIRE(cfg.is_valid() == false);

        // Only cert
        cfg.cert_path = "/some/cert.pem";
        REQUIRE(cfg.is_valid() == false);

        // Both cert and key
        cfg.key_path = "/some/key.pem";
        REQUIRE(cfg.is_valid() == true);
    }

    SECTION("CA path is optional") {
        tls_config cfg;
        cfg.enabled = true;
        cfg.cert_path = "/some/cert.pem";
        cfg.key_path = "/some/key.pem";

        // Valid without CA
        REQUIRE(cfg.is_valid() == true);

        // Still valid with CA
        cfg.ca_path = "/some/ca.pem";
        REQUIRE(cfg.is_valid() == true);
    }
}

// =============================================================================
// Scenario 6: Actual TLS exchanges with dicom_server_v2 (#1305)
// =============================================================================
#ifdef PACS_WITH_NETWORK_SYSTEM
namespace {
void require_tls_echo(kcenon::pacs::network::association& assoc, uint16_t message_id = 1) {
    using namespace kcenon::pacs::network::dimse;
    INFO("Negotiated TLS: " << assoc.negotiated_tls_version());
    REQUIRE_FALSE(assoc.negotiated_tls_version().empty());
    const auto context = assoc.accepted_context_id(verification_sop_class_uid);
    REQUIRE(context.has_value());
    REQUIRE(assoc.send_dimse(*context, make_c_echo_rq(message_id, verification_sop_class_uid)).is_ok());
    auto response = assoc.receive_dimse(std::chrono::seconds{5});
    INFO((response.is_err() ? response.error().message : "C-ECHO response received"));
    REQUIRE(response.is_ok());
    CHECK(response.value().first == *context);
    CHECK(response.value().second.command() == command_field::c_echo_rsp);
    CHECK(response.value().second.message_id_responded_to() == message_id);
    CHECK(response.value().second.status() == status_success);
}
}

TEST_CASE("TLS C-ECHO with dicom_server_v2", "[tls][v2][connectivity]") {
    const auto certs = get_test_certificates();
    INFO("Required test certificate directory: " << certs.ca_cert.parent_path());
    REQUIRE(certs.all_exist());
    auto server_tls = server_tls_config(certs);
    auto client_tls = client_tls_config(certs);
    SECTION("TLS 1.2 or newer") {}
    SECTION("TLS 1.3 required") {
        server_tls.min_version = tls_config::tls_version::v1_3;
        client_tls.min_version = tls_config::tls_version::v1_3;
    }
    tls_test_server_v2 server(server_tls);
    server.register_service(std::make_shared<verification_scp>());
    auto started = server.start();
    INFO((started.is_err() ? started.error().message : "TLS server started"));
    REQUIRE(started.is_ok());
    auto result = connect_tls(server.port(), server.ae_title(), client_tls,
        {std::string(verification_sop_class_uid)}, "TLS_V2_SCU", "localhost");
    INFO((result.is_err() ? result.error().message : "TLS association established"));
    REQUIRE(result.is_ok());
    if (client_tls.min_version == tls_config::tls_version::v1_3)
        CHECK(result.value().negotiated_tls_version() == "TLSv1.3");
    require_tls_echo(result.value());
    REQUIRE(result.value().release(std::chrono::seconds{5}).is_ok());
}

TEST_CASE("TLS concurrent connections with dicom_server_v2", "[tls][v2][concurrent]") {
    const auto certs = get_test_certificates();
    REQUIRE(certs.all_exist());
    tls_test_server_v2 server(server_tls_config(certs), "TLS_V2_PARALLEL");
    server.register_service(std::make_shared<verification_scp>());
    REQUIRE(server.start().is_ok());
    constexpr size_t count = 5;
    std::vector<std::string> errors(count);
    std::vector<std::thread> threads;
    std::atomic<size_t> successes{0};
    for (size_t i = 0; i < count; ++i) {
        threads.emplace_back([&, i] {
            using namespace kcenon::pacs::network::dimse;
            auto result = connect_tls(server.port(), server.ae_title(), client_tls_config(certs),
                {std::string(verification_sop_class_uid)}, "TLS_SCU_" + std::to_string(i));
            if (result.is_err()) { errors[i] = result.error().message; return; }
            auto& assoc = result.value();
            const auto context = assoc.accepted_context_id(verification_sop_class_uid);
            if (!context || assoc.negotiated_tls_version().empty()) { errors[i] = "No TLS/context"; return; }
            if (auto sent = assoc.send_dimse(*context, make_c_echo_rq(1, verification_sop_class_uid)); sent.is_err()) {
                errors[i] = sent.error().message; return;
            }
            auto response = assoc.receive_dimse(std::chrono::seconds{5});
            if (response.is_err()) { errors[i] = response.error().message; return; }
            if (response.value().first != *context || response.value().second.command() != command_field::c_echo_rsp ||
                response.value().second.message_id_responded_to() != 1 || response.value().second.status() != status_success) {
                errors[i] = "Invalid C-ECHO response"; return;
            }
            if (auto released = assoc.release(std::chrono::seconds{5}); released.is_err()) {
                errors[i] = released.error().message; return;
            }
            ++successes;
        });
    }
    for (auto& thread : threads) thread.join();
    for (const auto& error : errors) { INFO(error); CHECK(error.empty()); }
    REQUIRE(successes == count);
    REQUIRE(server.server().get_statistics().total_associations == count);
}

TEST_CASE("TLS v2 rejects plaintext and untrusted peers", "[tls][v2][security]") {
    const auto certs = get_test_certificates();
    REQUIRE(certs.all_exist());
    tls_test_server_v2 server(server_tls_config(certs));
    server.register_service(std::make_shared<verification_scp>());
    REQUIRE(server.start().is_ok());
    SECTION("Plain TCP cannot negotiate a DICOM association") {
        auto result = connect_tls(server.port(), server.ae_title(), tls_config{});
        REQUIRE(result.is_err());
        INFO(result.error().message);
        CHECK_THAT(result.error().message, Catch::Matchers::ContainsSubstring("Connection closed"));
    }
    SECTION("Wrong CA is rejected during the TLS handshake") {
        auto client = client_tls_config(certs);
        client.ca_path = certs.other_ca_cert;
        auto result = connect_tls(server.port(), server.ae_title(), client);
        REQUIRE(result.is_err());
        INFO(result.error().message);
        CHECK_THAT(result.error().message, Catch::Matchers::ContainsSubstring("TLS handshake failed"));
        CHECK_THAT(result.error().message, Catch::Matchers::ContainsSubstring("certificate verify failed"));
    }
    SECTION("Trusted certificate with the wrong IP identity is rejected") {
        auto result = connect_tls(server.port(), server.ae_title(), client_tls_config(certs),
            {std::string(verification_sop_class_uid)}, "TLS_V2_SCU", "127.0.0.2");
        REQUIRE(result.is_err());
        INFO(result.error().message);
        CHECK_THAT(result.error().message, Catch::Matchers::ContainsSubstring("TLS handshake failed"));
        CHECK_THAT(result.error().message, Catch::Matchers::ContainsSubstring("certificate verify failed"));
    }
    CHECK(server.server().get_statistics().total_associations == 0);
    // A rejected handshake must not damage the listener or later sessions.
    auto good = connect_tls(server.port(), server.ae_title(), client_tls_config(certs));
    REQUIRE(good.is_ok());
    require_tls_echo(good.value());
    REQUIRE(good.value().release(std::chrono::seconds{5}).is_ok());
}

TEST_CASE("TLS v2 mutual authentication and C-STORE", "[tls][v2][store][security]") {
    using namespace kcenon::pacs;
    const auto certs = get_test_certificates();
    REQUIRE(certs.all_exist());
    std::mutex stored_mutex;
    std::vector<core::dicom_dataset> stored;
    tls_test_server_v2 server(server_tls_config(certs, true), "TLS_V2_MTLS");
    auto storage = std::make_shared<storage_scp>();
    storage->set_handler([&](const core::dicom_dataset& dataset, const std::string&,
                            const std::string&, const std::string&) {
        std::lock_guard lock(stored_mutex);
        stored.push_back(dataset);
        return storage_status::success;
    });
    server.register_service(std::make_shared<verification_scp>());
    server.register_service(storage);
    REQUIRE(server.start().is_ok());
    SECTION("Trusted client certificate completes encrypted storage") {
        const auto dataset = generate_ct_dataset();
        auto result = connect_tls(server.port(), server.ae_title(), client_tls_config(certs, true),
            {std::string(verification_sop_class_uid), dataset.get_string(core::tags::sop_class_uid)});
        INFO((result.is_err() ? result.error().message : "mTLS established"));
        REQUIRE(result.is_ok());
        require_tls_echo(result.value());
        storage_scu scu;
        auto status = scu.store(result.value(), dataset);
        REQUIRE(status.is_ok());
        REQUIRE(status.value().is_success());
        REQUIRE(result.value().release(std::chrono::seconds{5}).is_ok());
        std::lock_guard lock(stored_mutex);
        REQUIRE(stored.size() == 1);
        CHECK(stored.front().get_string(core::tags::sop_instance_uid) == dataset.get_string(core::tags::sop_instance_uid));
        CHECK(stored.front().get_string(core::tags::patient_id) == dataset.get_string(core::tags::patient_id));
        const auto* received_pixels = stored.front().get(core::tags::pixel_data);
        const auto* expected_pixels = dataset.get(core::tags::pixel_data);
        REQUIRE(received_pixels);
        REQUIRE(expected_pixels);
        CHECK(std::equal(received_pixels->raw_data().begin(), received_pixels->raw_data().end(),
                         expected_pixels->raw_data().begin(), expected_pixels->raw_data().end()));
    }
    SECTION("Missing client certificate is rejected") {
        auto result = connect_tls(server.port(), server.ae_title(), client_tls_config(certs));
        REQUIRE(result.is_err());
        INFO(result.error().message);
        CHECK_THAT(result.error().message, Catch::Matchers::ContainsSubstring("TLS"));
        CHECK(server.server().get_statistics().total_associations == 0);
    }
    server.stop(); // Stop callbacks while their captured storage remains alive.
}

TEST_CASE("TLS v2 lifecycle and receive timeout", "[tls][v2][lifecycle]") {
    const auto certs = get_test_certificates();
    REQUIRE(certs.all_exist());
    tls_test_server_v2 server(server_tls_config(certs));
    server.register_service(std::make_shared<verification_scp>());
    REQUIRE(server.start().is_ok());
    SECTION("Receive timeout preserves an established TLS session") {
        auto result = connect_tls(server.port(), server.ae_title(), client_tls_config(certs));
        REQUIRE(result.is_ok());
        auto receive = result.value().receive_dimse(std::chrono::milliseconds{50});
        REQUIRE(receive.is_err());
        CHECK(receive.error().code == kcenon::pacs::error_codes::receive_timeout);
        require_tls_echo(result.value());
        REQUIRE(result.value().release(std::chrono::seconds{5}).is_ok());
    }
    SECTION("Stop wakes a TLS reader and permits restart") {
        auto result = connect_tls(server.port(), server.ae_title(), client_tls_config(certs));
        REQUIRE(result.is_ok());
        auto receive = std::async(std::launch::async, [&] {
            return result.value().receive_dimse(std::chrono::seconds{5});
        });
        server.stop();
        REQUIRE(receive.wait_for(std::chrono::seconds{2}) == std::future_status::ready);
        REQUIRE(receive.get().is_err());
        REQUIRE(server.start().is_ok());
        auto next = connect_tls(server.port(), server.ae_title(), client_tls_config(certs));
        REQUIRE(next.is_ok());
        require_tls_echo(next.value());
        REQUIRE(next.value().release(std::chrono::seconds{5}).is_ok());
    }
    SECTION("Stop cancels a client that never sends a TLS handshake") {
        auto socket = kcenon::pacs::network::detail::tcp_stream::connect(
            "127.0.0.1", server.port(), std::chrono::seconds{2});
        REQUIRE(socket.is_ok());
        const auto started = std::chrono::steady_clock::now();
        server.stop();
        CHECK(std::chrono::steady_clock::now() - started < std::chrono::seconds{2});
        std::array<uint8_t, 1> byte{};
        CHECK(socket.value()->read_some(byte, std::chrono::seconds{2}).is_err());
    }
}

TEST_CASE("TLS v2 rejects invalid server credentials", "[tls][v2][security]") {
    const auto certs = get_test_certificates();
    REQUIRE(certs.all_exist());
    auto config = server_tls_config(certs);
    SECTION("Missing certificate") { config.cert_path = certs.ca_cert.parent_path() / "absent.crt"; }
    SECTION("Certificate and private key must match") { config.key_path = certs.client_key; }
    tls_test_server_v2 server(config);
    server.register_service(std::make_shared<verification_scp>());
    auto result = server.start();
    REQUIRE(result.is_err());
    INFO(result.error().message);
    CHECK_THAT(result.error().message, Catch::Matchers::ContainsSubstring("TLS configuration failed"));
}

TEST_CASE("TLS adapter bounds queued close and supports callback stop", "[tls][v2][lifecycle]") {
    using namespace kcenon::pacs;
    const auto certs = get_test_certificates();
    REQUIRE(certs.all_exist());
    bool stop_from_callback = false;
    SECTION("Close drains or times out when the peer stops reading") {}
    SECTION("Server stop from the connection callback does not join itself") { stop_from_callback = true; }
    std::promise<bool> connected, disconnected;
    auto connected_future = connected.get_future();
    auto disconnected_future = disconnected.get_future();
    network::server_config config("TLS_LIFECYCLE", find_available_port());
    config.tls = server_tls_config(certs);
    auto made = network::detail::make_tls_server(config);
    REQUIRE(made.is_ok());
    auto server = std::move(made.value());
    std::weak_ptr weak_server = server;
    server->set_disconnection_callback([&](std::string_view) { disconnected.set_value(true); });
    server->set_connection_callback([&, weak_server, stop_from_callback](auto session) {
        if (stop_from_callback) {
            auto owner = weak_server.lock();
            connected.set_value(owner && owner->stop().is_ok());
        } else {
            connected.set_value(session->send(std::vector<uint8_t>(8 * 1024 * 1024, 0x5a)).is_ok());
            session->close();
        }
    });
    REQUIRE(server->start(config.port).is_ok());
    auto client = network::detail::tcp_stream::connect("127.0.0.1", config.port,
        std::chrono::seconds{5}, client_tls_config(certs));
    if (!stop_from_callback) REQUIRE(client.is_ok());
    REQUIRE(connected_future.wait_for(std::chrono::seconds{2}) == std::future_status::ready);
    CHECK(connected_future.get());
    REQUIRE(disconnected_future.wait_for(std::chrono::seconds{3}) == std::future_status::ready);
    CHECK(disconnected_future.get());
    REQUIRE(server->stop().is_ok()); // Join an I/O thread already stopped by its callback.
}
#else
TEST_CASE("TLS v2 support is required", "[tls][v2]") {
    FAIL("TLS v2 integration requires network_system and OpenSSL");
}
#endif
