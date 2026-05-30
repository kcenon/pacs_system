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

#include "kcenon/pacs/storage/index_database.h"
#include "kcenon/pacs/storage/migration_runner.h"
#ifdef PACS_WITH_DATABASE_SYSTEM
#include "kcenon/pacs/storage/pacs_database_adapter.h"
#include "kcenon/pacs/storage/recent_study_repository.h"
#include "kcenon/pacs/storage/viewer_state_record_repository.h"
#else
#include "kcenon/pacs/storage/viewer_state_record.h"
#include "kcenon/pacs/storage/viewer_state_repository.h"
#endif
#include "kcenon/pacs/web/rest_types.h"

#include <chrono>
#include <filesystem>
#include <thread>

using namespace kcenon::pacs::storage;
using namespace kcenon::pacs::web;

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

#ifdef PACS_WITH_DATABASE_SYSTEM
namespace {
/**
 * @brief Check if SQLite backend is supported by unified_database_system
 *
 * Creates a test adapter to verify SQLite connectivity works.
 */
bool is_sqlite_backend_supported() {
  pacs_database_adapter db(":memory:");
  auto result = db.connect();
  return result.is_ok();
}
}  // namespace
#endif

TEST_CASE("Viewer state repository operations", "[web][viewer_state][database]") {
#ifdef PACS_WITH_DATABASE_SYSTEM
  // Check if SQLite backend is supported
  if (!is_sqlite_backend_supported()) {
    SUCCEED("Skipped: SQLite backend not yet supported by unified_database_system");
    return;
  }

  // Create a separate pacs_database_adapter for the repository
  // This follows the same pattern as annotation_endpoints_test
  auto db_adapter = std::make_shared<pacs_database_adapter>(":memory:");
  REQUIRE(db_adapter->connect().is_ok());

  // Run migrations to create required tables
  migration_runner runner;
  auto migration_result = runner.run_migrations(*db_adapter);
  REQUIRE(migration_result.is_ok());

  viewer_state_record_repository repo(db_adapter);
#else
  auto db_result = index_database::open(":memory:");
  REQUIRE(db_result.is_ok());
  auto &db = db_result.value();
  viewer_state_repository repo(db->native_handle());
#endif
#ifdef PACS_WITH_DATABASE_SYSTEM
  SUCCEED("Repository constructed with connected adapter");
#else
  REQUIRE(repo.is_valid());
#endif

  SECTION("save and find viewer state") {
    viewer_state_record state;
    state.state_id = "state-uuid-123";
    state.study_uid = "1.2.840.study";
    state.user_id = "user1";
    state.state_json = R"({"layout":{"rows":2,"cols":2},"viewports":[]})";
    state.created_at = std::chrono::system_clock::now();
    state.updated_at = state.created_at;

 #ifdef PACS_WITH_DATABASE_SYSTEM
    auto save_result = repo.save(state);
 #else
    auto save_result = repo.save_state(state);
 #endif
    REQUIRE(save_result.is_ok());

 #ifdef PACS_WITH_DATABASE_SYSTEM
    auto found = repo.find_by_id("state-uuid-123");
    REQUIRE(found.is_ok());
    REQUIRE(found.value().state_id == "state-uuid-123");
    REQUIRE(found.value().study_uid == "1.2.840.study");
    REQUIRE(found.value().user_id == "user1");
    REQUIRE(found.value().state_json ==
            R"({"layout":{"rows":2,"cols":2},"viewports":[]})");
 #else
    auto found = repo.find_state_by_id("state-uuid-123");
    REQUIRE(found.has_value());
    REQUIRE(found->state_id == "state-uuid-123");
    REQUIRE(found->study_uid == "1.2.840.study");
    REQUIRE(found->user_id == "user1");
    REQUIRE(found->state_json == R"({"layout":{"rows":2,"cols":2},"viewports":[]})");
 #endif
  }

  SECTION("find states by study") {
    viewer_state_record state1;
    state1.state_id = "state-1";
    state1.study_uid = "1.2.840.study";
    state1.user_id = "user1";
    state1.state_json = "{}";
    state1.created_at = std::chrono::system_clock::now();
    state1.updated_at = state1.created_at;
 #ifdef PACS_WITH_DATABASE_SYSTEM
    (void)repo.save(state1);
 #else
    (void)repo.save_state(state1);
 #endif

    viewer_state_record state2;
    state2.state_id = "state-2";
    state2.study_uid = "1.2.840.study";
    state2.user_id = "user2";
    state2.state_json = "{}";
    state2.created_at = std::chrono::system_clock::now();
    state2.updated_at = state2.created_at;
 #ifdef PACS_WITH_DATABASE_SYSTEM
    (void)repo.save(state2);
 #else
    (void)repo.save_state(state2);
 #endif

    viewer_state_record state3;
    state3.state_id = "state-3";
    state3.study_uid = "1.2.840.other_study";
    state3.user_id = "user1";
    state3.state_json = "{}";
    state3.created_at = std::chrono::system_clock::now();
    state3.updated_at = state3.created_at;
 #ifdef PACS_WITH_DATABASE_SYSTEM
    (void)repo.save(state3);
 #else
    (void)repo.save_state(state3);
 #endif

 #ifdef PACS_WITH_DATABASE_SYSTEM
    auto states = repo.find_by_study("1.2.840.study");
    REQUIRE(states.is_ok());
    REQUIRE(states.value().size() == 2);
 #else
    auto states = repo.find_states_by_study("1.2.840.study");
    REQUIRE(states.size() == 2);
 #endif
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
 #ifdef PACS_WITH_DATABASE_SYSTEM
      (void)repo.save(state);
 #else
      (void)repo.save_state(state);
 #endif
    }

    viewer_state_query query;
    query.study_uid = "1.2.840.study";
    query.limit = 5;
    query.offset = 0;

 #ifdef PACS_WITH_DATABASE_SYSTEM
    auto page1 = repo.search(query);
    REQUIRE(page1.is_ok());
    REQUIRE(page1.value().size() == 5);
 #else
    auto page1 = repo.search_states(query);
    REQUIRE(page1.size() == 5);
 #endif

    query.offset = 5;
 #ifdef PACS_WITH_DATABASE_SYSTEM
    auto page2 = repo.search(query);
    REQUIRE(page2.is_ok());
    REQUIRE(page2.value().size() == 5);
 #else
    auto page2 = repo.search_states(query);
    REQUIRE(page2.size() == 5);
 #endif
  }

  SECTION("delete viewer state") {
    viewer_state_record state;
    state.state_id = "delete-test";
    state.study_uid = "1.2.840.study";
    state.state_json = "{}";
    state.created_at = std::chrono::system_clock::now();
    state.updated_at = state.created_at;
 #ifdef PACS_WITH_DATABASE_SYSTEM
    (void)repo.save(state);
 #else
    (void)repo.save_state(state);
 #endif

 #ifdef PACS_WITH_DATABASE_SYSTEM
    auto found = repo.find_by_id("delete-test");
    REQUIRE(found.is_ok());
 #else
    auto found = repo.find_state_by_id("delete-test");
    REQUIRE(found.has_value());
 #endif

 #ifdef PACS_WITH_DATABASE_SYSTEM
    auto remove_result = repo.remove("delete-test");
 #else
    auto remove_result = repo.remove_state("delete-test");
 #endif
    REQUIRE(remove_result.is_ok());

 #ifdef PACS_WITH_DATABASE_SYSTEM
    auto not_found = repo.find_by_id("delete-test");
    REQUIRE(not_found.is_err());
 #else
    auto not_found = repo.find_state_by_id("delete-test");
    REQUIRE_FALSE(not_found.has_value());
 #endif
  }

  SECTION("count viewer states") {
 #ifdef PACS_WITH_DATABASE_SYSTEM
    auto initial_count = repo.count();
    REQUIRE(initial_count.is_ok());
    REQUIRE(initial_count.value() == 0);
 #else
    REQUIRE(repo.count_states() == 0);
 #endif

    viewer_state_record state;
    state.state_id = "count-test";
    state.study_uid = "1.2.840.study";
    state.state_json = "{}";
    state.created_at = std::chrono::system_clock::now();
    state.updated_at = state.created_at;
 #ifdef PACS_WITH_DATABASE_SYSTEM
    (void)repo.save(state);
    auto count = repo.count();
    REQUIRE(count.is_ok());
    REQUIRE(count.value() == 1);
 #else
    (void)repo.save_state(state);
    REQUIRE(repo.count_states() == 1);
 #endif
  }
}

TEST_CASE("Recent studies repository operations", "[web][viewer_state][database]") {
#ifdef PACS_WITH_DATABASE_SYSTEM
  // Check if SQLite backend is supported
  if (!is_sqlite_backend_supported()) {
    SUCCEED("Skipped: SQLite backend not yet supported by unified_database_system");
    return;
  }

  // Create a separate pacs_database_adapter for the repository
  // This follows the same pattern as annotation_endpoints_test
  auto db_adapter = std::make_shared<pacs_database_adapter>(":memory:");
  REQUIRE(db_adapter->connect().is_ok());

  // Run migrations to create required tables
  migration_runner runner;
  auto migration_result = runner.run_migrations(*db_adapter);
  REQUIRE(migration_result.is_ok());

  recent_study_repository repo(db_adapter);
#else
  auto db_result = index_database::open(":memory:");
  REQUIRE(db_result.is_ok());
  auto &db = db_result.value();
  viewer_state_repository repo(db->native_handle());
#endif
  SECTION("record study access") {
 #ifdef PACS_WITH_DATABASE_SYSTEM
    auto result = repo.record_access("user1", "1.2.840.study1");
 #else
    auto result = repo.record_study_access("user1", "1.2.840.study1");
 #endif
    REQUIRE(result.is_ok());

 #ifdef PACS_WITH_DATABASE_SYSTEM
    auto recent = repo.find_by_user("user1", 10);
    REQUIRE(recent.is_ok());
    REQUIRE(recent.value().size() == 1);
    REQUIRE(recent.value()[0].study_uid == "1.2.840.study1");
 #else
    auto recent = repo.get_recent_studies("user1", 10);
    REQUIRE(recent.size() == 1);
    REQUIRE(recent[0].study_uid == "1.2.840.study1");
 #endif
  }

  SECTION("multiple study accesses") {
 #ifdef PACS_WITH_DATABASE_SYSTEM
    (void)repo.record_access("user1", "1.2.840.study1");
    (void)repo.record_access("user1", "1.2.840.study2");
    (void)repo.record_access("user1", "1.2.840.study3");
 #else
    (void)repo.record_study_access("user1", "1.2.840.study1");
    (void)repo.record_study_access("user1", "1.2.840.study2");
    (void)repo.record_study_access("user1", "1.2.840.study3");
 #endif

 #ifdef PACS_WITH_DATABASE_SYSTEM
    auto recent = repo.find_by_user("user1", 10);
    REQUIRE(recent.is_ok());
    REQUIRE(recent.value().size() == 3);
 #else
    auto recent = repo.get_recent_studies("user1", 10);
    REQUIRE(recent.size() == 3);
 #endif
  }

  SECTION("recent studies with limit") {
    for (int i = 1; i <= 25; ++i) {
 #ifdef PACS_WITH_DATABASE_SYSTEM
      (void)repo.record_access("user1", "1.2.840.study" + std::to_string(i));
 #else
      (void)repo.record_study_access("user1",
                                     "1.2.840.study" + std::to_string(i));
 #endif
    }

 #ifdef PACS_WITH_DATABASE_SYSTEM
    auto recent_20 = repo.find_by_user("user1", 20);
    REQUIRE(recent_20.is_ok());
    REQUIRE(recent_20.value().size() == 20);

    auto recent_10 = repo.find_by_user("user1", 10);
    REQUIRE(recent_10.is_ok());
    REQUIRE(recent_10.value().size() == 10);
 #else
    auto recent_20 = repo.get_recent_studies("user1", 20);
    REQUIRE(recent_20.size() == 20);

    auto recent_10 = repo.get_recent_studies("user1", 10);
    REQUIRE(recent_10.size() == 10);
 #endif
  }

  SECTION("recent studies ordered by access time") {
    // Small sleeps ensure distinct timestamps across all platforms
 #ifdef PACS_WITH_DATABASE_SYSTEM
    (void)repo.record_access("user1", "1.2.840.study1");
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    (void)repo.record_access("user1", "1.2.840.study2");
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    (void)repo.record_access("user1", "1.2.840.study3");
 #else
    (void)repo.record_study_access("user1", "1.2.840.study1");
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    (void)repo.record_study_access("user1", "1.2.840.study2");
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    (void)repo.record_study_access("user1", "1.2.840.study3");
 #endif

 #ifdef PACS_WITH_DATABASE_SYSTEM
    auto recent = repo.find_by_user("user1", 10);
    REQUIRE(recent.is_ok());
    REQUIRE(recent.value().size() == 3);
    // Most recent first
    REQUIRE(recent.value()[0].study_uid == "1.2.840.study3");
    REQUIRE(recent.value()[1].study_uid == "1.2.840.study2");
    REQUIRE(recent.value()[2].study_uid == "1.2.840.study1");
 #else
    auto recent = repo.get_recent_studies("user1", 10);
    REQUIRE(recent.size() == 3);
    // Most recent first
    REQUIRE(recent[0].study_uid == "1.2.840.study3");
    REQUIRE(recent[1].study_uid == "1.2.840.study2");
    REQUIRE(recent[2].study_uid == "1.2.840.study1");
 #endif
  }

  SECTION("re-access updates timestamp") {
 #ifdef PACS_WITH_DATABASE_SYSTEM
    (void)repo.record_access("user1", "1.2.840.study1");
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    (void)repo.record_access("user1", "1.2.840.study2");
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    (void)repo.record_access("user1", "1.2.840.study1"); // Re-access
 #else
    (void)repo.record_study_access("user1", "1.2.840.study1");
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    (void)repo.record_study_access("user1", "1.2.840.study2");
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    (void)repo.record_study_access("user1", "1.2.840.study1"); // Re-access
 #endif

 #ifdef PACS_WITH_DATABASE_SYSTEM
    auto recent = repo.find_by_user("user1", 10);
    REQUIRE(recent.is_ok());
    REQUIRE(recent.value().size() == 2);
    // study1 should be most recent now
    REQUIRE(recent.value()[0].study_uid == "1.2.840.study1");
    REQUIRE(recent.value()[1].study_uid == "1.2.840.study2");
 #else
    auto recent = repo.get_recent_studies("user1", 10);
    REQUIRE(recent.size() == 2);
    // study1 should be most recent now
    REQUIRE(recent[0].study_uid == "1.2.840.study1");
    REQUIRE(recent[1].study_uid == "1.2.840.study2");
 #endif
  }

  SECTION("clear recent studies") {
 #ifdef PACS_WITH_DATABASE_SYSTEM
    (void)repo.record_access("user1", "1.2.840.study1");
    (void)repo.record_access("user1", "1.2.840.study2");
    (void)repo.record_access("user2", "1.2.840.study3");

    REQUIRE(repo.count_for_user("user1").value() == 2);
    REQUIRE(repo.count_for_user("user2").value() == 1);

    auto clear_result = repo.clear_for_user("user1");
    REQUIRE(clear_result.is_ok());

    REQUIRE(repo.count_for_user("user1").value() == 0);
    REQUIRE(repo.count_for_user("user2").value() == 1); // Unaffected
 #else
    (void)repo.record_study_access("user1", "1.2.840.study1");
    (void)repo.record_study_access("user1", "1.2.840.study2");
    (void)repo.record_study_access("user2", "1.2.840.study3");

    REQUIRE(repo.count_recent_studies("user1") == 2);
    REQUIRE(repo.count_recent_studies("user2") == 1);

    auto clear_result = repo.clear_recent_studies("user1");
    REQUIRE(clear_result.is_ok());

    REQUIRE(repo.count_recent_studies("user1") == 0);
    REQUIRE(repo.count_recent_studies("user2") == 1); // Unaffected
 #endif
  }

  SECTION("count recent studies") {
 #ifdef PACS_WITH_DATABASE_SYSTEM
    REQUIRE(repo.count_for_user("user1").value() == 0);

    (void)repo.record_access("user1", "1.2.840.study1");
    (void)repo.record_access("user1", "1.2.840.study2");

    REQUIRE(repo.count_for_user("user1").value() == 2);
 #else
    REQUIRE(repo.count_recent_studies("user1") == 0);

    (void)repo.record_study_access("user1", "1.2.840.study1");
    (void)repo.record_study_access("user1", "1.2.840.study2");

    REQUIRE(repo.count_recent_studies("user1") == 2);
 #endif
  }

  SECTION("user isolation") {
 #ifdef PACS_WITH_DATABASE_SYSTEM
    (void)repo.record_access("user1", "1.2.840.study1");
    (void)repo.record_access("user2", "1.2.840.study2");

    auto user1_recent = repo.find_by_user("user1", 10);
    REQUIRE(user1_recent.is_ok());
    REQUIRE(user1_recent.value().size() == 1);
    REQUIRE(user1_recent.value()[0].study_uid == "1.2.840.study1");

    auto user2_recent = repo.find_by_user("user2", 10);
    REQUIRE(user2_recent.is_ok());
    REQUIRE(user2_recent.value().size() == 1);
    REQUIRE(user2_recent.value()[0].study_uid == "1.2.840.study2");
 #else
    (void)repo.record_study_access("user1", "1.2.840.study1");
    (void)repo.record_study_access("user2", "1.2.840.study2");

    auto user1_recent = repo.get_recent_studies("user1", 10);
    REQUIRE(user1_recent.size() == 1);
    REQUIRE(user1_recent[0].study_uid == "1.2.840.study1");

    auto user2_recent = repo.get_recent_studies("user2", 10);
    REQUIRE(user2_recent.size() == 1);
    REQUIRE(user2_recent[0].study_uid == "1.2.840.study2");
 #endif
  }
}
