/**
 * @file user.hpp
 * @brief User definition for RBAC
 *
 * @copyright Copyright (c) 2025
 */

#pragma once

#include "role.hpp"
#include <algorithm>
#include <string>
#include <vector>

namespace pacs::security {

/**
 * @brief Represents a user in the system
 */
struct User {
  std::string id;
  std::string username;
  std::vector<Role> roles;
  bool active{true};

  /**
   * @brief Check if user has a specific role
   */
  bool has_role(Role role) const {
    return std::ranges::find(roles, role) != roles.end();
  }
};

} // namespace pacs::security
