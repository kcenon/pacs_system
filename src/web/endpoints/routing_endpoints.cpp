// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file routing_endpoints.cpp
 * @brief Routing rule management REST API endpoints implementation
 *
 * @see Issue #570 - Implement Routing Rules CRUD REST API
 * @see Issue #571 - Implement Routing Control REST API
 * @see Issue #572 - Implement Routing Testing API & Storage SCP Integration
 * @see Issue #540 - Parent: Routing REST API & Storage SCP Integration
 * @copyright Copyright (c) 2025
 * @license MIT
 */

// IMPORTANT: Include Crow FIRST before any PACS headers to avoid forward
// declaration conflicts
#include "crow.h"

#include "../rest_app.h"

// Workaround for Windows: DELETE is defined as a macro in <winnt.h>
// which conflicts with crow::HTTPMethod::DELETE
#ifdef DELETE
#undef DELETE
#endif

#include "kcenon/pacs/client/routing_manager.h"
#include "kcenon/pacs/client/routing_types.h"
#include "kcenon/pacs/core/dicom_dataset.h"
#include "kcenon/pacs/core/dicom_tag_constants.h"
#include "kcenon/pacs/encoding/vr_type.h"
#include "kcenon/pacs/web/endpoints/routing_endpoints.h"
#include "kcenon/pacs/web/endpoints/system_endpoints.h"
#include "kcenon/pacs/web/rest_config.h"
#include "kcenon/pacs/web/rest_types.h"

#include <chrono>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace kcenon::pacs::web::endpoints {

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

/// Whether parsed JSON is an object. A failed parse throws on t(), so validity is tested first.
bool is_object(const crow::json::rvalue &json) {
    return json && json.t() == crow::json::type::Object;
}

/// A string member of a JSON object; absent or of another type reads as empty.
std::string string_member(const crow::json::rvalue &object, const std::string &key) {
    if (!is_object(object) || !object.has(key) ||
        object[key].t() != crow::json::type::String) {
        return "";
    }
    return std::string(object[key].s());
}

std::optional<int64_t> int_member(const crow::json::rvalue &object, const std::string &key) {
    if (!is_object(object) || !object.has(key) ||
        object[key].t() != crow::json::type::Number) {
        return std::nullopt;
    }
    return object[key].i();
}

bool bool_member(const crow::json::rvalue &object, const std::string &key, bool default_val) {
    if (!is_object(object) || !object.has(key)) {
        return default_val;
    }
    const auto type = object[key].t();
    if (type == crow::json::type::True) {
        return true;
    }
    if (type == crow::json::type::False) {
        return false;
    }
    return default_val;
}

std::string get_json_string(const std::string& body, const std::string& key) {
    const auto json = crow::json::load(body);
    return string_member(json, key);
}

std::optional<int64_t> get_json_int(const std::string& body, const std::string& key) {
    const auto json = crow::json::load(body);
    return int_member(json, key);
}

bool get_json_bool(const std::string& body, const std::string& key, bool default_val) {
    const auto json = crow::json::load(body);
    return bool_member(json, key, default_val);
}

/**
 * @brief Parse conditions array from JSON body
 */
std::vector<client::routing_condition> parse_conditions(const std::string& body) {
    std::vector<client::routing_condition> conditions;
    const auto json = crow::json::load(body);
    if (!is_object(json) || !json.has("conditions") || json["conditions"].t() != crow::json::type::List) {
        return conditions;
    }
    for (const auto& item : json["conditions"]) {
        client::routing_condition cond;
        cond.match_field = client::routing_field_from_string(string_member(item, "field"));
        cond.pattern = string_member(item, "pattern");
        cond.case_sensitive = bool_member(item, "case_sensitive", false);
        cond.negate = bool_member(item, "negate", false);
        if (!cond.pattern.empty()) {
            conditions.push_back(std::move(cond));
        }
    }
    return conditions;
}

/**
 * @brief Parse actions array from JSON body
 */
std::vector<client::routing_action> parse_actions(const std::string& body) {
    std::vector<client::routing_action> actions;
    const auto json = crow::json::load(body);
    if (!is_object(json) || !json.has("actions") || json["actions"].t() != crow::json::type::List) {
        return actions;
    }
    for (const auto& item : json["actions"]) {
        client::routing_action action;
        action.destination_node_id = string_member(item, "destination_node_id");
        action.priority = priority_from_string(string_member(item, "priority"));
        if (auto delay = int_member(item, "delay_minutes")) {
            action.delay = std::chrono::minutes(*delay);
        }
        action.delete_after_send = bool_member(item, "delete_after_send", false);
        action.notify_on_failure = bool_member(item, "notify_on_failure", true);
        if (!action.destination_node_id.empty()) {
            actions.push_back(std::move(action));
        }
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
    const auto json = crow::json::load(body);
    if (!is_object(json) || !json.has("rule_ids") || json["rule_ids"].t() != crow::json::type::List) {
        return ids;
    }
    for (const auto& item : json["rule_ids"]) {
        if (item.t() == crow::json::type::String && !std::string(item.s()).empty()) {
            ids.emplace_back(item.s());
        }
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

/**
 * @brief Parse dataset object from JSON body for routing test
 *
 * Extracts the "dataset" field and creates a dicom_dataset populated
 * with the provided DICOM attribute values.
 */
core::dicom_dataset parse_test_dataset(const std::string& body) {
    core::dicom_dataset dataset;

    const auto json = crow::json::load(body);
    if (!is_object(json) || !json.has("dataset") || json["dataset"].t() != crow::json::type::Object) {
        return dataset;
    }
    const auto& dataset_json = json["dataset"];

    // Map of JSON field names to DICOM tags
    // These match the routing_field enum values in routing_types.hpp
    struct FieldMapping {
        const char* json_key;
        core::dicom_tag tag;
    };

    static const FieldMapping field_mappings[] = {
        {"modality", core::tags::modality},
        {"station_ae", core::tags::station_name},
        {"institution", core::tags::institution_name},
        {"department", core::dicom_tag{0x0008, 0x1040}},  // Institutional Department
        {"referring_physician", core::tags::referring_physician_name},
        {"study_description", core::tags::study_description},
        {"series_description", core::tags::series_description},
        {"body_part", core::dicom_tag{0x0018, 0x0015}},  // Body Part Examined
        {"patient_id_pattern", core::tags::patient_id},
        {"patient_id", core::tags::patient_id},
        {"sop_class_uid", core::tags::sop_class_uid},
    };

    for (const auto& mapping : field_mappings) {
        std::string value = string_member(dataset_json, mapping.json_key);
        if (!value.empty()) {
            dataset.set_string(mapping.tag, encoding::vr_type::LO, value);
        }
    }

    return dataset;
}

/**
 * @brief Convert test result with multiple matched rules to JSON
 */
std::string test_result_to_json(
    bool matched,
    const std::vector<std::pair<std::string, std::vector<client::routing_action>>>& matches,
    const std::shared_ptr<client::routing_manager>& routing_manager) {

    std::ostringstream oss;
    oss << R"({"matched":)" << (matched ? "true" : "false");

    oss << R"(,"matched_rules":[)";
    bool first = true;
    for (const auto& [rule_id, actions] : matches) {
        if (!first) oss << ',';
        first = false;

        // Get rule name
        std::string rule_name;
        auto rule = routing_manager->get_rule(rule_id);
        if (rule) {
            rule_name = rule->name;
        }

        oss << R"({"rule_id":")" << json_escape(rule_id)
            << R"(","rule_name":")" << json_escape(rule_name)
            << R"(","actions":[)";

        bool first_action = true;
        for (const auto& action : actions) {
            if (!first_action) oss << ',';
            first_action = false;
            oss << action_to_json(action);
        }

        oss << "]}";
    }
    oss << "]}";

    return oss.str();
}

/**
 * @brief Convert single rule test result to JSON
 */
std::string single_rule_test_to_json(bool matched,
                                     const std::vector<client::routing_action>& actions) {
    std::ostringstream oss;
    oss << R"({"matched":)" << (matched ? "true" : "false")
        << R"(,"actions":[)";

    bool first = true;
    for (const auto& action : actions) {
        if (!first) oss << ',';
        first = false;
        oss << action_to_json(action);
    }

    oss << "]}";
    return oss.str();
}

} // namespace

// Internal implementation function called from rest_server.cpp
void register_routing_endpoints_impl(rest_app& app,
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

    // POST /api/v1/routing/enable - Enable routing globally
    CROW_ROUTE(app, "/api/v1/routing/enable")
        .methods(crow::HTTPMethod::POST)([ctx](const crow::request& /*req*/) {
            crow::response res;
            res.add_header("Content-Type", "application/json");
            add_cors_headers(res, *ctx);

            if (!ctx->routing_manager) {
                res.code = 503;
                res.body = make_error_json("SERVICE_UNAVAILABLE",
                                           "Routing manager not configured");
                return res;
            }

            ctx->routing_manager->enable();

            res.code = 200;
            res.body = R"({"enabled":true})";
            return res;
        });

    // POST /api/v1/routing/disable - Disable routing globally
    CROW_ROUTE(app, "/api/v1/routing/disable")
        .methods(crow::HTTPMethod::POST)([ctx](const crow::request& /*req*/) {
            crow::response res;
            res.add_header("Content-Type", "application/json");
            add_cors_headers(res, *ctx);

            if (!ctx->routing_manager) {
                res.code = 503;
                res.body = make_error_json("SERVICE_UNAVAILABLE",
                                           "Routing manager not configured");
                return res;
            }

            ctx->routing_manager->disable();

            res.code = 200;
            res.body = R"({"enabled":false})";
            return res;
        });

    // GET /api/v1/routing/status - Get routing status and statistics
    CROW_ROUTE(app, "/api/v1/routing/status")
        .methods(crow::HTTPMethod::GET)([ctx](const crow::request& /*req*/) {
            crow::response res;
            res.add_header("Content-Type", "application/json");
            add_cors_headers(res, *ctx);

            if (!ctx->routing_manager) {
                res.code = 503;
                res.body = make_error_json("SERVICE_UNAVAILABLE",
                                           "Routing manager not configured");
                return res;
            }

            bool enabled = ctx->routing_manager->is_enabled();
            auto all_rules = ctx->routing_manager->list_rules();
            auto enabled_rules = ctx->routing_manager->list_enabled_rules();
            auto stats = ctx->routing_manager->get_statistics();

            std::ostringstream oss;
            oss << R"({"enabled":)" << (enabled ? "true" : "false")
                << R"(,"rules_count":)" << all_rules.size()
                << R"(,"enabled_rules_count":)" << enabled_rules.size()
                << R"(,"statistics":{)"
                << R"("total_evaluated":)" << stats.total_evaluated
                << R"(,"total_matched":)" << stats.total_matched
                << R"(,"total_forwarded":)" << stats.total_forwarded
                << R"(,"total_failed":)" << stats.total_failed
                << R"(}})";

            res.code = 200;
            res.body = oss.str();
            return res;
        });

    // POST /api/v1/routing/test - Test all rules against a dataset (dry run)
    CROW_ROUTE(app, "/api/v1/routing/test")
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

            // Parse dataset from request body
            auto dataset = parse_test_dataset(req.body);
            if (dataset.empty()) {
                res.code = 400;
                res.body = make_error_json("INVALID_REQUEST",
                                           "dataset object is required");
                return res;
            }

            // Evaluate rules against the dataset
            // Note: evaluate_with_rule_ids requires routing to be enabled,
            // so we use a direct evaluation approach for testing
            auto matches = ctx->routing_manager->evaluate_with_rule_ids(dataset);

            bool matched = !matches.empty();
            res.code = 200;
            res.body = test_result_to_json(matched, matches, ctx->routing_manager);
            return res;
        });

    // POST /api/v1/routing/rules/<ruleId>/test - Test specific rule against a dataset
    CROW_ROUTE(app, "/api/v1/routing/rules/<string>/test")
        .methods(crow::HTTPMethod::POST)(
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
                auto rule = ctx->routing_manager->get_rule(rule_id);
                if (!rule) {
                    res.code = 404;
                    res.body = make_error_json("NOT_FOUND", "Routing rule not found");
                    return res;
                }

                // Parse dataset from request body
                auto dataset = parse_test_dataset(req.body);
                if (dataset.empty()) {
                    res.code = 400;
                    res.body = make_error_json("INVALID_REQUEST",
                                               "dataset object is required");
                    return res;
                }

                // Test only this specific rule
                // We need to manually check if all conditions match
                bool all_match = !rule->conditions.empty();
                for (const auto& condition : rule->conditions) {
                    // Get the field value from dataset
                    std::string value;
                    switch (condition.match_field) {
                        case client::routing_field::modality:
                            value = dataset.get_string(core::tags::modality);
                            break;
                        case client::routing_field::station_ae:
                            value = dataset.get_string(core::tags::station_name);
                            break;
                        case client::routing_field::institution:
                            value = dataset.get_string(core::tags::institution_name);
                            break;
                        case client::routing_field::department:
                            value = dataset.get_string(core::dicom_tag{0x0008, 0x1040});
                            break;
                        case client::routing_field::referring_physician:
                            value = dataset.get_string(core::tags::referring_physician_name);
                            break;
                        case client::routing_field::study_description:
                            value = dataset.get_string(core::tags::study_description);
                            break;
                        case client::routing_field::series_description:
                            value = dataset.get_string(core::tags::series_description);
                            break;
                        case client::routing_field::body_part:
                            value = dataset.get_string(core::dicom_tag{0x0018, 0x0015});
                            break;
                        case client::routing_field::patient_id_pattern:
                            value = dataset.get_string(core::tags::patient_id);
                            break;
                        case client::routing_field::sop_class_uid:
                            value = dataset.get_string(core::tags::sop_class_uid);
                            break;
                        default:
                            break;
                    }

                    // Simple wildcard matching
                    auto match_wildcard = [](std::string_view pattern,
                                            std::string_view val,
                                            bool case_sensitive) -> bool {
                        auto to_lower = [](std::string_view s) {
                            std::string result;
                            result.reserve(s.size());
                            for (char c : s) {
                                result += static_cast<char>(std::tolower(
                                    static_cast<unsigned char>(c)));
                            }
                            return result;
                        };

                        std::string pat_str = case_sensitive ?
                            std::string(pattern) : to_lower(pattern);
                        std::string val_str = case_sensitive ?
                            std::string(val) : to_lower(val);

                        const char* p = pat_str.c_str();
                        const char* v = val_str.c_str();
                        const char* star_p = nullptr;
                        const char* star_v = nullptr;

                        while (*v) {
                            if (*p == '*') {
                                star_p = p++;
                                star_v = v;
                            } else if (*p == '?' || *p == *v) {
                                ++p;
                                ++v;
                            } else if (star_p) {
                                p = star_p + 1;
                                v = ++star_v;
                            } else {
                                return false;
                            }
                        }

                        while (*p == '*') {
                            ++p;
                        }

                        return *p == '\0';
                    };

                    bool matched = match_wildcard(condition.pattern, value,
                                                  condition.case_sensitive);
                    if (condition.negate) {
                        matched = !matched;
                    }

                    if (!matched) {
                        all_match = false;
                        break;
                    }
                }

                res.code = 200;
                if (all_match) {
                    res.body = single_rule_test_to_json(true, rule->actions);
                } else {
                    res.body = single_rule_test_to_json(false, {});
                }
                return res;
            });
}

} // namespace kcenon::pacs::web::endpoints
