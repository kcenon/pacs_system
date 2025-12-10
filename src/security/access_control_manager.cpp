/**
 * @file access_control_manager.cpp
 * @brief Implementation of RBAC logic
 *
 * @copyright Copyright (c) 2025
 */

#include <pacs/security/access_control_manager.hpp>

namespace pacs::security {

access_control_manager::access_control_manager() {
  initialize_default_permissions();
}

// =============================================================================
// DICOM Operation Mapping
// =============================================================================

std::pair<ResourceType, std::uint32_t>
access_control_manager::map_dicom_operation(DicomOperation op) {
  switch (op) {
  case DicomOperation::CStore:
    return {ResourceType::Study, Action::Write};
  case DicomOperation::CFind:
    return {ResourceType::Metadata, Action::Read};
  case DicomOperation::CMove:
    return {ResourceType::Study, Action::Export};
  case DicomOperation::CGet:
    return {ResourceType::Study, Action::Read | Action::Export};
  case DicomOperation::CEcho:
    return {ResourceType::System, Action::Execute};
  case DicomOperation::NCreate:
    return {ResourceType::Study, Action::Write};
  case DicomOperation::NSet:
    return {ResourceType::Study, Action::Write};
  case DicomOperation::NGet:
    return {ResourceType::Study, Action::Read};
  case DicomOperation::NDelete:
    return {ResourceType::Study, Action::Delete};
  case DicomOperation::NAction:
    return {ResourceType::Study, Action::Execute};
  case DicomOperation::NEventReport:
    return {ResourceType::Study, Action::Read};
  }
  return {ResourceType::System, Action::None};
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

// =============================================================================
// DICOM Access Control
// =============================================================================

auto access_control_manager::validate_access(const user_context &ctx,
                                             ResourceType resource,
                                             std::uint32_t action_mask)
    -> kcenon::common::VoidResult {
  if (!ctx.is_valid()) {
    return kcenon::common::make_error<std::monostate>(
        1, "User context is not valid (inactive user)", "access_control");
  }

  if (!check_permission(ctx.user(), resource, action_mask)) {
    return kcenon::common::make_error<std::monostate>(
        2, "Access denied: insufficient permissions", "access_control");
  }

  return kcenon::common::ok();
}

AccessCheckResult
access_control_manager::check_dicom_operation(const user_context &ctx,
                                              DicomOperation op) const {
  if (!ctx.is_valid()) {
    auto result = AccessCheckResult::deny("User context is not valid");
    if (audit_callback_) {
      audit_callback_(ctx, op, result);
    }
    return result;
  }

  auto [resource, action] = map_dicom_operation(op);
  bool allowed = check_permission(ctx.user(), resource, action);

  AccessCheckResult result =
      allowed ? AccessCheckResult::allow()
              : AccessCheckResult::deny("Insufficient permissions for operation");

  if (audit_callback_) {
    audit_callback_(ctx, op, result);
  }

  return result;
}

user_context access_control_manager::get_context_for_ae(
    std::string_view ae_title, const std::string &session_id) const {
  std::lock_guard<std::mutex> lock(mutex_);

  // Look up AE Title to user mapping
  auto it = ae_to_user_id_.find(std::string(ae_title));
  if (it == ae_to_user_id_.end()) {
    // No mapping found, return anonymous context
    auto ctx = user_context::anonymous_context(session_id);
    ctx.set_source_ae_title(std::string(ae_title));
    return ctx;
  }

  // Get user from storage
  if (!storage_) {
    auto ctx = user_context::anonymous_context(session_id);
    ctx.set_source_ae_title(std::string(ae_title));
    return ctx;
  }

  auto user_result = storage_->get_user(it->second);
  if (user_result.is_err()) {
    auto ctx = user_context::anonymous_context(session_id);
    ctx.set_source_ae_title(std::string(ae_title));
    return ctx;
  }

  auto ctx = user_context(user_result.unwrap(), session_id);
  ctx.set_source_ae_title(std::string(ae_title));
  return ctx;
}

// =============================================================================
// AE Title Mapping
// =============================================================================

void access_control_manager::register_ae_title(std::string_view ae_title,
                                               std::string_view user_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  ae_to_user_id_[std::string(ae_title)] = std::string(user_id);
}

void access_control_manager::unregister_ae_title(std::string_view ae_title) {
  std::lock_guard<std::mutex> lock(mutex_);
  ae_to_user_id_.erase(std::string(ae_title));
}

auto access_control_manager::get_user_by_ae_title(std::string_view ae_title)
    -> std::optional<User> {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = ae_to_user_id_.find(std::string(ae_title));
  if (it == ae_to_user_id_.end()) {
    return std::nullopt;
  }

  if (!storage_) {
    return std::nullopt;
  }

  auto user_result = storage_->get_user(it->second);
  if (user_result.is_err()) {
    return std::nullopt;
  }

  return user_result.unwrap();
}

// =============================================================================
// Audit Callback
// =============================================================================

void access_control_manager::set_audit_callback(AccessAuditCallback callback) {
  std::lock_guard<std::mutex> lock(mutex_);
  audit_callback_ = std::move(callback);
}

} // namespace pacs::security
