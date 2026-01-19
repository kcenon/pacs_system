/**
 * @file routing_endpoints.cpp
 * @brief Routing rule management REST API endpoints implementation
 *
 * @see Issue #570 - Implement Routing Rules CRUD REST API
 * @see Issue #540 - Parent: Routing REST API & Storage SCP Integration
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

#include "pacs/client/routing_manager.hpp"
#include "pacs/client/routing_types.hpp"
#include "pacs/web/endpoints/routing_endpoints.hpp"
#include "pacs/web/endpoints/system_endpoints.hpp"
#include "pacs/web/rest_config.hpp"
#include "pacs/web/rest_types.hpp"

#include <chrono>
#include <sstream>
#include <vector>

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
 * @brief Convert job_priority to string
 */
std::string priority_to_string(client::job_priority priority) {
    switch (priority) {
        case client::job_priority::low: return "low";
        case client::job_priority::normal: return "normal";
        case client::job_priority::high: return "high";
        case client::job_priority::urgent: return "urgent";
        default: return "normal";
    }
}

/**
 * @brief Parse job_priority from string
 */
client::job_priority priority_from_string(const std::string& str) {
    if (str == "low") return client::job_priority::low;
    if (str == "high") return client::job_priority::high;
    if (str == "urgent") return client::job_priority::urgent;
    return client::job_priority::normal;
}

/**
 * @brief Convert routing_condition to JSON string
 */
std::string condition_to_json(const client::routing_condition& condition) {
    std::ostringstream oss;
    oss << R"({"field":")" << client::to_string(condition.match_field)
        << R"(","pattern":")" << json_escape(condition.pattern)
        << R"(","case_sensitive":)" << (condition.case_sensitive ? "true" : "false")
        << R"(,"negate":)" << (condition.negate ? "true" : "false")
        << '}';
    return oss.str();
}

/**
 * @brief Convert routing_action to JSON string
 */
std::string action_to_json(const client::routing_action& action) {
    std::ostringstream oss;
    oss << R"({"destination_node_id":")" << json_escape(action.destination_node_id)
        << R"(","priority":")" << priority_to_string(action.priority)
        << R"(","delay_minutes":)" << action.delay.count()
        << R"(,"delete_after_send":)" << (action.delete_after_send ? "true" : "false")
        << R"(,"notify_on_failure":)" << (action.notify_on_failure ? "true" : "false")
        << '}';
    return oss.str();
}

/**
 * @brief Convert routing_rule to JSON string
 */
std::string rule_to_json(const client::routing_rule& rule) {
    std::ostringstream oss;
    oss << R"({"rule_id":")" << json_escape(rule.rule_id)
        << R"(","name":")" << json_escape(rule.name)
        << R"(","description":")" << json_escape(rule.description)
        << R"(","enabled":)" << (rule.enabled ? "true" : "false")
        << R"(,"priority":)" << rule.priority;

    // Conditions
    oss << R"(,"conditions":[)";
    for (size_t i = 0; i < rule.conditions.size(); ++i) {
        if (i > 0) oss << ',';
        oss << condition_to_json(rule.conditions[i]);
    }
    oss << ']';

    // Actions
    oss << R"(,"actions":[)";
    for (size_t i = 0; i < rule.actions.size(); ++i) {
        if (i > 0) oss << ',';
        oss << action_to_json(rule.actions[i]);
    }
    oss << ']';

    // Schedule (optional)
    if (rule.schedule_cron.has_value()) {
        oss << R"(,"schedule_cron":")" << json_escape(*rule.schedule_cron) << '"';
    }

    // Statistics
    oss << R"(,"triggered_count":)" << rule.triggered_count
        << R"(,"success_count":)" << rule.success_count
        << R"(,"failure_count":)" << rule.failure_count;

    auto last_triggered = format_timestamp(rule.last_triggered);
    if (!last_triggered.empty()) {
        oss << R"(,"last_triggered":")" << last_triggered << '"';
    }

    auto created_at = format_timestamp(rule.created_at);
    if (!created_at.empty()) {
        oss << R"(,"created_at":")" << created_at << '"';
    }

    auto updated_at = format_timestamp(rule.updated_at);
    if (!updated_at.empty()) {
        oss << R"(,"updated_at":")" << updated_at << '"';
    }

    oss << '}';
    return oss.str();
}

/**
 * @brief Convert vector of routing_rules to JSON array string
 */
std::string rules_to_json(const std::vector<client::routing_rule>& rules,
                          size_t total_count) {
    std::ostringstream oss;
    oss << R"({"rules":[)";
    for (size_t i = 0; i < rules.size(); ++i) {
        if (i > 0) oss << ',';
        oss << rule_to_json(rules[i]);
    }
    oss << R"(],"total":)" << total_count << '}';
    return oss.str();
}

/**
 * @brief Get string value from JSON body (simple parser)
 */
std::string get_json_string(const std::string& body, const std::string& key) {
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
}

/**
 * @brief Get integer value from JSON body
 */
std::optional<int64_t> get_json_int(const std::string& body, const std::string& key) {
    std::string search = "\"" + key + "\":";
    auto pos = body.find(search);
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    pos += search.length();
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
}

/**
 * @brief Get boolean value from JSON body
 */
bool get_json_bool(const std::string& body, const std::string& key, bool default_val) {
    std::string search = "\"" + key + "\":";
    auto pos = body.find(search);
    if (pos == std::string::npos) {
        return default_val;
    }
    pos += search.length();
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
}

/**
 * @brief Parse conditions array from JSON body
 */
std::vector<client::routing_condition> parse_conditions(const std::string& body) {
    std::vector<client::routing_condition> conditions;

    // Find conditions array
    auto start = body.find("\"conditions\":[");
    if (start == std::string::npos) {
        return conditions;
    }
    start += 14; // skip "conditions":[

    auto end = body.find(']', start);
    if (end == std::string::npos) {
        return conditions;
    }

    std::string arr = body.substr(start, end - start);

    // Parse each condition object
    size_t obj_start = 0;
    while ((obj_start = arr.find('{', obj_start)) != std::string::npos) {
        auto obj_end = arr.find('}', obj_start);
        if (obj_end == std::string::npos) break;

        std::string obj = arr.substr(obj_start, obj_end - obj_start + 1);

        client::routing_condition cond;
        auto field_str = get_json_string(obj, "field");
        cond.match_field = client::routing_field_from_string(field_str);
        cond.pattern = get_json_string(obj, "pattern");
        cond.case_sensitive = get_json_bool(obj, "case_sensitive", false);
        cond.negate = get_json_bool(obj, "negate", false);

        if (!cond.pattern.empty()) {
            conditions.push_back(std::move(cond));
        }

        obj_start = obj_end + 1;
    }

    return conditions;
}

/**
 * @brief Parse actions array from JSON body
 */
std::vector<client::routing_action> parse_actions(const std::string& body) {
    std::vector<client::routing_action> actions;

    // Find actions array
    auto start = body.find("\"actions\":[");
    if (start == std::string::npos) {
        return actions;
    }
    start += 11; // skip "actions":[

    auto end = body.find(']', start);
    if (end == std::string::npos) {
        return actions;
    }

    std::string arr = body.substr(start, end - start);

    // Parse each action object
    size_t obj_start = 0;
    while ((obj_start = arr.find('{', obj_start)) != std::string::npos) {
        auto obj_end = arr.find('}', obj_start);
        if (obj_end == std::string::npos) break;

        std::string obj = arr.substr(obj_start, obj_end - obj_start + 1);

        client::routing_action action;
        action.destination_node_id = get_json_string(obj, "destination_node_id");
        auto priority_str = get_json_string(obj, "priority");
        action.priority = priority_from_string(priority_str);

        auto delay = get_json_int(obj, "delay_minutes");
        if (delay) {
            action.delay = std::chrono::minutes(*delay);
        }

        action.delete_after_send = get_json_bool(obj, "delete_after_send", false);
        action.notify_on_failure = get_json_bool(obj, "notify_on_failure", true);

        if (!action.destination_node_id.empty()) {
            actions.push_back(std::move(action));
        }

        obj_start = obj_end + 1;
    }

    return actions;
}

/**
 * @brief Parse routing rule from JSON body
 */
std::optional<client::routing_rule> parse_rule_from_json(const std::string& body,
                                                          std::string& error_message) {
    client::routing_rule rule;

    // Required: name
    rule.name = get_json_string(body, "name");
    if (rule.name.empty()) {
        error_message = "name is required";
        return std::nullopt;
    }

    // Optional fields
    rule.description = get_json_string(body, "description");
    rule.enabled = get_json_bool(body, "enabled", true);

    auto priority = get_json_int(body, "priority");
    if (priority) {
        rule.priority = static_cast<int>(*priority);
    }

    // Parse conditions
    rule.conditions = parse_conditions(body);
    if (rule.conditions.empty()) {
        error_message = "at least one condition is required";
        return std::nullopt;
    }

    // Parse actions
    rule.actions = parse_actions(body);
    if (rule.actions.empty()) {
        error_message = "at least one action is required";
        return std::nullopt;
    }

    // Optional schedule
    auto schedule = get_json_string(body, "schedule_cron");
    if (!schedule.empty()) {
        rule.schedule_cron = schedule;
    }

    return rule;
}

/**
 * @brief Parse rule IDs array from JSON body
 */
std::vector<std::string> parse_rule_ids(const std::string& body) {
    std::vector<std::string> ids;

    // Find rule_ids array
    auto start = body.find("\"rule_ids\":[");
    if (start == std::string::npos) {
        return ids;
    }
    start += 12; // skip "rule_ids":[

    auto end = body.find(']', start);
    if (end == std::string::npos) {
        return ids;
    }

    std::string arr = body.substr(start, end - start);

    // Parse each string
    size_t str_start = 0;
    while ((str_start = arr.find('"', str_start)) != std::string::npos) {
        ++str_start;
        auto str_end = arr.find('"', str_start);
        if (str_end == std::string::npos) break;

        std::string id = arr.substr(str_start, str_end - str_start);
        if (!id.empty()) {
            ids.push_back(std::move(id));
        }

        str_start = str_end + 1;
    }

    return ids;
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
void register_routing_endpoints_impl(crow::SimpleApp& app,
                                     std::shared_ptr<rest_server_context> ctx) {
    // GET /api/v1/routing/rules - List routing rules (paginated)
    CROW_ROUTE(app, "/api/v1/routing/rules")
        .methods(crow::HTTPMethod::GET)([ctx](const crow::request& req) {
            crow::response res;
            res.add_header("Content-Type", "application/json");
            add_cors_headers(res, *ctx);

            if (!ctx->routing_manager) {
                res.code = 503;
                res.body = make_error_json("SERVICE_UNAVAILABLE",
                                           "Routing manager not configured");
                return res;
            }

            // Parse pagination
            auto [limit, offset] = parse_pagination(req);

            // Filter by enabled if provided
            auto enabled_param = req.url_params.get("enabled");
            std::vector<client::routing_rule> rules;

            if (enabled_param) {
                std::string enabled_str(enabled_param);
                if (enabled_str == "true") {
                    rules = ctx->routing_manager->list_enabled_rules();
                } else {
                    rules = ctx->routing_manager->list_rules();
                }
            } else {
                rules = ctx->routing_manager->list_rules();
            }

            size_t total_count = rules.size();

            // Apply pagination
            std::vector<client::routing_rule> paginated;
            for (size_t i = offset; i < rules.size() && paginated.size() < limit; ++i) {
                paginated.push_back(rules[i]);
            }

            res.code = 200;
            res.body = rules_to_json(paginated, total_count);
            return res;
        });

    // POST /api/v1/routing/rules - Create a new routing rule
    CROW_ROUTE(app, "/api/v1/routing/rules")
        .methods(crow::HTTPMethod::POST)([ctx](const crow::request& req) {
            crow::response res;
            res.add_header("Content-Type", "application/json");
            add_cors_headers(res, *ctx);

            if (!ctx->routing_manager) {
                res.code = 503;
                res.body = make_error_json("SERVICE_UNAVAILABLE",
                                           "Routing manager not configured");
                return res;
            }

            std::string error_message;
            auto rule_opt = parse_rule_from_json(req.body, error_message);
            if (!rule_opt) {
                res.code = 400;
                res.body = make_error_json("INVALID_REQUEST", error_message);
                return res;
            }

            auto& rule = *rule_opt;

            // Generate rule_id if not provided
            if (rule.rule_id.empty()) {
                auto now = std::chrono::system_clock::now();
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch()).count();
                rule.rule_id = "rule_" + std::to_string(ms);
            }

            auto result = ctx->routing_manager->add_rule(rule);
            if (!result.is_ok()) {
                res.code = 409;
                res.body = make_error_json("CONFLICT", result.error().message);
                return res;
            }

            // Retrieve the created rule to get full details
            auto created_rule = ctx->routing_manager->get_rule(rule.rule_id);
            if (!created_rule) {
                res.code = 201;
                res.body = rule_to_json(rule);
                return res;
            }

            res.code = 201;
            res.body = rule_to_json(*created_rule);
            return res;
        });

    // GET /api/v1/routing/rules/<ruleId> - Get a specific routing rule
    CROW_ROUTE(app, "/api/v1/routing/rules/<string>")
        .methods(crow::HTTPMethod::GET)(
            [ctx](const crow::request& /*req*/, const std::string& rule_id) {
                crow::response res;
                res.add_header("Content-Type", "application/json");
                add_cors_headers(res, *ctx);

                if (!ctx->routing_manager) {
                    res.code = 503;
                    res.body = make_error_json("SERVICE_UNAVAILABLE",
                                               "Routing manager not configured");
                    return res;
                }

                auto rule = ctx->routing_manager->get_rule(rule_id);
                if (!rule) {
                    res.code = 404;
                    res.body = make_error_json("NOT_FOUND", "Routing rule not found");
                    return res;
                }

                res.code = 200;
                res.body = rule_to_json(*rule);
                return res;
            });

    // PUT /api/v1/routing/rules/<ruleId> - Update a routing rule
    CROW_ROUTE(app, "/api/v1/routing/rules/<string>")
        .methods(crow::HTTPMethod::PUT)(
            [ctx](const crow::request& req, const std::string& rule_id) {
                crow::response res;
                res.add_header("Content-Type", "application/json");
                add_cors_headers(res, *ctx);

                if (!ctx->routing_manager) {
                    res.code = 503;
                    res.body = make_error_json("SERVICE_UNAVAILABLE",
                                               "Routing manager not configured");
                    return res;
                }

                // Check if rule exists
                auto existing = ctx->routing_manager->get_rule(rule_id);
                if (!existing) {
                    res.code = 404;
                    res.body = make_error_json("NOT_FOUND", "Routing rule not found");
                    return res;
                }

                std::string error_message;
                auto rule_opt = parse_rule_from_json(req.body, error_message);
                if (!rule_opt) {
                    res.code = 400;
                    res.body = make_error_json("INVALID_REQUEST", error_message);
                    return res;
                }

                auto& rule = *rule_opt;
                rule.rule_id = rule_id; // Preserve the rule_id from URL

                auto result = ctx->routing_manager->update_rule(rule);
                if (!result.is_ok()) {
                    res.code = 500;
                    res.body = make_error_json("UPDATE_FAILED", result.error().message);
                    return res;
                }

                // Retrieve the updated rule
                auto updated_rule = ctx->routing_manager->get_rule(rule_id);
                if (!updated_rule) {
                    res.code = 200;
                    res.body = rule_to_json(rule);
                    return res;
                }

                res.code = 200;
                res.body = rule_to_json(*updated_rule);
                return res;
            });

    // DELETE /api/v1/routing/rules/<ruleId> - Delete a routing rule
    CROW_ROUTE(app, "/api/v1/routing/rules/<string>")
        .methods(crow::HTTPMethod::DELETE)(
            [ctx](const crow::request& /*req*/, const std::string& rule_id) {
                crow::response res;
                add_cors_headers(res, *ctx);

                if (!ctx->routing_manager) {
                    res.add_header("Content-Type", "application/json");
                    res.code = 503;
                    res.body = make_error_json("SERVICE_UNAVAILABLE",
                                               "Routing manager not configured");
                    return res;
                }

                // Check if rule exists
                auto existing = ctx->routing_manager->get_rule(rule_id);
                if (!existing) {
                    res.add_header("Content-Type", "application/json");
                    res.code = 404;
                    res.body = make_error_json("NOT_FOUND", "Routing rule not found");
                    return res;
                }

                auto result = ctx->routing_manager->remove_rule(rule_id);
                if (!result.is_ok()) {
                    res.add_header("Content-Type", "application/json");
                    res.code = 500;
                    res.body = make_error_json("DELETE_FAILED", result.error().message);
                    return res;
                }

                res.code = 204;
                return res;
            });

    // POST /api/v1/routing/rules/reorder - Reorder routing rules
    CROW_ROUTE(app, "/api/v1/routing/rules/reorder")
        .methods(crow::HTTPMethod::POST)([ctx](const crow::request& req) {
            crow::response res;
            res.add_header("Content-Type", "application/json");
            add_cors_headers(res, *ctx);

            if (!ctx->routing_manager) {
                res.code = 503;
                res.body = make_error_json("SERVICE_UNAVAILABLE",
                                           "Routing manager not configured");
                return res;
            }

            auto rule_ids = parse_rule_ids(req.body);
            if (rule_ids.empty()) {
                res.code = 400;
                res.body = make_error_json("INVALID_REQUEST",
                                           "rule_ids array is required");
                return res;
            }

            auto result = ctx->routing_manager->reorder_rules(rule_ids);
            if (!result.is_ok()) {
                res.code = 400;
                res.body = make_error_json("REORDER_FAILED", result.error().message);
                return res;
            }

            res.code = 200;
            res.body = R"({"status":"success"})";
            return res;
        });
}

} // namespace pacs::web::endpoints
