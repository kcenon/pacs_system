/**
 * @file xds_imaging_test.cpp
 * @brief Unit tests for IHE XDS-I.b Imaging Document Source and Consumer
 */

#include <kcenon/pacs/services/xds/imaging_document_source.h>
#include <kcenon/pacs/services/xds/imaging_document_consumer.h>
#include <kcenon/pacs/services/sop_classes/sr_storage.h>
#include <kcenon/pacs/core/dicom_dataset.h>
#include <kcenon/pacs/core/dicom_tag_constants.h>
#include <kcenon/pacs/encoding/vr_type.h>
#include <kcenon/pacs/security/atna_service_auditor.h>
#include <kcenon/pacs/security/atna_syslog_transport.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <memory>

using namespace kcenon::pacs::services::xds;
using namespace kcenon::pacs::services::sop_classes;
using namespace kcenon::pacs::core;
using namespace kcenon::pacs::encoding;

// ============================================================================
// Helper: Create sample instance references
// ============================================================================

namespace {

std::vector<kos_instance_reference> create_sample_references() {
    return {
        {"1.2.840.10008.5.1.4.1.1.2",       // CT Image Storage
         "1.2.3.4.5.6.7.8.1",               // SOP Instance UID 1
         "1.2.3.4.5.6.7.100",               // Series Instance UID
         "1.2.3.4.5.6.7.200"},              // Study Instance UID
        {"1.2.840.10008.5.1.4.1.1.2",       // CT Image Storage
         "1.2.3.4.5.6.7.8.2",               // SOP Instance UID 2
         "1.2.3.4.5.6.7.100",               // Same series
         "1.2.3.4.5.6.7.200"},              // Same study
        {"1.2.840.10008.5.1.4.1.1.2",       // CT Image Storage
         "1.2.3.4.5.6.7.8.3",               // SOP Instance UID 3
         "1.2.3.4.5.6.7.101",               // Different series
         "1.2.3.4.5.6.7.200"},              // Same study
    };
}

dicom_dataset create_patient_demographics() {
    dicom_dataset ds;
    ds.set_string(tags::patient_name, vr_type::PN, "TEST^PATIENT");
    ds.set_string(tags::patient_id, vr_type::LO, "12345");
    ds.set_string(tags::patient_birth_date, vr_type::DA, "19800101");
    ds.set_string(tags::patient_sex, vr_type::CS, "M");
    return ds;
}

}  // namespace

// ============================================================================
// Imaging Document Source - KOS Creation Tests
// ============================================================================

TEST_CASE("imaging_document_source creates KOS document", "[services][xds][source]") {
    imaging_document_source_config config;
    config.registry_url = "https://xds.example.com/iti41";
    config.source_oid = "1.2.3.4.5.6.7.8.9";
    config.assigning_authority_oid = "1.2.3.4.5.6.7.8.10";

    imaging_document_source source{config};
    auto references = create_sample_references();
    auto patient = create_patient_demographics();

    auto result = source.create_kos_document(
        "1.2.3.4.5.6.7.200", references, patient);

    REQUIRE(result.success);
    REQUIRE(result.kos_dataset.has_value());
    CHECK_FALSE(result.kos_instance_uid.empty());
    CHECK(result.reference_count == 3);
}

TEST_CASE("KOS document has correct SOP Class UID", "[services][xds][source]") {
    imaging_document_source source;
    auto references = create_sample_references();

    auto result = source.create_kos_document("1.2.3.4.5.6.7.200", references);

    REQUIRE(result.success);
    auto sop_class = result.kos_dataset->get_string(tags::sop_class_uid);
    CHECK(sop_class == key_object_selection_document_storage_uid);
}

TEST_CASE("KOS document contains patient demographics", "[services][xds][source]") {
    imaging_document_source source;
    auto references = create_sample_references();
    auto patient = create_patient_demographics();

    auto result = source.create_kos_document(
        "1.2.3.4.5.6.7.200", references, patient);

    REQUIRE(result.success);
    CHECK(result.kos_dataset->get_string(tags::patient_name) == "TEST^PATIENT");
    CHECK(result.kos_dataset->get_string(tags::patient_id) == "12345");
}

TEST_CASE("KOS document has KO modality", "[services][xds][source]") {
    imaging_document_source source;
    auto references = create_sample_references();

    auto result = source.create_kos_document("1.2.3.4.5.6.7.200", references);

    REQUIRE(result.success);
    CHECK(result.kos_dataset->get_string(tags::modality) == "KO");
}

TEST_CASE("KOS document has evidence sequence", "[services][xds][source]") {
    imaging_document_source source;
    auto references = create_sample_references();

    auto result = source.create_kos_document("1.2.3.4.5.6.7.200", references);

    REQUIRE(result.success);
    // Current Requested Procedure Evidence Sequence (0040,A375)
    constexpr dicom_tag evidence_seq{0x0040, 0xA375};
    const auto* elem = result.kos_dataset->get(evidence_seq);
    REQUIRE(elem != nullptr);
    CHECK(elem->is_sequence());
    CHECK_FALSE(elem->sequence_items().empty());
}

TEST_CASE("KOS creation fails with empty references", "[services][xds][source]") {
    imaging_document_source source;
    std::vector<kos_instance_reference> empty_refs;

    auto result = source.create_kos_document("1.2.3.4.5.6.7.200", empty_refs);

    CHECK_FALSE(result.success);
    CHECK_FALSE(result.error_message.empty());
}

TEST_CASE("KOS document without patient demographics uses empty values", "[services][xds][source]") {
    imaging_document_source source;
    auto references = create_sample_references();

    auto result = source.create_kos_document("1.2.3.4.5.6.7.200", references);

    REQUIRE(result.success);
    // Patient module should still exist but with empty Type 2 values
    CHECK(result.kos_dataset->contains(tags::patient_name));
    CHECK(result.kos_dataset->contains(tags::patient_id));
}

// ============================================================================
// Imaging Document Source - Document Entry Tests
// ============================================================================

TEST_CASE("build_document_entry extracts metadata from KOS", "[services][xds][source]") {
    imaging_document_source_config config;
    config.source_oid = "1.2.3.4.5.6.7.8.9";
    config.facility_type_code = "Hospital";

    imaging_document_source source{config};
    auto references = create_sample_references();
    auto patient = create_patient_demographics();

    auto kos_result = source.create_kos_document(
        "1.2.3.4.5.6.7.200", references, patient);
    REQUIRE(kos_result.success);

    auto entry = source.build_document_entry(kos_result.kos_dataset.value());

    CHECK_FALSE(entry.unique_id.empty());
    CHECK(entry.patient_id == "12345");
    CHECK(entry.class_code == "IMG");
    CHECK(entry.type_code == "KOS");
    CHECK(entry.mime_type == "application/dicom");
    CHECK(entry.facility_type_code == "Hospital");
}

TEST_CASE("build_submission_set creates valid metadata", "[services][xds][source]") {
    imaging_document_source_config config;
    config.source_oid = "1.2.3.4.5.6.7.8.9";

    imaging_document_source source{config};
    auto set = source.build_submission_set("12345");

    CHECK_FALSE(set.unique_id.empty());
    CHECK(set.source_id == "1.2.3.4.5.6.7.8.9");
    CHECK(set.patient_id == "12345");
    CHECK(set.content_type_code == "IMG");
    CHECK_FALSE(set.submission_time.empty());
}

// ============================================================================
// Imaging Document Source - Publication Tests
// ============================================================================

TEST_CASE("publish_document fails without registry URL", "[services][xds][source]") {
    imaging_document_source source;  // No config = no URL
    dicom_dataset kos;
    xds_document_entry entry;

    auto result = source.publish_document(kos, entry);
    CHECK_FALSE(result.success);
    CHECK_FALSE(result.error_message.empty());
}

TEST_CASE("publish_document succeeds with configured URL", "[services][xds][source]") {
    imaging_document_source_config config;
    config.registry_url = "https://xds.example.com/iti41";
    config.source_oid = "1.2.3.4.5.6.7.8.9";

    imaging_document_source source{config};
    auto references = create_sample_references();

    auto kos_result = source.create_kos_document("1.2.3.4.5.6.7.200", references);
    REQUIRE(kos_result.success);

    auto entry = source.build_document_entry(kos_result.kos_dataset.value());
    auto pub_result = source.publish_document(kos_result.kos_dataset.value(), entry);

    CHECK(pub_result.success);
    CHECK_FALSE(pub_result.document_entry_uuid.empty());
}

// ============================================================================
// Imaging Document Consumer - Configuration Tests
// ============================================================================

TEST_CASE("imaging_document_consumer construction", "[services][xds][consumer]") {
    SECTION("default construction") {
        imaging_document_consumer consumer;
        CHECK(consumer.config().registry_url.empty());
        CHECK(consumer.config().wado_rs_url.empty());
    }

    SECTION("construction with config") {
        imaging_document_consumer_config config;
        config.registry_url = "https://xds.example.com/iti18";
        config.wado_rs_url = "https://pacs.example.com/wado-rs";
        config.timeout_ms = 60000;

        imaging_document_consumer consumer{config};
        CHECK(consumer.config().registry_url == "https://xds.example.com/iti18");
        CHECK(consumer.config().wado_rs_url == "https://pacs.example.com/wado-rs");
        CHECK(consumer.config().timeout_ms == 60000);
    }
}

// ============================================================================
// Imaging Document Consumer - Query Tests
// ============================================================================

TEST_CASE("query_registry fails without URL", "[services][xds][consumer]") {
    imaging_document_consumer consumer;
    registry_query_params params;
    params.patient_id = "12345";

    auto result = consumer.query_registry(params);
    CHECK_FALSE(result.success);
    CHECK_FALSE(result.error_message.empty());
}

TEST_CASE("query_registry succeeds with configured URL", "[services][xds][consumer]") {
    imaging_document_consumer_config config;
    config.registry_url = "https://xds.example.com/iti18";

    imaging_document_consumer consumer{config};
    registry_query_params params;
    params.patient_id = "12345";

    auto result = consumer.query_registry(params);
    CHECK(result.success);
}

// ============================================================================
// Imaging Document Consumer - Reference Extraction Tests
// ============================================================================

TEST_CASE("extract_references parses KOS document", "[services][xds][consumer]") {
    // Create a KOS document first
    imaging_document_source source;
    auto references = create_sample_references();
    auto kos_result = source.create_kos_document("1.2.3.4.5.6.7.200", references);
    REQUIRE(kos_result.success);

    // Extract references from it
    imaging_document_consumer consumer;
    auto extract_result = consumer.extract_references(kos_result.kos_dataset.value());

    CHECK(extract_result.success);
    CHECK(extract_result.referenced_study_uids.size() == 1);
    CHECK(extract_result.referenced_study_uids[0] == "1.2.3.4.5.6.7.200");
    CHECK(extract_result.referenced_series_uids.size() == 2);
    CHECK(extract_result.referenced_instance_uids.size() == 3);
}

TEST_CASE("extract_references fails with empty dataset", "[services][xds][consumer]") {
    imaging_document_consumer consumer;
    dicom_dataset empty_ds;

    auto result = consumer.extract_references(empty_ds);
    CHECK_FALSE(result.success);
}

// ============================================================================
// Imaging Document Consumer - WADO-RS URL Tests
// ============================================================================

TEST_CASE("build_wado_rs_url constructs correct URL", "[services][xds][consumer]") {
    imaging_document_consumer_config config;
    config.wado_rs_url = "https://pacs.example.com/wado-rs";

    imaging_document_consumer consumer{config};

    auto url = consumer.build_wado_rs_url(
        "1.2.3.4.5", "1.2.3.4.6", "1.2.3.4.7");

    CHECK(url == "https://pacs.example.com/wado-rs/studies/1.2.3.4.5/series/1.2.3.4.6/instances/1.2.3.4.7");
}

// ============================================================================
// End-to-End: Source -> Consumer Round-Trip
// ============================================================================

TEST_CASE("KOS round-trip: create and extract references", "[services][xds][roundtrip]") {
    // Source creates KOS
    imaging_document_source_config src_config;
    src_config.registry_url = "https://xds.example.com/iti41";
    src_config.source_oid = "1.2.3.4.5.6.7.8.9";

    imaging_document_source source{src_config};
    auto references = create_sample_references();
    auto patient = create_patient_demographics();

    auto kos_result = source.create_kos_document(
        "1.2.3.4.5.6.7.200", references, patient);
    REQUIRE(kos_result.success);

    // Consumer extracts references
    imaging_document_consumer consumer;
    auto extract_result = consumer.extract_references(kos_result.kos_dataset.value());

    CHECK(extract_result.success);
    CHECK(extract_result.referenced_instance_uids.size() == references.size());

    // Verify all original instance UIDs are present
    for (const auto& ref : references) {
        auto it = std::find(
            extract_result.referenced_instance_uids.begin(),
            extract_result.referenced_instance_uids.end(),
            ref.sop_instance_uid);
        CHECK(it != extract_result.referenced_instance_uids.end());
    }
}

// ============================================================================
// ATNA Audit Emission Tests (Issue #1114)
// ============================================================================

namespace {

/// Captured shape of an XDS audit event emitted by the spy auditor.
struct captured_xds_audit {
    enum class kind { provide_and_register, retrieve };
    kind event_kind{kind::provide_and_register};
    std::string event_id;           // transaction code (ITI-41/RAD-68/ITI-43/RAD-69)
    std::string source_participant;
    std::string destination_participant;
    std::string study_uid;
    std::string patient_id;
    std::vector<std::string> sop_instance_uids;
    bool success{false};
};

/// Test spy that captures XDS-I.b audit calls without sending to syslog.
///
/// Uses the standard atna_service_auditor constructor with a harmless
/// transport config, then overrides the virtual audit methods so no
/// network traffic is produced.
class xds_audit_spy final : public kcenon::pacs::security::atna_service_auditor {
public:
    xds_audit_spy()
        : atna_service_auditor(
              kcenon::pacs::security::syslog_transport_config{},
              "TEST_PACS") {}

    void audit_xds_provide_and_register(
        xds_source_transaction transaction,
        const std::string& source_ae,
        const std::string& dest_ae,
        const std::string& study_uid,
        const std::string& patient_id,
        const std::vector<std::string>& sop_instance_uids,
        bool success) override {
        captured_xds_audit c;
        c.event_kind = captured_xds_audit::kind::provide_and_register;
        c.event_id = transaction == xds_source_transaction::iti_41
                         ? "ITI-41"
                         : "RAD-68";
        c.source_participant = source_ae;
        c.destination_participant = dest_ae;
        c.study_uid = study_uid;
        c.patient_id = patient_id;
        c.sop_instance_uids = sop_instance_uids;
        c.success = success;
        events.push_back(std::move(c));
    }

    void audit_xds_retrieve(
        xds_consumer_transaction transaction,
        const std::string& source_ae,
        const std::string& dest_ae,
        const std::string& study_uid,
        const std::string& patient_id,
        const std::vector<std::string>& sop_instance_uids,
        bool success) override {
        captured_xds_audit c;
        c.event_kind = captured_xds_audit::kind::retrieve;
        c.event_id = transaction == xds_consumer_transaction::iti_43
                         ? "ITI-43"
                         : "RAD-69";
        c.source_participant = source_ae;
        c.destination_participant = dest_ae;
        c.study_uid = study_uid;
        c.patient_id = patient_id;
        c.sop_instance_uids = sop_instance_uids;
        c.success = success;
        events.push_back(std::move(c));
    }

    std::vector<captured_xds_audit> events;
};

imaging_document_source make_source_with_registry() {
    imaging_document_source_config config;
    config.registry_url = "https://xds.example.com/iti41";
    config.source_oid = "1.2.3.4.5.6.7.8.9";
    return imaging_document_source{config};
}

imaging_document_source make_source_without_registry() {
    imaging_document_source_config config;
    config.source_oid = "1.2.3.4.5.6.7.8.9";  // registry_url intentionally empty
    return imaging_document_source{config};
}

document_reference make_doc_ref() {
    document_reference ref;
    ref.unique_id = "1.2.3.4.5.6.7.200";          // also serves as SOP Instance UID
    ref.repository_unique_id = "1.2.3.4.5.6.7.99";
    ref.entry_uuid = "urn:uuid:00000000-0000-0000-0000-000000000001";
    ref.patient_id = "12345";
    ref.class_code = "IMG";
    ref.type_code = "KOS";
    return ref;
}

}  // namespace

TEST_CASE("ATNA audit emitted for ITI-41 provide-and-register (success)",
          "[services][xds][atna]") {
    auto spy = std::make_shared<xds_audit_spy>();
    auto source = make_source_with_registry();
    source.set_audit_handler(spy);

    auto references = create_sample_references();
    auto patient = create_patient_demographics();
    auto kos_result = source.create_kos_document(
        "1.2.3.4.5.6.7.200", references, patient);
    REQUIRE(kos_result.success);

    auto entry = source.build_document_entry(kos_result.kos_dataset.value());
    auto pub_result = source.publish_document(
        kos_result.kos_dataset.value(), entry, /*is_imaging=*/false);

    REQUIRE(pub_result.success);
    REQUIRE(spy->events.size() == 1);

    const auto& ev = spy->events.front();
    CHECK(ev.event_kind == captured_xds_audit::kind::provide_and_register);
    CHECK(ev.event_id == "ITI-41");
    CHECK_FALSE(ev.source_participant.empty());
    CHECK_FALSE(ev.destination_participant.empty());
    CHECK(ev.study_uid == "1.2.3.4.5.6.7.200");
    CHECK(ev.patient_id == "12345");
    CHECK_FALSE(ev.sop_instance_uids.empty());
    CHECK(ev.success);
}

TEST_CASE("ATNA audit emitted for RAD-68 provide-and-register (failure path)",
          "[services][xds][atna]") {
    auto spy = std::make_shared<xds_audit_spy>();
    auto source = make_source_without_registry();  // forces failure
    source.set_audit_handler(spy);

    auto references = create_sample_references();
    auto patient = create_patient_demographics();
    auto kos_result = source.create_kos_document(
        "1.2.3.4.5.6.7.200", references, patient);
    REQUIRE(kos_result.success);

    auto entry = source.build_document_entry(kos_result.kos_dataset.value());
    auto pub_result = source.publish_document(
        kos_result.kos_dataset.value(), entry, /*is_imaging=*/true);

    CHECK_FALSE(pub_result.success);
    REQUIRE(spy->events.size() == 1);

    const auto& ev = spy->events.front();
    CHECK(ev.event_kind == captured_xds_audit::kind::provide_and_register);
    CHECK(ev.event_id == "RAD-68");
    CHECK_FALSE(ev.source_participant.empty());
    CHECK(ev.study_uid == "1.2.3.4.5.6.7.200");
    CHECK(ev.patient_id == "12345");
    // RAD-68 must carry SOP Instance UIDs from the KOS evidence sequence
    CHECK(ev.sop_instance_uids.size() == 3);
    CHECK_FALSE(ev.success);
}

TEST_CASE("ATNA audit emitted for ITI-43 retrieve document set (success)",
          "[services][xds][atna]") {
    auto spy = std::make_shared<xds_audit_spy>();
    imaging_document_consumer_config config;
    config.repository_url = "https://xds.example.com/iti43";
    imaging_document_consumer consumer{config};
    consumer.set_audit_handler(spy);

    auto result = consumer.retrieve_document(make_doc_ref(), /*is_imaging=*/false);

    CHECK(result.success);
    REQUIRE(spy->events.size() == 1);

    const auto& ev = spy->events.front();
    CHECK(ev.event_kind == captured_xds_audit::kind::retrieve);
    CHECK(ev.event_id == "ITI-43");
    CHECK_FALSE(ev.source_participant.empty());
    CHECK_FALSE(ev.destination_participant.empty());
    CHECK(ev.study_uid == "1.2.3.4.5.6.7.200");
    CHECK(ev.patient_id == "12345");
    CHECK_FALSE(ev.sop_instance_uids.empty());
    CHECK(ev.success);
}

TEST_CASE("ATNA audit emitted for RAD-69 retrieve imaging document set (failure)",
          "[services][xds][atna]") {
    auto spy = std::make_shared<xds_audit_spy>();
    imaging_document_consumer consumer;  // no repository_url configured
    consumer.set_audit_handler(spy);

    auto result = consumer.retrieve_document(make_doc_ref(), /*is_imaging=*/true);

    CHECK_FALSE(result.success);
    REQUIRE(spy->events.size() == 1);

    const auto& ev = spy->events.front();
    CHECK(ev.event_kind == captured_xds_audit::kind::retrieve);
    CHECK(ev.event_id == "RAD-69");
    CHECK_FALSE(ev.source_participant.empty());
    CHECK(ev.study_uid == "1.2.3.4.5.6.7.200");
    CHECK(ev.patient_id == "12345");
    CHECK_FALSE(ev.sop_instance_uids.empty());
    CHECK_FALSE(ev.success);
}

TEST_CASE("no audit emitted when audit handler is not installed",
          "[services][xds][atna]") {
    // Source without auditor: should not crash, no events captured.
    auto source = make_source_with_registry();
    auto references = create_sample_references();
    auto patient = create_patient_demographics();
    auto kos_result = source.create_kos_document(
        "1.2.3.4.5.6.7.200", references, patient);
    REQUIRE(kos_result.success);
    auto entry = source.build_document_entry(kos_result.kos_dataset.value());
    CHECK(source.publish_document(kos_result.kos_dataset.value(), entry).success);

    // Consumer without auditor: should not crash either.
    imaging_document_consumer_config cc;
    cc.repository_url = "https://xds.example.com/iti43";
    imaging_document_consumer consumer{cc};
    CHECK(consumer.retrieve_document(make_doc_ref()).success);
}
