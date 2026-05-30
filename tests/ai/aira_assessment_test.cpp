/**
 * @file aira_assessment_test.cpp
 * @brief Unit tests for IHE AIRA AI Result Assessment
 */

#include <kcenon/pacs/ai/aira_assessment.h>
#include <kcenon/pacs/ai/aira_assessment_manager.h>
#include <kcenon/pacs/core/dicom_dataset.h>
#include <kcenon/pacs/core/dicom_tag_constants.h>
#include <kcenon/pacs/encoding/vr_type.h>

#include <catch2/catch_test_macros.hpp>

namespace ai = kcenon::pacs::ai;
namespace core = kcenon::pacs::core;
namespace encoding = kcenon::pacs::encoding;

// ============================================================================
// Helper Functions
// ============================================================================

namespace {

ai::ai_assessment create_accept_assessment() {
    ai::ai_assessment assessment;
    assessment.type = ai::assessment_type::accept;
    assessment.status = ai::assessment_status::final_;
    assessment.assessor_name = "DR^SMITH^JOHN";
    assessment.ai_result.sop_class_uid = "1.2.840.10008.5.1.4.1.1.88.22";
    assessment.ai_result.sop_instance_uid = "1.2.3.4.5.6.7.8.100";
    assessment.ai_result.study_instance_uid = "1.2.3.4.5.6.7.8.200";
    assessment.ai_result.series_instance_uid = "1.2.3.4.5.6.7.8.300";
    return assessment;
}

ai::ai_assessment create_modify_assessment() {
    ai::ai_assessment assessment;
    assessment.type = ai::assessment_type::modify;
    assessment.status = ai::assessment_status::final_;
    assessment.assessor_name = "DR^JONES";
    assessment.ai_result.sop_class_uid = "1.2.840.10008.5.1.4.1.1.66.4";
    assessment.ai_result.sop_instance_uid = "1.2.3.4.5.6.7.8.101";
    assessment.ai_result.study_instance_uid = "1.2.3.4.5.6.7.8.200";
    assessment.ai_result.series_instance_uid = "1.2.3.4.5.6.7.8.301";

    ai::assessment_modification mod;
    mod.description = "Adjusted segmentation boundary at left lung apex";
    mod.modified_result_uid = "1.2.3.4.5.6.7.8.999";
    assessment.modification = mod;

    return assessment;
}

ai::ai_assessment create_reject_assessment() {
    ai::ai_assessment assessment;
    assessment.type = ai::assessment_type::reject;
    assessment.status = ai::assessment_status::final_;
    assessment.assessor_name = "DR^WILSON";
    assessment.ai_result.sop_class_uid = "1.2.840.10008.5.1.4.1.1.88.22";
    assessment.ai_result.sop_instance_uid = "1.2.3.4.5.6.7.8.102";
    assessment.ai_result.study_instance_uid = "1.2.3.4.5.6.7.8.200";
    assessment.ai_result.series_instance_uid = "1.2.3.4.5.6.7.8.302";

    ai::assessment_rejection rej;
    rej.reason_code = "AIRA-REJ-001";
    rej.reason_scheme = "99PACS";
    rej.reason_description = "False positive - no clinically significant finding";
    assessment.rejection = rej;

    return assessment;
}

}  // namespace

// ============================================================================
// Assessment Creator - Basic Creation Tests
// ============================================================================

TEST_CASE("assessment_creator creates accept assessment SR",
          "[ai][aira][creator]") {
    ai::assessment_creator creator;
    auto assessment = create_accept_assessment();

    auto result = creator.create_assessment(assessment);

    REQUIRE(result.success);
    REQUIRE(result.sr_dataset.has_value());
    CHECK_FALSE(result.assessment_uid.empty());
}

TEST_CASE("assessment SR has Enhanced SR SOP Class UID",
          "[ai][aira][creator]") {
    ai::assessment_creator creator;
    auto assessment = create_accept_assessment();

    auto result = creator.create_assessment(assessment);

    REQUIRE(result.success);
    auto sop_class = result.sr_dataset->get_string(core::tags::sop_class_uid);
    CHECK(sop_class == "1.2.840.10008.5.1.4.1.1.88.22");
}

TEST_CASE("assessment SR has SR modality", "[ai][aira][creator]") {
    ai::assessment_creator creator;
    auto assessment = create_accept_assessment();

    auto result = creator.create_assessment(assessment);

    REQUIRE(result.success);
    CHECK(result.sr_dataset->get_string(core::tags::modality) == "SR");
}

TEST_CASE("assessment SR preserves study instance UID",
          "[ai][aira][creator]") {
    ai::assessment_creator creator;
    auto assessment = create_accept_assessment();

    auto result = creator.create_assessment(assessment);

    REQUIRE(result.success);
    CHECK(result.sr_dataset->get_string(core::tags::study_instance_uid) ==
          "1.2.3.4.5.6.7.8.200");
}

TEST_CASE("assessment SR has content sequence", "[ai][aira][creator]") {
    ai::assessment_creator creator;
    auto assessment = create_accept_assessment();

    auto result = creator.create_assessment(assessment);

    REQUIRE(result.success);
    constexpr core::dicom_tag content_sequence{0x0040, 0xA730};
    const auto* elem = result.sr_dataset->get(content_sequence);
    REQUIRE(elem != nullptr);
    CHECK(elem->is_sequence());
    CHECK_FALSE(elem->sequence_items().empty());
}

TEST_CASE("assessment SR has referenced SOP sequence",
          "[ai][aira][creator]") {
    ai::assessment_creator creator;
    auto assessment = create_accept_assessment();

    auto result = creator.create_assessment(assessment);

    REQUIRE(result.success);
    constexpr core::dicom_tag ref_sop_seq{0x0008, 0x1199};
    const auto* elem = result.sr_dataset->get(ref_sop_seq);
    REQUIRE(elem != nullptr);
    CHECK(elem->is_sequence());
    REQUIRE_FALSE(elem->sequence_items().empty());

    constexpr core::dicom_tag ref_sop_uid{0x0008, 0x1155};
    auto ref_uid = elem->sequence_items()[0].get_string(ref_sop_uid);
    CHECK(ref_uid == "1.2.3.4.5.6.7.8.100");
}

// ============================================================================
// Assessment Creator - Assessment Types
// ============================================================================

TEST_CASE("modify assessment includes modification description",
          "[ai][aira][creator]") {
    ai::assessment_creator creator;
    auto assessment = create_modify_assessment();

    auto result = creator.create_assessment(assessment);

    REQUIRE(result.success);
    // Referenced SOP sequence should have 2 items (original + modified)
    constexpr core::dicom_tag ref_sop_seq{0x0008, 0x1199};
    const auto* elem = result.sr_dataset->get(ref_sop_seq);
    REQUIRE(elem != nullptr);
    CHECK(elem->sequence_items().size() == 2);
}

TEST_CASE("reject assessment includes rejection reason",
          "[ai][aira][creator]") {
    ai::assessment_creator creator;
    auto assessment = create_reject_assessment();

    auto result = creator.create_assessment(assessment);

    REQUIRE(result.success);
    // Content sequence should have rejection reason item
    constexpr core::dicom_tag content_seq{0x0040, 0xA730};
    const auto* content = result.sr_dataset->get(content_seq);
    REQUIRE(content != nullptr);
    // At least: assessment type + assessor + rejection reason = 3 items
    CHECK(content->sequence_items().size() >= 3);
}

TEST_CASE("assessment with comment adds text content item",
          "[ai][aira][creator]") {
    ai::assessment_creator creator;
    auto assessment = create_accept_assessment();
    assessment.comment = "AI result matches clinical expectations";

    auto result = creator.create_assessment(assessment);

    REQUIRE(result.success);
    constexpr core::dicom_tag content_seq{0x0040, 0xA730};
    const auto* content = result.sr_dataset->get(content_seq);
    REQUIRE(content != nullptr);
    // assessment type + assessor + comment = 3 items
    CHECK(content->sequence_items().size() == 3);
}

// ============================================================================
// Assessment Creator - Validation
// ============================================================================

TEST_CASE("assessment fails without AI result UID", "[ai][aira][creator]") {
    ai::assessment_creator creator;
    ai::ai_assessment assessment;
    assessment.type = ai::assessment_type::accept;
    assessment.assessor_name = "DR^TEST";
    // No AI result UID

    auto result = creator.create_assessment(assessment);

    CHECK_FALSE(result.success);
    CHECK_FALSE(result.error_message.empty());
}

TEST_CASE("assessment fails without assessor name", "[ai][aira][creator]") {
    ai::assessment_creator creator;
    ai::ai_assessment assessment;
    assessment.type = ai::assessment_type::accept;
    assessment.ai_result.sop_instance_uid = "1.2.3.4.5";
    // No assessor name

    auto result = creator.create_assessment(assessment);

    CHECK_FALSE(result.success);
    CHECK_FALSE(result.error_message.empty());
}

// ============================================================================
// Assessment Creator - Static Methods
// ============================================================================

TEST_CASE("assessment_type_to_code returns correct codes",
          "[ai][aira][creator]") {
    CHECK(ai::assessment_creator::assessment_type_to_code(
              ai::assessment_type::accept) == "AIRA-ACCEPT");
    CHECK(ai::assessment_creator::assessment_type_to_code(
              ai::assessment_type::modify) == "AIRA-MODIFY");
    CHECK(ai::assessment_creator::assessment_type_to_code(
              ai::assessment_type::reject) == "AIRA-REJECT");
}

TEST_CASE("status_to_completion_flag returns correct flags",
          "[ai][aira][creator]") {
    CHECK(ai::assessment_creator::status_to_completion_flag(
              ai::assessment_status::draft) == "PARTIAL");
    CHECK(ai::assessment_creator::status_to_completion_flag(
              ai::assessment_status::final_) == "COMPLETE");
    CHECK(ai::assessment_creator::status_to_completion_flag(
              ai::assessment_status::amended) == "COMPLETE");
}

TEST_CASE("to_string conversions", "[ai][aira]") {
    CHECK(ai::to_string(ai::assessment_type::accept) == "accept");
    CHECK(ai::to_string(ai::assessment_type::modify) == "modify");
    CHECK(ai::to_string(ai::assessment_type::reject) == "reject");

    CHECK(ai::to_string(ai::assessment_status::draft) == "draft");
    CHECK(ai::to_string(ai::assessment_status::final_) == "final");
    CHECK(ai::to_string(ai::assessment_status::amended) == "amended");
}

// ============================================================================
// Assessment Manager - Store and Retrieve
// ============================================================================

TEST_CASE("assessment_manager stores and retrieves assessment",
          "[ai][aira][manager]") {
    ai::assessment_creator creator;
    ai::assessment_manager manager;

    auto assessment = create_accept_assessment();
    auto result = creator.create_assessment(assessment);
    REQUIRE(result.success);

    CHECK(manager.store_assessment(result.sr_dataset.value()));
    CHECK(manager.count() == 1);

    auto retrieved = manager.retrieve_assessment(result.assessment_uid);
    REQUIRE(retrieved.has_value());
    CHECK(retrieved->get_string(core::tags::sop_instance_uid) ==
          result.assessment_uid);
}

TEST_CASE("assessment_manager rejects dataset without UID",
          "[ai][aira][manager]") {
    ai::assessment_manager manager;
    core::dicom_dataset empty_ds;

    CHECK_FALSE(manager.store_assessment(empty_ds));
    CHECK(manager.count() == 0);
}

// ============================================================================
// Assessment Manager - Query Operations
// ============================================================================

TEST_CASE("assessment_manager finds by AI result UID",
          "[ai][aira][manager]") {
    ai::assessment_creator creator;
    ai::assessment_manager manager;

    // Create and store multiple assessments
    auto accept = create_accept_assessment();
    auto accept_result = creator.create_assessment(accept);
    REQUIRE(accept_result.success);
    REQUIRE(manager.store_assessment(accept_result.sr_dataset.value()));

    auto reject = create_reject_assessment();
    auto reject_result = creator.create_assessment(reject);
    REQUIRE(reject_result.success);
    REQUIRE(manager.store_assessment(reject_result.sr_dataset.value()));

    // Find by accept assessment's AI result UID
    auto found = manager.find_by_ai_result("1.2.3.4.5.6.7.8.100");
    CHECK(found.size() == 1);

    // Find by reject assessment's AI result UID
    auto found2 = manager.find_by_ai_result("1.2.3.4.5.6.7.8.102");
    CHECK(found2.size() == 1);

    // Non-existent
    auto found3 = manager.find_by_ai_result("non-existent");
    CHECK(found3.empty());
}

TEST_CASE("assessment_manager finds by assessor", "[ai][aira][manager]") {
    ai::assessment_creator creator;
    ai::assessment_manager manager;

    auto accept = create_accept_assessment();
    auto result1 = creator.create_assessment(accept);
    REQUIRE(manager.store_assessment(result1.sr_dataset.value()));

    auto modify = create_modify_assessment();
    auto result2 = creator.create_assessment(modify);
    REQUIRE(manager.store_assessment(result2.sr_dataset.value()));

    auto by_smith = manager.find_by_assessor("DR^SMITH^JOHN");
    CHECK(by_smith.size() == 1);

    auto by_jones = manager.find_by_assessor("DR^JONES");
    CHECK(by_jones.size() == 1);

    auto by_unknown = manager.find_by_assessor("DR^UNKNOWN");
    CHECK(by_unknown.empty());
}

TEST_CASE("assessment_manager finds by type", "[ai][aira][manager]") {
    ai::assessment_creator creator;
    ai::assessment_manager manager;

    auto accept = create_accept_assessment();
    auto r1 = creator.create_assessment(accept);
    REQUIRE(manager.store_assessment(r1.sr_dataset.value()));

    auto reject = create_reject_assessment();
    auto r2 = creator.create_assessment(reject);
    REQUIRE(manager.store_assessment(r2.sr_dataset.value()));

    auto accepts = manager.find_by_type(ai::assessment_type::accept);
    CHECK(accepts.size() == 1);

    auto rejects = manager.find_by_type(ai::assessment_type::reject);
    CHECK(rejects.size() == 1);

    auto modifies = manager.find_by_type(ai::assessment_type::modify);
    CHECK(modifies.empty());
}

// ============================================================================
// Assessment Manager - Remove and Exists
// ============================================================================

TEST_CASE("assessment_manager remove and exists", "[ai][aira][manager]") {
    ai::assessment_creator creator;
    ai::assessment_manager manager;

    auto assessment = create_accept_assessment();
    auto result = creator.create_assessment(assessment);
    REQUIRE(result.success);
    REQUIRE(manager.store_assessment(result.sr_dataset.value()));

    CHECK(manager.exists(result.assessment_uid));
    CHECK(manager.remove(result.assessment_uid));
    CHECK_FALSE(manager.exists(result.assessment_uid));
    CHECK(manager.count() == 0);
}

// ============================================================================
// Assessment Manager - Statistics
// ============================================================================

TEST_CASE("assessment_manager statistics", "[ai][aira][manager]") {
    ai::assessment_creator creator;
    ai::assessment_manager manager;

    auto accept = create_accept_assessment();
    auto r1 = creator.create_assessment(accept);
    REQUIRE(manager.store_assessment(r1.sr_dataset.value()));

    auto modify = create_modify_assessment();
    auto r2 = creator.create_assessment(modify);
    REQUIRE(manager.store_assessment(r2.sr_dataset.value()));

    auto reject = create_reject_assessment();
    auto r3 = creator.create_assessment(reject);
    REQUIRE(manager.store_assessment(r3.sr_dataset.value()));

    auto stats = manager.get_statistics();
    CHECK(stats[ai::assessment_type::accept] == 1);
    CHECK(stats[ai::assessment_type::modify] == 1);
    CHECK(stats[ai::assessment_type::reject] == 1);
}

// ============================================================================
// Assessment Manager - Info Retrieval
// ============================================================================

TEST_CASE("assessment_manager get_info returns metadata",
          "[ai][aira][manager]") {
    ai::assessment_creator creator;
    ai::assessment_manager manager;

    auto assessment = create_accept_assessment();
    auto result = creator.create_assessment(assessment);
    REQUIRE(result.success);
    REQUIRE(manager.store_assessment(result.sr_dataset.value()));

    auto info = manager.get_info(result.assessment_uid);
    REQUIRE(info.has_value());
    CHECK(info->assessment_uid == result.assessment_uid);
    CHECK(info->type == ai::assessment_type::accept);
    CHECK(info->assessor_name == "DR^SMITH^JOHN");
    CHECK(info->ai_result_uid == "1.2.3.4.5.6.7.8.100");
}

// ============================================================================
// End-to-End: Creator -> Manager Round-Trip
// ============================================================================

TEST_CASE("full AIRA workflow: create, store, query, retrieve",
          "[ai][aira][roundtrip]") {
    ai::assessment_creator creator;
    ai::assessment_manager manager;

    // Create assessments for the same AI result
    auto accept = create_accept_assessment();
    accept.comment = "Confirmed lung nodule detection";

    auto accept_result = creator.create_assessment(accept);
    REQUIRE(accept_result.success);
    REQUIRE(manager.store_assessment(accept_result.sr_dataset.value()));

    // Query assessments
    auto assessments = manager.find_by_ai_result("1.2.3.4.5.6.7.8.100");
    REQUIRE(assessments.size() == 1);
    CHECK(assessments[0].type == ai::assessment_type::accept);

    // Retrieve and verify SR content
    auto retrieved = manager.retrieve_assessment(
        assessments[0].assessment_uid);
    REQUIRE(retrieved.has_value());
    CHECK(retrieved->get_string(core::tags::modality) == "SR");
    CHECK(retrieved->get_string(core::tags::study_instance_uid) ==
          "1.2.3.4.5.6.7.8.200");
}
