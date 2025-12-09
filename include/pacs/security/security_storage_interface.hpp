/**
 * @file security_storage_interface.hpp
 * @brief Storage interface for RBAC persistence
 *
 * @copyright Copyright (c) 2025
 */

#pragma once

#include "user.hpp"
#include <kcenon/common/patterns/result.h>
#include <string_view>
#include <vector>

namespace pacs::security {

/**
 * @brief Abstract interface for persisting security data (Users, Roles)
 */
class security_storage_interface {
public:
  template <typename T> using Result = kcenon::common::Result<T>;
  using VoidResult = kcenon::common::VoidResult;

  virtual ~security_storage_interface() = default;

  // User Management
  [[nodiscard]] virtual auto create_user(const User &user) -> VoidResult = 0;
  [[nodiscard]] virtual auto get_user(std::string_view id) -> Result<User> = 0;
  [[nodiscard]] virtual auto get_user_by_username(std::string_view username)
      -> Result<User> = 0;
  [[nodiscard]] virtual auto update_user(const User &user) -> VoidResult = 0;
  [[nodiscard]] virtual auto delete_user(std::string_view id) -> VoidResult = 0;

  // Role Assignment (usually part of user update, but explicit methods can
  // help)
  [[nodiscard]] virtual auto get_users_by_role(Role role)
      -> Result<std::vector<User>> = 0;

protected:
  security_storage_interface() = default;
  security_storage_interface(const security_storage_interface &) = delete;
  security_storage_interface &
  operator=(const security_storage_interface &) = delete;
  security_storage_interface(security_storage_interface &&) = default;
  security_storage_interface &
  operator=(security_storage_interface &&) = default;
};

} // namespace pacs::security
