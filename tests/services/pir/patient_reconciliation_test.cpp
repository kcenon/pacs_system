/**
 * @file patient_reconciliation_test.cpp
 * @brief Unit tests for IHE PIR Patient Information Reconciliation
 */

#include <kcenon/pacs/services/pir/patient_reconciliation_service.h>
#include <kcenon/pacs/core/dicom_dataset.h>
#include <kcenon/pacs/core/dicom_tag_constants.h>
#include <kcenon/pacs/encoding/vr_type.h>

#include <catch2/catch_test_macros.hpp>

using namespace kcenon::pacs::services::pir;
using namespace kcenon::pacs::core;
using namespace kcenon::pacs::encoding;

// ============================================================================
// Helper Functions
// ============================================================================

namespace {

dicom_dataset create_instance(const std::string& patient_id,
                              const std::string& patient_name,
                              const std::string& sop_instance_uid,
                              const std::string& study_uid) {
    dicom_dataset ds;
    ds.set_string(tags::patient_id, vr_type::LO, patient_id);
    ds.set_string(tags::patient_name, vr_type::PN, patient_name);
    ds.set_string(tags::patient_birth_date, vr_type::DA, "19800101");
    ds.set_string(tags::patient_sex, vr_type::CS, "M");
    ds.set_string(tags::sop_instance_uid, vr_type::UI, sop_instance_uid);
    ds.set_string(tags::study_instance_uid, vr_type::UI, study_uid);
    ds.set_string(tags::series_instance_uid, vr_type::UI,
                  "1.2.3.4.5.6.7.series.1");
    ds.set_string(tags::sop_class_uid, vr_type::UI,
                  "1.2.840.10008.5.1.4.1.1.2");  // CT Image Storage
    ds.set_string(tags::modality, vr_type::CS, "CT");
    return ds;
}

patient_reconciliation_service create_populated_service() {
    patient_reconciliation_service service;
    service.add_instance(create_instance(
        "PAT001", "SMITH^JOHN", "1.2.3.4.5.1", "1.2.3.4.5.study.1"));
    service.add_instance(create_instance(
        "PAT001", "SMITH^JOHN", "1.2.3.4.5.2", "1.2.3.4.5.study.1"));
    service.add_instance(create_instance(
        "PAT001", "SMITH^JOHN", "1.2.3.4.5.3", "1.2.3.4.5.study.2"));
    service.add_instance(create_instance(
        "PAT002", "JONES^MARY", "1.2.3.4.5.4", "1.2.3.4.5.study.3"));
    return service;
}

}  // namespace

// ============================================================================
// Instance Management
// ============================================================================

TEST_CASE("add_instance stores DICOM instances", "[services][pir]") {
    patient_reconciliation_service service;

    auto ds = create_instance(
        "PAT001", "SMITH^JOHN", "1.2.3.4.5.1", "1.2.3.4.5.study.1");
    CHECK(service.add_instance(ds));
    CHECK(service.instance_count() == 1);
}

TEST_CASE("add_instance rejects dataset without SOP Instance UID",
          "[services][pir]") {
    patient_reconciliation_service service;
    dicom_dataset ds;
    ds.set_string(tags::patient_id, vr_type::LO, "PAT001");

    CHECK_FALSE(service.add_instance(ds));
    CHECK(service.instance_count() == 0);
}

TEST_CASE("find_instances returns matching instances", "[services][pir]") {
    auto service = create_populated_service();

    auto pat1 = service.find_instances("PAT001");
    CHECK(pat1.size() == 3);

    auto pat2 = service.find_instances("PAT002");
    CHECK(pat2.size() == 1);

    auto none = service.find_instances("PAT999");
    CHECK(none.empty());
}

TEST_CASE("get_patient_ids returns distinct IDs", "[services][pir]") {
    auto service = create_populated_service();

    auto ids = service.get_patient_ids();
    CHECK(ids.size() == 2);
}

// ============================================================================
// Demographics Update
// ============================================================================

TEST_CASE("update_demographics updates patient name", "[services][pir]") {
    auto service = create_populated_service();

    demographics_update_request req;
    req.target_patient_id = "PAT001";
    req.updated_demographics.patient_name = "SMITH^JONATHAN";

    auto result = service.update_demographics(req);

    REQUIRE(result.success);
    CHECK(result.instances_updated == 3);

    auto instances = service.find_instances("PAT001");
    for (const auto& inst : instances) {
        CHECK(inst.get_string(tags::patient_name) == "SMITH^JONATHAN");
    }
}

TEST_CASE("update_demographics updates multiple fields", "[services][pir]") {
    auto service = create_populated_service();

    demographics_update_request req;
    req.target_patient_id = "PAT001";
    req.updated_demographics.patient_name = "CORRECTED^NAME";
    req.updated_demographics.patient_birth_date = "19810215";
    req.updated_demographics.patient_sex = "F";

    auto result = service.update_demographics(req);

    REQUIRE(result.success);
    CHECK(result.instances_updated == 3);

    auto instances = service.find_instances("PAT001");
    REQUIRE_FALSE(instances.empty());
    CHECK(instances[0].get_string(tags::patient_name) == "CORRECTED^NAME");
    CHECK(instances[0].get_string(tags::patient_birth_date) == "19810215");
    CHECK(instances[0].get_string(tags::patient_sex) == "F");
}

TEST_CASE("update_demographics does not affect other patients",
          "[services][pir]") {
    auto service = create_populated_service();

    demographics_update_request req;
    req.target_patient_id = "PAT001";
    req.updated_demographics.patient_name = "CHANGED^NAME";

    auto result = service.update_demographics(req);
    REQUIRE(result.success);

    auto pat2 = service.find_instances("PAT002");
    REQUIRE(pat2.size() == 1);
    CHECK(pat2[0].get_string(tags::patient_name) == "JONES^MARY");
}

TEST_CASE("update_demographics tracks affected studies", "[services][pir]") {
    auto service = create_populated_service();

    demographics_update_request req;
    req.target_patient_id = "PAT001";
    req.updated_demographics.patient_name = "UPDATED^NAME";

    auto result = service.update_demographics(req);

    REQUIRE(result.success);
    CHECK(result.studies_affected == 2);  // 2 distinct studies
}

TEST_CASE("update_demographics fails with empty patient ID",
          "[services][pir]") {
    auto service = create_populated_service();

    demographics_update_request req;
    // empty target_patient_id

    auto result = service.update_demographics(req);

    CHECK_FALSE(result.success);
    CHECK_FALSE(result.error_message.empty());
}

TEST_CASE("update_demographics fails for non-existent patient",
          "[services][pir]") {
    auto service = create_populated_service();

    demographics_update_request req;
    req.target_patient_id = "NON_EXISTENT";
    req.updated_demographics.patient_name = "TEST";

    auto result = service.update_demographics(req);

    CHECK_FALSE(result.success);
}

// ============================================================================
// Patient Merge
// ============================================================================

TEST_CASE("merge_patients reassigns instances to target",
          "[services][pir]") {
    auto service = create_populated_service();

    patient_merge_request req;
    req.source_patient_id = "PAT002";
    req.target_patient_id = "PAT001";

    auto result = service.merge_patients(req);

    REQUIRE(result.success);
    CHECK(result.instances_updated == 1);

    // All instances should now belong to PAT001
    auto pat1 = service.find_instances("PAT001");
    CHECK(pat1.size() == 4);

    auto pat2 = service.find_instances("PAT002");
    CHECK(pat2.empty());
}

TEST_CASE("merge_patients applies target demographics",
          "[services][pir]") {
    auto service = create_populated_service();

    patient_merge_request req;
    req.source_patient_id = "PAT002";
    req.target_patient_id = "PAT001";

    patient_demographics target_demo;
    target_demo.patient_name = "SMITH^JOHN^A";
    req.target_demographics = target_demo;

    auto result = service.merge_patients(req);

    REQUIRE(result.success);

    // The merged instances should have the target demographics
    auto all = service.find_instances("PAT001");
    bool found_merged = false;
    for (const auto& inst : all) {
        auto uid = inst.get_string(tags::sop_instance_uid);
        if (uid == "1.2.3.4.5.4") {  // Originally PAT002
            CHECK(inst.get_string(tags::patient_name) == "SMITH^JOHN^A");
            found_merged = true;
        }
    }
    CHECK(found_merged);
}

TEST_CASE("merge_patients fails with same source and target",
          "[services][pir]") {
    auto service = create_populated_service();

    patient_merge_request req;
    req.source_patient_id = "PAT001";
    req.target_patient_id = "PAT001";

    auto result = service.merge_patients(req);

    CHECK_FALSE(result.success);
}

TEST_CASE("merge_patients fails with empty IDs", "[services][pir]") {
    auto service = create_populated_service();

    SECTION("empty source") {
        patient_merge_request req;
        req.target_patient_id = "PAT001";

        auto result = service.merge_patients(req);
        CHECK_FALSE(result.success);
    }

    SECTION("empty target") {
        patient_merge_request req;
        req.source_patient_id = "PAT002";

        auto result = service.merge_patients(req);
        CHECK_FALSE(result.success);
    }
}

TEST_CASE("merge_patients fails for non-existent source",
          "[services][pir]") {
    auto service = create_populated_service();

    patient_merge_request req;
    req.source_patient_id = "NON_EXISTENT";
    req.target_patient_id = "PAT001";

    auto result = service.merge_patients(req);

    CHECK_FALSE(result.success);
}

// ============================================================================
// Audit Trail
// ============================================================================

TEST_CASE("audit trail records demographics update", "[services][pir]") {
    auto service = create_populated_service();

    demographics_update_request req;
    req.target_patient_id = "PAT001";
    req.updated_demographics.patient_name = "UPDATED^NAME";
    req.operator_name = "ADMIN";

    auto result = service.update_demographics(req);
    REQUIRE(result.success);

    auto trail = service.audit_trail();
    REQUIRE(trail.size() == 1);
    CHECK(trail[0].type == reconciliation_type::demographics_update);
    CHECK(trail[0].primary_patient_id == "PAT001");
    CHECK(trail[0].operator_name == "ADMIN");
    CHECK(trail[0].instances_updated == 3);
    CHECK(trail[0].success);
}

TEST_CASE("audit trail records patient merge", "[services][pir]") {
    auto service = create_populated_service();

    patient_merge_request req;
    req.source_patient_id = "PAT002";
    req.target_patient_id = "PAT001";
    req.operator_name = "MERGE_ADMIN";

    auto result = service.merge_patients(req);
    REQUIRE(result.success);

    auto trail = service.audit_trail();
    REQUIRE(trail.size() == 1);
    CHECK(trail[0].type == reconciliation_type::patient_merge);
    CHECK(trail[0].primary_patient_id == "PAT001");
    REQUIRE(trail[0].secondary_patient_id.has_value());
    CHECK(trail[0].secondary_patient_id.value() == "PAT002");
    CHECK(trail[0].operator_name == "MERGE_ADMIN");
}

TEST_CASE("audit_trail_for_patient filters by patient ID",
          "[services][pir]") {
    auto service = create_populated_service();

    // Do two operations
    demographics_update_request req1;
    req1.target_patient_id = "PAT001";
    req1.updated_demographics.patient_name = "UPDATED";
    REQUIRE(service.update_demographics(req1).success);

    patient_merge_request req2;
    req2.source_patient_id = "PAT002";
    req2.target_patient_id = "PAT001";
    REQUIRE(service.merge_patients(req2).success);

    // PAT001 should appear in both
    auto pat1_trail = service.audit_trail_for_patient("PAT001");
    CHECK(pat1_trail.size() == 2);

    // PAT002 should appear in merge only
    auto pat2_trail = service.audit_trail_for_patient("PAT002");
    CHECK(pat2_trail.size() == 1);
}

// ============================================================================
// to_string
// ============================================================================

TEST_CASE("to_string for reconciliation_type", "[services][pir]") {
    CHECK(to_string(reconciliation_type::demographics_update) ==
          "demographics_update");
    CHECK(to_string(reconciliation_type::patient_merge) == "patient_merge");
    CHECK(to_string(reconciliation_type::patient_link) == "patient_link");
}
