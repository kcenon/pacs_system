/**
 * @file access_control_manager_test.cpp
 * @brief Unit tests for Access Control Manager
 */

#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

#include <thread>

#include "pacs/security/access_control_manager.hpp"
#include "pacs/security/user_context.hpp"
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

TEST_CASE("AccessControlManager: DICOM Operations", "[security]") {
  auto storage = std::make_shared<MockSecurityStorage>();
  access_control_manager acm;
  acm.set_storage(storage);

  // Create users with different roles
  User radiologist;
  radiologist.id = "rad1";
  radiologist.username = "radiologist";
  radiologist.active = true;
  radiologist.roles.push_back(Role::Radiologist);
  REQUIRE(acm.create_user(radiologist).is_ok());

  User viewer;
  viewer.id = "view1";
  viewer.username = "viewer";
  viewer.active = true;
  viewer.roles.push_back(Role::Viewer);
  REQUIRE(acm.create_user(viewer).is_ok());

  User tech;
  tech.id = "tech1";
  tech.username = "technologist";
  tech.active = true;
  tech.roles.push_back(Role::Technologist);
  REQUIRE(acm.create_user(tech).is_ok());

  SECTION("Radiologist can perform all DICOM operations") {
    auto ctx = user_context(radiologist, "session1");

    auto store_result = acm.check_dicom_operation(ctx, DicomOperation::CStore);
    REQUIRE(store_result.allowed);

    auto find_result = acm.check_dicom_operation(ctx, DicomOperation::CFind);
    REQUIRE(find_result.allowed);

    auto move_result = acm.check_dicom_operation(ctx, DicomOperation::CMove);
    REQUIRE(move_result.allowed);
  }

  SECTION("Viewer can only query, not store") {
    auto ctx = user_context(viewer, "session2");

    auto find_result = acm.check_dicom_operation(ctx, DicomOperation::CFind);
    REQUIRE(find_result.allowed);

    auto store_result = acm.check_dicom_operation(ctx, DicomOperation::CStore);
    REQUIRE_FALSE(store_result.allowed);
  }

  SECTION("Technologist can store and query") {
    auto ctx = user_context(tech, "session3");

    auto store_result = acm.check_dicom_operation(ctx, DicomOperation::CStore);
    REQUIRE(store_result.allowed);

    auto find_result = acm.check_dicom_operation(ctx, DicomOperation::CFind);
    REQUIRE(find_result.allowed);
  }
}

TEST_CASE("AccessControlManager: AE Title Mapping", "[security]") {
  auto storage = std::make_shared<MockSecurityStorage>();
  access_control_manager acm;
  acm.set_storage(storage);

  // Create a user
  User modality_user;
  modality_user.id = "modality1";
  modality_user.username = "CT_SCANNER";
  modality_user.active = true;
  modality_user.roles.push_back(Role::Technologist);
  REQUIRE(acm.create_user(modality_user).is_ok());

  SECTION("AE Title can be mapped to user") {
    acm.register_ae_title("CT_SCANNER_AE", "modality1");

    auto user_opt = acm.get_user_by_ae_title("CT_SCANNER_AE");
    REQUIRE(user_opt.has_value());
    REQUIRE(user_opt->id == "modality1");
    REQUIRE(user_opt->has_role(Role::Technologist));
  }

  SECTION("Unknown AE Title returns empty") {
    auto user_opt = acm.get_user_by_ae_title("UNKNOWN_AE");
    REQUIRE_FALSE(user_opt.has_value());
  }

  SECTION("get_context_for_ae returns user context for known AE") {
    acm.register_ae_title("CT_SCANNER_AE", "modality1");

    auto ctx = acm.get_context_for_ae("CT_SCANNER_AE", "session123");
    REQUIRE(ctx.user().id == "modality1");
    REQUIRE(ctx.session_id() == "session123");
    REQUIRE(ctx.source_ae_title().has_value());
    REQUIRE(ctx.source_ae_title().value() == "CT_SCANNER_AE");
  }

  SECTION("get_context_for_ae returns anonymous context for unknown AE") {
    auto ctx = acm.get_context_for_ae("UNKNOWN_AE", "session456");
    REQUIRE(ctx.user().id == "anonymous");
    REQUIRE(ctx.session_id() == "session456");
  }

  SECTION("Unregister AE Title") {
    acm.register_ae_title("CT_SCANNER_AE", "modality1");
    acm.unregister_ae_title("CT_SCANNER_AE");

    auto user_opt = acm.get_user_by_ae_title("CT_SCANNER_AE");
    REQUIRE_FALSE(user_opt.has_value());
  }
}

TEST_CASE("AccessControlManager: Validate Access", "[security]") {
  access_control_manager acm;

  User admin;
  admin.id = "admin1";
  admin.active = true;
  admin.roles.push_back(Role::Administrator);

  User inactive_admin;
  inactive_admin.id = "admin2";
  inactive_admin.active = false;
  inactive_admin.roles.push_back(Role::Administrator);

  SECTION("validate_access succeeds for active user with permission") {
    auto ctx = user_context(admin, "session1");
    auto result = acm.validate_access(ctx, ResourceType::System, Action::Full);
    REQUIRE(result.is_ok());
  }

  SECTION("validate_access fails for inactive user") {
    auto ctx = user_context(inactive_admin, "session2");
    auto result = acm.validate_access(ctx, ResourceType::System, Action::Full);
    REQUIRE(result.is_err());
    REQUIRE(result.error().message.find("not valid") != std::string::npos);
  }

  SECTION("validate_access fails for user without permission") {
    User viewer;
    viewer.id = "viewer1";
    viewer.active = true;
    viewer.roles.push_back(Role::Viewer);

    auto ctx = user_context(viewer, "session3");
    auto result = acm.validate_access(ctx, ResourceType::System, Action::Write);
    REQUIRE(result.is_err());
    REQUIRE(result.error().message.find("Access denied") != std::string::npos);
  }
}

TEST_CASE("AccessControlManager: Audit Callback", "[security]") {
  access_control_manager acm;

  std::vector<std::pair<DicomOperation, bool>> audit_log;

  acm.set_audit_callback(
      [&audit_log](const user_context &ctx, DicomOperation op,
                   const AccessCheckResult &result) {
        audit_log.emplace_back(op, result.allowed);
      });

  User viewer;
  viewer.id = "viewer1";
  viewer.active = true;
  viewer.roles.push_back(Role::Viewer);

  auto ctx = user_context(viewer, "session1");

  SECTION("Audit callback is called for allowed operations") {
    (void)acm.check_dicom_operation(ctx, DicomOperation::CFind);
    REQUIRE(audit_log.size() == 1);
    REQUIRE(audit_log[0].first == DicomOperation::CFind);
    REQUIRE(audit_log[0].second == true);
  }

  SECTION("Audit callback is called for denied operations") {
    (void)acm.check_dicom_operation(ctx, DicomOperation::CStore);
    REQUIRE(audit_log.size() == 1);
    REQUIRE(audit_log[0].first == DicomOperation::CStore);
    REQUIRE(audit_log[0].second == false);
  }
}

TEST_CASE("user_context: Basic Operations", "[security]") {
  SECTION("System context has System role") {
    auto ctx = user_context::system_context();
    REQUIRE(ctx.has_role(Role::System));
    REQUIRE(ctx.is_valid());
    REQUIRE(ctx.session_id() == "system-internal");
  }

  SECTION("Anonymous context has no roles") {
    auto ctx = user_context::anonymous_context("test-session");
    REQUIRE_FALSE(ctx.has_role(Role::Viewer));
    REQUIRE_FALSE(ctx.has_role(Role::Administrator));
    REQUIRE(ctx.is_valid());
    REQUIRE(ctx.session_id() == "test-session");
  }

  SECTION("touch() updates last activity") {
    User u;
    u.id = "test";
    u.active = true;
    auto ctx = user_context(u, "session1");

    auto initial_activity = ctx.last_activity();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ctx.touch();
    REQUIRE(ctx.last_activity() > initial_activity);
  }

  SECTION("Source AE title and IP can be set") {
    User u;
    u.id = "test";
    u.active = true;
    auto ctx = user_context(u, "session1");

    ctx.set_source_ae_title("MODALITY_AE");
    ctx.set_source_ip("192.168.1.100");

    REQUIRE(ctx.source_ae_title().has_value());
    REQUIRE(ctx.source_ae_title().value() == "MODALITY_AE");
    REQUIRE(ctx.source_ip().has_value());
    REQUIRE(ctx.source_ip().value() == "192.168.1.100");
  }
}
