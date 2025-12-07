/**
 * @file health_json.hpp
 * @brief JSON serialization for health check data structures
 *
 * This file provides functions to serialize health_status and related
 * structures to JSON format suitable for REST API responses and
 * monitoring system integration.
 *
 * @see Issue #211 - Implement health check endpoint
 * @see RFC 8259 - The JavaScript Object Notation (JSON) Data Interchange Format
 */

#pragma once

#include "health_status.hpp"

#include <chrono>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <string>

namespace pacs::monitoring {

/**
 * @brief Convert time_point to ISO 8601 formatted string
 * @param tp The time point to convert
 * @return ISO 8601 formatted timestamp (e.g., "2024-01-15T10:30:00Z")
 */
[[nodiscard]] inline std::string to_iso8601(
    std::chrono::system_clock::time_point tp) {
    const auto time_t_val = std::chrono::system_clock::to_time_t(tp);
    std::tm tm_val{};

#if defined(_MSC_VER)
    gmtime_s(&tm_val, &time_t_val);
#else
    gmtime_r(&time_t_val, &tm_val);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_val, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

/**
 * @brief Escape special characters in JSON string
 * @param str The string to escape
 * @return JSON-safe escaped string
 */
[[nodiscard]] inline std::string escape_json_string(std::string_view str) {
    std::string result;
    result.reserve(str.size() + 10);  // Reserve extra space for escapes

    for (char c : str) {
        switch (c) {
            case '"':
                result += "\\\"";
                break;
            case '\\':
                result += "\\\\";
                break;
            case '\b':
                result += "\\b";
                break;
            case '\f':
                result += "\\f";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\t':
                result += "\\t";
                break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    // Control character - encode as \u00XX
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x",
                                  static_cast<unsigned char>(c));
                    result += buf;
                } else {
                    result += c;
                }
                break;
        }
    }

    return result;
}

/**
 * @brief Convert database_status to JSON string
 * @param status The database status to convert
 * @return JSON representation
 */
[[nodiscard]] inline std::string to_json(const database_status& status) {
    std::ostringstream oss;
    oss << "{"
        << R"("connected":)" << (status.connected ? "true" : "false");

    if (status.last_connected) {
        oss << R"(,"last_connected":")" << to_iso8601(*status.last_connected)
            << "\"";
    }

    oss << R"(,"active_connections":)" << status.active_connections;

    if (status.response_time) {
        oss << R"(,"response_time_ms":)" << status.response_time->count();
    }

    if (status.error_message) {
        oss << R"(,"error":")" << escape_json_string(*status.error_message)
            << "\"";
    }

    oss << "}";
    return oss.str();
}

/**
 * @brief Convert storage_status to JSON string
 * @param status The storage status to convert
 * @return JSON representation
 */
[[nodiscard]] inline std::string to_json(const storage_status& status) {
    std::ostringstream oss;
    oss << "{"
        << R"("writable":)" << (status.writable ? "true" : "false")
        << R"(,"readable":)" << (status.readable ? "true" : "false")
        << R"(,"total_bytes":)" << status.total_bytes
        << R"(,"used_bytes":)" << status.used_bytes
        << R"(,"available_bytes":)" << status.available_bytes
        << R"(,"usage_percent":)" << std::fixed << std::setprecision(2)
        << status.usage_percent();

    if (status.error_message) {
        oss << R"(,"error":")" << escape_json_string(*status.error_message)
            << "\"";
    }

    oss << "}";
    return oss.str();
}

/**
 * @brief Convert association_metrics to JSON string
 * @param metrics The association metrics to convert
 * @return JSON representation
 */
[[nodiscard]] inline std::string to_json(const association_metrics& metrics) {
    std::ostringstream oss;
    oss << "{"
        << R"("active":)" << metrics.active_associations
        << R"(,"max":)" << metrics.max_associations
        << R"(,"total":)" << metrics.total_associations
        << R"(,"failed":)" << metrics.failed_associations << "}";
    return oss.str();
}

/**
 * @brief Convert storage_metrics to JSON string
 * @param metrics The storage metrics to convert
 * @return JSON representation
 */
[[nodiscard]] inline std::string to_json(const storage_metrics& metrics) {
    std::ostringstream oss;
    oss << "{"
        << R"("total_instances":)" << metrics.total_instances
        << R"(,"total_studies":)" << metrics.total_studies
        << R"(,"total_series":)" << metrics.total_series
        << R"(,"successful_stores":)" << metrics.successful_stores
        << R"(,"failed_stores":)" << metrics.failed_stores << "}";
    return oss.str();
}

/**
 * @brief Convert version_info to JSON string
 * @param info The version info to convert
 * @return JSON representation
 */
[[nodiscard]] inline std::string to_json(const version_info& info) {
    std::ostringstream oss;
    oss << "{"
        << R"("version":")" << info.version_string() << "\""
        << R"(,"major":)" << info.major << R"(,"minor":)" << info.minor
        << R"(,"patch":)" << info.patch;

    if (!info.build_id.empty()) {
        oss << R"(,"build_id":")" << escape_json_string(info.build_id) << "\"";
    }

    oss << R"(,"startup_time":")" << to_iso8601(info.startup_time) << "\""
        << R"(,"uptime_seconds":)" << info.uptime().count() << "}";

    return oss.str();
}

/**
 * @brief Convert health_status to JSON string
 *
 * Produces a complete JSON representation of the health status suitable
 * for REST API responses. The output follows standard health check
 * response formats compatible with Kubernetes and other orchestrators.
 *
 * @param status The health status to convert
 * @return JSON representation
 *
 * @example
 * @code
 * health_status status;
 * status.level = health_level::healthy;
 * status.database.connected = true;
 *
 * std::string json = to_json(status);
 * // Returns: {"status":"healthy","timestamp":"2024-01-15T10:30:00Z",...}
 * @endcode
 */
[[nodiscard]] inline std::string to_json(const health_status& status) {
    std::ostringstream oss;
    oss << "{"
        << R"("status":")" << to_string(status.level) << "\""
        << R"(,"timestamp":")" << to_iso8601(status.timestamp) << "\""
        << R"(,"healthy":)" << (status.is_healthy() ? "true" : "false")
        << R"(,"operational":)" << (status.is_operational() ? "true" : "false");

    if (status.message) {
        oss << R"(,"message":")" << escape_json_string(*status.message) << "\"";
    }

    oss << R"(,"database":)" << to_json(status.database)
        << R"(,"storage":)" << to_json(status.storage)
        << R"(,"associations":)" << to_json(status.associations)
        << R"(,"metrics":)" << to_json(status.metrics)
        << R"(,"version":)" << to_json(status.version) << "}";

    return oss.str();
}

/**
 * @brief Convert health_status to pretty-printed JSON string
 *
 * Produces a human-readable JSON representation with indentation.
 *
 * @param status The health status to convert
 * @param indent Number of spaces for indentation (default: 2)
 * @return Pretty-printed JSON representation
 */
[[nodiscard]] inline std::string to_json_pretty(const health_status& status,
                                                 int indent = 2) {
    const std::string ind(static_cast<std::size_t>(indent), ' ');
    const std::string ind2(static_cast<std::size_t>(indent * 2), ' ');

    std::ostringstream oss;
    oss << "{\n"
        << ind << R"("status": ")" << to_string(status.level) << "\",\n"
        << ind << R"("timestamp": ")" << to_iso8601(status.timestamp) << "\",\n"
        << ind << R"("healthy": )" << (status.is_healthy() ? "true" : "false")
        << ",\n"
        << ind << R"("operational": )"
        << (status.is_operational() ? "true" : "false");

    if (status.message) {
        oss << ",\n"
            << ind << R"("message": ")" << escape_json_string(*status.message)
            << "\"";
    }

    // Database section
    oss << ",\n"
        << ind << R"("database": {)" << "\n"
        << ind2 << R"("connected": )"
        << (status.database.connected ? "true" : "false");

    if (status.database.last_connected) {
        oss << ",\n"
            << ind2 << R"("last_connected": ")"
            << to_iso8601(*status.database.last_connected) << "\"";
    }

    oss << ",\n"
        << ind2
        << R"("active_connections": )" << status.database.active_connections;

    if (status.database.response_time) {
        oss << ",\n"
            << ind2 << R"("response_time_ms": )"
            << status.database.response_time->count();
    }

    if (status.database.error_message) {
        oss << ",\n"
            << ind2 << R"("error": ")"
            << escape_json_string(*status.database.error_message) << "\"";
    }

    oss << "\n" << ind << "}";

    // Storage section
    oss << ",\n"
        << ind << R"("storage": {)" << "\n"
        << ind2 << R"("writable": )"
        << (status.storage.writable ? "true" : "false") << ",\n"
        << ind2 << R"("readable": )"
        << (status.storage.readable ? "true" : "false") << ",\n"
        << ind2 << R"("total_bytes": )" << status.storage.total_bytes << ",\n"
        << ind2 << R"("used_bytes": )" << status.storage.used_bytes << ",\n"
        << ind2 << R"("available_bytes": )" << status.storage.available_bytes
        << ",\n"
        << ind2 << R"("usage_percent": )" << std::fixed << std::setprecision(2)
        << status.storage.usage_percent();

    if (status.storage.error_message) {
        oss << ",\n"
            << ind2 << R"("error": ")"
            << escape_json_string(*status.storage.error_message) << "\"";
    }

    oss << "\n" << ind << "}";

    // Associations section
    oss << ",\n"
        << ind << R"("associations": {)" << "\n"
        << ind2 << R"("active": )" << status.associations.active_associations
        << ",\n"
        << ind2 << R"("max": )" << status.associations.max_associations << ",\n"
        << ind2 << R"("total": )" << status.associations.total_associations
        << ",\n"
        << ind2 << R"("failed": )" << status.associations.failed_associations
        << "\n"
        << ind << "}";

    // Metrics section
    oss << ",\n"
        << ind << R"("metrics": {)" << "\n"
        << ind2 << R"("total_instances": )" << status.metrics.total_instances
        << ",\n"
        << ind2 << R"("total_studies": )" << status.metrics.total_studies
        << ",\n"
        << ind2 << R"("total_series": )" << status.metrics.total_series << ",\n"
        << ind2
        << R"("successful_stores": )" << status.metrics.successful_stores
        << ",\n"
        << ind2 << R"("failed_stores": )" << status.metrics.failed_stores
        << "\n"
        << ind << "}";

    // Version section
    oss << ",\n"
        << ind << R"("version": {)" << "\n"
        << ind2 << R"("version": ")" << status.version.version_string()
        << "\",\n"
        << ind2 << R"("major": )" << status.version.major << ",\n"
        << ind2 << R"("minor": )" << status.version.minor << ",\n"
        << ind2 << R"("patch": )" << status.version.patch;

    if (!status.version.build_id.empty()) {
        oss << ",\n"
            << ind2 << R"("build_id": ")"
            << escape_json_string(status.version.build_id) << "\"";
    }

    oss << ",\n"
        << ind2 << R"("startup_time": ")"
        << to_iso8601(status.version.startup_time) << "\",\n"
        << ind2 << R"("uptime_seconds": )" << status.version.uptime().count()
        << "\n"
        << ind << "}"
        << "\n}";

    return oss.str();
}

}  // namespace pacs::monitoring
