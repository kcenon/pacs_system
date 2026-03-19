// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @file security_storage_interface.hpp
 * @brief Storage interface for RBAC persistence
 *
 * @copyright Copyright (c) 2025
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include "user.hpp"
#include <kcenon/common/patterns/result.h>
#include <string_view>
#include <vector>

namespace kcenon::pacs::security {

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

} // namespace kcenon::pacs::security
