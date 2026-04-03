/**
 * @file atna_syslog_transport_test.cpp
 * @brief Unit tests for ATNA Syslog transport
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "kcenon/pacs/security/atna_syslog_transport.h"
#include "kcenon/pacs/security/atna_audit_logger.h"

#include <string>

using namespace kcenon::pacs::security;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::StartsWith;

// =============================================================================
// Syslog Enum Tests
// =============================================================================

TEST_CASE("syslog_facility values match RFC 5424", "[security][syslog]") {
    CHECK(static_cast<uint8_t>(syslog_facility::kern) == 0);
    CHECK(static_cast<uint8_t>(syslog_facility::user) == 1);
    CHECK(static_cast<uint8_t>(syslog_facility::auth) == 4);
    CHECK(static_cast<uint8_t>(syslog_facility::authpriv) == 10);
    CHECK(static_cast<uint8_t>(syslog_facility::log_audit) == 13);
    CHECK(static_cast<uint8_t>(syslog_facility::local0) == 16);
    CHECK(static_cast<uint8_t>(syslog_facility::local7) == 23);
}

TEST_CASE("syslog_severity values match RFC 5424", "[security][syslog]") {
    CHECK(static_cast<uint8_t>(syslog_severity::emergency) == 0);
    CHECK(static_cast<uint8_t>(syslog_severity::alert) == 1);
    CHECK(static_cast<uint8_t>(syslog_severity::critical) == 2);
    CHECK(static_cast<uint8_t>(syslog_severity::error) == 3);
    CHECK(static_cast<uint8_t>(syslog_severity::warning) == 4);
    CHECK(static_cast<uint8_t>(syslog_severity::notice) == 5);
    CHECK(static_cast<uint8_t>(syslog_severity::informational) == 6);
    CHECK(static_cast<uint8_t>(syslog_severity::debug) == 7);
}

TEST_CASE("syslog_transport_protocol enum values", "[security][syslog]") {
    CHECK(static_cast<uint8_t>(syslog_transport_protocol::udp) == 0);
    CHECK(static_cast<uint8_t>(syslog_transport_protocol::tls) == 1);
}

// =============================================================================
// Configuration Tests
// =============================================================================

TEST_CASE("syslog_transport_config default values", "[security][syslog]") {
    syslog_transport_config config;

    CHECK(config.protocol == syslog_transport_protocol::udp);
    CHECK(config.host == "localhost");
    CHECK(config.port == 514);
    CHECK(config.app_name == "pacs_system");
    CHECK(config.hostname.empty());
    CHECK(config.facility == syslog_facility::authpriv);
    CHECK(config.severity == syslog_severity::informational);
    CHECK(config.ca_cert_path.empty());
    CHECK(config.client_cert_path.empty());
    CHECK(config.client_key_path.empty());
    CHECK(config.verify_server == true);
}

TEST_CASE("syslog_transport_config custom values", "[security][syslog]") {
    syslog_transport_config config;
    config.protocol = syslog_transport_protocol::tls;
    config.host = "audit.hospital.local";
    config.port = 6514;
    config.app_name = "my_pacs";
    config.hostname = "dicom-server-01";
    config.facility = syslog_facility::log_audit;
    config.severity = syslog_severity::notice;
    config.ca_cert_path = "/etc/pacs/ca.pem";
    config.verify_server = false;

    CHECK(config.protocol == syslog_transport_protocol::tls);
    CHECK(config.host == "audit.hospital.local");
    CHECK(config.port == 6514);
    CHECK(config.app_name == "my_pacs");
    CHECK(config.hostname == "dicom-server-01");
    CHECK(config.facility == syslog_facility::log_audit);
    CHECK(config.severity == syslog_severity::notice);
    CHECK(config.ca_cert_path == "/etc/pacs/ca.pem");
    CHECK(config.verify_server == false);
}

// =============================================================================
// Construction Tests
// =============================================================================

TEST_CASE("atna_syslog_transport constructs with default config",
          "[security][syslog]") {
    syslog_transport_config config;
    atna_syslog_transport transport(config);

    CHECK(transport.config().host == "localhost");
    CHECK(transport.config().port == 514);
    CHECK(transport.messages_sent() == 0);
    CHECK(transport.send_errors() == 0);
}

TEST_CASE("atna_syslog_transport auto-detects hostname",
          "[security][syslog]") {
    syslog_transport_config config;
    // hostname is empty, should be auto-detected
    atna_syslog_transport transport(config);

    // After construction, hostname should be filled
    CHECK_FALSE(transport.config().hostname.empty());
}

TEST_CASE("atna_syslog_transport preserves explicit hostname",
          "[security][syslog]") {
    syslog_transport_config config;
    config.hostname = "my-custom-host";
    atna_syslog_transport transport(config);

    CHECK(transport.config().hostname == "my-custom-host");
}

TEST_CASE("atna_syslog_transport is movable", "[security][syslog]") {
    syslog_transport_config config;
    config.host = "audit-server";
    config.port = 6514;

    atna_syslog_transport transport1(config);

    // Move construct
    atna_syslog_transport transport2(std::move(transport1));
    CHECK(transport2.config().host == "audit-server");
    CHECK(transport2.config().port == 6514);
}

TEST_CASE("atna_syslog_transport move assignment", "[security][syslog]") {
    syslog_transport_config config1;
    config1.host = "server1";

    syslog_transport_config config2;
    config2.host = "server2";

    atna_syslog_transport transport1(config1);
    atna_syslog_transport transport2(config2);

    transport2 = std::move(transport1);
    CHECK(transport2.config().host == "server1");
}

// =============================================================================
// RFC 5424 Message Formatting Tests
// =============================================================================

TEST_CASE("format_syslog_message produces RFC 5424 format",
          "[security][syslog]") {
    syslog_transport_config config;
    config.hostname = "pacs-host";
    config.app_name = "test_app";
    config.facility = syslog_facility::authpriv;
    config.severity = syslog_severity::informational;

    atna_syslog_transport transport(config);
    std::string xml = "<AuditMessage>test</AuditMessage>";

    auto msg = transport.format_syslog_message(xml);

    // PRI: authpriv(10) * 8 + informational(6) = 86
    CHECK_THAT(msg, StartsWith("<86>"));
    // Version
    CHECK_THAT(msg, ContainsSubstring("<86>1 "));
    // Hostname
    CHECK_THAT(msg, ContainsSubstring(" pacs-host "));
    // App name
    CHECK_THAT(msg, ContainsSubstring(" test_app "));
    // MSGID
    CHECK_THAT(msg, ContainsSubstring(" IHE+RFC-3881 "));
    // BOM + XML payload
    CHECK_THAT(msg, ContainsSubstring("<AuditMessage>test</AuditMessage>"));
}

TEST_CASE("format_syslog_message computes PRI correctly",
          "[security][syslog]") {
    SECTION("kern.emergency = 0") {
        syslog_transport_config config;
        config.hostname = "h";
        config.facility = syslog_facility::kern;
        config.severity = syslog_severity::emergency;
        atna_syslog_transport transport(config);

        auto msg = transport.format_syslog_message("x");
        CHECK_THAT(msg, StartsWith("<0>"));
    }

    SECTION("auth.warning = 36") {
        syslog_transport_config config;
        config.hostname = "h";
        config.facility = syslog_facility::auth;
        config.severity = syslog_severity::warning;
        atna_syslog_transport transport(config);

        auto msg = transport.format_syslog_message("x");
        // auth(4) * 8 + warning(4) = 36
        CHECK_THAT(msg, StartsWith("<36>"));
    }

    SECTION("local7.debug = 191") {
        syslog_transport_config config;
        config.hostname = "h";
        config.facility = syslog_facility::local7;
        config.severity = syslog_severity::debug;
        atna_syslog_transport transport(config);

        auto msg = transport.format_syslog_message("x");
        // local7(23) * 8 + debug(7) = 191
        CHECK_THAT(msg, StartsWith("<191>"));
    }

    SECTION("log_audit.informational = 110") {
        syslog_transport_config config;
        config.hostname = "h";
        config.facility = syslog_facility::log_audit;
        config.severity = syslog_severity::informational;
        atna_syslog_transport transport(config);

        auto msg = transport.format_syslog_message("x");
        // log_audit(13) * 8 + informational(6) = 110
        CHECK_THAT(msg, StartsWith("<110>"));
    }
}

TEST_CASE("format_syslog_message includes RFC 5424 timestamp",
          "[security][syslog]") {
    syslog_transport_config config;
    config.hostname = "h";
    atna_syslog_transport transport(config);

    auto msg = transport.format_syslog_message("test");

    // Should contain ISO 8601 timestamp ending with Z
    CHECK_THAT(msg, ContainsSubstring("T"));
    CHECK_THAT(msg, ContainsSubstring("Z"));
}

TEST_CASE("format_syslog_message includes BOM before XML",
          "[security][syslog]") {
    syslog_transport_config config;
    config.hostname = "h";
    atna_syslog_transport transport(config);

    auto msg = transport.format_syslog_message("<AuditMessage/>");

    // BOM is 0xEF 0xBB 0xBF (UTF-8 byte order mark)
    std::string bom = "\xEF\xBB\xBF";
    CHECK_THAT(msg, ContainsSubstring(bom + "<AuditMessage/>"));
}

TEST_CASE("format_syslog_message uses NIL values for optional fields",
          "[security][syslog]") {
    syslog_transport_config config;
    config.hostname = "h";
    config.app_name = "app";
    atna_syslog_transport transport(config);

    auto msg = transport.format_syslog_message("x");

    // PROCID and STRUCTURED-DATA should be NIL (-)
    // Format: ... APP-NAME SP PROCID SP MSGID SP SD SP MSG
    CHECK_THAT(msg, ContainsSubstring(" app - IHE+RFC-3881 - "));
}

// =============================================================================
// Integration with ATNA Audit Logger
// =============================================================================

TEST_CASE("format_syslog_message works with real ATNA XML",
          "[security][syslog]") {
    syslog_transport_config config;
    config.hostname = "pacs-server";
    config.app_name = "pacs_system";
    config.facility = syslog_facility::authpriv;
    config.severity = syslog_severity::informational;

    atna_syslog_transport transport(config);

    auto audit_msg = atna_audit_logger::build_user_authentication(
        "PACS_01", "dr.smith", "192.168.1.100", true);
    auto xml = atna_audit_logger::to_xml(audit_msg);

    auto syslog_msg = transport.format_syslog_message(xml);

    // Should contain the PRI
    CHECK_THAT(syslog_msg, StartsWith("<86>"));
    // Should contain the XML audit message
    CHECK_THAT(syslog_msg, ContainsSubstring("<?xml version="));
    CHECK_THAT(syslog_msg, ContainsSubstring("<AuditMessage>"));
    CHECK_THAT(syslog_msg, ContainsSubstring("UserID=\"dr.smith\""));
    CHECK_THAT(syslog_msg, ContainsSubstring("</AuditMessage>"));
}

// =============================================================================
// Statistics Tests
// =============================================================================

TEST_CASE("atna_syslog_transport statistics start at zero",
          "[security][syslog]") {
    syslog_transport_config config;
    atna_syslog_transport transport(config);

    CHECK(transport.messages_sent() == 0);
    CHECK(transport.send_errors() == 0);
}

TEST_CASE("atna_syslog_transport reset_statistics clears counters",
          "[security][syslog]") {
    syslog_transport_config config;
    atna_syslog_transport transport(config);

    // Force some errors by sending to unreachable host
    // (We can't easily test successful sends without a real server)
    transport.reset_statistics();

    CHECK(transport.messages_sent() == 0);
    CHECK(transport.send_errors() == 0);
}

// =============================================================================
// Connection State Tests
// =============================================================================

TEST_CASE("UDP transport is always 'connected'", "[security][syslog]") {
    syslog_transport_config config;
    config.protocol = syslog_transport_protocol::udp;
    atna_syslog_transport transport(config);

    CHECK(transport.is_connected() == true);
}

TEST_CASE("TLS transport starts disconnected", "[security][syslog]") {
    syslog_transport_config config;
    config.protocol = syslog_transport_protocol::tls;
    atna_syslog_transport transport(config);

    CHECK(transport.is_connected() == false);
}

TEST_CASE("close resets connection state", "[security][syslog]") {
    syslog_transport_config config;
    config.protocol = syslog_transport_protocol::tls;
    atna_syslog_transport transport(config);

    transport.close();
    CHECK(transport.is_connected() == false);
}

// =============================================================================
// Config Access Tests
// =============================================================================

TEST_CASE("config() returns reference to internal config",
          "[security][syslog]") {
    syslog_transport_config config;
    config.host = "192.168.1.50";
    config.port = 1514;
    config.app_name = "custom_pacs";

    atna_syslog_transport transport(config);

    const auto& ref = transport.config();
    CHECK(ref.host == "192.168.1.50");
    CHECK(ref.port == 1514);
    CHECK(ref.app_name == "custom_pacs");
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_CASE("send to unresolvable host returns error", "[security][syslog]") {
    syslog_transport_config config;
    config.protocol = syslog_transport_protocol::udp;
    config.host = "this-host-definitely-does-not-exist.invalid";
    config.port = 514;

    atna_syslog_transport transport(config);
    auto result = transport.send("<AuditMessage/>");

    CHECK_FALSE(result.is_ok());
    CHECK(transport.send_errors() == 1);
    CHECK(transport.messages_sent() == 0);
}

TEST_CASE("TLS send to unresolvable host returns error",
          "[security][syslog]") {
    syslog_transport_config config;
    config.protocol = syslog_transport_protocol::tls;
    config.host = "this-host-definitely-does-not-exist.invalid";
    config.port = 6514;

    atna_syslog_transport transport(config);
    auto result = transport.send("<AuditMessage/>");

    CHECK_FALSE(result.is_ok());
    CHECK(transport.send_errors() == 1);
}
