/**
 * @file atna_syslog_transport.cpp
 * @brief Implementation of the ATNA Syslog transport
 */

#include "pacs/security/atna_syslog_transport.hpp"

#include <chrono>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <sstream>

// Platform-specific network headers
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <unistd.h>
#endif

// OpenSSL for TLS transport (conditionally included)
#ifdef PACS_WITH_DIGITAL_SIGNATURES
    #include <openssl/err.h>
    #include <openssl/ssl.h>
#endif

namespace pacs::security {

// =============================================================================
// TLS Context (opaque, avoids OpenSSL in header)
// =============================================================================

struct atna_syslog_transport::tls_context {
#ifdef PACS_WITH_DIGITAL_SIGNATURES
    SSL_CTX* ctx{nullptr};
    SSL* ssl{nullptr};

    ~tls_context() {
        if (ssl) {
            SSL_shutdown(ssl);
            SSL_free(ssl);
        }
        if (ctx) {
            SSL_CTX_free(ctx);
        }
    }
#endif
};

// =============================================================================
// Platform Helpers
// =============================================================================

namespace {

void close_socket([[maybe_unused]] int sock) {
#ifdef _WIN32
    ::closesocket(static_cast<SOCKET>(sock));
#else
    ::close(sock);
#endif
}

#ifdef PACS_WITH_DIGITAL_SIGNATURES
std::string get_openssl_error() {
    unsigned long err = ERR_get_error();
    if (err == 0) {
        return "Unknown OpenSSL error";
    }
    char buf[256];
    ERR_error_string_n(err, buf, sizeof(buf));
    return std::string(buf);
}
#endif

}  // anonymous namespace

// =============================================================================
// Construction / Destruction
// =============================================================================

atna_syslog_transport::atna_syslog_transport(
    const syslog_transport_config& config)
    : config_(config) {
    if (config_.hostname.empty()) {
        config_.hostname = get_local_hostname();
    }
}

atna_syslog_transport::~atna_syslog_transport() {
    close();
}

atna_syslog_transport::atna_syslog_transport(
    atna_syslog_transport&& other) noexcept
    : config_(std::move(other.config_)),
      socket_(other.socket_),
      tls_(other.tls_),
      messages_sent_(other.messages_sent_.load(std::memory_order_relaxed)),
      send_errors_(other.send_errors_.load(std::memory_order_relaxed)) {
    other.socket_ = invalid_socket;
    other.tls_ = nullptr;
}

atna_syslog_transport& atna_syslog_transport::operator=(
    atna_syslog_transport&& other) noexcept {
    if (this != &other) {
        close();
        config_ = std::move(other.config_);
        socket_ = other.socket_;
        tls_ = other.tls_;
        messages_sent_.store(
            other.messages_sent_.load(std::memory_order_relaxed),
            std::memory_order_relaxed);
        send_errors_.store(
            other.send_errors_.load(std::memory_order_relaxed),
            std::memory_order_relaxed);
        other.socket_ = invalid_socket;
        other.tls_ = nullptr;
    }
    return *this;
}

// =============================================================================
// Send Operations
// =============================================================================

pacs::VoidResult atna_syslog_transport::send(
    const std::string& xml_message) {

    auto syslog_msg = format_syslog_message(xml_message);

    auto result = (config_.protocol == syslog_transport_protocol::tls)
        ? send_tls(syslog_msg)
        : send_udp(syslog_msg);

    if (result.is_ok()) {
        messages_sent_.fetch_add(1, std::memory_order_relaxed);
    } else {
        send_errors_.fetch_add(1, std::memory_order_relaxed);
    }

    return result;
}

// =============================================================================
// RFC 5424 Message Formatting
// =============================================================================

std::string atna_syslog_transport::format_syslog_message(
    const std::string& xml_message) const {

    // RFC 5424 format:
    // <PRI>VERSION SP TIMESTAMP SP HOSTNAME SP APP-NAME SP PROCID SP MSGID
    //   SP STRUCTURED-DATA SP MSG

    uint8_t pri = compute_priority(config_.facility, config_.severity);
    std::string timestamp = get_timestamp();

    std::ostringstream oss;
    oss << "<" << static_cast<int>(pri) << ">"
        << "1"  // Version
        << " " << timestamp
        << " " << (config_.hostname.empty() ? "-" : config_.hostname)
        << " " << (config_.app_name.empty() ? "-" : config_.app_name)
        << " " << "-"  // PROCID (NIL)
        << " " << "IHE+RFC-3881"  // MSGID — IHE ATNA identifier
        << " " << "-"  // STRUCTURED-DATA (NIL)
        << " " << "\xEF\xBB\xBF"  // BOM (RFC 5424 Section 6.4)
        << xml_message;

    return oss.str();
}

// =============================================================================
// Connection Management
// =============================================================================

bool atna_syslog_transport::is_connected() const noexcept {
    if (config_.protocol == syslog_transport_protocol::udp) {
        return true;  // UDP is connectionless
    }
    return socket_ != invalid_socket && tls_ != nullptr;
}

void atna_syslog_transport::close() {
    delete tls_;
    tls_ = nullptr;

    if (socket_ != invalid_socket) {
        close_socket(socket_);
        socket_ = invalid_socket;
    }
}

// =============================================================================
// Statistics
// =============================================================================

size_t atna_syslog_transport::messages_sent() const noexcept {
    return messages_sent_.load(std::memory_order_relaxed);
}

size_t atna_syslog_transport::send_errors() const noexcept {
    return send_errors_.load(std::memory_order_relaxed);
}

void atna_syslog_transport::reset_statistics() noexcept {
    messages_sent_.store(0, std::memory_order_relaxed);
    send_errors_.store(0, std::memory_order_relaxed);
}

const syslog_transport_config& atna_syslog_transport::config() const noexcept {
    return config_;
}

// =============================================================================
// Private — UDP Transport (RFC 5426)
// =============================================================================

pacs::VoidResult atna_syslog_transport::send_udp(
    const std::string& syslog_message) {

    // Resolve destination address
    struct addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    struct addrinfo* result = nullptr;
    std::string port_str = std::to_string(config_.port);

    int ret = ::getaddrinfo(
        config_.host.c_str(), port_str.c_str(), &hints, &result);
    if (ret != 0 || result == nullptr) {
        return pacs::pacs_void_error(
            pacs::error_codes::connection_failed,
            "Failed to resolve syslog host: " + config_.host);
    }

    // Create UDP socket
    socket_type sock = ::socket(
        result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sock == invalid_socket) {
        ::freeaddrinfo(result);
        return pacs::pacs_void_error(
            pacs::error_codes::connection_failed,
            "Failed to create UDP socket");
    }

    // Send the message
    auto bytes_sent = ::sendto(
        sock,
        syslog_message.data(),
        static_cast<int>(syslog_message.size()),
        0,
        result->ai_addr,
        static_cast<int>(result->ai_addrlen));

    ::freeaddrinfo(result);
    close_socket(sock);

    if (bytes_sent < 0) {
        return pacs::pacs_void_error(
            pacs::error_codes::send_failed,
            "Failed to send UDP syslog message");
    }

    return kcenon::common::ok();
}

// =============================================================================
// Private — TLS Transport (RFC 5425)
// =============================================================================

pacs::VoidResult atna_syslog_transport::send_tls(
    const std::string& syslog_message) {

#ifndef PACS_WITH_DIGITAL_SIGNATURES
    (void)syslog_message;
    return pacs::pacs_void_error(
        pacs::error_codes::connection_failed,
        "TLS syslog transport requires OpenSSL (PACS_WITH_DIGITAL_SIGNATURES)");
#else
    auto connect_result = ensure_tls_connected();
    if (!connect_result.is_ok()) {
        return connect_result;
    }

    // RFC 5425: Octet-counting framing
    // MSG-LEN SP SYSLOG-MSG
    std::string framed =
        std::to_string(syslog_message.size()) + " " + syslog_message;

    int bytes_written = SSL_write(
        tls_->ssl, framed.data(), static_cast<int>(framed.size()));

    if (bytes_written <= 0) {
        int ssl_err = SSL_get_error(tls_->ssl, bytes_written);
        close();
        return pacs::pacs_void_error(
            pacs::error_codes::send_failed,
            "TLS write failed (SSL error: " +
            std::to_string(ssl_err) + ")");
    }

    return kcenon::common::ok();
#endif
}

pacs::VoidResult atna_syslog_transport::ensure_tls_connected() {
#ifndef PACS_WITH_DIGITAL_SIGNATURES
    return pacs::pacs_void_error(
        pacs::error_codes::connection_failed,
        "TLS not available — OpenSSL not linked");
#else
    if (tls_ && tls_->ssl && socket_ != invalid_socket) {
        return kcenon::common::ok();  // Already connected
    }

    // Clean up previous state
    close();

    // Create SSL context
    tls_ = new tls_context();
    tls_->ctx = SSL_CTX_new(TLS_client_method());
    if (!tls_->ctx) {
        close();
        return pacs::pacs_void_error(
            pacs::error_codes::connection_failed,
            "Failed to create TLS context: " + get_openssl_error());
    }

    // Set minimum TLS version to 1.2 (IHE ATNA requirement)
    SSL_CTX_set_min_proto_version(tls_->ctx, TLS1_2_VERSION);

    // Load CA certificate for server verification
    if (!config_.ca_cert_path.empty()) {
        if (SSL_CTX_load_verify_locations(
                tls_->ctx, config_.ca_cert_path.c_str(), nullptr) != 1) {
            close();
            return pacs::pacs_void_error(
                pacs::error_codes::connection_failed,
                "Failed to load CA certificate: " + get_openssl_error());
        }
    }

    // Load client certificate (mutual TLS)
    if (!config_.client_cert_path.empty()) {
        if (SSL_CTX_use_certificate_file(
                tls_->ctx, config_.client_cert_path.c_str(),
                SSL_FILETYPE_PEM) != 1) {
            close();
            return pacs::pacs_void_error(
                pacs::error_codes::connection_failed,
                "Failed to load client certificate: " + get_openssl_error());
        }
    }

    if (!config_.client_key_path.empty()) {
        if (SSL_CTX_use_PrivateKey_file(
                tls_->ctx, config_.client_key_path.c_str(),
                SSL_FILETYPE_PEM) != 1) {
            close();
            return pacs::pacs_void_error(
                pacs::error_codes::connection_failed,
                "Failed to load client key: " + get_openssl_error());
        }
    }

    // Set verification mode
    if (config_.verify_server) {
        SSL_CTX_set_verify(tls_->ctx, SSL_VERIFY_PEER, nullptr);
    } else {
        SSL_CTX_set_verify(tls_->ctx, SSL_VERIFY_NONE, nullptr);
    }

    // Resolve and connect TCP socket
    struct addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo* result = nullptr;
    std::string port_str = std::to_string(config_.port);

    int ret = ::getaddrinfo(
        config_.host.c_str(), port_str.c_str(), &hints, &result);
    if (ret != 0 || result == nullptr) {
        close();
        return pacs::pacs_void_error(
            pacs::error_codes::connection_failed,
            "Failed to resolve TLS syslog host: " + config_.host);
    }

    socket_ = ::socket(
        result->ai_family, result->ai_socktype, result->ai_protocol);
    if (socket_ == invalid_socket) {
        ::freeaddrinfo(result);
        close();
        return pacs::pacs_void_error(
            pacs::error_codes::connection_failed,
            "Failed to create TCP socket");
    }

    if (::connect(socket_, result->ai_addr,
                  static_cast<int>(result->ai_addrlen)) != 0) {
        ::freeaddrinfo(result);
        close();
        return pacs::pacs_void_error(
            pacs::error_codes::connection_failed,
            "Failed to connect to TLS syslog server: " +
            config_.host + ":" + port_str);
    }
    ::freeaddrinfo(result);

    // Create SSL object and perform handshake
    tls_->ssl = SSL_new(tls_->ctx);
    if (!tls_->ssl) {
        close();
        return pacs::pacs_void_error(
            pacs::error_codes::connection_failed,
            "Failed to create SSL object: " + get_openssl_error());
    }

    SSL_set_fd(tls_->ssl, static_cast<int>(socket_));

    // Set SNI hostname
    SSL_set_tlsext_host_name(tls_->ssl, config_.host.c_str());

    if (SSL_connect(tls_->ssl) != 1) {
        std::string err = get_openssl_error();
        close();
        return pacs::pacs_void_error(
            pacs::error_codes::connection_failed,
            "TLS handshake failed: " + err);
    }

    return kcenon::common::ok();
#endif
}

// =============================================================================
// Private — Utility Functions
// =============================================================================

std::string atna_syslog_transport::get_local_hostname() {
    char hostname[256];
    if (::gethostname(hostname, sizeof(hostname)) == 0) {
        hostname[sizeof(hostname) - 1] = '\0';
        return std::string(hostname);
    }
    return "localhost";
}

std::string atna_syslog_transport::get_timestamp() {
    // RFC 5424 TIMESTAMP format: YYYY-MM-DDThh:mm:ss.sssZ
    auto now = std::chrono::system_clock::now();
    auto time_t_val = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm tm_val{};
#if defined(_WIN32)
    gmtime_s(&tm_val, &time_t_val);
#else
    gmtime_r(&time_t_val, &tm_val);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_val, "%Y-%m-%dT%H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count()
        << "Z";

    return oss.str();
}

uint8_t atna_syslog_transport::compute_priority(
    syslog_facility facility, syslog_severity severity) {
    // PRI = Facility * 8 + Severity
    return static_cast<uint8_t>(
        static_cast<uint8_t>(facility) * 8 +
        static_cast<uint8_t>(severity));
}

}  // namespace pacs::security
