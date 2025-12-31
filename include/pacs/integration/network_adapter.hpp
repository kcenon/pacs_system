/**
 * @file network_adapter.hpp
 * @brief Adapter for integrating network_system for DICOM protocol
 *
 * This file provides the network_adapter class for adapting network_system's
 * high-performance TCP server/client for DICOM protocol communication.
 * It handles server creation, client connection, and TLS configuration.
 *
 * @see IR-2 (network_system Integration)
 * @see DES-INT-002 - Network Adapter Design Specification
 */

#pragma once

#include "pacs/network/server_config.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

// KCENON_HAS_COMMON_SYSTEM is defined by CMake when common_system is available
#ifndef KCENON_HAS_COMMON_SYSTEM
#define KCENON_HAS_COMMON_SYSTEM 0
#endif

#if KCENON_HAS_COMMON_SYSTEM
#include <kcenon/common/patterns/result.h>
#endif

// Forward declarations for kcenon::network types (no ASIO dependency)
// Using direct forward declarations to reduce external header dependencies
// and maintain compatibility with network_system namespace refactoring
namespace kcenon::network::session {
class messaging_session;
class secure_session;
}  // namespace kcenon::network::session

namespace kcenon::network::core {
class messaging_server;
class secure_messaging_server;
}  // namespace kcenon::network::core

// Legacy namespace aliases for backward compatibility
namespace network_system::session {
using messaging_session = kcenon::network::session::messaging_session;
using secure_session = kcenon::network::session::secure_session;
}  // namespace network_system::session

namespace network_system::core {
using messaging_server = kcenon::network::core::messaging_server;
using secure_messaging_server = kcenon::network::core::secure_messaging_server;
}  // namespace network_system::core

namespace pacs::network {
class dicom_server;
}  // namespace pacs::network

namespace pacs::integration {

// Forward declarations
class dicom_session;

// =============================================================================
// Result Type and Error Info
// =============================================================================

#if KCENON_HAS_COMMON_SYSTEM
template <typename T>
using Result = kcenon::common::Result<T>;

using error_info = kcenon::common::error_info;
#else
/**
 * @brief Simple error info for fallback when common_system is unavailable
 */
struct error_info {
    int code = -1;
    std::string message;
    std::string module;

    error_info() = default;
    error_info(const std::string& msg) : message(msg) {}
    error_info(int c, const std::string& msg, const std::string& mod = "")
        : code(c), message(msg), module(mod) {}
};

/**
 * @brief Simple result type for error handling when common_system is unavailable
 */
template <typename T>
class Result {
public:
    Result(T value) : data_(std::move(value)), has_value_(true) {}
    Result(const error_info& err) : error_(err), has_value_(false) {}

    [[nodiscard]] bool is_ok() const noexcept { return has_value_; }
    [[nodiscard]] bool is_err() const noexcept { return !has_value_; }
    [[nodiscard]] T& value() & { return data_; }
    [[nodiscard]] const T& value() const& { return data_; }
    [[nodiscard]] T&& value() && { return std::move(data_); }
    [[nodiscard]] const error_info& error() const { return error_; }

private:
    T data_{};
    error_info error_;
    bool has_value_;
};
#endif

// =============================================================================
// TLS Configuration
// =============================================================================

/**
 * @struct tls_config
 * @brief Configuration for TLS/SSL secure transport
 *
 * DICOM supports TLS 1.2/1.3 for secure communication as defined in
 * DICOM PS3.15 (Security and System Management Profiles).
 */
struct tls_config {
    /// Enable TLS for connections
    bool enabled = false;

    /// Path to certificate file (PEM format)
    std::filesystem::path cert_path;

    /// Path to private key file (PEM format)
    std::filesystem::path key_path;

    /// Path to CA certificate file for verification (optional)
    std::filesystem::path ca_path;

    /// Verify peer certificate
    bool verify_peer = true;

    /// Minimum TLS version (1.2 recommended for DICOM)
    enum class tls_version { v1_2, v1_3 } min_version = tls_version::v1_2;

    /**
     * @brief Check if TLS configuration is valid
     * @return true if all required paths are set when enabled
     */
    [[nodiscard]] bool is_valid() const noexcept {
        if (!enabled) return true;
        return !cert_path.empty() && !key_path.empty();
    }
};

// =============================================================================
// Connection Configuration
// =============================================================================

/**
 * @struct connection_config
 * @brief Configuration for client connections
 */
struct connection_config {
    /// Remote host address
    std::string host;

    /// Remote port
    uint16_t port = 104;  // Standard DICOM port

    /// Connection timeout
    std::chrono::milliseconds timeout{30000};

    /// TLS configuration (optional)
    tls_config tls;

    connection_config() = default;
    connection_config(std::string h, uint16_t p)
        : host(std::move(h)), port(p) {}
};

// =============================================================================
// Network Adapter Class
// =============================================================================

/**
 * @class network_adapter
 * @brief Adapter for integrating network_system for DICOM protocol
 *
 * This class provides a PACS-specific interface to network_system's
 * TCP server and client functionality. Key features include:
 *
 * - **Server Creation**: Create DICOM servers using network_system's messaging_server
 * - **Client Connection**: Connect to remote DICOM peers with timeout support
 * - **TLS Support**: Optional TLS 1.2/1.3 for DICOM secure transport
 * - **Session Management**: Wrap network sessions for DICOM PDU handling
 *
 * Thread Safety: All public methods are thread-safe and can be called
 * from any thread.
 *
 * @example
 * @code
 * // Create a DICOM server
 * network::server_config config;
 * config.ae_title = "MY_SCP";
 * config.port = 11112;
 *
 * auto server = network_adapter::create_server(config);
 * if (server) {
 *     server->start();
 * }
 *
 * // Connect to a remote peer
 * connection_config conn_config{"192.168.1.100", 104};
 * conn_config.timeout = std::chrono::seconds{10};
 *
 * auto result = network_adapter::connect(conn_config);
 * if (result.is_ok()) {
 *     auto& session = result.value();
 *     // Use session for DICOM communication
 * }
 * @endcode
 */
class network_adapter {
public:
    // ─────────────────────────────────────────────────────
    // Type Aliases
    // ─────────────────────────────────────────────────────

    /// Session pointer type
    using session_ptr = std::shared_ptr<dicom_session>;

    /// Connection callback type
    using connection_callback = std::function<void(session_ptr)>;

    /// Disconnection callback type
    using disconnection_callback = std::function<void(const std::string&)>;

    /// Error callback type
    using error_callback = std::function<void(session_ptr, std::error_code)>;

    // ─────────────────────────────────────────────────────
    // Server Creation
    // ─────────────────────────────────────────────────────

    /**
     * @brief Create a DICOM server using network_system
     *
     * Creates and configures a network_system messaging_server (or
     * secure_messaging_server if TLS is enabled) for DICOM communication.
     *
     * @param config Server configuration
     * @param tls_cfg Optional TLS configuration
     * @return Unique pointer to dicom_server, or nullptr on failure
     *
     * @note The returned server is not started; call start() to begin accepting
     */
    [[nodiscard]] static std::unique_ptr<network::dicom_server>
        create_server(const network::server_config& config,
                      const tls_config& tls_cfg = {});

    // ─────────────────────────────────────────────────────
    // Client Connection
    // ─────────────────────────────────────────────────────

    /**
     * @brief Connect to a remote DICOM peer
     *
     * Establishes a TCP connection to the specified host and port,
     * optionally using TLS for secure transport.
     *
     * @param config Connection configuration
     * @return Result containing session on success, or error message
     *
     * @example
     * @code
     * connection_config config{"192.168.1.100", 104};
     * config.timeout = std::chrono::seconds{5};
     *
     * auto result = network_adapter::connect(config);
     * if (result.is_ok()) {
     *     auto session = std::move(result.value());
     *     // Send A-ASSOCIATE-RQ...
     * }
     * @endcode
     */
    [[nodiscard]] static Result<session_ptr> connect(
        const connection_config& config);

    /**
     * @brief Connect to a remote DICOM peer (simplified overload)
     *
     * @param host Remote host address
     * @param port Remote port
     * @param timeout Connection timeout
     * @return Result containing session on success, or error message
     */
    [[nodiscard]] static Result<session_ptr> connect(
        const std::string& host,
        uint16_t port,
        std::chrono::milliseconds timeout = std::chrono::seconds{30});

    // ─────────────────────────────────────────────────────
    // TLS Configuration
    // ─────────────────────────────────────────────────────

    /**
     * @brief Configure TLS settings
     *
     * Validates and prepares TLS configuration for use with
     * secure connections. This should be called before creating
     * servers or connecting to peers with TLS enabled.
     *
     * @param config TLS configuration to validate and apply
     * @return Result indicating success or validation error
     *
     * @example
     * @code
     * tls_config tls;
     * tls.enabled = true;
     * tls.cert_path = "/path/to/server.crt";
     * tls.key_path = "/path/to/server.key";
     * tls.ca_path = "/path/to/ca.crt";
     * tls.min_version = tls_config::tls_version::v1_2;
     *
     * auto result = network_adapter::configure_tls(tls);
     * if (result.is_err()) {
     *     std::cerr << "TLS config error: " << result.error() << '\n';
     * }
     * @endcode
     */
    [[nodiscard]] static Result<std::monostate> configure_tls(
        const tls_config& config);

    // ─────────────────────────────────────────────────────
    // Session Wrapping
    // ─────────────────────────────────────────────────────

    /**
     * @brief Wrap a network_system session for DICOM communication
     *
     * Creates a dicom_session wrapper around a network_system
     * messaging_session, enabling PDU-level DICOM communication.
     *
     * @param session The network_system session to wrap
     * @return Wrapped DICOM session
     */
    [[nodiscard]] static session_ptr wrap_session(
        std::shared_ptr<network_system::session::messaging_session> session);

    /**
     * @brief Wrap a secure network_system session for DICOM communication
     *
     * @param session The secure network_system session to wrap
     * @return Wrapped DICOM session
     */
    [[nodiscard]] static session_ptr wrap_session(
        std::shared_ptr<network_system::session::secure_session> session);

private:
    // Prevent instantiation
    network_adapter() = delete;
    ~network_adapter() = delete;
    network_adapter(const network_adapter&) = delete;
    network_adapter& operator=(const network_adapter&) = delete;
};

}  // namespace pacs::integration
