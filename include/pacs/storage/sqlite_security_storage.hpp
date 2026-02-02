/**
 * @file sqlite_security_storage.hpp
 * @brief SQLite implementation of security storage with SQL injection protection
 *
 * This implementation uses database_system's query builder to prevent SQL
 * injection vulnerabilities. All user inputs are properly escaped through
 * parameterized queries.
 *
 * @see Issue #609 - Phase 3-2: Cursor & Security Storage Migration
 * @copyright Copyright (c) 2025
 */

#pragma once

// Check for database_system availability
// This header requires database_system for database_context and database_manager
#ifndef PACS_WITH_DATABASE_SYSTEM
// Forward declare empty class when database_system is not available
namespace pacs::storage {
class sqlite_security_storage;
}  // namespace pacs::storage
#else  // PACS_WITH_DATABASE_SYSTEM

#include <pacs/security/security_storage_interface.hpp>

#include <database/core/database_context.h>
#include <database/database_manager.h>

#include <memory>
#include <string>
#include <vector>

namespace pacs::storage {

/**
 * @brief SQLite backend for security storage with SQL injection protection
 *
 * Uses database_system's query builder for parameterized queries.
 * All user inputs are properly escaped to prevent SQL injection.
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
  std::shared_ptr<database::database_context> db_context_;
  std::shared_ptr<database::database_manager> db_manager_;

  [[nodiscard]] auto initialize_database() -> VoidResult;
};

}  // namespace pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
