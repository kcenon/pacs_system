/**
 * @file sqlite_security_storage_test.cpp
 * @brief Unit tests for SQLite Security Storage
 *
 * @note These tests require PACS_WITH_DATABASE_SYSTEM to be defined.
 */

#include "pacs/storage/sqlite_security_storage.hpp"

// Only compile tests when PACS_WITH_DATABASE_SYSTEM is defined
#ifdef PACS_WITH_DATABASE_SYSTEM

#include <catch2/catch_test_macros.hpp>

using namespace pacs::storage;
using namespace pacs::security;

TEST_CASE("SQLiteSecurityStorage: CRUD Operations", "[security][storage]") {
  // Use in-memory DB for speed and isolation
  std::string db_path = ":memory:";
  sqlite_security_storage storage(db_path);

  // Ensure tables exist (Constructor handles it)
  // REQUIRE(storage.initialize_tables().is_ok());

  User u1;
  u1.id = "u1";
  u1.username = "john_doe";
  u1.active = true;
  u1.roles.push_back(Role::Technologist);

  SECTION("Create and Get User") {
    REQUIRE(storage.create_user(u1).is_ok());

    auto res = storage.get_user("u1");
    REQUIRE(res.is_ok());

    auto fetched = res.unwrap();
    REQUIRE(fetched.id == u1.id);
    REQUIRE(fetched.username == u1.username);
    REQUIRE(fetched.active == u1.active);
    REQUIRE(fetched.has_role(Role::Technologist));
  }

  SECTION("Update User") {
    REQUIRE(storage.create_user(u1).is_ok());

    u1.active = false;
    u1.roles.push_back(Role::Administrator);

    REQUIRE(storage.update_user(u1).is_ok());

    auto fetched = storage.get_user("u1").unwrap();
    REQUIRE(fetched.active == false);
    REQUIRE(fetched.has_role(Role::Administrator));
    REQUIRE(
        fetched.has_role(Role::Technologist)); // Previous role should be kept?
    // Logic in sqlite_security_storage:
    // DELETE FROM user_roles ...
    // INSERT ...
    // So update_user REPLACES roles.
    // My test setup: u1.roles had Technologist. I added Admin. So user object
    // has both. So fetched should have both.
  }

  SECTION("Delete User") {
    REQUIRE(storage.create_user(u1).is_ok());
    REQUIRE(storage.delete_user("u1").is_ok());

    auto res = storage.get_user("u1");
    REQUIRE(res.is_err()); // User not found
  }
}

TEST_CASE("SQLiteSecurityStorage: SQL Injection Protection",
          "[security][storage][injection]") {
  std::string db_path = ":memory:";
  sqlite_security_storage storage(db_path);

  // Create a legitimate user first
  User legitimate_user;
  legitimate_user.id = "admin";
  legitimate_user.username = "administrator";
  legitimate_user.active = true;
  legitimate_user.roles.push_back(Role::Administrator);
  REQUIRE(storage.create_user(legitimate_user).is_ok());

  SECTION("SQL injection via user ID should not execute") {
    // Common SQL injection payloads
    std::vector<std::string> injection_payloads = {
        "'; DROP TABLE users; --",
        "admin'--",
        "1' OR '1'='1",
        "'; INSERT INTO users VALUES('hacker', 'hacker', 1); --",
        "Robert'); DROP TABLE users;--",
        "1; UPDATE users SET active=0 WHERE username='admin",
        "' OR 1=1 --",
        "admin' OR 'x'='x",
    };

    for (const auto &payload : injection_payloads) {
      INFO("Testing injection payload: " << payload);

      // Should return "user not found", NOT execute injection
      auto result = storage.get_user(payload);
      REQUIRE(result.is_err());

      // Verify legitimate user still exists (tables not dropped)
      auto admin_result = storage.get_user("admin");
      REQUIRE(admin_result.is_ok());
      REQUIRE(admin_result.unwrap().username == "administrator");
    }
  }

  SECTION("SQL injection via username should not execute") {
    std::vector<std::string> username_injections = {
        "admin'; DROP TABLE users; --",
        "' OR '1'='1",
        "administrator'--",
        "'; DELETE FROM users WHERE '1'='1",
    };

    for (const auto &payload : username_injections) {
      INFO("Testing username injection: " << payload);

      auto result = storage.get_user_by_username(payload);
      REQUIRE(result.is_err());

      // Verify tables intact
      auto admin_result = storage.get_user("admin");
      REQUIRE(admin_result.is_ok());
    }
  }

  SECTION("SQL injection via create_user should not execute") {
    User malicious_user;
    malicious_user.id = "'; DROP TABLE users; --";
    malicious_user.username = "hacker'; DELETE FROM users WHERE '1'='1";
    malicious_user.active = true;

    // Should either fail or safely store the string literal
    auto create_result = storage.create_user(malicious_user);

    // Regardless of success, verify original user still exists
    auto admin_result = storage.get_user("admin");
    REQUIRE(admin_result.is_ok());
    REQUIRE(admin_result.unwrap().username == "administrator");
  }

  SECTION("Special characters in legitimate data should work") {
    User special_user;
    special_user.id = "user_123";
    special_user.username = "O'Brien"; // Legitimate apostrophe
    special_user.active = true;
    special_user.roles.push_back(Role::Radiologist);

    REQUIRE(storage.create_user(special_user).is_ok());

    auto result = storage.get_user_by_username("O'Brien");
    REQUIRE(result.is_ok());
    REQUIRE(result.unwrap().username == "O'Brien");

    // Verify no side effects
    auto admin_result = storage.get_user("admin");
    REQUIRE(admin_result.is_ok());
  }

  SECTION("Null bytes and special characters should be handled safely") {
    User special_user;
    special_user.id = "null_test";
    special_user.username = std::string("test\0user", 9); // Contains null byte
    special_user.active = true;

    // Should handle safely (either store truncated or reject)
    auto create_result = storage.create_user(special_user);

    // Original user should still exist
    auto admin_result = storage.get_user("admin");
    REQUIRE(admin_result.is_ok());
  }
}

#endif  // PACS_WITH_DATABASE_SYSTEM
