/**
 * @file key_image_endpoints_test.cpp
 * @brief Unit tests for key image API endpoints
 *
 * Tests work with both legacy SQLite interface (sqlite3*) and new
 * base_repository pattern (PACS_WITH_DATABASE_SYSTEM).
 *
 * @see Issue #545 - Implement Annotation & Measurement APIs
 * @see Issue #583 - Part 3: Key Image & Viewer State REST Endpoints
 *
 * @copyright Copyright (c) 2025
 * @license MIT
 */

#include "pacs/storage/key_image_record.hpp"
#include "pacs/storage/key_image_repository.hpp"

#include <catch2/catch_test_macros.hpp>

#include "pacs/storage/index_database.hpp"
#include "pacs/web/rest_types.hpp"

using namespace pacs::storage;
using namespace pacs::web;

TEST_CASE("Key image query structure", "[web][key_image]") {
  key_image_query query;

  SECTION("default values") {
    REQUIRE(query.limit == 0);
    REQUIRE(query.offset == 0);
    REQUIRE_FALSE(query.has_criteria());
  }

  SECTION("with study_uid") {
    query.study_uid = "1.2.840.123456";
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
}

TEST_CASE("Key image record validation", "[web][key_image]") {
  key_image_record ki;

  SECTION("invalid when key_image_id is empty") {
    REQUIRE_FALSE(ki.is_valid());
  }

  SECTION("invalid when study_uid is empty") {
    ki.key_image_id = "test-uuid";
    REQUIRE_FALSE(ki.is_valid());
  }

  SECTION("invalid when sop_instance_uid is empty") {
    ki.key_image_id = "test-uuid";
    ki.study_uid = "1.2.840.123456";
    REQUIRE_FALSE(ki.is_valid());
  }

  SECTION("valid when all required fields are set") {
    ki.key_image_id = "test-uuid";
    ki.study_uid = "1.2.840.123456";
    ki.sop_instance_uid = "1.2.840.123456.1";
    REQUIRE(ki.is_valid());
  }
}

// Repository tests require legacy SQLite interface which is only available
// when PACS_WITH_DATABASE_SYSTEM is not defined. In database_system mode,
// pacs_database_adapter opens a separate :memory: connection without tables.
#ifndef PACS_WITH_DATABASE_SYSTEM

TEST_CASE("Key image repository operations", "[web][key_image][database]") {
  auto db_result = index_database::open(":memory:");
  REQUIRE(db_result.is_ok());
  auto &db = db_result.value();

  // Note: For in-memory databases, db_adapter() may be nullptr because the
  // database_system's pacs_database_adapter opens a separate :memory: connection.
  // Always use native_handle() for in-memory databases in tests.
  key_image_repository repo(db->native_handle());

  SECTION("save and find key image") {
    key_image_record ki;
    ki.key_image_id = "ki-uuid-123";
    ki.study_uid = "1.2.840.study";
    ki.sop_instance_uid = "1.2.840.instance";
    ki.frame_number = 1;
    ki.user_id = "user1";
    ki.reason = "Significant finding";
    ki.document_title = "Key Images";
    ki.created_at = std::chrono::system_clock::now();

    auto save_result = repo.save(ki);
    REQUIRE(save_result.is_ok());

    auto found_result = repo.find_by_id("ki-uuid-123");
    REQUIRE(found_result.has_value());
    REQUIRE(found_result->key_image_id == "ki-uuid-123");
    REQUIRE(found_result->study_uid == "1.2.840.study");
    REQUIRE(found_result->sop_instance_uid == "1.2.840.instance");
    REQUIRE(found_result->frame_number == 1);
    REQUIRE(found_result->reason == "Significant finding");
  }

  SECTION("find by study") {
    key_image_record ki1;
    ki1.key_image_id = "ki-1";
    ki1.study_uid = "1.2.840.study";
    ki1.sop_instance_uid = "1.2.840.instance1";
    ki1.created_at = std::chrono::system_clock::now();
    (void)repo.save(ki1);

    key_image_record ki2;
    ki2.key_image_id = "ki-2";
    ki2.study_uid = "1.2.840.study";
    ki2.sop_instance_uid = "1.2.840.instance2";
    ki2.created_at = std::chrono::system_clock::now();
    (void)repo.save(ki2);

    key_image_record ki3;
    ki3.key_image_id = "ki-3";
    ki3.study_uid = "1.2.840.other_study";
    ki3.sop_instance_uid = "1.2.840.instance3";
    ki3.created_at = std::chrono::system_clock::now();
    (void)repo.save(ki3);

    auto key_images_result = repo.find_by_study("1.2.840.study");
    REQUIRE(key_images_result.size() == 2);
  }

  SECTION("search with pagination") {
    for (int i = 1; i <= 10; ++i) {
      key_image_record ki;
      ki.key_image_id = "ki-" + std::to_string(i);
      ki.study_uid = "1.2.840.study";
      ki.sop_instance_uid = "1.2.840.instance." + std::to_string(i);
      ki.created_at = std::chrono::system_clock::now();
      (void)repo.save(ki);
    }

    key_image_query query;
    query.study_uid = "1.2.840.study";
    query.limit = 5;
    query.offset = 0;

    auto page1_result = repo.search(query);
    REQUIRE(page1_result.size() == 5);

    query.offset = 5;
    auto page2_result = repo.search(query);
    REQUIRE(page2_result.size() == 5);
  }

  SECTION("delete key image") {
    key_image_record ki;
    ki.key_image_id = "delete-test";
    ki.study_uid = "1.2.840.study";
    ki.sop_instance_uid = "1.2.840.instance";
    ki.created_at = std::chrono::system_clock::now();
    (void)repo.save(ki);

    REQUIRE(repo.exists("delete-test"));

    auto remove_result = repo.remove("delete-test");
    REQUIRE(remove_result.is_ok());

    REQUIRE_FALSE(repo.exists("delete-test"));
  }

  SECTION("count key images") {
    REQUIRE(repo.count() == 0);

    key_image_record ki;
    ki.key_image_id = "count-test";
    ki.study_uid = "1.2.840.study";
    ki.sop_instance_uid = "1.2.840.instance";
    ki.created_at = std::chrono::system_clock::now();
    (void)repo.save(ki);

    REQUIRE(repo.count() == 1);
  }

  SECTION("count by study") {
    key_image_record ki1;
    ki1.key_image_id = "ki-1";
    ki1.study_uid = "1.2.840.study1";
    ki1.sop_instance_uid = "1.2.840.instance1";
    ki1.created_at = std::chrono::system_clock::now();
    (void)repo.save(ki1);

    key_image_record ki2;
    ki2.key_image_id = "ki-2";
    ki2.study_uid = "1.2.840.study1";
    ki2.sop_instance_uid = "1.2.840.instance2";
    ki2.created_at = std::chrono::system_clock::now();
    (void)repo.save(ki2);

    key_image_record ki3;
    ki3.key_image_id = "ki-3";
    ki3.study_uid = "1.2.840.study2";
    ki3.sop_instance_uid = "1.2.840.instance3";
    ki3.created_at = std::chrono::system_clock::now();
    (void)repo.save(ki3);

    REQUIRE(repo.count_by_study("1.2.840.study1") == 2);
    REQUIRE(repo.count_by_study("1.2.840.study2") == 1);
    REQUIRE(repo.count_by_study("1.2.840.nonexistent") == 0);
  }

  SECTION("optional frame number") {
    key_image_record ki_with_frame;
    ki_with_frame.key_image_id = "ki-with-frame";
    ki_with_frame.study_uid = "1.2.840.study";
    ki_with_frame.sop_instance_uid = "1.2.840.instance";
    ki_with_frame.frame_number = 5;
    ki_with_frame.created_at = std::chrono::system_clock::now();
    (void)repo.save(ki_with_frame);

    key_image_record ki_no_frame;
    ki_no_frame.key_image_id = "ki-no-frame";
    ki_no_frame.study_uid = "1.2.840.study";
    ki_no_frame.sop_instance_uid = "1.2.840.instance2";
    ki_no_frame.created_at = std::chrono::system_clock::now();
    (void)repo.save(ki_no_frame);

    auto found_with_result = repo.find_by_id("ki-with-frame");
    REQUIRE(found_with_result.has_value());
    REQUIRE(found_with_result->frame_number.has_value());
    REQUIRE(found_with_result->frame_number.value() == 5);

    auto found_without_result = repo.find_by_id("ki-no-frame");
    REQUIRE(found_without_result.has_value());
    REQUIRE_FALSE(found_without_result->frame_number.has_value());
  }
}

#endif  // !PACS_WITH_DATABASE_SYSTEM
