/**
 * @file atna_syslog_transport.hpp
 * @brief Syslog transport for ATNA audit messages (RFC 5424/5425/5426)
 *
 * Sends RFC 3881 XML audit messages via Syslog protocol to a centralized
 * Audit Record Repository, supporting both UDP (RFC 5426) and TLS (RFC 5425).
 *
 * @see RFC 5424 — The Syslog Protocol
 * @see RFC 5425 — TLS Transport Mapping for Syslog
 * @see RFC 5426 — Transmission of Syslog Messages over UDP
 * @see IHE ITI TF-2 Section 3.20 — Record Audit Event
 * @see Issue #819 - ATNA Syslog Transport
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_SECURITY_ATNA_SYSLOG_TRANSPORT_HPP
#define PACS_SECURITY_ATNA_SYSLOG_TRANSPORT_HPP

#include "pacs/core/result.hpp"

#include <atomic>
#include <cstdint>
#include <string>

namespace kcenon::pacs::security {

// =============================================================================
// Syslog Facility Codes (RFC 5424 Section 6.2.1)
// =============================================================================

/**
 * @brief Syslog facility values
 *
 * The audit facility is typically 10 (security/authorization) or
 * 13 (log audit) per IHE ATNA recommendations.
 */
enum class syslog_facility : uint8_t {
    kern = 0,
    user = 1,
    mail = 2,
    daemon = 3,
    auth = 4,
    syslog = 5,
    lpr = 6,
    news = 7,
    uucp = 8,
    cron = 9,
    authpriv = 10,   ///< Security/authorization (recommended for ATNA)
    ftp = 11,
    ntp = 12,
    log_audit = 13,  ///< Log audit
    log_alert = 14,
    clock = 15,
    local0 = 16,
    local1 = 17,
    local2 = 18,
    local3 = 19,
    local4 = 20,
    local5 = 21,
    local6 = 22,
    local7 = 23
};

// =============================================================================
// Syslog Severity (RFC 5424 Section 6.2.1)
// =============================================================================

/**
 * @brief Syslog severity levels
 */
enum class syslog_severity : uint8_t {
    emergency = 0,
    alert = 1,
    critical = 2,
    error = 3,
    warning = 4,
    notice = 5,
    informational = 6,  ///< Default for audit events
    debug = 7
};

// =============================================================================
// Transport Protocol
// =============================================================================

/**
 * @brief Syslog transport protocol
 */
enum class syslog_transport_protocol : uint8_t {
    udp,  ///< UDP (RFC 5426) — Fire-and-forget
    tls   ///< TLS over TCP (RFC 5425) — Secure
};

// =============================================================================
// Configuration
// =============================================================================

/**
 * @brief Configuration for the Syslog transport
 */
struct syslog_transport_config {
    /// Transport protocol (UDP or TLS)
    syslog_transport_protocol protocol{syslog_transport_protocol::udp};

    /// Audit Record Repository hostname or IP
    std::string host{"localhost"};

    /// Port number (514 for UDP, 6514 for TLS per IANA)
    uint16_t port{514};

    /// Application name in Syslog header
    std::string app_name{"pacs_system"};

    /// Hostname to report in Syslog header (auto-detected if empty)
    std::string hostname;

    /// Syslog facility
    syslog_facility facility{syslog_facility::authpriv};

    /// Syslog severity for audit events
    syslog_severity severity{syslog_severity::informational};

    // -- TLS-specific options (RFC 5425) --

    /// Path to CA certificate file for server verification
    std::string ca_cert_path;

    /// Path to client certificate file (mutual TLS)
    std::string client_cert_path;

    /// Path to client private key file (mutual TLS)
    std::string client_key_path;

    /// Whether to verify server certificate (disable only for testing)
    bool verify_server{true};
};

// =============================================================================
// ATNA Syslog Transport
// =============================================================================

/**
 * @brief Sends ATNA audit messages via Syslog protocol
 *
 * Formats RFC 3881 XML audit messages into RFC 5424 Syslog messages and
 * sends them to an Audit Record Repository via UDP or TLS.
 *
 * ## Usage
 * ```cpp
 * syslog_transport_config config;
 * config.host = "audit-server.hospital.local";
 * config.port = 6514;
 * config.protocol = syslog_transport_protocol::tls;
 * config.ca_cert_path = "/etc/pacs/certs/ca.pem";
 *
 * atna_syslog_transport transport(config);
 *
 * auto msg = atna_audit_logger::build_user_authentication(...);
 * auto xml = atna_audit_logger::to_xml(msg);
 * auto result = transport.send(xml);
 * ```
 */
class atna_syslog_transport {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct transport with configuration
     * @param config Transport configuration
     */
    explicit atna_syslog_transport(const syslog_transport_config& config);

    ~atna_syslog_transport();

    // Non-copyable
    atna_syslog_transport(const atna_syslog_transport&) = delete;
    atna_syslog_transport& operator=(const atna_syslog_transport&) = delete;

    // Movable
    atna_syslog_transport(atna_syslog_transport&& other) noexcept;
    atna_syslog_transport& operator=(atna_syslog_transport&& other) noexcept;

    // =========================================================================
    // Send Operations
    // =========================================================================

    /**
     * @brief Send an RFC 3881 XML audit message via Syslog
     *
     * Wraps the XML payload in an RFC 5424 Syslog message and sends it
     * to the configured Audit Record Repository.
     *
     * @param xml_message RFC 3881 XML audit message string
     * @return VoidResult indicating success or transport error
     */
    [[nodiscard]] kcenon::pacs::VoidResult send(const std::string& xml_message);

    // =========================================================================
    // RFC 5424 Message Formatting
    // =========================================================================

    /**
     * @brief Format an XML audit message as an RFC 5424 Syslog message
     *
     * Produces the complete Syslog message without sending it.
     * Useful for testing and logging.
     *
     * @param xml_message The audit XML payload
     * @return Formatted RFC 5424 message string
     */
    [[nodiscard]] std::string format_syslog_message(
        const std::string& xml_message) const;

    // =========================================================================
    // Connection Management
    // =========================================================================

    /**
     * @brief Check if the transport is connected (TLS only)
     *
     * For UDP, always returns true since UDP is connectionless.
     */
    [[nodiscard]] bool is_connected() const noexcept;

    /**
     * @brief Close the transport connection
     *
     * For TLS, performs graceful shutdown. For UDP, closes the socket.
     */
    void close();

    // =========================================================================
    // Statistics
    // =========================================================================

    [[nodiscard]] size_t messages_sent() const noexcept;
    [[nodiscard]] size_t send_errors() const noexcept;
    void reset_statistics() noexcept;

    // =========================================================================
    // Configuration Access
    // =========================================================================

    [[nodiscard]] const syslog_transport_config& config() const noexcept;

private:
    // =========================================================================
    // Private Implementation
    // =========================================================================

    [[nodiscard]] kcenon::pacs::VoidResult send_udp(const std::string& syslog_message);
    [[nodiscard]] kcenon::pacs::VoidResult send_tls(const std::string& syslog_message);
    [[nodiscard]] kcenon::pacs::VoidResult ensure_tls_connected();

    [[nodiscard]] static std::string get_local_hostname();
    [[nodiscard]] static std::string get_timestamp();
    [[nodiscard]] static uint8_t compute_priority(
        syslog_facility facility, syslog_severity severity);

    // =========================================================================
    // Private Members
    // =========================================================================

    syslog_transport_config config_;

    // Socket handle (-1 = not connected)
#ifdef _WIN32
    using socket_type = unsigned long long;  // SOCKET
    static constexpr socket_type invalid_socket = ~static_cast<socket_type>(0);
#else
    using socket_type = int;
    static constexpr socket_type invalid_socket = -1;
#endif
    socket_type socket_{invalid_socket};

    // Opaque TLS context (avoids OpenSSL includes in header)
    struct tls_context;
    tls_context* tls_{nullptr};

    std::atomic<size_t> messages_sent_{0};
    std::atomic<size_t> send_errors_{0};
};

}  // namespace kcenon::pacs::security

#endif  // PACS_SECURITY_ATNA_SYSLOG_TRANSPORT_HPP
