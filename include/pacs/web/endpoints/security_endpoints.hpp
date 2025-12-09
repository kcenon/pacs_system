/**
 * @file security_endpoints.hpp
 * @brief Security API endpoints for REST server
 *
 * @copyright Copyright (c) 2025
 * @license MIT
 */

#pragma once

#include "crow.h"
#include "pacs/web/endpoints/system_endpoints.hpp" // For rest_server_context

namespace pacs::web::endpoints {

/**
 * @brief Register security endpoints with the Crow app
 * @param app Crow application instance
 * @param ctx Shared server context
 */
void register_security_endpoints_impl(crow::SimpleApp &app,
                                      std::shared_ptr<rest_server_context> ctx);

} // namespace pacs::web::endpoints
