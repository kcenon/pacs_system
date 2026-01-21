/**
 * @file annotation_endpoints_test.cpp
 * @brief Unit tests for annotation API endpoints
 *
 * @see Issue #545 - Implement Annotation & Measurement APIs
 * @see Issue #582 - Part 2: Annotation & Measurement REST Endpoints
 *
 * @copyright Copyright (c) 2025
 * @license MIT
 */

#include <catch2/catch_test_macros.hpp>

#include "pacs/storage/annotation_record.hpp"
#include "pacs/storage/annotation_repository.hpp"
#include "pacs/storage/index_database.hpp"
#include "pacs/web/rest_types.hpp"

using namespace pacs::storage;
using namespace pacs::web;

TEST_CASE("Annotation type conversion", "[web][annotation]") {
  SECTION("to_string") {
    REQUIRE(to_string(annotation_type::arrow) == "arrow");
    REQUIRE(to_string(annotation_type::line) == "line");
    REQUIRE(to_string(annotation_type::rectangle) == "rectangle");
    REQUIRE(to_string(annotation_type::ellipse) == "ellipse");
    REQUIRE(to_string(annotation_type::polygon) == "polygon");
    REQUIRE(to_string(annotation_type::freehand) == "freehand");
    REQUIRE(to_string(annotation_type::text) == "text");
    REQUIRE(to_string(annotation_type::angle) == "angle");
    REQUIRE(to_string(annotation_type::roi) == "roi");
  }

  SECTION("from_string") {
    REQUIRE(annotation_type_from_string("arrow") == annotation_type::arrow);
    REQUIRE(annotation_type_from_string("line") == annotation_type::line);
    REQUIRE(annotation_type_from_string("rectangle") == annotation_type::rectangle);
    REQUIRE(annotation_type_from_string("ellipse") == annotation_type::ellipse);
    REQUIRE(annotation_type_from_string("polygon") == annotation_type::polygon);
    REQUIRE(annotation_type_from_string("freehand") == annotation_type::freehand);
    REQUIRE(annotation_type_from_string("text") == annotation_type::text);
    REQUIRE(annotation_type_from_string("angle") == annotation_type::angle);
    REQUIRE(annotation_type_from_string("roi") == annotation_type::roi);
    REQUIRE_FALSE(annotation_type_from_string("invalid").has_value());
  }
}

TEST_CASE("Annotation query structure", "[web][annotation]") {
  annotation_query query;

  SECTION("default values") {
    REQUIRE(query.limit == 0);
    REQUIRE(query.offset == 0);
    REQUIRE_FALSE(query.has_criteria());
  }

  SECTION("with study_uid") {
    query.study_uid = "1.2.840.123456";
    REQUIRE(query.has_criteria());
  }

  SECTION("with series_uid") {
    query.series_uid = "1.2.840.123456.1";
    REQUIRE(query.has_criteria());
  }

  SECTION("with sop_instance_uid") {
    query.sop_instance_uid = "1.2.840.123456.1.1";
    REQUIRE(query.has_criteria());
  }

  SECTION("with user_id") {
    query.user_id = "user123";
    REQUIRE(query.has_criteria());
  }

  SECTION("with type") {
    query.type = annotation_type::arrow;
    REQUIRE(query.has_criteria());
  }
}

TEST_CASE("Annotation record validation", "[web][annotation]") {
  annotation_record ann;

  SECTION("invalid when annotation_id is empty") {
    REQUIRE_FALSE(ann.is_valid());
  }

  SECTION("invalid when study_uid is empty") {
    ann.annotation_id = "test-uuid";
    REQUIRE_FALSE(ann.is_valid());
  }

  SECTION("valid when annotation_id and study_uid are set") {
    ann.annotation_id = "test-uuid";
    ann.study_uid = "1.2.840.123456";
    REQUIRE(ann.is_valid());
  }
}

TEST_CASE("Annotation style defaults", "[web][annotation]") {
  annotation_style style;

  REQUIRE(style.color == "#FFFF00");
  REQUIRE(style.line_width == 2);
  REQUIRE(style.fill_color.empty());
  REQUIRE(style.fill_opacity == 0.0f);
  REQUIRE(style.font_family == "Arial");
  REQUIRE(style.font_size == 14);
}

TEST_CASE("Annotation repository operations", "[web][annotation][database]") {
  auto db_result = index_database::open(":memory:");
  REQUIRE(db_result.is_ok());
  auto &db = db_result.value();

  annotation_repository repo(db->native_handle());

  SECTION("save and find annotation") {
    annotation_record ann;
    ann.annotation_id = "test-uuid-123";
    ann.study_uid = "1.2.840.study";
    ann.series_uid = "1.2.840.series";
    ann.sop_instance_uid = "1.2.840.instance";
    ann.frame_number = 1;
    ann.user_id = "user1";
    ann.type = annotation_type::arrow;
    ann.geometry_json = R"({"start":{"x":0,"y":0},"end":{"x":100,"y":100}})";
    ann.text = "Test annotation";
    ann.created_at = std::chrono::system_clock::now();
    ann.updated_at = ann.created_at;

    auto save_result = repo.save(ann);
    REQUIRE(save_result.is_ok());

    auto found = repo.find_by_id("test-uuid-123");
    REQUIRE(found.has_value());
    REQUIRE(found->annotation_id == "test-uuid-123");
    REQUIRE(found->study_uid == "1.2.840.study");
    REQUIRE(found->type == annotation_type::arrow);
    REQUIRE(found->text == "Test annotation");
  }

  SECTION("find by instance") {
    annotation_record ann1;
    ann1.annotation_id = "ann-1";
    ann1.study_uid = "1.2.840.study";
    ann1.sop_instance_uid = "1.2.840.instance";
    ann1.type = annotation_type::text;
    ann1.created_at = std::chrono::system_clock::now();
    ann1.updated_at = ann1.created_at;
    repo.save(ann1);

    annotation_record ann2;
    ann2.annotation_id = "ann-2";
    ann2.study_uid = "1.2.840.study";
    ann2.sop_instance_uid = "1.2.840.instance";
    ann2.type = annotation_type::arrow;
    ann2.created_at = std::chrono::system_clock::now();
    ann2.updated_at = ann2.created_at;
    repo.save(ann2);

    annotation_record ann3;
    ann3.annotation_id = "ann-3";
    ann3.study_uid = "1.2.840.study";
    ann3.sop_instance_uid = "1.2.840.other";
    ann3.type = annotation_type::line;
    ann3.created_at = std::chrono::system_clock::now();
    ann3.updated_at = ann3.created_at;
    repo.save(ann3);

    auto annotations = repo.find_by_instance("1.2.840.instance");
    REQUIRE(annotations.size() == 2);
  }

  SECTION("search with pagination") {
    for (int i = 1; i <= 10; ++i) {
      annotation_record ann;
      ann.annotation_id = "ann-" + std::to_string(i);
      ann.study_uid = "1.2.840.study";
      ann.type = annotation_type::text;
      ann.created_at = std::chrono::system_clock::now();
      ann.updated_at = ann.created_at;
      repo.save(ann);
    }

    annotation_query query;
    query.study_uid = "1.2.840.study";
    query.limit = 5;
    query.offset = 0;

    auto page1 = repo.search(query);
    REQUIRE(page1.size() == 5);

    query.offset = 5;
    auto page2 = repo.search(query);
    REQUIRE(page2.size() == 5);
  }

  SECTION("update annotation") {
    annotation_record ann;
    ann.annotation_id = "update-test";
    ann.study_uid = "1.2.840.study";
    ann.text = "Original text";
    ann.type = annotation_type::text;
    ann.created_at = std::chrono::system_clock::now();
    ann.updated_at = ann.created_at;
    repo.save(ann);

    ann.text = "Updated text";
    ann.updated_at = std::chrono::system_clock::now();
    auto update_result = repo.update(ann);
    REQUIRE(update_result.is_ok());

    auto found = repo.find_by_id("update-test");
    REQUIRE(found.has_value());
    REQUIRE(found->text == "Updated text");
  }

  SECTION("delete annotation") {
    annotation_record ann;
    ann.annotation_id = "delete-test";
    ann.study_uid = "1.2.840.study";
    ann.type = annotation_type::text;
    ann.created_at = std::chrono::system_clock::now();
    ann.updated_at = ann.created_at;
    repo.save(ann);

    REQUIRE(repo.exists("delete-test"));

    auto remove_result = repo.remove("delete-test");
    REQUIRE(remove_result.is_ok());

    REQUIRE_FALSE(repo.exists("delete-test"));
  }

  SECTION("count annotations") {
    REQUIRE(repo.count() == 0);

    annotation_record ann;
    ann.annotation_id = "count-test";
    ann.study_uid = "1.2.840.study";
    ann.type = annotation_type::text;
    ann.created_at = std::chrono::system_clock::now();
    ann.updated_at = ann.created_at;
    repo.save(ann);

    REQUIRE(repo.count() == 1);
  }
}
