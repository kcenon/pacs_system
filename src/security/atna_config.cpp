/**
 * @file atna_config.cpp
 * @brief Implementation of ATNA audit configuration management
 */

#include "kcenon/pacs/security/atna_config.h"

#include <algorithm>
#include <filesystem>
#include <sstream>

namespace kcenon::pacs::security {

namespace {

// =============================================================================
// JSON Helpers (self-contained to avoid web module dependency)
// =============================================================================

std::string escape_json(std::string_view s) {
    std::string result;
    result.reserve(s.size() + 10);
    for (char c : s) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n";  break;
            case '\r': result += "\\r";  break;
            case '\t': result += "\\t";  break;
            default:   result += c;      break;
        }
    }
    return result;
}

/// Skip whitespace in a string, returning the position after whitespace
size_t skip_ws(std::string_view s, size_t pos) {
    while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' ||
                               s[pos] == '\n' || s[pos] == '\r')) {
        ++pos;
    }
    return pos;
}

/// Extract a JSON string value starting at pos (past the opening quote)
std::string extract_string(std::string_view s, size_t& pos) {
    std::string result;
    while (pos < s.size() && s[pos] != '"') {
        if (s[pos] == '\\' && pos + 1 < s.size()) {
            ++pos;
            switch (s[pos]) {
                case '"':  result += '"';  break;
                case '\\': result += '\\'; break;
                case 'n':  result += '\n'; break;
                case 'r':  result += '\r'; break;
                case 't':  result += '\t'; break;
                default:   result += s[pos]; break;
            }
        } else {
            result += s[pos];
        }
        ++pos;
    }
    if (pos < s.size()) ++pos;  // skip closing quote
    return result;
}

/// Find the value for a JSON key, returning the value as a string
/// Handles: "key": "string_value", "key": number, "key": true/false
std::string find_json_value(std::string_view json, std::string_view key) {
    // Search for "key":
    std::string needle = "\"" + std::string(key) + "\"";
    auto key_pos = json.find(needle);
    if (key_pos == std::string_view::npos) return "";

    size_t pos = key_pos + needle.size();
    pos = skip_ws(json, pos);
    if (pos >= json.size() || json[pos] != ':') return "";
    ++pos;
    pos = skip_ws(json, pos);
    if (pos >= json.size()) return "";

    if (json[pos] == '"') {
        // String value
        ++pos;
        return extract_string(json, pos);
    }

    // Non-string value (number, boolean, null)
    size_t start = pos;
    while (pos < json.size() && json[pos] != ',' && json[pos] != '}' &&
           json[pos] != ' ' && json[pos] != '\n' && json[pos] != '\r') {
        ++pos;
    }
    return std::string(json.substr(start, pos - start));
}

/// Find a nested JSON object value for a dotted key (e.g., "transport.host")
std::string find_nested_value(std::string_view json,
                              std::string_view section,
                              std::string_view key) {
    // Find the section object
    std::string needle = "\"" + std::string(section) + "\"";
    auto sec_pos = json.find(needle);
    if (sec_pos == std::string_view::npos) return "";

    // Find the opening brace of the nested object
    auto brace_pos = json.find('{', sec_pos + needle.size());
    if (brace_pos == std::string_view::npos) return "";

    // Find the matching closing brace
    int depth = 1;
    size_t end_pos = brace_pos + 1;
    while (end_pos < json.size() && depth > 0) {
        if (json[end_pos] == '{') ++depth;
        else if (json[end_pos] == '}') --depth;
        ++end_pos;
    }

    auto sub_json = json.substr(brace_pos, end_pos - brace_pos);
    return find_json_value(sub_json, key);
}

bool parse_bool(const std::string& val, bool default_val) {
    if (val == "true") return true;
    if (val == "false") return false;
    return default_val;
}

uint16_t parse_uint16(const std::string& val, uint16_t default_val) {
    if (val.empty()) return default_val;
    try {
        auto v = std::stoul(val);
        if (v > 65535) return default_val;
        return static_cast<uint16_t>(v);
    } catch (...) {
        return default_val;
    }
}

syslog_transport_protocol parse_protocol(const std::string& val) {
    if (val == "tls") return syslog_transport_protocol::tls;
    return syslog_transport_protocol::udp;
}

std::string protocol_to_string(syslog_transport_protocol p) {
    return (p == syslog_transport_protocol::tls) ? "tls" : "udp";
}

}  // namespace

// =============================================================================
// Default Configuration
// =============================================================================

atna_config make_default_atna_config() {
    return atna_config{};
}

// =============================================================================
// JSON Serialization
// =============================================================================

std::string to_json(const atna_config& config) {
    std::ostringstream oss;
    oss << R"({"enabled":)" << (config.enabled ? "true" : "false")
        << R"(,"audit_source_id":")" << escape_json(config.audit_source_id)
        << R"(","transport":{"protocol":")"
        << protocol_to_string(config.transport.protocol)
        << R"(","host":")" << escape_json(config.transport.host)
        << R"(","port":)" << config.transport.port
        << R"(,"app_name":")" << escape_json(config.transport.app_name)
        << R"(","ca_cert_path":")" << escape_json(config.transport.ca_cert_path)
        << R"(","client_cert_path":")"
        << escape_json(config.transport.client_cert_path)
        << R"(","client_key_path":")"
        << escape_json(config.transport.client_key_path)
        << R"(","verify_server":)"
        << (config.transport.verify_server ? "true" : "false")
        << R"(},"audit_storage":)"
        << (config.audit_storage ? "true" : "false")
        << R"(,"audit_query":)"
        << (config.audit_query ? "true" : "false")
        << R"(,"audit_authentication":)"
        << (config.audit_authentication ? "true" : "false")
        << R"(,"audit_security_alerts":)"
        << (config.audit_security_alerts ? "true" : "false")
        << "}";
    return oss.str();
}

// =============================================================================
// JSON Parsing
// =============================================================================

atna_config parse_atna_config(std::string_view json_str) {
    atna_config config;

    // Top-level fields
    auto enabled_val = find_json_value(json_str, "enabled");
    if (!enabled_val.empty()) {
        config.enabled = parse_bool(enabled_val, config.enabled);
    }

    auto source_id = find_json_value(json_str, "audit_source_id");
    if (!source_id.empty()) {
        config.audit_source_id = source_id;
    }

    // Event filtering
    auto storage_val = find_json_value(json_str, "audit_storage");
    if (!storage_val.empty()) {
        config.audit_storage = parse_bool(storage_val, config.audit_storage);
    }

    auto query_val = find_json_value(json_str, "audit_query");
    if (!query_val.empty()) {
        config.audit_query = parse_bool(query_val, config.audit_query);
    }

    auto auth_val = find_json_value(json_str, "audit_authentication");
    if (!auth_val.empty()) {
        config.audit_authentication =
            parse_bool(auth_val, config.audit_authentication);
    }

    auto alerts_val = find_json_value(json_str, "audit_security_alerts");
    if (!alerts_val.empty()) {
        config.audit_security_alerts =
            parse_bool(alerts_val, config.audit_security_alerts);
    }

    // Transport nested object
    auto proto = find_nested_value(json_str, "transport", "protocol");
    if (!proto.empty()) {
        config.transport.protocol = parse_protocol(proto);
    }

    auto host = find_nested_value(json_str, "transport", "host");
    if (!host.empty()) {
        config.transport.host = host;
    }

    auto port = find_nested_value(json_str, "transport", "port");
    if (!port.empty()) {
        config.transport.port =
            parse_uint16(port, config.transport.port);
    }

    auto app = find_nested_value(json_str, "transport", "app_name");
    if (!app.empty()) {
        config.transport.app_name = app;
    }

    auto ca = find_nested_value(json_str, "transport", "ca_cert_path");
    if (!ca.empty()) {
        config.transport.ca_cert_path = ca;
    }

    auto cert = find_nested_value(json_str, "transport", "client_cert_path");
    if (!cert.empty()) {
        config.transport.client_cert_path = cert;
    }

    auto key = find_nested_value(json_str, "transport", "client_key_path");
    if (!key.empty()) {
        config.transport.client_key_path = key;
    }

    auto verify = find_nested_value(json_str, "transport", "verify_server");
    if (!verify.empty()) {
        config.transport.verify_server =
            parse_bool(verify, config.transport.verify_server);
    }

    return config;
}

// =============================================================================
// Validation
// =============================================================================

atna_config_validation validate(const atna_config& config) {
    atna_config_validation result;

    if (config.audit_source_id.empty()) {
        result.valid = false;
        result.errors.emplace_back("audit_source_id must not be empty");
    }

    if (config.transport.host.empty()) {
        result.valid = false;
        result.errors.emplace_back("transport.host must not be empty");
    }

    if (config.transport.port == 0) {
        result.valid = false;
        result.errors.emplace_back(
            "transport.port must be in range 1-65535");
    }

    if (config.transport.protocol == syslog_transport_protocol::tls) {
        if (config.transport.ca_cert_path.empty()) {
            result.valid = false;
            result.errors.emplace_back(
                "transport.ca_cert_path is required for TLS protocol");
        } else if (!std::filesystem::exists(
                       config.transport.ca_cert_path)) {
            result.valid = false;
            result.errors.emplace_back(
                "transport.ca_cert_path file does not exist: " +
                config.transport.ca_cert_path);
        }

        if (!config.transport.client_cert_path.empty() &&
            !std::filesystem::exists(config.transport.client_cert_path)) {
            result.valid = false;
            result.errors.emplace_back(
                "transport.client_cert_path file does not exist: " +
                config.transport.client_cert_path);
        }

        if (!config.transport.client_key_path.empty() &&
            !std::filesystem::exists(config.transport.client_key_path)) {
            result.valid = false;
            result.errors.emplace_back(
                "transport.client_key_path file does not exist: " +
                config.transport.client_key_path);
        }
    }

    return result;
}

}  // namespace kcenon::pacs::security
