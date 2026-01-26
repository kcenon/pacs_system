/**
 * @file sqlite_security_storage.cpp
 * @brief Implementation of SQLite security storage with SQL injection protection
 *
 * This file implements secure database operations for user management using
 * database_system's query builder for parameterized queries to prevent SQL
 * injection vulnerabilities.
 *
 * @copyright Copyright (c) 2025
 */

#include <pacs/storage/sqlite_security_storage.hpp>
#include <database/query_builder.h>
#include <variant> // for std::monostate

namespace pacs::storage {

using namespace security;
using kcenon::common::make_error;
using kcenon::common::ok;

namespace {
constexpr int kSqliteError = 1;
constexpr int kUserNotFound = 2;
[[maybe_unused]] constexpr int kDatabaseNotConnected = 4;
} // namespace

// ============================================================================
// Implementation using database_system (SQL injection safe via query builder)
// ============================================================================

sqlite_security_storage::sqlite_security_storage(std::string db_path)
    : db_path_(std::move(db_path)) {
  if (auto res = initialize_with_database_system(); res.is_err()) {
    // Log error - initialization failed
  }
}

sqlite_security_storage::~sqlite_security_storage() {
  if (db_manager_) {
    (void)db_manager_->disconnect_result();
  }
}

auto sqlite_security_storage::initialize_with_database_system() -> VoidResult {
  db_context_ = std::make_shared<database::database_context>();
  db_manager_ = std::make_shared<database::database_manager>(db_context_);

  if (!db_manager_->set_mode(database::database_types::sqlite)) {
    return make_error<std::monostate>(kSqliteError, "Failed to set SQLite mode",
                                      "sqlite_security_storage");
  }

  auto connect_result = db_manager_->connect_result(db_path_);
  if (connect_result.is_err()) {
    return make_error<std::monostate>(
        kSqliteError, "Failed to connect: " + connect_result.error().message,
        "sqlite_security_storage");
  }

  // Create tables using raw SQL (DDL is safe - no user input)
  const char *create_tables_sql = R"(
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

  auto exec_result = db_manager_->execute_query_result(create_tables_sql);
  if (exec_result.is_err()) {
    return make_error<std::monostate>(
        kSqliteError, "Failed to create tables: " + exec_result.error().message,
        "sqlite_security_storage");
  }

  return ok();
}

auto sqlite_security_storage::create_user(const User &user) -> VoidResult {
  if (!db_manager_) {
    return make_error<std::monostate>(kDatabaseNotConnected,
                                      "Database not connected",
                                      "sqlite_security_storage");
  }

  // Use query_builder for SQL injection protection
  database::query_builder builder(database::database_types::sqlite);
  auto insert_sql = builder
                        .insert_into("users")
                        .values({{"id", user.id},
                                 {"username", user.username},
                                 {"active", user.active ? 1 : 0}})
                        .build();

  auto result = db_manager_->insert_query_result(insert_sql);
  if (result.is_err()) {
    return make_error<std::monostate>(
        kSqliteError, "Failed to insert user: " + result.error().message,
        "sqlite_security_storage");
  }

  // Insert roles using query_builder
  for (const auto &role : user.roles) {
    database::query_builder role_builder(database::database_types::sqlite);
    auto role_sql = role_builder
                        .insert_into("user_roles")
                        .values({{"user_id", user.id},
                                 {"role", std::string(to_string(role))}})
                        .build();

    auto role_result = db_manager_->insert_query_result(role_sql);
    if (role_result.is_err()) {
      // Log warning but continue (best effort for roles)
    }
  }

  return ok();
}

auto sqlite_security_storage::get_user(std::string_view id) -> Result<User> {
  if (!db_manager_) {
    return make_error<User>(kDatabaseNotConnected, "Database not connected",
                            "sqlite_security_storage");
  }

  User user;
  user.id = std::string(id);

  // Use query_builder for SQL injection protection
  database::query_builder builder(database::database_types::sqlite);
  auto select_sql =
      builder.select(std::vector<std::string>{"username", "active"})
          .from("users")
          .where("id", "=", std::string(id))
          .build();

  auto result = db_manager_->select_query_result(select_sql);
  if (result.is_err()) {
    return make_error<User>(kSqliteError,
                            "DB Error: " + result.error().message,
                            "sqlite_security_storage");
  }

  const auto &rows = result.value();
  if (rows.empty()) {
    return make_error<User>(kUserNotFound, "User not found",
                            "sqlite_security_storage");
  }

  // Extract user data from first row
  const auto &row = rows[0];
  if (auto it = row.find("username"); it != row.end()) {
    if (std::holds_alternative<std::string>(it->second)) {
      user.username = std::get<std::string>(it->second);
    }
  }
  if (auto it = row.find("active"); it != row.end()) {
    if (std::holds_alternative<int64_t>(it->second)) {
      user.active = std::get<int64_t>(it->second) != 0;
    }
  }

  // Get roles using sql_query_builder
  database::query_builder role_builder(database::database_types::sqlite);
  auto role_sql = role_builder.select(std::vector<std::string>{"role"})
                      .from("user_roles")
                      .where("user_id", "=", std::string(id))
                      .build();

  auto role_result = db_manager_->select_query_result(role_sql);
  if (role_result.is_ok()) {
    for (const auto &role_row : role_result.value()) {
      if (auto it = role_row.find("role"); it != role_row.end()) {
        if (std::holds_alternative<std::string>(it->second)) {
          if (auto role = parse_role(std::get<std::string>(it->second))) {
            user.roles.push_back(*role);
          }
        }
      }
    }
  }

  return user;
}

auto sqlite_security_storage::get_user_by_username(std::string_view username)
    -> Result<User> {
  if (!db_manager_) {
    return make_error<User>(kDatabaseNotConnected, "Database not connected",
                            "sqlite_security_storage");
  }

  // Use sql_query_builder for SQL injection protection
  database::query_builder builder(database::database_types::sqlite);
  auto select_sql =
      builder.select(std::vector<std::string>{"id", "username", "active"})
          .from("users")
          .where("username", "=", std::string(username))
          .build();

  auto result = db_manager_->select_query_result(select_sql);
  if (result.is_err()) {
    return make_error<User>(kSqliteError,
                            "DB Error: " + result.error().message,
                            "sqlite_security_storage");
  }

  const auto &rows = result.value();
  if (rows.empty()) {
    return make_error<User>(kUserNotFound, "User not found",
                            "sqlite_security_storage");
  }

  User user;
  const auto &row = rows[0];

  if (auto it = row.find("id"); it != row.end()) {
    if (std::holds_alternative<std::string>(it->second)) {
      user.id = std::get<std::string>(it->second);
    }
  }
  if (auto it = row.find("username"); it != row.end()) {
    if (std::holds_alternative<std::string>(it->second)) {
      user.username = std::get<std::string>(it->second);
    }
  }
  if (auto it = row.find("active"); it != row.end()) {
    if (std::holds_alternative<int64_t>(it->second)) {
      user.active = std::get<int64_t>(it->second) != 0;
    }
  }

  // Get roles using sql_query_builder
  database::query_builder role_builder(database::database_types::sqlite);
  auto role_sql = role_builder.select(std::vector<std::string>{"role"})
                      .from("user_roles")
                      .where("user_id", "=", user.id)
                      .build();

  auto role_result = db_manager_->select_query_result(role_sql);
  if (role_result.is_ok()) {
    for (const auto &role_row : role_result.value()) {
      if (auto it = role_row.find("role"); it != role_row.end()) {
        if (std::holds_alternative<std::string>(it->second)) {
          if (auto role = parse_role(std::get<std::string>(it->second))) {
            user.roles.push_back(*role);
          }
        }
      }
    }
  }

  return user;
}

auto sqlite_security_storage::update_user(const User &user) -> VoidResult {
  if (!db_manager_) {
    return make_error<std::monostate>(kDatabaseNotConnected,
                                      "Database not connected",
                                      "sqlite_security_storage");
  }

  // Use transaction for atomicity
  auto tx_result = db_manager_->begin_transaction();
  if (tx_result.is_err()) {
    return make_error<std::monostate>(
        kSqliteError,
        "Failed to begin transaction: " + tx_result.error().message,
        "sqlite_security_storage");
  }

  // Update user using sql_query_builder
  database::query_builder update_builder(database::database_types::sqlite);
  auto update_sql = update_builder.update("users")
                        .set({{"active", user.active ? 1 : 0}})
                        .where("id", "=", user.id)
                        .build();

  auto update_result = db_manager_->update_query_result(update_sql);
  if (update_result.is_err()) {
    (void)db_manager_->rollback_transaction();
    return make_error<std::monostate>(
        kSqliteError, "Failed to update user: " + update_result.error().message,
        "sqlite_security_storage");
  }

  // Delete existing roles using sql_query_builder
  database::query_builder delete_builder(database::database_types::sqlite);
  auto delete_sql = delete_builder.delete_from("user_roles")
                        .where("user_id", "=", user.id)
                        .build();

  auto delete_result = db_manager_->delete_query_result(delete_sql);
  if (delete_result.is_err()) {
    (void)db_manager_->rollback_transaction();
    return make_error<std::monostate>(
        kSqliteError,
        "Failed to delete roles: " + delete_result.error().message,
        "sqlite_security_storage");
  }

  // Insert new roles using sql_query_builder
  for (const auto &role : user.roles) {
    database::query_builder role_builder(database::database_types::sqlite);
    auto role_sql =
        role_builder.insert_into("user_roles")
            .values(
                {{"user_id", user.id}, {"role", std::string(to_string(role))}})
            .build();

    auto role_result = db_manager_->insert_query_result(role_sql);
    if (role_result.is_err()) {
      (void)db_manager_->rollback_transaction();
      return make_error<std::monostate>(
          kSqliteError,
          "Failed to insert role: " + role_result.error().message,
          "sqlite_security_storage");
    }
  }

  auto commit_result = db_manager_->commit_transaction();
  if (commit_result.is_err()) {
    (void)db_manager_->rollback_transaction();
    return make_error<std::monostate>(
        kSqliteError,
        "Failed to commit transaction: " + commit_result.error().message,
        "sqlite_security_storage");
  }

  return ok();
}

auto sqlite_security_storage::delete_user(std::string_view id) -> VoidResult {
  if (!db_manager_) {
    return make_error<std::monostate>(kDatabaseNotConnected,
                                      "Database not connected",
                                      "sqlite_security_storage");
  }

  // Use sql_query_builder for SQL injection protection
  database::query_builder builder(database::database_types::sqlite);
  auto delete_sql = builder.delete_from("users")
                        .where("id", "=", std::string(id))
                        .build();

  auto result = db_manager_->delete_query_result(delete_sql);
  if (result.is_err()) {
    return make_error<std::monostate>(
        kSqliteError, "Failed to delete user: " + result.error().message,
        "sqlite_security_storage");
  }

  return ok();
}

auto sqlite_security_storage::get_users_by_role(Role role)
    -> Result<std::vector<User>> {
  if (!db_manager_) {
    return make_error<std::vector<User>>(kDatabaseNotConnected,
                                         "Database not connected",
                                         "sqlite_security_storage");
  }

  // Use sql_query_builder for SQL injection protection
  database::query_builder builder(database::database_types::sqlite);
  auto select_sql =
      builder
          .select(std::vector<std::string>{"u.id", "u.username", "u.active"})
          .from("users u")
          .join("user_roles ur", "u.id = ur.user_id")
          .where("ur.role", "=", std::string(to_string(role)))
          .build();

  auto result = db_manager_->select_query_result(select_sql);
  if (result.is_err()) {
    return make_error<std::vector<User>>(
        kSqliteError, "DB Error: " + result.error().message,
        "sqlite_security_storage");
  }

  std::vector<User> users;
  for (const auto &row : result.value()) {
    User user;
    if (auto it = row.find("id"); it != row.end()) {
      if (std::holds_alternative<std::string>(it->second)) {
        user.id = std::get<std::string>(it->second);
      }
    }
    if (auto it = row.find("username"); it != row.end()) {
      if (std::holds_alternative<std::string>(it->second)) {
        user.username = std::get<std::string>(it->second);
      }
    }
    if (auto it = row.find("active"); it != row.end()) {
      if (std::holds_alternative<int64_t>(it->second)) {
        user.active = std::get<int64_t>(it->second) != 0;
      }
    }

    // Fetch all roles for this user using sql_query_builder
    database::query_builder role_builder(database::database_types::sqlite);
    auto role_sql =
        role_builder.select(std::vector<std::string>{"role"})
            .from("user_roles")
            .where("user_id", "=", user.id)
            .build();

    auto role_result = db_manager_->select_query_result(role_sql);
    if (role_result.is_ok()) {
      for (const auto &role_row : role_result.value()) {
        if (auto it = role_row.find("role"); it != role_row.end()) {
          if (std::holds_alternative<std::string>(it->second)) {
            if (auto r = parse_role(std::get<std::string>(it->second))) {
              user.roles.push_back(*r);
            }
          }
        }
      }
    }

    users.push_back(std::move(user));
  }

  return users;
}


} // namespace pacs::storage
