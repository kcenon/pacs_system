/**
 * @file access_control_manager_test.cpp
 * @brief Unit tests for Access Control Manager
 */

#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

#include "pacs/security/access_control_manager.hpp"
#include "pacs/security/role.hpp"
#include "pacs/security/security_storage_interface.hpp"

using namespace pacs::security;

// Mock Storage
class MockSecurityStorage : public security_storage_interface {
public:
  kcenon::common::VoidResult create_user(const User &user) override {
    users[user.id] = user;
    return kcenon::common::ok();
  }

  kcenon::common::Result<User> get_user(std::string_view id) override {
    if (users.contains(std::string(id))) {
      return users[std::string(id)];
    }
    return kcenon::common::make_error<User>(1, "Not found");
  }

  kcenon::common::Result<User>
  get_user_by_username(std::string_view username) override {
    for (const auto &[id, u] : users) {
      if (u.username == username)
        return u;
    }
    return kcenon::common::make_error<User>(1, "Not found");
  }

  kcenon::common::VoidResult update_user(const User &user) override {
    users[user.id] = user;
    return kcenon::common::ok();
  }

  kcenon::common::VoidResult delete_user(std::string_view id) override {
    users.erase(std::string(id));
    return kcenon::common::ok();
  }

  kcenon::common::Result<std::vector<User>>
  get_users_by_role(Role role) override {
    std::vector<User> res;
    for (const auto &[id, u] : users) {
      if (u.has_role(role))
        res.push_back(u);
    }
    return res;
  }

  std::map<std::string, User> users;
};

TEST_CASE("AccessControlManager: Permission Checks", "[security]") {
  access_control_manager acm; // Checks default permissions

  User admin;
  admin.active = true;
  admin.roles.push_back(Role::Administrator);

  User viewer;
  viewer.active = true;
  viewer.roles.push_back(Role::Viewer);

  User inactive;
  inactive.active = false;
  inactive.roles.push_back(Role::Administrator);

  SECTION("Active admin has full access") {
    REQUIRE(acm.check_permission(admin, ResourceType::System, Action::Full));
    REQUIRE(acm.check_permission(admin, ResourceType::Study, Action::Read));
  }

  SECTION("Viewer has read access but not write") {
    REQUIRE(acm.check_permission(viewer, ResourceType::Study, Action::Read));
    // Using action masks directly for check
    REQUIRE_FALSE(acm.check_permission(viewer, ResourceType::Study,
                                       static_cast<uint32_t>(Action::Write)));
    REQUIRE_FALSE(
        acm.check_permission(viewer, ResourceType::System, Action::Read));
  }

  SECTION("Inactive user has no access") {
    REQUIRE_FALSE(
        acm.check_permission(inactive, ResourceType::Study, Action::Read));
  }
}

TEST_CASE("AccessControlManager: User Management", "[security]") {
  auto storage = std::make_shared<MockSecurityStorage>();
  access_control_manager acm;
  acm.set_storage(storage);

  User u;
  u.id = "user1";
  u.username = "testuser";
  u.active = true;

  SECTION("Create User") {
    auto res = acm.create_user(u);
    REQUIRE(res.is_ok());
    REQUIRE(storage->users.contains("user1"));
  }

  SECTION("Assign Role") {
    REQUIRE(acm.create_user(u).is_ok());
    auto res = acm.assign_role("user1", Role::Radiologist);
    REQUIRE(res.is_ok());

    auto user = storage->get_user("user1").unwrap();
    REQUIRE(user.has_role(Role::Radiologist));
  }
}
