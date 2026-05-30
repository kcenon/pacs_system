// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file security_endpoints.h
 * @brief Security API endpoints for REST server
 *
 * @copyright Copyright (c) 2025
 * @license MIT
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include "crow.h"
#include "kcenon/pacs/web/endpoints/system_endpoints.h" // For rest_server_context

namespace kcenon::pacs::web::endpoints {

/**
 * @brief Register security endpoints with the Crow app
 * @param app Crow application instance
 * @param ctx Shared server context
 */
void register_security_endpoints_impl(crow::SimpleApp &app,
                                      std::shared_ptr<rest_server_context> ctx);

} // namespace kcenon::pacs::web::endpoints
