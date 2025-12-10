/**
 * @file user_context.hpp
 * @brief User context for session-based access control
 *
 * @copyright Copyright (c) 2025
 */

#pragma once

#include "user.hpp"
#include <chrono>
#include <optional>
#include <string>

namespace pacs::security {

/**
 * @brief Represents the security context for a user session
 *
 * Encapsulates user information along with session-specific data
 * for access control decisions.
 */
class user_context {
public:
  using clock = std::chrono::steady_clock;
  using time_point = clock::time_point;

  /**
   * @brief Construct a user context
   * @param user The authenticated user
   * @param session_id Unique session identifier
   */
  user_context(User user, std::string session_id)
      : user_(std::move(user)), session_id_(std::move(session_id)),
        created_at_(clock::now()), last_activity_(clock::now()) {}

  /**
   * @brief Create a system context for internal operations
   */
  static user_context system_context() {
    User system_user;
    system_user.id = "system";
    system_user.username = "system";
    system_user.roles = {Role::System};
    system_user.active = true;
    return user_context(std::move(system_user), "system-internal");
  }

  /**
   * @brief Create an anonymous context with minimal permissions
   */
  static user_context anonymous_context(const std::string &session_id) {
    User anon_user;
    anon_user.id = "anonymous";
    anon_user.username = "anonymous";
    anon_user.roles = {}; // No roles = no permissions
    anon_user.active = true;
    return user_context(std::move(anon_user), session_id);
  }

  // Accessors
  [[nodiscard]] const User &user() const noexcept { return user_; }
  [[nodiscard]] const std::string &session_id() const noexcept {
    return session_id_;
  }
  [[nodiscard]] time_point created_at() const noexcept { return created_at_; }
  [[nodiscard]] time_point last_activity() const noexcept {
    return last_activity_;
  }

  /**
   * @brief Check if the context is valid (user is active)
   */
  [[nodiscard]] bool is_valid() const noexcept { return user_.active; }

  /**
   * @brief Check if user has a specific role
   */
  [[nodiscard]] bool has_role(Role role) const { return user_.has_role(role); }

  /**
   * @brief Update last activity timestamp
   */
  void touch() { last_activity_ = clock::now(); }

  // Optional: Source information for audit
  void set_source_ae_title(std::string ae) {
    source_ae_title_ = std::move(ae);
  }
  [[nodiscard]] const std::optional<std::string> &
  source_ae_title() const noexcept {
    return source_ae_title_;
  }

  void set_source_ip(std::string ip) { source_ip_ = std::move(ip); }
  [[nodiscard]] const std::optional<std::string> &source_ip() const noexcept {
    return source_ip_;
  }

private:
  User user_;
  std::string session_id_;
  time_point created_at_;
  time_point last_activity_;
  std::optional<std::string> source_ae_title_;
  std::optional<std::string> source_ip_;
};

} // namespace pacs::security
