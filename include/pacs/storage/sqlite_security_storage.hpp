/**
 * @file sqlite_security_storage.hpp
 * @brief SQLite implementation of security storage with SQL injection protection
 *
 * This implementation uses database_system's query builder to prevent SQL
 * injection vulnerabilities. All user inputs are properly escaped through
 * parameterized queries.
 *
 * @copyright Copyright (c) 2025
 */

#pragma once

#include <pacs/security/security_storage_interface.hpp>
#include <string>
#include <vector>
#include <memory>

#ifdef PACS_WITH_DATABASE_SYSTEM
#include <database/database_manager.h>
#include <database/core/database_context.h>
#endif

// Forward declaration for SQLite3 (fallback when database_system unavailable)
struct sqlite3;

namespace pacs::storage {

/**
 * @brief SQLite backend for security storage with SQL injection protection
 *
 * Uses database_system's query builder for parameterized queries when available.
 * Falls back to direct SQLite with manual escaping when database_system is not
 * linked (compile-time conditional).
 */
class sqlite_security_storage : public security::security_storage_interface {
public:
  /**
   * @brief Construct with database path
   * @param db_path Path to SQLite database file
   */
  explicit sqlite_security_storage(std::string db_path);

  ~sqlite_security_storage() override;

  // User Management
  [[nodiscard]] auto create_user(const security::User &user)
      -> VoidResult override;
  [[nodiscard]] auto get_user(std::string_view id)
      -> Result<security::User> override;
  [[nodiscard]] auto get_user_by_username(std::string_view username)
      -> Result<security::User> override;
  [[nodiscard]] auto update_user(const security::User &user)
      -> VoidResult override;
  [[nodiscard]] auto delete_user(std::string_view id) -> VoidResult override;

  [[nodiscard]] auto get_users_by_role(security::Role role)
      -> Result<std::vector<security::User>> override;

private:
  std::string db_path_;

#ifdef PACS_WITH_DATABASE_SYSTEM
  std::shared_ptr<database::database_context> db_context_;
  std::shared_ptr<database::database_manager> db_manager_;

  [[nodiscard]] auto initialize_with_database_system() -> VoidResult;
#else
  sqlite3 *db_{nullptr};

  [[nodiscard]] auto open_db() -> VoidResult;
  void close_db();
  [[nodiscard]] auto initialize_tables() -> VoidResult;
  [[nodiscard]] static auto escape_string(std::string_view input) -> std::string;
#endif
};

} // namespace pacs::storage
