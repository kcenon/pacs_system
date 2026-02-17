/**
 * @file storage_commitment_types_test.cpp
 * @brief Unit tests for Storage Commitment types and SOP class registration
 */

#include <pacs/services/storage_commitment_types.hpp>
#include <pacs/services/sop_class_registry.hpp>
#include <pacs/core/dicom_tag_constants.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace pacs::services;
using namespace pacs::core;

// ============================================================================
// SOP Reference Tests
// ============================================================================

TEST_CASE("sop_reference construction", "[services][storage_commitment]") {
    SECTION("default construction") {
        sop_reference ref;
        CHECK(ref.sop_class_uid.empty());
        CHECK(ref.sop_instance_uid.empty());
    }

    SECTION("value construction") {
        sop_reference ref{
            "1.2.840.10008.5.1.4.1.1.2",
            "1.2.3.4.5.6.7.8.9"
        };
        CHECK(ref.sop_class_uid == "1.2.840.10008.5.1.4.1.1.2");
        CHECK(ref.sop_instance_uid == "1.2.3.4.5.6.7.8.9");
    }
}

// ============================================================================
// Commitment Failure Reason Tests
// ============================================================================

TEST_CASE("commitment_failure_reason values match DICOM standard",
          "[services][storage_commitment]") {
    // PS3.4 Table J.3-2 failure reason codes
    CHECK(static_cast<uint16_t>(commitment_failure_reason::processing_failure) == 0x0110);
    CHECK(static_cast<uint16_t>(commitment_failure_reason::no_such_object_instance) == 0x0112);
    CHECK(static_cast<uint16_t>(commitment_failure_reason::resource_limitation) == 0x0213);
    CHECK(static_cast<uint16_t>(commitment_failure_reason::referenced_sop_class_not_supported) == 0x0122);
    CHECK(static_cast<uint16_t>(commitment_failure_reason::class_instance_conflict) == 0x0119);
    CHECK(static_cast<uint16_t>(commitment_failure_reason::duplicate_transaction_uid) == 0xA770);
}

TEST_CASE("commitment_failure_reason to_string", "[services][storage_commitment]") {
    CHECK(to_string(commitment_failure_reason::processing_failure) == "Processing failure");
    CHECK(to_string(commitment_failure_reason::no_such_object_instance) == "No such object instance");
    CHECK(to_string(commitment_failure_reason::resource_limitation) == "Resource limitation");
    CHECK(to_string(commitment_failure_reason::referenced_sop_class_not_supported)
          == "Referenced SOP Class not supported");
    CHECK(to_string(commitment_failure_reason::class_instance_conflict)
          == "Class/Instance conflict");
    CHECK(to_string(commitment_failure_reason::duplicate_transaction_uid)
          == "Duplicate Transaction UID");
}

// ============================================================================
// Commitment Result Tests
// ============================================================================

TEST_CASE("commitment_result", "[services][storage_commitment]") {
    SECTION("empty result") {
        commitment_result result;
        CHECK_FALSE(result.all_successful());
        CHECK(result.total_instances() == 0);
    }

    SECTION("all successful") {
        commitment_result result;
        result.transaction_uid = "1.2.3.4";
        result.success_references.push_back({"1.2.840.10008.5.1.4.1.1.2", "1.1.1"});
        result.success_references.push_back({"1.2.840.10008.5.1.4.1.1.2", "1.1.2"});
        result.timestamp = std::chrono::system_clock::now();

        CHECK(result.all_successful());
        CHECK(result.total_instances() == 2);
    }

    SECTION("partial failure") {
        commitment_result result;
        result.transaction_uid = "1.2.3.5";
        result.success_references.push_back({"1.2.840.10008.5.1.4.1.1.2", "1.1.1"});
        result.failed_references.push_back({
            {"1.2.840.10008.5.1.4.1.1.2", "1.1.2"},
            commitment_failure_reason::no_such_object_instance
        });

        CHECK_FALSE(result.all_successful());
        CHECK(result.total_instances() == 2);
    }

    SECTION("all failed") {
        commitment_result result;
        result.transaction_uid = "1.2.3.6";
        result.failed_references.push_back({
            {"1.2.840.10008.5.1.4.1.1.2", "1.1.1"},
            commitment_failure_reason::processing_failure
        });

        CHECK_FALSE(result.all_successful());
        CHECK(result.total_instances() == 1);
    }
}

// ============================================================================
// SOP Class UID Constants Tests
// ============================================================================

TEST_CASE("storage commitment SOP class UIDs", "[services][storage_commitment]") {
    CHECK(storage_commitment_push_model_sop_class_uid == "1.2.840.10008.1.20.1");
    CHECK(storage_commitment_push_model_sop_instance_uid == "1.2.840.10008.1.20.1.1");
}

TEST_CASE("storage commitment action/event type IDs", "[services][storage_commitment]") {
    CHECK(storage_commitment_action_type_request == 1);
    CHECK(storage_commitment_event_type_success == 1);
    CHECK(storage_commitment_event_type_failure == 2);
}

// ============================================================================
// SOP Class Registry Tests
// ============================================================================

TEST_CASE("storage commitment registered in SOP class registry",
          "[services][storage_commitment]") {
    auto& registry = sop_class_registry::instance();

    SECTION("SOP class is registered and supported") {
        CHECK(registry.is_supported(storage_commitment_push_model_sop_class_uid));
    }

    SECTION("SOP class info is correct") {
        const auto* info = registry.get_info(storage_commitment_push_model_sop_class_uid);
        REQUIRE(info != nullptr);
        CHECK(info->uid == "1.2.840.10008.1.20.1");
        CHECK(info->name == "Storage Commitment Push Model");
        CHECK(info->category == sop_class_category::storage_commitment);
        CHECK(info->modality == modality_type::other);
        CHECK_FALSE(info->is_retired);
        CHECK_FALSE(info->supports_multiframe);
    }

    SECTION("category query returns storage commitment SOP class") {
        auto classes = registry.get_by_category(sop_class_category::storage_commitment);
        REQUIRE(classes.size() == 1);
        CHECK(classes[0] == "1.2.840.10008.1.20.1");
    }

    SECTION("not classified as storage SOP class") {
        CHECK_FALSE(is_storage_sop_class(storage_commitment_push_model_sop_class_uid));
    }
}

// ============================================================================
// DICOM Tag Constants Tests
// ============================================================================

TEST_CASE("storage commitment DICOM tag constants", "[services][storage_commitment]") {
    // Transaction UID (0008,1195)
    CHECK(tags::transaction_uid.group() == 0x0008);
    CHECK(tags::transaction_uid.element() == 0x1195);

    // Failure Reason (0008,1197)
    CHECK(tags::failure_reason.group() == 0x0008);
    CHECK(tags::failure_reason.element() == 0x1197);

    // Failed SOP Sequence (0008,1198)
    CHECK(tags::failed_sop_sequence.group() == 0x0008);
    CHECK(tags::failed_sop_sequence.element() == 0x1198);

    // Referenced SOP Sequence (0008,1199)
    CHECK(tags::referenced_sop_sequence.group() == 0x0008);
    CHECK(tags::referenced_sop_sequence.element() == 0x1199);
}
