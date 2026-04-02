// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file user.hpp
 * @brief User definition for RBAC
 *
 * @copyright Copyright (c) 2025
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include "role.hpp"
#include <algorithm>
#include <string>
#include <vector>

namespace kcenon::pacs::security {

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

} // namespace kcenon::pacs::security
