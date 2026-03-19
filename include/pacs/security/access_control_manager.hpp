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
 * @file access_control_manager.hpp
 * @brief Core RBAC logic
 *
 * @copyright Copyright (c) 2025
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include "permission.hpp"
#include "security_storage_interface.hpp"
#include "user.hpp"
#include "user_context.hpp"
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace kcenon::pacs::security {

/**
 * @brief DICOM operation types for permission checking
 */
enum class DicomOperation {
  CStore,   ///< C-STORE (storage)
  CFind,    ///< C-FIND (query)
  CMove,    ///< C-MOVE (retrieve/move)
  CGet,     ///< C-GET (retrieve)
  CEcho,    ///< C-ECHO (verification)
  NCreate,  ///< N-CREATE
  NSet,     ///< N-SET
  NGet,     ///< N-GET
  NDelete,  ///< N-DELETE
  NAction,  ///< N-ACTION
  NEventReport ///< N-EVENT-REPORT
};

/**
 * @brief Result of an access check
 */
struct AccessCheckResult {
  bool allowed{false};
  std::string reason;

  static AccessCheckResult allow() { return {true, ""}; }
  static AccessCheckResult deny(std::string reason) {
    return {false, std::move(reason)};
  }

  explicit operator bool() const { return allowed; }
};

/**
 * @brief Callback for audit logging of access attempts
 */
using AccessAuditCallback =
    std::function<void(const user_context &ctx, DicomOperation op,
                       const AccessCheckResult &result)>;

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

  /**
   * @brief Validate access for a user context
   * @param ctx User context to validate
   * @param resource Resource being accessed
   * @param action_mask Actions being performed
   * @return Result with void on success, error on denial
   */
  [[nodiscard]] auto validate_access(const user_context &ctx,
                                     ResourceType resource,
                                     std::uint32_t action_mask)
      -> kcenon::common::VoidResult;

  /**
   * @brief Check if a DICOM operation is allowed
   * @param ctx User context
   * @param op DICOM operation to check
   * @return AccessCheckResult with allowed status and reason
   */
  [[nodiscard]] AccessCheckResult check_dicom_operation(const user_context &ctx,
                                                        DicomOperation op) const;

  /**
   * @brief Get user context for an AE Title
   * @param ae_title The AE Title to look up
   * @param session_id Session identifier for the context
   * @return User context if found, std::nullopt for unregistered AE titles
   */
  [[nodiscard]] std::optional<user_context> get_context_for_ae(
      std::string_view ae_title, const std::string &session_id) const;

  // Configuration
  void set_role_permissions(Role role, std::vector<Permission> permissions);
  [[nodiscard]] const std::vector<Permission> &
  get_role_permissions(Role role) const;

  // Storage Integration
  void set_storage(std::shared_ptr<security_storage_interface> storage);

  // AE Title to User mapping
  void register_ae_title(std::string_view ae_title, std::string_view user_id);
  void unregister_ae_title(std::string_view ae_title);

  // Audit callback
  void set_audit_callback(AccessAuditCallback callback);

  // User Management Facade
  [[nodiscard]] auto create_user(const User &user)
      -> kcenon::common::VoidResult;
  [[nodiscard]] auto assign_role(std::string_view user_id, Role role)
      -> kcenon::common::VoidResult;
  [[nodiscard]] auto get_user(std::string_view id)
      -> kcenon::common::Result<User>;

  /**
   * @brief Get user by AE Title
   * @param ae_title The AE Title to look up
   * @return User if found
   */
  [[nodiscard]] auto get_user_by_ae_title(std::string_view ae_title)
      -> std::optional<User>;

private:
  std::map<Role, std::vector<Permission>> role_permissions_;
  std::shared_ptr<security_storage_interface> storage_;
  std::map<std::string, std::string> ae_to_user_id_;
  AccessAuditCallback audit_callback_;
  mutable std::mutex mutex_;

  void initialize_default_permissions();

  /**
   * @brief Map DICOM operation to resource type and action
   */
  static std::pair<ResourceType, std::uint32_t>
  map_dicom_operation(DicomOperation op);
};

} // namespace kcenon::pacs::security
