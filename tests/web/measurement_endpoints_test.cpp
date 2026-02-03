/**
 * @file measurement_endpoints_test.cpp
 * @brief Unit tests for measurement API endpoints
 *
 * @see Issue #545 - Implement Annotation & Measurement APIs
 * @see Issue #582 - Part 2: Annotation & Measurement REST Endpoints
 *
 * @copyright Copyright (c) 2025
 * @license MIT
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "pacs/storage/index_database.hpp"
#include "pacs/storage/measurement_record.hpp"
#include "pacs/storage/measurement_repository.hpp"
#include "pacs/web/rest_types.hpp"

using namespace pacs::storage;
using namespace pacs::web;

TEST_CASE("Measurement type conversion", "[web][measurement]") {
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

TEST_CASE("Measurement query structure", "[web][measurement]") {
  measurement_query query;

  SECTION("default values") {
    REQUIRE(query.limit == 0);
    REQUIRE(query.offset == 0);
    REQUIRE_FALSE(query.has_criteria());
  }

  SECTION("with sop_instance_uid") {
    query.sop_instance_uid = "1.2.840.123456.1.1";
    REQUIRE(query.has_criteria());
  }

  SECTION("with study_uid") {
    query.study_uid = "1.2.840.123456";
    REQUIRE(query.has_criteria());
  }

  SECTION("with user_id") {
    query.user_id = "user123";
    REQUIRE(query.has_criteria());
  }

  SECTION("with type") {
    query.type = measurement_type::length;
    REQUIRE(query.has_criteria());
  }
}

TEST_CASE("Measurement record validation", "[web][measurement]") {
  measurement_record meas;

  SECTION("invalid when measurement_id is empty") {
    REQUIRE_FALSE(meas.is_valid());
  }

  SECTION("invalid when sop_instance_uid is empty") {
    meas.measurement_id = "test-uuid";
    REQUIRE_FALSE(meas.is_valid());
  }

  SECTION("valid when measurement_id and sop_instance_uid are set") {
    meas.measurement_id = "test-uuid";
    meas.sop_instance_uid = "1.2.840.123456";
    REQUIRE(meas.is_valid());
  }
}

TEST_CASE("Measurement record defaults", "[web][measurement]") {
  measurement_record meas;

  REQUIRE(meas.pk == 0);
  REQUIRE(meas.measurement_id.empty());
  REQUIRE(meas.sop_instance_uid.empty());
  REQUIRE_FALSE(meas.frame_number.has_value());
  REQUIRE(meas.user_id.empty());
  REQUIRE(meas.type == measurement_type::length);
  REQUIRE(meas.value == 0.0);
  REQUIRE(meas.unit.empty());
  REQUIRE(meas.label.empty());
}

// Repository tests require legacy SQLite interface which is only available
// when PACS_WITH_DATABASE_SYSTEM is not defined. In database_system mode,
// pacs_database_adapter opens a separate :memory: connection without tables.
#ifndef PACS_WITH_DATABASE_SYSTEM

TEST_CASE("Measurement repository operations", "[web][measurement][database]") {
  auto db_result = index_database::open(":memory:");
  REQUIRE(db_result.is_ok());
  auto &db = db_result.value();

  // Note: For in-memory databases, db_adapter() may be nullptr because the
  // database_system's pacs_database_adapter opens a separate :memory: connection.
  // Always use native_handle() for in-memory databases in tests.
  measurement_repository repo(db->native_handle());

  SECTION("save and find measurement") {
    measurement_record meas;
    meas.measurement_id = "test-uuid-123";
    meas.sop_instance_uid = "1.2.840.instance";
    meas.frame_number = 1;
    meas.user_id = "user1";
    meas.type = measurement_type::length;
    meas.geometry_json = R"({"start":{"x":0,"y":0},"end":{"x":100,"y":100}})";
    meas.value = 45.5;
    meas.unit = "mm";
    meas.label = "Tumor length";
    meas.created_at = std::chrono::system_clock::now();

    auto save_result = repo.save(meas);
    REQUIRE(save_result.is_ok());

    auto found_result = repo.find_by_id("test-uuid-123");
    REQUIRE(found_result.has_value());
    REQUIRE(found_result->measurement_id == "test-uuid-123");
    REQUIRE(found_result->sop_instance_uid == "1.2.840.instance");
    REQUIRE(found_result->type == measurement_type::length);
    REQUIRE(found_result->value == 45.5);
    REQUIRE(found_result->unit == "mm");
    REQUIRE(found_result->label == "Tumor length");
  }

  SECTION("find by instance") {
    measurement_record meas1;
    meas1.measurement_id = "meas-1";
    meas1.sop_instance_uid = "1.2.840.instance";
    meas1.type = measurement_type::length;
    meas1.value = 10.0;
    meas1.unit = "mm";
    meas1.created_at = std::chrono::system_clock::now();
    (void)repo.save(meas1);

    measurement_record meas2;
    meas2.measurement_id = "meas-2";
    meas2.sop_instance_uid = "1.2.840.instance";
    meas2.type = measurement_type::area;
    meas2.value = 25.0;
    meas2.unit = "mm2";
    meas2.created_at = std::chrono::system_clock::now();
    (void)repo.save(meas2);

    measurement_record meas3;
    meas3.measurement_id = "meas-3";
    meas3.sop_instance_uid = "1.2.840.other";
    meas3.type = measurement_type::angle;
    meas3.value = 90.0;
    meas3.unit = "degrees";
    meas3.created_at = std::chrono::system_clock::now();
    (void)repo.save(meas3);

    auto measurements_result = repo.find_by_instance("1.2.840.instance");
    REQUIRE(measurements_result.size() == 2);
  }

  SECTION("search with pagination") {
    for (int i = 1; i <= 10; ++i) {
      measurement_record meas;
      meas.measurement_id = "meas-" + std::to_string(i);
      meas.sop_instance_uid = "1.2.840.instance";
      meas.type = measurement_type::length;
      meas.value = static_cast<double>(i) * 10.0;
      meas.unit = "mm";
      meas.created_at = std::chrono::system_clock::now();
      (void)repo.save(meas);
    }

    measurement_query query;
    query.sop_instance_uid = "1.2.840.instance";
    query.limit = 5;
    query.offset = 0;

    auto page1_result = repo.search(query);
    REQUIRE(page1_result.size() == 5);

    query.offset = 5;
    auto page2_result = repo.search(query);
    REQUIRE(page2_result.size() == 5);
  }

  SECTION("search by type") {
    measurement_record meas1;
    meas1.measurement_id = "meas-length";
    meas1.sop_instance_uid = "1.2.840.instance";
    meas1.type = measurement_type::length;
    meas1.value = 10.0;
    meas1.unit = "mm";
    meas1.created_at = std::chrono::system_clock::now();
    (void)repo.save(meas1);

    measurement_record meas2;
    meas2.measurement_id = "meas-area";
    meas2.sop_instance_uid = "1.2.840.instance";
    meas2.type = measurement_type::area;
    meas2.value = 25.0;
    meas2.unit = "mm2";
    meas2.created_at = std::chrono::system_clock::now();
    (void)repo.save(meas2);

    measurement_query query;
    query.type = measurement_type::length;

    auto results = repo.search(query);
    REQUIRE(results.size() == 1);
    REQUIRE(results[0].type == measurement_type::length);
  }

  SECTION("delete measurement") {
    measurement_record meas;
    meas.measurement_id = "delete-test";
    meas.sop_instance_uid = "1.2.840.instance";
    meas.type = measurement_type::length;
    meas.value = 10.0;
    meas.unit = "mm";
    meas.created_at = std::chrono::system_clock::now();
    (void)repo.save(meas);

    REQUIRE(repo.exists("delete-test"));

    auto remove_result = repo.remove("delete-test");
    REQUIRE(remove_result.is_ok());

    REQUIRE_FALSE(repo.exists("delete-test"));
  }

  SECTION("count measurements") {
    REQUIRE(repo.count() == 0);

    measurement_record meas;
    meas.measurement_id = "count-test";
    meas.sop_instance_uid = "1.2.840.instance";
    meas.type = measurement_type::length;
    meas.value = 10.0;
    meas.unit = "mm";
    meas.created_at = std::chrono::system_clock::now();
    (void)repo.save(meas);

    REQUIRE(repo.count() == 1);
  }

  SECTION("measurement values are accurate") {
    measurement_record meas;
    meas.measurement_id = "precision-test";
    meas.sop_instance_uid = "1.2.840.instance";
    meas.type = measurement_type::length;
    meas.value = 123.456789;
    meas.unit = "mm";
    meas.created_at = std::chrono::system_clock::now();
    (void)repo.save(meas);

    auto found_result = repo.find_by_id("precision-test");
    REQUIRE(found_result.has_value());
    REQUIRE(found_result->value == Catch::Approx(123.456789).epsilon(0.0001));
  }
}

#endif  // !PACS_WITH_DATABASE_SYSTEM
