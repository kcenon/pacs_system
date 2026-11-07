// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file rest_app.h
 * @brief The Crow application type of rest_server, with one gate in front of every route
 *
 * Private to pacs_web. A check each route has to remember is a check some route forgets, so the
 * REST API is gated here instead: a new route starts out protected.
 */

#pragma once

// IMPORTANT: Include Crow FIRST before any PACS headers to avoid forward
// declaration conflicts
#include "crow.h"

#include <memory>
#include <string>

namespace kcenon::pacs::web {

struct rest_server_context;

/**
 * @brief Runs around every route of rest_server
 *
 * With OAuth 2.0 enabled, a request to the REST API needs a valid token holding a scope that allows
 * it, whatever the route. Without OAuth 2.0 the gate lets everything through, which is why pacs_server
 * refuses to serve the REST API without it. DICOMweb, WADO-URI and the metrics routes keep their own
 * scopes and check them themselves.
 *
 * It also answers CORS preflight, since Crow replies to OPTIONS itself before any route runs, and
 * writes every change to the REST API, and every refused attempt at one, to the audit trail.
 */
struct rest_gate {
  struct context {
    /// Who the token names, once the gate has checked it
    std::string subject;
  };

  void before_handle(crow::request &req, crow::response &res, context &ctx);
  void after_handle(crow::request &req, crow::response &res, context &ctx);

  /// Set once, before the application runs
  std::shared_ptr<rest_server_context> server;
};

/// The application every endpoint of rest_server registers with
using rest_app = crow::App<rest_gate>;

} // namespace kcenon::pacs::web
