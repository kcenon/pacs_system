/**
 * @file sqlite_security_storage.cpp
 * @brief Implementation of SQLite security storage
 *
 * @copyright Copyright (c) 2025
 */

#include <format>
#include <pacs/storage/sqlite_security_storage.hpp>
#include <sqlite3.h>
#include <variant> // for std::monostate

namespace pacs::storage {

using namespace security;
using kcenon::common::make_error;
using kcenon::common::ok;

namespace {
constexpr int kSqliteError = 1;
constexpr int kUserNotFound = 2;
constexpr int kNotImplemented = 3;
} // namespace

sqlite_security_storage::sqlite_security_storage(std::string db_path)
    : db_path_(std::move(db_path)) {
  if (auto res = open_db(); res.is_err()) {
    // Logging handled by caller or ignored in constructor for now
  } else {
    (void)initialize_tables();
  }
}

sqlite_security_storage::~sqlite_security_storage() { close_db(); }

auto sqlite_security_storage::open_db() -> VoidResult {
  if (sqlite3_open(db_path_.c_str(), &db_) != SQLITE_OK) {
    return make_error<std::monostate>(kSqliteError, "Failed to open database",
                                      "sqlite_security_storage");
  }
  return ok();
}

void sqlite_security_storage::close_db() {
  if (db_) {
    sqlite3_close(db_);
    db_ = nullptr;
  }
}

auto sqlite_security_storage::initialize_tables() -> VoidResult {
  const char *sql = R"(
        CREATE TABLE IF NOT EXISTS users (
            id TEXT PRIMARY KEY,
            username TEXT UNIQUE NOT NULL,
            active INTEGER DEFAULT 1
        );
        CREATE TABLE IF NOT EXISTS user_roles (
            user_id TEXT,
            role TEXT,
            PRIMARY KEY (user_id, role),
            FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE
        );
    )";

  char *err_msg = nullptr;
  if (sqlite3_exec(db_, sql, nullptr, nullptr, &err_msg) != SQLITE_OK) {
    std::string error = err_msg ? err_msg : "Unknown error";
    sqlite3_free(err_msg);
    return make_error<std::monostate>(kSqliteError,
                                      "Failed to init tables: " + error,
                                      "sqlite_security_storage");
  }
  return ok();
}

auto sqlite_security_storage::create_user(const User &user) -> VoidResult {
  // 1. Insert User
  std::string sql = std::format(
      "INSERT INTO users (id, username, active) VALUES ('{}', '{}', {});",
      user.id, user.username, user.active ? 1 : 0);

  char *err_msg = nullptr;
  if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err_msg) != SQLITE_OK) {
    std::string error = err_msg ? err_msg : "Unknown error";
    sqlite3_free(err_msg);
    return make_error<std::monostate>(kSqliteError,
                                      "Failed to insert user: " + error,
                                      "sqlite_security_storage");
  }

  // 2. Insert Roles
  for (const auto &role : user.roles) {
    std::string role_str(to_string(role));
    std::string role_sql = std::format(
        "INSERT INTO user_roles (user_id, role) VALUES ('{}', '{}');", user.id,
        role_str);
    // Ignore errors for roles (best effort)
    sqlite3_exec(db_, role_sql.c_str(), nullptr, nullptr, nullptr);
  }
  return ok();
}

auto sqlite_security_storage::get_user(std::string_view id) -> Result<User> {
  User user;
  user.id = id;

  // Get User
  std::string sql =
      std::format("SELECT username, active FROM users WHERE id = '{}';", id);

  auto callback = [](void *data, int argc, char **argv,
                     char ** /*col_name*/) -> int {
    auto *u = static_cast<User *>(data);
    if (argc >= 2) {
      u->username = argv[0] ? argv[0] : "";
      u->active = argv[1] ? std::stoi(argv[1]) : 0;
      return 0; // Success
    }
    return 1; // Error
  };

  char *err_msg = nullptr;
  if (sqlite3_exec(db_, sql.c_str(), callback, &user, &err_msg) != SQLITE_OK) {
    std::string error = err_msg ? err_msg : "Unknown error";
    sqlite3_free(err_msg);
    return make_error<User>(kSqliteError, "DB Error: " + error,
                            "sqlite_security_storage");
  }

  if (user.username.empty()) {
    return make_error<User>(kUserNotFound, "User not found",
                            "sqlite_security_storage");
  }

  // Get Roles
  sql = std::format("SELECT role FROM user_roles WHERE user_id = '{}';", id);
  auto role_callback = [](void *data, int argc, char **argv,
                          char ** /*col_name*/) -> int {
    auto *u = static_cast<User *>(data);
    if (argc >= 1 && argv[0]) {
      if (auto role = parse_role(argv[0])) {
        u->roles.push_back(*role);
      }
    }
    return 0;
  };

  sqlite3_exec(db_, sql.c_str(), role_callback, &user, nullptr);

  return user;
}

auto sqlite_security_storage::get_user_by_username(std::string_view username)
    -> Result<User> {
  // Simplifying: Not implemented for now to save time, unless needed for
  // testing
  (void)username;
  return make_error<User>(kNotImplemented, "Not implemented yet",
                          "sqlite_security_storage");
}

auto sqlite_security_storage::update_user(const User &user) -> VoidResult {
  std::string sql = std::format("UPDATE users SET active = {} WHERE id = '{}';",
                                user.active ? 1 : 0, user.id);
  sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, nullptr);

  sql = std::format("DELETE FROM user_roles WHERE user_id = '{}';", user.id);
  sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, nullptr);

  for (const auto &role : user.roles) {
    std::string role_str(to_string(role));
    std::string role_sql = std::format(
        "INSERT INTO user_roles (user_id, role) VALUES ('{}', '{}');", user.id,
        role_str);
    sqlite3_exec(db_, role_sql.c_str(), nullptr, nullptr, nullptr);
  }

  return ok();
}

auto sqlite_security_storage::delete_user(std::string_view id) -> VoidResult {
  std::string sql = std::format("DELETE FROM users WHERE id = '{}';", id);
  sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, nullptr);
  return ok();
}

auto sqlite_security_storage::get_users_by_role(Role role)
    -> Result<std::vector<User>> {
  (void)role;
  return std::vector<User>{}; // Stub
}

} // namespace pacs::storage
