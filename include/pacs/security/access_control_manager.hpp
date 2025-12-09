/**
 * @file access_control_manager.hpp
 * @brief Core RBAC logic
 *
 * @copyright Copyright (c) 2025
 */

#pragma once

#include "permission.hpp"
#include "security_storage_interface.hpp"
#include "user.hpp"
#include <map>
#include <memory>
#include <vector>

namespace pacs::security {

/**
 * @brief Manages permissions and access checks
 */
class access_control_manager {
public:
  access_control_manager();

  // Permission Checks
  [[nodiscard]] bool check_permission(const User &user, ResourceType resource,
                                      std::uint32_t action_mask) const;
  [[nodiscard]] bool has_role(const User &user, Role role) const;

  // Configuration
  void set_role_permissions(Role role, std::vector<Permission> permissions);
  [[nodiscard]] const std::vector<Permission> &
  get_role_permissions(Role role) const;

  // Storage Integration
  void set_storage(std::shared_ptr<security_storage_interface> storage);

  // User Management Facade
  [[nodiscard]] auto create_user(const User &user)
      -> kcenon::common::VoidResult;
  [[nodiscard]] auto assign_role(std::string_view user_id, Role role)
      -> kcenon::common::VoidResult;
  [[nodiscard]] auto get_user(std::string_view id)
      -> kcenon::common::Result<User>;

private:
  std::map<Role, std::vector<Permission>> role_permissions_;
  std::shared_ptr<security_storage_interface> storage_;

  void initialize_default_permissions();
};

} // namespace pacs::security
