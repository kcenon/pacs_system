/**
 * @file atna_config_test.cpp
 * @brief Unit tests for ATNA audit configuration management
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "pacs/security/atna_config.hpp"

#include <string>

using namespace kcenon::pacs::security;
using Catch::Matchers::ContainsSubstring;

// =============================================================================
// Default Configuration Tests
// =============================================================================

TEST_CASE("make_default_atna_config returns sensible defaults",
          "[security][atna_config]") {
    auto config = make_default_atna_config();

    CHECK(config.enabled == false);
    CHECK(config.audit_source_id == "PACS_SYSTEM");
    CHECK(config.transport.protocol == syslog_transport_protocol::udp);
    CHECK(config.transport.host == "localhost");
    CHECK(config.transport.port == 514);
    CHECK(config.transport.app_name == "pacs_system");
    CHECK(config.audit_storage == true);
    CHECK(config.audit_query == true);
    CHECK(config.audit_authentication == true);
    CHECK(config.audit_security_alerts == true);
}

TEST_CASE("atna_config default-constructed matches make_default",
          "[security][atna_config]") {
    atna_config config;
    auto default_config = make_default_atna_config();

    CHECK(config.enabled == default_config.enabled);
    CHECK(config.audit_source_id == default_config.audit_source_id);
    CHECK(config.transport.host == default_config.transport.host);
    CHECK(config.transport.port == default_config.transport.port);
}

// =============================================================================
// JSON Serialization Tests
// =============================================================================

TEST_CASE("to_json produces valid JSON with all fields",
          "[security][atna_config]") {
    atna_config config;
    config.enabled = true;
    config.audit_source_id = "TEST_PACS";
    config.transport.host = "audit-server.local";
    config.transport.port = 6514;
    config.transport.protocol = syslog_transport_protocol::tls;
    config.transport.ca_cert_path = "/certs/ca.pem";
    config.audit_storage = true;
    config.audit_query = false;

    auto json = to_json(config);

    CHECK_THAT(json, ContainsSubstring(R"("enabled":true)"));
    CHECK_THAT(json, ContainsSubstring(R"("audit_source_id":"TEST_PACS")"));
    CHECK_THAT(json, ContainsSubstring(R"("protocol":"tls")"));
    CHECK_THAT(json, ContainsSubstring(R"("host":"audit-server.local")"));
    CHECK_THAT(json, ContainsSubstring(R"("port":6514)"));
    CHECK_THAT(json, ContainsSubstring(R"("ca_cert_path":"/certs/ca.pem")"));
    CHECK_THAT(json, ContainsSubstring(R"("audit_storage":true)"));
    CHECK_THAT(json, ContainsSubstring(R"("audit_query":false)"));
}

TEST_CASE("to_json escapes special characters in strings",
          "[security][atna_config]") {
    atna_config config;
    config.audit_source_id = "PACS \"System\" 01";

    auto json = to_json(config);

    CHECK_THAT(json, ContainsSubstring("PACS \\\"System\\\" 01"));
}

TEST_CASE("to_json default config produces disabled UDP config",
          "[security][atna_config]") {
    auto json = to_json(make_default_atna_config());

    CHECK_THAT(json, ContainsSubstring(R"("enabled":false)"));
    CHECK_THAT(json, ContainsSubstring(R"("protocol":"udp")"));
    CHECK_THAT(json, ContainsSubstring(R"("host":"localhost")"));
    CHECK_THAT(json, ContainsSubstring(R"("port":514)"));
}

// =============================================================================
// JSON Parsing Tests
// =============================================================================

TEST_CASE("parse_atna_config parses complete JSON",
          "[security][atna_config]") {
    std::string json = R"({
        "enabled": true,
        "audit_source_id": "MY_PACS",
        "transport": {
            "protocol": "tls",
            "host": "audit.hospital.local",
            "port": 6514,
            "app_name": "my_pacs",
            "ca_cert_path": "/etc/certs/ca.pem",
            "client_cert_path": "/etc/certs/client.pem",
            "client_key_path": "/etc/certs/client.key",
            "verify_server": true
        },
        "audit_storage": true,
        "audit_query": false,
        "audit_authentication": true,
        "audit_security_alerts": false
    })";

    auto config = parse_atna_config(json);

    CHECK(config.enabled == true);
    CHECK(config.audit_source_id == "MY_PACS");
    CHECK(config.transport.protocol == syslog_transport_protocol::tls);
    CHECK(config.transport.host == "audit.hospital.local");
    CHECK(config.transport.port == 6514);
    CHECK(config.transport.app_name == "my_pacs");
    CHECK(config.transport.ca_cert_path == "/etc/certs/ca.pem");
    CHECK(config.transport.client_cert_path == "/etc/certs/client.pem");
    CHECK(config.transport.client_key_path == "/etc/certs/client.key");
    CHECK(config.transport.verify_server == true);
    CHECK(config.audit_storage == true);
    CHECK(config.audit_query == false);
    CHECK(config.audit_authentication == true);
    CHECK(config.audit_security_alerts == false);
}

TEST_CASE("parse_atna_config with minimal JSON uses defaults",
          "[security][atna_config]") {
    std::string json = R"({"enabled": true})";

    auto config = parse_atna_config(json);

    CHECK(config.enabled == true);
    CHECK(config.audit_source_id == "PACS_SYSTEM");
    CHECK(config.transport.host == "localhost");
    CHECK(config.transport.port == 514);
    CHECK(config.transport.protocol == syslog_transport_protocol::udp);
    CHECK(config.audit_storage == true);
    CHECK(config.audit_query == true);
}

TEST_CASE("parse_atna_config with empty JSON returns defaults",
          "[security][atna_config]") {
    auto config = parse_atna_config("{}");

    CHECK(config.enabled == false);
    CHECK(config.audit_source_id == "PACS_SYSTEM");
    CHECK(config.transport.host == "localhost");
}

TEST_CASE("parse_atna_config handles UDP protocol",
          "[security][atna_config]") {
    std::string json = R"({
        "transport": {
            "protocol": "udp",
            "host": "syslog.local",
            "port": 514
        }
    })";

    auto config = parse_atna_config(json);

    CHECK(config.transport.protocol == syslog_transport_protocol::udp);
    CHECK(config.transport.host == "syslog.local");
    CHECK(config.transport.port == 514);
}

TEST_CASE("parse_atna_config with unknown protocol defaults to UDP",
          "[security][atna_config]") {
    std::string json = R"({
        "transport": {
            "protocol": "invalid_proto"
        }
    })";

    auto config = parse_atna_config(json);
    CHECK(config.transport.protocol == syslog_transport_protocol::udp);
}

TEST_CASE("parse_atna_config handles escaped strings",
          "[security][atna_config]") {
    std::string json = "{\"audit_source_id\": \"PACS \\\"Test\\\" System\"}";

    auto config = parse_atna_config(json);
    CHECK(config.audit_source_id == "PACS \"Test\" System");
}

// =============================================================================
// Round-trip Tests
// =============================================================================

TEST_CASE("to_json then parse_atna_config round-trips correctly",
          "[security][atna_config]") {
    atna_config original;
    original.enabled = true;
    original.audit_source_id = "ROUND_TRIP_TEST";
    original.transport.protocol = syslog_transport_protocol::tls;
    original.transport.host = "audit-server.example.com";
    original.transport.port = 6514;
    original.transport.app_name = "test_app";
    original.transport.ca_cert_path = "/ca.pem";
    original.transport.client_cert_path = "/client.pem";
    original.transport.client_key_path = "/client.key";
    original.transport.verify_server = false;
    original.audit_storage = false;
    original.audit_query = true;
    original.audit_authentication = false;
    original.audit_security_alerts = true;

    auto json = to_json(original);
    auto parsed = parse_atna_config(json);

    CHECK(parsed.enabled == original.enabled);
    CHECK(parsed.audit_source_id == original.audit_source_id);
    CHECK(parsed.transport.protocol == original.transport.protocol);
    CHECK(parsed.transport.host == original.transport.host);
    CHECK(parsed.transport.port == original.transport.port);
    CHECK(parsed.transport.app_name == original.transport.app_name);
    CHECK(parsed.transport.ca_cert_path == original.transport.ca_cert_path);
    CHECK(parsed.transport.client_cert_path ==
          original.transport.client_cert_path);
    CHECK(parsed.transport.client_key_path ==
          original.transport.client_key_path);
    CHECK(parsed.transport.verify_server == original.transport.verify_server);
    CHECK(parsed.audit_storage == original.audit_storage);
    CHECK(parsed.audit_query == original.audit_query);
    CHECK(parsed.audit_authentication == original.audit_authentication);
    CHECK(parsed.audit_security_alerts == original.audit_security_alerts);
}

// =============================================================================
// Validation Tests
// =============================================================================

TEST_CASE("validate passes for valid default config",
          "[security][atna_config]") {
    auto config = make_default_atna_config();
    auto result = validate(config);

    CHECK(result.valid == true);
    CHECK(result.errors.empty());
}

TEST_CASE("validate fails for empty audit_source_id",
          "[security][atna_config]") {
    atna_config config;
    config.audit_source_id = "";

    auto result = validate(config);

    CHECK(result.valid == false);
    REQUIRE(result.errors.size() >= 1);
    CHECK_THAT(result.errors[0],
               ContainsSubstring("audit_source_id"));
}

TEST_CASE("validate fails for empty host",
          "[security][atna_config]") {
    atna_config config;
    config.transport.host = "";

    auto result = validate(config);

    CHECK(result.valid == false);
    bool found = false;
    for (const auto& err : result.errors) {
        if (err.find("host") != std::string::npos) {
            found = true;
            break;
        }
    }
    CHECK(found);
}

TEST_CASE("validate fails for zero port",
          "[security][atna_config]") {
    atna_config config;
    config.transport.port = 0;

    auto result = validate(config);

    CHECK(result.valid == false);
    bool found = false;
    for (const auto& err : result.errors) {
        if (err.find("port") != std::string::npos) {
            found = true;
            break;
        }
    }
    CHECK(found);
}

TEST_CASE("validate fails for TLS without ca_cert_path",
          "[security][atna_config]") {
    atna_config config;
    config.transport.protocol = syslog_transport_protocol::tls;
    config.transport.ca_cert_path = "";

    auto result = validate(config);

    CHECK(result.valid == false);
    bool found = false;
    for (const auto& err : result.errors) {
        if (err.find("ca_cert_path") != std::string::npos) {
            found = true;
            break;
        }
    }
    CHECK(found);
}

TEST_CASE("validate fails for TLS with non-existent ca_cert_path",
          "[security][atna_config]") {
    atna_config config;
    config.transport.protocol = syslog_transport_protocol::tls;
    config.transport.ca_cert_path = "/nonexistent/path/ca.pem";

    auto result = validate(config);

    CHECK(result.valid == false);
    bool found = false;
    for (const auto& err : result.errors) {
        if (err.find("does not exist") != std::string::npos) {
            found = true;
            break;
        }
    }
    CHECK(found);
}

TEST_CASE("validate passes for UDP without TLS cert paths",
          "[security][atna_config]") {
    atna_config config;
    config.transport.protocol = syslog_transport_protocol::udp;

    auto result = validate(config);

    CHECK(result.valid == true);
    CHECK(result.errors.empty());
}

TEST_CASE("validate collects multiple errors",
          "[security][atna_config]") {
    atna_config config;
    config.audit_source_id = "";
    config.transport.host = "";
    config.transport.port = 0;

    auto result = validate(config);

    CHECK(result.valid == false);
    CHECK(result.errors.size() == 3);
}

// =============================================================================
// Event Filtering Config Tests
// =============================================================================

TEST_CASE("event filtering fields are independent",
          "[security][atna_config]") {
    atna_config config;
    config.audit_storage = false;
    config.audit_query = true;
    config.audit_authentication = false;
    config.audit_security_alerts = true;

    CHECK(config.audit_storage == false);
    CHECK(config.audit_query == true);
    CHECK(config.audit_authentication == false);
    CHECK(config.audit_security_alerts == true);

    auto json = to_json(config);
    auto parsed = parse_atna_config(json);

    CHECK(parsed.audit_storage == false);
    CHECK(parsed.audit_query == true);
    CHECK(parsed.audit_authentication == false);
    CHECK(parsed.audit_security_alerts == true);
}
