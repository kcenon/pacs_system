// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file rest_gate.cpp
 * @brief Authorization and CORS preflight in front of every route of rest_server
 */

#include "rest_app.h"

#include "kcenon/pacs/storage/audit_record.h"
#include "kcenon/pacs/storage/index_database.h"
#include "kcenon/pacs/web/auth/oauth2_middleware.h"
#include "kcenon/pacs/web/endpoints/system_endpoints.h"
#include "kcenon/pacs/web/rest_config.h"

#include <array>
#include <chrono>
#include <string>
#include <string_view>
#include <vector>

// Workaround for Windows: DELETE is defined as a macro in <winnt.h>
// which conflicts with crow::HTTPMethod::DELETE
#ifdef DELETE
#undef DELETE
#endif

namespace kcenon::pacs::web {

namespace {

bool starts_with(std::string_view text, std::string_view prefix) {
  return text.substr(0, prefix.size()) == prefix;
}

/// Routes that check their own tokens and scopes. Everything else is gated.
bool guards_itself(std::string_view url) {
  constexpr std::array<std::string_view, 5> prefixes{
      "/dicomweb", "/wado", "/metrics", "/api/metrics/", "/api/health/"};
  for (const auto prefix : prefixes) {
    if (starts_with(url, prefix)) {
      return true;
    }
  }
  return false;
}

/// Settings, accounts and the audit trail are for administrators, in both directions.
bool only_for_administrators(std::string_view url) {
  constexpr std::array<std::string_view, 3> prefixes{
      "/api/v1/security/", "/api/v1/audit/", "/api/v1/system/config"};
  for (const auto prefix : prefixes) {
    if (starts_with(url, prefix)) {
      return true;
    }
  }
  return false;
}

bool only_reads(crow::HTTPMethod method) {
  return method == crow::HTTPMethod::GET || method == crow::HTTPMethod::HEAD;
}

std::vector<std::string> scopes_for(crow::HTTPMethod method, std::string_view url) {
  const std::string admin{auth::rest_scopes::admin};
  const std::string write{auth::rest_scopes::write};
  const std::string read{auth::rest_scopes::read};
  const bool reads = only_reads(method);
  // Deleting a study removes what the archive exists to keep; a worklist item is an ordinary edit.
  const bool removes_a_study = method == crow::HTTPMethod::DELETE && starts_with(url, "/api/v1/studies/");
  if (only_for_administrators(url) || removes_a_study) {
    return {admin};
  }
  if (reads) {
    return {read, write, admin};
  }
  return {write, admin};
}

/// Changes through the REST API and refused attempts at them. A failed write to the trail must not
/// turn a finished request into a failure, so its result is not checked.
void record_change(rest_server_context &server, const crow::request &req, const crow::response &res,
                   const std::string &subject) {
  if (!server.database || req.method == crow::HTTPMethod::OPTIONS || only_reads(req.method) ||
      !starts_with(req.url, "/api/v1/")) {
    return;
  }
  const bool refused = res.code == 401 || res.code == 403;
  storage::audit_record record;
  record.event_type = storage::to_string(refused ? storage::audit_event_type::security_event
                                                 : storage::audit_event_type::data_access);
  record.outcome = storage::to_string(res.code < 400 ? storage::audit_outcome::success : storage::audit_outcome::failure);
  record.timestamp = std::chrono::system_clock::now();
  record.user_id = subject;
  record.source_ip = req.remote_ip_address;
  record.message = crow::method_name(req.method) + " " + req.url + " answered " + std::to_string(res.code);
  (void)server.database->add_audit_log(record);
}

} // namespace

void rest_gate::before_handle(crow::request &req, crow::response &res, context &ctx) {
  if (!server || !server->oauth2 || !server->oauth2->enabled() || guards_itself(req.url)) {
    return;
  }
  const auto authenticated = server->oauth2->authenticate(req, res);
  if (!authenticated) {
    res.end();
    return;
  }
  ctx.subject = authenticated->claims.sub;
  if (!server->oauth2->require_any_scope(authenticated->claims, res, scopes_for(req.method, req.url))) {
    res.end();
  }
}

void rest_gate::after_handle(crow::request &req, crow::response &res, context &ctx) {
  if (server) {
    record_change(*server, req, res, ctx.subject);
  }
  const rest_server_config *config = server ? server->config : nullptr;
  if (config == nullptr || config->cors_allowed_origins.empty()) {
    return;
  }
  // Routes add this themselves; the gate's own refusals and Crow's own answers do not.
  if (res.get_header_value("Access-Control-Allow-Origin").empty()) {
    res.add_header("Access-Control-Allow-Origin", config->cors_allowed_origins);
  }
  // Crow answers OPTIONS while it reads the URL, before the request's headers exist, so the Origin
  // cannot be compared here. Naming the one configured origin leaves that comparison to the browser.
  if (req.method == crow::HTTPMethod::OPTIONS && res.code < 300) {
    res.add_header("Access-Control-Allow-Methods", "GET, HEAD, POST, PUT, DELETE, OPTIONS");
    res.add_header("Access-Control-Allow-Headers", "Accept, Authorization, Content-Type");
    res.add_header("Access-Control-Max-Age", "86400");
  }
}

} // namespace kcenon::pacs::web
