/**
 * @file atna_service_auditor.hpp
 * @brief High-level facade for ATNA audit logging in DICOM services
 *
 * Wraps atna_audit_logger (message generation) and atna_syslog_transport
 * (message delivery) into a simple API that DICOM services can call
 * with minimal coupling.
 *
 * @see IHE ITI TF-1 Section 9 — Audit Trail and Node Authentication (ATNA)
 * @see Issue #821 - Integrate ATNA audit logging into DICOM services
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_SECURITY_ATNA_SERVICE_AUDITOR_HPP
#define PACS_SECURITY_ATNA_SERVICE_AUDITOR_HPP

#include "atna_audit_logger.hpp"
#include "atna_syslog_transport.hpp"

#include <atomic>
#include <memory>
#include <string>

namespace kcenon::pacs::security {

/**
 * @brief High-level facade for emitting ATNA audit events from DICOM services
 *
 * Combines the audit message generator (atna_audit_logger) with the syslog
 * transport (atna_syslog_transport) to provide simple, one-call audit methods
 * for common DICOM operations.
 *
 * ## Usage
 * ```cpp
 * syslog_transport_config config;
 * config.host = "audit-server.hospital.local";
 * config.port = 6514;
 * config.protocol = syslog_transport_protocol::tls;
 *
 * atna_service_auditor auditor(config, "PACS_SYSTEM_01");
 *
 * // After a C-STORE operation:
 * auditor.audit_instance_stored("MODALITY_01", "PACS_SCP",
 *     "1.2.3.4.5", "PAT001", true);
 *
 * // After a C-FIND operation:
 * auditor.audit_query("WORKSTATION_01", "PACS_SCP",
 *     "STUDY", true);
 * ```
 */
class atna_service_auditor {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct an auditor with syslog transport configuration
     *
     * @param config Syslog transport configuration
     * @param audit_source_id Identifier for this audit source (e.g., "PACS_01")
     */
    atna_service_auditor(const syslog_transport_config& config,
                         std::string audit_source_id);

    ~atna_service_auditor() = default;

    // Non-copyable, movable
    atna_service_auditor(const atna_service_auditor&) = delete;
    atna_service_auditor& operator=(const atna_service_auditor&) = delete;
    atna_service_auditor(atna_service_auditor&&) noexcept;
    atna_service_auditor& operator=(atna_service_auditor&&) noexcept;

    // =========================================================================
    // DICOM Service Audit Methods
    // =========================================================================

    /**
     * @brief Audit a C-STORE (DICOM Instances Transferred) event
     *
     * @param source_ae Calling AE title (sender)
     * @param dest_ae Called AE title (receiver)
     * @param study_uid Study Instance UID
     * @param patient_id Patient ID (optional, empty if unavailable)
     * @param success Whether the operation succeeded
     */
    void audit_instance_stored(const std::string& source_ae,
                               const std::string& dest_ae,
                               const std::string& study_uid,
                               const std::string& patient_id,
                               bool success);

    /**
     * @brief Audit a C-FIND (Query) event
     *
     * @param calling_ae Calling AE title (query requester)
     * @param called_ae Called AE title (query responder)
     * @param query_level Query level string (PATIENT/STUDY/SERIES/IMAGE)
     * @param success Whether the operation succeeded
     */
    void audit_query(const std::string& calling_ae,
                     const std::string& called_ae,
                     const std::string& query_level,
                     bool success);

    /**
     * @brief Audit a User Authentication event
     *
     * @param user_id User identifier (AE title or username)
     * @param is_login true for login, false for logout
     * @param success Whether the authentication succeeded
     */
    void audit_authentication(const std::string& user_id,
                              bool is_login,
                              bool success);

    /**
     * @brief Audit a Security Alert event (e.g., access denied)
     *
     * @param user_id User identifier
     * @param alert_description Description of the security alert
     */
    void audit_security_alert(const std::string& user_id,
                              const std::string& alert_description);

    // =========================================================================
    // Enable / Disable
    // =========================================================================

    /**
     * @brief Enable or disable audit event emission
     *
     * When disabled, audit methods return immediately without generating
     * or sending any messages. Useful for testing or low-resource environments.
     *
     * @param enabled true to enable, false to disable
     */
    void set_enabled(bool enabled) noexcept;

    /**
     * @brief Check if audit event emission is enabled
     * @return true if enabled
     */
    [[nodiscard]] bool is_enabled() const noexcept;

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get the number of audit events successfully sent
     */
    [[nodiscard]] size_t events_sent() const noexcept;

    /**
     * @brief Get the number of audit event send failures
     */
    [[nodiscard]] size_t events_failed() const noexcept;

    /**
     * @brief Reset statistics counters
     */
    void reset_statistics() noexcept;

    // =========================================================================
    // Configuration Access
    // =========================================================================

    /**
     * @brief Get the audit source identifier
     */
    [[nodiscard]] const std::string& audit_source_id() const noexcept;

    /**
     * @brief Get the underlying transport (for advanced use)
     */
    [[nodiscard]] const atna_syslog_transport& transport() const noexcept;

private:
    // =========================================================================
    // Private Helpers
    // =========================================================================

    /**
     * @brief Send an audit message via syslog transport
     *
     * Converts the message to XML and sends it. Updates statistics.
     *
     * @param message The audit message to send
     */
    void send_audit(const atna_audit_message& message);

    // =========================================================================
    // Private Members
    // =========================================================================

    /// Audit source identifier (e.g., "PACS_SYSTEM_01")
    std::string audit_source_id_;

    /// Syslog transport for sending audit messages
    atna_syslog_transport transport_;

    /// Whether audit is enabled
    std::atomic<bool> enabled_{true};

    /// Statistics
    std::atomic<size_t> events_sent_{0};
    std::atomic<size_t> events_failed_{0};
};

}  // namespace kcenon::pacs::security

#endif  // PACS_SECURITY_ATNA_SERVICE_AUDITOR_HPP
