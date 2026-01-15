/**
 * @file remote_nodes_endpoints.cpp
 * @brief Remote PACS node management REST API endpoints implementation
 *
 * @see Issue #536 - Implement Remote Node REST API Endpoints
 * @copyright Copyright (c) 2025
 * @license MIT
 */

// IMPORTANT: Include Crow FIRST before any PACS headers to avoid forward
// declaration conflicts
#include "crow.h"

// Workaround for Windows: DELETE is defined as a macro in <winnt.h>
// which conflicts with crow::HTTPMethod::DELETE
#ifdef DELETE
#undef DELETE
#endif

#include "pacs/client/remote_node.hpp"
#include "pacs/client/remote_node_manager.hpp"
#include "pacs/web/endpoints/remote_nodes_endpoints.hpp"
#include "pacs/web/endpoints/system_endpoints.hpp"
#include "pacs/web/rest_config.hpp"
#include "pacs/web/rest_types.hpp"

#include <chrono>
#include <sstream>

namespace pacs::web::endpoints {

namespace {

/**
 * @brief Add CORS headers to response
 */
void add_cors_headers(crow::response& res, const rest_server_context& ctx) {
    if (ctx.config && !ctx.config->cors_allowed_origins.empty()) {
        res.add_header("Access-Control-Allow-Origin",
                       ctx.config->cors_allowed_origins);
    }
}

/**
 * @brief Format ISO 8601 timestamp
 */
std::string format_timestamp(std::chrono::system_clock::time_point tp) {
    if (tp == std::chrono::system_clock::time_point{}) {
        return "";
    }
    auto time_t_val = std::chrono::system_clock::to_time_t(tp);
    std::tm tm_val{};
#ifdef _WIN32
    gmtime_s(&tm_val, &time_t_val);
#else
    gmtime_r(&time_t_val, &tm_val);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_val);
    return buf;
}

/**
 * @brief Convert remote_node to JSON string
 */
std::string node_to_json(const client::remote_node& node) {
    std::ostringstream oss;
    oss << R"({"node_id":")" << json_escape(node.node_id)
        << R"(","name":")" << json_escape(node.name)
        << R"(","ae_title":")" << json_escape(node.ae_title)
        << R"(","host":")" << json_escape(node.host)
        << R"(","port":)" << node.port
        << R"(,"supports_find":)" << (node.supports_find ? "true" : "false")
        << R"(,"supports_move":)" << (node.supports_move ? "true" : "false")
        << R"(,"supports_get":)" << (node.supports_get ? "true" : "false")
        << R"(,"supports_store":)" << (node.supports_store ? "true" : "false")
        << R"(,"supports_worklist":)" << (node.supports_worklist ? "true" : "false")
        << R"(,"connection_timeout_sec":)" << node.connection_timeout.count()
        << R"(,"dimse_timeout_sec":)" << node.dimse_timeout.count()
        << R"(,"max_associations":)" << node.max_associations
        << R"(,"status":")" << client::to_string(node.status) << '"';

    auto last_verified = format_timestamp(node.last_verified);
    if (!last_verified.empty()) {
        oss << R"(,"last_verified":")" << last_verified << '"';
    }

    auto last_error_time = format_timestamp(node.last_error);
    if (!last_error_time.empty()) {
        oss << R"(,"last_error":")" << last_error_time << '"';
    }

    if (!node.last_error_message.empty()) {
        oss << R"(,"last_error_message":")" << json_escape(node.last_error_message) << '"';
    }

    auto created_at = format_timestamp(node.created_at);
    if (!created_at.empty()) {
        oss << R"(,"created_at":")" << created_at << '"';
    }

    auto updated_at = format_timestamp(node.updated_at);
    if (!updated_at.empty()) {
        oss << R"(,"updated_at":")" << updated_at << '"';
    }

    oss << '}';
    return oss.str();
}

/**
 * @brief Convert vector of remote_nodes to JSON array string
 */
std::string nodes_to_json(const std::vector<client::remote_node>& nodes,
                          size_t total_count) {
    std::ostringstream oss;
    oss << R"({"nodes":[)";
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (i > 0) {
            oss << ',';
        }
        oss << node_to_json(nodes[i]);
    }
    oss << R"(],"total":)" << total_count << '}';
    return oss.str();
}

/**
 * @brief Parse node from JSON body
 */
std::optional<client::remote_node> parse_node_from_json(const std::string& body,
                                                         std::string& error_message) {
    client::remote_node node;

    // Simple JSON parsing (in production, use a proper JSON library)
    auto get_string_value = [&body](const std::string& key) -> std::string {
        std::string search = "\"" + key + "\":\"";
        auto pos = body.find(search);
        if (pos == std::string::npos) {
            return "";
        }
        pos += search.length();
        auto end_pos = body.find('"', pos);
        if (end_pos == std::string::npos) {
            return "";
        }
        return body.substr(pos, end_pos - pos);
    };

    auto get_int_value = [&body](const std::string& key) -> std::optional<int64_t> {
        std::string search = "\"" + key + "\":";
        auto pos = body.find(search);
        if (pos == std::string::npos) {
            return std::nullopt;
        }
        pos += search.length();
        // Skip whitespace
        while (pos < body.length() && (body[pos] == ' ' || body[pos] == '\t')) {
            ++pos;
        }
        if (pos >= body.length()) {
            return std::nullopt;
        }
        try {
            size_t idx = 0;
            auto val = std::stoll(body.substr(pos), &idx);
            return val;
        } catch (...) {
            return std::nullopt;
        }
    };

    auto get_bool_value = [&body](const std::string& key,
                                   bool default_val) -> bool {
        std::string search = "\"" + key + "\":";
        auto pos = body.find(search);
        if (pos == std::string::npos) {
            return default_val;
        }
        pos += search.length();
        // Skip whitespace
        while (pos < body.length() && (body[pos] == ' ' || body[pos] == '\t')) {
            ++pos;
        }
        if (pos >= body.length()) {
            return default_val;
        }
        if (body.substr(pos, 4) == "true") {
            return true;
        }
        if (body.substr(pos, 5) == "false") {
            return false;
        }
        return default_val;
    };

    // Required fields
    node.name = get_string_value("name");
    node.ae_title = get_string_value("ae_title");
    node.host = get_string_value("host");

    if (node.ae_title.empty()) {
        error_message = "ae_title is required";
        return std::nullopt;
    }

    if (node.host.empty()) {
        error_message = "host is required";
        return std::nullopt;
    }

    // Optional fields with defaults
    auto port_val = get_int_value("port");
    node.port = port_val.value_or(104);

    node.supports_find = get_bool_value("supports_find", true);
    node.supports_move = get_bool_value("supports_move", true);
    node.supports_get = get_bool_value("supports_get", false);
    node.supports_store = get_bool_value("supports_store", true);
    node.supports_worklist = get_bool_value("supports_worklist", false);

    auto connection_timeout = get_int_value("connection_timeout_sec");
    if (connection_timeout) {
        node.connection_timeout = std::chrono::seconds(*connection_timeout);
    }

    auto dimse_timeout = get_int_value("dimse_timeout_sec");
    if (dimse_timeout) {
        node.dimse_timeout = std::chrono::seconds(*dimse_timeout);
    }

    auto max_assoc = get_int_value("max_associations");
    if (max_assoc) {
        node.max_associations = static_cast<size_t>(*max_assoc);
    }

    return node;
}

/**
 * @brief Parse pagination parameters from request
 */
std::pair<size_t, size_t> parse_pagination(const crow::request& req) {
    size_t limit = 20;
    size_t offset = 0;

    auto limit_param = req.url_params.get("limit");
    if (limit_param) {
        try {
            limit = std::stoul(limit_param);
            if (limit > 100) {
                limit = 100;
            }
        } catch (...) {
            // Use default
        }
    }

    auto offset_param = req.url_params.get("offset");
    if (offset_param) {
        try {
            offset = std::stoul(offset_param);
        } catch (...) {
            // Use default
        }
    }

    return {limit, offset};
}

} // namespace

// Internal implementation function called from rest_server.cpp
void register_remote_nodes_endpoints_impl(crow::SimpleApp& app,
                                           std::shared_ptr<rest_server_context> ctx) {
    // GET /api/v1/remote-nodes - List remote nodes (paginated)
    CROW_ROUTE(app, "/api/v1/remote-nodes")
        .methods(crow::HTTPMethod::GET)([ctx](const crow::request& req) {
            crow::response res;
            res.add_header("Content-Type", "application/json");
            add_cors_headers(res, *ctx);

            if (!ctx->node_manager) {
                res.code = 503;
                res.body = make_error_json("SERVICE_UNAVAILABLE",
                                           "Remote node manager not configured");
                return res;
            }

            // Parse pagination
            auto [limit, offset] = parse_pagination(req);

            // Filter by status if provided
            auto status_param = req.url_params.get("status");
            std::vector<client::remote_node> nodes;

            if (status_param) {
                auto status = client::node_status_from_string(status_param);
                nodes = ctx->node_manager->list_nodes_by_status(status);
            } else {
                nodes = ctx->node_manager->list_nodes();
            }

            size_t total_count = nodes.size();

            // Apply pagination
            std::vector<client::remote_node> paginated;
            for (size_t i = offset; i < nodes.size() && paginated.size() < limit; ++i) {
                paginated.push_back(nodes[i]);
            }

            res.code = 200;
            res.body = nodes_to_json(paginated, total_count);
            return res;
        });

    // POST /api/v1/remote-nodes - Create a new remote node
    CROW_ROUTE(app, "/api/v1/remote-nodes")
        .methods(crow::HTTPMethod::POST)([ctx](const crow::request& req) {
            crow::response res;
            res.add_header("Content-Type", "application/json");
            add_cors_headers(res, *ctx);

            if (!ctx->node_manager) {
                res.code = 503;
                res.body = make_error_json("SERVICE_UNAVAILABLE",
                                           "Remote node manager not configured");
                return res;
            }

            std::string error_message;
            auto node_opt = parse_node_from_json(req.body, error_message);
            if (!node_opt) {
                res.code = 400;
                res.body = make_error_json("INVALID_REQUEST", error_message);
                return res;
            }

            auto& node = *node_opt;

            // Generate node_id if not provided
            if (node.node_id.empty()) {
                // Use ae_title + timestamp as default node_id
                auto now = std::chrono::system_clock::now();
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch()).count();
                node.node_id = node.ae_title + "_" + std::to_string(ms);
            }

            auto result = ctx->node_manager->add_node(node);
            if (!result.is_ok()) {
                res.code = 409;
                res.body = make_error_json("CONFLICT", result.error().message);
                return res;
            }

            // Retrieve the created node to get full details
            auto created_node = ctx->node_manager->get_node(node.node_id);
            if (!created_node) {
                res.code = 201;
                res.body = node_to_json(node);
                return res;
            }

            res.code = 201;
            res.body = node_to_json(*created_node);
            return res;
        });

    // GET /api/v1/remote-nodes/<nodeId> - Get a specific remote node
    CROW_ROUTE(app, "/api/v1/remote-nodes/<string>")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& /*req*/, const std::string& node_id) {
                crow::response res;
                res.add_header("Content-Type", "application/json");
                add_cors_headers(res, *ctx);

                if (!ctx->node_manager) {
                    res.code = 503;
                    res.body = make_error_json("SERVICE_UNAVAILABLE",
                                               "Remote node manager not configured");
                    return res;
                }

                auto node = ctx->node_manager->get_node(node_id);
                if (!node) {
                    res.code = 404;
                    res.body = make_error_json("NOT_FOUND", "Remote node not found");
                    return res;
                }

                res.code = 200;
                res.body = node_to_json(*node);
                return res;
            });

    // PUT /api/v1/remote-nodes/<nodeId> - Update a remote node
    CROW_ROUTE(app, "/api/v1/remote-nodes/<string>")
        .methods(crow::HTTPMethod::PUT)(
            [ctx](const crow::request& req, const std::string& node_id) {
                crow::response res;
                res.add_header("Content-Type", "application/json");
                add_cors_headers(res, *ctx);

                if (!ctx->node_manager) {
                    res.code = 503;
                    res.body = make_error_json("SERVICE_UNAVAILABLE",
                                               "Remote node manager not configured");
                    return res;
                }

                // Check if node exists
                auto existing = ctx->node_manager->get_node(node_id);
                if (!existing) {
                    res.code = 404;
                    res.body = make_error_json("NOT_FOUND", "Remote node not found");
                    return res;
                }

                std::string error_message;
                auto node_opt = parse_node_from_json(req.body, error_message);
                if (!node_opt) {
                    res.code = 400;
                    res.body = make_error_json("INVALID_REQUEST", error_message);
                    return res;
                }

                auto& node = *node_opt;
                node.node_id = node_id; // Preserve the node_id from URL

                auto result = ctx->node_manager->update_node(node);
                if (!result.is_ok()) {
                    res.code = 500;
                    res.body = make_error_json("UPDATE_FAILED", result.error().message);
                    return res;
                }

                // Retrieve the updated node
                auto updated_node = ctx->node_manager->get_node(node_id);
                if (!updated_node) {
                    res.code = 200;
                    res.body = node_to_json(node);
                    return res;
                }

                res.code = 200;
                res.body = node_to_json(*updated_node);
                return res;
            });

    // DELETE /api/v1/remote-nodes/<nodeId> - Delete a remote node
    CROW_ROUTE(app, "/api/v1/remote-nodes/<string>")
        .methods(crow::HTTPMethod::DELETE)(
            [ctx](const crow::request& /*req*/, const std::string& node_id) {
                crow::response res;
                add_cors_headers(res, *ctx);

                if (!ctx->node_manager) {
                    res.add_header("Content-Type", "application/json");
                    res.code = 503;
                    res.body = make_error_json("SERVICE_UNAVAILABLE",
                                               "Remote node manager not configured");
                    return res;
                }

                // Check if node exists
                auto existing = ctx->node_manager->get_node(node_id);
                if (!existing) {
                    res.add_header("Content-Type", "application/json");
                    res.code = 404;
                    res.body = make_error_json("NOT_FOUND", "Remote node not found");
                    return res;
                }

                auto result = ctx->node_manager->remove_node(node_id);
                if (!result.is_ok()) {
                    res.add_header("Content-Type", "application/json");
                    res.code = 500;
                    res.body = make_error_json("DELETE_FAILED", result.error().message);
                    return res;
                }

                res.code = 204;
                return res;
            });

    // POST /api/v1/remote-nodes/<nodeId>/verify - Verify node connectivity
    CROW_ROUTE(app, "/api/v1/remote-nodes/<string>/verify")
        .methods(crow::HTTPMethod::POST)(
            [ctx](const crow::request& /*req*/, const std::string& node_id) {
                crow::response res;
                res.add_header("Content-Type", "application/json");
                add_cors_headers(res, *ctx);

                if (!ctx->node_manager) {
                    res.code = 503;
                    res.body = make_error_json("SERVICE_UNAVAILABLE",
                                               "Remote node manager not configured");
                    return res;
                }

                // Check if node exists
                auto existing = ctx->node_manager->get_node(node_id);
                if (!existing) {
                    res.code = 404;
                    res.body = make_error_json("NOT_FOUND", "Remote node not found");
                    return res;
                }

                auto start = std::chrono::steady_clock::now();
                auto result = ctx->node_manager->verify_node(node_id);
                auto end = std::chrono::steady_clock::now();
                auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    end - start).count();

                if (result.is_ok()) {
                    std::ostringstream oss;
                    oss << R"({"success":true,"response_time_ms":)" << elapsed_ms << '}';
                    res.code = 200;
                    res.body = oss.str();
                } else {
                    std::ostringstream oss;
                    oss << R"({"success":false,"error":")"
                        << json_escape(result.error().message)
                        << R"(","response_time_ms":)" << elapsed_ms << '}';
                    res.code = 200;
                    res.body = oss.str();
                }
                return res;
            });

    // GET /api/v1/remote-nodes/<nodeId>/status - Get node status
    CROW_ROUTE(app, "/api/v1/remote-nodes/<string>/status")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& /*req*/, const std::string& node_id) {
                crow::response res;
                res.add_header("Content-Type", "application/json");
                add_cors_headers(res, *ctx);

                if (!ctx->node_manager) {
                    res.code = 503;
                    res.body = make_error_json("SERVICE_UNAVAILABLE",
                                               "Remote node manager not configured");
                    return res;
                }

                auto node = ctx->node_manager->get_node(node_id);
                if (!node) {
                    res.code = 404;
                    res.body = make_error_json("NOT_FOUND", "Remote node not found");
                    return res;
                }

                auto stats = ctx->node_manager->get_statistics(node_id);

                std::ostringstream oss;
                oss << R"({"status":")" << client::to_string(node->status) << '"';

                auto last_verified = format_timestamp(node->last_verified);
                if (!last_verified.empty()) {
                    oss << R"(,"last_verified":")" << last_verified << '"';
                }

                if (!node->last_error_message.empty()) {
                    oss << R"(,"last_error_message":")"
                        << json_escape(node->last_error_message) << '"';
                }

                oss << R"(,"total_connections":)" << stats.total_connections
                    << R"(,"active_connections":)" << stats.active_connections
                    << R"(,"successful_operations":)" << stats.successful_operations
                    << R"(,"failed_operations":)" << stats.failed_operations;

                if (stats.avg_response_time.count() > 0) {
                    oss << R"(,"avg_response_time_ms":)" << stats.avg_response_time.count();
                }

                oss << '}';

                res.code = 200;
                res.body = oss.str();
                return res;
            });

    // POST /api/v1/remote-nodes/<nodeId>/query - Query remote PACS
    // Note: Full implementation requires query_scu integration
    // This is a placeholder that returns NOT_IMPLEMENTED for now
    CROW_ROUTE(app, "/api/v1/remote-nodes/<string>/query")
        .methods(crow::HTTPMethod::POST)(
            [ctx](const crow::request& /*req*/, const std::string& node_id) {
                crow::response res;
                res.add_header("Content-Type", "application/json");
                add_cors_headers(res, *ctx);

                if (!ctx->node_manager) {
                    res.code = 503;
                    res.body = make_error_json("SERVICE_UNAVAILABLE",
                                               "Remote node manager not configured");
                    return res;
                }

                // Check if node exists
                auto node = ctx->node_manager->get_node(node_id);
                if (!node) {
                    res.code = 404;
                    res.body = make_error_json("NOT_FOUND", "Remote node not found");
                    return res;
                }

                // TODO: Implement query functionality with query_scu integration
                // This requires association management and proper DICOM query handling
                res.code = 501;
                res.body = make_error_json("NOT_IMPLEMENTED",
                    "Query functionality requires query_scu integration");
                return res;
            });

    // POST /api/v1/remote-nodes/<nodeId>/retrieve - Retrieve from remote PACS
    // Note: Full implementation requires job_manager (#537)
    // This is a placeholder that returns NOT_IMPLEMENTED for now
    CROW_ROUTE(app, "/api/v1/remote-nodes/<string>/retrieve")
        .methods(crow::HTTPMethod::POST)(
            [ctx](const crow::request& /*req*/, const std::string& node_id) {
                crow::response res;
                res.add_header("Content-Type", "application/json");
                add_cors_headers(res, *ctx);

                if (!ctx->node_manager) {
                    res.code = 503;
                    res.body = make_error_json("SERVICE_UNAVAILABLE",
                                               "Remote node manager not configured");
                    return res;
                }

                // Check if node exists
                auto node = ctx->node_manager->get_node(node_id);
                if (!node) {
                    res.code = 404;
                    res.body = make_error_json("NOT_FOUND", "Remote node not found");
                    return res;
                }

                // TODO: Implement retrieve functionality with job_manager (#537)
                // Retrieve operations should create a job and return job_id
                res.code = 501;
                res.body = make_error_json("NOT_IMPLEMENTED",
                    "Retrieve functionality requires job_manager (Issue #537)");
                return res;
            });
}

} // namespace pacs::web::endpoints
