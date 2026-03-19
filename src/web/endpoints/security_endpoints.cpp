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
 * @file security_endpoints.cpp
 * @brief Security API endpoints implementation
 *
 * @copyright Copyright (c) 2025
 * @license MIT
 */

#include "pacs/web/endpoints/security_endpoints.hpp"
#include "pacs/security/access_control_manager.hpp"
#include "pacs/security/role.hpp"
#include "pacs/security/user.hpp"
#include "pacs/web/rest_types.hpp"

namespace kcenon::pacs::web::endpoints {

void register_security_endpoints_impl(
    crow::SimpleApp &app, std::shared_ptr<rest_server_context> ctx) {

  // POST /api/v1/security/users - Create a new user
  CROW_ROUTE(app, "/api/v1/security/users")
      .methods(crow::HTTPMethod::POST)([ctx](const crow::request &req) {
        crow::response res;
        res.add_header("Content-Type", "application/json");

        if (!ctx->security_manager) {
          res.body = make_error_json("SECURITY_UNAVAILABLE",
                                     "Security manager not configured");
          res.code = 503;
          return res;
        }

        auto x = crow::json::load(req.body);
        if (!x) {
          res.body = make_error_json("INVALID_JSON", "Invalid JSON body");
          res.code = 400;
          return res;
        }

        if (!x.has("username") || !x.has("id")) {
          res.body =
              make_error_json("MISSING_FIELDS", "Username and ID are required");
          res.code = 400;
          return res;
        }

        kcenon::pacs::security::User user;
        user.id = x["id"].s();
        user.username = x["username"].s();
        user.active = true; // Default to active

        auto result = ctx->security_manager->create_user(user);
        if (result.is_err()) {
          res.body = make_error_json(
              "CREATE_FAILED", "Failed to create user"); // In real app, expose
                                                         // inner error safely
          res.code = 500;
        } else {
          res.body = make_success_json("User created");
          res.code = 201;
        }
        return res;
      });

  // POST /api/v1/security/users/<id>/roles - Assign role to user
  CROW_ROUTE(app, "/api/v1/security/users/<string>/roles")
      .methods(crow::HTTPMethod::POST)([ctx](const crow::request &req,
                                             std::string user_id) {
        crow::response res;
        res.add_header("Content-Type", "application/json");

        if (!ctx->security_manager) {
          res.body = make_error_json("SECURITY_UNAVAILABLE",
                                     "Security manager not configured");
          res.code = 503;
          return res;
        }

        auto x = crow::json::load(req.body);
        if (!x || !x.has("role")) {
          res.body = make_error_json("INVALID_REQUEST", "Role is required");
          res.code = 400;
          return res;
        }

        std::string role_str = x["role"].s();
        auto role_opt = kcenon::pacs::security::parse_role(role_str);
        if (!role_opt) {
          res.body = make_error_json("INVALID_ROLE", "Invalid role specified");
          res.code = 400;
          return res;
        }

        auto result = ctx->security_manager->assign_role(user_id, *role_opt);
        if (result.is_err()) {
          // Could distinguish user not found vs other errors
          res.body = make_error_json("ASSIGN_FAILED", "Failed to assign role");
          res.code = 500;
        } else {
          res.body = make_success_json("Role assigned");
          res.code = 200;
        }
        return res;
      });
}

} // namespace kcenon::pacs::web::endpoints
