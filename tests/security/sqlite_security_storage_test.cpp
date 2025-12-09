/**
 * @file sqlite_security_storage_test.cpp
 * @brief Unit tests for SQLite Security Storage
 */

#include <catch2/catch_test_macros.hpp>

#include "pacs/storage/sqlite_security_storage.hpp"

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
