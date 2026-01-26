/**
 * @file routing_repository.cpp
 * @brief Implementation of the routing repository for rule persistence
 *
 * @see Issue #539 - Implement Routing Manager for Auto-Forwarding
 * @see Issue #530 - PACS Client System Support (Parent Epic)
 */

#include "pacs/storage/routing_repository.hpp"

#ifdef PACS_WITH_DATABASE_SYSTEM


#include <sqlite3.h>

#include <chrono>
#include <cstring>
#include <sstream>

namespace pacs::storage {

// =============================================================================
// Helper Functions
// =============================================================================

namespace {

/// Convert time_point to ISO8601 string
[[nodiscard]] std::string to_timestamp_string(
    std::chrono::system_clock::time_point tp) {
    if (tp == std::chrono::system_clock::time_point{}) {
        return "";
    }
    auto time = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &time);
#else
    gmtime_r(&time, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}

/// Parse ISO8601 string to time_point
[[nodiscard]] std::chrono::system_clock::time_point from_timestamp_string(
    const char* str) {
    if (!str || str[0] == '\0') {
        return {};
    }
    std::tm tm{};
    if (std::sscanf(str, "%d-%d-%d %d:%d:%d",
                    &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                    &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 6) {
        return {};
    }
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;
#ifdef _WIN32
    auto time = _mkgmtime(&tm);
#else
    auto time = timegm(&tm);
#endif
    return std::chrono::system_clock::from_time_t(time);
}

/// Convert optional time_point to timestamp string (unused - kept for consistency)
// [[nodiscard]] std::string to_optional_timestamp(
//     const std::optional<std::chrono::system_clock::time_point>& tp) {
//     if (!tp.has_value()) {
//         return "";
//     }
//     return to_timestamp_string(tp.value());
// }

/// Parse timestamp string to optional time_point
[[nodiscard]] std::optional<std::chrono::system_clock::time_point>
from_optional_timestamp(const char* str) {
    if (!str || str[0] == '\0') {
        return std::nullopt;
    }
    auto tp = from_timestamp_string(str);
    if (tp == std::chrono::system_clock::time_point{}) {
        return std::nullopt;
    }
    return tp;
}

/// Get text column safely (returns empty string if NULL)
[[nodiscard]] std::string get_text_column(sqlite3_stmt* stmt, int col) {
    auto text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, col));
    return text ? text : "";
}

/// Get int column with default
[[nodiscard]] int get_int_column(sqlite3_stmt* stmt, int col, int default_val = 0) {
    if (sqlite3_column_type(stmt, col) == SQLITE_NULL) {
        return default_val;
    }
    return sqlite3_column_int(stmt, col);
}

/// Get int64 column with default
[[nodiscard]] int64_t get_int64_column(sqlite3_stmt* stmt, int col, int64_t default_val = 0) {
    if (sqlite3_column_type(stmt, col) == SQLITE_NULL) {
        return default_val;
    }
    return sqlite3_column_int64(stmt, col);
}

/// Get optional string column
[[nodiscard]] std::optional<std::string> get_optional_text(sqlite3_stmt* stmt, int col) {
    if (sqlite3_column_type(stmt, col) == SQLITE_NULL) {
        return std::nullopt;
    }
    auto text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, col));
    return text ? std::optional<std::string>{text} : std::nullopt;
}

/// Bind optional string
void bind_optional_text(sqlite3_stmt* stmt, int idx, const std::optional<std::string>& value) {
    if (value.has_value()) {
        sqlite3_bind_text(stmt, idx, value->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, idx);
    }
}

/// Bind optional timestamp
void bind_optional_timestamp(
    sqlite3_stmt* stmt,
    int idx,
    const std::optional<std::chrono::system_clock::time_point>& tp) {
    if (tp.has_value()) {
        auto str = to_timestamp_string(tp.value());
        sqlite3_bind_text(stmt, idx, str.c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, idx);
    }
}

/// Escape string for JSON
[[nodiscard]] std::string escape_json_string(const std::string& str) {
    std::ostringstream oss;
    for (char c : str) {
        switch (c) {
            case '"':  oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b"; break;
            case '\f': oss << "\\f"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:   oss << c; break;
        }
    }
    return oss.str();
}

/// Unescape JSON string
[[nodiscard]] std::string unescape_json_string(std::string_view str) {
    std::string result;
    result.reserve(str.size());
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '\\' && i + 1 < str.size()) {
            ++i;
            switch (str[i]) {
                case '"':  result += '"'; break;
                case '\\': result += '\\'; break;
                case 'b':  result += '\b'; break;
                case 'f':  result += '\f'; break;
                case 'n':  result += '\n'; break;
                case 'r':  result += '\r'; break;
                case 't':  result += '\t'; break;
                default:   result += str[i]; break;
            }
        } else {
            result += str[i];
        }
    }
    return result;
}

/// Find next JSON string value
[[nodiscard]] std::pair<std::string, size_t> extract_json_string(
    std::string_view json, size_t pos) {
    auto start = json.find('"', pos);
    if (start == std::string_view::npos) return {"", std::string_view::npos};

    size_t end = start + 1;
    while (end < json.size()) {
        if (json[end] == '\\' && end + 1 < json.size()) {
            end += 2;
        } else if (json[end] == '"') {
            break;
        } else {
            ++end;
        }
    }

    if (end >= json.size()) return {"", std::string_view::npos};

    auto value = unescape_json_string(json.substr(start + 1, end - start - 1));
    return {value, end + 1};
}

}  // namespace

// =============================================================================
// JSON Serialization for Conditions
// =============================================================================

std::string routing_repository::serialize_conditions(
    const std::vector<client::routing_condition>& conditions) {
    if (conditions.empty()) return "[]";

    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < conditions.size(); ++i) {
        if (i > 0) oss << ",";
        const auto& cond = conditions[i];
        oss << "{";
        oss << "\"field\":\"" << client::to_string(cond.match_field) << "\",";
        oss << "\"pattern\":\"" << escape_json_string(cond.pattern) << "\",";
        oss << "\"case_sensitive\":" << (cond.case_sensitive ? "true" : "false") << ",";
        oss << "\"negate\":" << (cond.negate ? "true" : "false");
        oss << "}";
    }
    oss << "]";
    return oss.str();
}

std::vector<client::routing_condition> routing_repository::deserialize_conditions(
    std::string_view json) {
    std::vector<client::routing_condition> result;
    if (json.empty() || json == "[]") return result;

    size_t pos = 0;
    while (pos < json.size()) {
        auto obj_start = json.find('{', pos);
        if (obj_start == std::string_view::npos) break;

        auto obj_end = json.find('}', obj_start);
        if (obj_end == std::string_view::npos) break;

        auto obj = json.substr(obj_start, obj_end - obj_start + 1);

        client::routing_condition cond;

        // Parse field
        auto field_pos = obj.find("\"field\"");
        if (field_pos != std::string_view::npos) {
            auto [field_value, next] = extract_json_string(obj, field_pos + 7);
            cond.match_field = client::routing_field_from_string(field_value);
        }

        // Parse pattern
        auto pattern_pos = obj.find("\"pattern\"");
        if (pattern_pos != std::string_view::npos) {
            auto [pattern_value, next] = extract_json_string(obj, pattern_pos + 9);
            cond.pattern = pattern_value;
        }

        // Parse case_sensitive
        auto case_pos = obj.find("\"case_sensitive\"");
        if (case_pos != std::string_view::npos) {
            cond.case_sensitive = (obj.find("true", case_pos) != std::string_view::npos &&
                                   obj.find("true", case_pos) < obj.find(',', case_pos));
        }

        // Parse negate
        auto negate_pos = obj.find("\"negate\"");
        if (negate_pos != std::string_view::npos) {
            cond.negate = (obj.find("true", negate_pos) != std::string_view::npos);
        }

        result.push_back(std::move(cond));
        pos = obj_end + 1;
    }

    return result;
}

// =============================================================================
// JSON Serialization for Actions
// =============================================================================

std::string routing_repository::serialize_actions(
    const std::vector<client::routing_action>& actions) {
    if (actions.empty()) return "[]";

    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < actions.size(); ++i) {
        if (i > 0) oss << ",";
        const auto& action = actions[i];
        oss << "{";
        oss << "\"destination\":\"" << escape_json_string(action.destination_node_id) << "\",";
        oss << "\"priority\":\"" << client::to_string(action.priority) << "\",";
        oss << "\"delay_minutes\":" << action.delay.count() << ",";
        oss << "\"delete_after_send\":" << (action.delete_after_send ? "true" : "false") << ",";
        oss << "\"notify_on_failure\":" << (action.notify_on_failure ? "true" : "false");
        oss << "}";
    }
    oss << "]";
    return oss.str();
}

std::vector<client::routing_action> routing_repository::deserialize_actions(
    std::string_view json) {
    std::vector<client::routing_action> result;
    if (json.empty() || json == "[]") return result;

    size_t pos = 0;
    while (pos < json.size()) {
        auto obj_start = json.find('{', pos);
        if (obj_start == std::string_view::npos) break;

        auto obj_end = json.find('}', obj_start);
        if (obj_end == std::string_view::npos) break;

        auto obj = json.substr(obj_start, obj_end - obj_start + 1);

        client::routing_action action;

        // Parse destination
        auto dest_pos = obj.find("\"destination\"");
        if (dest_pos != std::string_view::npos) {
            auto [dest_value, next] = extract_json_string(obj, dest_pos + 13);
            action.destination_node_id = dest_value;
        }

        // Parse priority
        auto prio_pos = obj.find("\"priority\"");
        if (prio_pos != std::string_view::npos) {
            auto [prio_value, next] = extract_json_string(obj, prio_pos + 10);
            action.priority = client::job_priority_from_string(prio_value);
        }

        // Parse delay_minutes
        auto delay_pos = obj.find("\"delay_minutes\"");
        if (delay_pos != std::string_view::npos) {
            auto colon = obj.find(':', delay_pos);
            if (colon != std::string_view::npos) {
                int minutes = 0;
                std::sscanf(obj.data() + colon + 1, "%d", &minutes);
                action.delay = std::chrono::minutes{minutes};
            }
        }

        // Parse delete_after_send
        auto delete_pos = obj.find("\"delete_after_send\"");
        if (delete_pos != std::string_view::npos) {
            action.delete_after_send = (obj.find("true", delete_pos) != std::string_view::npos &&
                                        obj.find("true", delete_pos) < obj.find(',', delete_pos + 20));
        }

        // Parse notify_on_failure
        auto notify_pos = obj.find("\"notify_on_failure\"");
        if (notify_pos != std::string_view::npos) {
            action.notify_on_failure = (obj.find("true", notify_pos) != std::string_view::npos);
        }

        result.push_back(std::move(action));
        pos = obj_end + 1;
    }

    return result;
}

// =============================================================================
// Construction / Destruction
// =============================================================================

routing_repository::routing_repository(sqlite3* db) : db_(db) {}

routing_repository::~routing_repository() = default;

routing_repository::routing_repository(routing_repository&&) noexcept = default;

auto routing_repository::operator=(routing_repository&&) noexcept -> routing_repository& = default;

// =============================================================================
// CRUD Operations
// =============================================================================

VoidResult routing_repository::save(const client::routing_rule& rule) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "routing_repository"});
    }

    static constexpr const char* sql = R"(
        INSERT INTO routing_rules (
            rule_id, name, description, enabled, priority,
            conditions_json, actions_json,
            schedule_cron, effective_from, effective_until,
            triggered_count, success_count, failure_count,
            last_triggered, created_at, updated_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(rule_id) DO UPDATE SET
            name = excluded.name,
            description = excluded.description,
            enabled = excluded.enabled,
            priority = excluded.priority,
            conditions_json = excluded.conditions_json,
            actions_json = excluded.actions_json,
            schedule_cron = excluded.schedule_cron,
            effective_from = excluded.effective_from,
            effective_until = excluded.effective_until,
            updated_at = CURRENT_TIMESTAMP
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "routing_repository"});
    }

    int idx = 1;
    sqlite3_bind_text(stmt, idx++, rule.rule_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, rule.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, rule.description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, idx++, rule.enabled ? 1 : 0);
    sqlite3_bind_int(stmt, idx++, rule.priority);

    auto conditions_json = serialize_conditions(rule.conditions);
    sqlite3_bind_text(stmt, idx++, conditions_json.c_str(), -1, SQLITE_TRANSIENT);

    auto actions_json = serialize_actions(rule.actions);
    sqlite3_bind_text(stmt, idx++, actions_json.c_str(), -1, SQLITE_TRANSIENT);

    bind_optional_text(stmt, idx++, rule.schedule_cron);
    bind_optional_timestamp(stmt, idx++, rule.effective_from);
    bind_optional_timestamp(stmt, idx++, rule.effective_until);

    sqlite3_bind_int64(stmt, idx++, static_cast<int64_t>(rule.triggered_count));
    sqlite3_bind_int64(stmt, idx++, static_cast<int64_t>(rule.success_count));
    sqlite3_bind_int64(stmt, idx++, static_cast<int64_t>(rule.failure_count));

    auto last_triggered_str = to_timestamp_string(rule.last_triggered);
    if (last_triggered_str.empty()) {
        sqlite3_bind_null(stmt, idx++);
    } else {
        sqlite3_bind_text(stmt, idx++, last_triggered_str.c_str(), -1, SQLITE_TRANSIENT);
    }

    auto created_str = to_timestamp_string(rule.created_at);
    if (created_str.empty()) {
        sqlite3_bind_text(stmt, idx++, "CURRENT_TIMESTAMP", -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_text(stmt, idx++, created_str.c_str(), -1, SQLITE_TRANSIENT);
    }

    auto updated_str = to_timestamp_string(rule.updated_at);
    if (updated_str.empty()) {
        sqlite3_bind_text(stmt, idx++, "CURRENT_TIMESTAMP", -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_text(stmt, idx++, updated_str.c_str(), -1, SQLITE_TRANSIENT);
    }

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to save rule: " + std::string(sqlite3_errmsg(db_)),
            "routing_repository"});
    }

    return kcenon::common::ok();
}

std::optional<client::routing_rule> routing_repository::find_by_id(
    std::string_view rule_id) const {
    if (!db_) return std::nullopt;

    static constexpr const char* sql = R"(
        SELECT pk, rule_id, name, description, enabled, priority,
               conditions_json, actions_json,
               schedule_cron, effective_from, effective_until,
               triggered_count, success_count, failure_count,
               last_triggered, created_at, updated_at
        FROM routing_rules WHERE rule_id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, rule_id.data(), static_cast<int>(rule_id.size()), SQLITE_TRANSIENT);

    std::optional<client::routing_rule> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = parse_row(stmt);
    }

    sqlite3_finalize(stmt);
    return result;
}

std::optional<client::routing_rule> routing_repository::find_by_pk(int64_t pk) const {
    if (!db_) return std::nullopt;

    static constexpr const char* sql = R"(
        SELECT pk, rule_id, name, description, enabled, priority,
               conditions_json, actions_json,
               schedule_cron, effective_from, effective_until,
               triggered_count, success_count, failure_count,
               last_triggered, created_at, updated_at
        FROM routing_rules WHERE pk = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_int64(stmt, 1, pk);

    std::optional<client::routing_rule> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = parse_row(stmt);
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<client::routing_rule> routing_repository::find_rules(
    const routing_rule_query_options& options) const {
    std::vector<client::routing_rule> result;
    if (!db_) return result;

    std::ostringstream sql;
    sql << R"(
        SELECT pk, rule_id, name, description, enabled, priority,
               conditions_json, actions_json,
               schedule_cron, effective_from, effective_until,
               triggered_count, success_count, failure_count,
               last_triggered, created_at, updated_at
        FROM routing_rules WHERE 1=1
    )";

    if (options.enabled_only.has_value()) {
        sql << " AND enabled = " << (options.enabled_only.value() ? "1" : "0");
    }

    if (options.order_by_priority) {
        sql << " ORDER BY priority DESC, created_at ASC";
    } else {
        sql << " ORDER BY created_at DESC";
    }

    sql << " LIMIT " << options.limit << " OFFSET " << options.offset;

    sqlite3_stmt* stmt = nullptr;
    auto sql_str = sql.str();
    if (sqlite3_prepare_v2(db_, sql_str.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return result;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        result.push_back(parse_row(stmt));
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<client::routing_rule> routing_repository::find_enabled_rules() const {
    routing_rule_query_options options;
    options.enabled_only = true;
    options.order_by_priority = true;
    return find_rules(options);
}

VoidResult routing_repository::remove(std::string_view rule_id) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "routing_repository"});
    }

    static constexpr const char* sql = "DELETE FROM routing_rules WHERE rule_id = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "routing_repository"});
    }

    sqlite3_bind_text(stmt, 1, rule_id.data(), static_cast<int>(rule_id.size()), SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to delete rule: " + std::string(sqlite3_errmsg(db_)),
            "routing_repository"});
    }

    return kcenon::common::ok();
}

bool routing_repository::exists(std::string_view rule_id) const {
    if (!db_) return false;

    static constexpr const char* sql = "SELECT 1 FROM routing_rules WHERE rule_id = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, rule_id.data(), static_cast<int>(rule_id.size()), SQLITE_TRANSIENT);

    bool found = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return found;
}

// =============================================================================
// Rule Ordering
// =============================================================================

VoidResult routing_repository::update_priority(std::string_view rule_id, int priority) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "routing_repository"});
    }

    static constexpr const char* sql = R"(
        UPDATE routing_rules SET
            priority = ?,
            updated_at = CURRENT_TIMESTAMP
        WHERE rule_id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "routing_repository"});
    }

    sqlite3_bind_int(stmt, 1, priority);
    sqlite3_bind_text(stmt, 2, rule_id.data(), static_cast<int>(rule_id.size()), SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to update priority: " + std::string(sqlite3_errmsg(db_)),
            "routing_repository"});
    }

    return kcenon::common::ok();
}

VoidResult routing_repository::enable_rule(std::string_view rule_id) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "routing_repository"});
    }

    static constexpr const char* sql = R"(
        UPDATE routing_rules SET
            enabled = 1,
            updated_at = CURRENT_TIMESTAMP
        WHERE rule_id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "routing_repository"});
    }

    sqlite3_bind_text(stmt, 1, rule_id.data(), static_cast<int>(rule_id.size()), SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to enable rule: " + std::string(sqlite3_errmsg(db_)),
            "routing_repository"});
    }

    return kcenon::common::ok();
}

VoidResult routing_repository::disable_rule(std::string_view rule_id) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "routing_repository"});
    }

    static constexpr const char* sql = R"(
        UPDATE routing_rules SET
            enabled = 0,
            updated_at = CURRENT_TIMESTAMP
        WHERE rule_id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "routing_repository"});
    }

    sqlite3_bind_text(stmt, 1, rule_id.data(), static_cast<int>(rule_id.size()), SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to disable rule: " + std::string(sqlite3_errmsg(db_)),
            "routing_repository"});
    }

    return kcenon::common::ok();
}

// =============================================================================
// Statistics
// =============================================================================

VoidResult routing_repository::increment_triggered(std::string_view rule_id) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "routing_repository"});
    }

    static constexpr const char* sql = R"(
        UPDATE routing_rules SET
            triggered_count = triggered_count + 1,
            last_triggered = CURRENT_TIMESTAMP
        WHERE rule_id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "routing_repository"});
    }

    sqlite3_bind_text(stmt, 1, rule_id.data(), static_cast<int>(rule_id.size()), SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to increment triggered: " + std::string(sqlite3_errmsg(db_)),
            "routing_repository"});
    }

    return kcenon::common::ok();
}

VoidResult routing_repository::increment_success(std::string_view rule_id) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "routing_repository"});
    }

    static constexpr const char* sql = R"(
        UPDATE routing_rules SET success_count = success_count + 1 WHERE rule_id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "routing_repository"});
    }

    sqlite3_bind_text(stmt, 1, rule_id.data(), static_cast<int>(rule_id.size()), SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to increment success: " + std::string(sqlite3_errmsg(db_)),
            "routing_repository"});
    }

    return kcenon::common::ok();
}

VoidResult routing_repository::increment_failure(std::string_view rule_id) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "routing_repository"});
    }

    static constexpr const char* sql = R"(
        UPDATE routing_rules SET failure_count = failure_count + 1 WHERE rule_id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "routing_repository"});
    }

    sqlite3_bind_text(stmt, 1, rule_id.data(), static_cast<int>(rule_id.size()), SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to increment failure: " + std::string(sqlite3_errmsg(db_)),
            "routing_repository"});
    }

    return kcenon::common::ok();
}

VoidResult routing_repository::reset_statistics(std::string_view rule_id) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "routing_repository"});
    }

    static constexpr const char* sql = R"(
        UPDATE routing_rules SET
            triggered_count = 0,
            success_count = 0,
            failure_count = 0,
            last_triggered = NULL
        WHERE rule_id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "routing_repository"});
    }

    sqlite3_bind_text(stmt, 1, rule_id.data(), static_cast<int>(rule_id.size()), SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to reset statistics: " + std::string(sqlite3_errmsg(db_)),
            "routing_repository"});
    }

    return kcenon::common::ok();
}

size_t routing_repository::count() const {
    if (!db_) return 0;

    static constexpr const char* sql = "SELECT COUNT(*) FROM routing_rules";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }

    size_t result = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
    }

    sqlite3_finalize(stmt);
    return result;
}

size_t routing_repository::count_enabled() const {
    if (!db_) return 0;

    static constexpr const char* sql = "SELECT COUNT(*) FROM routing_rules WHERE enabled = 1";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }

    size_t result = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
    }

    sqlite3_finalize(stmt);
    return result;
}

// =============================================================================
// Database Information
// =============================================================================

bool routing_repository::is_valid() const noexcept {
    return db_ != nullptr;
}

// =============================================================================
// Private Implementation
// =============================================================================

client::routing_rule routing_repository::parse_row(void* stmt_ptr) const {
    auto* stmt = static_cast<sqlite3_stmt*>(stmt_ptr);
    client::routing_rule rule;

    int col = 0;
    rule.pk = get_int64_column(stmt, col++);
    rule.rule_id = get_text_column(stmt, col++);
    rule.name = get_text_column(stmt, col++);
    rule.description = get_text_column(stmt, col++);
    rule.enabled = (get_int_column(stmt, col++) != 0);
    rule.priority = get_int_column(stmt, col++);

    auto conditions_json = get_text_column(stmt, col++);
    rule.conditions = deserialize_conditions(conditions_json);

    auto actions_json = get_text_column(stmt, col++);
    rule.actions = deserialize_actions(actions_json);

    rule.schedule_cron = get_optional_text(stmt, col++);

    auto effective_from_str = get_text_column(stmt, col++);
    rule.effective_from = from_optional_timestamp(effective_from_str.c_str());

    auto effective_until_str = get_text_column(stmt, col++);
    rule.effective_until = from_optional_timestamp(effective_until_str.c_str());

    rule.triggered_count = static_cast<size_t>(get_int64_column(stmt, col++));
    rule.success_count = static_cast<size_t>(get_int64_column(stmt, col++));
    rule.failure_count = static_cast<size_t>(get_int64_column(stmt, col++));

    auto last_triggered_str = get_text_column(stmt, col++);
    rule.last_triggered = from_timestamp_string(last_triggered_str.c_str());

    auto created_str = get_text_column(stmt, col++);
    rule.created_at = from_timestamp_string(created_str.c_str());

    auto updated_str = get_text_column(stmt, col++);
    rule.updated_at = from_timestamp_string(updated_str.c_str());

    return rule;
}

}  // namespace pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
