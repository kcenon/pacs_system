/**
 * @file viewer_state_endpoints_test.cpp
 * @brief Unit tests for viewer state API endpoints
 *
 * @see Issue #545 - Implement Annotation & Measurement APIs
 * @see Issue #583 - Part 3: Key Image & Viewer State REST Endpoints
 *
 * @copyright Copyright (c) 2025
 * @license MIT
 */

#include <catch2/catch_test_macros.hpp>

#include "pacs/storage/index_database.hpp"
#include "pacs/storage/viewer_state_record.hpp"
#include "pacs/storage/viewer_state_repository.hpp"
#include "pacs/web/rest_types.hpp"

#include <chrono>
#include <thread>

using namespace pacs::storage;
using namespace pacs::web;

TEST_CASE("Viewer state query structure", "[web][viewer_state]") {
  viewer_state_query query;

  SECTION("default values") {
    REQUIRE(query.limit == 0);
    REQUIRE(query.offset == 0);
    REQUIRE_FALSE(query.has_criteria());
  }

  SECTION("with study_uid") {
    query.study_uid = "1.2.840.123456";
    REQUIRE(query.has_criteria());
  }

  SECTION("with user_id") {
    query.user_id = "user123";
    REQUIRE(query.has_criteria());
  }
}

TEST_CASE("Viewer state record validation", "[web][viewer_state]") {
  viewer_state_record state;

  SECTION("invalid when state_id is empty") {
    REQUIRE_FALSE(state.is_valid());
  }

  SECTION("invalid when study_uid is empty") {
    state.state_id = "test-uuid";
    REQUIRE_FALSE(state.is_valid());
  }

  SECTION("valid when all required fields are set") {
    state.state_id = "test-uuid";
    state.study_uid = "1.2.840.123456";
    REQUIRE(state.is_valid());
  }
}

TEST_CASE("Recent study record validation", "[web][viewer_state]") {
  recent_study_record record;

  SECTION("invalid when user_id is empty") {
    REQUIRE_FALSE(record.is_valid());
  }

  SECTION("invalid when study_uid is empty") {
    record.user_id = "user123";
    REQUIRE_FALSE(record.is_valid());
  }

  SECTION("valid when all required fields are set") {
    record.user_id = "user123";
    record.study_uid = "1.2.840.123456";
    REQUIRE(record.is_valid());
  }
}

TEST_CASE("Viewer state repository operations", "[web][viewer_state][database]") {
  auto db_result = index_database::open(":memory:");
  REQUIRE(db_result.is_ok());
  auto &db = db_result.value();

  // Always use native_handle() for in-memory databases
  // pacs_database_adapter creates a separate connection which doesn't share
  // the same in-memory database, causing tests to fail (tables not visible).
  // For file-based databases in production, db_adapter() works correctly.
  viewer_state_repository repo(db->native_handle());

  SECTION("save and find viewer state") {
    viewer_state_record state;
    state.state_id = "state-uuid-123";
    state.study_uid = "1.2.840.study";
    state.user_id = "user1";
    state.state_json = R"({"layout":{"rows":2,"cols":2},"viewports":[]})";
    state.created_at = std::chrono::system_clock::now();
    state.updated_at = state.created_at;

    auto save_result = repo.save_state(state);
    REQUIRE(save_result.is_ok());

    auto found = repo.find_state_by_id("state-uuid-123");
    REQUIRE(found.has_value());
    REQUIRE(found->state_id == "state-uuid-123");
    REQUIRE(found->study_uid == "1.2.840.study");
    REQUIRE(found->user_id == "user1");
    REQUIRE(found->state_json == R"({"layout":{"rows":2,"cols":2},"viewports":[]})");
  }

  SECTION("find states by study") {
    viewer_state_record state1;
    state1.state_id = "state-1";
    state1.study_uid = "1.2.840.study";
    state1.user_id = "user1";
    state1.state_json = "{}";
    state1.created_at = std::chrono::system_clock::now();
    state1.updated_at = state1.created_at;
    (void)repo.save_state(state1);

    viewer_state_record state2;
    state2.state_id = "state-2";
    state2.study_uid = "1.2.840.study";
    state2.user_id = "user2";
    state2.state_json = "{}";
    state2.created_at = std::chrono::system_clock::now();
    state2.updated_at = state2.created_at;
    (void)repo.save_state(state2);

    viewer_state_record state3;
    state3.state_id = "state-3";
    state3.study_uid = "1.2.840.other_study";
    state3.user_id = "user1";
    state3.state_json = "{}";
    state3.created_at = std::chrono::system_clock::now();
    state3.updated_at = state3.created_at;
    (void)repo.save_state(state3);

    auto states = repo.find_states_by_study("1.2.840.study");
    REQUIRE(states.size() == 2);
  }

  SECTION("search with pagination") {
    for (int i = 1; i <= 10; ++i) {
      viewer_state_record state;
      state.state_id = "state-" + std::to_string(i);
      state.study_uid = "1.2.840.study";
      state.user_id = "user1";
      state.state_json = "{}";
      state.created_at = std::chrono::system_clock::now();
      state.updated_at = state.created_at;
      (void)repo.save_state(state);
    }

    viewer_state_query query;
    query.study_uid = "1.2.840.study";
    query.limit = 5;
    query.offset = 0;

    auto page1 = repo.search_states(query);
    REQUIRE(page1.size() == 5);

    query.offset = 5;
    auto page2 = repo.search_states(query);
    REQUIRE(page2.size() == 5);
  }

  SECTION("delete viewer state") {
    viewer_state_record state;
    state.state_id = "delete-test";
    state.study_uid = "1.2.840.study";
    state.state_json = "{}";
    state.created_at = std::chrono::system_clock::now();
    state.updated_at = state.created_at;
    (void)repo.save_state(state);

    auto found = repo.find_state_by_id("delete-test");
    REQUIRE(found.has_value());

    auto remove_result = repo.remove_state("delete-test");
    REQUIRE(remove_result.is_ok());

    auto not_found = repo.find_state_by_id("delete-test");
    REQUIRE_FALSE(not_found.has_value());
  }

  SECTION("count viewer states") {
    REQUIRE(repo.count_states() == 0);

    viewer_state_record state;
    state.state_id = "count-test";
    state.study_uid = "1.2.840.study";
    state.state_json = "{}";
    state.created_at = std::chrono::system_clock::now();
    state.updated_at = state.created_at;
    (void)repo.save_state(state);

    REQUIRE(repo.count_states() == 1);
  }
}

TEST_CASE("Recent studies repository operations", "[web][viewer_state][database]") {
  auto db_result = index_database::open(":memory:");
  REQUIRE(db_result.is_ok());
  auto &db = db_result.value();

  // Always use native_handle() for in-memory databases
  // See comment in "Viewer state repository operations" test case above
  viewer_state_repository repo(db->native_handle());

  SECTION("record study access") {
    auto result = repo.record_study_access("user1", "1.2.840.study1");
    REQUIRE(result.is_ok());

    auto recent = repo.get_recent_studies("user1", 10);
    REQUIRE(recent.size() == 1);
    REQUIRE(recent[0].study_uid == "1.2.840.study1");
  }

  SECTION("multiple study accesses") {
    (void)repo.record_study_access("user1", "1.2.840.study1");
    (void)repo.record_study_access("user1", "1.2.840.study2");
    (void)repo.record_study_access("user1", "1.2.840.study3");

    auto recent = repo.get_recent_studies("user1", 10);
    REQUIRE(recent.size() == 3);
  }

  SECTION("recent studies with limit") {
    for (int i = 1; i <= 25; ++i) {
      (void)repo.record_study_access("user1", "1.2.840.study" + std::to_string(i));
    }

    auto recent_20 = repo.get_recent_studies("user1", 20);
    REQUIRE(recent_20.size() == 20);

    auto recent_10 = repo.get_recent_studies("user1", 10);
    REQUIRE(recent_10.size() == 10);
  }

  SECTION("recent studies ordered by access time") {
    // Small sleeps ensure distinct timestamps across all platforms
    (void)repo.record_study_access("user1", "1.2.840.study1");
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    (void)repo.record_study_access("user1", "1.2.840.study2");
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    (void)repo.record_study_access("user1", "1.2.840.study3");

    auto recent = repo.get_recent_studies("user1", 10);
    REQUIRE(recent.size() == 3);
    // Most recent first
    REQUIRE(recent[0].study_uid == "1.2.840.study3");
    REQUIRE(recent[1].study_uid == "1.2.840.study2");
    REQUIRE(recent[2].study_uid == "1.2.840.study1");
  }

  SECTION("re-access updates timestamp") {
    (void)repo.record_study_access("user1", "1.2.840.study1");
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    (void)repo.record_study_access("user1", "1.2.840.study2");
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    (void)repo.record_study_access("user1", "1.2.840.study1"); // Re-access

    auto recent = repo.get_recent_studies("user1", 10);
    REQUIRE(recent.size() == 2);
    // study1 should be most recent now
    REQUIRE(recent[0].study_uid == "1.2.840.study1");
    REQUIRE(recent[1].study_uid == "1.2.840.study2");
  }

  SECTION("clear recent studies") {
    (void)repo.record_study_access("user1", "1.2.840.study1");
    (void)repo.record_study_access("user1", "1.2.840.study2");
    (void)repo.record_study_access("user2", "1.2.840.study3");

    REQUIRE(repo.count_recent_studies("user1") == 2);
    REQUIRE(repo.count_recent_studies("user2") == 1);

    auto clear_result = repo.clear_recent_studies("user1");
    REQUIRE(clear_result.is_ok());

    REQUIRE(repo.count_recent_studies("user1") == 0);
    REQUIRE(repo.count_recent_studies("user2") == 1); // Unaffected
  }

  SECTION("count recent studies") {
    REQUIRE(repo.count_recent_studies("user1") == 0);

    (void)repo.record_study_access("user1", "1.2.840.study1");
    (void)repo.record_study_access("user1", "1.2.840.study2");

    REQUIRE(repo.count_recent_studies("user1") == 2);
  }

  SECTION("user isolation") {
    (void)repo.record_study_access("user1", "1.2.840.study1");
    (void)repo.record_study_access("user2", "1.2.840.study2");

    auto user1_recent = repo.get_recent_studies("user1", 10);
    REQUIRE(user1_recent.size() == 1);
    REQUIRE(user1_recent[0].study_uid == "1.2.840.study1");

    auto user2_recent = repo.get_recent_studies("user2", 10);
    REQUIRE(user2_recent.size() == 1);
    REQUIRE(user2_recent[0].study_uid == "1.2.840.study2");
  }
}
