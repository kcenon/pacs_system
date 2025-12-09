/**
 * @file access_control_manager.cpp
 * @brief Implementation of RBAC logic
 *
 * @copyright Copyright (c) 2025
 */

#include <iostream>
#include <pacs/security/access_control_manager.hpp>

namespace pacs::security {

access_control_manager::access_control_manager() {
  initialize_default_permissions();
}

void access_control_manager::initialize_default_permissions() {
  // Viewer: Read-only access to Studies
  role_permissions_[Role::Viewer] = {{ResourceType::Study, Action::Read},
                                     {ResourceType::Metadata, Action::Read}};

  // Technologist: Read access, Create/Update studies (but not delete)
  role_permissions_[Role::Technologist] = {
      {ResourceType::Study, Action::Read | Action::Write},
      {ResourceType::Metadata, Action::Read | Action::Write}};

  // Radiologist: Same as Technologist + Verification (represented as Write in
  // this simple model or could be separate) Giving Full study access for now
  // (Delete/Export)
  role_permissions_[Role::Radiologist] = {
      {ResourceType::Study, Action::Full},
      {ResourceType::Metadata, Action::Read | Action::Write}};

  // Administrator: User management, System config
  role_permissions_[Role::Administrator] = {
      {ResourceType::System, Action::Full},
      {ResourceType::Role, Action::Full},
      {ResourceType::Audit, Action::Full},
      {ResourceType::Study, Action::Full},
      {ResourceType::Series, Action::Full},
      {ResourceType::Image, Action::Full}};

  // System: Internal superuser
  role_permissions_[Role::System] = {{ResourceType::Study, Action::Full},
                                     {ResourceType::Metadata, Action::Full},
                                     {ResourceType::System, Action::Full},
                                     {ResourceType::User, Action::Full},
                                     {ResourceType::Audit, Action::Full}};
}

void access_control_manager::set_storage(
    std::shared_ptr<security_storage_interface> storage) {
  storage_ = std::move(storage);
}

auto access_control_manager::create_user(const User &user)
    -> kcenon::common::VoidResult {
  if (!storage_)
    return kcenon::common::make_error<std::monostate>(
        1, "Storage not configured", "access_control");
  return storage_->create_user(user);
}

auto access_control_manager::get_user(std::string_view id)
    -> kcenon::common::Result<User> {
  if (!storage_)
    return kcenon::common::make_error<User>(1, "Storage not configured",
                                            "access_control");
  return storage_->get_user(id);
}

auto access_control_manager::assign_role(std::string_view user_id, Role role)
    -> kcenon::common::VoidResult {
  if (!storage_)
    return kcenon::common::make_error<std::monostate>(
        1, "Storage not configured", "access_control");

  // Get user, add role, update
  auto user_res = storage_->get_user(user_id);
  if (user_res.is_err())
    return kcenon::common::make_error<std::monostate>(2, "User not found",
                                                      "access_control");

  auto user = user_res.unwrap();
  if (!user.has_role(role)) {
    user.roles.push_back(role);
    return storage_->update_user(user);
  }
  return kcenon::common::ok();
}

bool access_control_manager::check_permission(const User &user,
                                              ResourceType resource,
                                              std::uint32_t action_mask) const {
  if (!user.active)
    return false;

  for (const auto &role : user.roles) {
    auto it = role_permissions_.find(role);
    if (it != role_permissions_.end()) {
      for (const auto &perm : it->second) {
        if (perm.resource == resource && perm.has_action(action_mask)) {
          return true;
        }
      }
    }
  }
  return false;
}

bool access_control_manager::has_role(const User &user, Role role) const {
  return user.has_role(role);
}

void access_control_manager::set_role_permissions(
    Role role, std::vector<Permission> permissions) {
  role_permissions_[role] = std::move(permissions);
}

const std::vector<Permission> &
access_control_manager::get_role_permissions(Role role) const {
  static const std::vector<Permission> empty;
  auto it = role_permissions_.find(role);
  if (it != role_permissions_.end()) {
    return it->second;
  }
  return empty;
}

} // namespace pacs::security
