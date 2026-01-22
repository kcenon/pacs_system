/**
 * @file config_loader.cpp
 * @brief Implementation of YAML configuration loader
 */

#include "config_loader.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>

namespace pacs::samples {

namespace {

/**
 * @brief Simple YAML parser for configuration files
 *
 * This is a minimal YAML parser that handles:
 * - Key-value pairs
 * - Nested sections (with indentation)
 * - Simple lists (- item syntax)
 * - Comments (# lines)
 * - Quoted strings
 */
class simple_yaml_parser {
public:
    using yaml_map = std::map<std::string, std::string>;
    using yaml_list = std::vector<std::string>;

    explicit simple_yaml_parser(std::string_view content) {
        parse(content);
    }

    [[nodiscard]] auto get_string(const std::string& path,
                                   const std::string& default_value = "") const
        -> std::string {
        auto it = values_.find(path);
        return it != values_.end() ? it->second : default_value;
    }

    [[nodiscard]] auto get_int(const std::string& path, int default_value = 0) const
        -> int {
        auto value = get_string(path);
        if (value.empty()) return default_value;
        try {
            return std::stoi(value);
        } catch (...) {
            return default_value;
        }
    }

    [[nodiscard]] auto get_bool(const std::string& path, bool default_value = false) const
        -> bool {
        auto value = get_string(path);
        if (value.empty()) return default_value;
        std::transform(value.begin(), value.end(), value.begin(), ::tolower);
        return value == "true" || value == "yes" || value == "on" || value == "1";
    }

    [[nodiscard]] auto get_list(const std::string& path) const
        -> std::vector<std::string> {
        auto it = lists_.find(path);
        return it != lists_.end() ? it->second : std::vector<std::string>{};
    }

private:
    void parse(std::string_view content) {
        std::istringstream stream{std::string(content)};
        std::string line;
        std::vector<std::pair<int, std::string>> path_stack;
        std::string current_list_path;

        while (std::getline(stream, line)) {
            // Skip empty lines
            if (line.empty() || line.find_first_not_of(" \t\r\n") == std::string::npos) {
                continue;
            }

            // Skip comments
            auto non_ws = line.find_first_not_of(" \t");
            if (non_ws != std::string::npos && line[non_ws] == '#') {
                continue;
            }

            // Calculate indentation level
            int indent = 0;
            for (char c : line) {
                if (c == ' ') indent++;
                else if (c == '\t') indent += 2;
                else break;
            }

            // Trim the line
            auto trimmed = trim(line);

            // Check for list item
            if (trimmed.starts_with("- ")) {
                auto item = trim(trimmed.substr(2));
                item = strip_quotes(item);
                if (!current_list_path.empty()) {
                    lists_[current_list_path].push_back(item);
                }
                continue;
            }

            // Reset list context if not a list item
            current_list_path.clear();

            // Parse key: value
            auto colon_pos = trimmed.find(':');
            if (colon_pos == std::string::npos) continue;

            std::string key = trim(trimmed.substr(0, colon_pos));
            std::string value = (colon_pos + 1 < trimmed.size())
                                    ? trim(trimmed.substr(colon_pos + 1))
                                    : "";

            // Pop path stack to appropriate level
            while (!path_stack.empty() && path_stack.back().first >= indent) {
                path_stack.pop_back();
            }

            // Build full path
            std::string full_path;
            for (const auto& [_, segment] : path_stack) {
                full_path += segment + ".";
            }
            full_path += key;

            if (value.empty()) {
                // This is a section header
                path_stack.emplace_back(indent, key);
                current_list_path = full_path;
            } else {
                // This is a value
                value = strip_quotes(value);
                values_[full_path] = value;
            }
        }
    }

    [[nodiscard]] static auto trim(std::string_view str) -> std::string {
        auto start = str.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        auto end = str.find_last_not_of(" \t\r\n");
        return std::string(str.substr(start, end - start + 1));
    }

    [[nodiscard]] static auto strip_quotes(std::string_view str) -> std::string {
        if (str.length() >= 2) {
            if ((str.front() == '"' && str.back() == '"') ||
                (str.front() == '\'' && str.back() == '\'')) {
                return std::string(str.substr(1, str.length() - 2));
            }
        }
        return std::string(str);
    }

    yaml_map values_;
    std::map<std::string, yaml_list> lists_;
};

}  // namespace

auto config_loader::load(const std::filesystem::path& path)
    -> kcenon::common::Result<production_config> {
    std::ifstream file(path);
    if (!file.is_open()) {
        return kcenon::common::Result<production_config>::err(
            kcenon::common::error_info("Failed to open configuration file: " + path.string()));
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return load_from_string(buffer.str());
}

auto config_loader::load_from_string(std::string_view yaml_content)
    -> kcenon::common::Result<production_config> {
    try {
        simple_yaml_parser parser(yaml_content);
        production_config config = create_default();

        // Server configuration
        config.server.ae_title = parser.get_string("server.ae_title", config.server.ae_title);
        config.server.port = static_cast<std::uint16_t>(
            parser.get_int("server.port", config.server.port));
        config.server.max_associations = static_cast<std::size_t>(
            parser.get_int("server.max_associations",
                           static_cast<int>(config.server.max_associations)));
        config.server.idle_timeout = std::chrono::seconds(
            parser.get_int("server.idle_timeout_seconds",
                           static_cast<int>(config.server.idle_timeout.count())));

        // TLS configuration
        config.server.tls.enabled = parser.get_bool("server.tls.enabled",
                                                     config.server.tls.enabled);
        config.server.tls.certificate = parser.get_string("server.tls.certificate");
        config.server.tls.private_key = parser.get_string("server.tls.private_key");
        config.server.tls.ca_certificate = parser.get_string("server.tls.ca_certificate");
        config.server.tls.require_client_cert =
            parser.get_bool("server.tls.require_client_cert", false);
        config.server.tls.min_version = parser.get_string("server.tls.min_version", "1.2");

        // Storage configuration
        config.storage.root_path = parser.get_string("storage.root_path",
                                                      config.storage.root_path.string());
        config.storage.naming_scheme = parser.get_string("storage.naming_scheme",
                                                          config.storage.naming_scheme);
        config.storage.duplicate_policy = parser.get_string("storage.duplicate_policy",
                                                             config.storage.duplicate_policy);
        config.storage.database.path = parser.get_string("storage.database.path",
                                                          config.storage.database.path.string());

        // Security configuration
        config.security.access_control.enabled =
            parser.get_bool("security.access_control.enabled", true);
        config.security.access_control.default_role =
            parser.get_string("security.access_control.default_role", "viewer");
        config.security.allowed_ae_titles =
            parser.get_list("security.allowed_ae_titles");
        config.security.anonymization.auto_anonymize =
            parser.get_bool("security.anonymization.auto_anonymize", false);

        auto profile_str = parser.get_string("security.anonymization.profile", "basic");
        config.security.anonymization.profile = parse_anonymization_profile(profile_str);

        // REST API configuration
        config.rest_api.enabled = parser.get_bool("rest_api.enabled", true);
        config.rest_api.port = static_cast<std::uint16_t>(
            parser.get_int("rest_api.port", config.rest_api.port));
        config.rest_api.cors_enabled = parser.get_bool("rest_api.cors_enabled", true);

        // Monitoring configuration
        config.monitoring.health_check_interval = std::chrono::seconds(
            parser.get_int("monitoring.health_check_interval_seconds", 30));
        config.monitoring.metrics_enabled = parser.get_bool("monitoring.metrics_enabled", true);

        // Logging configuration
        config.logging.level = parser.get_string("logging.level", "info");
        config.logging.audit_log_path = parser.get_string("logging.audit_log_path");

        // Validate configuration
        auto validation = validate(config);
        if (!validation.is_ok()) {
            return kcenon::common::Result<production_config>::err(
                kcenon::common::error_info(validation.error().message));
        }

        return kcenon::common::Result<production_config>::ok(std::move(config));

    } catch (const std::exception& e) {
        return kcenon::common::Result<production_config>::err(
            kcenon::common::error_info(std::string("Configuration parsing error: ") + e.what()));
    }
}

auto config_loader::create_default() -> production_config {
    production_config config;

    // Server defaults
    config.server.ae_title = "PROD_PACS";
    config.server.port = 11112;
    config.server.max_associations = 100;
    config.server.idle_timeout = std::chrono::seconds{300};
    config.server.tls.enabled = false;

    // Storage defaults
    config.storage.root_path = "./pacs_data";
    config.storage.naming_scheme = "uid_hierarchical";
    config.storage.duplicate_policy = "replace";
    config.storage.database.path = "./pacs_data/index.db";

    // Security defaults
    config.security.access_control.enabled = true;
    config.security.access_control.default_role = "viewer";
    config.security.anonymization.auto_anonymize = false;
    config.security.anonymization.profile = security::anonymization_profile::basic;

    // REST API defaults
    config.rest_api.enabled = true;
    config.rest_api.port = 8080;
    config.rest_api.cors_enabled = true;

    // Monitoring defaults
    config.monitoring.health_check_interval = std::chrono::seconds{30};
    config.monitoring.metrics_enabled = true;

    // Logging defaults
    config.logging.level = "info";

    return config;
}

auto config_loader::validate(const production_config& config)
    -> kcenon::common::VoidResult {
    // Validate AE Title (max 16 characters, no leading/trailing spaces)
    if (config.server.ae_title.empty()) {
        return kcenon::common::VoidResult::err(
            kcenon::common::error_info("AE Title cannot be empty"));
    }
    if (config.server.ae_title.length() > 16) {
        return kcenon::common::VoidResult::err(
            kcenon::common::error_info("AE Title exceeds maximum length of 16 characters"));
    }

    // Validate port
    if (config.server.port == 0) {
        return kcenon::common::VoidResult::err(
            kcenon::common::error_info("Server port cannot be 0"));
    }

    // Validate REST API port
    if (config.rest_api.enabled && config.rest_api.port == 0) {
        return kcenon::common::VoidResult::err(
            kcenon::common::error_info("REST API port cannot be 0"));
    }

    // Validate TLS configuration
    if (config.server.tls.enabled) {
        if (config.server.tls.certificate.empty()) {
            return kcenon::common::VoidResult::err(
                kcenon::common::error_info("TLS enabled but no certificate specified"));
        }
        if (config.server.tls.private_key.empty()) {
            return kcenon::common::VoidResult::err(
                kcenon::common::error_info("TLS enabled but no private key specified"));
        }
    }

    // Validate naming scheme
    const std::vector<std::string> valid_schemes = {
        "uid_flat", "uid_hierarchical", "date_based"
    };
    auto scheme_it = std::find(valid_schemes.begin(), valid_schemes.end(),
                               config.storage.naming_scheme);
    if (scheme_it == valid_schemes.end()) {
        return kcenon::common::VoidResult::err(
            kcenon::common::error_info("Invalid naming scheme: " + config.storage.naming_scheme));
    }

    // Validate duplicate policy
    const std::vector<std::string> valid_policies = {"reject", "replace", "rename"};
    auto policy_it = std::find(valid_policies.begin(), valid_policies.end(),
                               config.storage.duplicate_policy);
    if (policy_it == valid_policies.end()) {
        return kcenon::common::VoidResult::err(
            kcenon::common::error_info("Invalid duplicate policy: " + config.storage.duplicate_policy));
    }

    return kcenon::common::VoidResult::ok(std::monostate{});
}

auto config_loader::parse_value(std::string_view value) -> std::string {
    return trim(value);
}

auto config_loader::parse_bool(std::string_view value) -> bool {
    auto v = trim(value);
    std::transform(v.begin(), v.end(), v.begin(), ::tolower);
    return v == "true" || v == "yes" || v == "on" || v == "1";
}

auto config_loader::parse_int(std::string_view value) -> int {
    auto v = trim(value);
    try {
        return std::stoi(v);
    } catch (...) {
        return 0;
    }
}

auto config_loader::parse_anonymization_profile(std::string_view value)
    -> security::anonymization_profile {
    auto v = trim(value);
    std::transform(v.begin(), v.end(), v.begin(), ::tolower);

    if (v == "basic" || v == "basic_profile") {
        return security::anonymization_profile::basic;
    }
    if (v == "hipaa" || v == "hipaa_safe_harbor") {
        return security::anonymization_profile::hipaa_safe_harbor;
    }
    if (v == "retain_longitudinal" || v == "retain_longitudinal_full_dates") {
        return security::anonymization_profile::retain_longitudinal;
    }
    if (v == "retain_device" || v == "retain_device_identity" ||
        v == "retain_patient" || v == "retain_patient_characteristics") {
        return security::anonymization_profile::retain_patient_characteristics;
    }
    if (v == "gdpr") {
        return security::anonymization_profile::gdpr_compliant;
    }

    // Default to basic
    return security::anonymization_profile::basic;
}

auto config_loader::trim(std::string_view str) -> std::string {
    auto start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = str.find_last_not_of(" \t\r\n");
    return std::string(str.substr(start, end - start + 1));
}

}  // namespace pacs::samples
