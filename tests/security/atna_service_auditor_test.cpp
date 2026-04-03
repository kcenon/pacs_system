/**
 * @file atna_service_auditor_test.cpp
 * @brief Unit tests for ATNA service auditor facade
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "kcenon/pacs/security/atna_service_auditor.h"

#include <string>

using namespace kcenon::pacs::security;
using Catch::Matchers::ContainsSubstring;

// =============================================================================
// Helper: Create auditor pointing to unresolvable host (no real network needed)
// =============================================================================

namespace {

syslog_transport_config make_test_config() {
    syslog_transport_config config;
    config.host = "localhost";
    config.port = 15140;  // Unlikely to be in use
    config.protocol = syslog_transport_protocol::udp;
    return config;
}

}  // namespace

// =============================================================================
// Construction Tests
// =============================================================================

TEST_CASE("atna_service_auditor constructs with config and source ID",
          "[security][auditor]") {
    atna_service_auditor auditor(make_test_config(), "PACS_TEST_01");

    CHECK(auditor.audit_source_id() == "PACS_TEST_01");
    CHECK(auditor.is_enabled() == true);
    CHECK(auditor.events_sent() == 0);
    CHECK(auditor.events_failed() == 0);
}

TEST_CASE("atna_service_auditor is movable", "[security][auditor]") {
    atna_service_auditor auditor1(make_test_config(), "PACS_MOVE");

    atna_service_auditor auditor2(std::move(auditor1));
    CHECK(auditor2.audit_source_id() == "PACS_MOVE");
    CHECK(auditor2.is_enabled() == true);
}

TEST_CASE("atna_service_auditor move assignment", "[security][auditor]") {
    atna_service_auditor auditor1(make_test_config(), "SOURCE_A");
    atna_service_auditor auditor2(make_test_config(), "SOURCE_B");

    auditor2 = std::move(auditor1);
    CHECK(auditor2.audit_source_id() == "SOURCE_A");
}

// =============================================================================
// Enable / Disable Tests
// =============================================================================

TEST_CASE("atna_service_auditor enable/disable", "[security][auditor]") {
    atna_service_auditor auditor(make_test_config(), "PACS_TEST");

    CHECK(auditor.is_enabled() == true);

    auditor.set_enabled(false);
    CHECK(auditor.is_enabled() == false);

    auditor.set_enabled(true);
    CHECK(auditor.is_enabled() == true);
}

TEST_CASE("disabled auditor does not send events", "[security][auditor]") {
    syslog_transport_config config;
    config.host = "this-host-does-not-exist.invalid";
    config.port = 514;

    atna_service_auditor auditor(config, "PACS_TEST");
    auditor.set_enabled(false);

    // These should return immediately without attempting to send
    auditor.audit_instance_stored("SCU", "SCP", "1.2.3", "PAT01", true);
    auditor.audit_query("SCU", "SCP", "STUDY", true);
    auditor.audit_authentication("user1", true, true);
    auditor.audit_security_alert("user1", "test alert");

    CHECK(auditor.events_sent() == 0);
    CHECK(auditor.events_failed() == 0);
}

// =============================================================================
// Statistics Tests
// =============================================================================

TEST_CASE("atna_service_auditor statistics start at zero",
          "[security][auditor]") {
    atna_service_auditor auditor(make_test_config(), "PACS_TEST");

    CHECK(auditor.events_sent() == 0);
    CHECK(auditor.events_failed() == 0);
}

TEST_CASE("atna_service_auditor reset_statistics", "[security][auditor]") {
    atna_service_auditor auditor(make_test_config(), "PACS_TEST");

    auditor.reset_statistics();

    CHECK(auditor.events_sent() == 0);
    CHECK(auditor.events_failed() == 0);
}

// =============================================================================
// Audit Event Emission Tests (with unresolvable host to verify error counting)
// =============================================================================

TEST_CASE("audit_instance_stored with unresolvable host counts as failure",
          "[security][auditor]") {
    syslog_transport_config config;
    config.host = "this-host-does-not-exist.invalid";
    config.port = 514;

    atna_service_auditor auditor(config, "PACS_TEST");

    auditor.audit_instance_stored(
        "MODALITY_01", "PACS_SCP", "1.2.3.4.5.6", "PAT001", true);

    CHECK(auditor.events_failed() == 1);
    CHECK(auditor.events_sent() == 0);
}

TEST_CASE("audit_query with unresolvable host counts as failure",
          "[security][auditor]") {
    syslog_transport_config config;
    config.host = "this-host-does-not-exist.invalid";
    config.port = 514;

    atna_service_auditor auditor(config, "PACS_TEST");

    auditor.audit_query("WORKSTATION", "PACS_SCP", "STUDY", true);

    CHECK(auditor.events_failed() == 1);
    CHECK(auditor.events_sent() == 0);
}

TEST_CASE("audit_authentication with unresolvable host counts as failure",
          "[security][auditor]") {
    syslog_transport_config config;
    config.host = "this-host-does-not-exist.invalid";
    config.port = 514;

    atna_service_auditor auditor(config, "PACS_TEST");

    auditor.audit_authentication("dr.smith", true, true);

    CHECK(auditor.events_failed() == 1);
}

TEST_CASE("audit_security_alert with unresolvable host counts as failure",
          "[security][auditor]") {
    syslog_transport_config config;
    config.host = "this-host-does-not-exist.invalid";
    config.port = 514;

    atna_service_auditor auditor(config, "PACS_TEST");

    auditor.audit_security_alert("intruder", "Unauthorized access attempt");

    CHECK(auditor.events_failed() == 1);
}

// =============================================================================
// Multiple Events Test
// =============================================================================

TEST_CASE("multiple audit events accumulate statistics",
          "[security][auditor]") {
    syslog_transport_config config;
    config.host = "this-host-does-not-exist.invalid";
    config.port = 514;

    atna_service_auditor auditor(config, "PACS_TEST");

    auditor.audit_instance_stored("SCU", "SCP", "1.2.3", "P1", true);
    auditor.audit_instance_stored("SCU", "SCP", "1.2.4", "P2", false);
    auditor.audit_query("SCU", "SCP", "PATIENT", true);
    auditor.audit_authentication("user1", true, false);
    auditor.audit_security_alert("user2", "alert");

    CHECK(auditor.events_failed() == 5);
    CHECK(auditor.events_sent() == 0);

    auditor.reset_statistics();
    CHECK(auditor.events_failed() == 0);
    CHECK(auditor.events_sent() == 0);
}

// =============================================================================
// Configuration Access Tests
// =============================================================================

TEST_CASE("transport() returns reference to internal transport",
          "[security][auditor]") {
    syslog_transport_config config;
    config.host = "audit-server.local";
    config.port = 6514;

    atna_service_auditor auditor(config, "PACS_01");

    CHECK(auditor.transport().config().host == "audit-server.local");
    CHECK(auditor.transport().config().port == 6514);
}

TEST_CASE("audit_source_id returns configured source ID",
          "[security][auditor]") {
    atna_service_auditor auditor(make_test_config(), "MY_PACS_SYSTEM");

    CHECK(auditor.audit_source_id() == "MY_PACS_SYSTEM");
}

// =============================================================================
// Success Path Test (localhost UDP — may succeed if socket is available)
// =============================================================================

TEST_CASE("audit event to localhost may succeed or fail gracefully",
          "[security][auditor]") {
    atna_service_auditor auditor(make_test_config(), "PACS_LOCAL");

    auditor.audit_instance_stored(
        "MODALITY", "PACS", "1.2.3.4.5", "PAT001", true);

    // Should have attempted to send — either sent or failed, but no crash
    CHECK((auditor.events_sent() + auditor.events_failed()) == 1);
}
