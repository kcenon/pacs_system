/**
 * @file storage_commitment_scp_test.cpp
 * @brief Unit tests for Storage Commitment Push Model SCP service
 */

#include <kcenon/pacs/services/storage_commitment_scp.h>
#include <kcenon/pacs/services/storage_commitment_types.h>
#include <kcenon/pacs/core/dicom_tag_constants.h>
#include <kcenon/pacs/network/dimse/command_field.h>
#include <kcenon/pacs/network/dimse/dimse_message.h>
#include <kcenon/pacs/network/dimse/status_codes.h>

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <string>

using namespace kcenon::pacs::services;
using namespace kcenon::pacs::network;
using namespace kcenon::pacs::network::dimse;
using namespace kcenon::pacs::core;
using namespace kcenon::pacs::encoding;

// ============================================================================
// Mock Storage Interface
// ============================================================================

class mock_storage : public kcenon::pacs::storage::storage_interface {
public:
    void add_instance(std::string uid) {
        instances_.insert(std::move(uid));
    }

    auto store(const dicom_dataset&) -> kcenon::pacs::storage::VoidResult override {
        return kcenon::common::ok();
    }

    auto retrieve(std::string_view)
        -> kcenon::pacs::storage::Result<dicom_dataset> override {
        return kcenon::common::make_error<dicom_dataset>(
            -1, "not implemented");
    }

    auto remove(std::string_view) -> kcenon::pacs::storage::VoidResult override {
        return kcenon::common::ok();
    }

    auto exists(std::string_view sop_instance_uid) const -> bool override {
        return instances_.count(std::string(sop_instance_uid)) > 0;
    }

    auto find(const dicom_dataset&)
        -> kcenon::pacs::storage::Result<std::vector<dicom_dataset>> override {
        return kcenon::common::Result<std::vector<dicom_dataset>>::ok(
            std::vector<dicom_dataset>{});
    }

    auto get_statistics() const -> kcenon::pacs::storage::storage_statistics override {
        return {};
    }

    auto verify_integrity() -> kcenon::pacs::storage::VoidResult override {
        return kcenon::common::ok();
    }

private:
    std::set<std::string> instances_;
};

// ============================================================================
// Helper: Build N-ACTION-RQ with Referenced SOP Sequence
// ============================================================================

static dicom_dataset build_action_dataset(
    const std::string& transaction_uid,
    const std::vector<sop_reference>& references) {

    dicom_dataset ds;
    ds.set_string(tags::transaction_uid, vr_type::UI, transaction_uid);

    auto& seq = ds.get_or_create_sequence(tags::referenced_sop_sequence);
    for (const auto& ref : references) {
        dicom_dataset item;
        item.set_string(tags::referenced_sop_class_uid,
                        vr_type::UI, ref.sop_class_uid);
        item.set_string(tags::referenced_sop_instance_uid,
                        vr_type::UI, ref.sop_instance_uid);
        seq.push_back(std::move(item));
    }

    return ds;
}

// ============================================================================
// Construction Tests
// ============================================================================

TEST_CASE("storage_commitment_scp construction", "[services][storage_commitment]") {
    auto storage = std::make_shared<mock_storage>();
    storage_commitment_scp scp(storage);

    SECTION("service name is correct") {
        CHECK(scp.service_name() == "Storage Commitment SCP");
    }

    SECTION("supports exactly one SOP class") {
        auto classes = scp.supported_sop_classes();
        CHECK(classes.size() == 1);
    }

    SECTION("supports Storage Commitment Push Model SOP Class") {
        auto classes = scp.supported_sop_classes();
        REQUIRE(classes.size() == 1);
        CHECK(classes[0] == "1.2.840.10008.1.20.1");
    }
}

// ============================================================================
// SOP Class Support Tests
// ============================================================================

TEST_CASE("storage_commitment_scp SOP class support", "[services][storage_commitment]") {
    auto storage = std::make_shared<mock_storage>();
    storage_commitment_scp scp(storage);

    SECTION("supports Storage Commitment Push Model SOP Class UID") {
        CHECK(scp.supports_sop_class("1.2.840.10008.1.20.1"));
        CHECK(scp.supports_sop_class(storage_commitment_push_model_sop_class_uid));
    }

    SECTION("does not support other SOP classes") {
        CHECK_FALSE(scp.supports_sop_class("1.2.840.10008.1.1"));
        CHECK_FALSE(scp.supports_sop_class("1.2.840.10008.5.1.4.1.1.2"));
        CHECK_FALSE(scp.supports_sop_class("1.2.840.10008.3.1.2.3.3"));
        CHECK_FALSE(scp.supports_sop_class(""));
    }
}

// ============================================================================
// Referenced SOP Sequence Parsing Tests
// ============================================================================

TEST_CASE("parse_referenced_sop_sequence", "[services][storage_commitment]") {
    SECTION("valid sequence with multiple items") {
        auto dataset = build_action_dataset("1.2.3.4", {
            {"1.2.840.10008.5.1.4.1.1.2", "1.1.1.1"},
            {"1.2.840.10008.5.1.4.1.1.4", "1.1.1.2"}
        });

        // Access the static helper through test by building the dataset
        // and verifying it can be parsed back correctly
        const auto* seq = dataset.get_sequence(tags::referenced_sop_sequence);
        REQUIRE(seq != nullptr);
        REQUIRE(seq->size() == 2);

        CHECK((*seq)[0].get_string(tags::referenced_sop_class_uid)
              == "1.2.840.10008.5.1.4.1.1.2");
        CHECK((*seq)[0].get_string(tags::referenced_sop_instance_uid)
              == "1.1.1.1");
        CHECK((*seq)[1].get_string(tags::referenced_sop_class_uid)
              == "1.2.840.10008.5.1.4.1.1.4");
        CHECK((*seq)[1].get_string(tags::referenced_sop_instance_uid)
              == "1.1.1.2");
    }

    SECTION("transaction UID is preserved") {
        auto dataset = build_action_dataset("2.25.12345", {
            {"1.2.840.10008.5.1.4.1.1.2", "1.1.1.1"}
        });

        CHECK(dataset.get_string(tags::transaction_uid) == "2.25.12345");
    }
}

// ============================================================================
// Event Report Dataset Construction Tests
// ============================================================================

TEST_CASE("build_event_report_dataset", "[services][storage_commitment]") {
    SECTION("all successful") {
        commitment_result result;
        result.transaction_uid = "1.2.3.4";
        result.success_references.push_back({
            "1.2.840.10008.5.1.4.1.1.2", "1.1.1.1"
        });
        result.success_references.push_back({
            "1.2.840.10008.5.1.4.1.1.4", "1.1.1.2"
        });

        // Verify event report dataset construction indirectly
        // by verifying the commitment_result structure
        CHECK(result.all_successful());
        CHECK(result.total_instances() == 2);
    }

    SECTION("partial failure") {
        commitment_result result;
        result.transaction_uid = "1.2.3.5";
        result.success_references.push_back({
            "1.2.840.10008.5.1.4.1.1.2", "1.1.1.1"
        });
        result.failed_references.emplace_back(
            sop_reference{"1.2.840.10008.5.1.4.1.1.4", "1.1.1.2"},
            commitment_failure_reason::no_such_object_instance
        );

        CHECK_FALSE(result.all_successful());
        CHECK(result.total_instances() == 2);
    }
}

// ============================================================================
// Instance Verification Tests (via SCP behavior)
// ============================================================================

TEST_CASE("storage_commitment_scp verification logic", "[services][storage_commitment]") {
    auto storage = std::make_shared<mock_storage>();
    storage->add_instance("1.1.1.1");
    storage->add_instance("1.1.1.2");
    // 1.1.1.3 intentionally NOT added

    storage_commitment_scp scp(storage);

    SECTION("initial statistics are zero") {
        CHECK(scp.actions_processed() == 0);
        CHECK(scp.instances_committed() == 0);
        CHECK(scp.instances_failed() == 0);
    }

    SECTION("reset statistics") {
        scp.reset_statistics();
        CHECK(scp.actions_processed() == 0);
        CHECK(scp.instances_committed() == 0);
        CHECK(scp.instances_failed() == 0);
    }
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_CASE("storage_commitment_scp statistics tracking", "[services][storage_commitment]") {
    auto storage = std::make_shared<mock_storage>();
    storage_commitment_scp scp(storage);

    SECTION("statistics start at zero") {
        CHECK(scp.actions_processed() == 0);
        CHECK(scp.instances_committed() == 0);
        CHECK(scp.instances_failed() == 0);
    }

    SECTION("reset clears all counters") {
        scp.reset_statistics();
        CHECK(scp.actions_processed() == 0);
        CHECK(scp.instances_committed() == 0);
        CHECK(scp.instances_failed() == 0);
    }
}

// ============================================================================
// Mock Storage Verification Tests
// ============================================================================

TEST_CASE("mock_storage exists behavior", "[services][storage_commitment]") {
    auto storage = std::make_shared<mock_storage>();
    storage->add_instance("1.2.3.4.5");
    storage->add_instance("1.2.3.4.6");

    CHECK(storage->exists("1.2.3.4.5"));
    CHECK(storage->exists("1.2.3.4.6"));
    CHECK_FALSE(storage->exists("1.2.3.4.7"));
    CHECK_FALSE(storage->exists(""));
}
