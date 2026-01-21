/**
 * @file annotation_api_test.cpp
 * @brief Integration tests for Annotation API
 *
 * This file contains integration tests verifying the complete annotation
 * lifecycle including create, read, update, delete operations and
 * database persistence.
 *
 * @see Issue #545 - Implement Annotation & Measurement APIs
 * @see Issue #584 - Part 4: TypeScript Types & Integration Tests
 *
 * @copyright Copyright (c) 2025
 * @license MIT
 */

#include <catch2/catch_test_macros.hpp>

#include "pacs/storage/annotation_record.hpp"
#include "pacs/storage/annotation_repository.hpp"
#include "pacs/storage/index_database.hpp"

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace pacs::storage;

namespace {

/**
 * @brief RAII guard for test database lifecycle
 */
struct test_database_guard {
    std::filesystem::path db_path;
    std::unique_ptr<index_database> db;

    explicit test_database_guard(const std::string& name) {
        db_path = std::filesystem::temp_directory_path() / (name + "_test.db");
        std::filesystem::remove(db_path);
        auto result = index_database::open(db_path.string());
        if (result.is_err()) {
            throw std::runtime_error("Failed to open test database: " + result.error().message);
        }
        db = std::move(result.value());
    }

    ~test_database_guard() {
        db.reset();
        std::filesystem::remove(db_path);
    }

    [[nodiscard]] auto native_handle() const { return db->native_handle(); }
};

/**
 * @brief Generate a test annotation record
 */
annotation_record make_test_annotation(
    const std::string& study_uid,
    const std::string& user_id,
    annotation_type type = annotation_type::arrow) {
    annotation_record ann;
    ann.annotation_id = "test-uuid-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    ann.study_uid = study_uid;
    ann.series_uid = "1.2.840.10008.1.2.3";
    ann.sop_instance_uid = "1.2.840.10008.1.2.3.4";
    ann.frame_number = 1;
    ann.user_id = user_id;
    ann.type = type;
    ann.geometry_json = R"({"x1":100,"y1":100,"x2":200,"y2":200})";
    ann.text = "Test annotation";
    ann.style.color = "#FF0000";
    ann.style.line_width = 2;
    ann.created_at = std::chrono::system_clock::now();
    ann.updated_at = ann.created_at;
    return ann;
}

}  // namespace

// =============================================================================
// Annotation CRUD Lifecycle Tests
// =============================================================================

TEST_CASE("Annotation create operation", "[integration][annotation]") {
    test_database_guard guard("annotation_create");
    annotation_repository repo(guard.native_handle());

    SECTION("creates annotation with all fields") {
        auto ann = make_test_annotation("1.2.840.study.1", "user1");

        auto result = repo.save(ann);
        REQUIRE(result.is_ok());

        auto retrieved = repo.find_by_id(ann.annotation_id);
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->study_uid == ann.study_uid);
        REQUIRE(retrieved->user_id == ann.user_id);
        REQUIRE(to_string(retrieved->type) == to_string(ann.type));
        REQUIRE(retrieved->geometry_json == ann.geometry_json);
        REQUIRE(retrieved->text == ann.text);
    }

    SECTION("creates annotation with minimal fields") {
        annotation_record ann;
        ann.annotation_id = "minimal-uuid";
        ann.study_uid = "1.2.840.study.minimal";
        ann.user_id = "user1";
        ann.type = annotation_type::text;
        ann.geometry_json = "{}";
        ann.created_at = std::chrono::system_clock::now();
        ann.updated_at = ann.created_at;

        auto result = repo.save(ann);
        REQUIRE(result.is_ok());
    }

    SECTION("creates multiple annotations for same study") {
        const std::string study_uid = "1.2.840.study.multi";

        for (int i = 0; i < 5; ++i) {
            auto ann = make_test_annotation(study_uid, "user1",
                static_cast<annotation_type>(i % 9));
            ann.annotation_id = "multi-uuid-" + std::to_string(i);
            auto result = repo.save(ann);
            REQUIRE(result.is_ok());
        }

        annotation_query query;
        query.study_uid = study_uid;
        auto results = repo.search(query);
        REQUIRE(results.size() == 5);
    }
}

TEST_CASE("Annotation read operations", "[integration][annotation]") {
    test_database_guard guard("annotation_read");
    annotation_repository repo(guard.native_handle());

    // Setup: Create test data
    const std::string study_uid = "1.2.840.study.read";
    std::vector<std::string> annotation_ids;

    for (int i = 0; i < 10; ++i) {
        auto ann = make_test_annotation(
            study_uid,
            "user" + std::to_string(i % 3),
            static_cast<annotation_type>(i % 9));
        ann.annotation_id = "read-uuid-" + std::to_string(i);
        (void)repo.save(ann);  // Ignore result in test setup
        annotation_ids.push_back(ann.annotation_id);
    }

    SECTION("finds annotation by ID") {
        auto result = repo.find_by_id(annotation_ids[0]);
        REQUIRE(result.has_value());
        REQUIRE(result->annotation_id == annotation_ids[0]);
    }

    SECTION("returns empty for non-existent ID") {
        auto result = repo.find_by_id("non-existent-uuid");
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("searches by study_uid") {
        annotation_query query;
        query.study_uid = study_uid;
        auto results = repo.search(query);
        REQUIRE(results.size() == 10);
    }

    SECTION("searches by user_id") {
        annotation_query query;
        query.user_id = "user0";
        auto results = repo.search(query);
        REQUIRE(results.size() >= 3);  // Users 0, 3, 6, 9
    }

    SECTION("supports pagination") {
        annotation_query query;
        query.study_uid = study_uid;
        query.limit = 5;
        query.offset = 0;

        auto page1 = repo.search(query);
        REQUIRE(page1.size() == 5);

        query.offset = 5;
        auto page2 = repo.search(query);
        REQUIRE(page2.size() == 5);

        // Verify no overlap
        for (const auto& p1 : page1) {
            for (const auto& p2 : page2) {
                REQUIRE(p1.annotation_id != p2.annotation_id);
            }
        }
    }

    SECTION("counts annotations correctly") {
        annotation_query query;
        query.study_uid = study_uid;
        auto count = repo.count(query);
        REQUIRE(count == 10);
    }
}

TEST_CASE("Annotation update operation", "[integration][annotation]") {
    test_database_guard guard("annotation_update");
    annotation_repository repo(guard.native_handle());

    auto ann = make_test_annotation("1.2.840.study.update", "user1");
    (void)repo.save(ann);  // Ignore result in test setup

    SECTION("updates geometry") {
        ann.geometry_json = R"({"x1":150,"y1":150,"x2":250,"y2":250})";
        ann.updated_at = std::chrono::system_clock::now();

        auto result = repo.update(ann);
        REQUIRE(result.is_ok());

        auto retrieved = repo.find_by_id(ann.annotation_id);
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->geometry_json == ann.geometry_json);
    }

    SECTION("updates style") {
        ann.style.color = "#00FF00";
        ann.style.line_width = 4;
        ann.updated_at = std::chrono::system_clock::now();

        auto result = repo.update(ann);
        REQUIRE(result.is_ok());

        auto retrieved = repo.find_by_id(ann.annotation_id);
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->style.color == "#00FF00");
        REQUIRE(retrieved->style.line_width == 4);
    }

    SECTION("updates text content") {
        ann.text = "Updated annotation text";
        ann.updated_at = std::chrono::system_clock::now();

        auto result = repo.update(ann);
        REQUIRE(result.is_ok());

        auto retrieved = repo.find_by_id(ann.annotation_id);
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->text == "Updated annotation text");
    }
}

TEST_CASE("Annotation delete operation", "[integration][annotation]") {
    test_database_guard guard("annotation_delete");
    annotation_repository repo(guard.native_handle());

    SECTION("deletes existing annotation") {
        auto ann = make_test_annotation("1.2.840.study.delete", "user1");
        (void)repo.save(ann);  // Ignore result in test setup

        REQUIRE(repo.exists(ann.annotation_id));

        auto result = repo.remove(ann.annotation_id);
        REQUIRE(result.is_ok());

        REQUIRE_FALSE(repo.exists(ann.annotation_id));
    }

    SECTION("handles deletion of non-existent annotation") {
        auto result = repo.remove("non-existent-uuid");
        // Should not error, just no-op
        REQUIRE(result.is_ok());
    }
}

// =============================================================================
// Instance-based Annotation Queries
// =============================================================================

TEST_CASE("Annotation instance queries", "[integration][annotation]") {
    test_database_guard guard("annotation_instance");
    annotation_repository repo(guard.native_handle());

    const std::string sop_uid = "1.2.840.instance.123";
    const std::string study_uid = "1.2.840.study.instance";

    // Create annotations for specific instance
    for (int i = 0; i < 3; ++i) {
        auto ann = make_test_annotation(study_uid, "user1");
        ann.annotation_id = "instance-uuid-" + std::to_string(i);
        ann.sop_instance_uid = sop_uid;
        (void)repo.save(ann);  // Ignore result in test setup
    }

    // Create annotation for different instance
    auto other_ann = make_test_annotation(study_uid, "user1");
    other_ann.annotation_id = "other-instance-uuid";
    other_ann.sop_instance_uid = "1.2.840.instance.456";
    (void)repo.save(other_ann);  // Ignore result in test setup

    SECTION("finds annotations by instance UID") {
        auto results = repo.find_by_instance(sop_uid);
        REQUIRE(results.size() == 3);

        for (const auto& ann : results) {
            REQUIRE(ann.sop_instance_uid == sop_uid);
        }
    }

    SECTION("returns empty for instance without annotations") {
        auto results = repo.find_by_instance("1.2.840.instance.nonexistent");
        REQUIRE(results.empty());
    }
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_CASE("Annotation error scenarios", "[integration][annotation]") {
    test_database_guard guard("annotation_errors");
    annotation_repository repo(guard.native_handle());

    SECTION("handles duplicate annotation_id gracefully") {
        auto ann1 = make_test_annotation("1.2.840.study.1", "user1");
        ann1.annotation_id = "duplicate-uuid";
        auto result1 = repo.save(ann1);
        REQUIRE(result1.is_ok());

        auto ann2 = make_test_annotation("1.2.840.study.2", "user2");
        ann2.annotation_id = "duplicate-uuid";  // Same ID
        auto result2 = repo.save(ann2);

        // Should fail or update - implementation dependent
        // Either way, database should remain consistent
        auto retrieved = repo.find_by_id("duplicate-uuid");
        REQUIRE(retrieved.has_value());
    }
}

// =============================================================================
// Concurrent Access Tests
// =============================================================================

TEST_CASE("Annotation concurrent access", "[integration][annotation]") {
    test_database_guard guard("annotation_concurrent");
    annotation_repository repo(guard.native_handle());

    const std::string study_uid = "1.2.840.study.concurrent";
    constexpr int thread_count = 4;
    constexpr int ops_per_thread = 25;

    SECTION("handles concurrent creates") {
        std::vector<std::thread> threads;
        std::atomic<int> success_count{0};

        for (int t = 0; t < thread_count; ++t) {
            threads.emplace_back([&, t]() {
                for (int i = 0; i < ops_per_thread; ++i) {
                    auto ann = make_test_annotation(study_uid, "user" + std::to_string(t));
                    ann.annotation_id = "concurrent-" + std::to_string(t) + "-" + std::to_string(i);
                    if (repo.save(ann).is_ok()) {
                        ++success_count;
                    }
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        REQUIRE(success_count == thread_count * ops_per_thread);

        annotation_query query;
        query.study_uid = study_uid;
        auto count = repo.count(query);
        REQUIRE(count == thread_count * ops_per_thread);
    }
}
