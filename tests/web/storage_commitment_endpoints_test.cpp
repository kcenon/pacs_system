/**
 * @file storage_commitment_endpoints_test.cpp
 * @brief Unit tests for DICOMweb Storage Commitment endpoints (Supplement 234)
 *
 * Tests the utility functions and data structures for storage commitment
 * REST API. Route handler testing requires integration test setup.
 *
 * @see DICOM Supplement 234 -- DICOMweb Storage Commitment
 */

#include <catch2/catch_test_macros.hpp>

#include "pacs/web/endpoints/storage_commitment_endpoints.hpp"

using namespace kcenon::pacs::web::storage_commitment;
using namespace kcenon::pacs::services;

// ============================================================================
// transaction_state tests
// ============================================================================

TEST_CASE("transaction_state to_string", "[web][storage-commitment]") {
    CHECK(to_string(transaction_state::pending) == "PENDING");
    CHECK(to_string(transaction_state::success) == "SUCCESS");
    CHECK(to_string(transaction_state::partial) == "PARTIAL");
    CHECK(to_string(transaction_state::failure) == "FAILURE");
}

TEST_CASE("transaction_state parse_state", "[web][storage-commitment]") {
    SECTION("Valid states") {
        auto pending = parse_state("PENDING");
        REQUIRE(pending.has_value());
        CHECK(*pending == transaction_state::pending);

        auto success = parse_state("SUCCESS");
        REQUIRE(success.has_value());
        CHECK(*success == transaction_state::success);

        auto partial = parse_state("PARTIAL");
        REQUIRE(partial.has_value());
        CHECK(*partial == transaction_state::partial);

        auto failure = parse_state("FAILURE");
        REQUIRE(failure.has_value());
        CHECK(*failure == transaction_state::failure);
    }

    SECTION("Invalid states") {
        CHECK_FALSE(parse_state("invalid").has_value());
        CHECK_FALSE(parse_state("").has_value());
        CHECK_FALSE(parse_state("pending").has_value());  // case-sensitive
    }
}

// ============================================================================
// transaction_store tests
// ============================================================================

TEST_CASE("transaction_store operations", "[web][storage-commitment]") {
    transaction_store store;

    SECTION("Empty store") {
        CHECK(store.size() == 0);
        CHECK_FALSE(store.find("1.2.3").has_value());
        CHECK(store.list().empty());
    }

    SECTION("Add and find transaction") {
        commitment_transaction txn;
        txn.transaction_uid = "2.25.1234";
        txn.state = transaction_state::pending;
        txn.created_at = std::chrono::system_clock::now();

        store.add(txn);

        CHECK(store.size() == 1);
        auto found = store.find("2.25.1234");
        REQUIRE(found.has_value());
        CHECK(found->transaction_uid == "2.25.1234");
        CHECK(found->state == transaction_state::pending);
    }

    SECTION("Find nonexistent transaction") {
        commitment_transaction txn;
        txn.transaction_uid = "2.25.1234";
        txn.state = transaction_state::pending;
        txn.created_at = std::chrono::system_clock::now();

        store.add(txn);

        CHECK_FALSE(store.find("2.25.9999").has_value());
    }

    SECTION("Update transaction") {
        commitment_transaction txn;
        txn.transaction_uid = "2.25.5678";
        txn.state = transaction_state::pending;
        txn.created_at = std::chrono::system_clock::now();

        store.add(txn);

        txn.state = transaction_state::success;
        txn.completed_at = std::chrono::system_clock::now();
        CHECK(store.update(txn));

        auto found = store.find("2.25.5678");
        REQUIRE(found.has_value());
        CHECK(found->state == transaction_state::success);
        CHECK(found->completed_at.has_value());
    }

    SECTION("Update nonexistent transaction") {
        commitment_transaction txn;
        txn.transaction_uid = "2.25.9999";
        txn.state = transaction_state::success;

        CHECK_FALSE(store.update(txn));
    }

    SECTION("List transactions") {
        for (int i = 0; i < 5; ++i) {
            commitment_transaction txn;
            txn.transaction_uid = "2.25." + std::to_string(i);
            txn.state = transaction_state::pending;
            txn.created_at = std::chrono::system_clock::now();
            store.add(txn);
        }

        CHECK(store.size() == 5);

        auto all = store.list();
        CHECK(all.size() == 5);

        auto limited = store.list(3);
        CHECK(limited.size() == 3);
    }
}

// ============================================================================
// JSON serialization tests
// ============================================================================

TEST_CASE("transaction_to_json", "[web][storage-commitment]") {
    SECTION("Pending transaction") {
        commitment_transaction txn;
        txn.transaction_uid = "2.25.1234";
        txn.state = transaction_state::pending;
        txn.created_at = std::chrono::system_clock::now();
        txn.requested_references.push_back(
            {"1.2.840.10008.5.1.4.1.1.2", "1.2.3.4.5"});

        auto json = transaction_to_json(txn);

        CHECK(json.find("\"transactionUID\":\"2.25.1234\"") != std::string::npos);
        CHECK(json.find("\"state\":\"PENDING\"") != std::string::npos);
        CHECK(json.find("\"requestedReferences\":[") != std::string::npos);
        CHECK(json.find("\"sopInstanceUID\":\"1.2.3.4.5\"") != std::string::npos);
        CHECK(json.find("\"successReferences\":[]") != std::string::npos);
        CHECK(json.find("\"failedReferences\":[]") != std::string::npos);
    }

    SECTION("Completed transaction with successes") {
        commitment_transaction txn;
        txn.transaction_uid = "2.25.5678";
        txn.state = transaction_state::success;
        txn.created_at = std::chrono::system_clock::now();
        txn.completed_at = std::chrono::system_clock::now();
        txn.requested_references.push_back(
            {"1.2.840.10008.5.1.4.1.1.2", "1.2.3.4.5"});
        txn.success_references.push_back(
            {"1.2.840.10008.5.1.4.1.1.2", "1.2.3.4.5"});

        auto json = transaction_to_json(txn);

        CHECK(json.find("\"state\":\"SUCCESS\"") != std::string::npos);
        CHECK(json.find("\"completedAt\"") != std::string::npos);
        CHECK(json.find("\"successReferences\":[{") != std::string::npos);
    }

    SECTION("Partial transaction with failures") {
        commitment_transaction txn;
        txn.transaction_uid = "2.25.9999";
        txn.state = transaction_state::partial;
        txn.created_at = std::chrono::system_clock::now();
        txn.completed_at = std::chrono::system_clock::now();

        txn.requested_references.push_back(
            {"1.2.840.10008.5.1.4.1.1.2", "1.2.3.4.5"});
        txn.requested_references.push_back(
            {"1.2.840.10008.5.1.4.1.1.2", "6.7.8.9.10"});

        txn.success_references.push_back(
            {"1.2.840.10008.5.1.4.1.1.2", "1.2.3.4.5"});
        txn.failed_references.emplace_back(
            sop_reference{"1.2.840.10008.5.1.4.1.1.2", "6.7.8.9.10"},
            commitment_failure_reason::no_such_object_instance);

        auto json = transaction_to_json(txn);

        CHECK(json.find("\"state\":\"PARTIAL\"") != std::string::npos);
        CHECK(json.find("\"failedReferences\":[{") != std::string::npos);
        CHECK(json.find("\"failureReason\"") != std::string::npos);
        CHECK(json.find("No such object instance") != std::string::npos);
    }

    SECTION("Transaction with study UID") {
        commitment_transaction txn;
        txn.transaction_uid = "2.25.1111";
        txn.state = transaction_state::pending;
        txn.study_instance_uid = "1.2.840.10008.9.9.9";
        txn.created_at = std::chrono::system_clock::now();

        auto json = transaction_to_json(txn);

        CHECK(json.find("\"studyInstanceUID\":\"1.2.840.10008.9.9.9\"") !=
              std::string::npos);
    }
}

TEST_CASE("transactions_to_json", "[web][storage-commitment]") {
    SECTION("Empty list") {
        std::vector<commitment_transaction> empty;
        CHECK(transactions_to_json(empty) == "[]");
    }

    SECTION("Multiple transactions") {
        std::vector<commitment_transaction> txns;

        commitment_transaction t1;
        t1.transaction_uid = "2.25.1";
        t1.state = transaction_state::success;
        t1.created_at = std::chrono::system_clock::now();
        txns.push_back(t1);

        commitment_transaction t2;
        t2.transaction_uid = "2.25.2";
        t2.state = transaction_state::pending;
        t2.created_at = std::chrono::system_clock::now();
        txns.push_back(t2);

        auto json = transactions_to_json(txns);
        CHECK(json.front() == '[');
        CHECK(json.back() == ']');
        CHECK(json.find("\"2.25.1\"") != std::string::npos);
        CHECK(json.find("\"2.25.2\"") != std::string::npos);
    }
}

// ============================================================================
// Request parsing tests
// ============================================================================

TEST_CASE("parse_commitment_request", "[web][storage-commitment]") {
    SECTION("Valid request with references") {
        std::string body = R"({
            "referencedSOPSequence": [
                {
                    "sopClassUID": "1.2.840.10008.5.1.4.1.1.2",
                    "sopInstanceUID": "1.2.3.4.5"
                },
                {
                    "sopClassUID": "1.2.840.10008.5.1.4.1.1.2",
                    "sopInstanceUID": "6.7.8.9.10"
                }
            ]
        })";

        auto result = parse_commitment_request(body);

        REQUIRE(result.valid);
        REQUIRE(result.references.size() == 2);
        CHECK(result.references[0].sop_class_uid == "1.2.840.10008.5.1.4.1.1.2");
        CHECK(result.references[0].sop_instance_uid == "1.2.3.4.5");
        CHECK(result.references[1].sop_instance_uid == "6.7.8.9.10");
    }

    SECTION("Single reference") {
        std::string body = R"({
            "referencedSOPSequence": [
                {"sopClassUID": "1.2.3", "sopInstanceUID": "4.5.6"}
            ]
        })";

        auto result = parse_commitment_request(body);

        REQUIRE(result.valid);
        CHECK(result.references.size() == 1);
    }

    SECTION("Empty body") {
        auto result = parse_commitment_request("");

        CHECK_FALSE(result.valid);
        CHECK_FALSE(result.error_message.empty());
    }

    SECTION("Missing referencedSOPSequence") {
        auto result = parse_commitment_request(R"({"other": "data"})");

        CHECK_FALSE(result.valid);
        CHECK(result.error_message.find("referencedSOPSequence") !=
              std::string::npos);
    }

    SECTION("Empty references array") {
        auto result = parse_commitment_request(
            R"({"referencedSOPSequence": []})");

        CHECK_FALSE(result.valid);
    }

    SECTION("Missing sopInstanceUID") {
        auto result = parse_commitment_request(
            R"({"referencedSOPSequence": [{"sopClassUID": "1.2.3"}]})");

        CHECK_FALSE(result.valid);
        CHECK(result.error_message.find("sopInstanceUID") != std::string::npos);
    }

    SECTION("Reference with sopInstanceUID only (no sopClassUID)") {
        auto result = parse_commitment_request(
            R"({"referencedSOPSequence": [{"sopInstanceUID": "1.2.3"}]})");

        REQUIRE(result.valid);
        CHECK(result.references.size() == 1);
        CHECK(result.references[0].sop_instance_uid == "1.2.3");
        CHECK(result.references[0].sop_class_uid.empty());
    }

    SECTION("Study UID parameter is accepted") {
        std::string body = R"({
            "referencedSOPSequence": [
                {"sopClassUID": "1.2.3", "sopInstanceUID": "4.5.6"}
            ]
        })";

        auto result = parse_commitment_request(body, "1.2.840.study.1");

        REQUIRE(result.valid);
        CHECK(result.references.size() == 1);
    }
}

// ============================================================================
// commitment_failure_reason integration tests
// ============================================================================

TEST_CASE("commitment_failure_reason to_string integration",
          "[web][storage-commitment]") {
    CHECK(kcenon::pacs::services::to_string(commitment_failure_reason::processing_failure) ==
          "Processing failure");
    CHECK(kcenon::pacs::services::to_string(commitment_failure_reason::no_such_object_instance) ==
          "No such object instance");
    CHECK(kcenon::pacs::services::to_string(commitment_failure_reason::resource_limitation) ==
          "Resource limitation");
    CHECK(kcenon::pacs::services::to_string(
              commitment_failure_reason::referenced_sop_class_not_supported) ==
          "Referenced SOP Class not supported");
    CHECK(kcenon::pacs::services::to_string(commitment_failure_reason::class_instance_conflict) ==
          "Class/Instance conflict");
    CHECK(kcenon::pacs::services::to_string(commitment_failure_reason::duplicate_transaction_uid) ==
          "Duplicate Transaction UID");
}
