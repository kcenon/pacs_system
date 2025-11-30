/**
 * @file network_adapter_test.cpp
 * @brief Unit tests for network_adapter and dicom_session
 */

#include <pacs/integration/network_adapter.hpp>
#include <pacs/integration/dicom_session.hpp>
#include <pacs/network/server_config.hpp>
#include <pacs/network/dicom_server.hpp>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <thread>
#include <vector>

using namespace pacs::integration;
using namespace pacs::network;
using namespace std::chrono_literals;

// =============================================================================
// TLS Configuration Tests
// =============================================================================

TEST_CASE("tls_config validation", "[network_adapter][tls]") {
    SECTION("Disabled TLS is always valid") {
        tls_config config;
        config.enabled = false;

        REQUIRE(config.is_valid());
    }

    SECTION("Enabled TLS requires paths") {
        tls_config config;
        config.enabled = true;

        // Empty paths are invalid
        REQUIRE_FALSE(config.is_valid());

        // Setting cert path alone is not enough
        config.cert_path = "/some/path/cert.pem";
        REQUIRE_FALSE(config.is_valid());

        // Setting both cert and key paths makes it valid
        config.key_path = "/some/path/key.pem";
        REQUIRE(config.is_valid());
    }

    SECTION("Default values are sensible") {
        tls_config config;

        REQUIRE_FALSE(config.enabled);
        REQUIRE(config.verify_peer);
        REQUIRE(config.min_version == tls_config::tls_version::v1_2);
    }
}

// =============================================================================
// Connection Configuration Tests
// =============================================================================

TEST_CASE("connection_config construction", "[network_adapter][config]") {
    SECTION("Default construction") {
        connection_config config;

        REQUIRE(config.host.empty());
        REQUIRE(config.port == 104);  // Standard DICOM port
        REQUIRE(config.timeout == 30000ms);
        REQUIRE_FALSE(config.tls.enabled);
    }

    SECTION("Parameterized construction") {
        connection_config config{"192.168.1.100", 11112};

        REQUIRE(config.host == "192.168.1.100");
        REQUIRE(config.port == 11112);
    }

    SECTION("TLS can be configured") {
        connection_config config{"localhost", 2762};  // DICOM TLS port
        config.tls.enabled = true;
        config.tls.cert_path = "/path/to/cert.pem";
        config.tls.key_path = "/path/to/key.pem";
        config.tls.min_version = tls_config::tls_version::v1_3;

        REQUIRE(config.tls.enabled);
        REQUIRE(config.tls.is_valid());
    }
}

// =============================================================================
// TLS Validation Tests
// =============================================================================

TEST_CASE("network_adapter TLS configuration", "[network_adapter][tls]") {
    SECTION("Disabled TLS passes validation") {
        tls_config config;
        config.enabled = false;

        auto result = network_adapter::configure_tls(config);
        REQUIRE(result.is_ok());
    }

    SECTION("Non-existent certificate file fails validation") {
        tls_config config;
        config.enabled = true;
        config.cert_path = "/nonexistent/path/cert.pem";
        config.key_path = "/nonexistent/path/key.pem";

        auto result = network_adapter::configure_tls(config);
        REQUIRE(result.is_err());
    }
}

// =============================================================================
// Server Creation Tests
// =============================================================================

TEST_CASE("network_adapter server creation", "[network_adapter][server]") {
    SECTION("Valid configuration creates server") {
        server_config config;
        config.ae_title = "TEST_SCP";
        config.port = 11113;
        config.max_associations = 10;

        auto server = network_adapter::create_server(config);
        REQUIRE(server != nullptr);
    }

    SECTION("Empty AE title returns nullptr") {
        server_config config;
        config.ae_title = "";
        config.port = 11113;

        auto server = network_adapter::create_server(config);
        REQUIRE(server == nullptr);
    }

    SECTION("Zero port returns nullptr") {
        server_config config;
        config.ae_title = "TEST_SCP";
        config.port = 0;

        auto server = network_adapter::create_server(config);
        REQUIRE(server == nullptr);
    }

    SECTION("Invalid TLS config returns nullptr") {
        server_config srv_config;
        srv_config.ae_title = "TEST_SCP";
        srv_config.port = 11113;

        tls_config tls;
        tls.enabled = true;
        // Missing required paths

        auto server = network_adapter::create_server(srv_config, tls);
        REQUIRE(server == nullptr);
    }
}

// =============================================================================
// Connection Tests
// =============================================================================

TEST_CASE("network_adapter connection", "[network_adapter][connection]") {
    SECTION("Empty host returns error") {
        connection_config config;
        config.host = "";
        config.port = 104;

        auto result = network_adapter::connect(config);
        REQUIRE(result.is_err());
    }

    SECTION("Zero port returns error") {
        connection_config config;
        config.host = "localhost";
        config.port = 0;

        auto result = network_adapter::connect(config);
        REQUIRE(result.is_err());
    }

    SECTION("Invalid TLS configuration returns error") {
        connection_config config;
        config.host = "localhost";
        config.port = 104;
        config.tls.enabled = true;
        // Missing required paths

        auto result = network_adapter::connect(config);
        REQUIRE(result.is_err());
    }

    SECTION("Simplified connect API works") {
        auto result = network_adapter::connect("", 104);
        REQUIRE(result.is_err());  // Empty host
    }
}

// =============================================================================
// Session Wrapping Tests
// =============================================================================

TEST_CASE("network_adapter session wrapping", "[network_adapter][session]") {
    SECTION("Null session returns nullptr") {
        std::shared_ptr<network_system::session::messaging_session> null_session;
        auto wrapped = network_adapter::wrap_session(null_session);
        REQUIRE(wrapped == nullptr);
    }

    SECTION("Null secure session returns nullptr") {
        std::shared_ptr<network_system::session::secure_session> null_session;
        auto wrapped = network_adapter::wrap_session(null_session);
        REQUIRE(wrapped == nullptr);
    }
}

// =============================================================================
// PDU Data Tests
// =============================================================================

TEST_CASE("pdu_data structure", "[dicom_session][pdu]") {
    SECTION("Default construction") {
        pdu_data pdu;
        REQUIRE(pdu.type == pdu_type::abort);
        REQUIRE(pdu.payload.empty());
    }

    SECTION("Parameterized construction") {
        std::vector<uint8_t> payload = {0x01, 0x02, 0x03};
        pdu_data pdu(pdu_type::associate_rq, payload);

        REQUIRE(pdu.type == pdu_type::associate_rq);
        REQUIRE(pdu.payload.size() == 3);
        REQUIRE(pdu.payload[0] == 0x01);
    }

    SECTION("Move construction works") {
        std::vector<uint8_t> payload = {0x01, 0x02, 0x03, 0x04};
        pdu_data pdu(pdu_type::p_data_tf, std::move(payload));

        REQUIRE(pdu.type == pdu_type::p_data_tf);
        REQUIRE(pdu.payload.size() == 4);
    }
}

// =============================================================================
// Server Configuration Tests
// =============================================================================

TEST_CASE("server_config defaults", "[network_adapter][server][config]") {
    SECTION("Default values are sensible") {
        server_config config;

        REQUIRE(config.ae_title == "PACS_SCP");
        REQUIRE(config.port == 11112);
        REQUIRE(config.max_associations == 20);
        REQUIRE(config.idle_timeout == std::chrono::seconds{300});
        REQUIRE(config.association_timeout == std::chrono::seconds{30});
        REQUIRE_FALSE(config.accept_unknown_calling_ae);
    }

    SECTION("Parameterized construction") {
        server_config config("MY_PACS", 104);

        REQUIRE(config.ae_title == "MY_PACS");
        REQUIRE(config.port == 104);
    }
}

// =============================================================================
// Integration Tests (require actual network)
// =============================================================================

TEST_CASE("network_adapter integration", "[network_adapter][integration][.slow]") {
    // These tests are marked as slow and may be skipped in CI

    SECTION("Server can be created and started") {
        server_config config;
        config.ae_title = "INT_TEST_SCP";
        config.port = 11199;  // High port to avoid conflicts

        auto server = network_adapter::create_server(config);
        REQUIRE(server != nullptr);

        // Note: Starting the server would require proper cleanup
        // and is better suited for integration tests
    }
}
