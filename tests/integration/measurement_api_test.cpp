/**
 * @file measurement_api_test.cpp
 * @brief Integration tests for Measurement API
 *
 * This file contains integration tests verifying the complete measurement
 * lifecycle including create, read, delete operations and database persistence.
 *
 * @note Tests in this file use the legacy SQLite interface (sqlite3*) and are
 *       only compiled when PACS_WITH_DATABASE_SYSTEM is NOT defined.
 *       For tests using the new base_repository pattern, see
 *       tests/storage/measurement_repository_test.cpp
 *
 * @see Issue #545 - Implement Annotation & Measurement APIs
 * @see Issue #584 - Part 4: TypeScript Types & Integration Tests
 *
 * @copyright Copyright (c) 2025
 * @license MIT
 */

#include "pacs/storage/measurement_record.hpp"
#include "pacs/storage/measurement_repository.hpp"

// Only compile legacy SQLite tests when PACS_WITH_DATABASE_SYSTEM is NOT defined
#ifndef PACS_WITH_DATABASE_SYSTEM

#include <catch2/catch_test_macros.hpp>

#include "pacs/storage/index_database.hpp"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <memory>
#include <mutex>
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
 * @brief Generate a test measurement record
 */
measurement_record make_test_measurement(
    const std::string& sop_instance_uid,
    const std::string& user_id,
    measurement_type type = measurement_type::length) {
    measurement_record meas;
    meas.measurement_id = "test-meas-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    meas.sop_instance_uid = sop_instance_uid;
    meas.frame_number = 1;
    meas.user_id = user_id;
    meas.type = type;
    meas.geometry_json = R"({"x1":100,"y1":100,"x2":200,"y2":200})";
    meas.value = 150.5;
    meas.unit = "mm";
    meas.label = "Test measurement";
    meas.created_at = std::chrono::system_clock::now();
    return meas;
}

}  // namespace

// =============================================================================
// Measurement CRUD Lifecycle Tests
// =============================================================================

TEST_CASE("Measurement create operation", "[integration][measurement]") {
    test_database_guard guard("measurement_create");
    measurement_repository repo(guard.native_handle());

    SECTION("creates measurement with all fields") {
        auto meas = make_test_measurement("1.2.840.instance.1", "user1");

        auto result = repo.save(meas);
        REQUIRE(result.is_ok());

        auto retrieved = repo.find_by_id(meas.measurement_id);
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->sop_instance_uid == meas.sop_instance_uid);
        REQUIRE(retrieved->user_id == meas.user_id);
        REQUIRE(to_string(retrieved->type) == to_string(meas.type));
        REQUIRE(std::abs(retrieved->value - meas.value) < 0.001);
        REQUIRE(retrieved->unit == meas.unit);
    }

    SECTION("creates measurements with different types") {
        const std::string sop_uid = "1.2.840.instance.types";

        std::vector<std::pair<measurement_type, std::string>> type_unit_pairs = {
            {measurement_type::length, "mm"},
            {measurement_type::area, "cm2"},
            {measurement_type::angle, "degrees"},
            {measurement_type::hounsfield, "HU"},
            {measurement_type::suv, "g/ml"},
            {measurement_type::ellipse_area, "cm2"},
            {measurement_type::polygon_area, "cm2"}
        };

        for (size_t i = 0; i < type_unit_pairs.size(); ++i) {
            auto meas = make_test_measurement(sop_uid, "user1", type_unit_pairs[i].first);
            meas.measurement_id = "type-uuid-" + std::to_string(i);
            meas.unit = type_unit_pairs[i].second;
            auto result = repo.save(meas);
            REQUIRE(result.is_ok());

            auto retrieved = repo.find_by_id(meas.measurement_id);
            REQUIRE(retrieved.has_value());
            REQUIRE(to_string(retrieved->type) == to_string(type_unit_pairs[i].first));
            REQUIRE(retrieved->unit == type_unit_pairs[i].second);
        }
    }

    SECTION("creates multiple measurements for same instance") {
        const std::string sop_uid = "1.2.840.instance.multi";

        for (int i = 0; i < 5; ++i) {
            auto meas = make_test_measurement(sop_uid, "user1");
            meas.measurement_id = "multi-meas-" + std::to_string(i);
            meas.value = 100.0 + i * 10.0;
            auto result = repo.save(meas);
            REQUIRE(result.is_ok());
        }

        auto results = repo.find_by_instance(sop_uid);
        REQUIRE(results.size() == 5);
    }
}

TEST_CASE("Measurement read operations", "[integration][measurement]") {
    test_database_guard guard("measurement_read");
    measurement_repository repo(guard.native_handle());

    // Setup: Create test data
    const std::string sop_uid = "1.2.840.instance.read";
    std::vector<std::string> measurement_ids;

    for (int i = 0; i < 10; ++i) {
        auto meas = make_test_measurement(
            sop_uid,
            "user" + std::to_string(i % 3),
            static_cast<measurement_type>(i % 7));
        meas.measurement_id = "read-meas-" + std::to_string(i);
        meas.value = 50.0 + i * 5.0;
        (void)repo.save(meas);  // Ignore result in test setup
        measurement_ids.push_back(meas.measurement_id);
    }

    SECTION("finds measurement by ID") {
        auto result = repo.find_by_id(measurement_ids[0]);
        REQUIRE(result.has_value());
        REQUIRE(result->measurement_id == measurement_ids[0]);
    }

    SECTION("returns empty for non-existent ID") {
        auto result = repo.find_by_id("non-existent-meas");
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("searches by sop_instance_uid") {
        measurement_query query;
        query.sop_instance_uid = sop_uid;
        auto results = repo.search(query);
        REQUIRE(results.size() == 10);
    }

    SECTION("searches by user_id") {
        measurement_query query;
        query.user_id = "user0";
        auto results = repo.search(query);
        REQUIRE(results.size() >= 3);  // Users 0, 3, 6, 9
    }

    SECTION("searches by measurement_type") {
        measurement_query query;
        query.type = measurement_type::length;
        auto results = repo.search(query);
        REQUIRE(results.size() >= 1);

        for (const auto& m : results) {
            REQUIRE(m.type == measurement_type::length);
        }
    }

    SECTION("supports pagination") {
        measurement_query query;
        query.sop_instance_uid = sop_uid;
        query.limit = 5;
        query.offset = 0;

        auto page1 = repo.search(query);
        REQUIRE(page1.size() == 5);

        query.offset = 5;
        auto page2 = repo.search(query);
        REQUIRE(page2.size() == 5);
    }

    SECTION("counts measurements correctly") {
        measurement_query query;
        query.sop_instance_uid = sop_uid;
        auto count = repo.count(query);
        REQUIRE(count == 10);
    }
}

TEST_CASE("Measurement delete operation", "[integration][measurement]") {
    test_database_guard guard("measurement_delete");
    measurement_repository repo(guard.native_handle());

    SECTION("deletes existing measurement") {
        auto meas = make_test_measurement("1.2.840.instance.delete", "user1");
        (void)repo.save(meas);  // Ignore result in test setup

        REQUIRE(repo.exists(meas.measurement_id));

        auto result = repo.remove(meas.measurement_id);
        REQUIRE(result.is_ok());

        REQUIRE_FALSE(repo.exists(meas.measurement_id));
    }

    SECTION("handles deletion of non-existent measurement") {
        auto result = repo.remove("non-existent-meas");
        REQUIRE(result.is_ok());
    }
}

// =============================================================================
// Instance-based Measurement Queries
// =============================================================================

TEST_CASE("Measurement instance queries", "[integration][measurement]") {
    test_database_guard guard("measurement_instance");
    measurement_repository repo(guard.native_handle());

    const std::string sop_uid = "1.2.840.instance.query";

    // Create measurements for specific instance
    for (int i = 0; i < 3; ++i) {
        auto meas = make_test_measurement(sop_uid, "user1");
        meas.measurement_id = "instance-meas-" + std::to_string(i);
        (void)repo.save(meas);  // Ignore result in test setup
    }

    // Create measurement for different instance
    auto other_meas = make_test_measurement("1.2.840.instance.other", "user1");
    other_meas.measurement_id = "other-meas";
    (void)repo.save(other_meas);  // Ignore result in test setup

    SECTION("finds measurements by instance UID") {
        auto results = repo.find_by_instance(sop_uid);
        REQUIRE(results.size() == 3);

        for (const auto& m : results) {
            REQUIRE(m.sop_instance_uid == sop_uid);
        }
    }

    SECTION("returns empty for instance without measurements") {
        auto results = repo.find_by_instance("1.2.840.instance.nonexistent");
        REQUIRE(results.empty());
    }
}

// =============================================================================
// Measurement Value Tests
// =============================================================================

TEST_CASE("Measurement value precision", "[integration][measurement]") {
    test_database_guard guard("measurement_precision");
    measurement_repository repo(guard.native_handle());

    SECTION("preserves decimal precision") {
        auto meas = make_test_measurement("1.2.840.instance.precision", "user1");
        meas.measurement_id = "precision-meas";
        meas.value = 123.456789;
        (void)repo.save(meas);  // Ignore result in test setup

        auto retrieved = repo.find_by_id(meas.measurement_id);
        REQUIRE(retrieved.has_value());
        REQUIRE(std::abs(retrieved->value - 123.456789) < 0.000001);
    }

    SECTION("handles zero value") {
        auto meas = make_test_measurement("1.2.840.instance.zero", "user1");
        meas.measurement_id = "zero-meas";
        meas.value = 0.0;
        (void)repo.save(meas);  // Ignore result in test setup

        auto retrieved = repo.find_by_id(meas.measurement_id);
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->value == 0.0);
    }

    SECTION("handles negative value") {
        auto meas = make_test_measurement("1.2.840.instance.negative", "user1");
        meas.measurement_id = "negative-meas";
        meas.value = -42.5;  // e.g., relative position
        (void)repo.save(meas);  // Ignore result in test setup

        auto retrieved = repo.find_by_id(meas.measurement_id);
        REQUIRE(retrieved.has_value());
        REQUIRE(std::abs(retrieved->value - (-42.5)) < 0.001);
    }

    SECTION("handles large value") {
        auto meas = make_test_measurement("1.2.840.instance.large", "user1");
        meas.measurement_id = "large-meas";
        meas.value = 999999.999;
        (void)repo.save(meas);  // Ignore result in test setup

        auto retrieved = repo.find_by_id(meas.measurement_id);
        REQUIRE(retrieved.has_value());
        REQUIRE(std::abs(retrieved->value - 999999.999) < 0.001);
    }
}

// =============================================================================
// Concurrent Access Tests
// =============================================================================

TEST_CASE("Measurement concurrent access", "[integration][measurement]") {
    test_database_guard guard("measurement_concurrent");
    measurement_repository repo(guard.native_handle());

    const std::string sop_uid = "1.2.840.instance.concurrent";
    constexpr int thread_count = 4;
    constexpr int ops_per_thread = 25;

    SECTION("handles concurrent creates") {
        std::vector<std::thread> threads;
        std::atomic<int> success_count{0};
        std::mutex repo_mutex;

        for (int t = 0; t < thread_count; ++t) {
            threads.emplace_back([&, t]() {
                for (int i = 0; i < ops_per_thread; ++i) {
                    auto meas = make_test_measurement(sop_uid, "user" + std::to_string(t));
                    meas.measurement_id = "concurrent-" + std::to_string(t) + "-" + std::to_string(i);
                    meas.value = t * 100.0 + i;
                    std::lock_guard<std::mutex> lock(repo_mutex);
                    if (repo.save(meas).is_ok()) {
                        ++success_count;
                    }
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        REQUIRE(success_count == thread_count * ops_per_thread);

        measurement_query query;
        query.sop_instance_uid = sop_uid;
        auto count = repo.count(query);
        REQUIRE(count == thread_count * ops_per_thread);
    }
}

// =============================================================================
// Measurement Type Conversion Tests
// =============================================================================

TEST_CASE("Measurement type conversion", "[integration][measurement]") {
    SECTION("to_string") {
        REQUIRE(to_string(measurement_type::length) == "length");
        REQUIRE(to_string(measurement_type::area) == "area");
        REQUIRE(to_string(measurement_type::angle) == "angle");
        REQUIRE(to_string(measurement_type::hounsfield) == "hounsfield");
        REQUIRE(to_string(measurement_type::suv) == "suv");
        REQUIRE(to_string(measurement_type::ellipse_area) == "ellipse_area");
        REQUIRE(to_string(measurement_type::polygon_area) == "polygon_area");
    }

    SECTION("from_string") {
        REQUIRE(measurement_type_from_string("length") == measurement_type::length);
        REQUIRE(measurement_type_from_string("area") == measurement_type::area);
        REQUIRE(measurement_type_from_string("angle") == measurement_type::angle);
        REQUIRE(measurement_type_from_string("hounsfield") == measurement_type::hounsfield);
        REQUIRE(measurement_type_from_string("suv") == measurement_type::suv);
        REQUIRE(measurement_type_from_string("ellipse_area") == measurement_type::ellipse_area);
        REQUIRE(measurement_type_from_string("polygon_area") == measurement_type::polygon_area);
        REQUIRE_FALSE(measurement_type_from_string("invalid").has_value());
    }
}

#endif  // !PACS_WITH_DATABASE_SYSTEM
