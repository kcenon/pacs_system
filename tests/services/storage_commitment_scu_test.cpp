/**
 * @file storage_commitment_scu_test.cpp
 * @brief Unit tests for Storage Commitment Push Model SCU service
 */

#include <pacs/services/storage_commitment_scu.hpp>
#include <pacs/core/dicom_tag_constants.hpp>
#include <pacs/core/result.hpp>
#include <pacs/network/dimse/command_field.hpp>
#include <pacs/network/dimse/dimse_message.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace pacs::services;
using namespace pacs::network::dimse;
using namespace pacs::core;
using namespace pacs::encoding;

// ============================================================================
// Helper: Build N-EVENT-REPORT-RQ with dataset
// ============================================================================

static dimse_message make_event_report_with_dataset(
    const std::string& transaction_uid,
    const std::vector<sop_reference>& success_refs,
    const std::vector<std::pair<sop_reference, commitment_failure_reason>>& failed_refs,
    uint16_t event_type_id = storage_commitment_event_type_success) {

    auto msg = make_n_event_report_rq(
        1,
        storage_commitment_push_model_sop_class_uid,
        storage_commitment_push_model_sop_instance_uid,
        event_type_id);

    dicom_dataset ds;

    // Transaction UID
    ds.set_string(tags::transaction_uid, vr_type::UI, transaction_uid);

    // Referenced SOP Sequence (success)
    if (!success_refs.empty()) {
        auto& seq = ds.get_or_create_sequence(tags::referenced_sop_sequence);
        for (const auto& ref : success_refs) {
            dicom_dataset item;
            item.set_string(tags::referenced_sop_class_uid,
                            vr_type::UI, ref.sop_class_uid);
            item.set_string(tags::referenced_sop_instance_uid,
                            vr_type::UI, ref.sop_instance_uid);
            seq.push_back(std::move(item));
        }
    }

    // Failed SOP Sequence
    if (!failed_refs.empty()) {
        auto& seq = ds.get_or_create_sequence(tags::failed_sop_sequence);
        for (const auto& [ref, reason] : failed_refs) {
            dicom_dataset item;
            item.set_string(tags::referenced_sop_class_uid,
                            vr_type::UI, ref.sop_class_uid);
            item.set_string(tags::referenced_sop_instance_uid,
                            vr_type::UI, ref.sop_instance_uid);
            item.set_numeric<uint16_t>(tags::failure_reason, vr_type::US,
                                       static_cast<uint16_t>(reason));
            seq.push_back(std::move(item));
        }
    }

    msg.set_dataset(std::move(ds));
    return msg;
}

// ============================================================================
// Construction Tests
// ============================================================================

TEST_CASE("storage_commitment_scu construction", "[services][storage-commitment][scu]") {
    SECTION("default construction succeeds") {
        storage_commitment_scu scu;
        CHECK(scu.requests_sent() == 0);
        CHECK(scu.event_reports_received() == 0);
    }

    SECTION("construction with nullptr logger succeeds") {
        storage_commitment_scu scu(nullptr);
        CHECK(scu.requests_sent() == 0);
        CHECK(scu.event_reports_received() == 0);
    }
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_CASE("storage_commitment_scu statistics", "[services][storage-commitment][scu]") {
    storage_commitment_scu scu;

    SECTION("initial statistics are zero") {
        CHECK(scu.requests_sent() == 0);
        CHECK(scu.event_reports_received() == 0);
    }

    SECTION("reset_statistics clears all counters") {
        scu.reset_statistics();
        CHECK(scu.requests_sent() == 0);
        CHECK(scu.event_reports_received() == 0);
    }
}

// ============================================================================
// handle_event_report: Wrong Command Type
// ============================================================================

TEST_CASE("storage_commitment_scu rejects wrong command type",
          "[services][storage-commitment][scu]") {
    storage_commitment_scu scu;

    // Send a C-ECHO-RQ instead of N-EVENT-REPORT-RQ
    auto echo_msg = make_c_echo_rq(1);

    auto result = scu.handle_event_report(echo_msg);
    REQUIRE(result.is_err());
    CHECK(scu.event_reports_received() == 0);
}

// ============================================================================
// handle_event_report: Missing Dataset
// ============================================================================

TEST_CASE("storage_commitment_scu rejects event report without dataset",
          "[services][storage-commitment][scu]") {
    storage_commitment_scu scu;

    // Create N-EVENT-REPORT-RQ without dataset
    auto msg = make_n_event_report_rq(
        1,
        storage_commitment_push_model_sop_class_uid,
        storage_commitment_push_model_sop_instance_uid,
        storage_commitment_event_type_success);
    // Intentionally do NOT set a dataset

    auto result = scu.handle_event_report(msg);
    REQUIRE(result.is_err());
    CHECK(scu.event_reports_received() == 0);
}

// ============================================================================
// handle_event_report: All Success
// ============================================================================

TEST_CASE("storage_commitment_scu handles all-success event report",
          "[services][storage-commitment][scu]") {
    storage_commitment_scu scu;

    std::vector<sop_reference> refs = {
        {"1.2.840.10008.5.1.4.1.1.2", "1.2.3.4.5.1"},
        {"1.2.840.10008.5.1.4.1.1.2", "1.2.3.4.5.2"},
    };

    auto msg = make_event_report_with_dataset(
        "2.25.123456789", refs, {});

    auto result = scu.handle_event_report(msg);
    REQUIRE(result.is_ok());

    const auto& cr = result.value();
    CHECK(cr.transaction_uid == "2.25.123456789");
    CHECK(cr.success_references.size() == 2);
    CHECK(cr.failed_references.empty());
    CHECK(cr.all_successful());
    CHECK(cr.total_instances() == 2);
    CHECK(scu.event_reports_received() == 1);
}

// ============================================================================
// handle_event_report: Mixed Success and Failure
// ============================================================================

TEST_CASE("storage_commitment_scu handles mixed success/failure event report",
          "[services][storage-commitment][scu]") {
    storage_commitment_scu scu;

    std::vector<sop_reference> success_refs = {
        {"1.2.840.10008.5.1.4.1.1.2", "1.2.3.4.5.1"},
    };
    std::vector<std::pair<sop_reference, commitment_failure_reason>> failed_refs = {
        {{"1.2.840.10008.5.1.4.1.1.2", "1.2.3.4.5.2"},
         commitment_failure_reason::no_such_object_instance},
    };

    auto msg = make_event_report_with_dataset(
        "2.25.987654321", success_refs, failed_refs,
        storage_commitment_event_type_failure);

    auto result = scu.handle_event_report(msg);
    REQUIRE(result.is_ok());

    const auto& cr = result.value();
    CHECK(cr.transaction_uid == "2.25.987654321");
    CHECK(cr.success_references.size() == 1);
    CHECK(cr.failed_references.size() == 1);
    CHECK_FALSE(cr.all_successful());
    CHECK(cr.total_instances() == 2);

    // Check failure reason
    CHECK(cr.failed_references[0].second ==
          commitment_failure_reason::no_such_object_instance);

    CHECK(scu.event_reports_received() == 1);
}

// ============================================================================
// handle_event_report: All Failures
// ============================================================================

TEST_CASE("storage_commitment_scu handles all-failure event report",
          "[services][storage-commitment][scu]") {
    storage_commitment_scu scu;

    std::vector<std::pair<sop_reference, commitment_failure_reason>> failed_refs = {
        {{"1.2.840.10008.5.1.4.1.1.2", "1.2.3.4.5.1"},
         commitment_failure_reason::processing_failure},
        {{"1.2.840.10008.5.1.4.1.1.2", "1.2.3.4.5.2"},
         commitment_failure_reason::resource_limitation},
    };

    auto msg = make_event_report_with_dataset(
        "2.25.111222333", {}, failed_refs,
        storage_commitment_event_type_failure);

    auto result = scu.handle_event_report(msg);
    REQUIRE(result.is_ok());

    const auto& cr = result.value();
    CHECK(cr.success_references.empty());
    CHECK(cr.failed_references.size() == 2);
    CHECK_FALSE(cr.all_successful());

    CHECK(cr.failed_references[0].second ==
          commitment_failure_reason::processing_failure);
    CHECK(cr.failed_references[1].second ==
          commitment_failure_reason::resource_limitation);
}

// ============================================================================
// Callback Invocation
// ============================================================================

TEST_CASE("storage_commitment_scu invokes callback on event report",
          "[services][storage-commitment][scu]") {
    storage_commitment_scu scu;

    std::string callback_uid;
    size_t callback_count = 0;

    scu.set_commitment_callback(
        [&](const std::string& uid, const commitment_result& /*cr*/) {
            callback_uid = uid;
            callback_count++;
        });

    std::vector<sop_reference> refs = {
        {"1.2.840.10008.5.1.4.1.1.2", "1.2.3.4.5.1"},
    };

    auto msg = make_event_report_with_dataset("2.25.555666777", refs, {});

    auto result = scu.handle_event_report(msg);
    REQUIRE(result.is_ok());

    CHECK(callback_count == 1);
    CHECK(callback_uid == "2.25.555666777");
}

TEST_CASE("storage_commitment_scu no callback if not set",
          "[services][storage-commitment][scu]") {
    storage_commitment_scu scu;
    // No callback set

    std::vector<sop_reference> refs = {
        {"1.2.840.10008.5.1.4.1.1.2", "1.2.3.4.5.1"},
    };

    auto msg = make_event_report_with_dataset("2.25.888999000", refs, {});

    // Should succeed without crashing even with no callback
    auto result = scu.handle_event_report(msg);
    REQUIRE(result.is_ok());
    CHECK(scu.event_reports_received() == 1);
}

// ============================================================================
// Multiple Event Reports
// ============================================================================

TEST_CASE("storage_commitment_scu tracks multiple event reports",
          "[services][storage-commitment][scu]") {
    storage_commitment_scu scu;

    std::vector<sop_reference> refs = {
        {"1.2.840.10008.5.1.4.1.1.2", "1.2.3.4.5.1"},
    };

    auto msg1 = make_event_report_with_dataset("2.25.111", refs, {});
    auto msg2 = make_event_report_with_dataset("2.25.222", refs, {});

    auto r1 = scu.handle_event_report(msg1);
    auto r2 = scu.handle_event_report(msg2);

    REQUIRE(r1.is_ok());
    REQUIRE(r2.is_ok());
    CHECK(scu.event_reports_received() == 2);

    // Verify different transaction UIDs
    CHECK(r1.value().transaction_uid == "2.25.111");
    CHECK(r2.value().transaction_uid == "2.25.222");
}

// ============================================================================
// Event Report with Empty Sequences
// ============================================================================

TEST_CASE("storage_commitment_scu handles event report with no sequences",
          "[services][storage-commitment][scu]") {
    storage_commitment_scu scu;

    // Build a message with only Transaction UID, no sequences
    auto msg = make_n_event_report_rq(
        1,
        storage_commitment_push_model_sop_class_uid,
        storage_commitment_push_model_sop_instance_uid,
        storage_commitment_event_type_success);

    dicom_dataset ds;
    ds.set_string(tags::transaction_uid, vr_type::UI, "2.25.999");
    msg.set_dataset(std::move(ds));

    auto result = scu.handle_event_report(msg);
    REQUIRE(result.is_ok());

    const auto& cr = result.value();
    CHECK(cr.transaction_uid == "2.25.999");
    CHECK(cr.success_references.empty());
    CHECK(cr.failed_references.empty());
    CHECK_FALSE(cr.all_successful());
}

// ============================================================================
// Multiple Instances Independence
// ============================================================================

TEST_CASE("multiple storage_commitment_scu instances are independent",
          "[services][storage-commitment][scu]") {
    storage_commitment_scu scu1;
    storage_commitment_scu scu2;

    std::vector<sop_reference> refs = {
        {"1.2.840.10008.5.1.4.1.1.2", "1.2.3.4.5.1"},
    };

    auto msg = make_event_report_with_dataset("2.25.100", refs, {});

    auto r = scu1.handle_event_report(msg);
    REQUIRE(r.is_ok());

    CHECK(scu1.event_reports_received() == 1);
    CHECK(scu2.event_reports_received() == 0);
}
