/**
 * @file sqlite_security_storage.hpp
 * @brief SQLite implementation of security storage
 *
 * @copyright Copyright (c) 2025
 */

#pragma once

#include <pacs/security/security_storage_interface.hpp>
#include <string>
#include <vector>

// Forward declaration for SQLite3
struct sqlite3;

namespace pacs::storage {

/**
 * @brief SQLite backend for security storage
 */
class sqlite_security_storage : public security::security_storage_interface {
public:
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
  sqlite3 *db_{nullptr};

  [[nodiscard]] auto open_db() -> VoidResult;
  void close_db();
  [[nodiscard]] auto initialize_tables() -> VoidResult;
};

} // namespace pacs::storage
