/**
 * @file atna_audit_logger_test.cpp
 * @brief Unit tests for IHE ATNA-compliant audit message generator
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "pacs/security/atna_audit_logger.hpp"

#include <string>

using namespace kcenon::pacs::security;
using Catch::Matchers::ContainsSubstring;

// =============================================================================
// Data Structure Tests
// =============================================================================

TEST_CASE("atna_coded_value holds code, system, and display name",
          "[security][atna]") {
    atna_coded_value cv{"110100", "DCM", "Application Activity"};

    CHECK(cv.code == "110100");
    CHECK(cv.code_system_name == "DCM");
    CHECK(cv.display_name == "Application Activity");
}

TEST_CASE("atna_event_action enum values match RFC 3881",
          "[security][atna]") {
    CHECK(static_cast<char>(atna_event_action::create) == 'C');
    CHECK(static_cast<char>(atna_event_action::read) == 'R');
    CHECK(static_cast<char>(atna_event_action::update) == 'U');
    CHECK(static_cast<char>(atna_event_action::delete_action) == 'D');
    CHECK(static_cast<char>(atna_event_action::execute) == 'E');
}

TEST_CASE("atna_event_outcome enum values match RFC 3881",
          "[security][atna]") {
    CHECK(static_cast<uint8_t>(atna_event_outcome::success) == 0);
    CHECK(static_cast<uint8_t>(atna_event_outcome::minor_failure) == 4);
    CHECK(static_cast<uint8_t>(atna_event_outcome::serious_failure) == 8);
    CHECK(static_cast<uint8_t>(atna_event_outcome::major_failure) == 12);
}

TEST_CASE("atna_network_access_type enum values", "[security][atna]") {
    CHECK(static_cast<uint8_t>(atna_network_access_type::machine_name) == 1);
    CHECK(static_cast<uint8_t>(atna_network_access_type::ip_address) == 2);
    CHECK(static_cast<uint8_t>(atna_network_access_type::phone_number) == 3);
}

TEST_CASE("atna_object_type and atna_object_role values", "[security][atna]") {
    CHECK(static_cast<uint8_t>(atna_object_type::person) == 1);
    CHECK(static_cast<uint8_t>(atna_object_type::system_object) == 2);
    CHECK(static_cast<uint8_t>(atna_object_type::organization) == 3);
    CHECK(static_cast<uint8_t>(atna_object_type::other) == 4);

    CHECK(static_cast<uint8_t>(atna_object_role::patient) == 1);
    CHECK(static_cast<uint8_t>(atna_object_role::report) == 3);
    CHECK(static_cast<uint8_t>(atna_object_role::query) == 24);
}

TEST_CASE("atna_active_participant default values", "[security][atna]") {
    atna_active_participant ap;
    CHECK(ap.user_id.empty());
    CHECK(ap.user_is_requestor == false);
    CHECK(ap.network_access_point_type ==
          atna_network_access_type::ip_address);
    CHECK(ap.role_id_codes.empty());
}

TEST_CASE("atna_audit_message default values", "[security][atna]") {
    atna_audit_message msg;
    CHECK(msg.event_action == atna_event_action::execute);
    CHECK(msg.event_outcome == atna_event_outcome::success);
    CHECK(msg.active_participants.empty());
    CHECK(msg.participant_objects.empty());
    CHECK(msg.event_type_codes.empty());
}

// =============================================================================
// Well-Known Constants Tests
// =============================================================================

TEST_CASE("ATNA event IDs have correct DCM codes", "[security][atna]") {
    CHECK(atna_event_ids::application_activity.code == "110100");
    CHECK(atna_event_ids::audit_log_used.code == "110101");
    CHECK(atna_event_ids::begin_transferring.code == "110102");
    CHECK(atna_event_ids::dicom_instances_accessed.code == "110103");
    CHECK(atna_event_ids::dicom_instances_transferred.code == "110104");
    CHECK(atna_event_ids::dicom_study_deleted.code == "110105");
    CHECK(atna_event_ids::data_export.code == "110106");
    CHECK(atna_event_ids::data_import.code == "110107");
    CHECK(atna_event_ids::network_entry.code == "110108");
    CHECK(atna_event_ids::order_record.code == "110109");
    CHECK(atna_event_ids::patient_record.code == "110110");
    CHECK(atna_event_ids::procedure_record.code == "110111");
    CHECK(atna_event_ids::query.code == "110112");
    CHECK(atna_event_ids::security_alert.code == "110113");
    CHECK(atna_event_ids::user_authentication.code == "110114");

    // All should use DCM code system
    CHECK(atna_event_ids::application_activity.code_system_name == "DCM");
    CHECK(atna_event_ids::user_authentication.code_system_name == "DCM");
}

TEST_CASE("ATNA event type codes for start/stop and login/logout",
          "[security][atna]") {
    CHECK(atna_event_types::application_start.code == "110120");
    CHECK(atna_event_types::application_stop.code == "110121");
    CHECK(atna_event_types::login.code == "110122");
    CHECK(atna_event_types::logout.code == "110123");
}

TEST_CASE("ATNA role ID codes", "[security][atna]") {
    CHECK(atna_role_ids::application.code == "110150");
    CHECK(atna_role_ids::application_launcher.code == "110151");
    CHECK(atna_role_ids::destination.code == "110152");
    CHECK(atna_role_ids::source.code == "110153");
    CHECK(atna_role_ids::destination_media.code == "110154");
    CHECK(atna_role_ids::source_media.code == "110155");
}

TEST_CASE("ATNA object ID type codes", "[security][atna]") {
    CHECK(atna_object_id_types::patient_number.code == "2");
    CHECK(atna_object_id_types::patient_number.code_system_name == "RFC-3881");
    CHECK(atna_object_id_types::study_instance_uid.code == "110180");
    CHECK(atna_object_id_types::sop_class_uid.code == "110181");
    CHECK(atna_object_id_types::node_id.code == "110182");
}

// =============================================================================
// XML Generation Tests
// =============================================================================

TEST_CASE("to_xml produces valid XML structure", "[security][atna]") {
    auto msg = atna_audit_logger::build_user_authentication(
        "PACS_SERVER", "admin", "192.168.1.100", true);

    auto xml = atna_audit_logger::to_xml(msg);

    CHECK_THAT(xml, ContainsSubstring("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"));
    CHECK_THAT(xml, ContainsSubstring("<AuditMessage>"));
    CHECK_THAT(xml, ContainsSubstring("</AuditMessage>"));
    CHECK_THAT(xml, ContainsSubstring("<EventIdentification"));
    CHECK_THAT(xml, ContainsSubstring("</EventIdentification>"));
    CHECK_THAT(xml, ContainsSubstring("<ActiveParticipant"));
    CHECK_THAT(xml, ContainsSubstring("<AuditSourceIdentification"));
}

TEST_CASE("to_xml includes correct event attributes", "[security][atna]") {
    auto msg = atna_audit_logger::build_user_authentication(
        "PACS_SERVER", "admin", "192.168.1.100", true);

    auto xml = atna_audit_logger::to_xml(msg);

    // EventActionCode="E" for Execute
    CHECK_THAT(xml, ContainsSubstring("EventActionCode=\"E\""));
    // EventOutcomeIndicator="0" for Success
    CHECK_THAT(xml, ContainsSubstring("EventOutcomeIndicator=\"0\""));
    // EventID csd-code="110114" for UserAuthentication
    CHECK_THAT(xml, ContainsSubstring("csd-code=\"110114\""));
    // EventTypeCode for Login
    CHECK_THAT(xml, ContainsSubstring("csd-code=\"110122\""));
}

TEST_CASE("to_xml includes ActiveParticipant details", "[security][atna]") {
    auto msg = atna_audit_logger::build_user_authentication(
        "PACS_SERVER", "admin", "192.168.1.100", true);

    auto xml = atna_audit_logger::to_xml(msg);

    CHECK_THAT(xml, ContainsSubstring("UserID=\"admin\""));
    CHECK_THAT(xml, ContainsSubstring("UserIsRequestor=\"true\""));
    CHECK_THAT(xml, ContainsSubstring("NetworkAccessPointID=\"192.168.1.100\""));
    CHECK_THAT(xml, ContainsSubstring("NetworkAccessPointTypeCode=\"2\""));
}

TEST_CASE("to_xml includes AuditSourceIdentification", "[security][atna]") {
    auto msg = atna_audit_logger::build_user_authentication(
        "PACS_SERVER", "admin", "192.168.1.100", true);

    auto xml = atna_audit_logger::to_xml(msg);

    CHECK_THAT(xml, ContainsSubstring("AuditSourceID=\"PACS_SERVER\""));
}

TEST_CASE("to_xml includes ParticipantObjectIdentification",
          "[security][atna]") {
    auto msg = atna_audit_logger::build_dicom_instances_accessed(
        "PACS", "MODALITY_AE", "10.0.0.5",
        "1.2.840.113619.2.55.3", "PAT001");

    auto xml = atna_audit_logger::to_xml(msg);

    // Study object
    CHECK_THAT(xml, ContainsSubstring(
        "ParticipantObjectID=\"1.2.840.113619.2.55.3\""));
    // Patient object
    CHECK_THAT(xml, ContainsSubstring(
        "ParticipantObjectID=\"PAT001\""));
    // ParticipantObjectIDTypeCode for study
    CHECK_THAT(xml, ContainsSubstring("csd-code=\"110180\""));
    // ParticipantObjectIDTypeCode for patient
    CHECK_THAT(xml, ContainsSubstring("csd-code=\"2\""));
}

TEST_CASE("to_xml escapes special XML characters", "[security][atna]") {
    auto msg = atna_audit_logger::build_security_alert(
        "PACS", "user<admin>", "10.0.0.1",
        "Alert: A&B \"test\" 'msg'");

    auto xml = atna_audit_logger::to_xml(msg);

    CHECK_THAT(xml, ContainsSubstring("user&lt;admin&gt;"));
    CHECK_THAT(xml, ContainsSubstring("A&amp;B"));
    CHECK_THAT(xml, ContainsSubstring("&quot;test&quot;"));
    CHECK_THAT(xml, ContainsSubstring("&apos;msg&apos;"));
}

TEST_CASE("to_xml formats EventDateTime with milliseconds",
          "[security][atna]") {
    auto msg = atna_audit_logger::build_application_activity(
        "PACS", "MyApp", true);

    auto xml = atna_audit_logger::to_xml(msg);

    // Should contain a datetime pattern like "2026-03-02T..."
    CHECK_THAT(xml, ContainsSubstring("EventDateTime=\""));
    CHECK_THAT(xml, ContainsSubstring("Z\""));
}

TEST_CASE("to_xml includes RoleIDCode as child element", "[security][atna]") {
    auto msg = atna_audit_logger::build_application_activity(
        "PACS", "MyApp", true);

    auto xml = atna_audit_logger::to_xml(msg);

    // Application role should be present as child element
    CHECK_THAT(xml, ContainsSubstring("<RoleIDCode"));
    CHECK_THAT(xml, ContainsSubstring("csd-code=\"110150\""));
    CHECK_THAT(xml, ContainsSubstring("</ActiveParticipant>"));
}

TEST_CASE("to_xml handles message with no participant objects",
          "[security][atna]") {
    auto msg = atna_audit_logger::build_user_authentication(
        "PACS", "admin", "10.0.0.1", false);

    auto xml = atna_audit_logger::to_xml(msg);

    // Should still produce valid XML without ParticipantObjectIdentification
    CHECK_THAT(xml, ContainsSubstring("<AuditMessage>"));
    CHECK_THAT(xml, ContainsSubstring("</AuditMessage>"));
    // Logout event type
    CHECK_THAT(xml, ContainsSubstring("csd-code=\"110123\""));
}

TEST_CASE("to_xml handles query object with query data", "[security][atna]") {
    auto msg = atna_audit_logger::build_query(
        "PACS", "FINDER_AE", "10.0.0.2",
        "PatientName=DOE*", "PAT002");

    auto xml = atna_audit_logger::to_xml(msg);

    CHECK_THAT(xml, ContainsSubstring("<ParticipantObjectQuery>"));
    CHECK_THAT(xml, ContainsSubstring("PatientName=DOE*"));
    CHECK_THAT(xml, ContainsSubstring("</ParticipantObjectQuery>"));
}

// =============================================================================
// Factory Method Tests
// =============================================================================

TEST_CASE("build_application_activity produces correct start message",
          "[security][atna]") {
    auto msg = atna_audit_logger::build_application_activity(
        "PACS_01", "DicomServer", true);

    CHECK(msg.event_id.code == "110100");
    CHECK(msg.event_action == atna_event_action::execute);
    CHECK(msg.event_outcome == atna_event_outcome::success);
    REQUIRE(msg.event_type_codes.size() == 1);
    CHECK(msg.event_type_codes[0].code == "110120"); // Application Start

    REQUIRE(msg.active_participants.size() == 1);
    CHECK(msg.active_participants[0].user_id == "DicomServer");
    CHECK(msg.active_participants[0].user_is_requestor == false);
    REQUIRE(msg.active_participants[0].role_id_codes.size() == 1);
    CHECK(msg.active_participants[0].role_id_codes[0].code == "110150");

    CHECK(msg.audit_source.audit_source_id == "PACS_01");
}

TEST_CASE("build_application_activity produces stop message",
          "[security][atna]") {
    auto msg = atna_audit_logger::build_application_activity(
        "PACS_01", "DicomServer", false,
        atna_event_outcome::minor_failure);

    REQUIRE(msg.event_type_codes.size() == 1);
    CHECK(msg.event_type_codes[0].code == "110121"); // Application Stop
    CHECK(msg.event_outcome == atna_event_outcome::minor_failure);
}

TEST_CASE("build_dicom_instances_accessed with study and patient",
          "[security][atna]") {
    auto msg = atna_audit_logger::build_dicom_instances_accessed(
        "PACS", "CT_AE", "192.168.1.50",
        "1.2.840.12345", "PAT100");

    CHECK(msg.event_id.code == "110103");
    CHECK(msg.event_action == atna_event_action::read);

    REQUIRE(msg.active_participants.size() == 1);
    CHECK(msg.active_participants[0].user_id == "CT_AE");
    CHECK(msg.active_participants[0].user_is_requestor == true);
    CHECK(msg.active_participants[0].network_access_point_id == "192.168.1.50");

    REQUIRE(msg.participant_objects.size() == 2);
    // Study object
    CHECK(msg.participant_objects[0].object_id == "1.2.840.12345");
    CHECK(msg.participant_objects[0].object_role == atna_object_role::report);
    CHECK(msg.participant_objects[0].object_id_type_code.code == "110180");
    // Patient object
    CHECK(msg.participant_objects[1].object_id == "PAT100");
    CHECK(msg.participant_objects[1].object_role == atna_object_role::patient);
    CHECK(msg.participant_objects[1].object_id_type_code.code == "2");
}

TEST_CASE("build_dicom_instances_accessed without patient",
          "[security][atna]") {
    auto msg = atna_audit_logger::build_dicom_instances_accessed(
        "PACS", "CT_AE", "10.0.0.1", "1.2.3.4");

    REQUIRE(msg.participant_objects.size() == 1);
    CHECK(msg.participant_objects[0].object_id == "1.2.3.4");
}

TEST_CASE("build_dicom_instances_transferred for import",
          "[security][atna]") {
    auto msg = atna_audit_logger::build_dicom_instances_transferred(
        "PACS", "MODALITY", "10.0.0.10",
        "PACS_AE", "10.0.0.20",
        "1.2.840.99999", "PAT200", true);

    CHECK(msg.event_id.code == "110104");
    CHECK(msg.event_action == atna_event_action::create); // Import = Create

    REQUIRE(msg.active_participants.size() == 2);
    // Source
    CHECK(msg.active_participants[0].user_id == "MODALITY");
    CHECK(msg.active_participants[0].user_is_requestor == false); // Not requestor for import
    CHECK(msg.active_participants[0].role_id_codes[0].code == "110153"); // Source
    // Destination
    CHECK(msg.active_participants[1].user_id == "PACS_AE");
    CHECK(msg.active_participants[1].user_is_requestor == true); // Requestor for import
    CHECK(msg.active_participants[1].role_id_codes[0].code == "110152"); // Destination

    REQUIRE(msg.participant_objects.size() == 2);
}

TEST_CASE("build_dicom_instances_transferred for export",
          "[security][atna]") {
    auto msg = atna_audit_logger::build_dicom_instances_transferred(
        "PACS", "PACS_AE", "10.0.0.20",
        "WORKSTATION", "10.0.0.30",
        "1.2.840.88888", "", false);

    CHECK(msg.event_action == atna_event_action::read); // Export = Read

    REQUIRE(msg.active_participants.size() == 2);
    CHECK(msg.active_participants[0].user_is_requestor == true); // Requestor for export
    CHECK(msg.active_participants[1].user_is_requestor == false);

    // No patient object (empty patient_id)
    REQUIRE(msg.participant_objects.size() == 1);
}

TEST_CASE("build_study_deleted includes study and patient objects",
          "[security][atna]") {
    auto msg = atna_audit_logger::build_study_deleted(
        "PACS", "admin_user", "10.0.0.1",
        "1.2.840.55555", "PAT300");

    CHECK(msg.event_id.code == "110105");
    CHECK(msg.event_action == atna_event_action::delete_action);

    REQUIRE(msg.active_participants.size() == 1);
    CHECK(msg.active_participants[0].user_id == "admin_user");
    CHECK(msg.active_participants[0].user_is_requestor == true);

    REQUIRE(msg.participant_objects.size() == 2);
    CHECK(msg.participant_objects[0].object_id == "1.2.840.55555");
    CHECK(msg.participant_objects[1].object_id == "PAT300");
}

TEST_CASE("build_study_deleted without patient", "[security][atna]") {
    auto msg = atna_audit_logger::build_study_deleted(
        "PACS", "admin", "10.0.0.1", "1.2.3.4");

    REQUIRE(msg.participant_objects.size() == 1);
    CHECK(msg.participant_objects[0].object_id == "1.2.3.4");
}

TEST_CASE("build_security_alert default outcome is serious_failure",
          "[security][atna]") {
    auto msg = atna_audit_logger::build_security_alert(
        "PACS", "system", "10.0.0.1",
        "TLS handshake failed");

    CHECK(msg.event_id.code == "110113");
    CHECK(msg.event_action == atna_event_action::execute);
    CHECK(msg.event_outcome == atna_event_outcome::serious_failure);

    REQUIRE(msg.participant_objects.size() == 1);
    CHECK(msg.participant_objects[0].object_name == "TLS handshake failed");
    CHECK(msg.participant_objects[0].object_role ==
          atna_object_role::security_resource);
}

TEST_CASE("build_user_authentication login event", "[security][atna]") {
    auto msg = atna_audit_logger::build_user_authentication(
        "PACS", "dr.smith", "192.168.1.200", true);

    CHECK(msg.event_id.code == "110114");
    REQUIRE(msg.event_type_codes.size() == 1);
    CHECK(msg.event_type_codes[0].code == "110122"); // Login

    REQUIRE(msg.active_participants.size() == 1);
    CHECK(msg.active_participants[0].user_id == "dr.smith");
    CHECK(msg.participant_objects.empty());
}

TEST_CASE("build_user_authentication logout event", "[security][atna]") {
    auto msg = atna_audit_logger::build_user_authentication(
        "PACS", "dr.smith", "192.168.1.200", false);

    REQUIRE(msg.event_type_codes.size() == 1);
    CHECK(msg.event_type_codes[0].code == "110123"); // Logout
}

TEST_CASE("build_user_authentication failed login", "[security][atna]") {
    auto msg = atna_audit_logger::build_user_authentication(
        "PACS", "hacker", "10.0.0.99", true,
        atna_event_outcome::serious_failure);

    CHECK(msg.event_outcome == atna_event_outcome::serious_failure);
}

TEST_CASE("build_query includes query data and patient", "[security][atna]") {
    auto msg = atna_audit_logger::build_query(
        "PACS", "FINDER_AE", "10.0.0.5",
        "PatientName=DOE*&StudyDate=20240101-", "PAT400");

    CHECK(msg.event_id.code == "110112");
    CHECK(msg.event_action == atna_event_action::execute);

    REQUIRE(msg.active_participants.size() == 1);
    CHECK(msg.active_participants[0].role_id_codes[0].code == "110153"); // Source

    REQUIRE(msg.participant_objects.size() == 2);
    // Query object
    CHECK(msg.participant_objects[0].object_role == atna_object_role::query);
    CHECK(msg.participant_objects[0].object_query ==
          "PatientName=DOE*&StudyDate=20240101-");
    // Patient object
    CHECK(msg.participant_objects[1].object_id == "PAT400");
}

TEST_CASE("build_query without patient", "[security][atna]") {
    auto msg = atna_audit_logger::build_query(
        "PACS", "AE", "10.0.0.1", "Modality=CT");

    REQUIRE(msg.participant_objects.size() == 1);
    CHECK(msg.participant_objects[0].object_role == atna_object_role::query);
}

TEST_CASE("build_export includes source, destination, study, patient",
          "[security][atna]") {
    auto msg = atna_audit_logger::build_export(
        "PACS", "EXPORTER_AE", "10.0.0.5",
        "REMOTE_AE", "1.2.840.77777", "PAT500");

    CHECK(msg.event_id.code == "110106");
    CHECK(msg.event_action == atna_event_action::read);

    REQUIRE(msg.active_participants.size() == 2);
    // Exporter (source)
    CHECK(msg.active_participants[0].user_id == "EXPORTER_AE");
    CHECK(msg.active_participants[0].user_is_requestor == true);
    CHECK(msg.active_participants[0].role_id_codes[0].code == "110153"); // Source
    // Destination
    CHECK(msg.active_participants[1].user_id == "REMOTE_AE");
    CHECK(msg.active_participants[1].user_is_requestor == false);
    CHECK(msg.active_participants[1].role_id_codes[0].code == "110152"); // Dest

    REQUIRE(msg.participant_objects.size() == 2);
    CHECK(msg.participant_objects[0].object_id == "1.2.840.77777");
    CHECK(msg.participant_objects[1].object_id == "PAT500");
}

TEST_CASE("build_export without patient", "[security][atna]") {
    auto msg = atna_audit_logger::build_export(
        "PACS", "AE", "10.0.0.1", "DEST", "1.2.3");

    REQUIRE(msg.participant_objects.size() == 1);
    CHECK(msg.participant_objects[0].object_id == "1.2.3");
}

// =============================================================================
// XML Round-Trip Validation
// =============================================================================

TEST_CASE("All factory methods produce parseable XML", "[security][atna]") {
    // Ensure each factory method generates XML without errors
    SECTION("application_activity") {
        auto xml = atna_audit_logger::to_xml(
            atna_audit_logger::build_application_activity(
                "SRC", "App", true));
        CHECK_THAT(xml, ContainsSubstring("<AuditMessage>"));
        CHECK_THAT(xml, ContainsSubstring("</AuditMessage>"));
    }

    SECTION("dicom_instances_accessed") {
        auto xml = atna_audit_logger::to_xml(
            atna_audit_logger::build_dicom_instances_accessed(
                "SRC", "AE", "1.2.3.4", "1.2.3"));
        CHECK_THAT(xml, ContainsSubstring("<AuditMessage>"));
    }

    SECTION("dicom_instances_transferred") {
        auto xml = atna_audit_logger::to_xml(
            atna_audit_logger::build_dicom_instances_transferred(
                "SRC", "SRC_AE", "1.2.3.4", "DST_AE", "5.6.7.8", "1.2.3"));
        CHECK_THAT(xml, ContainsSubstring("<AuditMessage>"));
    }

    SECTION("study_deleted") {
        auto xml = atna_audit_logger::to_xml(
            atna_audit_logger::build_study_deleted(
                "SRC", "user", "1.2.3.4", "1.2.3"));
        CHECK_THAT(xml, ContainsSubstring("<AuditMessage>"));
    }

    SECTION("security_alert") {
        auto xml = atna_audit_logger::to_xml(
            atna_audit_logger::build_security_alert(
                "SRC", "user", "1.2.3.4", "alert"));
        CHECK_THAT(xml, ContainsSubstring("<AuditMessage>"));
    }

    SECTION("user_authentication") {
        auto xml = atna_audit_logger::to_xml(
            atna_audit_logger::build_user_authentication(
                "SRC", "user", "1.2.3.4", true));
        CHECK_THAT(xml, ContainsSubstring("<AuditMessage>"));
    }

    SECTION("query") {
        auto xml = atna_audit_logger::to_xml(
            atna_audit_logger::build_query(
                "SRC", "AE", "1.2.3.4", "query=*"));
        CHECK_THAT(xml, ContainsSubstring("<AuditMessage>"));
    }

    SECTION("export") {
        auto xml = atna_audit_logger::to_xml(
            atna_audit_logger::build_export(
                "SRC", "AE", "1.2.3.4", "DEST", "1.2.3"));
        CHECK_THAT(xml, ContainsSubstring("<AuditMessage>"));
    }
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("to_xml handles empty audit source", "[security][atna]") {
    atna_audit_message msg;
    msg.event_id = atna_event_ids::security_alert;
    msg.event_date_time = std::chrono::system_clock::now();

    auto xml = atna_audit_logger::to_xml(msg);
    CHECK_THAT(xml, ContainsSubstring("<AuditMessage>"));
    CHECK_THAT(xml, ContainsSubstring("AuditSourceID=\"\""));
}

TEST_CASE("to_xml handles ActiveParticipant without network info",
          "[security][atna]") {
    atna_audit_message msg;
    msg.event_id = atna_event_ids::application_activity;
    msg.event_date_time = std::chrono::system_clock::now();

    atna_active_participant ap;
    ap.user_id = "local_process";
    ap.user_is_requestor = true;
    // No network_access_point_id
    msg.active_participants.push_back(std::move(ap));
    msg.audit_source.audit_source_id = "PACS";

    auto xml = atna_audit_logger::to_xml(msg);
    CHECK_THAT(xml, ContainsSubstring("UserID=\"local_process\""));
    // Should NOT contain NetworkAccessPointID when empty
    // The participant should self-close since no role codes
    CHECK_THAT(xml, ContainsSubstring("UserIsRequestor=\"true\"/>"));
}

TEST_CASE("to_xml handles ParticipantObject with details",
          "[security][atna]") {
    atna_audit_message msg;
    msg.event_id = atna_event_ids::query;
    msg.event_date_time = std::chrono::system_clock::now();
    msg.audit_source.audit_source_id = "PACS";

    atna_participant_object obj;
    obj.object_type = atna_object_type::system_object;
    obj.object_role = atna_object_role::query;
    obj.object_id_type_code = atna_object_id_types::sop_class_uid;
    obj.object_details.push_back({"TransferSyntax", "1.2.840.10008.1.2"});
    msg.participant_objects.push_back(std::move(obj));

    auto xml = atna_audit_logger::to_xml(msg);
    CHECK_THAT(xml, ContainsSubstring("<ParticipantObjectDetail"));
    CHECK_THAT(xml, ContainsSubstring("type=\"TransferSyntax\""));
    CHECK_THAT(xml, ContainsSubstring("value=\"1.2.840.10008.1.2\""));
}

TEST_CASE("to_xml handles AuditSource with type codes", "[security][atna]") {
    atna_audit_message msg;
    msg.event_id = atna_event_ids::application_activity;
    msg.event_date_time = std::chrono::system_clock::now();
    msg.audit_source.audit_source_id = "PACS";
    msg.audit_source.audit_enterprise_site_id = "Hospital_A";
    msg.audit_source.audit_source_type_codes.push_back(4); // Application Server

    auto xml = atna_audit_logger::to_xml(msg);
    CHECK_THAT(xml, ContainsSubstring(
        "AuditEnterpriseSiteID=\"Hospital_A\""));
    CHECK_THAT(xml, ContainsSubstring(
        "<AuditSourceTypeCode code=\"4\""));
    CHECK_THAT(xml, ContainsSubstring("</AuditSourceIdentification>"));
}

TEST_CASE("to_xml handles ActiveParticipant with AlternativeUserID and UserName",
          "[security][atna]") {
    atna_audit_message msg;
    msg.event_id = atna_event_ids::user_authentication;
    msg.event_date_time = std::chrono::system_clock::now();
    msg.audit_source.audit_source_id = "PACS";

    atna_active_participant ap;
    ap.user_id = "dr.smith";
    ap.alternative_user_id = "pid:12345";
    ap.user_name = "Dr. Jane Smith";
    ap.user_is_requestor = true;
    msg.active_participants.push_back(std::move(ap));

    auto xml = atna_audit_logger::to_xml(msg);
    CHECK_THAT(xml, ContainsSubstring("AlternativeUserID=\"pid:12345\""));
    CHECK_THAT(xml, ContainsSubstring("UserName=\"Dr. Jane Smith\""));
}
