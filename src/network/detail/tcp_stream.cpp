// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file tcp_stream.cpp
 * @brief Implementation of the blocking TCP client connection
 */

#include "kcenon/pacs/network/detail/tcp_stream.h"

#include <algorithm>
#include <cerrno>
#include <climits>
#include <system_error>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif
#else
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace kcenon::pacs::network::detail {

namespace {

using kcenon::pacs::error_info;
using kcenon::pacs::error_codes::connection_failed;
using kcenon::pacs::error_codes::connection_timeout;
using kcenon::pacs::error_codes::receive_failed;
using kcenon::pacs::error_codes::receive_timeout;
using kcenon::pacs::error_codes::send_failed;
using clock = std::chrono::steady_clock;

#ifdef _WIN32
using native_socket = SOCKET;
const native_socket invalid_native_socket = INVALID_SOCKET;
#else
using native_socket = int;
constexpr native_socket invalid_native_socket = -1;
#endif

native_socket to_native(std::intptr_t handle) {
    return static_cast<native_socket>(handle);
}

int last_socket_error() {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

std::string socket_error_text(int code) {
    return std::system_category().message(code);
}

bool would_block(int code) {
#ifdef _WIN32
    return code == WSAEWOULDBLOCK || code == WSAEINPROGRESS;
#else
    return code == EWOULDBLOCK || code == EAGAIN || code == EINPROGRESS;
#endif
}

bool interrupted(int code) {
#ifdef _WIN32
    return code == WSAEINTR;
#else
    return code == EINTR;
#endif
}

void close_native(native_socket s) {
#ifdef _WIN32
    closesocket(s);
#else
    ::close(s);
#endif
}

#ifdef _WIN32
/// Winsock stays initialized from the first connection until process exit
bool ensure_winsock() {
    static const bool initialized = [] {
        WSADATA data;
        return WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }();
    return initialized;
}
#endif

bool set_non_blocking(native_socket s) {
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(s, FIONBIO, &mode) == 0;
#else
    const int flags = fcntl(s, F_GETFL, 0);
    return flags >= 0 && fcntl(s, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

void set_socket_options(native_socket s) {
    // DICOM exchanges are request/response, so small PDUs go out at once
    int one = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY,
               reinterpret_cast<const char*>(&one), sizeof(one));
#ifdef SO_NOSIGPIPE
    // A write to a closed connection fails instead of raising SIGPIPE
    // (Linux passes MSG_NOSIGNAL to send() instead)
    setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
}

/// Milliseconds until @p deadline, never negative
int remaining_ms(clock::time_point deadline) {
    const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
        deadline - clock::now()).count();
    if (left <= 0) {
        return 0;
    }
    return static_cast<int>((std::min)(left, static_cast<decltype(left)>(INT_MAX)));
}

enum class wait_result { ready, timeout, error };

/// Wait until @p s is readable (writable when @p for_write) or @p deadline
/// passes. Errors and hang-ups count as ready, so the next socket call
/// reports them.
wait_result wait_for_socket(native_socket s, bool for_write,
                            clock::time_point deadline) {
    for (;;) {
        const int timeout_ms = remaining_ms(deadline);
#ifdef _WIN32
        // select() reports a failed non-blocking connect in the except set
        fd_set ready_fds;
        FD_ZERO(&ready_fds);
        FD_SET(s, &ready_fds);
        fd_set except_fds;
        FD_ZERO(&except_fds);
        FD_SET(s, &except_fds);
        timeval tv{};
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        const int rc = select(0, for_write ? nullptr : &ready_fds,
                              for_write ? &ready_fds : nullptr, &except_fds, &tv);
#else
        // poll() rather than select(): descriptors above FD_SETSIZE are fine
        pollfd pfd{};
        pfd.fd = s;
        pfd.events = for_write ? POLLOUT : POLLIN;
        const int rc = ::poll(&pfd, 1, timeout_ms);
#endif
        if (rc > 0) {
            return wait_result::ready;
        }
        if (rc == 0) {
            return wait_result::timeout;
        }
        if (!interrupted(last_socket_error())) {
            return wait_result::error;
        }
    }
}

struct addrinfo_deleter {
    void operator()(addrinfo* list) const noexcept {
        freeaddrinfo(list);
    }
};

}  // namespace

// =============================================================================
// Construction / Destruction
// =============================================================================

tcp_stream::tcp_stream(std::intptr_t handle) noexcept : handle_(handle) {}

tcp_stream::~tcp_stream() {
    close_native(to_native(handle_));
}

kcenon::pacs::Result<std::unique_ptr<tcp_stream>> tcp_stream::connect(
    const std::string& host,
    uint16_t port,
    duration timeout) {

    const std::string target = host + ":" + std::to_string(port);

#ifdef _WIN32
    if (!ensure_winsock()) {
        return error_info{connection_failed, "WSAStartup failed", "network"};
    }
#endif

    const auto deadline = clock::now() + timeout;

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* found = nullptr;
    const std::string service = std::to_string(port);
    if (getaddrinfo(host.c_str(), service.c_str(), &hints, &found) != 0 ||
        found == nullptr) {
        return error_info{connection_failed,
            "Cannot resolve host '" + host + "'", "network"};
    }
    const std::unique_ptr<addrinfo, addrinfo_deleter> addresses(found);

    // IPv4 first: a refused IPv6 attempt costs about a second on Windows,
    // and dicom_server_v2 listens on IPv4 only
    std::vector<const addrinfo*> candidates;
    for (const addrinfo* ai = found; ai != nullptr; ai = ai->ai_next) {
        candidates.push_back(ai);
    }
    std::stable_partition(candidates.begin(), candidates.end(),
        [](const addrinfo* ai) { return ai->ai_family == AF_INET; });

    std::string last_error = "no usable address";
    for (const addrinfo* ai : candidates) {
        const native_socket s = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (s == invalid_native_socket) {
            last_error = socket_error_text(last_socket_error());
            continue;
        }
        if (!set_non_blocking(s)) {
            last_error = socket_error_text(last_socket_error());
            close_native(s);
            continue;
        }
        set_socket_options(s);

        if (::connect(s, ai->ai_addr, static_cast<socklen_t>(ai->ai_addrlen)) != 0) {
            const int err = last_socket_error();
            if (!would_block(err)) {
                last_error = socket_error_text(err);
                close_native(s);
                continue;
            }

            const auto waited = wait_for_socket(s, true, deadline);
            if (waited == wait_result::timeout) {
                close_native(s);
                return error_info{connection_timeout,
                    "Timed out connecting to " + target, "network"};
            }
            if (waited == wait_result::error) {
                last_error = socket_error_text(last_socket_error());
                close_native(s);
                continue;
            }

            int so_error = 0;
            socklen_t so_error_length = sizeof(so_error);
            if (getsockopt(s, SOL_SOCKET, SO_ERROR,
                           reinterpret_cast<char*>(&so_error), &so_error_length) != 0) {
                so_error = last_socket_error();
            }
            if (so_error != 0) {
                last_error = socket_error_text(so_error);
                close_native(s);
                continue;
            }
        }

        return std::unique_ptr<tcp_stream>(
            new tcp_stream(static_cast<std::intptr_t>(s)));
    }

    return error_info{connection_failed,
        "Cannot connect to " + target + ": " + last_error, "network"};
}

// =============================================================================
// I/O
// =============================================================================

kcenon::pacs::VoidResult tcp_stream::write_all(
    std::span<const uint8_t> data,
    duration timeout) {

    const auto deadline = clock::now() + timeout;
    const native_socket s = to_native(handle_);

    std::size_t sent = 0;
    while (sent < data.size()) {
        const std::size_t chunk =
            (std::min)(data.size() - sent, static_cast<std::size_t>(INT_MAX));
#ifdef _WIN32
        const int n = ::send(s, reinterpret_cast<const char*>(data.data() + sent),
                             static_cast<int>(chunk), 0);
#elif defined(MSG_NOSIGNAL)
        const auto n = ::send(s, data.data() + sent, chunk, MSG_NOSIGNAL);
#else
        const auto n = ::send(s, data.data() + sent, chunk, 0);
#endif
        if (n > 0) {
            sent += static_cast<std::size_t>(n);
            continue;
        }

        const int err = last_socket_error();
        if (n < 0 && interrupted(err)) {
            continue;
        }
        if (n < 0 && would_block(err)) {
            const auto waited = wait_for_socket(s, true, deadline);
            if (waited == wait_result::ready) {
                continue;
            }
            if (waited == wait_result::timeout) {
                return error_info{send_failed,
                    "Timed out writing to the peer", "network"};
            }
            return error_info{send_failed,
                "Write failed: " + socket_error_text(last_socket_error()), "network"};
        }
        return error_info{send_failed,
            "Write failed: " + socket_error_text(err), "network"};
    }

    return kcenon::pacs::ok();
}

kcenon::pacs::Result<std::size_t> tcp_stream::read_some(
    std::span<uint8_t> buffer,
    duration timeout) {

    const auto deadline = clock::now() + timeout;
    const native_socket s = to_native(handle_);

    for (;;) {
#ifdef _WIN32
        const int n = ::recv(s, reinterpret_cast<char*>(buffer.data()),
            static_cast<int>((std::min)(buffer.size(), static_cast<std::size_t>(INT_MAX))), 0);
#else
        const auto n = ::recv(s, buffer.data(), buffer.size(), 0);
#endif
        if (n > 0) {
            return static_cast<std::size_t>(n);
        }
        if (n == 0) {
            return error_info{receive_failed,
                "Connection closed by the peer", "network"};
        }

        const int err = last_socket_error();
        if (interrupted(err)) {
            continue;
        }
        if (!would_block(err)) {
            return error_info{receive_failed,
                "Read failed: " + socket_error_text(err), "network"};
        }

        const auto waited = wait_for_socket(s, false, deadline);
        if (waited == wait_result::timeout) {
            return error_info{receive_timeout, "Receive timeout", "network"};
        }
        if (waited == wait_result::error) {
            return error_info{receive_failed,
                "Read failed: " + socket_error_text(last_socket_error()), "network"};
        }
    }
}

void tcp_stream::shutdown() noexcept {
#ifdef _WIN32
    ::shutdown(to_native(handle_), SD_BOTH);
#else
    ::shutdown(to_native(handle_), SHUT_RDWR);
#endif
}

}  // namespace kcenon::pacs::network::detail
