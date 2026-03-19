/**
 * @file atna_config.hpp
 * @brief Configuration management for ATNA audit logging
 *
 * Provides the atna_config struct and helper functions for parsing,
 * serializing, and validating ATNA audit configuration. Wraps the
 * lower-level syslog_transport_config with audit-specific settings
 * such as event filtering and the audit source identifier.
 *
 * @see IHE ITI TF-1 Section 9 — Audit Trail and Node Authentication (ATNA)
 * @see Issue #823 - Add ATNA audit configuration management
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_SECURITY_ATNA_CONFIG_HPP
#define PACS_SECURITY_ATNA_CONFIG_HPP

#include "atna_syslog_transport.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace kcenon::pacs::security {

// =============================================================================
// ATNA Audit Configuration
// =============================================================================

/**
 * @brief Configuration for ATNA audit logging
 *
 * Combines the syslog transport configuration with audit-specific settings
 * such as the audit source identifier, event filtering, and master
 * enable/disable control.
 *
 * ## Usage
 * ```cpp
 * atna_config config;
 * config.enabled = true;
 * config.audit_source_id = "PACS_SYSTEM_01";
 * config.transport.host = "audit-server.hospital.local";
 * config.transport.port = 6514;
 * config.transport.protocol = syslog_transport_protocol::tls;
 * config.transport.ca_cert_path = "/etc/pacs/certs/ca.pem";
 *
 * // Optionally disable specific event types
 * config.audit_security_alerts = false;
 * ```
 */
struct atna_config {
    /// Master enable/disable for ATNA audit logging
    bool enabled{false};

    /// Audit source identifier (e.g., "PACS_SYSTEM_01")
    std::string audit_source_id{"PACS_SYSTEM"};

    /// Syslog transport configuration
    syslog_transport_config transport;

    // -- Event filtering --

    /// Audit C-STORE events (DICOM Instances Transferred)
    bool audit_storage{true};

    /// Audit C-FIND events (Query)
    bool audit_query{true};

    /// Audit login/logout events (User Authentication)
    bool audit_authentication{true};

    /// Audit security alert events (access denied, etc.)
    bool audit_security_alerts{true};
};

// =============================================================================
// Default Configuration
// =============================================================================

/**
 * @brief Create a default ATNA configuration
 *
 * Returns a configuration with sensible defaults:
 * - Disabled by default (must be explicitly enabled)
 * - UDP transport to localhost:514
 * - All event types enabled
 *
 * @return Default atna_config
 */
[[nodiscard]] atna_config make_default_atna_config();

// =============================================================================
// JSON Serialization
// =============================================================================

/**
 * @brief Serialize an atna_config to a JSON string
 *
 * Produces a compact JSON representation suitable for configuration files
 * and REST API responses.
 *
 * @param config The configuration to serialize
 * @return JSON string
 */
[[nodiscard]] std::string to_json(const atna_config& config);

/**
 * @brief Parse an atna_config from a JSON string
 *
 * Reads a JSON configuration string and populates an atna_config struct.
 * Fields not present in the JSON retain their default values.
 *
 * @param json_str JSON string to parse
 * @return Parsed atna_config, or defaults for malformed input
 */
[[nodiscard]] atna_config parse_atna_config(std::string_view json_str);

// =============================================================================
// Validation
// =============================================================================

/**
 * @brief Validation result for ATNA configuration
 */
struct atna_config_validation {
    bool valid{true};
    std::vector<std::string> errors;
};

/**
 * @brief Validate an ATNA configuration
 *
 * Checks for:
 * - Non-empty audit_source_id
 * - Non-empty transport host
 * - Valid port range (1-65535)
 * - TLS certificate paths exist when protocol is TLS
 *
 * @param config The configuration to validate
 * @return Validation result with any errors
 */
[[nodiscard]] atna_config_validation validate(const atna_config& config);

}  // namespace kcenon::pacs::security

#endif  // PACS_SECURITY_ATNA_CONFIG_HPP
