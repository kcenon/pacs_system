/**
 * @file accept_worker.cpp
 * @brief Implementation of the accept_worker class
 */

#include "pacs/network/detail/accept_worker.hpp"

#include <cerrno>
#include <cstring>
#include <sstream>

#ifdef _WIN32
// Windows-specific includes already in header
#else
#include <fcntl.h>
#endif

namespace pacs::network::detail {

// =============================================================================
// Construction / Destruction
// =============================================================================

accept_worker::accept_worker(
    uint16_t port,
    connection_callback on_connection,
    maintenance_callback on_maintenance)
    : thread_base("accept_worker")
    , port_(port)
    , on_connection_(std::move(on_connection))
    , on_maintenance_(std::move(on_maintenance)) {
}

accept_worker::~accept_worker() {
    // Ensure graceful shutdown if still running
    if (is_running()) {
        stop();
    }
}

// =============================================================================
// Configuration
// =============================================================================

void accept_worker::set_max_pending_connections(int backlog) {
    backlog_ = backlog;
}

uint16_t accept_worker::port() const noexcept {
    return port_;
}

int accept_worker::max_pending_connections() const noexcept {
    return backlog_;
}

// =============================================================================
// Status
// =============================================================================

bool accept_worker::is_accepting() const noexcept {
    return accepting_.load(std::memory_order_acquire);
}

std::string accept_worker::to_string() const {
    std::ostringstream oss;
    oss << "accept_worker{"
        << "port=" << port_
        << ", backlog=" << backlog_
        << ", accepting=" << (is_accepting() ? "true" : "false")
        << ", running=" << (is_running() ? "true" : "false")
        << ", sessions=" << session_id_counter_.load(std::memory_order_relaxed)
        << "}";
    return oss.str();
}

// =============================================================================
// thread_base Overrides
// =============================================================================

accept_worker::result_void accept_worker::before_start() {
    // Initialize TCP socket for accepting connections

#ifdef _WIN32
    // Initialize Winsock
    WSADATA wsa_data;
    int wsa_result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (wsa_result != 0) {
        return kcenon::common::error_info("WSAStartup failed: " + std::to_string(wsa_result));
    }
    wsa_initialized_ = true;

    // Create socket
    listen_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket_ == INVALID_SOCKET) {
        int err = WSAGetLastError();
        WSACleanup();
        wsa_initialized_ = false;
        return kcenon::common::error_info("Failed to create socket: " + std::to_string(err));
    }

    // Set socket options
    int opt = 1;
    setsockopt(listen_socket_, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));

    // Set non-blocking mode
    u_long mode = 1;
    ioctlsocket(listen_socket_, FIONBIO, &mode);

    // Bind
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(listen_socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        int err = WSAGetLastError();
        closesocket(listen_socket_);
        listen_socket_ = INVALID_SOCKET;
        WSACleanup();
        wsa_initialized_ = false;
        return kcenon::common::error_info("Failed to bind to port " +
                                          std::to_string(port_) + ": " + std::to_string(err));
    }

    // Listen
    if (listen(listen_socket_, backlog_) == SOCKET_ERROR) {
        int err = WSAGetLastError();
        closesocket(listen_socket_);
        listen_socket_ = INVALID_SOCKET;
        WSACleanup();
        wsa_initialized_ = false;
        return kcenon::common::error_info("Failed to listen: " + std::to_string(err));
    }
#else
    // Create socket
    listen_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket_ < 0) {
        return kcenon::common::error_info("Failed to create socket: " +
                                          std::string(std::strerror(errno)));
    }

    // Set socket options
    int opt = 1;
    setsockopt(listen_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Set non-blocking mode
    int flags = fcntl(listen_socket_, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(listen_socket_, F_SETFL, flags | O_NONBLOCK);
    }

    // Bind
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(listen_socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::string err_msg = std::strerror(errno);
        close(listen_socket_);
        listen_socket_ = -1;
        return kcenon::common::error_info("Failed to bind to port " +
                                          std::to_string(port_) + ": " + err_msg);
    }

    // Listen
    if (listen(listen_socket_, backlog_) < 0) {
        std::string err_msg = std::strerror(errno);
        close(listen_socket_);
        listen_socket_ = -1;
        return kcenon::common::error_info("Failed to listen: " + err_msg);
    }
#endif

    accepting_.store(true, std::memory_order_release);
    return kcenon::common::ok();
}

accept_worker::result_void accept_worker::do_work() {
    // Main work routine for the accept loop

#ifdef _WIN32
    if (listen_socket_ == INVALID_SOCKET) {
        return kcenon::common::ok();
    }

    // Use select with short timeout to check for incoming connections
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(listen_socket_, &readfds);

    timeval tv{};
    tv.tv_sec = 0;
    tv.tv_usec = 10000;  // 10ms timeout for responsive maintenance callbacks

    int result = select(0, &readfds, nullptr, nullptr, &tv);

    if (result > 0 && FD_ISSET(listen_socket_, &readfds)) {
        sockaddr_in client_addr{};
        int addr_len = sizeof(client_addr);

        SOCKET client_socket = accept(listen_socket_,
                                       reinterpret_cast<sockaddr*>(&client_addr),
                                       &addr_len);

        if (client_socket != INVALID_SOCKET) {
            // Generate session ID and invoke callback
            uint64_t session_id = next_session_id();

            if (on_connection_) {
                on_connection_(session_id);
            }

            // Note: For now, we accept and immediately close the connection
            // since association doesn't support real network I/O yet.
            // The connection callback can be extended to handle the socket
            // when network_system integration is complete.
            closesocket(client_socket);
        }
    }
#else
    if (listen_socket_ < 0) {
        return kcenon::common::ok();
    }

    // Use select with short timeout to check for incoming connections
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(listen_socket_, &readfds);

    timeval tv{};
    tv.tv_sec = 0;
    tv.tv_usec = 10000;  // 10ms timeout for responsive maintenance callbacks

    int result = select(listen_socket_ + 1, &readfds, nullptr, nullptr, &tv);

    if (result > 0 && FD_ISSET(listen_socket_, &readfds)) {
        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);

        int client_socket = accept(listen_socket_,
                                    reinterpret_cast<sockaddr*>(&client_addr),
                                    &addr_len);

        if (client_socket >= 0) {
            // Generate session ID and invoke callback
            uint64_t session_id = next_session_id();

            if (on_connection_) {
                on_connection_(session_id);
            }

            // Note: For now, we accept and immediately close the connection
            // since association doesn't support real network I/O yet.
            // The connection callback can be extended to handle the socket
            // when network_system integration is complete.
            close(client_socket);
        }
    }
#endif

    // Invoke maintenance callback for periodic tasks (e.g., idle timeout checks)
    if (on_maintenance_) {
        on_maintenance_();
    }

    return kcenon::common::ok();
}

accept_worker::result_void accept_worker::after_stop() {
    // Clean up resources after the accept loop stops

    accepting_.store(false, std::memory_order_release);

#ifdef _WIN32
    if (listen_socket_ != INVALID_SOCKET) {
        closesocket(listen_socket_);
        listen_socket_ = INVALID_SOCKET;
    }
    if (wsa_initialized_) {
        WSACleanup();
        wsa_initialized_ = false;
    }
#else
    if (listen_socket_ >= 0) {
        close(listen_socket_);
        listen_socket_ = -1;
    }
#endif

    return kcenon::common::ok();
}

bool accept_worker::should_continue_work() const {
    // Return false to indicate no pending work that must complete before shutdown.
    // The accept loop will exit promptly when stop() is called.
    // Note: returning true here would prevent graceful shutdown.
    return false;
}

void accept_worker::on_stop_requested() {
    // Called when stop() is requested, before thread actually stops.
    // Close the socket to unblock any pending accept operations.

#ifdef _WIN32
    if (listen_socket_ != INVALID_SOCKET) {
        closesocket(listen_socket_);
        listen_socket_ = INVALID_SOCKET;
    }
#else
    if (listen_socket_ >= 0) {
        close(listen_socket_);
        listen_socket_ = -1;
    }
#endif
}

// =============================================================================
// Private Methods
// =============================================================================

uint64_t accept_worker::next_session_id() {
    return session_id_counter_.fetch_add(1, std::memory_order_relaxed);
}

}  // namespace pacs::network::detail
